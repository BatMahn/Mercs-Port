# MechWarrior 2: Mercenaries (1996) - 64-bit port

A native 64-bit reimplementation of **MechWarrior 2: Mercenaries**, rebuilt from the original disc's `MERCS\MW2SHELL.EXE`
and `MERCS\MW2.EXE`. Portable C99 with SDL3, drawn through SDL3's GPU API (Direct3D 12 or Vulkan on Windows, Vulkan on
Linux, Metal on macOS); builds on macOS, Linux and Windows. You need your own copy of the game (see "Setting up the game
folder").

The program is `mercs`.

## Status (2026-11-10, v1.0)
A development release. The single-player game runs from the original menus through missions and back:

- **Shell:** the original menus and career, from the arrival sequence and pilot registration to the Office, contracts
  (including the procedurally generated ones), the DropShip and Mission Command: Duty Roster, finances, trading, hiring,
  Mech Ops, the Mech Lab, deployment, briefing, debriefing, salvage and repairs. Careers are saved in the original
  ten-slot Load/Save screen and can be exchanged with the original DOS and Windows saves. Instant Action has its own
  selections, parameters and custom designs.
- **Simulation:** movement, combat, heat, jump jets, AI, lancemate orders, AeroTech support, mission objectives, the
  cockpit instruments and all seven MFD windows.
- **Look:** chosen in Combat Variables, Graphics Options:
  - **DOS** (the default): the original software renderer.
  - **3dfx:** the dedicated Mercenaries 3dfx edition's look (needs two files from that disc).
  - **TITANIUM:** the Titanium Edition's Direct3D look (needs three files from that disc).

  The in-game videos have their own look setting: original, smooth, scanlines, CRT or curved CRT.
- **Sound:** the original's menu and mission audio, and the disc's CD audio as the music from your own rips.
- **Controls:** keyboard, mouse and joystick, set up in the original Cockpit Controls screens.

The DOS game is the reference for all behaviour. The 3dfx and Titanium editions contribute looks only.

**Not yet done or not yet checked:**

- The full career has not been played from start to finish.
- The port has not yet been compared side by side with the original running in DOSBox the way its sister ports have;
  expect differences in AI, combat and the DOS look until it is.
- Windows has been run only under Wine (Vulkan), not yet on Windows hardware. macOS (Metal) builds and plays; Linux (Vulkan) runs the tests.
- MercNet multiplayer is not implemented.

See [CHANGELOG.md](CHANGELOG.md) for what changed and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for what is still open.

## System requirements
The game draws through SDL3's GPU API. There is no OpenGL renderer.

| System | Graphics | Minimum |
|---|---|---|
| Windows 10 / 11 (64-bit) | Direct3D 12 (default) or Vulkan, the other one tried if the chosen one can't start or can't build its shaders | Direct3D 12 feature level 11_0, or Vulkan 1.0 |
| Linux (64-bit) | Vulkan | Vulkan 1.0 with a driver and the loader (`libvulkan1` / `vulkan-loader`) |
| macOS | Metal | Apple Silicon or a Metal-capable Intel Mac |

## Build
Needs a C99 compiler, make, pkg-config and SDL3 3.2.0 or newer (development files). The compiled shaders are included;
a normal build needs no shader tools.

    macOS:                  xcode-select --install; brew install sdl3 pkgconf
    Fedora:                 sudo dnf install gcc make pkgconf-pkg-config SDL3-devel vulkan-loader mesa-vulkan-drivers
    Debian / Ubuntu:        sudo apt install build-essential pkg-config libvulkan1 mesa-vulkan-drivers, and SDL3
                            (libsdl3-dev where the release has it, else build it from libsdl.org's release tarball)

    make mercs              # the game: menus + missions in one program
    make app                # macOS: "Mercenaries.app", with SDL3 bundled inside

Windows: MSYS2's MINGW64 shell with its gcc, make, pkgconf and SDL3 packages, then `make mercs WINDOWS=1`; keep
`SDL3.dll` beside `mercs.exe`. See [BUILDING.md](BUILDING.md) for cross-compiling and packaging.

## Setting up the game folder
`mercs` needs its own folder, `Mercs-game`, built from **your original Mercenaries CD**. None of the game's files are
part of this repository.

    Mercs-game/
      cd/         REQUIRED - the whole retail Mercenaries disc, copied as it is (PRODUCT.INI, MERCS\, RMG\, SMK\ ...)
      install/    created by the port - careers, saves, settings and controls
      music/      optional - the disc's CD audio, Track02..Track22 (FLAC, MP3 or WAV)
      3d/         optional - models.prj and skygnd.par from the Mercenaries 3dfx disc's MERCS\MW2.PRJ / SKYGND.PAR
      titanium/   optional - MW2.PRJ, SKYGND.PAR, particle.PID from the Titanium disc's INSTALL\16BIT\

Use files from MechWarrior 2: Mercenaries only; files from the base game or Ghost Bear's Legacy belong to different
games even where the names match. Keep the retail `MERCS\MW2SHELL.EXE` and `RMG\` in `cd/`: the generated contracts
and Instant Action read them as data (the port never runs the old executable).

**See [FOLDER_SETUP.md](FOLDER_SETUP.md) for the details:** copying the disc or a BIN/CUE image (`make cdrip`),
ripping the music, save locations and the common mistakes.

## Playing

    ./mercs /path/to/Mercs-game      # the first time; afterwards just ./mercs

On macOS, open Mercenaries.app. If no game folder is remembered or found nearby, the game asks for it with the system's
folder picker and remembers it in the settings file:

    Linux   ~/.config/mercsport/mercsport.cfg        macOS   ~/Library/Application Support/mercsport/mercsport.cfg
    Windows %APPDATA%\mercsport\mercsport.cfg

Careers live in `Mercs-game/install/retail/MERCS-CAREERS/`; back up that whole folder. Original DOS or Windows saves
placed at `install/retail/mersave0.sav` .. `mersave9.sav` can be loaded from the matching slot.

In a mission, Esc (or the right mouse button) opens the original menus; F5 opens Load/Save Game; F10 opens the video
options for the in-game videos (look and 4:3 / fill / stretch).

## Tests
The tests need your own game data, which isn't in the repository:

- `make check GAME=<Mercs-game>` runs the tool and data checks.
- `MW2_INSTALL_DIR=<install> ./sweep_objectives <MW2.PRJ> [scenes]` runs every mission's objectives headless.

## Licences
The port's source code is the author's. miniaudio (third_party/) is public domain / MIT-0, stb_truetype is public
domain / MIT, Liberation Sans (assets/fonts/) is under the SIL Open Font License, and mt32emu (third_party/mt32emu/)
is LGPL 2.1: see [THIRD_PARTY.md](THIRD_PARTY.md). MechWarrior 2: Mercenaries is Activision's. No game data is
included here.
