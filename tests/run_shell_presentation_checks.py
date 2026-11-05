#!/usr/bin/env python3
"""Check shell presentation and input with private configuration and outputs.

Builds/runs viewport, video, embedded art/audio, DATABASE audio, original dialog,
Escape-menu, panels and real shell interaction fixtures. Original-code oracles
and campaign/mission-return suites remain separate. Only logs and numerical/hash
metadata are exported; screenshots containing original artwork stay temporary.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
import tempfile
import time


TARGETS = (
    'test_mercs_viewport', 'test_mercs_video', 'test_mercs_art',
    'test_mercs_shell_audio', 'test_mercs_dialog', 'test_mercs_gamemenu',
    'test_mercs_panels', 'test_mercs_shell_interaction', 'test_mercs_startup',
    'test_mercs_combat_variables',
)
ASSET_TREES = ('cd', '3d', 'titanium')
REQUIRED_INPUTS = (
    'cd/MERCS/DATABASE.MW2', 'cd/MERCS/MW2.PRJ', 'cd/MERCS/MW2.EXE',
    'cd/MERCS/MW2SHELL.EXE', 'cd/SMK/MINTRO.SMK',
    'cd/SMK/SPA1BRF1.SMK', 'cd/SMK/MLAND.SMK',
)


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def asset_files(game):
    """Only immutable asset trees are exposed; no user's install/config/save."""
    files = []
    for tree in ASSET_TREES:
        for directory, directories, names in os.walk(game / tree):
            directories.sort()
            for name in sorted(names):
                path = Path(directory) / name
                if path.is_file():
                    files.append(path)
    return sorted(files)


