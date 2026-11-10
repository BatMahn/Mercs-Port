#!/usr/bin/env python3
"""Linux GUI regression: real SDL input, actual movies and native mission child.
Requires Xvfb, libX11, libXtst (input SDL 3 accepts), ffmpeg and a window manager (openbox) for the fullscreen check. Uses a fresh private copy of the supplied game
assets with an empty native install overlay. Choose a new output directory for
each run so previous saves and interrupted fixture copies cannot affect it.
The picker is a stand-in command; native macOS/Windows dialogs need local testing.
"""
from pathlib import Path
import argparse
import ctypes as C
import ctypes.util
import hashlib
import json
import os
import shlex
import shutil
import signal
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--exe', type=Path, default=Path(__file__).resolve().parents[1] / 'mercs')
parser.add_argument('--profiles-only', action='store_true', help='Check all three game looks and preserved Training assignment; skip the already-tested intro')
args = parser.parse_args()
OUT = args.output.resolve()
EXE = args.exe.resolve()
GAME = OUT / "Mercs data 'quoted'"
SOURCE = args.game.resolve()
if SOURCE == OUT or SOURCE in OUT.parents or OUT in SOURCE.parents:
    parser.error('--output must be separate from the supplied game directory')
if GAME.exists():
    parser.error('The private game fixture already exists; choose a new --output directory')
OUT.mkdir(parents=True, exist_ok=True)

def copy_fixture(source, destination):
    # Buffered copying avoids fast-copy/sendfile surprises in virtual filesystems.
    with open(source, 'rb') as incoming, open(destination, 'wb') as outgoing:
        shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
    shutil.copystat(source, destination)
    if os.stat(source).st_size != os.stat(destination).st_size:
        raise RuntimeError('Incomplete private fixture copy: ' + str(source))
    return destination

# Saved careers, original-format exports, settings and generated launch files
# belong to this run. Do not inherit the supplied game's writable overlay.
def ignore_source_overlay(directory, names):
    return ['install'] if Path(directory).resolve() == SOURCE and 'install' in names else []

shutil.copytree(SOURCE, GAME, copy_function=copy_fixture, ignore=ignore_source_overlay)
for overlay in ('retail', 'direct3d', '3dfx'):
    (GAME / 'install' / overlay).mkdir(parents=True)
fixture_archives = []
for relative in ('cd/MERCS/MW2.PRJ', '3d/models.prj', 'titanium/MW2.PRJ'):
    original, copied = SOURCE / relative, GAME / relative
    if original.is_file():
        digest = hashlib.sha256(original.read_bytes()).hexdigest()
        if hashlib.sha256(copied.read_bytes()).hexdigest() != digest:
            raise RuntimeError('Private fixture hash mismatch: ' + relative)
        fixture_archives.append({'path': relative, 'bytes': copied.stat().st_size, 'sha256': digest})
print('Verified private fixture archives:', fixture_archives, flush=True)
CFG = OUT / 'gui.cfg'
CFG.write_text('resolution=window\nvideos=crt\nvideo_fit=fill\n')
PICKER = OUT / 'picker.sh'
PICKER.write_text("#!/bin/sh\nprintf '%s\\n' chosen >> " + shlex.quote(str(OUT / 'picker-calls.txt')) + '\nprintf "%s\\n" ' + shlex.quote(str(GAME)) + '\n')
PICKER.chmod(0o700)
COUNTER = OUT / 'picker-calls.txt'
if COUNTER.exists(): COUNTER.unlink()

xlib = C.CDLL(ctypes.util.find_library('X11'))
_xtst_name = ctypes.util.find_library('Xtst')
xtst = C.CDLL(_xtst_name) if _xtst_name else None
if xtst:
    xtst.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    xtst.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
Window = C.c_ulong
Display = C.c_void_p
class KeyEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', Display), ('window', Window), ('root', Window),
                ('subwindow', Window), ('time', C.c_ulong), ('x', C.c_int), ('y', C.c_int),
                ('x_root', C.c_int), ('y_root', C.c_int), ('state', C.c_uint),
                ('keycode', C.c_uint), ('same_screen', C.c_int)]
class Event(C.Union):
    _fields_ = [('key', KeyEvent), ('pad', C.c_long * 24)]
