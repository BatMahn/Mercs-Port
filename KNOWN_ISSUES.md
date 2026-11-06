# Known issues

What is still open in the Mercenaries port, in plain language.

- **Side-by-side comparison with the original has only started.** The DOS sim now runs in DOSBox
  (docs/DOSBOX_COMPARISON.md), but only BET1SCN1's opening has been compared. Differences found there and not yet
  fixed:
  - **IVA1's cockpit panels:** they show the DOS shading bands, but light where DOS is dark.
  - **Camo stripe edges:** they land a texel or two off.
  - **Shadow interior:** the mech shadows keep the ground texture's contrast, where DOS remaps it flatter.

  AI and combat have not been compared at all.
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
