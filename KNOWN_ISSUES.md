# Known issues

What is still open in the Mercenaries port, in plain language.

- **Side-by-side comparison with the original.** Every fixed mission's opening has been compared with the DOS game in
  DOSBox (docs/DOSBOX_COMPARISON.md). Differences found and not yet fixed:
  - **Light levels across large textured polygons** can differ slightly (OUT3's hangar roof is a little lighter than in
    DOS). The port lights them per triangle; walking the whole polygon's outline as one span list was tried and matched
    DOSBox worse on MED1 and OUT3, so how DOS interpolates there is still not pinned down.
  - **Camo stripe edges** land a texel or two off (texture coordinate rounding), and **shadow interiors** are a multiply
    by the shadow row's mean darkening, where DOS remaps each pixel through the row. Remapping texel by texel was tried;
    with the port's overlapping shadow polygons it came out no closer to DOSBox, so it was left out.

  - **REC1/REC2's external camera**, perched on the rocks, shows the rock faces next to it darker in DOS, and from a
    slightly different spot (DOS sees more of the rock's shaded side). The fog distance is not the cause; still open.
  - **TRA1's friendly units** walk close to the player on the way back in DOS (one passes right beside the player at
    about three minutes and the autopilot detours around it); the port's stay about 250 m away.
  - **GRA1's Catapult engages sooner in DOS.** On autopilot the player is hit about 15 s earlier in DOS (the
    lower-left target viewer then shows static from display damage), and DOS never plays "Enemy power-up detected".
    The port holds the Catapult powered down until it sights the player 480 m off. What wakes it earlier in DOS needs a
    trace of the DOS AI; the port's hold behaviour is the one traced from the Windows engine and checked on IVA3.

  Combat has been compared only through whole-mission outcomes (BET1, DOU1, GRA1, IVA3, TRA1), not shot by shot.
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
  original (same depth and fog, more vertex work), and some multi-unit timings follow one traced path rather than a
  side-by-side capture.
- **Tests that check the palette effects against the original** need private fixtures (captured palettes from the game)
  and are left out of `make check`.