xlib.XOpenDisplay.argtypes = [C.c_char_p]; xlib.XOpenDisplay.restype = Display
xlib.XDefaultRootWindow.argtypes = [Display]; xlib.XDefaultRootWindow.restype = Window
xlib.XQueryTree.argtypes = [Display, Window, C.POINTER(Window), C.POINTER(Window), C.POINTER(C.POINTER(Window)), C.POINTER(C.c_uint)]
xlib.XFetchName.argtypes = [Display, Window, C.POINTER(C.c_char_p)]
xlib.XFree.argtypes = [C.c_void_p]
xlib.XStringToKeysym.argtypes = [C.c_char_p]; xlib.XStringToKeysym.restype = C.c_ulong
xlib.XKeysymToKeycode.argtypes = [Display, C.c_ulong]; xlib.XKeysymToKeycode.restype = C.c_ubyte
xlib.XSendEvent.argtypes = [Display, Window, C.c_int, C.c_long, C.POINTER(Event)]
xlib.XSetInputFocus.argtypes = [Display, Window, C.c_int, C.c_ulong]
xlib.XFlush.argtypes = [Display]
xlib.XWarpPointer.argtypes = [Display, Window, Window, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int, C.c_int]
xlib.XGetGeometry.argtypes = [Display, Window, C.POINTER(Window), C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint)]
xlib.XCloseDisplay.argtypes = [Display]

display_name = next(':' + str(n) for n in range(210, 240)
                    if not Path('/tmp/.X' + str(n) + '-lock').exists())
env = {k: v for k, v in os.environ.items() if not k.startswith('MERCS_')}
env.update(DISPLAY=display_name, SDL_VIDEO_DRIVER='x11', SDL_AUDIO_DRIVER='dummy',
           LIBGL_ALWAYS_SOFTWARE='1', MERCS_CONFIG=str(CFG),
           MERCS_PICKER=shlex.quote(str(PICKER)),
           MERCS_AUTOPILOT=f'10,0,0,0,{OUT}/native-first-mission.ppm,1',
           MERCS_STATE_DUMP='0.5,2,9', MERCS_SIZE='640x400')
env.pop('MERCS_SHELL_SKIP_INTRO', None)
if args.profiles_only: env['MERCS_SHELL_SKIP_INTRO'] = '1'
report = {'checks': [], 'captures': [], 'display': display_name,
          'scenario': 'game-looks' if args.profiles_only else 'full-shell',
          'fixture_archives': fixture_archives}
server = app = wm = None
display = None
logfile = None
LOG_PATH = OUT / 'gui.log'

def log_text():
    return LOG_PATH.read_text(errors='replace') if LOG_PATH.exists() else ''

def wait_for(needle, timeout=25, count=1):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if log_text().count(needle) >= count: return
        if app.poll() is not None: raise RuntimeError(f'Application exited {app.returncode}, waiting for {needle}:\n' + log_text()[-5000:])
        time.sleep(0.05)
    raise RuntimeError('Timed out waiting for ' + needle + '\n' + log_text()[-5000:])

def children(win):
    root, parent, count = Window(), Window(), C.c_uint()
    result = C.POINTER(Window)()
    if not xlib.XQueryTree(display, win, C.byref(root), C.byref(parent), C.byref(result), C.byref(count)): return []
    found = [result[i] for i in range(count.value)]
    if result: xlib.XFree(result)
    return found

def find_window():
    pending = children(root_window)
    while pending:
        win = pending.pop()
        name = C.c_char_p()
        if xlib.XFetchName(display, win, C.byref(name)) and name:
            title = C.string_at(name).decode(errors='replace'); xlib.XFree(name)
            if title == 'MechWarrior 2: Mercenaries': return win
        pending.extend(children(win))
    raise RuntimeError('Mercs SDL window not found')

def has_focus():
    # (a repeated XSetInputFocus while a modifier is held makes SDL re-read the keyboard on the focus change and drop
    # the held Alt: set it only when the window does not have it)
    focus, revert = Window(), C.c_int()
    xlib.XGetInputFocus(display, C.byref(focus), C.byref(revert))
    return focus.value == window

def send_key(name, down, state=0):
    code = xlib.XKeysymToKeycode(display, xlib.XStringToKeysym(name.encode()))
    if not code: raise RuntimeError('Unknown key ' + name)
    if down and not (xtst and has_focus()): xlib.XSetInputFocus(display, window, 2, 0)
    if xtst:   # (as click: XTest reaches SDL3's XInput2 keyboard and modifier state)
        xtst.XTestFakeKeyEvent(display, code, 1 if down else 0, 0); xlib.XFlush(display); time.sleep(0.06)
        return
    kind, mask = (2, 1) if down else (3, 2)
    e = Event(); e.key.type = kind; e.key.display = display; e.key.window = window
    e.key.root = root_window; e.key.same_screen = 1; e.key.keycode = code; e.key.state = state
    # Escape can destroy the window on keydown. Deliver keyup to X11's current
    # InputFocus destination instead of addressing that now-destroyed window.
    if not xlib.XSendEvent(display, window if down else 1, 0, mask, C.byref(e)): raise RuntimeError('XSendEvent key failed')
    xlib.XFlush(display); time.sleep(0.06)

