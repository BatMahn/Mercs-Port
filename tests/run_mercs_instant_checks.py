#!/usr/bin/env python3
"""Real three-edition Instant Action child with a separate active career.

The native validator, generated USERSTAR/custom MEK graph and real renderer run
before original Abort Mission input. Existing career, owned designs, USERSTAR
and result artifacts must remain byte-identical; no IA result is published.
"""
import argparse,hashlib,json,re,tempfile
from pathlib import Path
from run_combat_variables_checks import ROOT,fixture,environment,checked,records

def snapshot(folder):
    return {p.relative_to(folder).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
            for p in folder.rglob('*') if p.is_file() and 'MERCS-INSTANT' not in p.parts}

def run(game,edition,output):
    with tempfile.TemporaryDirectory(prefix=f'mercs-instant-{edition}-') as tmp:
        base=Path(tmp);folder=fixture(base,game);env=environment(base)
        retail=folder/'install/retail';retail.mkdir();overlay=retail if edition=='dos' else folder/'install'/edition
        if edition!='dos':overlay.mkdir()
        store=retail/'MERCS-CAREERS';store.mkdir()
        archive={'dos':game/'cd/MERCS/MW2.PRJ','3dfx':game/'3d/models.prj','direct3d':game/'titanium/MW2.PRJ'}[edition]
        # Existing active/deployed career and owned launch records are strong
        # preservation inputs. The IA child must not bind to their token.
        career=checked([ROOT/'test_mercs_rmg',archive,game/'cd',overlay,'--prepare-launch',edition,store],env)
        match=re.search(r'RMG_DEPLOYED scene=(\w+) edition=\w+ token=(\w+)',career);assert match,career
        return_dir=overlay/'MERCS-RETURNS'/match[2];(return_dir/'RESULT.DAT').write_bytes(b'EXISTING CAREER RESULT PRESERVATION FIXTURE\0')
        snd,dif=records(True);(overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
        prepared=checked([ROOT/'test_mercs_instant',archive,game/'cd',overlay,'--prepare-launch',edition],env)
        launch=re.search(r'INSTANT_DEPLOYED scene=(\w+) edition=\w+ token=(\w+)',prepared);assert launch,prepared
        scene,token=launch.groups();before=snapshot(folder/'install')
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
        picture=base/'instant.ppm'
        env.update(MERCS_SIZE='800x600',MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',MERCS_STATE_DUMP='/0.5',
                   MERCS_AUTOPILOT=f'4,0,0,0,{picture},1,0',MERCS_LIST_ACTORS='1',MERCS_DBG_START='1',
                   MERCS_KEYS='1.5:esc,1.7:1,1.9:enter',MERCS_RETURN_TOKEN=match[2])
        log=checked([ROOT/'mercs',folder,'--mission',scene,'--edition',edition,'--instant-token',token],env)
        states=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('STATE ')]
        assert states and max(float(s['t']) for s in states)>=1.5,(edition,'mission did not run')
        assert any(int(s['enemies'])>=3 for s in states),(edition,'selected enemies missing')
        assert all(s['ending']=='0' for s in states if float(s['t'])<1.5),(edition,'ended before abort')
        assert 'Player: ANIHILAT / ANI00USR' in log,log[-3000:]
        assert snapshot(folder/'install')==before,(edition,'career, owned or result files changed')
        assert not (overlay/'MERCS-INSTANT'/token/'RESULT.DAT').exists()
        assert 'committed mission return' not in log
        if output:
            (output/f'{edition}-instant.log').write_text(log);(output/f'{edition}-prepare.log').write_text(prepared)
            if picture.exists():(output/f'{edition}.ppm').write_bytes(picture.read_bytes())
        return dict(edition=edition,scene=scene,state_samples=len(states),preserved_files=len(before),custom_player='ANI00USR',career_unchanged=True)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
    game=a.game.resolve();out=a.output.resolve() if a.output else None
    if out:out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mercs-instant-ui-') as tmp:
        base=Path(tmp);folder=fixture(base,game);env=environment(base)
        cmd=[ROOT/'test_mercs_instant_ui',folder]
        if out:cmd.append(out)
        ui=checked(cmd,env)
        if out:(out/'ui.log').write_text(ui)
    results=[run(game,edition,out) for edition in ('dos','3dfx','direct3d')]
    report=json.dumps(dict(scope=__doc__,passed=True,missions=results),indent=2)+'\n'
    if out:(out/'results.json').write_text(report)
    print(report,end='')
if __name__=='__main__':main()
