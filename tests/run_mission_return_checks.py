#!/usr/bin/env python3
"""Verify Mercs mission-return integration with original inputs kept read-only.

Includes original-code golden tests, controlled live-state round trips, profile
changes and real SDL/OpenGL application children. The latter exit after two
seconds under the existing autopilot hook; they are not full mission wins.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game', required=True, type=Path)
    p.add_argument('--dos-elf', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--skip-regression', action='store_true', help='development iteration only')
    args = p.parse_args()
    project = Path(__file__).resolve().parents[1]
    game = args.game.resolve()
    output = (args.output or project / 'build/validation/mission-return-validation').resolve()
    output.mkdir(parents=True, exist_ok=True)
    archives = {'dos': game / 'cd/MERCS/MW2.PRJ',
                '3dfx': game / '3d/models.prj',
                'direct3d': game / 'titanium/MW2.PRJ'}
    originals = [*archives.values(), game / 'cd/MERCS/MW2.EXE',
                 game / 'cd/MERCS/MERCSW.DLL', game / 'cd/MERCS/MW2SHELL.DLL',
                 game / 'cd/MERCS/MW2SHELL.EXE']
    before = {str(f.relative_to(game)): sha(f) for f in originals}
    env = {k: v for k, v in os.environ.items() if not k.startswith('MERCS_')}
    env['SDL_AUDIO_DRIVER'] = 'dummy'
    if sys.platform.startswith('linux'):
        env['SDL_VIDEO_DRIVER'] = 'offscreen'
        env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    records = []

    def run(name, command, *, contains=(), extra_env=None, timeout=90):
        started = time.monotonic()
        result = subprocess.run([str(v) for v in command], cwd=project,
                                env={**env, **(extra_env or {})}, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout)
        log = output / (name + '.log')
        log.write_text(result.stdout)
        passed = result.returncode == 0 and all(s in result.stdout for s in contains)
        records.append({'name': name, 'exit_code': result.returncode, 'passed': passed,
                        'seconds': round(time.monotonic() - started, 3),
                        'log': log.name, 'log_sha256': sha(log)})
        print(('PASS' if passed else 'FAIL') + ': ' + name, flush=True)
        if not passed:
            raise RuntimeError(f'{name}: {result.stdout[-12000:]}')
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='mercs-return-check-') as scratch:
        scratch = Path(scratch)
        env['MERCS_CONFIG'] = str(scratch / 'port.cfg')
        if not args.skip_regression:
            command = [sys.executable, project / 'tests/run_engine_continuation.py',
                       '--game', game, '--output', output / 'regression']
            if args.dos_elf:
                command += ['--dos-elf', args.dos_elf.resolve()]
            run('prior-regression', command, contains=('build and 25 focused native groups',), timeout=240)
        run('build-return', ['make', '-j4', 'mercs', 'test_mercs_results',
                            'test_mercs_result_transport', 'test_mercs_career',
                            'test_mercs_exchange', 'test_mercs_mission_return',
                            'test_mercs_roundtrip'], timeout=180)
        for name in ('results', 'result_transport', 'career'):
            run(name, [project / ('test_mercs_' + name)])
        exchange = scratch / 'exchange'; exchange.mkdir()
        run('exchange', [project / 'test_mercs_exchange', exchange])
        names = list(archives)
        for i, (edition, archive) in enumerate(archives.items()):
            run(edition + '-adapter', [project / 'test_mercs_mission_return', archive])
            overlay = scratch / (edition + '-roundtrip'); overlay.mkdir()
            run(edition + '-roundtrip', [project / 'test_mercs_roundtrip', archive, overlay, edition],
                contains=('12 complete real-data round trips',))
            other = names[(i + 1) % len(names)]
            run(edition + '-profile-change', [project / 'test_mercs_roundtrip', archive, overlay,
                                             edition, 'refresh', archives[other], other],
                contains=('current/reference/results survive',))

        # Real directories and read-only asset references keep every application
        # write inside this private game root. No original install/save folder is
        # mounted as the child's writable overlay; no original file is changed.
        private_game = scratch / 'Mercs-game'; private_game.mkdir()
        for root, directories, files in os.walk(game):
            relative = Path(root).relative_to(game)
            if relative == Path('.'):
                directories[:] = [d for d in directories if d != 'install']
            dest = private_game / relative
            dest.mkdir(parents=True, exist_ok=True)
            for name in files:
                (dest / name).symlink_to((Path(root) / name).resolve())
        for edition, archive in archives.items():
            overlay = private_game / 'install' / ('retail' if edition == 'dos' else edition)
            overlay.mkdir(parents=True)
            prepared = run(edition + '-child-prepare', [project / 'test_mercs_roundtrip',
                            archive, overlay, edition, 'prepare'])
            token = prepared.strip().splitlines()[-1]
            if len(token) != 32 or any(c not in '0123456789abcdef' for c in token):
                raise RuntimeError('Invalid prepared launch token')
            frame = scratch / (edition + '-child.ppm')
            run(edition + '-actual-child', [project / 'mercs', private_game,
                '--mission', 'BET1SCN1', '--edition', edition, '--return-token', token],
                contains=('committed mission return', 'autopilot:'),
                extra_env={'MERCS_AUTOPILOT': f'2,0,0,0,{frame},1,0',
                           'MERCS_SIZE': '640x480', 'MERCS_TEST_FPS': '30'}, timeout=120)
            if not frame.is_file() or frame.stat().st_size < 640 * 480 * 3:
                raise RuntimeError('Application did not render the expected frame')
            records[-1]['rendered_frame_sha256'] = sha(frame)
            records[-1]['rendered_frame_bytes'] = frame.stat().st_size
            run(edition + '-child-consume', [project / 'test_mercs_roundtrip', archive,
                overlay, edition, 'consume', token], contains=('actual application child',))
    after = {str(f.relative_to(game)): sha(f) for f in originals}
    if before != after:
        raise RuntimeError('An original input changed')
    sources = [*project.joinpath('src').glob('*.c'), *project.joinpath('src').glob('*.h'),
               project / 'tools/mercs.c', project / 'tools/mercsshell.c', project / 'tools/glview.c',
               project / 'Makefile', Path(__file__).resolve(),
               *project.joinpath('tests').glob('test_mercs_*return*.c'),
               project / 'tests/test_mercs_roundtrip.c', project / 'tests/test_mercs_exchange.c']
    record = {'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'platform': platform.platform(), 'machine': platform.machine(),
              'scope': '36 controlled real-data return/redeploy cycles, six profile-preservation cases, '
                       'three real SDL/OpenGL two-second child runs and shell imports; '
                       'not full mission wins, campaign completion, debrief settlement, Mac or Windows execution',
              'all_passed': all(r['passed'] for r in records),
              'prior_regression_included': not args.skip_regression,
              'controlled_roundtrips': 36, 'actual_application_runs': 3,
              'original_inputs_unchanged': True, 'inputs_sha256': before,
              'native_mercs_sha256': sha(project / 'mercs'),
              'source_sha256': {str(f.relative_to(project)): sha(f) for f in sorted(set(sources))},
              'checks': records}
    (output / 'native-results.json').write_text(json.dumps(record, indent=2) + '\n')
    print(f"PASS: {len(records)} check stages; 36 controlled round trips and 3 actual application children", flush=True)


if __name__ == '__main__':
    main()