def press(name, state=0):
    send_key(name, True, state); send_key(name, False, state)

def alt_enter():
    send_key('Alt_L', True)
    press('Return', 8)
    send_key('Alt_L', False, 8)
    time.sleep(1)   # (the window manager - or X11 without one - applies the change asynchronously)

def type_text(value):
    for char in value:
        press('space' if char == ' ' else char)

def click(x, y):
    root, gx, gy, width, height, border, depth = Window(), C.c_int(), C.c_int(), C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
    if not xlib.XGetGeometry(display, window, C.byref(root), C.byref(gx), C.byref(gy), C.byref(width), C.byref(height), C.byref(border), C.byref(depth)):
        raise RuntimeError('Cannot read window geometry')
    scale = min(width.value / 640, height.value / 480)
    px = round((width.value - 640 * scale) / 2 + x * scale)
    py = round((height.value - 480 * scale) / 2 + y * scale)
    xlib.XSetInputFocus(display, window, 2, 0)
    xlib.XWarpPointer(display, 0, window, 0, 0, 0, 0, px, py)
    xlib.XFlush(display); time.sleep(0.1)
    if xtst:   # SDL3 reads the mouse through XInput2, which a sent core event does not reach: press through XTest
        for pressed in (1, 0):
            xtst.XTestFakeButtonEvent(display, 1, pressed, 0); xlib.XFlush(display); time.sleep(0.05)
        time.sleep(0.15)
        return
    for kind, mask, state in ((4, 4, 0), (5, 8, 256)):
        e = Event(); e.key.type = kind; e.key.display = display; e.key.window = window
        e.key.root = root_window; e.key.same_screen = 1; e.key.keycode = 1
        e.key.x = px; e.key.y = py; e.key.x_root = px + gx.value; e.key.y_root = py + gy.value; e.key.state = state
        if not xlib.XSendEvent(display, window, 0, mask, C.byref(e)): raise RuntimeError('XSendEvent button failed')
    xlib.XFlush(display); time.sleep(0.2)

def capture(name):
    time.sleep(0.25)
    path = OUT / (name + '.png')
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'x11grab',
                    '-video_size', '1280x1024', '-i', display_name, '-frames:v', '1',
                    '-update', '1', '-y', str(path)], env=env, check=True, timeout=15)
    report['captures'].append(str(path)); print('CAPTURE', name, flush=True)

def go(x, y, page):
    needle = 'mercs shell: page=' + page + '\n'
    count = log_text().count(needle) + 1
    click(x, y)
    wait_for(needle, count=count)
    time.sleep(1.5)   # (a page's opening clip plays before it takes clicks)

def skip_movie(name):
    needle = 'movie-ended=' + name + ' result=0'
    count = log_text().count(needle) + 1
    wait_for('mercs shell: movie=' + name + ' ', count=count)
    press('space')
    wait_for(needle, count=count)

def escape_to_menu():
    # Escape opens the original game menu over a career page; its first entry (New) leaves for the main menu, after
    # "Save Game?" (No) when the career has unsaved changes
    for _ in range(4):
        if last_page() == 'main-menu': return
        press('Escape'); time.sleep(0.5)
        press('Return'); time.sleep(0.8)
        if last_page() != 'main-menu': press('n'); time.sleep(0.8)
    if last_page() != 'main-menu': raise RuntimeError('Could not return to original main menu')

