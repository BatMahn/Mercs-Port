# Evidence, implementation choices and limits

Mercenaries original instructions and data govern this port. The mature Mech2
source supplies infrastructure where comparison supports reuse. GBL is the
scoped reference for the requested movie/fullscreen/CRT presentation only.

The latest evidence index is
GAMEPLAY_FIDELITY_FOLLOWUP.md. The broader
single-player implementation index is
MERCS_COMPLETION_HANDOFF.md.
The earlier gameplay checkpoint's assumption table is retained under
`docs/research/checkpoints/gameplay-assumptions.md`; its open-item statements
are historical, not the current implementation inventory.

## SDL3 platform migration

SDL3 replaces the platform APIs while preserving the existing Mercs simulation,
original asset decoding, save format and three game-look profiles. The runtime
uses SDL3 per-window input, physical drawable dimensions, per-target renderer
state and logical audio-device streams. The existing original mixer feeds
44.1-kHz stereo float samples; SDL converts them for the selected physical device.
Fractional SDL mouse motion is retained until the original control conversion.

The migrated APIs are tested with SDL 3.4.18 on Linux. Selected captures compare
to the immediately preceding SDL2 source, not to new original-hardware captures.
Existing original-binary goldens retain their separately documented scope.
The current evidence is indexed in [SDL3_MIGRATION.md](docs/SDL3_MIGRATION.md).

## Evidence classes

| Evidence | What it establishes | What it does not establish |
| --- | --- | --- |
| Original bytes, static tables and hashes | Identity, layout and authored values in the supplied edition | Every caller's behavior or another edition's equality |
| Executed original function/instruction probes | Observed arithmetic, branches and outputs for specified inputs | Whole-program ordering, all RNG history, original hardware or unbounded input coverage |
| Native core comparisons | Native outputs agree with the measured cases | Every unmeasured scene or mission outcome |
| Actual SDL/OpenGL runs | Real native event/render/child-launch integration in the recorded environment | Native macOS/Windows or original-driver pixel parity |
| Controlled campaign returns | Persistence, ownership, debrief and successor transitions for those outcomes | That the player can win every mission through ordinary play |
| Automated input-driven campaign play | Real objective completion/failure, return and career operations along the recorded route | Human play, unrecorded missions, all AI choices or original-game timing parity; the pilot observes internal state |
| Source/mock app-packaging checks | Packaging structure, architecture guards and command construction | A successful M4 Pro build, Finder launch or audio/graphics run |

## Recovered rules and explicit native choices

