#!/usr/bin/env python3
"""Exercise every MFD mode plus original keyboard commands in real missions.

Original assets are read from the supplied game. Each worker uses a private
overlay. This checks native renderer/dispatcher/persistence integration, not
pixel equality against original hardware or a macOS gameplay run.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile
from run_combat_variables_checks import ROOT, fixture, environment, checked, records

DEFAULT = [1,8,9,10,0,12,13]


def states(log):
    return [dict(re.findall(r'(\w+)=([^\s]+)', line))
            for line in log.splitlines() if line.startswith('STATE ')]


def exercise(game, edition, output):
    with tempfile.TemporaryDirectory(prefix=f'mercs-controls-{edition}-') as tmp:
        base=Path(tmp); folder=fixture(base,game); env=environment(base)
        overlay=folder/'install'/('retail' if edition=='dos' else edition); overlay.mkdir()
        archive={'dos':game/'cd/MERCS/MW2.PRJ','3dfx':game/'3d/models.prj',
                 'direct3d':game/'titanium/MW2.PRJ'}[edition]
        checked([ROOT/'test_mercs_roundtrip',archive,overlay,edition,'prepare'],env)
        snd,dif=records(True)
        dif[9]=1  # Original satellite-MFD and map-zoom permission.
        (overlay/'MW2SND.CFG').write_bytes(snd); (overlay/'MW2DIF.CFG').write_bytes(dif)
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
        hud=overlay/'MERCS.HUD'

        def run(tag, duration=1.1, **extra):
            picture=base/f'{tag}.ppm'; settings=env.copy()
            settings.update(MERCS_SIZE='800x600',MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',
                MERCS_TEST_SKY_STILL='1',
                MERCS_STATE_DUMP='/0.1',MERCS_AUTOPILOT=f'{duration},0,0,0,{picture},1,0')
            settings.update(extra)
            log=checked([ROOT/'mercs',folder,'--mission','TRA1SCN1','--edition',edition],settings)
            if output:
                (output/f'{edition}-{tag}.log').write_text(log)
                (output/f'{edition}-{tag}.ppm').write_bytes(picture.read_bytes())
            parsed=states(log)
            assert parsed and float(parsed[-1]['t'])>=duration,(edition,tag,'short run')
            assert not re.search(r'\b(?:nan|inf)\b',log,re.I),(edition,tag,'non-finite state')
            return parsed,hashlib.sha256(picture.read_bytes()).hexdigest()

        modes=[]
        for mode in range(14):
            hud.write_bytes(struct.pack('<9iB',*([mode]*7),1,1,1))
            parsed,digest=run(f'mfd-{mode:02}',MERCS_TEST_TARGET='0',MERCS_TEST_HEAT='0.1:40')
            assert list(map(int,parsed[-1]['mfd'].split(';')))==[mode]*7
            modes.append(dict(mode=mode,windows=7,sha256=digest))
        assert len({m['sha256'] for m in modes})>=11,(edition,'most modes drew identical frames')

        hud.write_bytes(struct.pack('<9iB',*([6]*7),1,1,1))
        dif[9]=0;(overlay/'MW2DIF.CFG').write_bytes(dif)
        _,disabled=run('satellite-disabled',MERCS_TEST_TARGET='0',MERCS_TEST_HEAT='0.1:40')
        assert disabled==modes[0]['sha256'],(edition,'disabled satellite MFD drew content')
        assert disabled!=modes[6]['sha256'],(edition,'enabled satellite map was blank')
        hud.write_bytes(struct.pack('<9iB',*DEFAULT,1,1,1))
        gated,_=run('satellite-zoom-disabled',MERCS_KEYS='0.1:f3,0.3:x,0.5:x')
        assert gated[-1]['satmap']=='1' and gated[-1]['sat_i']=='0'
        dif[9]=1;(overlay/'MW2DIF.CFG').write_bytes(dif)
        zoomed,_=run('satellite-zoom',1.3,MERCS_KEYS='0.1:f3,0.3:x,0.5:x,0.7:x,0.9:x')
        sequence=[]
        for sample in zoomed:
            if sample['satmap']=='1' and (not sequence or sequence[-1]!=sample['sat_i']):sequence.append(sample['sat_i'])
        assert sequence==['0','1','2','3','0'],(edition,sequence)
        restored,_=run('satellite-return',MERCS_KEYS='0.1:f3,0.5:f3')
        assert restored[-1]['satmap']=='0' and restored[-1]['cockpit']=='1'

        hud.write_bytes(struct.pack('<9iB',*DEFAULT,1,1,1))
        keys=['f1','shift+f1','shift+f2','shift+f3','shift+f4','shift+f5',
              'shift+f6','shift+f9','shift+f10','u','f2','alt+f11']
        parsed,digest=run('keyboard-save',2,
            MERCS_KEYS=','.join(f'{0.1+i*0.1:.2f}:{key}' for i,key in enumerate(keys)))
        wanted=[2,9,10,11,1,13,0]
        final=parsed[-1]
        assert list(map(int,final['mfd'].split(';')))==wanted,(edition,final['mfd'])
        assert final['compass']==final['weapons_on']==final['cockpit']=='0'
        saved=hud.read_bytes(); width=37 if edition=='dos' else 40
        assert len(saved)==width,(edition,len(saved))
        assert struct.unpack('<10i' if width==40 else '<9iB',saved)==tuple(wanted+[0,0,2])
        parsed,_=run('keyboard-reload')
        assert list(map(int,parsed[-1]['mfd'].split(';')))==wanted
        assert parsed[-1]['weapons_on']==parsed[-1]['compass']=='0'
        assert parsed[-1]['radar_mode']=='2'
        parsed,_=run('keyboard-reset',MERCS_KEYS='0.1:shift+f12')
        assert list(map(int,parsed[-1]['mfd'].split(';')))==DEFAULT
        assert parsed[-1]['weapons_on']==parsed[-1]['compass']==parsed[-1]['radar_mode']=='1'
        hud.write_bytes(struct.pack('<9iB',*DEFAULT,1,1,1))

        parsed,_=run('weapon-groups',1.3,
            MERCS_KEYS='0.1:shift+4,0.3:p,0.6:shift+5,0.9:shift+p')
        before=next(p for p in parsed if float(p['t'])<0.3)
        after_fire=next(p for p in parsed if 0.4<=float(p['t'])<0.6)
        assert before['sel']=='0' and before['fired']=='0',(edition,'wrong original initial mount')
        assert after_fire['wgroups'].split(';')[0]=='3'
        assert after_fire['sel']=='1' and int(after_fire['fired'])>=1,(edition,'fire/cycle order',after_fire)
        assert parsed[-1]['wgroups'].split(';')[:2]==['3','4']
        assert int(parsed[-1]['fired'])>=2,(edition,'alpha command did not fire other ready mounts')
        return dict(edition=edition,modes=modes,keyboard_commands=len(keys),
                    saved_bytes=len(saved),reload=True,reset=True,groups=True,
                    satellite_permission=True,satellite_zoom=sequence,satellite_return=True)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,required=True); ap.add_argument('--output',type=Path)
    args=ap.parse_args(); game=args.game.resolve(); out=args.output.resolve() if args.output else None
    if out: out.mkdir(parents=True,exist_ok=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        results=list(pool.map(lambda ed:exercise(game,ed,out),('dos','3dfx','direct3d')))
    report=dict(scope=__doc__,platform='Linux x86_64 / SDL offscreen / Mesa software OpenGL',
                window_modes=294,editions=results,passed=True)
    text=json.dumps(report,indent=2)+'\n'
    if out:(out/'results.json').write_text(text)
    print(text,end='')


if __name__=='__main__': main()
