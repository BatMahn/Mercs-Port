# Mercenaries development history

## v0.1.6: shell side by side with DOS; Mercenary landing and first-mech assignment — 2026-11-10

The shell was walked through side by side with the DOS game in DOSBox, both careers: registration, the letter, the
DropShip, Mission Command, the Duty Roster and the Mech Lab for a Mercenary; the Office, its computer, contracts,
personnel, inventory and finances for a Commander. Everything matched except two things, now fixed:

- **A new Mercenary career skipped the DropShip landing.** MW2SHELL.EXE plays MLAND2 for both careers when transitions
  are on (0x6e038 Commander, 0x6e0ec Mercenary); the port played it for the Commander only.
- **The Mercenary pilot started with no 'Mech.** Entering Mission Command, the original gives a pilot without a 'Mech
  the unit's first one (0x3cb60 -> 0x3ca00; a Commander only when the unit owns exactly one, 0x3caa8). DOSBox opens the
  Duty Roster with the pilot "piloting COM-7X Commando"; the port showed "no mech assigned" until NEXT was pressed.

Also checked, no change needed:

- **Typing during a blocking sound cue** is dropped in DOS too (DOSBox with the SB16: MIKE typed while the terminal's
  click sound plays never reaches the call sign), as the port does.
- **Whole-mission sweep at HUD + 3 s**, cockpit and external, every fixed mission: cockpit frames within about 1 %, the
  larger gaps being the DOS start-up message bar. REC1/REC2's perched external camera shows the rocks beside it darker in
  DOS (open, see KNOWN_ISSUES).
- **TRA1 on autopilot at full throttle:** nav arrivals within about 5 s and the mission completes the same way. The
  instructor lines and the nav-point calls play within half a second of DOSBox's (SB16 capture).