| Area | Recovered behavior | Native boundary or deliberate policy |
| --- | --- | --- |
| Career and economy | Original list values, quotes, hire/dismiss, roster, groups, repairs and campaign transitions | Durable staged commits and retryable I/O keep a failed write from partially changing live state |
| Save interchange | DOS0x850 and retail Windows0x853 rows, header/list bytes, screen/restore flags and accepted procedural state | Runtime uses a common row plus retained opaque Windows extension; native resume needs its companion; no active simulation snapshot |
| Deployment | Original STAR/GPS/mech identities, pilot attributes and return binding | Player plus two owned ground mates; optional AeroTech support appends after those mechs, allowing four units and correcting a bounded mismatch with the original three-slot attribute pass |
| Authored stock players | MCC4, SIG2 and OUT3 use their exact archive player instead of `USERSTAR`; unused owned deployment still follows the normal shell import | No borrowed-unit exemption from original ejection/death rules; stock-player damage is not serialized over the owned mech |
| Unit and scenery alliances | Original STAR loading populates both ordinal unit alliances and the first-word affiliation map for GT scenery; the player uses the former | The corrected player value supplies career side statistics and native AMS IFF; general AI selection and damage remain separate consumers |
| Fleet limits | Original variable-length saved lists and identity matching | Bounded4096-row codec;96-row complete transactions/UI are explicitly tested |
| Mech Lab | Original equipment, engine, armor, mass, cost, pending critical placement and acceptance | Corrected original dirty flags, CASE/double-sink reservation, stale MASC removal and upgrade recalc ordering are separately documented |
| Procedural missions | Exact measured generator bytes, draws, tables, templates, placement and named-resource linkage | Native elapsed UI time supplies seed; exceptional unsafe original draws reject/retry before acceptance; accepted seed is immutable; RANDWLD/RND aliases repair original naming mismatch |
| Instant Action | Original roster/parameters, random-year draw order, custom designs and launch inputs | Isolated capsule/ephemeral resources prevent career return contamination; deterministic numeric USR sorting replaces DOS directory enumeration |
| Earned scores | Original asset total, strict qualification, all ten rows and Watcom tie ordering | Transaction receipt prevents duplicate submission after interruption; external conflicting score data is preserved |
| Combat and effects | Original ammunition guard/cook-off, critical loss, special weapons, orders, location branches, debris, light hold and numeric edition tables | Floating integration and broader timing/RNG are not whole-original-program emulation; original numerical quirks are retained where bounded |
| Missile acquisition | Executed original body-position publication, cached target coordinates and bounded lock gates; current body centers, AI behavior-3 foot targeting outside Easy, exclusive minimum and inclusive maximum | Native angle/distance calculations remain floating point; three archive geometry inputs use the retail LRM10 comparison profile, and no original Titanium executable is executed |
| Lock duration | Executed DOS/retail Windows player and AI acquisition, interruption, decay and NARC sequences; DOS364 versus Windows362 ticks, including AI forced-lock retention | Native elapsed seconds convert to floating ticks; bounded comparisons report conversion error, and original scheduler/Titanium timing are not certified |
| AI firing visibility | Original posed eye ray, horizontal range, front-half-plane and selected-owner acceptance; native nearest world/unit/debris hit and original update order | Direct GT identity matches the bounded original Windows IVA1/OUT1 initial startup cases; later parent recreation, arbitrary hierarchy histories, equal cross-list ties, fixed-point terrain equivalence and legacy no-eye/zero-range fallbacks remain separate limits |
| Independent ammunition pools | Original loaders build type pools from the complete link array before matching weapon mounts, preserving bins after a gun is destroyed | Strict type/bin/pool/cursor/link consistency checks remain; surviving ammunition is neither dropped nor synthesized from reference critical slots |
| Cockpit | Original attachment, part classes, THNG controller rotations, near plane and open panes | All 65 supplied cockpit-capable resource models have native pose/representation checks; original-driver pixel identity is not certified |
| Texture animation | Executed DOS/Windows slot-clock transitions, resets/stops, sparse frames, XPLO/polygon bindings and expanded include order | Native simulation schedules the clock service; arbitrary later dynamic rebinding and whole original render-loop ordering are not reconstructed |
| Ground and LOD | Original WTBO chain/selection, DOS tiled ground, edition topology, shade/fog and splitting | GL clipping, interpolation coverage and complete driver quantization remain modern approximations |
| Terrain index | Executed original integer-centimetre bounds, nearest/positive-half rounding and wrapped-add/arithmetic-shift subdivision | Native posed vertices and incoming contact coordinates remain floating point; fractional queries between adjacent integer bounds remain a separate limit |
| Polygon normals | Executed original DOS/Windows signed-64-bit cross, highest-bit candidate metric and Q29 normalization, shared by cockpit and collision | Preserve original imperfect magnitudes and degenerate results; out-of-range coordinate differences retain an explicit native guard, and world rotation/contact integration remain floating point |
| Placement | Original BWD block-stack composition, includes, origins and repeated instances | Bounded traversal rejects malformed input; no guessing from briefing prose |
| Buried spawns | Original strict ordinary terrain selector | Once-only mobile-spawn correction is a native adaptation; elevated/fixed/airborne starts are excluded |
| Aircraft | Original MGDF/root publication, authored GP/MGEO data and class-2 single-player absolute-altitude publication; successful attached wrecks share the latter branch | Class-2/5/8 geometry shares controller-centre placement in both visual profiles; only class 2 gets the recovered absolute-altitude rule. Native later wreck eligibility is not proved equivalent to the original persistent ground-contact flag |
| Human target selection | Executed original current-body coordinate producers and ordinary-flag actor range checks; both unit/thing consumers use the shared human body delta | Existing flags, alliance/shutdown gates and inclusive 1750-metre maximum are preserved; native floating distance and whole original target-loop ordering remain separate limits |
| Controls/HUD | CPC records, driver metadata, original commands, seven windows, file widths, novice steering and map material/gates | Physical device/OS behavior still needs native testing; no invented enabling of the default-disabled map permission |
| Audio | Executed gameplay selectors, original sound rules, positional attenuation, fixed-point stereo pan, caps, voice banks and SFLX output | Native resampling/limiting and physical output remain separate; no certified audible Mac mix or every mission trigger |
| Long objective labels | Original ordered MSN writes can span nominal text-field boundaries | Native writes remain bounded by the complete payload and reject invalid/unterminated input atomically |
| Cold launch | Original main-menu entry after intro | Automatic working generation is retained for explicit recovery; cold launch no longer auto-restores it |
| Shell/video | Original art/fonts/events/dialog defaults; original movie media | Centered native fit, F5/F10, optional CRT/fill/stretch and retry messages are modern policies |
| Edition selection | Separate DOS, dedicated 3dfx and Titanium resources and numeric weapon data | Native OpenGL reproduction; no Glide/hardware emulation claim |
| Soundtrack | User-selected full DOS tracks02–22 | All graphics choices keep that soundtrack; Titanium never replaces it |
| Camouflage | A unit's STAR +0 value (0x60a10) picks its set through the mission's CAMO chunks (0x62bf6 -> 0xbc92c); the even plate slots 0-0xa become slot + set for sets 0/1, else base[set] + slot/2 with base {0,1,0x56,0x66,0x76,0x86,0xf3,0xf9} (0x62e74); the insignia is slot 0x3f + the STAR value. DOSBox BET1SCN1, plus a patched STAR that changed the player's plates | Applied to the dedicated 3dfx look as well, since its maps use the same plate layout (INFERRED) |
| DOS unit shadows | Polygons with normal y >= 0 dropped to the unit's ground (+5 unless 0) and filled through LUMA row `shadowtable` (0x509e8 -> 0x50948 -> 0x507b4) | Drawn as a multiply by that row's mean darkening of the ground texture, per channel, rather than a per-pixel LUMA remap |
| DOS per-vertex light | Vertex normals: polygons from the last, face normals added and renormalised (0x46168); light per vertex (0x4f6c8) with the normal passed z, y, x (0x4f79f); textured spans take the vertex levels (0x49f20) | The point-light direction is from each vertex rather than from the first polygon to light it. A textured polygon's level is walked as DOS's spans (0x7b43e light pass 0x7bd2c: 16.16 edges and pixel pairs, truncated, checkerboard dither of half a level) in the fragment shader, clipped at DOS's near plane (+0x3c x 4) with its cosine (0x4d246, Q29 normal from the port's float one); the dither's phase is from DOSBox level maps (its setter 0x7fd88 has no traced caller). Point lights (MLIT) still use the port's per-vertex direction. The Gouraud colour polygons using vertex levels is INFERRED from LUT1/BET1 frames (IVA1 disagrees) |
| DOS MLIT point lights | Mission chunk MLIT {x, y, z, box half size} (0x62ba9 -> 0x4f4fc); used with Multiple Lights; + 128 cos while under 0x80 (0x4fb3b) | Box test on the fragment / vertex position rather than any vertex of the polygon (0x4f5f8); up to 4 lights |
| Bad original records | The original's record read (MERCSW.DLL 0x1001ead0) sums nothing. The 26 shipped wrong-checksum records (DOS ASA00STD; the 3Dfx/Titanium MEKs, CELs, POLYs, MEKTABLE) and truncated BET4CAM1 are fingerprinted by SHA-256 | Exceptions are exact; missing BET4 rows are not fabricated; any other mismatch or malformed data rejects |
| Dynamic salvage (Titanium) | Traced from the Titanium disc: mercsw.dll 0x10035ef6 / 0x10059a10 / 0x10059d0f (recording, SALVAGE.MW2 layout, srand(time) + 96 rand() a wreck) and MW2SHELL.DLL Salvage.cpp (0x100675e8 build and rolls, 0x1006729d whole-chassis odds, 0x100652b8 / 0x1006581f / 0x1006565a / 0x100659c6 / 0x10065c2e actions, 0x10069597 commit, debrief gate and per-mission table, hotspot table 0x100b9ba8). Background is Titanium DATABASE.MW2 resource 47 | An option: auto = Titanium Direct3D only (DOS and the retail Windows shell never read SALVAGE.MW2) |
| Salvage name and armour | Wreck name = the unit's loadout prefix (5 characters), shell base `<name>STD`; armour divided by the difficulty's factor 1 / 3 / 4 for every wreck, as 0x10059a10 does | INFERRED: the engine's name table entry is the loadout prefix; a missing `<name>STD` falls back to the chassis' 00 variant (Titanium would skip the wreck) |
| Salvage inventory rows | Taken items become owned rows as the port's Mech Lab returns them (price, kind 0/1/2); heat sinks 1 or 2 by slots, jump jet 6, CASE 5 (0x10064537). Damaged flag at +0x16 as Titanium; ammunition type = its bin's weapon code / 100 | INFERRED: the ammunition type mapping; the port's Mech Lab and repair do not yet treat a damaged item differently |
| Salvage quirks kept | Weapon type 0 (LRM 20 Clan) falls outside 0x10061b13's categories and cannot be salvaged; the 18-items-a-category limit in 0x100652b8 counts a table it never fills and never triggers | Original behaviour |

MercNet is explicitly out of scope. The user has confirmed a working Mac build
and tabled further platform work. Whole-campaign, global-timing and physical
input/audio evidence limits remain in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
