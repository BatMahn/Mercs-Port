#!/usr/bin/env python3
"""Exercise actual left/right input, torso sounds, menus and no-ally orders.

Uses private per-run installs, all three supplied game archives, native SDL3,
offscreen OpenGL and dummy audio. --expect-old-bug captures the preceding
delivery, proving that the acceptance test distinguishes the reported fault.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from run_combat_variables_checks import fixture, environment, records

INPUT = ('legs_pan_minus {\n+ keyboard GreyLeftArrow\n}\n'
         'legs_pan_plus {\n+ keyboard GreyRightArrow\n}\n'
         'torso_pan_minus {\n+ keyboard Comma\n}\n'
         'torso_pan_plus {\n+ keyboard Period\n}\n')
CASES = (
    ('leg-turns', '0.5-2:left,3-4.5:right', '', 1),
    ('torso-turns', '0.5-2:comma,3-4.5:period', '', 1),
    ('external-torso', '0.5-2:comma,3-4.5:period', '', 0),
    ('menu-arrows', '', '0.5:esc,0.7:3,0.9:right,1.1:left,1.3:down,1.5:right,1.7:left,1.9:esc,2.1:esc', 1),
    ('command', '', '1:alt+j', 1),
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snd_record(archive, sound):
    """Read original PRJ/SFLX metadata, without reusing the runtime selector."""
    data = archive.read_bytes()
    assert data[:4] == b'PROJ'
    count = struct.unpack_from('<H', data, 24)[0]
    for i in range(count):
        entry = 26 + 24*i
        if data[entry:entry+4].rstrip(b'\0') == b'SNDS':
            index = struct.unpack_from('<I', data, entry+4)[0]
            start, length = struct.unpack_from('<II', data, index+22+8*sound)
            payload = data[start+62:start+length]
            name = data[start+30:start+46].split(b'\0')[0].decode('ascii')
            assert payload[:4] == b'SFLX'
            blocks, samples = struct.unpack_from('<IH', payload, 8)
            return dict(id=sound, name=name, samples=blocks*samples, packed_sha256=hashlib.sha256(payload).hexdigest())
    raise AssertionError('missing SNDS')


def checked(command, env):
    p = subprocess.run([str(a) for a in command], env=env, capture_output=True, text=True, timeout=120)
    assert p.returncode == 0, p.stdout + p.stderr
    return p.stdout + p.stderr


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', required=True, type=Path)
    ap.add_argument('--binary', required=True, type=Path)
    ap.add_argument('--prepare', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--expect-old-bug', action='store_true')
    ap.add_argument('--baseline', type=Path)
    args = ap.parse_args()
    game, binary, prepare, output = [p.resolve() for p in (args.game,args.binary,args.prepare,args.output)]
    assert game != output and game not in output.parents
    output.mkdir(parents=True, exist_ok=False)
    archives = {ed:game/path for ed,path in (
        ('dos','cd/MERCS/MW2.PRJ'),('3dfx','3d/models.prj'),('direct3d','titanium/MW2.PRJ'))}
    report = dict(scope=__doc__, platform='Linux x86_64 / SDL3 offscreen / dummy audio',
        binary_sha256=sha(binary), prepare_sha256=sha(prepare),
        archive_sha256={ed:sha(path) for ed,path in archives.items()},
        expected_old_bug=args.expect_old_bug, cases=[])
    for edition,archive in archives.items():
        bad, servo = snd_record(archive,316), snd_record(archive,341)
        assert bad['name']=='STAR3A1' and servo['name']=='TORSLOOP'
        assert bad['samples'] != servo['samples']
        for tag,hold,keys,cockpit in CASES:
            with tempfile.TemporaryDirectory(prefix='mercs-turn-audio-') as tmp:
                base=Path(tmp); folder=fixture(base,game); env=environment(base)
                overlay=folder/'install'/('retail' if edition=='dos' else edition); overlay.mkdir()
                checked([prepare,archive,overlay,edition,'prepare'],env)
                snd,dif=records(True)
                (overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
                (overlay/'INPUT.MAP').write_text(INPUT)
                (overlay/'GAMEKEY.MAP').write_bytes((game/'cd/MERCS/GAMEKEY.MAP').read_bytes())
                (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
                env.update(MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',MERCS_SIZE='640x480',
                    MERCS_STATE_DUMP='/0.25',MERCS_SINK_TRACE='1',MERCS_RADIO_TRACE='1',
                    MERCS_SFX_LOG='1',MERCS_SFX_SIMCLOCK='1',MERCS_HOLD=hold,MERCS_KEYS=keys,
                    MERCS_AUTOPILOT=f'6,-9,-999,0,{base}/capture.ppm,{cockpit},0')
                log=checked([binary,folder,'--mission','TRA1SCN1','--edition',edition],env)
                (output/f'{edition}-{tag}.log').write_text(log)
                states=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('STATE ')]
                assert states and float(states[-1]['t']) >= (4.0 if tag=='menu-arrows' else 6.0)
                pcm=Counter(re.findall(r'SFXLOG [\d.]+ pcm (\d+)',log))
                sounds=Counter(re.findall(r'SFXLOG [\d.]+ snd (\d+)',log))
                voices=re.findall(r'SFXLOG [\d.]+ snd (\d+) vol [\d.]+ voice',log)
                torso_starts=re.findall(r'TORSOLOG ([\d.]+) start snd (\d+)',log)
                torso_stops=re.findall(r'TORSOLOG ([\d.]+) stop snd (\d+)',log)
                if tag=='leg-turns':
                    # Verify both controls actually turned the mech.
                    headings=[float(s['heading']) for s in states]
                    assert max(headings)-min(headings)>30, 'arrows never reached movement'
                if tag in ('torso-turns','external-torso'):
                    twists=[float(s['twist']) for s in states]
                    assert max(twists)-min(twists)>15, 'torso controls never reached servo'
                if tag=='command':
                    assert voices == ['607'], 'order without an eligible ally produced a reply'
                if args.expect_old_bug:
                    if tag in ('leg-turns','torso-turns'):
                        assert pcm[str(bad['samples'])] >= 2, 'did not reproduce old speech loop'
                else:
                    assert not pcm[str(bad['samples'])], 'STAR3A1 is still played as raw effects PCM'
                    assert not sounds['316'], 'turn/menu input triggered STAR3A1 speech'
                    if tag=='torso-turns':
                        assert len(torso_starts)==len(torso_stops)==2
                        assert all(int(s[1])==341 for s in torso_starts)
                        assert sounds['341']>=2 and not pcm[str(servo['samples'])], 'TORSLOOP did not use original capped one-shot path'
                        assert float(torso_stops[-1][0])<5.5, 'servo loop did not stop on settling'
                    if tag in ('leg-turns','external-torso','menu-arrows'):
                        assert not torso_starts and not pcm[str(servo['samples'])] and not sounds['341']
                    if tag=='menu-arrows':
                        assert sounds['245']>=1 and sounds['237']>=1, 'correct menu cues missing'
                        assert not sounds['219'] and not sounds['225'], 'inherited menu cues remain'
                report['cases'].append(dict(edition=edition,name=tag,wrong_record=bad,servo_record=servo,
                    pcm_counts=dict(pcm),sound_counts=dict(sounds),voice_ids=voices,
                    torso_starts=torso_starts,torso_stops=torso_stops,
                    state=states[-1],state_samples=len(states),
                    log_sha256=sha(output/f'{edition}-{tag}.log')))
                print(edition,tag,'PCM',dict(pcm),'voices',voices,flush=True)
    assert report['binary_sha256']==sha(binary) and report['prepare_sha256']==sha(prepare)
    assert report['archive_sha256']=={ed:sha(p) for ed,p in archives.items()}
    if args.baseline:
        before=json.loads(args.baseline.read_text())
        assert len(before['cases'])==len(report['cases'])
        for old,new in zip(before['cases'],report['cases']):
            assert (old['edition'],old['name'])==(new['edition'],new['name'])
            assert old['state']==new['state'], (new['edition'],new['name'],'state changed')
            assert old['voice_ids']==new['voice_ids'], 'genuine voice dispatch changed'
        report['baseline_sha256']=sha(args.baseline)
        report['movement_state_and_genuine_voices_unchanged']=True
    report['protected_inputs_unchanged']=report['passed']=True
    (output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: 15 native input/audio captures complete; original archives unchanged.')


if __name__=='__main__':
    main()
