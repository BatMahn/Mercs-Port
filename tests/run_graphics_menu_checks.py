#!/usr/bin/env python3
"""Verify Mercs renderer/texture options through real mission menus and rendering.

Original data is read through private game-folder overlays. The checks use SDL3
offscreen video, Mesa software GL and dummy audio, not a physical Mac display.
The renderer choice is deliberately queued for the next mission: a running
simulation retains its original archive and record IDs. Filtering is applied
to the active renderer on Accept; Escape leaves the saved choices unchanged.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from run_combat_variables_checks import ROOT, fixture, records

EDITIONS = ('dos', 'direct3d', '3dfx')
ARCHIVES = {'dos': 'cd/MERCS/MW2.PRJ', 'direct3d': 'titanium/MW2.PRJ',
            '3dfx': '3d/models.prj'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def config(path):
    return dict(line.split('=', 1) for line in path.read_text().splitlines()
                if '=' in line and not line.startswith('#'))


def run_checked(command, env):
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                            text=True, capture_output=True, timeout=120)
    text = result.stdout + result.stderr
    if result.returncode:
        raise AssertionError(f'{command[0]} exited {result.returncode}\n{text[-6000:]}')
    assert not re.search(r'\b(?:nan|inf)\b', text, re.I), text[-3000:]
    return text


def exercise(game, edition, binary, prepare, output, available=True):
    with tempfile.TemporaryDirectory(prefix=f'mercs-graphics-{edition}-') as tmp:
        base = Path(tmp)
        folder = fixture(base, game)
        if not available:
            (folder / '3d').unlink()
            (folder / 'titanium').unlink()
        overlay = folder / 'install' / ('retail' if edition == 'dos' else edition)
        overlay.mkdir()
        cfg = base / 'port.cfg'
        env = {k: v for k, v in os.environ.items() if not k.startswith(('MERCS_', 'MW2_'))}
        env.update(SDL_VIDEO_DRIVER='offscreen', SDL_AUDIO_DRIVER='dummy',
                   LIBGL_ALWAYS_SOFTWARE='1', MERCS_CONFIG=str(cfg))
        run_checked([prepare, game / ARCHIVES[edition], overlay, edition, 'prepare'], env)
        snd, dif = records(True)
        (overlay / 'MW2SND.CFG').write_bytes(snd)
        (overlay / 'MW2DIF.CFG').write_bytes(dif)

        def reset(filter_name='original'):
            cfg.write_text(f'game={folder}\nrenderer={edition}\nresolution=window\n'
                           f'render=native\naa=0\ntexture_filter={filter_name}\n')

        def run(tag, keys='', seconds=1.2, capture_menu=False):
            pic = output / f'{edition}-{tag}.ppm'
            runenv = dict(env, MERCS_SIZE='800x600', MERCS_NO_STARTUP='1',
                          MERCS_TEST_FPS='30', MERCS_TEST_SKY_STILL='1',
                          MERCS_GRAPHICS_TRACE='1', MERCS_SFX_LOG='1',
                          MERCS_STATE_DUMP='/0.2',
                          MERCS_AUTOPILOT=f'{seconds},0,0,0,{pic},1,0')
            if keys:
                runenv['MERCS_KEYS'] = keys
            if capture_menu:
                runenv['MERCS_SHOT'] = f'0.85:{output}/{edition}-menu.ppm'
            text = run_checked([binary, folder, '--mission', 'TRA1SCN1', '--edition', edition], runenv)
            log = output / f'{edition}-{tag}.log'
            log.write_text(text)
            assert pic.is_file() and pic.stat().st_size > 800 * 600 * 3
            states = [dict(re.findall(r'(\w+)=([^\s]+)', line))
                      for line in text.splitlines() if line.startswith('STATE ')]
            assert states and float(states[-1]['t']) >= seconds - 0.01
            assert states[-1]['menu'] == '0', (edition, tag, 'menu did not close')
            return text, sha(pic)

        if not available:
            reset()
            text, digest = run('only-dos',
                '0.1:esc,0.2:4,0.3:6,0.4:3,0.5:0,0.6:esc,0.7:esc')
            saved = config(cfg)
            assert saved['renderer'] == 'dos'
            assert 'MISSIONGRAPHICS active=dos next=dos' in text
            return {'only_available_renderer': 'dos', 'passed': True, 'frame_sha256': digest}

        images = {}
        original_config = None
        for filter_name in ('original', 'nearest', 'linear'):
            reset(filter_name)
            text, digest = run(filter_name)
            actual = 'nearest' if filter_name == 'nearest' or (filter_name == 'original' and edition == 'dos') else 'bilinear'
            assert f'profile={edition} kind=' in text
            assert f'filter={actual} mipmaps=off' in text, (edition, filter_name)
            assert f'texture_filter={filter_name}' in text
            assert config(cfg)['texture_filter'] == filter_name
            images[filter_name] = digest
            if filter_name == 'original':
                original_config = cfg.read_bytes()
        original_filter = 'nearest' if edition == 'dos' else 'linear'
        assert images['original'] == images[original_filter], (edition, 'original sampling changed')
        assert images['nearest'] != images['linear'], (edition, 'texture filter did not change frame')

        reset()
        next_edition = EDITIONS[(EDITIONS.index(edition) + 1) % len(EDITIONS)]
        text, changed = run('menu-accept',
            '0.1:esc,0.2:4,0.3:6,0.4:4,0.5:4,0.6:3,0.7:left,0.8:right,0.9:0,1.0:esc,1.1:esc',
            seconds=1.5, capture_menu=True)
        assert f'MISSIONGRAPHICS active={edition} next={next_edition} texture_filter=2 applied=1' in text
        saved = config(cfg)
        assert saved['renderer'] == next_edition and saved['texture_filter'] == 'linear'
        assert 'textures=own profile_locked=1' in text
        assert f'profile={next_edition} kind=' not in text, 'active archive changed mid-mission'
        assert re.search(r'SFXLOG .* snd 245 ', text), 'proper Mercs menu-open cue missing'
        assert re.search(r'SFXLOG .* snd 237 ', text), 'proper Mercs menu-move cue missing'

        # A second process reads the committed filtering; the explicit edition
        # mirrors the shell's post-return profile synchronization (covered by its
        # separate frontend test). Do not fabricate an in-flight archive swap.
        reloaded, reload_image = run('menu-reload')
        assert 'texture_filter=linear' in reloaded
        assert reload_image == images['linear'], (edition, 'saved filter was not applied')

        # The launcher canonicalizes port.cfg before opening a mission. Start
        # from that canonical original file so byte equality measures Escape,
        # not harmless expansion of the minimal fixture's omitted defaults.
        assert original_config is not None
        cfg.write_bytes(original_config)
        before = cfg.read_bytes()
        text, cancelled = run('menu-cancel',
            '0.1:esc,0.2:4,0.3:6,0.4:3,0.5:4,0.6:esc,0.7:esc,0.8:esc')
        assert 'MISSIONGRAPHICS ' not in text
        assert cfg.read_bytes() == before, (edition, 'Escape saved a staged option')
        assert (overlay / 'MW2SND.CFG').read_bytes() == snd
        assert (overlay / 'MW2DIF.CFG').read_bytes() == dif
        return {'edition': edition, 'startup_frames': images,
                'original_filter': original_filter, 'next_renderer': next_edition,
                'live_filter_and_persistence': True, 'escape_cancels': True,
                'menu_accept_sha256': changed, 'menu_cancel_sha256': cancelled,
                'native_options_preserved': True}


def baseline_original(game, edition, binary, prepare, output):
    """Optional previous-delivery capture under the same fixed inputs."""
    with tempfile.TemporaryDirectory(prefix=f'mercs-original-{edition}-') as tmp:
        base = Path(tmp)
        folder = fixture(base, game)
        overlay = folder / 'install' / ('retail' if edition == 'dos' else edition)
        overlay.mkdir()
        cfg = base / 'port.cfg'
        env = {k: v for k, v in os.environ.items() if not k.startswith(('MERCS_', 'MW2_'))}
        env.update(SDL_VIDEO_DRIVER='offscreen', SDL_AUDIO_DRIVER='dummy',
                   LIBGL_ALWAYS_SOFTWARE='1', MERCS_CONFIG=str(cfg))
        run_checked([prepare, game / ARCHIVES[edition], overlay, edition, 'prepare'], env)
        snd, dif = records(True)
        (overlay / 'MW2SND.CFG').write_bytes(snd)
        (overlay / 'MW2DIF.CFG').write_bytes(dif)
        cfg.write_text(f'game={folder}\nrenderer={edition}\nresolution=window\n'
                       'render=native\naa=0\ntexture_filter=original\n')
        picture = output / f'{edition}-previous-default.ppm'
        env.update(MERCS_SIZE='800x600', MERCS_NO_STARTUP='1', MERCS_TEST_FPS='30',
                   MERCS_TEST_SKY_STILL='1', MERCS_GRAPHICS_TRACE='1',
                   MERCS_SFX_LOG='1', MERCS_STATE_DUMP='/0.2',
                   MERCS_AUTOPILOT=f'1.2,0,0,0,{picture},1,0')
        log = run_checked([binary, folder, '--mission', 'TRA1SCN1', '--edition', edition], env)
        (output / f'{edition}-previous-default.log').write_text(log)
        return sha(picture)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--binary', type=Path, default=ROOT / 'mercs')
    parser.add_argument('--prepare', type=Path, default=ROOT / 'test_mercs_roundtrip')
    parser.add_argument('--baseline-binary', type=Path,
                        help='optional preceding delivery to compare all three original-filter frames')
    args = parser.parse_args()
    game, out, binary, prepare = [p.resolve() for p in (args.game, args.output, args.binary, args.prepare)]
    assert game != out and game not in out.parents
    out.mkdir(parents=True, exist_ok=False)
    original_hashes = {name: sha(game / path) for name, path in ARCHIVES.items()}
    binary_hash = sha(binary)
    results = []
    for edition in EDITIONS:
        results.append(exercise(game, edition, binary, prepare, out))
        print(f'PASS {edition}: default/nearest/smooth, menu Accept/Escape, saved next renderer', flush=True)
    missing = exercise(game, 'dos', binary, prepare, out, available=False)
    assert binary_hash == sha(binary), 'binary changed while testing'
    assert original_hashes == {name: sha(game / path) for name, path in ARCHIVES.items()}
    report = dict(scope=__doc__, binary_sha256=binary_hash, archives=original_hashes,
                  profiles=results, missing_data=missing, passed=True)
    if args.baseline_binary:
        old = args.baseline_binary.resolve()
        previous = {edition: baseline_original(game, edition, old, prepare, out) for edition in EDITIONS}
        for result in results:
            assert previous[result['edition']] == result['startup_frames']['original'], \
                (result['edition'], 'original look differs from previous delivery')
        report['previous_default_frames'] = dict(binary_sha256=sha(old), frames=previous,
                                                 all_three_identical=True)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS unavailable profile omitted; supplied archives and native settings unchanged', flush=True)


if __name__ == '__main__':
    main()