def source_files(project):
    files = [project / 'Makefile', Path(__file__).resolve()]
    for tree in ('src', 'tools', 'tests', 'third_party'):
        files.extend(path for path in (project / tree).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.py'))
    # Some native fixtures include generated numerical goldens from this tree.
    files.extend((project / 'tests/fixtures').rglob('*.h'))
    return sorted(set(files))


def hashes(files, root):
    return {str(path.relative_to(root)): sha(path) for path in files}


def private_game(game, originals, destination):
    destination.mkdir()
    for source in originals:
        target = destination / source.relative_to(game)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.symlink_to(source.resolve())
    # These are real private directories, never links to the supplied install.
    (destination / 'install/retail').mkdir(parents=True)


def frames(directory, *, required=()):
    """Check complete SDL BMP captures, then retain only their dimensions/hashes."""
    result = []
    for path in sorted(directory.glob('*.bmp')):
        size = path.stat().st_size
        with path.open('rb') as stream:
            header = stream.read(54)
        if len(header) < 54 or header[:2] != b'BM':
            raise RuntimeError('Invalid rendered BMP: ' + path.name)
        declared, offset = struct.unpack_from('<I', header, 2)[0], struct.unpack_from('<I', header, 10)[0]
        width, height = struct.unpack_from('<ii', header, 18)
        bits = struct.unpack_from('<H', header, 28)[0]
        rows = abs(height)
        stride = ((width * bits + 31) // 32) * 4
        if (width <= 0 or rows <= 0 or bits not in (24, 32) or
                declared != size or offset < 54 or offset + stride * rows > size):
            raise RuntimeError('Incomplete rendered BMP: ' + path.name)
        result.append({'name': path.name, 'width': width, 'height': rows,
                       'bits': bits, 'bytes': size, 'sha256': sha(path)})
    present = {record['name'] for record in result}
    missing = set(required) - present
    if missing:
        raise RuntimeError('Missing rendered captures: ' + ', '.join(sorted(missing)))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help='Supplied Mercs-game directory')
    parser.add_argument('--output', type=Path, help='Logs and native-results.json only')
    parser.add_argument('--jobs', type=int, default=4, help='Parallel make jobs (default: 4)')
    parser.add_argument('--skip-build', action='store_true', help='Run existing test binaries; recorded in report')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    project = Path(__file__).resolve().parents[1]
    game = args.game.resolve()
    output = (args.output or project / 'build/validation/shell-presentation-validation').resolve()
    for name in REQUIRED_INPUTS:
        if not (game / name).is_file():
            parser.error('Required original fixture is missing: ' + str(game / name))
    for tree in ASSET_TREES:
        if output == game / tree or game / tree in output.parents:
            parser.error('--output must be outside the original asset trees')
    output.mkdir(parents=True, exist_ok=True)

    originals = asset_files(game)
    sources = source_files(project)
    before = hashes(originals, game)
    source_before = hashes(sources, project)
    records = []
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('MERCS_', 'MW2_'))}
    env['SDL_AUDIO_DRIVER'] = 'dummy'
    if sys.platform.startswith('linux'):
        env.update(SDL_VIDEO_DRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')
    executable_suffix = '.exe' if os.name == 'nt' else ''

    def executable(name):
        return project / (name + executable_suffix)

    def run(name, command, *, contains=(), timeout=120):
        started = time.monotonic()
        command = [str(part) for part in command]
        exception = None
        try:
            result = subprocess.run(command, cwd=project, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors='replace',
                                    timeout=timeout)
            stdout, code = result.stdout, result.returncode
        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or ''
            if isinstance(stdout, bytes):
                stdout = stdout.decode('utf-8', errors='replace')
            exception = 'Timed out after %s seconds' % timeout
            stdout += '\n' + exception + '\n'
            code = None
        except OSError as error:
            stdout, code = str(error) + '\n', None
            exception = str(error)
        log = output / (name + '.log')
        log.write_text(stdout, encoding='utf-8')
        passed = code == 0 and all(text in stdout for text in contains)
        record = {'name': name, 'passed': passed, 'exit_code': code, 'command': command,
                  'seconds': round(time.monotonic() - started, 3),
                  'log': log.name, 'log_sha256': sha(log)}
        if exception:
            record['error'] = exception
        records.append(record)
        print(('PASS' if passed else 'FAIL') + ': ' + name, flush=True)
        if not passed:
            raise RuntimeError(name + '\n' + stdout[-16000:])
        return record, stdout

    error = None
    completed = False
    inputs_unchanged = sources_unchanged = False
    after = source_after = {}
    try:
        with tempfile.TemporaryDirectory(prefix='mercs-presentation-check-') as temporary:
            scratch = Path(temporary)
            private = scratch / 'Mercs-game'
            private_game(game, originals, private)
            env['MERCS_CONFIG'] = str(scratch / 'port.cfg')
            env['MERCS_INSTALL_DIR'] = str(private / 'install/retail')
            env['MERCS_CD_DIR'] = str(private / 'cd')
            video_output = scratch / 'video'
            interaction_output = scratch / 'interaction'
            startup_output = scratch / 'startup'
            combat_output = scratch / 'combat-variables'
            video_output.mkdir()
            interaction_output.mkdir()
            startup_output.mkdir()
            combat_output.mkdir()
            if not args.skip_build:
                run('build-presentation', ['make', '-j%d' % args.jobs, *TARGETS], timeout=300)
            run('viewport', [executable('test_mercs_viewport')],
                contains=('test_mercs_viewport: ok',))
            record, _ = run('video', [executable('test_mercs_video'),
                                     private / 'cd/SMK/MINTRO.SMK', video_output],
                            contains=('test_mercs_video: ok',), timeout=180)
            record['rendered_frames'] = frames(video_output, required=(
                'mintro-original-frame600.bmp', 'mintro-crt-frame600.bmp'))
            run('art', [executable('test_mercs_art'), private / 'cd/MERCS/DATABASE.MW2', private / 'cd'],
                contains=('test_mercs_art: ok',))
            run('shell-audio', [executable('test_mercs_shell_audio'), private / 'cd/MERCS/DATABASE.MW2'],
                contains=('73 original WAV PCM goldens',))
            run('dialog', [executable('test_mercs_dialog')],
                contains=('original DOS dialog layout/input goldens',))
            run('game-menu', [executable('test_mercs_gamemenu')],
                contains=('original DOS Escape-menu input/layout goldens',))
            run('panels', [executable('test_mercs_panels'), private / 'cd/MERCS/MW2SHELL.EXE'],
                contains=('original external DOS tables',))
            record, stdout = run('shell-interaction', [executable('test_mercs_shell_interaction'),
                                                       private, interaction_output],
                                 contains=('PASS: original shell interaction',), timeout=120)
            capture_lines = re.findall(r'^CAPTURE (\S+) (\d+)x(\d+) fnv1a64=([0-9a-f]{16})$', stdout, re.M)
            captures = frames(interaction_output, required=(
                'interaction-registration-after-target-reset.bmp',
                'interaction-original-dialog-wide.bmp', 'interaction-original-dialog-pressed.bmp',
                'interaction-video-settings-wide.bmp', 'interaction-original-escape-menu.bmp',
                'interaction-original-save-after-modal.bmp', 'interaction-original-hall-gate.bmp'))
            by_name = {name + '.bmp': (int(width), int(height), pixel_hash)
                       for name, width, height, pixel_hash in capture_lines}
            if not by_name or set(by_name) != {capture['name'] for capture in captures}:
                raise RuntimeError('Interaction log/capture inventory differs')
            for capture in captures:
                width, height, pixel_hash = by_name[capture['name']]
                if (width, height) != (capture['width'], capture['height']):
                    raise RuntimeError('Interaction log/capture dimensions differ: ' + capture['name'])
                capture['native_pixel_fnv1a64'] = pixel_hash
            record['rendered_frames'] = captures
            record, _ = run('startup', [executable('test_mercs_startup'), private, startup_output],
                            contains=('PASS: 18 cold-launch cases',), timeout=120)
            record['rendered_frames'] = frames(startup_output, required=(
                'failed-checkpoint-cold-menu.bmp',))
            record, _ = run('combat-variables', [executable('test_mercs_combat_variables'), private, combat_output],
                            contains=('PASS: original Combat Variables menu dispatch',), timeout=90)
            record['rendered_frames'] = frames(combat_output, required=(
                'combat-original-values.bmp', 'combat-edited-values.bmp'))
            completed = True
    except (RuntimeError, OSError) as caught:
        error = str(caught)
    finally:
        try:
            after = hashes(asset_files(game), game)
            source_after = hashes(source_files(project), project)
            inputs_unchanged = after == before
            sources_unchanged = source_after == source_before
            if not inputs_unchanged:
                error = (error + '\n' if error else '') + 'An original input changed during validation'
            if not sources_unchanged:
                error = (error + '\n' if error else '') + 'Source files changed during validation; rerun after edits finish'
        except OSError as caught:
            error = (error + '\n' if error else '') + 'Could not recheck inputs/sources: ' + str(caught)
        all_passed = completed and not error and all(record['passed'] for record in records)
        report = {
            'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'platform': platform.platform(), 'machine': platform.machine(),
            'scope': 'Native viewport/video renderer lifecycle, exact embedded/WAV PCM and sound ownership, '
                     'original dialog/menu/panel goldens and actual shell event handlers with original artwork. '
                     'No campaign/playthrough, perceptual listening, other-OS or physical Retina hardware claim.',
            'all_passed': all_passed, 'build_included': not args.skip_build,
            'original_inputs_unchanged': inputs_unchanged,
            'source_files_unchanged_during_run': sources_unchanged,
            'input_hash_scope': 'Every file exposed from cd/, 3d/ and titanium/; install and user configuration excluded.',
            'original_input_sha256': before, 'source_sha256': source_before,
            'test_binary_sha256': {name: sha(executable(name)) for name in TARGETS if executable(name).is_file()},
            'environment': {key: env[key] for key in ('SDL_AUDIO_DRIVER', 'SDL_VIDEO_DRIVER', 'LIBGL_ALWAYS_SOFTWARE')
                            if key in env},
            'private_output_policy': 'Private config and writable install overlay; BMP/config/save outputs removed with '
                                     'the temporary directory. Only logs and frame dimensions/hashes are exported.',
            'checks': records,
        }
        if error:
            report['error'] = error
        if after and not inputs_unchanged:
            report['original_input_sha256_after'] = after
        if source_after and not sources_unchanged:
            report['source_sha256_after'] = source_after
        (output / 'native-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not all_passed:
        print('FAIL: shell presentation validation\n' + (error or 'Incomplete checks'), file=sys.stderr)
        return 1
    count = sum(len(record.get('rendered_frames', [])) for record in records)
    print('PASS: %d shell presentation stages; %d frame hashes; original inputs unchanged' %
          (len(records), count), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
