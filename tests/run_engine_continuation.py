#!/usr/bin/env python3
"""Build and run the Mercs decomp continuation's bounded native checks.

Original game inputs are read-only. Generated launch files and configuration
are confined to a temporary directory. This does not launch a GUI or establish
campaign completion or hardware-edition engine parity.
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


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game', required=True, type=Path)
    p.add_argument('--dos-elf', type=Path)
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    project = Path(__file__).resolve().parents[1]
    game = args.game.resolve()
    output = (args.output or project / 'build/validation/engine-continuation').resolve()
    archives = {'dos': game / 'cd/MERCS/MW2.PRJ',
                '3dfx': game / '3d/models.prj',
                'direct3d': game / 'titanium/MW2.PRJ'}
    dll = game / 'cd/MERCS/MERCSW.DLL'
    exe = game / 'cd/MERCS/MW2.EXE'
    titanium8 = game / 'titanium/8bit/MW2.PRJ'
    for path in (*archives.values(), titanium8, dll, exe):
        if not path.is_file():
            p.error(f'Missing original input: {path}')
    output.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('MERCS_')}
    env['SDL_AUDIO_DRIVER'] = 'dummy'
    records = []
    targets = ['mercs', 'test_mercs_return', 'test_mercs_ai_trait',
               'test_mercs_ai_transport', 'test_mercs_prj_checksum', 'test_mercs_effects', 'test_mercs_fx_area',
               'test_mercs_combat', 'test_mercs_loadout', 'test_mercs_timing',
               'test_mercs_resources', 'test_mercs_shell_data', 'test_mercs_shell_sim']

    def run(name, command, *, contains=(), timeout=60):
        started = time.monotonic()
        result = subprocess.run([str(v) for v in command], cwd=project, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=timeout)
        log = output / (name + '.log')
        log.write_text(result.stdout)
        passed = result.returncode == 0 and all(s in result.stdout for s in contains)
        records.append({'name': name, 'command': [str(v) for v in command],
                        'exit_code': result.returncode, 'passed': passed,
                        'seconds': round(time.monotonic() - started, 3),
                        'log': log.name, 'log_sha256': digest(log)})
        print(('PASS' if passed else 'FAIL') + ': ' + name, flush=True)
        if not passed:
            raise RuntimeError(f'{name} failed; see {log}\n{result.stdout}')

    with tempfile.TemporaryDirectory(prefix='mercs-engine-followup-') as scratch:
        scratch = Path(scratch)
        env['MERCS_CONFIG'] = str(scratch / 'port.cfg')
        dos = args.dos_elf.resolve() if args.dos_elf else scratch / 'retail-dos.elf'
        if not args.dos_elf:
            subprocess.run([sys.executable, str(project / 'tools/le2elf.py'),
                            str(exe), str(dos)], check=True, cwd=project,
                           stdout=subprocess.DEVNULL, timeout=30)
        run('build', ['make', '-j4', *targets], timeout=180)
        for name in ('ai_trait', 'effects', 'fx_area', 'combat'):
            run(name, [project / ('test_mercs_' + name)])
        run('loadout', [project / 'test_mercs_loadout', archives['dos']])
        run('timing', [project / 'test_mercs_timing', archives['dos'], dll, dos])
        run('known-archive-checksum', [project / 'test_mercs_prj_checksum',
                                     archives['dos'], archives['3dfx'], archives['direct3d'],
                                     titanium8])
        for edition, archive in archives.items():
            run(edition + '-return', [project / 'test_mercs_return', archive, edition])
            npc_overlay = scratch / (edition + '-npc-transport')
            npc_overlay.mkdir()
            run(edition + '-ai-transport', [project / 'test_mercs_ai_transport', archive, npc_overlay])
            run(edition + '-resources', [project / 'test_mercs_resources', archive])
            for name in ('shell_data', 'shell_sim'):
                overlay = scratch / (edition + '-' + name)
                overlay.mkdir()
                command = [project / ('test_mercs_' + name), archive, overlay]
                if name == 'shell_data':
                    command.append(edition)
                run(edition + '-' + name, command)
            run(edition + '-preflight', [project / 'mercs', game, '--edition', edition,
                                        '--mission', 'OUT3SCN1', '--inspect'],
                contains=(('32' if edition == 'dos' else '33') + ' parts, 11 nav points, track 17',
                          'kodiak / kdk00std'))
        if (scratch / 'port.cfg').exists():
            raise RuntimeError('Read-only preflight wrote the configuration')
        inputs = {str(path.relative_to(game)): digest(path)
                  for path in (*archives.values(), titanium8, dll, exe)}
        inputs['converted_dos_analysis_elf'] = digest(dos)
    relevant = ['src/mercsreturn.c', 'src/mercsreturn.h', 'src/combat.c', 'src/combat.h',
                'src/ai.c', 'src/ai.h', 'src/mech3d.c', 'src/mech3d.h', 'src/prj.c', 'src/prj.h',
                'src/fx.c', 'src/fx.h', 'src/msim.c', 'src/msim.h', 'tools/glview.c',
                'tests/test_mercs_return.c', 'tests/test_mercs_ai_trait.c',
                'tests/test_mercs_ai_transport.c', 'tests/test_mercs_prj_checksum.c',
                'tests/test_mercs_shell_data.c',
                'tests/run_engine_continuation.py', 'Makefile']
    record = {'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'scope': 'Bounded native return/AI/effect continuation and three-profile initial-contract regression; no full mission, GUI, campaign, Mac or Windows run',
              'platform': platform.platform(), 'machine': platform.machine(),
              'build_passed': records[0]['passed'],
              'check_groups': len(records) - 1,
              'all_passed': all(r['passed'] for r in records),
              'inputs_sha256': inputs,
              'source_sha256': {name: digest(project / name) for name in relevant},
              'native_mercs_sha256': digest(project / 'mercs'),
              'checks': records}
    (output / 'native-results.json').write_text(json.dumps(record, indent=2) + '\n')
    print(f"PASS: build and {record['check_groups']} focused native groups; {output / 'native-results.json'}", flush=True)


if __name__ == '__main__':
    main()
