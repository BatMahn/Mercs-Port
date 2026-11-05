#!/usr/bin/env python3
"""Verify career integration with original inputs kept read-only.

Runs the preceding engine/return regression, new original-code golden fixtures,
three archive-specific controlled careers, and real frontend handlers rendered
with the original shell artwork. Controlled outcomes are not full playthroughs.
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--dos-elf', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--skip-regression', action='store_true', help='development iteration only')
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    game = args.game.resolve()
    output = (args.output or project / 'build/validation/campaign-validation').resolve()
    output.mkdir(parents=True, exist_ok=True)
    archives = {'dos': game / 'cd/MERCS/MW2.PRJ',
                '3dfx': game / '3d/models.prj',
                'direct3d': game / 'titanium/MW2.PRJ'}
    originals = [*archives.values(), *(game / 'cd/MERCS' / n for n in
                 ('MW2.EXE', 'MERCSW.DLL', 'MW2SHELL.EXE', 'MW2SHELL.DLL', 'DATABASE.MW2'))]
    before = {str(p.relative_to(game)): sha(p) for p in originals}
    records = []
    env = {k: v for k, v in os.environ.items() if not k.startswith('MERCS_')}
    env['SDL_AUDIO_DRIVER'] = 'dummy'
    if sys.platform.startswith('linux'):
        env.update(SDL_VIDEO_DRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')

    def run(name, command, *, contains=(), timeout=120):
        started = time.monotonic()
        result = subprocess.run([str(x) for x in command], cwd=project, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=timeout)
        log = output / (name + '.log'); log.write_text(result.stdout)
        passed = result.returncode == 0 and all(x in result.stdout for x in contains)
        records.append({'name': name, 'passed': passed, 'exit_code': result.returncode,
                        'seconds': round(time.monotonic() - started, 3),
                        'log': log.name, 'log_sha256': sha(log)})
        print(('PASS' if passed else 'FAIL') + ': ' + name, flush=True)
        if not passed:
            raise RuntimeError(result.stdout[-16000:])

    with tempfile.TemporaryDirectory(prefix='mercs-career-check-') as scratch:
        scratch = Path(scratch)
        env['MERCS_CONFIG'] = str(scratch / 'port.cfg')
        if not args.skip_regression:
            command = ['python3', project / 'tests/run_mission_return_checks.py', '--game', game,
                       '--output', output / 'return-regression']
            if args.dos_elf:
                command += ['--dos-elf', args.dos_elf.resolve()]
            run('prior-return-and-engine', command, contains=('actual application children',), timeout=600)
        run('build-campaign', ['make', '-j4', 'mercs', 'test_mercs_save', 'test_mercs_debrief',
                              'test_mercs_repair', 'test_mercs_repair_view', 'test_mercs_statistics',
                              'test_mercs_debrief_view', 'test_mercs_debrief_assets',
                              'test_mercs_campaign', 'test_mercs_campaign_tail', 'test_mercs_frontend'], timeout=240)
        for name in ('save', 'debrief', 'debrief_view', 'repair', 'repair_view', 'statistics'):
            run(name + '-original-goldens', [project / ('test_mercs_' + name)])
        for name, archive in archives.items():
            run(name + '-debrief-assets', [project / 'test_mercs_debrief_assets', archive,
                                          game / 'cd/MERCS/DATABASE.MW2'])
            overlay = scratch / ('career-' + name); overlay.mkdir()
            run(name + '-campaign', [project / 'test_mercs_campaign', archive, overlay])
            run(name + '-campaign-tail', [project / 'test_mercs_campaign_tail', archive])
        private = scratch / 'Mercs-game'; private.mkdir()
        for root, dirs, files in os.walk(game):
            relative = Path(root).relative_to(game)
            if relative == Path('.'):
                dirs[:] = [d for d in dirs if d != 'install']
            dest = private / relative; dest.mkdir(parents=True, exist_ok=True)
            for name in files:
                (dest / name).symlink_to((Path(root) / name).resolve())
        frames = scratch / 'frames'; frames.mkdir()
        run('original-screen-frontend', [project / 'test_mercs_frontend', private, frames],
            contains=('controlled original-screen frontend journey',), timeout=240)
        rendered = []
        for f in sorted(frames.glob('*.bmp')):
            if f.stat().st_size < 640 * 480 * 3:
                raise RuntimeError('Incomplete rendered frontend frame: ' + f.name)
            rendered.append({'name': f.name, 'bytes': f.stat().st_size, 'sha256': sha(f)})
        if len(rendered) < 9:
            raise RuntimeError('Missing frontend captures')
        # Screens contain original supplied artwork; record hashes, do not
        # bundle game artwork into a source-only handoff.
        records[-1]['rendered_frames'] = rendered
    after = {str(p.relative_to(game)): sha(p) for p in originals}
    if after != before:
        raise RuntimeError('An original input changed')
    sources = [*project.joinpath('src').glob('*.c'), *project.joinpath('src').glob('*.h'),
               *project.joinpath('tools').glob('mercs*.*'), project / 'tools/glview.c',
               project / 'Makefile', Path(__file__).resolve(),
               *project.joinpath('tests').glob('test_mercs_*.c')]
    report = {'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'platform': platform.platform(), 'machine': platform.machine(),
              'scope': 'Original DOS career transitions and DOS/Windows repair/statistic goldens; '
                       'three-archive controlled save/debrief/settlement journeys; actual original-art '
                       'frontend handlers with controlled simulator outcomes; no complete campaign, '
                       'Mac or Windows execution claim',
              'all_passed': all(r['passed'] for r in records),
              'prior_return_and_engine_included': not args.skip_regression,
              'original_inputs_unchanged': True, 'original_input_sha256': before,
              'native_mercs_sha256': sha(project / 'mercs'),
              'source_sha256': {str(p.relative_to(project)): sha(p) for p in sorted(set(sources))},
              'checks': records}
    (output / 'native-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {len(records)} career validation stages; original inputs unchanged', flush=True)


if __name__ == '__main__':
    main()
