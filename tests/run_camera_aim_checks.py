#!/usr/bin/env python3
"""Exercise the production cockpit camera/reticle with ordinary held controls.

Uses private install/config folders and supplied archives. No original game or
career file is modified. It checks half-pitch view response, movable reticles,
offscreen glances and neutral centering at two aspect ratios in all three looks.
The bounded original camera/aim/reticle fixtures establish the underlying math.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EDITIONS = {'dos': ('cd/MERCS/MW2.PRJ', 'retail'),
            'direct3d': ('titanium/MW2.PRJ', 'direct3d'),
            '3dfx': ('3d/models.prj', '3dfx')}
CASES = [('neutral', '', 0), ('tilt-down', 'up', 0), ('tilt-up', 'down', 0),
         ('glance-left', 'kp7', 0), ('glance-right', 'kp9', 0),
         ('twisted-tilt', 'up', 35)]


def run(game, binary, out, edition, case, size):
    name, key, twist = case
    w, h = map(int, size.split('x'))
    dest = out/f'{edition}-{name}-{size}'
    dest.mkdir(parents=True, exist_ok=True)
    frame = dest/'view.ppm'
    with tempfile.TemporaryDirectory(prefix='mercs-camera-') as temp:
        base = Path(temp)
        folder = base/'Mercs-game'
        folder.mkdir()
        for tree in ('cd', '3d', 'titanium'):
            (folder/tree).symlink_to(game/tree, target_is_directory=True)
        overlay = folder/'install'/EDITIONS[edition][1]
        overlay.mkdir(parents=True)
        env = {k: v for k, v in os.environ.items() if not k.startswith(('MERCS_', 'MW2_'))}
        env.update(SDL_VIDEO_DRIVER='offscreen', SDL_AUDIO_DRIVER='dummy',
                   LIBGL_ALWAYS_SOFTWARE='1', MESA_SHADER_CACHE_DISABLE='true',
                   MERCS_CONFIG=str(base/'port.cfg'), MERCS_SIZE=size,
                   MERCS_RENDER='0x0', MERCS_STATE_DUMP='/0.5',
                   MERCS_NO_STARTUP='1', MERCS_TEST_FPS='30',
                   MERCS_TEST_PASSIVE='1', MERCS_TEST_SKY_STILL='1',
                   MERCS_AUTOPILOT=f'2.5,0,{twist},0,{frame},1,0')
        if key:
            env['MERCS_HOLD'] = '0-3:'+key
        prepared = subprocess.run([str(ROOT/'test_mercs_roundtrip'),
            str(game/EDITIONS[edition][0]), str(overlay), edition, 'prepare'],
            cwd=ROOT, env=env, capture_output=True, text=True, timeout=30)
        assert prepared.returncode == 0, prepared.stdout+prepared.stderr
        p = subprocess.run([str(binary), str(folder), '--mission', 'TRA1SCN1',
                            '--edition', edition], cwd=ROOT, env=env,
                           capture_output=True, text=True, timeout=180)
        log = p.stdout+p.stderr
        (dest/'run.log').write_text(log)
        assert p.returncode == 0, f'{dest.name}: mission exit {p.returncode}'
        states = [dict(re.findall(r'(\w+)=([^\s]+)', line))
                  for line in log.splitlines() if line.startswith('STATE ')]
        assert states and float(states[-1]['t']) >= 2.49, dest.name
        s = states[-1]
        pitch = float(s['pitch'])
        vx, vy = map(float, s['viewaim'].split(','))
        ax, ay = map(float, s['weaponaim'].split(','))
        px, py, visible = map(float, s['reticle'].split(','))
        assert int(s['pow']) == 2 and s['hud_off'] == '0', dest.name
        if name == 'neutral':
            assert visible and abs(px-w/2) <= 1 and abs(py-(h/2-1)) <= 1, (dest.name, s)
        elif name.startswith('tilt') or name == 'twisted-tilt':
            assert abs(pitch) > 20, (dest.name, 'input did not move pitch', s)
            assert abs(vy-(pitch*0.5-float(s['jolt']))) < 0.15, (dest.name, s)
            assert visible and abs(py-(h/2-1)) > h*0.1, (dest.name, s)
            assert (py < h/2) == (pitch > 0), (dest.name, s)
        elif name.startswith('glance'):
            assert not visible and abs(vx-ax) > 60, (dest.name, s)
        return dict(edition=edition, case=name, size=size, pitch=pitch,
                    view_angles=[vx, vy], aim_angles=[ax, ay],
                    reticle_pixel=[px, py], reticle_visible=bool(visible),
                    image=str(frame.relative_to(out)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game', type=Path)
    p.add_argument('--mercs', type=Path, default=ROOT/'mercs')
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    jobs = [(edition, case, '640x480') for edition in EDITIONS for case in CASES]
    jobs += [(edition, CASES[0], '1280x720') for edition in EDITIONS]
    jobs += [(edition, CASES[0], '1280x960') for edition in EDITIONS]
    with ThreadPoolExecutor(max_workers=2) as pool:
        rows = list(pool.map(lambda job: run(a.game.resolve(), a.mercs.resolve(), a.out, *job), jobs))
    (a.out/'summary.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(f'PASS {len(rows)} production camera/reticle missions in three looks; half pitch, glance clipping, aim projection, both aspect ratios and HUD scaling above768lines')


if __name__ == '__main__':
    main()
