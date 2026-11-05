#!/usr/bin/env python3
"""Launch a bound, seeded procedural career mission through the real child.

Each graphics profile gets a private overlay and a committed DEPLOYED career.
The real native command-line validator and renderer must both reopen and load
the generated graph. The original Abort Mission menu produces an owned return.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tempfile
from run_combat_variables_checks import ROOT, fixture, environment, checked, records


def run(game,edition,output):
    with tempfile.TemporaryDirectory(prefix=f'mercs-rmg-{edition}-') as tmp:
        base=Path(tmp); folder=fixture(base,game); env=environment(base)
        retail=folder/'install/retail'; retail.mkdir()
        overlay=retail if edition=='dos' else folder/'install'/edition
        if edition!='dos':overlay.mkdir()
        store=retail/'MERCS-CAREERS';store.mkdir()
        archive={'dos':game/'cd/MERCS/MW2.PRJ','3dfx':game/'3d/models.prj',
                 'direct3d':game/'titanium/MW2.PRJ'}[edition]
        prepared=checked([ROOT/'test_mercs_rmg',archive,game/'cd',overlay,
                          '--prepare-launch',edition,store],env)
        match=re.search(r'RMG_DEPLOYED scene=(\w+) edition=\w+ token=(\w+)',prepared)
        assert match,prepared
        scene,token=match.groups()
        snd,dif=records(True)
        (overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
        picture=base/'mission.ppm'
        env.update(MERCS_SIZE='800x600',MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',
                   MERCS_STATE_DUMP='/0.5',MERCS_AUTOPILOT=f'4,0,0,0,{picture},1,0',
                   MERCS_LIST_ACTORS='1',MERCS_DBG_START='1',
                   MERCS_KEYS='1.5:esc,1.7:1,1.9:enter')
        log=checked([ROOT/'mercs',folder,'--mission',scene,'--edition',edition,
                     '--return-token',token],env)
        if output:
            (output/f'{edition}-prepare.log').write_text(prepared)
            (output/f'{edition}-mission.log').write_text(log)
            if picture.exists():(output/f'{edition}.ppm').write_bytes(picture.read_bytes())
        states=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('STATE ')]
        assert states and max(float(s['t']) for s in states)>=1.5,(edition,'no mission run')
        assert any(int(s['enemies'])>0 for s in states),(edition,'generated units absent')
        assert all(s['ending']=='0' for s in states if float(s['t'])<1.5),(edition,'mission ended before the abort input')
        assert re.search(r'^actor \d+ ',log,re.M),(edition,'no loaded unit models')
        assert re.search(r'^START mis5stp0 ',log,re.M|re.I),(edition,'accepted start NAVP missing')
        packets=list((overlay/'MERCS-RETURNS'/token).glob('*'))
        complete=[p for p in packets if p.name=='RESULT.DAT']
        assert complete,(edition,[p.name for p in packets],log[-1000:])
        assert len(complete[0].read_bytes())>3000
        return dict(edition=edition,scene=scene,state_samples=len(states),
                    result_bytes=complete[0].stat().st_size,
                    result_sha256=hashlib.sha256(complete[0].read_bytes()).hexdigest())


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,required=True);ap.add_argument('--output',type=Path)
    args=ap.parse_args();out=args.output.resolve() if args.output else None
    if out:out.mkdir(parents=True,exist_ok=True)
    results=[run(args.game.resolve(),ed,out) for ed in ('dos','3dfx','direct3d')]
    report=json.dumps(dict(scope=__doc__,missions=results,passed=True),indent=2)+'\n'
    if out:(out/'results.json').write_text(report)
    print(report,end='')


if __name__=='__main__':main()
