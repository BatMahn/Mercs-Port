# Known issues

What is still open in the Mercenaries port, in plain language.

- **Side-by-side comparison with the original.** Every fixed mission's opening has been compared with the DOS game in
  DOSBox (docs/DOSBOX_COMPARISON.md). Differences found and not yet fixed:
  - **External camera glide:** for its first two seconds after `c` on DOU1 the port's camera rises over the hill behind
    the mech before settling; DOS's stays low. At rest both match (and on REC1 both perch on the same rock).
  - **REC2 and MYB1 openings:** DOSBox shows white shapes in REC2's sky that the port does not, and the port shows a
    camouflaged block structure to the right on MYB1 that DOS does not. Neither is traced yet (the captures are taken
    at different moments, so a timing difference is possible).
  - **Large textured polygons are split into triangles** in the port, where DOS fills the whole polygon span by span;
    the light levels across a big quad can differ slightly (OUT3's hangar roof is a little lighter than in DOS).
  - **Camo stripe edges** land a texel or two off, and **shadow interiors** keep the ground's contrast where DOS remaps
    them flatter.

  Combat has been compared only through whole-mission outcomes (BET1, DOU1, GRA1, IVA3), not shot by shot.
- **The full career has not been played from start to finish.** Individual contracts, the debriefing, salvage and
  repairs, and save exchange with the original are tested; the campaign end and the long tail of contracts are not.
- **Windows is untested.** It cross-compiles and links, but has not been run, on Direct3D 12 or Vulkan.
- **The latest cockpit motion fix** (accelerated looks) still needs a visual check on a Mac.
- **MercNet multiplayer** is not implemented.
- **Some original details are approximated:** the accelerated looks' near clipping is done differently from the
  original (same depth and fog, more vertex work), and some multi-unit timings follow one traced path rather than a
  side-by-side capture.
- **Tests that check the palette effects against the original** need private fixtures (captured palettes from the game)
  and are left out of `make check`.
