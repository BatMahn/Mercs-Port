# Known issues

What is still open in the Mercenaries port, in plain language.

- **Side-by-side comparison with the original.** Every fixed mission's opening has been compared with the DOS game in
  DOSBox (docs/DOSBOX_COMPARISON.md). Differences found and not yet fixed:
  - **Light levels** follow DOS's span walk, cosine, near plane and 2D screen clip (v1.0.1, v1.0.2, v1.2.0). Still off,
    in DOSBox level maps: on LUT1 8 % of pixels sit one level off, nearly all in the two large foreground polygons on
    the right, where the dither's phase flips one row before DOS's (their visible edges match DOS's, so the walked left
    edge's 16.16 position differs by a fraction somewhere above the screen; not found). On the fog-table mission MED1
    8 % of pixels are two or more levels off, nearly all on the near building at the left: DOS shows one flat level
    where the port shows the wall's banded levels, consistent with DOS not drawing that lower wall face (not traced).
    Where the far terrain meets the ground at the horizon, DOS draws the terrain's bottom pixel row and the port shows
    the ground there: a textured polygon's coverage is still the GPU's (float corners, top-left rule), not DOS's walk,
    which includes the bottom row (Gouraud polygons already use the walk's coverage).
  - **Camo stripe edges** land a texel or two off (texture coordinate rounding), and **shadow interiors** are a multiply
    by the shadow row's mean darkening, where DOS remaps each pixel through the row. Remapping texel by texel was tried;
    with the port's overlapping shadow polygons it came out no closer to DOSBox, so it was left out.

  - **REC1/REC2's external camera**, perched on the rocks: DOS shows dark, shaded rock faces beside the camera where the
    port shows the lit face nearest it. The rest of the view lines up, so the camera is in the same place. Neither the
    fog nor DOS's near plane (v1.0.2) is the cause; DOS draws without a depth buffer, in its sorted polygon order, and
    the likeliest cause is that order letting faces behind the near rock paint over it. Not reproduced.
  - **TRA1's instructor Catapult stalls on a dune in the port.** Traced in DOSBox (after v1.1.0, the four friendly
    AI records and the player's position watched over four minutes on autopilot at full throttle): the Thug, Bombardier
    and Panther go north and stop around (-470, 2180) m within the first 80 s, as in the port. The Catapult leads the
    player round the navs (Alpha, Beta, Gamma) in both, and passes within about 20 m of the player near Nav Gamma in
    both (DOS about 19 m, the port about 12 m). It then heads north-west towards Nav Delta. At (-927, 1528) m it walks
    into the slope of the TLADUNL dune. DOS slows there for about 3 s and then climbs the dune (its height rises from
    64 m to 92 m) and walks on. The port's Catapult stays stuck at that spot for the rest of the run, 250 to 400 m
    from the player. The terrain height there matches DOS exactly. The port refuses every move against a terrain face
    whose normal is about (0.48, 0.71, -0.51), right at the 45 degree climbable limit; the move is harmless (no
    damage), but it is cut back each tick and the AI's avoidance turns back and forth without getting free. Why DOS's
    move (0x1fca0) lets the mech up that face is not yet traced.
  - **GRA1's ambush timing** varies from run to run in DOS. Traced in DOSBox (after v1.1.0, AI records watched through
    the whole run): the Catapult and the Hunchback stay powered down in their rest state (10) in place for the whole
    mission in DOS as well, so the earlier report that DOS's Catapult wakes sooner was wrong. In both games the Crab
    walks in from the west on nearly the same track (the port's starts 1 to 2 s sooner) and it is the Crab that
    destroys the player. DOS took 28 to 44 s from the HUD over six runs (its randomness differs each run); the port
    takes 25 to 28 s over seven seeds, at or just before the early edge of DOS's spread. Whether the port's Crab
    opens fire a little sooner or from further out has not been measured shot by shot.

  Damage application has been compared hit by hit with DOS (v1.2.0, GRA1, about 1,400 hits) and matches; which weapon
  fired each hit was not logged, so per-weapon damage values are checked only against the port's weapon table.
- **The Salvage Yard (Titanium's dynamic salvage, v1.1.0)** is built from Titanium's code and art but has not been
  compared with the running Titanium game, and no career has been played through it by a person. Damaged salvaged
  items follow Titanium's arms dealer and Mech Lab (no install until repaired; sold at 15% and scrapped; repaired for
  15% with the Titanium-only Repair button), but that screen has not been compared with the running game. The wreck
  list is kept with each native save from the return until the debrief is left, so a career saved there keeps its
  Salvage Yard; an original `mersaveN.sav` export does not carry it.
- **The full career has not been played by a person from start to finish.** Both careers are walked to the ending in
  every edition with recorded wins (every contract accepted, returned, debriefed and continued), and individual
  contracts, salvage, repairs and save exchange are tested; the missions after the first few have not been fought.
- **Windows has been run only under Wine** (Vulkan, through Mesa's software Vulkan driver): the menus, movies,
  registration, DropShip, Duty Roster, a career mission and Instant Action with their returns all work, and a mission
  frame is pixel-identical to the Linux build. Direct3D 12 could not be checked: Wine 9's vkd3d cannot compile the
  shaders, and the game now steps over to Vulkan by itself when that happens. On real Windows hardware neither
  backend has been run yet.
- **The latest cockpit motion fix** (accelerated looks) still needs a visual check on a Mac.
- **MercNet multiplayer** is not implemented.
- **Some original details are approximated:** the accelerated looks' near clipping is done differently from the
  original (same depth and fog, more vertex work; the world near plane is 60 cm in first person where Titanium uses
  half the view width + 1, 161 cm at 640 x 480 - see ASSUMPTIONS.md), and some multi-unit timings follow one traced path rather than a
  side-by-side capture.
- **Tests that check the palette effects against the original** need private fixtures (captured palettes from the game)
  and are left out of `make check`.