def register_commander():
    click(500, 60)
    skip_movie('MLAND2')
    wait_for('page=starport')
    capture('02-starport')
    go(260, 320, 'registration')
    capture('03-registration')
    type_text('hawkes'); press('Return'); type_text('merc unit')
    capture('04-registration-filled')
    go(550, 452, 'news')
    wait_for('registered pilot=HAWKES')
    capture('05-initial-letter')
    go(558, 450, 'starport')
    go(125, 300, 'office')
    time.sleep(2)  # let the original entering-door clip settle
    capture('06-office')
    go(240, 405, 'office-computer')
    capture('07-office-computer')
    go(150, 60, 'contracts')
    capture('08-contract-training')
    click(180, 400); capture('09-contract-scalpel')
    click(70, 400)
    go(450, 453, 'office-computer')
    wait_for('contract-accepted=TRA1CAM1')
    go(555, 450, 'office')
    go(145, 235, 'starport')
    click(465, 330)
    skip_movie('MTO')
    wait_for('page=dropship')
    capture('10-dropship')
    go(270, 250, 'mission-command')
    capture('11-mission-command')
    go(120, 240, 'duty-roster'); capture('12-original-duty-roster')
    go(550, 450, 'mission-command')
    report['checks'].append('Original Commander menu -> MLAND2 -> starport terminal -> registration/letter -> office/computer -> original contract paging/accept -> MTO -> DropShip/mission command')

def quit_app():
    # The original menu's Escape opens the game menu; its Quit entry asks "Sell Out?" (Yes), then, with unsaved
    # changes, "Save Game Before Exiting?" (No)
    press('Escape'); time.sleep(0.5)
    press('Up'); press('Up'); press('Return'); time.sleep(0.8)
    press('y'); time.sleep(0.8)
    if app.poll() is None: press('n')
    app.wait(timeout=10)

def last_page():
    pages = [l for l in log_text().splitlines() if l.startswith('mercs shell: page=')]
    return pages[-1].split('=', 1)[1] if pages else ''

def leave_debriefing():
    # After a return the original shows the debriefing; its leave button (NEXT / CONTRACTS / REPLAY) settles the
    # mission and lands on the briefing or the StarPort. Walk back to mission command for the next look.
    wait_for('page=debriefing', count=log_text().count('mission-return=') )
    time.sleep(1)
    click(420, 447)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline and last_page() == 'debriefing':
        if log_text().rstrip().split('\n')[-1].startswith('mercs shell: movie=') : press('space')
        time.sleep(0.2)
    if last_page() == 'briefing': go(552, 465, 'mission-command')
    elif last_page() == 'starport':
        click(465, 330); skip_movie('MTO'); wait_for('page=dropship', count=log_text().count('page=dropship') )
        go(270, 250, 'mission-command')
    if last_page() != 'mission-command': raise RuntimeError('Could not return to mission command after the debriefing, at ' + last_page())

def launch_profiles():
    for index, (name, x, overlay) in enumerate((('dos', 220, 'retail'), ('direct3d', 365, 'direct3d'), ('3dfx', 520, '3dfx')), 1):
        press('F10'); wait_for('page=video-options')
        click(x, 361)
        if name != 'dos': wait_for('game-look=' + name)
        config = CFG.read_text()
        assert 'renderer=' + name in config
        capture('look-' + name + '-options')
        press('Escape')
        go(120, 424, 'briefing')
        capture('briefing-' + name)
        if index == 1:
            click(490, 280); wait_for('original-clip=TRA1BRF1')
            capture('briefing-original-av')
        count = log_text().count('mission-return=TRA1SCN1 result=0') + 1
        click(420, 452)
        wait_for('mission-return=TRA1SCN1 result=0', timeout=60, count=count)
        expected_paths = 'launch-profile=' + name + ' overlay=' + str(GAME / 'install' / overlay) + ' music=' + str(GAME / 'music')
        assert expected_paths in log_text()
        # the deployed loadout went out through the overlay; the shell deletes that transient MEK once the return is committed
        assert 'return committed' in log_text() or (GAME / 'install' / overlay / 'MEK/PLY00MER.MEK').is_file()
        assert log_text().count('Player: COMMANDO / PLY00MER') == index
        frame = OUT / 'native-first-mission.ppm'
        assert frame.stat().st_size > 640 * 400 * 3
        frame.replace(OUT / ('native-' + name + '.ppm'))
        leave_debriefing()
        report['checks'].append(name + ': original mission-command/briefing launch, preserved accepted contract/owned Commando, private edition overlay and full DOS music, 10 seconds of actual simulation and normal return')

