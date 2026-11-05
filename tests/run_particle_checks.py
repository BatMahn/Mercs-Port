#!/usr/bin/env python3
"""Controlled real TRA1 missions: toggle only PARTICLES, compare game states,
observe live emissions and rendered frames in all three edition profiles.
Requires mercs + test_mercs_roundtrip and external game assets. Linux SDL/Mesa
captures are not an original-game or macOS equivalence claim.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile

from run_combat_variables_checks import records, fixture, environment, checked

ROOT = Path(__file__).resolve().parents[1]


def mission(game, binary, edition, enabled, output):
    tag = f'{edition}-{"on" if enabled else "off"}'
    with tempfile.TemporaryDirectory(prefix=f'mercs-particles-{tag}-') as tmp:
        base = Path(tmp); folder = fixture(base, game); env = environment(base)
        overlay = folder / 'install' / ('retail' if edition == 'dos' else edition)
        overlay.mkdir()
        archive = {'dos':game/'cd/MERCS/MW2.PRJ', '3dfx':game/'3d/models.prj',
                   'direct3d':game/'titanium/MW2.PRJ'}[edition]
        checked([ROOT/'test_mercs_roundtrip', archive, overlay, edition, 'prepare'], env)
        snd, dif = records(True)
        struct.pack_into('<I', snd, 0x28, int(enabled))
        (overlay/'MW2SND.CFG').write_bytes(snd); (overlay/'MW2DIF.CFG').write_bytes(dif)
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
        snapshots = [base/f'frame-{i}.ppm' for i in range(3)]
        env.update(MERCS_SIZE='800x600', MERCS_NO_STARTUP='1', MERCS_TEST_FPS='30',
                   MERCS_TEST_SKY_STILL='1',
                   MERCS_STATE_DUMP='/0.2', MERCS_OPTIONS_TRACE='1', MERCS_PARTICLE_TRACE='1',
                   MERCS_TEST_PASSIVE='1',
                   MERCS_SHOT=','.join(f'{t}:{p}' for t,p in zip((0.5,1,2),snapshots)),
                   MERCS_AUTOPILOT=f'6,0,0,1,{base/"final.ppm"},1,0')
        log = checked([binary, folder, '--mission', 'TRA1SCN1', '--edition', edition], env)
        traces = [dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',line))
                  for line in log.splitlines() if line.startswith('particles ')]
        states = [line for line in log.splitlines() if line.startswith('STATE')]
        assert states and 't=6.' in states[-1], (tag, 'incomplete six-second mission')
        assert traces, (tag, 'no particle trace')
        if edition in ('dos','3dfx'):
            assert max(t['primitives'] for t in traces) > 0 if enabled else all(t['primitives']==0 for t in traces)
            assert max(t['missile'] for t in traces) > 0 if enabled else all(t['missile']==0 and t['ballistic']==0 for t in traces)
            assert all(t['enabled']==int(enabled) for t in traces)
        else:
            assert all(t['primitives']==0 for t in traces), 'Titanium entered palette renderer'
            assert ('titanium: missile smoke from' in log) == enabled
        assert 'no particle.PID' not in log, (tag, 'cross-edition missing PID probe')
        pixels = [p.read_bytes().split(b'\n',3)[3] for p in snapshots]
        if output:
            (output/f'{tag}.log').write_text(log)
            for i,p in enumerate(snapshots): (output/f'{tag}-{i}.ppm').write_bytes(p.read_bytes())
        assert (overlay/'MW2SND.CFG').read_bytes()==snd
        assert (overlay/'MW2DIF.CFG').read_bytes()==dif
        return dict(edition=edition, enabled=enabled, state_samples=len(states),
                    state_sha256=hashlib.sha256('\n'.join(states).encode()).hexdigest(),
                    max_sampled_emitters=max(t['emitters'] for t in traces),
                    max_sampled_primitives=max(t['primitives'] for t in traces),
                    emitted_missile=max(t['missile'] for t in traces),
                    emitted_ballistic=max(t['ballistic'] for t in traces),
                    titanium_pid_active='titanium: missile smoke from' in log,
                    states=states, pixels=pixels)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,required=True)
    ap.add_argument('--mercs',type=Path,default=ROOT/'mercs')
    ap.add_argument('--output',type=Path)
    args=ap.parse_args(); out=args.output.resolve() if args.output else None
    if out: out.mkdir(parents=True,exist_ok=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        rows=list(pool.map(lambda x:mission(args.game.resolve(),args.mercs.resolve(),*x,out),
                           [(e,on) for e in ('dos','3dfx','direct3d') for on in (True,False)]))
    differences={}; trail_differences={}
    for edition in ('dos','3dfx','direct3d'):
        on,off=[r for r in rows if r['edition']==edition]
        assert on['states']==off['states'], (edition, 'PARTICLES changed simulation state')
        differences[edition]=[sum(a[i:i+3]!=b[i:i+3] for i in range(0,len(a),3))
                              for a,b in zip(on['pixels'],off['pixels'])]
        # Freeze both sky-scroll paths, and independently require a difference
        # in the fixed trail window below the horizon. Gameplay/HUD state is
        # also compared byte-for-byte in the STATE trace above.
        trail_differences[edition]=[
            sum(a[(y*800+x)*3:(y*800+x)*3+3] != b[(y*800+x)*3:(y*800+x)*3+3]
                for y in range(390,575) for x in range(340,480))
            for a,b in zip(on['pixels'],off['pixels'])]
        assert max(trail_differences[edition])>0, (edition, 'no rendered trail in the sky-free region')
    for row in rows: del row['states']; del row['pixels']
    result=dict(scope=__doc__, cases=rows,
                whole_frame_changed_pixels_at_0_5_1_2_seconds=differences,
                whole_frame_note='MERCS_TEST_SKY_STILL freezes both sky-scroll paths; only PARTICLES changes between matched runs.',
                trail_region_xyxy=[340,390,480,575],
                trail_region_changed_pixels_at_0_5_1_2_seconds=trail_differences, passed=True)
    report=json.dumps(result,indent=2)+'\n'
    if out: (out/'results.json').write_text(report)
    print(report,end='')


if __name__=='__main__': main()