- **GRA1 on autopilot:** the Commando walks the same speed and is caught by the ambush in both, dead about a minute in
  (DOS about 10 s earlier after brushing a wreck the port's mech passes clear of).

## v0.1.5: Windows run under Wine; shell artwork and movies on Vulkan; GPU fallback; GUI test — 2026-11-10

The Windows build was run for the first time (Wine 9, Mesa's software Vulkan driver, Xvfb): the shell from the intro
movie through registration, the letter, the DropShip, the Duty Roster and a launched career mission with its abort
and debriefing, and Instant Action out and back. A direct mission frame is pixel-identical to the Linux build's.
Fixed:

- **The shell would not start on SDL's Vulkan renderer** ("The Mercenaries registration or mission artwork could not
  be decoded"). Its backgrounds were 24-bit textures, a format SDL's Vulkan renderer does not have. They are 32-bit
  now. The movies had the same format: they now fall back to a 32-bit texture, filled from the same 24-bit frame,
  when the renderer has no 24-bit one.
- **A Direct3D 12 device that cannot build the shaders ended the game** at its first draw ("Could not create graphics
  pipeline state"). Every program's pipeline is now built once when the device is created; if that fails the next
  backend (Vulkan) is tried, and the game says so on the console. Seen with Wine's vkd3d; a broken Direct3D 12 driver
  on real Windows would be stepped over the same way.

The Linux GUI test (tests/run_mercs_gui.py) passes again, full and --profiles-only. SDL 3 reads input through
XInput2, which events sent with XSendEvent do not reach, so it now presses keys and the mouse through XTest; it starts
openbox when installed (SDL 3 asks the window manager for fullscreen); it waits out each page's opening clip before
clicking; and it follows the shell as it is now - the debriefing after each return, the game menu's New and Quit
(Escape no longer leaves a page or the game by itself), the returned transient MEK deleted after the return is
committed, and the Duty Roster checked through the launched mission's player line.

## v0.1.4: external camera glide; mission-opening differences explained — 2026-11-09

- **External camera on DOU1.** For its first two seconds after `c` the port's camera climbed 50 m and back, through
  the rocks; DOSBox glides straight out behind the mech. The camera's floor counted the DropShip leaving overhead (a
  moving object, its top 53 m over the start) as ground. It now takes fixed surfaces only. REC1's camera still perches
  on its rock as in DOS.
- **REC2's white shapes and MYB1's extra mech** (listed in v0.1.3) are not differences: both are timing. The earlier
  DOSBox frames were taken about 25 s into the mission, the port's at 3 s. The REC2 shapes are the DropShip's
  exhaust puffs, which the port draws too by 15-20 s; MYB1's is the allied Dragon, which walks off to its first nav
  point in both (gone from the view by 10 s in the port, as in DOSBox).

Tried and not kept (see KNOWN_ISSUES.md): walking a large textured polygon's light levels over its whole outline, and
remapping shadowed ground texel by texel; neither came closer to DOSBox.

## v0.1.3: DOS look - Gouraud cockpits, night ground, damage shading — 2026-11-09

Traced in MW2.EXE and checked against DOSBox with patched copies of the archive (see docs/DOSBOX_COMPARISON.md):

- **Cockpit panels too light** on IVA1-5, MCC3/6/7, MED1, REC1/2 and others. Mercenaries' Gouraud fill (0x495d0) draws
  a corner as its polygon's ramp + the corner's light level, counted down from the top of the ramp when the mission's
  LUMA table is a fog table (the fog palettes' ramps run from the colour to the fog). The port used MW2's rule (ramp +
  the corner's intensity scaled by the level), which made fog missions' cockpits pale. The re-captured cockpit frames
  went from 20-30 % of pixels off to under 3 % on those missions; BET and LUT are unchanged. With a fog table the
  mission's point lights are not added per vertex either (0x4f84a).
- **Night ground on BET2 and BET4.** The port showed TL1PLAIN's dim texture where DOS is black. DOS interpolates the
  vertex light levels in screen space, so on a huge ground polygon the far corners' level (0 at night, through the
  depth term) covers everything seen. The port now does the same (the corners it makes on the near plane are lit
  themselves, as 0x4cd65 / 0x4f6c8 do).
- **Alt on BET2:** the port stood the player on its 4 cm drawing lift of flat ground pieces (0.06 m); the lift is now
  left out of the collision (DOS: 0.00 m).
- **Damage shading in the DOS look:** a damaged mech location's light levels are scaled by (16 - n) / 16, or raised
  towards 15 when PLNT +52 is set (0x4689b). The port had applied this only to the 3D editions. (Traced; not yet
  captured in DOSBox.)

Shaders regenerated (glslangValidator 15.1.0, SPIRV-Cross, DXC 1.9 dev 4950: the unchanged sources reproduce the
previous src/rb_shaders.c byte for byte).

## v0.1.2: shell side-by-side with DOS; target display, Harris letter, registration keys, pointer — 2026-11-09

The shell was run side by side with the DOS game in DOSBox at 640 x 480 (both careers, from the main menu through
registration, the StarPort, the Office, the office computer and the contract pages), and the target display was
compared in a mission. Fixed:

- **Target display.** Mercenaries' DOS viewer draws the targeted unit as a solid silhouette in the display colour
  (DOSBox IVA3SCN1, the Mongoose: colour 7 fill, no edges); F4 then gives the shaded model, then off. The port drew
  MW2's black polygons with coloured outlines. It now fills them. A damaged location takes its damage colour
  (INFERRED: only an intact target was captured).
- **Colonel Harris's letter.** The Mercenary (novice) career accepts its first contract at once. The port's accept
  replaced the starting record's news, so the letter was gone. The original keeps it until the DropShip is boarded;
  the news text now survives the accept.
- **Registration keys.** In DOS, Return in the unit-name field and Tab do nothing; only ACCEPT registers. The port's
  Return registered the pilot and Tab switched fields. Both now follow DOS (Return in the pilot-name field still moves
  to the unit name).
- **Pointer recentring.** The DOS mouse driver reset puts the pointer in the middle of the screen after each movie and
  on entering the StarPort from the main menu. The port now does the same while its window has the mouse.

Checked and matching (no change): the main menu (pixel-identical apart from the pointer), its hover hints, the
registration screen, the StarPort hotspots and labels, the launch movie and DropShip, the Commander career's letter,
Office, office computer and first contract with its NEXT pages; in missions, the speed curve and top speed (COM00STD,
61 kph), the leg turn rate (90 degrees a second), heat and its dH/dT readout, and enemy starts above the ground (IVA3:
no fall damage in either).

## v0.1.1: DOSBox sweep of every mission; held enemies, spawn height, Alt readout — 2026-11-09

Every fixed mission's opening was captured in DOSBox (cockpit and external view, 640 x 480) and in the port, and the
differences were traced in the original MW2.EXE. Fixed:

- **Held enemy groups never woke.** 115 enemy star tables go from their start node straight to a hold node
  (kind 0x400, state 10, powered down). The port ran no AI rules at all for a powered-down unit, so a held group sat
  switched off for the whole mission unless the player shot it. In the original a held unit keeps its senses: a
  sighting reaches its leader, and the group assignment (DOS 0x2dc1c) picks members in any state but target/attack,
  powered-down ones included, which wakes them. DOSBox IVA3SCN1: the Mongoose 345 m away heads for the player from the
  first seconds; in the port it never moved. Held units now run their rules with movement and fire held off, and wake
  when their state changes. Shutdown (state 11) is unchanged. BET1 is unchanged too: in DOSBox and the port nothing
  attacks a player standing still there for 90 s.
- **Elevated player starts.** 12 missions put the player's start point above the ground (GRA4 40 m, IVA3 30 m, SOL5
  45 m, SPA1 and SPA2 12 m, ...). The port dropped the mech, which landed with leg damage. The original has the mech on
  the ground and undamaged from the first frame (DOSBox GRA4SCN1, IVA3SCN1: the Alt readout at the terrain height).
  The player now starts on the surface below the start point (the sim's own ground query, so a roof or bridge start
  keeps its roof or bridge). Enemies keep their authored starts; whether the original grounds them too is not yet
  checked.
- **The Alt readout.** The original prints (object +0x54 - unit +0x400) x 1000 / 65536 as "%.2f m" (DOS 0x3fa10,
  MERCSW 0x1002eded), i.e. the height in centimetres x 1000 / 65536, about 1.53 x the height in metres. DOSBox
  BUZ1SCN1 reads 2836.67 m where the mech stands 1859.04 m up. The port printed the height in metres. It now prints the
  original's number (unit +0x400 taken as 0, INFERRED: it is 0 standing on BUZ1's raised terrain).

Checked and matching (no change): the opening frames of BET1/3/5, BIG1, BUZ1, DOM1, DOW2, GRA2/3/5, LUT2/3/6, MCC2/4 within
a few levels per pixel; DOU1 (the player is killed within ~13 s standing still in both), GRA1, the BET1 automated
playthrough (won in 148 s), LUT1's start below zero (-1.42 m in both), the HUD message periods, the mission start
message, and the external camera at rest (REC1's view perched on a rock structure and DOU1's from behind are both
the original's). The DOSBox harness itself had a trap: the DOS sim writes the player's mech back to `MEK\PLY00MER.MEK` when a
mission ends, so after one death every later run starts with a wrecked mech. docs/DOSBOX_COMPARISON.md now says to
restore it before each run.

The 3dfx and Titanium effect cards (fire, smoke, jet and death cards, kind 7 with light nibble 0xf) were checked for
the black-flame bug fixed in the MW2 port: Mercs draws them as sprites, not through the lit model path, and they render
correctly.

## macOS app: lipo's argument order — 2026-11-06

- The app packaging failed on GitHub's macOS runner with "libSDL3.0.dylib lacks the executable's arm64
  architecture". The script ran `lipo -verify_arch arm64 FILE`, but real `lipo` reads every argument after
  `-verify_arch` as an architecture name, so the file came out as an unknown architecture. It now runs
  `lipo FILE -verify_arch arm64`. The packaging tests' mock `lipo` follows the real argument order, so the old form
  now fails 7 tests.

## Check programs link with libm — 2026-11-06

- `test_mercs_lance`, `test_mercs_bwdplace`, `test_mercs_gps_ai`, `test_mercs_environment` and `bwdtool` link
  `bwdplace.c`, which calls `sqrtl`, without `-lm`. Newer compilers inline it, so they linked anyway, but Ubuntu
  22.04 (the Linux CI) does not. All the check programs now link even with `-fno-builtin`.

## macOS packaging with the system awk — 2026-11-06

- `scripts/macos_mercs_app.sh` failed on macOS with "nonterminated character class". macOS's BWK awk ends a
  `/.../` regex at the `/` inside `[^/]`. The two checks that used it now match against string regexes. With BWK awk
  (`original-awk`) the packaging tests went from 11 failures to all passing.

## Game icon — 2026-11-06

- The Mercenaries icon is used everywhere:
  - **macOS app:** `packaging/macos/mercsport.icns`. The app had no icon before.
  - **Windows:** `packaging/windows/mercsport.ico`, built from the icon's 16-256 px frames, replaces the
    MechWarrior 2 icon in `mercs.exe`.
  - **Running window:** a 64 px copy in `src/mercsicon.h`, set with `SDL_SetWindowIcon` on the shell and mission
    windows. This is what Linux title bars and taskbars show.
  - **Fedora RPM:** a desktop entry plus a 256 px icon, so it appears in the application menu.
  - **Linux tarball:** carries both files.
- The MechWarrior 2 `.ico` and its 32 px source are removed. `packaging/windows/mw2port.rc` is renamed
  `mercsport.rc`.

## GitHub builds — 2026-11-06

- `.github/workflows/build.yml` replaces the release-only workflow. It builds the macOS app, the Linux tarball, the
  Fedora RPM and the Windows zip on every push to main, on pull requests and on demand. A `v*` tag also publishes
  them as a release.
- The Linux job compiles every check program (`make test-bins`, new). The macOS job runs `make check-packaging`.
- The RPM spec is renamed to `packaging/mercsport.spec`. It was missing `README.md`, `FOLDER_SETUP.md` and
  `THIRD_PARTY.md`: it listed them by their source paths, so rpmbuild stopped. They are now installed to
  `/usr/share/doc/mercsport`. Debuginfo is off, since the spec builds from the checked-out tree.
- The Windows cross-build was checked locally: `mercs.exe` links against SDL 3.4.18 (MinGW) and imports only
  `SDL3.dll` and system DLLs.

## DOS lighting traced: per-vertex light, mission point lights — 2026-11-06

All of this is traced in MW2.EXE and checked against DOSBox frames of BET1SCN1, LUT1SCN1 and IVA1SCN1 at 1024 x 768.

- **Per-vertex light.** Textured polygons are lit per vertex, and the level is interpolated across the polygon.
  - At load, DOS builds a normal for each vertex (0x46168). It goes through the object's polygons from the last,
    adds each face normal to its corners, and renormalises after every addition.
  - Each frame, every vertex is lit from that normal (0x4f6c8).
  - The textured span setup reads each vertex's level (0x49f20).
  - The port lit every polygon flat, which gave the mechs hard-edged, darker faces.
- **The x/z swap.** The per-vertex call (0x4f79f) passes the normal in the order z, y, x. As a result, a vertex
  sees the light's x against its normal's z, and z against x. The polygon path (0x4d20e) doesn't do this. The port
  reproduces the swap, which brings back the window grid on BET1's buildings.
- **Gouraud panels.** The cockpit's colour-ramp polygons take each corner at its own vertex level instead of the
  polygon's.
- **Mission point lights (MLIT chunks).** The port now reads these: position and box half-size (0x62ba9 → 0x4f4fc).
  - They are used only while Multiple Lights is on.
  - Each light whose box holds the polygon or vertex adds 128 × cosine, with no intensity scaling, until the
    light reaches 0x80.
  - BET1, BET2, MCC1, SIG3, SIG4 and SOL3 have one. BET1's sits 10 m up beside the player's start.
- **Unit shadows.** These now multiply each colour channel by the LUMA row's own darkening, so they take DOS's
  greenish tint.
- **Difference from DOSBox** (mean absolute difference per channel, 1024 x 768, before → after):

  | Mission | Cockpit | External |
  |---|---|---|
  | BET1 | 10.5 → 9.9 | 9.2 → 8.2 |
  | LUT1 | 7.6 → 6.9 | 7.8 → 7.5 |
  | IVA1 | 23.0 → 29.5 | 7.5 → 7.5 |

  IVA1's cockpit gets worse: its panels show the DOS band pattern, but with the brightness inverted (see
  KNOWN_ISSUES).

## DOSBox comparisons: unit shadows, radar nav blips — 2026-11-06

- Units now cast shadows in the DOS look when Shadows is on. As in MW2.EXE 0x509e8 / 0x507b4, each polygon
  whose normal does not point down is dropped straight to the ground under the unit (5 cm above it, unless the
  ground is at 0), and the pixels under it go through the LUMA row named by SKYGND.PAR `shadowtable`. The port
  draws that row as a multiply by its average darkening of the ground texture, so overlapping polygons darken
  again, as in DOS. Matches DOSBox BET1SCN1 in shape and position. The shade is an approximation: DOS's shadow
  is slightly greener.
- Radar nav blips now use the original sprite table (MW2.EXE 0xc9d14; MERCSW.DLL has the same one). Every nav is
  the 5 x 5 diamond (RTGTNP, RRTGTNP once reached). The selected nav gets RNP / RRNP drawn on top, after the
  units (0x12038, 0x11da0). The port had the two rows swapped. This now matches DOSBox pixel for pixel, before
  and after N.
- Removed the "ramp-3 texels take the polygon's ramp" plate rule from the first comparison. LUT1SCN1 showed it
  was wrong: DOS's grey panel lines came out yellow.
- Lighting is still darker than DOS. See docs/DOSBOX_COMPARISON.md section 4 for what has been ruled out.

## First DOSBox comparisons: camouflage by star — 2026-11-06

The original DOS sim now runs headless in DOSBox straight into a mission, at 320 x 200, 640 x 480 and 1024 x 768
(`scripts/dosbox/`, docs/DOSBOX_COMPARISON.md). Compared with the port on BET1SCN1:

- Units now wear their star's camouflage. The STAR value picks a set through the mission's CAMO chunks
  (MW2.EXE 0x62bf6 / 0x62e74), which remaps the plate slots; the insignia is slot 0x3f plus the star value. Every
  unit had been drawn in plate set 0, and the actors' star value never reached the renderer. This applies to the DOS
  and 3dfx looks; the player's mech gets its star too.
- On textured polygons whose colour ramp is 8-15 (the mechs' plates), the ramp-3 texels are drawn in the polygon's
  ramp, so the 'b'/'c' plates come out brown as in DOS.
- New test: test_mercs_camo. The check runner now skips, and names, the three tests that need the private
  captured-palette fixtures, instead of stopping on them.
- Open, from the same comparison: lighting levels (mechs, cockpit, buildings darker than DOS), mech shadows,
  and the radar blip's shape (docs/DOSBOX_COMPARISON.md section 4).

## SDL_GPU renderer migration — 2026-11-03

Replaced the active OpenGL backend with SDL3 GPU: Metal on macOS, Vulkan on
Linux, and selectable D3D12/Vulkan on Windows. Kept the Mercs-specific world,
cockpit, palette, particle, terrain and HUD rules behind `rb.h`. Converted
loading screens, screenshots, MFDs and low-resolution targets. Included
SPIR-V, MSL and signed DXIL shaders in source, added GPU settings to Combat
Variables, and updated Linux/Windows/macOS packaging. See
the migration report for exact validation
and remaining native-platform checks. No campaign progression is claimed by
this rendering change.

## Solaris 3 shot-path investigation — 2026-11-03

The reviewed medium-group and local terrain-flank input policy passes 118
controls and retains exact historical/prefix comparisons. The second SOL3
attempt still loses naturally. A third ordinary attempt uses the same setup
and input policy with three pre-existing read-only logging switches; all 4,989
inputs and all observed-state lines match the second attempt byte for byte.

The trace identifies 32 admitted medium-laser emissions terminating through
the native height fallback near the player. Static current/led eye-ray checks
explain only one of the eight suspect trigger groups. The original projectile
terrain consumer is being compared before any production change. All three
losses, normal restoration, and the 59-attempt/404-operation boundary remain
recorded; the career still has 18 unique wins.

The Sigurd lookahead corrects an older planning sentence about the protected
FRR Drop Ship: its GT AFFL 6 uses the established STAR first-word map and is
friendly alliance 0. The engine already uses the correct map.

## Dedicated particle colors and Solaris 3 preparation — 2026-11-03

Dedicated Mercs16 palette particles now use the original RGB6-times-four
expansion in a local submission buffer. The old index-3 output `(255,195,0)`
becomes the captured original `(252,192,0)`. Scene, HUD and other texture
palettes retain their ownership. DOS, Titanium, legacy texture modes and the
missing-primary-PALG fallback preserve their previous paths.

The same actual-loader/shot/GL fixture passes 968 checks versus 16 failures
on the exact old source. It covers three scene loads, ten framebuffer readback
cases and a real native class-5 launch. ASan/UBSan passes with leak detection
disabled; the inherited O1 formatting warning is retained separately. The
coordinated 18-case build passes with zero warnings or errors. Original hardware
pixels, general palette-phase policy and whole-frame equivalence remain open.

Normal SOL3 repair, purchases and accepted edits prepared the 78.50-ton
AC/10/four-medium-laser Stalker with 100 finite AC/10 rounds. The first attempt
destroyed two opponents before a natural saved loss; all receipts, damage,
ammunition use and return results remain recorded. The career has 18 unique
wins and is still being continued through ordinary controls.

## Cached HUD sprite palette and bounded particle proof — 2026-11-03

Dedicated 3dfx and Titanium now refresh cached SHP artwork from the submitted
shared palette. A new deferred entry marks affected sprites dirty and rebuilds
them at their next draw; existing eager callers retain their behavior. The
existing mapping, Scale3x, edge bleed, filtering, transparency and texture IDs
are preserved. A delayed legacy slot cannot overwrite the current bound palette.

Eighteen original draws of RETICLE6 supply the color/clip reference. The exact
preceding native source fails 64 of 387 checks; the deferred implementation
passes 485 checks and ASan/UBSan. Its 68 framebuffer render/readback cases cover
all seven sprite routes and scoped scratch-allocation retry. The coordinated
build passes 15 cases with zero diagnostics. Two fixed WOL1 HUD measurements
reduce uploads from 581,301,540 to 144,396 bytes over 120 frames, with identical
final images. These single-view software-GL timings are not platform benchmarks.

Five original dedicated particle draws separately establish that the tested
packed-HUD callback leaves their float RGB table unchanged. No world/effect
palette propagation patch is inferred. SOL2's paid energy preparation and
three natural losses are retained. A normal 29,375-C-bill armor upgrade and
ordinary input focus policy preceded its fourth-attempt success: native and
persisted outcome 2, final 144.73529 seconds and 555 health. The career now has
18 unique wins; its ending remains unvalidated by ordinary play.

## Accelerated HUD palette and Solaris 1 victory — 2026-11-03

Dedicated 3dfx and Titanium now feed their per-draw HUD colors from the shared
palette controller. Exact original executions establish the RGB6 transition,
selected RGB565/555 callback and software glyph consumption. The native route
keeps the font atlases and cached sprite artwork intact, shares admission with
damage/light/vision requests, and preserves accelerated begin-before-store
base changes separately from DOS. The bounded native gate passes 21,382 checks,
400 captured submissions and eight real-font GPU draws. ASan/UBSan and the
fourteen-case coordinated build pass. World texture changes and original
monitor pixels remain separate; canonical admission can change the timing of
the retained world tint carrier. See
`docs/research/mission-order-dispatch/nuclear-palette/accelerated-native/`.

Solaris 1 won on its third ordinary-input attempt, after two retained natural
losses. The first exposed a critical head-location loss; the second exposed a
test-pilot turn that exhausted torso aiming margin. A read-only historical
decision replay identified the withheld firing inputs. A narrower ordinary
turn, with the same paid loadout and unchanged production binary, completed
all three opponents. The saved success retains 491 health and is the
seventeenth distinct mission win. The successful debrief boundary has 52
attempts and 334 normal shell operations; the complete career remains open.

## Mission arrival coordinates and GRA3 ordinary victory — 2026-11-03

Arrival predicates now use the original current body/object centres, target
flag rejection and scenery-origin fallback. Existing radar/aim/guidance
eligibility remains intact. Sixty original predicates and sixty original
ready-node evaluators supply the bounded expected results. The native gate
passes 346 checks; exact old source fails 95. Independent byte/source review,
ASan/UBSan and the 17-case coordinated integration pass. Fixed-integer overflow,
missing native nodes and two artificial flag-zero visit controls remain
explicit limits. See `docs/research/arrival-coordinates/`.

The read-only campaign observer can now report exact eye-to-target world hits
without mutating the live query cache. Its 709 checks and byte comparisons
identified terrain obstruction missed by the prior box-only visibility test.
The next GRA3 ordinary-input attempt won on the preceding production binary,
with all three defenders destroyed and both allies alive. The checkpoint now
has sixteen distinct saved wins, 49 attempts and 292 normal shell operations.
The complete career ending remains open.

## DOS shared palette and prelaunch ammunition protection — 2026-11-02

The DOS frontend now uses one original-backed palette controller for nuclear,
damage, light, vision and time-of-day transitions. It preserves shared busy
admission, current-buffer interruption and the original frame-count return
quirk. Cached DOS effect cards keep indices and resolve the current palette in
the fragment shader, including the gray ramp and transparent key. Procedural
RGBA cards and non-DOS paths retain their existing behavior.

The controller matches 337 captured submissions in 9,222 checks. GPU and actual
frontend gates pass 84 and 165 checks, with retained preceding-source failures;
the focused coordinated build passes 18 cases with zero compiler diagnostics.
Original monitor colours and dedicated accelerated/Titanium nuclear palettes
are separate limits. See `docs/research/mission-order-dispatch/nuclear-palette/`.

An ordinary fourteen-bin GRA1 design exposed a late mission-return rejection.
Both original loaders overflow the shared pool even though the DOS editor
accepts the layout. The native shell now rejects more than ten bins of a type
before any owned-lance deployment writes. The guard preserves Mech Lab editing,
inventory and unused owned files in authored stock-player scenes. Its fixture
passes 252 checks across three archives; exact old source fails 90. See
`docs/research/ammo-pool-capacity/`.

The ordinary saved career has reached fifteen unique wins through GRA2. GRA1's
seventh attempt returned successfully with a ten-bin gun/laser design; its prior
losses and unsaved sixth native success remain recorded. GRA2 won first try
through normal Catapult inspection and the required pass targets. Original
repair executions reproduce its duplicate weapon rows and stored-mass quirk,
so no repair behavior was altered. The complete career ending remains open.

## Independent original nuclear damage wave — 2026-11-02

The nuclear-order producer now arms a separate expanding damage wave before
requesting its ordinary type-22 visual. It grows by 200 centimetres per raw
tick to a 100-metre cap, applies the recovered 4,096-Q16 damage coefficient with
zero unit heat, and retains the original full final frame delta. Allocation
refusal leaves this wave active; a second producer replaces it without deleting
already accepted ordinary effects. The wave advances before leader dispatch,
starting on the step after a nuclear command.

The bounded proof executes 48 original DOS/Windows operations. The native gate
passes 2,776 checks, including 986 retained dispatch checks; all 20 full-step
missing-wave controls fail against the exact preceding source. Address and
undefined-behavior sanitizer checks and the 16-case coordinated integration
gate pass. The dedicated palette flash, complete original-frame equivalence
and actual mission casualties remain separate from this proof. See
`docs/research/mission-order-dispatch/nuclear-wave/`.

## Original mission order and type-22 nuclear dispatch — 2026-11-02

The mission interpreter now evaluates each table once in ascending node order,
aggregates its result and publishes its next order before advancing to the next
table. A newly enabled earlier node can remain current until the following
update, as it does in the original DOS and Windows executables. Later-table
controls retain earlier tables' published orders until those tables are visited.

The leader consumes that published order before the next mission-table pass.
An admitted kind-0x1000 order requests the existing type-22 effect at the chosen
member's body point with source -2. It runs once per native frame, respects
rejected or pushed state transitions, and retains effect-pool refusal behavior.
The final fixture passes 986 checks; the preceding production source fails 223,
and the first candidate without the admission guard fails 20. The focused
coordinated build passes 15 integration cases with zero compiler diagnostics.

This checkpoint covers the scheduler and type-22 channel. A subsequent review
identified a separate original singleton expanding-damage wave and palette
state; their native consumers remain under investigation. The original whole
main loop and synchronized nuclear presentation are outside this bounded proof.
See `docs/research/mission-order-dispatch/` for the original executions,
negative controls, independent byte/source reviews and exact scope.

## Ordinary SPA2/SPA3 wins and original resource-return behavior — 2026-11-02

The saved career has now won thirteen unique missions through SPA3. SPA2's
two long harpoons were destroyed with ordinary laser fire at real near mesh
surfaces from supported ice. The first attempt then lost to the close Crab;
the retained retry defended against it before crossing to the eastern engines
and won with all protected structures alive. SPA3 won on its first attempt,
using the lower cave and three real combatants instead of the off-map latch.
Normal Continue, contract acceptance and paid repair have reached GRA1.

The Stalker's depleted-bin and 20-to-10 heat-sink return behavior was checked
against original code before considering a port correction. Both original
engines and shell cycles reproduce the exact-ID sink rule, and five complete
native return byte streams match. This adds evidence and input-driver
improvements; it requires no gameplay-engine change. The source retains every
actual loss and distinguishes declared geometry/policy probes from mission wins.

## Class-2 aircraft altitude and target selection — 2026-11-02

The supported SPA1 attempt exposed class-2 fighters omitted by the earlier
class-5/8 aircraft check. Their authored MGEO centre is 213000 cm, but the
simulation retained a 550-cm fallback; the accelerated pose also omitted the
centre. The original DOS and Windows loaders, root publication and movement
publication establish the correct absolute body altitude. Class-2 actors now
retain that centre in both visual profiles and publish their original altitude
after ground/vertical processing. Successful attached wreck movement shares
the correction; grounded and detached early exits stay distinct.

Human unit and thing selectors now use the shared current-body delta. The
original flags, alliance/shutdown gates, range bypass and inclusive 1750-metre
maximum remain. Original ordinary-flag fixtures distinguish the correct nearby
fighter from the historical bad coordinate. The expanded native gate passes
7602 checks across 71 class-2 resource records and both hip-motion modes; the
same fixture reports 4255 failures against the exact prior sources.

The coordinated consumer gate passes 19 cases. Its initial three ground-test
aborts exposed the old synthetic assumption that class-2 actors should be
raised onto terrain. That test now requires their authored baseline to stay
unchanged at initial world setup, matching the recovered class rule. Failed
fixtures, the initial report-collection error, and corrected results are
retained. Whole original flight physics and subsequent wreck contact-state
equivalence remain explicit limits. See `docs/research/aircraft-coordinates/`.

The subsequent ordinary-rule SPA1 attempt won, with matching native and saved
outcomes. All six aircraft were destroyed and the command centre and both
engines survived. This advances the saved career to eleven unique wins and
SPA2; the two earlier SPA1 losses and all input/source hashes remain recorded.
A fresh complete-source extraction builds the identical production executable
without diagnostics, with all 171 production inputs matched.

## Gameplay audio, controller pose and mission texture loading — 2026-11-02

Audited remaining gameplay sound producers against Mercenaries instead of
inherited Mech2 record numbers. Corrected individual cues and their original
online, jet-port, fuel, warning and guided-fire gates. Restored the separate
effect/ambience allocation budgets and original positional attenuation, range,
propagation and fixed-point stereo panning. Legitimate speech remains intact.

Restored authored THNG torso binding and controller pitch/convergence throughout
cockpit, model, eye, mounts and collision representations. The cockpit camera
and projected reticle follow their separate original pitch rules. Exact matrix
angle extraction also preserves the arm ray during near-vertical falls. Expanded
coverage includes every supplied cockpit model and short movement runs through
67 authored scenes per edition. GRA5 spawn recovery now chooses a surface that
runtime contact can support, and aircraft use one shared authored flight centre.

Implemented shared texture-slot animation clocks and the complete 32-object
muzzle pool. Scene and effect bindings now replay the original expanded BWD
chunk order, including repeated includes and later rate/mode changes. This fixes
actual scenery/effect slot selection in 14 DOS and 20 scenes in each accelerated
archive. Original numeric proximity and clock rules are retained.

Removed a campaign-return rejection for valid authored objective descriptions
longer than 31 characters. The result writer reproduces the original ordered
wire writes inside a bounded staged payload, retaining atomic failure for
invalid or overflowing input. An automated normal-rule input driver now records
real mission outcomes and production campaign persistence; it never injects a
win, damage, health, ammunition, funds or objective completion. See the current
follow-up report for the achieved playthrough scope and remaining limitations.

Restored authored stock-player mission launches for MCC4's Stalker, SIG2's
Pegasus and OUT3's Kodiak. The normal owned roster is still staged and validated;
unused deployment MEKs return unchanged through the original selective import.
The native envelope carries complete deployment trailers. SIG2 Situation text
now follows the original scene-derived name independently of its briefing movie.
Nine real child launches across the three editions and ordinary-launch return
regressions pass; these controlled checks do not count as campaign wins.

An actual MCC3 attempt exposed valid ammunition links remaining after the owned
Dragon's AC/5 was destroyed. Loaders now construct independent ammunition pools
before matching mounts, preserving orphan bins and original link order. Normal
fire, AMS, jettison and strict return capture share that state. Executed DOS and
Windows cases agree on the repaired path, including the exact private damaged
deployment; bounded-input and sanitizer checks cover the changed representation.

Corrected fractional terrain-quadtree bounds that could omit valid integer
ground positions. The index now uses the original rounded vertex domain and
wrapped-add/arithmetic-shift midpoint rule. Executed DOS and Windows trees,
ground queries and arithmetic edge cases provide the reference. A separate
read-only terrain observer assists the input-only campaign pilot and is compiled
only into its validation target; it never advances or edits mission state.

Separated the player's STAR ordinal alliance from the scenery affiliation map.
Repeated affiliation numbers had assigned the player the wrong side in eight
audited scenes per edition. Original DOS/Windows startup and getter executions,
a failing old-source control, and three-archive native checks establish the
correction for career side statistics and the alliance used by missile defense.
The scenery mapping and general AI/damage paths retain their existing behavior.

Corrected missile acquisition coordinates and the maximum-range boundary.
Player and AI locks share current controller body positions, with the original
AI behavior-3 foot-target exception and its Easy-difficulty normalization.
Executed original target producers and lock gates distinguish the correction
from the former render-height approximation. The old source fails 33 of 81
bounded lock comparisons; the corrected consumer passes those checks and 243
coordinate-component checks across three archive geometry inputs under the
retail LRM10 profile. Pre-fire visibility and projectile emission remain
separate consumers.

Corrected the ordinary AI pre-fire visibility query to use its published eye
pose, horizontal target range and nearest world/unit/debris collision. Selected
direct unit/thing hits permit firing; other obstructions hold it. Command
targets take precedence over stale node targets. The full native firing gate
checks decisions, following-frame emission and selection RNG order; the old
consumer fails 18 assertions across nine cases, while the staged correction
passed 49 including diagnostic classifications. The subsequent original Windows
startup ownership study supports two additional parent-before-child fixtures,
bringing the native gate to 53 checks per archive geometry input. The current
direct mapping agrees for those cases; later parent recreation and cross-list
ties remain explicit limits.

Recovered DOS364 versus retail Windows362 acquisition/decay duration through
1,556 original lock calls. The native duration getter preserves the existing
profile selection, partial progress and AI forced-lock branch. The native
sequence gate compares 1,556 flags and 1,556 timer values; its small reported
float conversion tolerance does not normalize or alter simulation timing.

An actual SPA1 launch exposed a large ice face with a sideways collision normal,
causing the stationary player to fall through the deck and naturally lose.
The normal executable independently reproduced the fall without the test
observer. Both original Mercs executables instead return an upward Q29 normal
using signed-64-bit cross arithmetic and a highest-bit candidate metric.
The collision constructor now shares that recovered routine with the cockpit,
preserving the original approximate magnitude. The old collision source fails
53 of 154 focused assertions; the correction passes all 154 across the three
archive geometry inputs and the associated consumer regressions. The normal
executable now lands and remains on the actual authored deck in the same
six-second no-input comparison. No mission-specific floor or spawn relocation
is added.

Evidence, reproduction commands and current validation are indexed in
`docs/GAMEPLAY_FIDELITY_FOLLOWUP.md`. MercNet remains excluded and broader native
platform certification remains tabled as requested.

## SDL3 turning audio and Combat Variables graphics — 2026-11-02

Replaced the inherited Mech2 torso-loop number316 (Mercs STAR3A1 speech) with
Mercs TORSLOOP341 and restored the original cockpit/player/two-degree torso
error gate. Leg heading changes no longer start the servo loop. The mission
menu now uses Mercs MECGCLX1/MECMENU1 records237/245, preserving ordinary
lancemate acknowledgement dispatch.

Added an original-art Graphics page to shell Combat Variables, with DOS,
Titanium Direct3D and dedicated3dfx choices, Original/Nearest/Smooth texture
filtering, native object/terrain switches, DOS shading and HUD layout.
Settings survive reopening and renderer changes while retaining active career
state and unknown native configuration bytes. The mission Combat Variables
menu also reaches Graphics; renderer changes are saved for the next launch,
and the shell handles the old result before adopting the requested profile.

DOS Smooth filtering resolves each of four samples through its LUMA/palette
before blending colors. Original filtering stays unchanged. DOS object and
terrain texture switches now use the authored palette fallback for their own
object classes instead of leaving those textured faces unaffected.

See `docs/AUDIO_GRAPHICS_FOLLOWUP.md` for original-code evidence, regression
coverage and platform limits. The source remains native SDL3.

## Native SDL3 migration — 2026-11-02

Ported the active shell, FMV player, mission window, input, mixer and test
harnesses to native SDL3. Replaced SDL2 device callbacks with owned SDL3 audio
streams while retaining original decoder and mixer arithmetic. Corrected bool
return handling, borrowed base-path ownership, per-window text/relative input,
float pointer coordinates, joystick ID enumeration and 64-bit animation clocks.
Movie restoration now preserves renderer state independently for each target;
physical-window input mapping avoids using a selected canvas as the output size.

The Makefile now discovers SDL3 through `pkg-config sdl3`; `make app` bundles
the linked SDL3 dylib or framework. Added nine packaging regression cases and
updated current build instructions. Existing three game looks, original UI,
DOS soundtrack and save formats are retained. Selected original/CRT movie frames
and three-profile cockpit captures match the preceding SDL2 port exactly.
See `docs/SDL3_MIGRATION.md` for full validation scope and remaining platform limits.

## Single-player completion and integrated regression pass — 2026-11-02

Restored the remaining original finance, inventory/trading, hiring, Mech Ops,
Mech Lab, roster/weapon-group and Instant Action screens. Procedural RAN/RND
contracts now use the original generator, authored templates and corrected
shared BWD transforms. Player plus two owned lancemates and optional AeroTech
support deploy through identity-bound launch/return records. Original DOS and
retail Windows saves can be imported/exported with their complete row layout
and unknown bytes preserved. The owned-fleet bound is the existing codec's 4,096
rather than an artificial 32-mech runtime view.

Cockpit Controls now uses the original devices/editor, four saved pages,
custom bindings and novice/expert setting. All fourteen MFD modes work in each
of seven windows. Original satellite class/terrain colors, map marks, zoom
permissions, HUD save/reset, weapon groups 4/5, alpha, fire-cycle and starmate
orders have runtime consumers. Expert +/-60-degree pitch and novice steering,
speed and auto-aim follow executed Mercs arithmetic.

Replaced first-record-only world rendering with original distance-dependent
WTBO selection. Implemented DOS textured ground and each accelerated edition's
own topology/depth bands/fog; preserved the dedicated H(0) factor and Titanium
interpolation. Corrected special weapons, original ammo-critical/cook-off
behavior, location branches, effect attachment, non-unit damage, debris and
scene-light lifetime. Hardware-edition numeric weapon tables now have their
own audited values instead of assuming identical tables.

Severed-leg falling and collision-triggered wreck breakup now use recovered
original state and contact rules. The body, cockpit, aiming direction, camera,
sky and horizon share the complete fall rotation. Auxiliary MFD cameras no
longer replace the main HUD projection cache. Final visual review also traced
a tan stripe across accelerated skies to an elevated road with an unassigned
bitmap: native material dispatch now follows the original missing-bitmap skip,
with deliberate texture-off shading and wire/map behavior preserved.

Earned Merc Lords scores now submit at the original bankruptcy, ending and
disbanding boundaries, with original qualification/tie sorting and a native
retry receipt that prevents duplicates. Native transactional save policy keeps
failed writes/imports from losing the current career or staged customization.

Final integration exposed a clock-dependent procedural offer change during
native resume. Saves now preserve every preview's seed and generation year;
original inactive offer exports also retain their history seeds. Accepted
mission identity, valid zero seeds and older companion decoding are preserved.
A deterministic clock test retains full-state equality and checks legacy
accepted mission resources. The final test fixtures also isolate both game
and install write roots so synthetic saves cannot reach supplied game data.

This continuation retains the reported cockpit, buried-spawn, walking/radio,
startup, shell-centering, sound and original-menu fixes below. Current checks,
original probe counts and exact remaining limits are indexed in
`docs/research/MERCS_COMPLETION_HANDOFF.md`. MercNet remains unimplemented;
native Mac/Windows and whole-campaign execution are not implied by the Linux
checks. Earlier entries below describe their own historical checkpoints.

## Combat Variables and gameplay regressions — follow-up revision

Restored the original Combat Variables screen, thirteen value controls and
three volume sliders. Actual DOS widget/font execution supplies the layout,
click boundaries, slider preview thresholds and Escape/right-click save rules.
Full 84-byte sound and 23-byte difficulty records preserve unknown fields and
extensions; the mission Audio menu no longer truncates the sound record.
Mission settings, room labels, transition movies and audio gain consumers are
connected. NATIVE render size remains an explicit port extension.
Restored the original DOS and dedicated 3dfx projectile palette particles,
including their different point counts and lifetimes, so PARTICLES controls
real effects in all three profiles. Titanium keeps its separate PID smoke.
Visual random state remains independent of combat; other particle producers
remain documented unfinished work.

Cold startup now returns to the original main menu after MINTRO instead of
loading the previous working checkpoint and its failed debrief. Explicit
Save/Load and normal mission-child returns retain their original routes.
Eighteen checkpoint/intro combinations preserve every saved byte. Continuing
an explicitly loaded failed BET1 report also exposed the original truncated
BET4 equipment suffix: an exact-identity native compatibility rule keeps its
33 complete authored items, refuses altered data and never invents the
fifteen undefined original rows.

Closed reproduced DOS Commando panel seams by synchronizing integer span and
raster projection at shared vertices, including genuinely near-clipped faces.
Accelerated cockpit and world geometry retain separate near clips but now
share a comparable depth-buffer mapping. Original meshes, EYEO and genuine
open panes remain unchanged. Controlled GPU checks and matched rest/stride/
twist views isolate the renderer correction.

Settled buried mobile ground units once at world installation. The reproduced
Thug started outside the original ground selector's vertical search window;
ordinary movement keeps that selector. Roofs, elevated starts, aircraft and
fixed objects are excluded from the native startup adaptation.

Replaced inherited movement sound IDs with executed Mercs mappings. Nearby
footfalls no longer use a ricochet, and gait changes no longer select speech
resources. Correct player footsteps and authored radio messages remain.
PCM instructor speech also keeps its independent Voice gain when Effects is
muted; the previous cross-division could produce a non-finite mixer value.

See `docs/research/MERCS_GAMEPLAY_FOLLOWUP.md` for the exact behavior, original
evidence, regression checks and remaining platform/gameplay limits. Earlier
entries describe their own checkpoints; their open-item statements may be
superseded by this revision.

## Accelerated terrain fog and backdrop ownership — follow-up revision

Fixed the black foreground wedges in accelerated TRA1. The affected terrain
face is valid and textured: the fault was fog evaluated before clipping and
then transported incorrectly across clipped triangles. The accelerated path
now clips at the near plane, computes the original visibility byte at the new
vertices and transports it explicitly for screen-linear interpolation.

The dedicated 3dfx world path applies the original per-vertex height factor as
well as the camera factor. Titanium retains its camera-only world factor and
uses Mercs' actual SKYGND Z multiplier of two, replacing inherited MW2 scaling.

Matched original `tl1skyd`/`tl1star` registration and unlink behavior: a usable
camera-relative backdrop owns those objects, so they are no longer also drawn
in the ordinary world pass. Discovery covers SKYDOM1 and SKYDOMF as well as
SKYDOME, including the seven previously missed authored scenes per archive.

Alternative WTBO detail levels remain separate. No terrain face was hidden,
no replacement texture was synthesized, and the cockpit shape is unchanged.
See `docs/research/MERCS_WORLD_RENDERING_FIXES.md` for original instruction
probes, actual GPU regressions, full-frame checks and remaining limits.

## Cockpit geometry, clipping, lighting and compass — 2026-11-02

Restored representation-specific OBJ classes and the selected head-subtree
cockpit pass. Ordinary body pieces retain world depth; head descendants use
the original 8 cm cockpit near plane. Dynamic visible-part/object storage removes
the inherited 16-part cutoff. The shader's span clipping and physical near
cutoff now agree with that cockpit plane.

Reconstructed the original polygon-plane normal selection and strict facing
test. Kept the original EYEO attachment and approximately 73.74-degree vertical
framing; authored cockpit openings and strut shapes are preserved.

Corrected DOS palette lighting: ambient plus positive light contribution,
mission-specific SKYGND global intensity, and raw quarter-centimetre fog before
material scaling. The compass tape now moves in the original Mercs direction;
DOS and Windows use the same checked whole-degree/Q16 arithmetic, without the
inherited accelerated one-pixel bias.

See `docs/research/MERCS_COCKPIT_FIXES.md` for original-code probes, real-resource
checks, actual shader fixtures, three-profile rendered comparisons and limits.
An all-WTBO upload experiment was rejected after original DOS loading and draw
selection confirmed that flagged records are alternative detail levels. The
renderer retains its first-record policy; the 512-entry cockpit order and
dynamic part storage remain. Accelerated foreground and backdrop defects,
including the existing world/terrain wedge, remain separate unfinished work.

## Original shell presentation, audio and menus — 2026-11-01

Corrected shell scaling and input mapping after the user's Mac screenshots
showed displaced/cropped rooms. Original artwork now composes into one fixed
640x480 target and is copied once to a centered drawable rectangle. Resizing,
fullscreen changes and movie returns rebuild the physical fit; input uses the
same rectangle and rejects letterbox bars. This is a verified Linux correction,
not a claim of native Mac execution or a fix for the separate mission cockpit.

Restored original DATABASE WAV speech, room loops, doors, menu/repair feedback,
spoken boarding refusals, idle samples and line-reveal audio. Decoding retains
8/16-bit PCM meaning and mono/stereo channels through the existing mixer;
embedded room movies also retain signed16 stereo. The DOS oracle covers 122
named executions and all 73 WAVs have independent PCM goldens. Native UI poll
cadence and its separate idle RNG seed remain explicit timing policies.

Replaced steady-state in-game OS alerts with the original dialog sprites,
fonts, button defaults and input rules. Added the original Escape/right-click
menu and wired the main Options button to it. Merc Lords and The First Circuit
use original assets and checked external DOS data. Combat Variables and Cockpit
Controls remain explicitly unfinished. F10 retains the separate port video
and three-edition settings; the native game-folder chooser remains available.

Corrected menu-backed confirmations, Save/New/Quit pending actions, hidden
Save/Load buttons, on-disk slot checks, Save return navigation and the original
description font. Repair departure now retains the correct parent context;
ordinary Save/EXIT recreates a fresh repair view, while loading resumes the
saved parent. Escape opens the original menu instead of discarding repairs.

See `docs/research/MERCS_SHELL_PRESENTATION_FIXES.md` for reproducible checks,
current evidence and remaining limits. Earlier entries below record their own
checkpoint behavior, including shortcuts superseded by this revision.

## Original debrief, careers and repair — 2026-11-01

Connected validated mission returns to the original debrief/aftermath screens,
MEJECT/MDEATH branches, compensation/expenses, automatic authored-candidate
salvage, guarded losses and campaign successors. Original bankruptcy, no-mech
and final rich/poor gates run before settlement. Type1 training chains retain
owned mechs; terminal training completion performs original cleanup and hall
boarding transfers the next authored roster.

Added original DOS save encoding/decoding, fourteen owned/shop/candidate lists,
complete authored initialization and persistent native continuation. Generation
writes preserve the previous save until commit. The working career recovers
pending returns without a second salvage roll or payment. Save/Load uses the
original ten slots, art, editable descriptions and controls; F5 is a native
shortcut. Switching graphics rehydrates resources while preserving the complete
progressed career. Names in current/reference arrays are paired by identity.

Restored the original Repair/Reload screen and transactions: armor/internals,
critical costs, real owned replacements, reservation/rollback, exact-funds
acceptance and the distinct strict Repair All gate. Original body sprites,
condition states, relative text geometry and inventory-dependent colors are used.

Corrected CAR startup/kill/star/alliance producers against both retail engines
and routed native once-only loss events into them. Extended authored CPGN loading
to follow original signed negative ammo counts and optional absent pilot/tech
resources, rather than rejecting valid later contracts. Original debrief glyph
and page-layout checks caught resource-index and authored-section append errors.

Evidence includes original execution/byte comparisons, three-archive controlled
career tests, actual frontend handlers with original-art captures, read-error
recovery, repair/cancel and edition preservation, plus the earlier engine and
mission-return regression. This is a complete source handoff, without game assets
or compiled builds. Full campaign gameplay and native Mac/Windows execution remain
open. See `docs/research/MERCS_CAMPAIGN_INTEGRATION.md`.

## Original mission-return integration — 2026-11-01

Connected simulator exit to complete Mercs CAR214/MSN2516 output, a launch-bound
native packet and the original DOS shell's selective owned-mech import. Added
exact DOS current/reference records, traced startup IDs, original deployment
serialization and preservation across graphics-profile changes. Stock data no
longer overwrites the current mech on every launch.

Corrected selected-star result timing, fresh DOS CAR status, activation/arming
and reset/rearm metadata, relative timeout and state8 failure handling using
executed original routines. Added packet corruption/stale/partial checks and
separate quit-after-publication-failure handling. Original debrief, counters,
financial/salvage settlement and full saved careers remain separate work.

Validation includes original-code goldens,30 controlled real-data cycles,
six profile preservation cases and three real SDL/OpenGL mission children.
See `docs/research/MERCS_MISSION_RETURN_INTEGRATION.md`.

## Interactive first-mission shell — 2026-11-01

Replaced the command-line-only default with native folder selection, remembered
data-folder discovery, actual intro playback, Mech Pilot Registration, original
initial contract and briefing text, briefing movies, and a Launch Mission button.
The shell prepares verified Mercs launch records and opens the native simulation;
the user proceeds entirely through the application menus.

Regular registration uses the original owned Commando and the two fixed initial
contracts. Training reads the original Commando/Jenner/Panther choice list and
requires an explicit Assign action. Added support for the original 58-byte MEK
configuration trailers while preserving existing 50-byte layouts. Original
loadouts, player GPS, actor weapons, and bounded mission steps have focused tests.

Integrated the scoped GBL movie filters and fullscreen presentation choices.
Movie playback owns its audio device, preserves the shared renderer, handles
skip/fullscreen/resize, and restores the shell. Settings saves preserve newer
simulation preferences. Added a per-edition launch lock and an in-memory star
encoder so concurrent shells cannot overwrite each other's launch files.

Added the user's three-choice Game Look selector: DOS, Windows Direct3D, and
3dfx. A change keeps pilot/mission/Training assignment selections and uses the
chosen edition's full archive for the next deployment. The Direct3D option uses
Titanium graphics; 3dfx uses the supplied dedicated disc. Both accelerated
archives now decode their actual palette-based 1.2x textures instead of using
the inherited MW2 packed-texture decoder. Their verified linear/reciprocal fog
paths remain distinct. The full retail/DOS soundtrack is shared by every look.

Added `make app` and a Finder-launchable macOS app wrapper with local SDL/font
bundling. Native macOS/Apple Silicon and Windows runtime validation remains open;
the integrated application is tested on Linux x86_64. This handoff contains an
ordinary source ZIP, not compiled release builds. Full campaign result handling,
save/reload, debrief, salvage, and repair transactions remain later work.

See `docs/research/SHELL_VALIDATION.md` for the actual GUI and fixture checks.

## Source handoffs and shell requirements — 2026-11-01

Recorded the user's hold on compiled build deliveries until the native
Mercenaries shell is ready, and the preference for ordinary extractable source
ZIPs during development. The user explicitly defines the first shell-ready
milestone as an executable that compiles, plays the in-game FMV, opens Mech Pilot
Registration, lets the user pull up missions, and launches a mission. Debrief,
salvage, repair and complete save/reload support follow this first gate.

Inspected the supplied GBL fullscreen movie and optional CRT-filter changes as
the user's explicit video presentation reference. Added a scoped integration
map in `docs/research/GBL_VIDEO_REFERENCE.md`; no GBL runtime code has been
transplanted yet. Recorded the complete retail/DOS soundtrack as the standard
across editions and the delivered MP3 folder naming.

This is a documentation and source-packaging update. Runtime implementation and
the first mission checkpoint's validation results are unchanged; no new build
was produced for this handoff.

## First native mission milestone — 2026-11-01

Created a separate Mercenaries source project from the supplied MW2 v1.0.2
foundation. Added the native mission launcher, Mercs game-folder checks,
configuration namespace, and separate writable edition overlays. Preserved the
original game data and the older ports' settings.

Reconstructed the Mercenaries GPS/star record writer, corrected AI level
normalization, extracted the 66-entry weapon table and equipment IDs, expanded
weapon mounts to fourteen, and implemented shared ammunition pools and the
extreme-cold cooling branch. Added focused tests against original records.

Retained exact DOS/Windows weapon timer differences, Mercs CPIT weapon windows,
startup/critical/weapon sound records, and the 190-command input name table.
Corrected INIT time-phase interpretation and DOS backdrop palette indices.
Restored original MW2-prefixed effect asset names while retaining a separate
MERCS environment/configuration namespace. Disabled the old clan-shell build
targets and kept their source/recipes as historical reference.

Ran the unmodified retail DOS engine with the full supplied BIN/CUE, and launched
the same embedded-player scene in the native program. The reference captures
establish concrete remaining HUD/rendering differences. The campaign, salvage,
and career persistence remain separate required work.

The foundation's release history is preserved in
`docs/mw2-foundation/CHANGELOG.md`; those versions are not Mercenaries releases.
