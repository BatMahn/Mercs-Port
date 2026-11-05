#!/usr/bin/env python3
"""Exercise original include-order texture binding in real SDL3 mission runs.

BIG1's native breadth-first loader selected a one-frame world bitmap for slot
0x14c. Expanded original chunk order restores the 32-frame explosion group.
The explicit effect diagnostic checks its shared renderer clock through reset,
animation and stop; it is not an ordinary-play campaign result.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile

from run_combat_variables_checks import fixture, environment, records
from run_turn_audio_checks import checked, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('game', 'binary', 'prepare', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--expect-old-order', action='store_true')
    args = parser.parse_args()
    game, binary, prepare, output = [getattr(args, n).resolve()
                                    for n in ('game', 'binary', 'prepare', 'output')]
    assert game != output and game not in output.parents
    output.mkdir(parents=True, exist_ok=False)
    archives = {edition: game/path for edition, path in (
        ('dos', 'cd/MERCS/MW2.PRJ'), ('3dfx', '3d/models.prj'),
        ('direct3d', 'titanium/MW2.PRJ'))}
    report = dict(scope=__doc__, binary_sha256=sha(binary),
                  prepare_sha256=sha(prepare), expected_old_order=args.expect_old_order,
                  archive_sha256={edition: sha(path) for edition, path in archives.items()},
                  cases=[])
    for edition, archive in archives.items():
        with tempfile.TemporaryDirectory(prefix='mercs-texture-binding-') as temp:
            base = Path(temp); folder = fixture(base, game); env = environment(base)
            overlay = folder/'install'/('retail' if edition == 'dos' else edition)
            overlay.mkdir()
            checked([prepare, archive, overlay, edition, 'prepare'], env)
            sound, difficulty = records(True)
            (overlay/'MW2SND.CFG').write_bytes(sound)
            (overlay/'MW2DIF.CFG').write_bytes(difficulty)
            (base/'port.cfg').write_text(
                f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
            env.update(MERCS_SIZE='640x480', MERCS_TEST_FPS='30', MERCS_NO_STARTUP='1',
                       MERCS_STATE_DUMP='/0.5', MERCS_ANIM_LOG='0x4c',
                       MERCS_FX_TEST='d:1',
                       MERCS_AUTOPILOT=f'5,-9,-999,0,{base}/capture.ppm,1,0')
            log = checked([binary, folder, '--mission', 'BIG1SCN1', '--edition', edition], env)
            logfile = output/f'{edition}.log'; logfile.write_text(log)
            samples = [dict(time=float(t), frame=int(f), frames=int(n), rate=int(r), mode=int(m))
                       for t, f, n, r, m in re.findall(
                           r'^ANIM t=([\d.]+) slot 0x4c frame (\d+) of (\d+) rate (\d+) mode (-?\d+)',
                           log, re.M)]
            times = list(map(float, re.findall(r'^STATE .*?t=([\d.]+)', log, re.M)))
            assert times and max(times) >= 5, (edition, 'short mission')
            assert 'FXTEST type d slot 0' in log, (edition, 'effect diagnostic did not execute')
            if args.expect_old_order:
                assert not samples, (edition, 'old breadth-first slot was not reproduced')
            else:
                assert samples and all(s['frames'] == 32 and s['rate'] == 16 for s in samples)
                assert any(s['mode'] == 1 and s['frame'] > 0 and s['time'] < 1 for s in samples)
                assert any(s['mode'] == 2 and s['frame'] == 0 and 1 <= s['time'] <= 1.2 for s in samples)
                assert any(s['mode'] == 2 and s['frame'] >= 20 for s in samples)
                assert samples[-1]['mode'] == 0 and samples[-1]['frame'] == 0
            report['cases'].append(dict(edition=edition, samples=samples,
                                        log_sha256=sha(logfile)))
            print(edition, 'PASS', len(samples), 'renderer clock samples', flush=True)
    assert report['binary_sha256'] == sha(binary)
    assert report['prepare_sha256'] == sha(prepare)
    assert report['archive_sha256'] == {e: sha(p) for e, p in archives.items()}
    report['passed'] = report['protected_inputs_unchanged'] = True
    (output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: three actual BIG1 mission texture-clock traces; original archives unchanged.')


if __name__ == '__main__':
    main()
