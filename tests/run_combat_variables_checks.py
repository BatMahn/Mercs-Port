#!/usr/bin/env python3
"""Exercise the restored menu and settings in real missions using private files.

Requires make mercs test_mercs_combat_variables test_mercs_roundtrip. Original
assets stay external. Runs on Linux SDL offscreen; these are controlled native
mission samples, not original-game or Mac playthroughs. Particle render parity
is described separately in tests/fixtures/mercs-combat-variables-effects.md.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIELDS = (0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x48,0x4c)


def records(enabled):
    snd = bytearray(84)
    for off in (0,4,8,12):
        struct.pack_into('<I',snd,off,65536 if enabled else 0)
    struct.pack_into('<I',snd,0x10,15)
    struct.pack_into('<I',snd,0x34,8)
    for off in FIELDS:
        struct.pack_into('<I',snd,off,int(enabled))
    snd[0x38:0x43] = b'vesa480.dll'
    dif = bytearray(23)
    dif[:6] = bytes((0,0,1,1,int(enabled),0 if enabled else 2))
    return snd,dif


def fixture(base, game):
    folder=base/'Mercs-game';folder.mkdir()
    for name in ('cd','3d','titanium','music'):
        if (game/name).exists(): (folder/name).symlink_to(game/name,target_is_directory=True)
    (folder/'install').mkdir()
    return folder


def environment(base):
    env={k:v for k,v in os.environ.items() if not k.startswith(('MERCS_','MW2_'))}
    env.update(SDL_VIDEO_DRIVER='offscreen',SDL_AUDIO_DRIVER='dummy',
               LIBGL_ALWAYS_SOFTWARE='1',MESA_SHADER_CACHE_DISABLE='true',
               MERCS_CONFIG=str(base/'port.cfg'))
    return env


def checked(cmd,env,timeout=120):
    p=subprocess.run([str(x) for x in cmd],cwd=ROOT,env=env,text=True,capture_output=True,timeout=timeout)
    output=p.stdout+p.stderr
    if p.returncode: raise RuntimeError(f'{cmd[0]} exited {p.returncode}\n{output}')
    return output


def mission(game, edition, enabled, output):
    tag=f'{edition}-{"on" if enabled else "off"}'
    with tempfile.TemporaryDirectory(prefix=f'mercs-options-{tag}-') as tmp:
        base=Path(tmp);folder=fixture(base,game);env=environment(base)
        overlay=folder/'install'/('retail' if edition=='dos' else edition);overlay.mkdir()
        archive={'dos':game/'cd/MERCS/MW2.PRJ','3dfx':game/'3d/models.prj','direct3d':game/'titanium/MW2.PRJ'}[edition]
        checked([ROOT/'test_mercs_roundtrip',archive,overlay,edition,'prepare'],env)
        snd,dif=records(enabled)
        (overlay/'MW2SND.CFG').write_bytes(snd);(overlay/'MW2DIF.CFG').write_bytes(dif)
        (base/'port.cfg').write_text(f'game={folder}\nrenderer={edition}\nresolution=window\nrender=640x480\naa=0\n')
        picture=base/'mission.ppm'
        env.update(MERCS_SIZE='800x600',MERCS_NO_STARTUP='1',MERCS_TEST_FPS='30',
                   MERCS_STATE_DUMP='/0.5',MERCS_OPTIONS_TRACE='1',MERCS_SFX_LOG='1',MERCS_DEBUG_LOADOUT='1',
                   MERCS_AUTOPILOT=f'6,0,0,1,{picture},1,0')
        log=checked([ROOT/'mercs',folder,'--mission','TRA1SCN1','--edition',edition],env)
        if output:
            (output/f'{tag}.log').write_text(log)
            if picture.is_file(): (output/f'{tag}.ppm').write_bytes(picture.read_bytes())
        traces=[line for line in log.splitlines() if line.startswith('COMBATOPTIONS')]
        assert len(traces)==1,(tag,traces)
        values=dict(re.findall(r'(\w+)=([^\s]+)',traces[0]))
        for key in ('object_textures','terrain_textures','density','chunks','particles','multiple_lights','shadows','heat'):
            assert values[key]==str(int(enabled)),(tag,key,values[key])
        assert values['detail']==('1' if enabled else '2')
        assert values['difficulty']==('0' if enabled else '2')
        assert values['render']=='640x480'
        for key in ('music','effects','voice'):assert values[key]==('65536' if enabled else '0')
        states=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('STATE')]
        assert states and float(states[-1]['t'])>=6,(tag,'short mission')
        heat=max(float(s['heat']) for s in states)
        if enabled:assert heat>0,(tag,'no heat produced while firing')
        else:assert heat==0,(tag,'heat tracking off failed',heat)
        sound=re.findall(r'^SFXLOG .* vol ([0-9.]+)',log,re.M)
        assert sound,(tag,'no sound submissions')
        if not enabled: assert max(map(float,sound))==0,(tag,'nonzero muted submission')
        assert (overlay/'MW2SND.CFG').read_bytes()==snd
        assert (overlay/'MW2DIF.CFG').read_bytes()==dif
        result={'edition':edition,'enabled':enabled,'loaded':values,'state_samples':len(states),
                'maximum_heat':heat,'sound_submissions':len(sound),
                'maximum_sound_volume':max(map(float,sound)),
                'frame_sha256':hashlib.sha256(picture.read_bytes()).hexdigest()}
        return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();game=args.game.resolve()
    out=args.output.resolve() if args.output else None
    if out:out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mercs-combat-menu-') as tmp:
        base=Path(tmp);folder=fixture(base,game);env=environment(base)
        cmd=[ROOT/'test_mercs_combat_variables',folder]
        if out:cmd.append(out)
        menu=checked(cmd,env)
    with ThreadPoolExecutor(max_workers=2) as pool:
        cases=list(pool.map(lambda x:mission(game,*x,out),[(e,on) for e in ('dos','3dfx','direct3d') for on in (True,False)]))
    for edition in ('dos','3dfx','direct3d'):
        frames=[c['frame_sha256'] for c in cases if c['edition']==edition]
        assert len(set(frames))==2,(edition,'all toggles left image unchanged')
    result={'scope':__doc__,'menu':menu,'missions':cases,'passed':True}
    report=json.dumps(result,indent=2)+'\n'
    if out:(out/'results.json').write_text(report)
    print(report,end='')


if __name__=='__main__':main()
