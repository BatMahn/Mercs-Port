#!/usr/bin/env python3
"""Check original Mercs expert/novice controls in real isolated missions.

The exact arithmetic is separately checked against original x86 fixtures by
test_mercs_hud_controls. These runs verify that saved input maps reach the real
frontend, including its torso servo and movement consumer.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import tempfile
from run_combat_variables_checks import ROOT, fixture, environment, checked, records
from run_mercs_control_checks import states

EXPERT = ('torso_tilt_minus {\n+ keyboard GreyUpArrow\n}\n'
          'torso_tilt_plus {\n+ keyboard GreyDownArrow\n}\n'
          'throttle_plus {\n+ keyboard Equal\n}\n')
NOVICE = ('novice_forward {\n+ keyboard GreyUpArrow\n}\n'
          'novice_backward {\n+ keyboard GreyDownArrow\n}\n'
          'torso_tilt_plus {\n+ keyboard D\n}\n')


def exercise(game, edition, output):
    with tempfile.TemporaryDirectory(prefix=f'mercs-steering-{edition}-') as tmp:
        base=Path(tmp); folder=fixture(base,game); env=environment(base)
        overlay=folder/'install'/('retail' if edition=='dos' else edition); overlay.mkdir()
        archive={'dos':game/'cd/MERCS/MW2.PRJ','3dfx':game/'3d/models.prj',
                 'direct3d':game/'titanium/MW2.PRJ'}[edition]
        checked([ROOT/'test_mercs_roundtrip',archive,overlay,edition,'prepare'],env)
        snd,dif=records(True)
        (overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')

        def run(tag,mapping,hold,duration,**extra):
            (overlay/'INPUT.MAP').write_text(mapping)
            picture=base/f'{tag}.ppm'; settings=env.copy()
            settings.update(MERCS_SIZE='800x600',MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',
                MERCS_STATE_DUMP='/0.2',MERCS_SINK_TRACE='1',MERCS_HOLD=hold,
                MERCS_AUTOPILOT=f'{duration},-9,-999,0,{picture},1,0')
            settings.update(extra)
            log=checked([ROOT/'mercs',folder,'--mission','TRA1SCN1','--edition',edition],settings)
            if output:
                (output/f'{edition}-{tag}.log').write_text(log)
                (output/f'{edition}-{tag}.ppm').write_bytes(picture.read_bytes())
            sample=states(log)
            sinks=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('SINK ')]
            assert sample and sinks and float(sample[-1]['t'])>=duration,(edition,tag,'short run')
            assert not re.search(r'\b(?:nan|inf)\b',log,re.I),(edition,tag,'non-finite state')
            return sample,sinks

        up,us=run('expert-up',EXPERT,'0-4:up',3.2)
        down,ds=run('expert-down',EXPERT,'0-4:down',3.2)
        assert up[-1]['novice']==down[-1]['novice']=='0'
        assert abs(float(up[-1]['pitch'])-60)<0.15
        assert abs(float(down[-1]['pitch'])+60)<0.15
        assert max(float(s['dpi']) for s in us)==60 and min(float(s['dpi']) for s in ds)==-60
        expert,es=run('expert-forward',EXPERT,'0-2:equal',1.1)
        forward,fs=run('novice-forward',NOVICE,'0-2:up',1.1)
        backward,bs=run('novice-backward',NOVICE,'0-2:down',1.1)
        assert forward[-1]['novice']==backward[-1]['novice']=='1'
        assert float(forward[-1]['drive'])==1 and forward[-1]['reverse']=='0'
        assert float(backward[-1]['drive'])==-1 and backward[-1]['reverse']=='1'
        assert float(forward[-1]['kmh'])>0 and float(backward[-1]['kmh'])<0
        normal=float(es[-1]['top_kmh']); boosted=float(fs[-1]['top_kmh'])
        assert abs(boosted-normal*1.5)<0.01,(edition,normal,boosted)
        aimed,aim_sinks=run('novice-target',NOVICE,'0-3:d',2,
            MERCS_TEST_TARGET='0',MERCS_TEST_DROP='0.1:0:5000:20000:0:0:0',
            MERCS_TEST_IMMOBILE='0.05:0')
        assert aimed[-1]['tgt']=='0' and float(aimed[-1]['pitch'])>59.8
        assert all(abs(float(s['dpi'])-60)<0.01 for s in aim_sinks if float(s['t'])>0.5)
        return dict(edition=edition,expert_tilt=[float(down[-1]['pitch']),float(up[-1]['pitch'])],
                    forward_kmh=float(forward[-1]['kmh']),reverse_kmh=float(backward[-1]['kmh']),
                    maximum_speed_kmh=normal,novice_maximum_speed_kmh=boosted,
                    target_tilt=float(aimed[-1]['pitch']),passed=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args();out=args.output.resolve() if args.output else None
    if out:out.mkdir(parents=True,exist_ok=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        result=list(pool.map(lambda ed:exercise(args.game.resolve(),ed,out),('dos','3dfx','direct3d')))
    report=dict(scope=__doc__,missions=18,platform='Linux x86_64 / SDL offscreen / Mesa software OpenGL',
                results=result,passed=True)
    text=json.dumps(report,indent=2)+'\n'
    if out:(out/'results.json').write_text(text)
    print(text,end='')


if __name__=='__main__':main()
