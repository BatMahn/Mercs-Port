# Third-party components

| Component | Use | Licence |
|-----------|-----|---------|
| miniaudio (third_party/miniaudio.*) | decoding the CD soundtrack (FLAC / MP3 / WAV) | public domain (Unlicense) or MIT-0 |
| stb_truetype (src/third_party/stb_truetype.h) | rendering the port's own settings text | public domain or MIT |
| Liberation Sans Bold (assets/fonts) | the port's own settings text, matched to the original artwork | SIL Open Font License 1.1 |
| SDL3 (system library) | window, input, audio output, SDL_GPU and menu rendering | zlib |

Mercenaries archives, art, music and text are read from the separate original
game folder. This source package does not bundle original game executables,
PRJ archives, movies or soundtrack files. Application-icon assets under
`packaging/` retain their original provenance and remain included.

## mt32emu (Munt)

`third_party/mt32emu/` - the Roland MT-32 emulation library from the Munt project (https://github.com/munt/munt),
copyright Dean Beeler, Jerome Fisher, Sergey V. Mikayev and contributors, GNU Lesser General Public License 2.1 or later
(`third_party/mt32emu/COPYING.LESSER.txt`). Retained from the Mech2 foundation; the normal Mercenaries build does not require it. The MT-32 ROMs are not
included: put your own MT32_CONTROL.ROM and MT32_PCM.ROM in the game folder's `mt32/` (or set `mt32=`).

## Renderer migration provenance

The SDL_GPU backend and shader generator derive from
[BatMahn/MW2-Port v1.1.0](https://github.com/BatMahn/MW2-Port/tree/v1.1.0),
commit `38a6834a1081895cf118325bd3ab1c862e7756b5`. Mercs-specific shader and
rendering behavior was ported from this project's existing implementation.
The supplied handoff and exact source/tool hashes are retained under
`docs/renderer-migration/`. Shader build tools are not redistributed in the
source archive or required by an ordinary game build.
