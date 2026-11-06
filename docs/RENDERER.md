# Mercenaries render backend

Project: **[BatMahn/Mercs-Port](https://github.com/BatMahn/Mercs-Port)**.

Mercs now draws through **SDL3 GPU**, using Metal on macOS, Vulkan on Linux,
and Direct3D 12 or Vulkan on Windows. The active application has no OpenGL
renderer, loader, context creation, GL headers or GL link libraries. The
existing `glr`, `glview` and `glshot` names are retained for source/API and
command compatibility; their implementations use the new backend.

## Platform selection

| Platform | Mission renderer | Menu renderer |
| --- | --- | --- |
| Windows 10/11 | D3D12 first by default; Vulkan selectable; tries the other API if device/window initialization fails | Same preference order, then SDL software |
| Linux | Vulkan | Vulkan, then SDL software |
| macOS | Metal | Metal |

`mercsport.cfg` accepts `gpu=auto`, `gpu=d3d12` or `gpu=vulkan`. Windows
Combat Variables exposes this setting beside Resolution. macOS and Linux
show their fixed API. A change applies to the next mission launch; menus use
the selection from their next application start. Existing configuration files
without `gpu` select Auto. `MERCS_GPU` overrides the preference for diagnosis;
`MW2_GPU` is accepted if the Mercs variable is absent or empty.

The game appearance setting remains independent: DOS, dedicated Mercs 3dfx,
and Titanium Direct3D still read their own original models, palettes and
textures. Selecting the Vulkan API does not select a different game edition.

A failed mission device initialization reports the platform-specific reason
on stderr and in a message box for interactive launches. There is no legacy
renderer fallback. The menu's permitted software fallback cannot request a
hidden OpenGL framebuffer: its explicit driver list and SDL framebuffer hint
exclude that route.

## Backend boundary

`src/rb.h` is the small drawing interface. `src/rb_gpu.c` implements it with
SDL_GPU; no SDL_GPU object is exposed to the world renderer or HUD. This keeps
a future legacy backend possible without implementing one now.

The interface records a current target, viewport, scissor, depth/blend state,
program, uniforms, textures and buffers. Uploads and draws execute in order
at presentation, fenced readback or an explicit finish. Early uploads share
a transfer pass; updates after an earlier draw preserve that draw's resource
contents. Pipelines and samplers are cached and adjacent compatible HUD
streams are combined.

Coordinate and data contracts:

- Viewports, scissors, blit rectangles and readback rectangles use bottom-left
  coordinates. Texture uploads contain top-first rows.
- `rb_read_rgb` and `rb_read_rgba` return bottom-first rows. The latter retains
  actual GPU alpha and reports transfer success, supporting rendering tests.
- The backend preserves level-zero-only nearest/linear filtering and existing
  numeric filter/wrap settings. No OpenGL enum is needed by callers.
- Offscreen color targets can have depth. The backend prefers a supported
  24-bit depth format, otherwise 32-bit float. Requested MSAA steps down to a
  sample count supported by both color and depth attachments.
- Full clears use render-pass load operations. Scissored clears use the
  internal clear shader. HUD rendering cannot accidentally write depth.
- Scratch blit targets and fallback resources belong to the device and are
  reset on shutdown. Texture-table growth cannot invalidate a recorded blit.

## Mercs world and HUD preservation

This migration uses the MW2 v1.1.0 backend as a starting point and ports the
existing Mercs shaders. It does not replace the Mercs world renderer with the
upstream MW2 world implementation.

Mercs has a geometry stage that clips near-plane crossings, recomputes its
fog at newly clipped vertices, and then places cockpit and world geometry in
a shared depth range. SDL_GPU has no geometry shader stage. The accelerated
vertex shader therefore reads a packed 25-float record and emits up to two
triangles for each source triangle. A spare second triangle degenerates when
unneeded. This preserves the earlier clipping and interpolation sequence,
including billboard behavior, without a separate CPU projection path.

The records contain the original 13 mesh floats, bounding sphere, Titanium
color, cockpit identifier, WTBO distance interval and satellite material.
The DOS path retains its own vertex count, integer projection, span walking,
polygon indexing and palette-first texture filtering. Mercs' 512 cockpit
painter ranks, ground renderer, indexed sprites, dedicated reciprocal-depth
fog, height visibility and Titanium lighting remain represented explicitly.

Clip-depth conversion happens after the legacy projection and forced depth
values. The prescribed division form is used for nonzero `w`; exactly zero
`w` uses the finite homogeneous limit to avoid a NaN at the eye plane.
Fragment coordinates are converted to the earlier bottom-left convention for
DOS spans and horizons. Palette points and lines are submitted as one-pixel
triangles through a separate program.

The HUD retains its deferred SHP palette-upload cache, font atlases, blending,
clipping and existing draw routes. Palette updates within a frame preserve
already-recorded draws. Loading screens, screenshots, MFD viewports, native
and original-resolution targets all use the same backend.

The six-vertex accelerated path performs more vertex work than the previous
geometry-stage path. Linux software-renderer checks establish correctness,
not hardware performance on macOS or a Windows GPU. Native Metal
and D3D12 performance still require measuring on those systems.

## Shader sources and regeneration

`shaders/` contains GLSL 450 sources for the two world variants, HUD, palette
particles and partial clears. The checked-in `src/rb_shaders.c` and `.h`
contain Vulkan SPIR-V, Metal MSL 2.1, signed Direct3D 12 DXIL, and reflected C
uniform layouts. An ordinary build needs **no shader compiler**.

After changing a shader:

```sh
make shaders
# Or, with tool executables under a private prefix:
make shaders SHADER_TOOLS=/path/to/shader-tools
```

The generator uses glslangValidator, SPIRV-Cross and DXC. It checks shared
uniform-block layouts, runs `spirv-val` when available and rejects unsigned
DXIL containers. The SDL_GPU migration revision was generated with glslang 15.1.0,
SPIRV-Cross commit `57e4efcc84112abcd426dc5ec248b5339bee04f2`, and official
DXC 1.9.2609.5. The standalone DXIL validator was also run on every generated
container. SPIRV-Cross was built with its Makefile, which does not embed its
Git revision in the executable; the exact commit is recorded here.

The 2026-11-06 revision (the DOS plate-colour change in world.frag) was regenerated with glslang 15.1.0, the
Ubuntu 24.04 SPIRV-Cross package (2021.01.15+1.3.239.0) and the DXC 1.8.2505.1 release (libdxcompiler 1.9-dev);
its DXIL is signed by that release's validator. The tool change reorders some generated text in rb_shaders.c; the
shaders' behaviour differs only by that change, which the Vulkan runs here confirm. Metal and Direct3D 12 output
has not been run on hardware since.

## Validation and limits

See the migration evidence for the exact
builds, fixture counts, source hashes and still-frame comparisons. Vulkan
execution is verified in this environment. Windows cross-compilation and
DXIL validation do not establish D3D12 runtime behavior. Generated Metal
source and portable app-packaging tests do not establish native Metal
compilation or macOS execution. Separately, BatMahn has confirmed that the
Metal build compiles and launches on macOS. The latest cockpit motion
correction still needs native Mac visual confirmation.

The earlier research and source snapshots are retained under `docs/` as
historical evidence, including the retired loader files. They are excluded
from every active application and renderer-test build. No OpenGL 2.1 backend
is included.

## Upstream references

- [BatMahn/MW2-Port renderer migration, v1.1.0](https://github.com/BatMahn/MW2-Port/tree/v1.1.0), commit `38a6834a1081895cf118325bd3ab1c862e7756b5`.
- [SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU).
- [SDL_CreateGPUShader resource binding rules](https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader).
- [SDL_CreateGPUDevice](https://wiki.libsdl.org/SDL3/SDL_CreateGPUDevice).
- Supplied renderer handoff.
