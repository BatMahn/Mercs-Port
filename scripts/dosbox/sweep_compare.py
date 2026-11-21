#!/usr/bin/env python3
"""Compare DOSBox and port frames scene by scene.

usage: sweep_compare.py SCENES.txt DOS_DIR PORT_DIR OUT_DIR

DOS_DIR holds <SCENE>_a.png / _b.png (640 x 480 at the top left of the grab), PORT_DIR <SCENE>_a.ppm / _b.ppm.
Writes OUT_DIR/<SCENE>_<a|b>.png (DOS | port | pixels more than 32 levels off) and OUT_DIR/results.json.
The top and bottom 30 rows (the message bars) are left out of the numbers."""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw

scenes_file, dos_dir, port_dir, out = sys.argv[1:5]
os.makedirs(out, exist_ok=True)
rows = []
for s in open(scenes_file).read().split():
    r = {'scene': s}
    for v in 'ab':
        dp, pp = f'{dos_dir}/{s}_{v}.png', f'{port_dir}/{s}_{v}.ppm'
        if not os.path.exists(dp) or not os.path.exists(pp) or os.path.getsize(pp) == 0:
            r[v] = None
            continue
        a = np.asarray(Image.open(dp).convert('RGB').crop((0, 0, 640, 480))).astype(int)
        b = np.asarray(Image.open(pp).convert('RGB').resize((640, 480))).astype(int)
        d = np.abs(a - b).max(2)
        d[:30, :] = 0
        d[450:, :] = 0
        r[v] = {'mean': round(float(np.abs(a - b)[30:450].mean()), 2), 'gt32': round(float((d > 32).mean() * 100), 2)}
        sheet = Image.new('RGB', (1920, 480))
        sheet.paste(Image.fromarray(a.astype('uint8')), (0, 0))
        sheet.paste(Image.fromarray(b.astype('uint8')), (640, 0))
        sheet.paste(Image.fromarray(((d > 32) * 255).astype('uint8')).convert('RGB'), (1280, 0))
        dr = ImageDraw.Draw(sheet)
        for x, t in ((5, 'DOS'), (645, 'PORT'), (1285, 'diff>32')):
            dr.text((x, 465), t, fill=(255, 255, 0))
        sheet.save(f'{out}/{s}_{v}.png')
    rows.append(r)
json.dump(rows, open(f'{out}/results.json', 'w'), indent=1)
for r in rows:
    a, b = r.get('a'), r.get('b')
    print(r['scene'], 'cockpit', a and (a['mean'], a['gt32']), 'external', b and (b['mean'], b['gt32']))
