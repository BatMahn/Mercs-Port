# Known issues

What is still open in the Mercenaries port, in plain language.

- **Side-by-side comparison with the original.** Every fixed mission's opening has been compared with the DOS game in
  DOSBox (docs/DOSBOX_COMPARISON.md). Differences found and not yet fixed:
  - **Cockpit panels on IVA1, IVA3, MCC3, MCC6, MED1 and REC1:** lighter than DOS, near black there. IVA and MCC3's light points straight
    up (0, 20000, 0) with a high ambient level; the cockpits of missions with a sideways light (BET1, LUT1) match.
  - **Night ground on BET2 and BET4:** the port shows the dim ground texture of the TL1PLAIN object where DOS shows
    black. Patching TL1PLAIN's colours in a copy of the archive changed nothing in DOS, on BET1 or BET4, so DOS never
    draws it. It is the mission's one object of class 0x90, which DOS registers for a separate draw pass at mission
    start (0x1038c); why that pass shows nothing is not yet traced.
  - **Enemy starts above the ground:** IVA3's enemies start 41 m above a plateau and drop on to it. The player's
    elevated starts are now grounded as in DOS; the enemies' have not been checked.
  - **External camera glide:** for its first two seconds after `c` on DOU1 the port's camera rises over the hill behind
    the mech before settling; DOS's stays low. At rest both match (and on REC1 both perch on the same rock).
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
