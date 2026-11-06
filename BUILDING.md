# Building BatMahn/Mercs-Port

Source repository: **[BatMahn/Mercs-Port](https://github.com/BatMahn/Mercs-Port)**.
Maintained by **[BatMahn](https://github.com/BatMahn)**.

This source builds directly against **SDL3 3.2.0 or newer**. The migration was
checked with SDL **3.4.18**. SDL2 headers, `sdl2-config`, and `sdl2-compat` are not
used by the Mercenaries executable. Missions use **Metal on macOS**, **Vulkan
on Linux**, and **D3D12 or Vulkan on Windows**. The active application no longer
uses OpenGL. DOS, Titanium Direct3D and dedicated 3dfx remain independent
game-appearance choices. See [RENDERER.md](docs/RENDERER.md).

Clone the repository, then run the platform-specific commands below:

```sh
git clone https://github.com/BatMahn/Mercs-Port.git
cd Mercs-Port
```

A source ZIP can instead be extracted into a fresh `Mercs-Port/` directory.
Keep the `Mercs-game/` folder separate; its data, music, installation overlays, and saved
careers retain the layout documented in [FOLDER_SETUP.md](FOLDER_SETUP.md).
If reusing a source checkout, run `make clean` before the first SDL3 build so
old SDL/OpenGL object files cannot be linked into the new executable.

## Dependencies

- A C99 compiler and GNU Make (Apple's command-line developer tools on macOS;
  GCC or Clang on Linux; MinGW-w64 in MSYS2 on Windows).
- SDL3 development headers and its matching native library, version 3.2.0+.
- `pkg-config` or the compatible `pkgconf` command, with the `sdl3` module
  visible in its search path.
- A supported platform GPU API and driver: Metal on macOS, Vulkan on Linux,
  or Direct3D 12/Vulkan on Windows. OpenGL development files are not needed.
- The compiled shaders are included as C sources; normal builds need no
  glslang, SPIRV-Cross, DXC, or Metal development command-line compiler.
- Python 3 for the regression runners. It is not required to run the game.

The project discovers SDL3 with the official `pkg-config sdl3` interface:

```sh
pkg-config --modversion sdl3
pkg-config --cflags --libs sdl3
make check-sdl3
```

`SDL3_DIR=/path/to/prefix` selects a private SDL3 prefix through its
`lib/pkgconfig/sdl3.pc`. `PKG_CONFIG=/path/to/pkg-config` selects another
configuration tool. For a
private SDL installation, point `PKG_CONFIG_PATH` at the directory containing
`sdl3.pc`, usually `<prefix>/lib/pkgconfig` or `<prefix>/lib64/pkgconfig`.
Choose the SDK for the same architecture as the compiler; a Windows cross
compiler must use the Windows SDL3 module rather than the host Linux module.

The Makefile's SDL check applies only to targets that need SDL. Data extraction
tools and non-SDL tests can still be built without it.

## macOS / Apple Silicon

Install SDL3 and pkg-config through your normal development-package setup.
With Homebrew, the packages are `sdl3` and `pkgconf`. Then, from `Mercs-Port/`:

```sh
make app
```

This builds `mercs` with the Metal backend and packages **Mercenaries.app**. Open it in Finder. The
original in-game menus and the existing game-data folder chooser remain the
entry route; normal play needs no mission command-line argument.

The app packager copies the native SDL3 dylib referenced by the executable
into `Contents/Frameworks`, rewrites its load commands, includes the menu font,
and applies a local ad-hoc signature. It also supports an already-linked
`SDL3.framework`, preserving the framework's version and resource symlinks.
It checks that SDL3 contains every architecture in the executable and refuses
unresolved additional native-library dependencies. A failed packaging or
signing step leaves the previous completed app in place.

The actual load commands and `LC_RPATH` values determine the SDL3 location
first. The packager falls back to `pkg-config --variable=libdir sdl3`. For an
unusual SDK layout, set `MERCS_SDL3_LIBRARY` to the matching dylib or the binary
inside `SDL3.framework`; the older `MERCS_SDL3_DYLIB` override is also accepted.
The ordinary Makefile build expects a working `sdl3.pc` module. Packaging an
externally built framework-linked executable can also be done directly with
`sh scripts/macos_mercs_app.sh`.

`MERCS_APP_VERSION=0.3.1 make app` is an optional example of setting a numeric
bundle version. No unrelated Mech2 release version is inherited. Ad-hoc signing
is for a locally built application; this script does not notarize a release.

BatMahn has confirmed native Metal compilation and launch. The latest cockpit
motion correction still needs a visual check on macOS; broader Retina/window,
Finder-launch and physical-device coverage remains incomplete. A Metal-capable
GPU and macOS 10.14 or newer are the backend feature floor; the actual SDL3
library and application deployment target may require a newer macOS. The portable packaging tests simulate
Mach-O tool output and cannot certify those native behaviors.

## Linux

Install your distribution's SDL3 development package, pkg-config, the C build
tools, the Vulkan loader (`libvulkan1` or `vulkan-loader`) and your GPU's
Vulkan driver. For Mesa, install `mesa-vulkan-drivers`. Then:

```sh
make mercs
./mercs
```

The data-folder chooser uses zenity or kdialog. Keep the normal SDL3 video and
audio backends enabled for interactive play. For unattended tests, SDL3's
driver environment variables use underscores:

```sh
SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=offscreen ./test_mercs_cockpit_render
```

The offscreen backend must be present in your SDL3 build and have an available
Vulkan driver. Mesa's lavapipe can run the rendering checks on a headless
machine; set `VK_DRIVER_FILES` to its installed ICD JSON when explicit driver
selection is needed. `SDL_VIDEO_DRIVER=dummy` supports software-renderer shell
tests but cannot present a mission GPU window.
The dummy audio backend exercises the mixer and stream lifecycle without
making sound. Actual output-device testing remains a separate check.

## Windows / MinGW-w64

Use an MSYS2 MinGW-w64 shell with the matching compiler, Make, pkg-config, and
SDL3 development package. SDL's official instructions recommend the UCRT64
environment for a new setup. From the source directory:

```sh
make mercs WINDOWS=1
```

For a cross compiler, set `CC` and `PKG_CONFIG` to the target-specific tools,
for example `CC=x86_64-w64-mingw32-gcc` and
`PKG_CONFIG=x86_64-w64-mingw32-pkg-config`. Make sure that tool resolves the
target SDK's `sdl3.pc`.

Place the matching **SDL3.dll** beside `mercs.exe` when running outside the
MSYS2 environment. Windows 10/11 defaults to Direct3D 12; Combat Variables
lets you select Vulkan. If the selected GPU API cannot initialize, Mercs tries
the other. `gpu=auto|d3d12|vulkan` is saved in `mercsport.cfg`.

The native Windows picker is retained; the OpenGL loader is retired. This
revision cross-compiles and links with MinGW-w64 and SDL3 3.4.18. Its imports
contain no OpenGL DLL. Native Windows launch, device fallback and graphics
execution remain to be tested on Windows.

## Building SDL3 itself

For a private shared-library installation from an official SDL source release,
the upstream CMake workflow is:

```sh
cmake -S /path/to/SDL3-source -B /path/to/SDL3-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/path/to/private-sdl3 \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF \
  -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF
cmake --build /path/to/SDL3-build --parallel
cmake --install /path/to/SDL3-build
```

Install SDL's platform development dependencies first so the required video
and audio backends are built. For Apple universal libraries, upstream supports
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`; the application and every bundled
library must agree on architectures. A native single-architecture build is
sufficient for a locally built Apple Silicon application.

## Focused packaging check

```sh
make check-packaging
```

This runs the real app-packaging script against temporary simulated Mach-O
fixtures. It checks direct dylibs, framework layout, paths containing spaces,
`@rpath` and pkg-config resolution, explicit overrides, architecture rejection,
SDL2 rejection, dependency auditing, signature ordering, and preservation of an
existing app on failure, and refusal of direct OpenGL application dependencies. It needs neither the original game data nor macOS.
The gameplay regression commands are documented in the current handoff.

## GitHub builds

`.github/workflows/build.yml` builds four packages:

| Package | Built on | How |
|---|---|---|
| macOS arm64 `.app` (zip) | macOS | Homebrew SDL3, `make check-packaging` |
| Linux x86_64 tarball | Ubuntu 22.04 | SDL3 built without OpenGL, bundled with `$ORIGIN` |
| Fedora RPM | Fedora container | `packaging/mercsport.spec`, built against Fedora's SDL3 |
| Windows x64 zip | Ubuntu 24.04 | MinGW-w64 cross-compile, `SDL3.dll` beside `mercs.exe` |

When it runs:

- **Every push to `main`, every pull request, or *Run workflow*:** the packages are kept as the run's artifacts.
- **A `v*` tag:** the packages are also published as a GitHub release.

The Linux job also compiles every check program (`make test-bins`). The checks themselves need the original game
data, so they are run locally with `make check GAME=...`.

## Upstream references

- [SDL3 migration guide](https://wiki.libsdl.org/SDL3/README-migration):
  headers, `pkg-config sdl3`, and removal of the SDLmain library.
- [SDL CMake build guide](https://wiki.libsdl.org/SDL3/README-cmake):
  source installation, shared libraries, frameworks, and Apple architectures.
- [SDL3 startup guide](https://wiki.libsdl.org/SDL3/README-main-functions):
  explicit `SDL_main.h` integration.
- [SDL3 MinGW guide](https://wiki.libsdl.org/SDL3/INTRO-mingw) and
  [Windows build notes](https://wiki.libsdl.org/SDL3/README-windows):
  native MinGW-w64 tooling and the SDL3 runtime DLL.
