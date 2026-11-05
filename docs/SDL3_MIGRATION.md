# Native SDL3 migration

This source revision moves the current Mercenaries port directly to SDL3.
The original registration, office, contracts, management screens, movies,
mission launch and return remain the game flow. DOS, dedicated 3dfx and
Titanium/Direct3D still select their existing Mercs resources and native
OpenGL rendering behavior. The full DOS soundtrack remains the standard.

The implementation was compiled and tested with **SDL 3.4.18**. The Makefile
requires SDL3 **3.2.0 or newer**, discovered through `pkg-config sdl3`.
The tested executable links `libSDL3.so.0`; it has no SDL2 runtime dependency.
See [BUILDING.md](../BUILDING.md) for the platform instructions.

## Build on the Mac

Extract the ZIP into a fresh source directory so SDL2 object files cannot be
mixed into this build. Keep the existing `Mercs-game` directory and its saves.
With the Apple command-line compiler tools already available and Homebrew:

```sh
brew install sdl3 pkgconf
cd /path/to/Mercs-Port
make app
```

Open the resulting `Mercenaries.app` normally. The original in-game shell and
the existing native game-data folder chooser remain the entry route. Select
the outer `Mercs-game` folder when prompted. No data conversion or new music
extraction is required. Native save generations and original-save interchange
retain their formats.

The macOS packager bundles the SDL3 dylib actually linked by the executable,
or a complete linked SDL3 framework. It retains architecture checks, resource
and font copying, dependency checks and local ad-hoc signing. Failed packaging
preserves the previous completed app. The supplied source ZIP contains no
compiled application or SDK.

## What changed

| Area | SDL3 implementation |
| --- | --- |
| Startup | `SDL_RunApp` performs platform startup before the existing native launcher; the simulation remains its own compiled entry point. |
| Mission window | Native SDL3 window flags, display IDs, fullscreen mode selection, physical pixel-size queries and GL context destruction. Alt+Enter retains desktop fullscreen behavior. |
| Input | SDL3 keyboard fields and boolean held-key snapshots, window-scoped text and relative input, fractional pointer movement, and joystick instance-ID enumeration. Text entry stays in the original shell fields. |
| Shell composition | Original art still draws into a 640×480 canvas, then into a centered physical destination. Pointer mapping uses the physical window size instead of accidentally using the selected canvas size. |
| Movies | Native SDL3 texture/render APIs and independently saved renderer state for each render target. Logical presentation, explicit/unset viewport, clip, scale and color/blend state survive playback and resize. |
| Audio | The original mixer supplies a native SDL3 audio stream. Stream locks serialize game updates with the demand callback; destroying the owned stream stops playback before sample storage is freed. |
| Movie audio | An independent owned SDL3 stream queues the original signed-16 PCM, flushes its finite tail and starts with the first video frame. Closing a movie preserves another caller's audio device. |
| Clocks and ownership | Animation clocks use 64-bit SDL ticks. Original 32-bit random seeds retain explicit narrowing. SDL-owned base-path strings are borrowed rather than freed. |
| Build and tests | Native SDL3 discovery, direct API migration, updated event/readback assertions, new stream lifecycle checks and nine macOS packaging tests. |

The original audio decoder and mixer arithmetic were compared with the
preceding ZIP and remain unchanged. `smk.c`, `cdaudio.c`, the shell's original
sound-cue implementation, and simulation/save/economy consumers retain their
existing behavior. This migration does not replace the renderer with SDL's
GPU API or change an edition's recovered numeric rules.

## Validation results

The complete machine-readable index is
release-validation.json.
Commands, logs, input identities, source hashes and comparison conditions are
included under `docs/research/sdl3-validation/`.

| Check | Result and scope |
| --- | --- |
| Build | All 86 previous delivery targets plus the inherited `glview` and `test_inputmap` targets compiled: **88 targets**. |
| Engine/resource/preflight suite | **55 checks passed**, including real GL fixtures, original control/audio data and all three resource variants. |
| Shell/career suite | **41 checks passed**, including 16 regenerated original economy complete-document comparisons and all three original AeroTech record/deployment cases. |
| Runtime groups | **11 passed**: cockpit, moving cockpit, ground contact, movement audio, Combat Variables, steering, controls, presentation, campaign, procedural mission children and Instant Action children. |
| Career UI | **246 recorded input events and 70 captured frames** across economy, Mech Lab, Instant Action, Save/Load/interchange and the frontend journey. All six executable runs passed, including generation of 13 original-save fixtures. |
| Cockpit and ground | 12 static cockpit captures across two mechs, three looks and two sizes, plus moving-cockpit checks in all three looks. Four ground routes produced 480 state samples with zero buried-actor reports. |
| Controls | **294 MFD window cases**, HUD save/reload/reset, weapon groups, satellite permissions and camera returns passed. |
| Viewport | 40 rendered frames and 38 point mappings passed, covering poisoned renderer state, target reset, integer/fractional DPI and subpixel letterbox edges. |
| Audio | Original PCM/voice goldens, 73 original shell WAVs, gain/gait cases, variable stream requests, pause/recursive locking, independent devices and teardown passed. ASan and UBSan passed with leak detection disabled. |
| Packaging | **9 tests passed** against the actual macOS script using simulated Mach-O tools, including architecture/dependency failures and preservation of an existing app. |
| Linux window backends | SDL3 GL context creation passed with both offscreen EGL and X11/GLX under Xvfb. Both used Mesa software GL. |

