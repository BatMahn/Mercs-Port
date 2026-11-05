#!/usr/bin/env python3
"""Capture native Mercs movement audio dispatch with private game overlays.

Example: python3 tests/run_movement_audio_checks.py --game-dir /path/Mercs-game
 --binary ./mercs --prepare ./test_mercs_roundtrip --output-dir /tmp/audio-after
 --baseline /tmp/audio-before/results.json

Uses the real scene, animation, input, sound decoder and dispatch. SDL's dummy
audio device and the existing simulation-clock hook make queue ordering
repeatable; this is not a claim of listening on a Mac or original Miles mixing.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', required=True, type=Path)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--prepare', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    game, binary, prepare, output = (p.resolve() for p in
                                    (args.game_dir, args.binary, args.prepare, args.output_dir))
    assert game not in output.parents and output != game
    output.mkdir(parents=True, exist_ok=False)
    archives = {name: game / path for name, path in
                (('dos', 'cd/MERCS/MW2.PRJ'), ('3dfx', '3d/models.prj'), ('direct3d', 'titanium/MW2.PRJ'))}
    report = dict(scope=__doc__, platform=platform.platform(), machine=platform.machine(),
                  binary_sha256=sha(binary), prepare_sha256=sha(prepare),
                  archives={name: sha(path) for name, path in archives.items()}, captures=[])
    cases = [('run', '1', None),
             ('stop-start', '-9', '2:6,8:1,10:6,16:1,18:6,24:1')]
    for name, throttle, keys in cases:
        with tempfile.TemporaryDirectory(prefix='mercs-movement-audio-') as temp:
            base = Path(temp); private = base / 'Mercs-game'; private.mkdir()
            for tree in ('cd', '3d', 'titanium'):
                (private / tree).symlink_to(game / tree, target_is_directory=True)
            overlay = private / 'install/direct3d'; overlay.mkdir(parents=True)
            env = {k: v for k, v in os.environ.items() if not k.startswith(('MERCS_', 'MW2_'))}
            env.update(SDL_VIDEO_DRIVER='offscreen', SDL_AUDIO_DRIVER='dummy',
                       LIBGL_ALWAYS_SOFTWARE='1', MESA_SHADER_CACHE_DISABLE='true',
                       MERCS_CONFIG=str(base / 'port.cfg'), MERCS_SIZE='320x240', MERCS_RENDER='0x0',
                       MERCS_STATE_DUMP='1', MERCS_SFX_LOG='1', MERCS_SFX_SIMCLOCK='1',
                       MERCS_STEP_TRACE='1', MERCS_RADIO_TRACE='1', MERCS_TEST_FPS='30',
                       MERCS_AUTOPILOT=f'30,{throttle},0,0,{output}/{name}.ppm,1,0')
            if keys:
                env['MERCS_KEYS'] = keys
            prep = subprocess.run([str(prepare), str(archives['direct3d']), str(overlay),
                                   'direct3d', 'prepare'], env=env, capture_output=True, text=True, timeout=30)
            assert not prep.returncode, prep.stdout + prep.stderr
            command = [str(binary), str(private), '--mission', 'TRA1SCN1', '--edition', 'direct3d']
            run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
            text = run.stdout + run.stderr
            (output / f'{name}.log').write_text(text)
            assert not run.returncode, text[-4000:]
            states = [line for line in text.splitlines() if line.startswith('STATE final ')]
            assert len(states) == 1
            state = dict(x.split('=', 1) for x in states[0].split() if '=' in x)
            assert state['pmech'] == 'COMMANDO' and state['camode'] == '0'
            sounds = Counter(re.findall(r'SFXLOG [\d.]+ snd (\d+)', text))
            radio = re.findall(r'^radio (?!node)(\S+)', text, re.M)
            messages = [line for line in text.splitlines() if line.startswith('MSGV ')]
            # These quiet, unfired TRA1 routes isolate the reported gait bug.
            # Keep the acceptance gate active even without an older binary's
            # capture: a trace alone does not prove the bad resources stopped.
            assert sounds.get('259', 0) == 0, 'inherited ricochet-as-step remains'
            assert not any(sounds.get(str(v), 0) for v in (301, 302, 328, 335)), 'inherited gait speech remains'
            assert sounds.get('244', 0) > 0, 'authored Commando footstep missing'
            assert sounds.get('282', 0) > 0, 'other-mech footsteps missing'
            report['captures'].append(dict(name=name, sound_counts=dict(sounds),
                                           player_steps=len(re.findall(r'^footstep key', text, re.M)),
                                           radio=radio, messages=messages, state=state,
                                           log_sha256=sha(output / f'{name}.log'),
                                           original_gait_ids_checked=True))
            print(name, dict(sounds), flush=True)
    assert report['binary_sha256'] == sha(binary)
    assert report['archives'] == {name: sha(path) for name, path in archives.items()}
    report['binary_unchanged'] = report['original_archives_unchanged'] = True
    if args.baseline:
        before = json.loads(args.baseline.read_text())
        comparisons = []
        for previous, current in zip(before['captures'], report['captures']):
            assert previous['name'] == current['name']
            a, b = previous['sound_counts'], current['sound_counts']
            assert previous['state'] == current['state'], 'movement state changed'
            assert previous['player_steps'] == current['player_steps']
            assert previous['radio'] == current['radio'] and previous['messages'] == current['messages']
            assert b.get('259', 0) == 0, 'inherited ricochet ID remains'
            assert b.get('282', 0) >= a.get('259', 0), 'other-mech footsteps disappeared'
            assert not any(b.get(str(sound), 0) for sound in (301, 302, 328, 335)), 'inherited gait speech remains'
            assert b.get('244', 0) == a.get('244', 0) > 0, 'correct player footsteps changed'
            comparisons.append(dict(name=current['name'], passed=True, state_fields=len(current['state']),
                                     all_state_fields_unchanged=True, player_steps_unchanged=True,
                                     authored_radio_and_messages_unchanged=True))
        report['baseline_comparison'] = comparisons
        report['baseline_sha256'] = sha(args.baseline)
    report['pass'] = True
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: native movement traces complete; protected archives unchanged.')


if __name__ == '__main__':
    main()