try:
    with (OUT / 'xvfb.log').open('w') as xlog:
        server = subprocess.Popen(['Xvfb', display_name, '-screen', '0', '1280x1024x24', '-nolisten', 'tcp', '-ac', '-noreset'],
                                  env=env, stdout=xlog, stderr=subprocess.STDOUT, start_new_session=True)
    for _ in range(100):
        if server.poll() is not None: raise RuntimeError('Xvfb failed: ' + (OUT / 'xvfb.log').read_text())
        display = xlib.XOpenDisplay(display_name.encode())
        if display: break
        time.sleep(0.05)
    if not display: raise RuntimeError('Cannot open local test display')
    root_window = xlib.XDefaultRootWindow(display)
    # SDL 3 asks the window manager for fullscreen (_NET_WM_STATE): without one the Alt+Enter check cannot pass.
    # Use openbox when it is installed.
    if shutil.which('openbox'):
        wm = subprocess.Popen(['openbox'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        time.sleep(1)
    logfile = LOG_PATH.open('w')
    app = subprocess.Popen([str(EXE)], cwd=OUT, env=env,
                           stdout=logfile, stderr=subprocess.STDOUT, start_new_session=True)
    if not args.profiles_only:
        wait_for('movie=MINTRO'); window = find_window()
        time.sleep(1)
        alt_enter(); press('space'); wait_for('movie-ended=MINTRO result=0')
        assert 'resolution=desktop' in CFG.read_text()
        alt_enter(); assert 'resolution=window' in CFG.read_text()
        report['checks'].append('No-argument folder picker, actual MINTRO playback, fullscreen toggle and restored shell renderer')
    wait_for('page=main-menu'); window = find_window()
    capture('01-original-menu')
    register_commander()
    launch_profiles()
    escape_to_menu()
    click(500, 135); wait_for('page=starport')
    time.sleep(3)   # (the StarPort's opening clips play before it takes clicks)
    go(260, 320, 'registration')
    type_text('cadet'); press('Return')
    go(550, 452, 'news'); go(558, 450, 'starport')
    click(465, 330); skip_movie('MTO'); wait_for('page=dropship')
    go(270, 250, 'mission-command'); go(120, 240, 'duty-roster')
    capture('training-unassigned-original-roster')
    # NEXT three times: Commando -> Jenner -> Panther (the shell logs no assignment; the launch's Player line checks it)
    for _ in range(3):
        click(560, 315); time.sleep(1.5)
    capture('training-panther-original-roster')
    go(550, 450, 'mission-command'); go(120, 330, 'briefing')
    click(420, 452); wait_for('mission-return=TRA1SCN1 result=0', timeout=60, count=4)
    assert 'Player: PANTHER / PNT00MER' in log_text()
    (OUT / 'native-first-mission.ppm').replace(OUT / 'native-training-panther.ppm')
    report['checks'].append('Original Mercenary mode/letter/DropShip/Duty Roster; explicit NEXT selection cycles Commando -> Jenner -> Panther; actual Panther deployment returns normally')
    # Changing one shell preference must merge the latest simulation settings.
    config = CFG.read_text().replace('hud_layout=wide', 'hud_layout=original').replace('dos_shading=dithered', 'dos_shading=smooth')
    CFG.write_text(config)
    press('F10'); click(440, 225); click(440, 294)
    assert 'hud_layout=original' in CFG.read_text() and 'dos_shading=smooth' in CFG.read_text()
    click(365, 361); wait_for('game-look=direct3d', count=2)
    press('Escape'); escape_to_menu(); quit_app()
    assert app.returncode == 0 and COUNTER.read_text().count('chosen') == 1
    logfile.close()
    LOG_PATH = OUT / 'warm-start.log'; logfile = LOG_PATH.open('w')
    env['MERCS_PICKER'] = 'exit 75'; env['MERCS_SHELL_SKIP_INTRO'] = '1'
    app = subprocess.Popen([str(EXE)], cwd=OUT, env=env, stdout=logfile, stderr=subprocess.STDOUT, start_new_session=True)
    wait_for('startup-look=direct3d'); wait_for('page=main-menu')
    window = find_window(); capture('12-remembered-look-folder')
    quit_app()
    assert app.returncode == 0 and COUNTER.read_text().count('chosen') == 1
    report['checks'].append('Fullscreen/CRT/fitting preferences still merge safely; Direct3D and game folder survive restart without a repeated picker')
    report['success'] = True
except Exception as error:
    report['success'] = False; report['error'] = str(error)
    try:
        if display and app and app.poll() is None: capture('failure')
    except Exception: pass
    print('FAIL', str(error), flush=True)
finally:
    if display: xlib.XCloseDisplay(display); display = None
    for process in (app, wm, server):
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: os.killpg(process.pid, signal.SIGKILL); process.wait()
    if logfile: logfile.close()
    (OUT / 'gui-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
if not report.get('success'): raise SystemExit(1)
