#!/usr/bin/env python3
"""Capture actual Mercs inputs through SDL3, event producers and the mixer.

All three supplied archives are read through private per-run install folders.
SFXLOG records submissions; SFXMIX records accepted native mixer handles.
This checks routing and state changes, not audible hardware fidelity.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import tempfile

from run_combat_variables_checks import fixture, environment, records
from run_turn_audio_checks import checked, sha, snd_record


INPUT = ('zoom_factor_plus {\n+ keyboard Z\n}\n'
         'weapon_cycle {\n+ keyboard W\n}\n')
CASES = (
    ('modes', 8, '5-6:z', '0.5:l,1:l,1.5:l,2:l,2.5:f3,3:f3,4:f11,4.5:f11,6.5:ctrl+z'),
    ('ammo-power', 9, '0.7-0.8:w,1.7-1.8:w,2.7-2.8:w',
     '1:k,2:k,3:s,3.2:k,3.5:s,6:k,6.5:k'),
    ('eject', 4, '', '1:ctrl+alt+e'),
)
NAMES = {149:'IREDMODE',167:'JETTISON',214:'MECBEJXX',240:'MECHUDON',
         253:'MECOMBEP',263:'MECSHTD1',268:'MECTURX2',347:'VIEWZOOM'}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for key in ('game','binary','prepare','output'): ap.add_argument('--'+key,required=True,type=Path)
    a=ap.parse_args()
    game,binary,prepare,out=[getattr(a,x).resolve() for x in ('game','binary','prepare','output')]
    assert game!=out and game not in out.parents
    out.mkdir(parents=True,exist_ok=False)
    archives={e:game/p for e,p in (('dos','cd/MERCS/MW2.PRJ'),('3dfx','3d/models.prj'),('direct3d','titanium/MW2.PRJ'))}
    report=dict(scope=__doc__,platform='Linux x86_64 / SDL3 offscreen / dummy audio',
                binary_sha256=sha(binary),prepare_sha256=sha(prepare),
                archive_sha256={e:sha(p) for e,p in archives.items()},cases=[])
    for ed,archive in archives.items():
        metadata={i:snd_record(archive,i) for i in NAMES}
        assert all(metadata[i]['name']==n for i,n in NAMES.items())
        for tag,seconds,hold,keys in CASES:
            with tempfile.TemporaryDirectory(prefix='mercs-gameplay-audio-') as tmp:
                base=Path(tmp);folder=fixture(base,game);env=environment(base)
                overlay=folder/'install'/('retail' if ed=='dos' else ed);overlay.mkdir()
                checked([prepare,archive,overlay,ed,'prepare'],env)
                snd,dif=records(True)
                (overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
                (overlay/'INPUT.MAP').write_text(INPUT)
                (overlay/'GAMEKEY.MAP').write_bytes((game/'cd/MERCS/GAMEKEY.MAP').read_bytes())
                (base/'port.cfg').write_text(f'game={folder}\nrenderer={ed}\nresolution=window\nrender=640x480\naa=0\n')
                env.update(MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',MERCS_SIZE='640x480',
                           MERCS_STATE_DUMP='/0.25',MERCS_SFX_LOG='1',MERCS_SFX_SIMCLOCK='1',
                           MERCS_TEST_AUDIO_REALTIME='1',
                           MERCS_RAMP_TRACE='1',MERCS_HOLD=hold,MERCS_KEYS=keys,
                           MERCS_AUTOPILOT=f'{seconds},-9,-999,0,{base}/capture.ppm,1,0')
                log=checked([binary,folder,'--mission','TRA1SCN1','--edition',ed],env)
                logfile=out/f'{ed}-{tag}.log';logfile.write_text(log)
                submitted=Counter(map(int,re.findall(r'SFXLOG [\d.]+ snd (\d+)',log)))
                accepted=Counter(map(int,re.findall(r'SFXMIX [\d.]+ snd (\d+)',log)))
                states=[dict(re.findall(r'(\w+)=([^\s]+)',l)) for l in log.splitlines() if l.startswith('STATE ')]
                assert states and float(states[-1]['t'])>=seconds-0.5,(ed,tag,'short mission')
                assert not submitted[316],(ed,tag,'unrelated STAR3A1 speech')
                if tag=='modes':
                    assert submitted[149]==2 and submitted[253]==2 and submitted[240]==2,(ed,tag,submitted)
                    zoom=[float(z) for z in re.findall(r'RAMP .* zoom=([\d.]+)',log)]
                    assert zoom and max(zoom)>1.1 and zoom[-1]==1.0,(ed,tag,'zoom input failed')
                    assert submitted[347]>=2,(ed,tag,'zoom change sound missing')
                    expected=(149,253,240,347)
                elif tag=='ammo-power':
                    assert submitted[167]==2,(ed,tag,'jettison must make the two original submissions',submitted)
                    assert submitted[263]==1 and submitted[268]==6,(ed,tag,'shutdown/startup missing',submitted)
                    assert max(int(s['sel']) for s in states)>=3,(ed,tag,'weapon cycling failed')
                    assert any(s['shut']=='1' and ':180:' in s['weapons'] for s in states if float(s['t'])>3.2),(ed,tag,'shutdown consumed ammunition')
                    assert all(float(t)>=6 for t in re.findall(r'SFXLOG ([\d.]+) snd 167 ',log)),(ed,tag,'shutdown jettison emitted a cue')
                    expected=(167,263,268)
                else:
                    assert submitted[214]==1 and any(s.get('dead')=='1' and s.get('ejected')=='1' for s in states),(ed,tag,'eject input failed',submitted)
                    expected=(214,)
                assert all(accepted[i]>=1 for i in expected),(ed,tag,'submission failed to reach mixer',accepted)
                report['cases'].append(dict(edition=ed,name=tag,submitted=dict(submitted),accepted=dict(accepted),
                                           records={i:metadata[i] for i in expected},state_samples=len(states),
                                           final_state=states[-1],log_sha256=sha(logfile)))
                print(ed,tag,'submitted',dict(submitted),'accepted',dict(accepted),flush=True)
    assert report['binary_sha256']==sha(binary) and report['prepare_sha256']==sha(prepare)
    assert report['archive_sha256']=={e:sha(p) for e,p in archives.items()}
    report['passed']=report['protected_inputs_unchanged']=True
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: nine native gameplay input/mixer captures; original archives unchanged.')


if __name__=='__main__': main()