The optional independent 3dfx/Titanium **original DLL** weapon-table oracle was
not rerun because those raw DLLs were absent from the restored game bundle.
The earlier oracle evidence and its unchanged implementation remain in the
source. All three graphics profiles were exercised using their actual supplied
archives. No optional shell fixture group was skipped.

The legacy input-map test requires an installed layout: original `GAMEKEY.MAP`
and `INPUT.MAP` beside `GIDDI`. After composing that layout privately from the
supplied reference game, its 67 bindings and 56 control rules passed unchanged.
Earlier setup failures are documented in the validation index.

Every runtime group passed individually on its first execution. An additional
hash guard observed the concurrent rebuilding of six UI test executables, so
four affected groups were repeated after all test binaries were stable. The
final runtime report accepts seven unaffected runs and four fresh runs, with
separate hashes and logs. All 25 tracked executables were stable during that
revalidation. No assertion was removed to obtain a passing result.

Existing integer-conversion warnings in unchanged `mercscareer.c` appear when
building legacy targets. The SDL3 migration compiles successfully; the validation
log records the compiler output rather than describing the whole tree as
warning-free.

## Direct comparison with the preceding SDL2 port

Both versions used the same supplied data and the same Linux software-GL stack.
Movie files were compared after decoding their BMP containers to RGB, because
SDL2 and SDL3 encode different BMP metadata.

| Paired capture | Size | Differing RGB pixels |
| --- | --- | ---: |
| DOS intro, frame 600, Original presentation | 960×600 | 0 / 576,000 |
| DOS intro, frame 600, CRT presentation | 960×600 | 0 / 576,000 |
| Kodiak, OUT3SCN1, DOS | 640×480 | 0 / 307,200 |
| Kodiak, OUT3SCN1, Direct3D | 640×480 | 0 / 307,200 |
| Kodiak, OUT3SCN1, 3dfx | 640×480 | 0 / 307,200 |
| **Total** | | **0 / 2,073,600** |

The three mission runs also matched all **124 recorded final state fields**.
These used two seconds at the fixed 30-fps test step, zero throttle/twist, a
frozen sky and private installation overlays. This proves parity for the five
selected captures against the preceding port; it does not prove every original
hardware frame or every runtime trajectory matches.

The comparison report
records the conditions and hashes. Newly captured game images remain outside
the source ZIP; the report contains their metadata and numerical comparisons.

## Data preservation and remaining limits

All **1,098 supplied Mercs game files**, totaling 494,485,887 bytes, retained
their original hashes through validation. Tests used private writable overlays.
The source archive preserves every file from the preceding source ZIP and
includes implementation hashes for verifying the delivered source.

Native macOS/M4 Pro and Windows compilation or execution was not available.
Retina/fullscreen behavior, Finder launch, real sound-device output and physical
joysticks therefore still need an on-machine check. The Linux audio tests used
SDL's dummy device; they verify decoded samples, mixing and lifetime behavior,
not audible output quality. LeakSanitizer could not inspect the environment's
process metadata, so its check is not claimed.

A complete campaign has not been played through ordinary gameplay. Complete
original timing/RNG/physics/raster parity is not claimed. MercNet multiplayer
remains unimplemented. The current boundaries remain in
[KNOWN_ISSUES.md](../KNOWN_ISSUES.md).

## Upstream references

- [SDL3 migration guide](https://wiki.libsdl.org/SDL3/README-migration)
- [SDL3 audio streams](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream)
- [SDL3 startup](https://wiki.libsdl.org/SDL3/NonstandardStartup)
- [SDL3 releases](https://github.com/libsdl-org/SDL/releases)
- [Homebrew SDL3](https://formulae.brew.sh/formula/sdl3)
- [Homebrew pkgconf](https://formulae.brew.sh/formula/pkgconf)
