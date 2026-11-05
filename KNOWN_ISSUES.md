# Known issues

What is still open in the Mercenaries port, in plain language.

- **Not yet compared side by side with the original.** The behaviour was traced from the original executables, but the
  port has not been checked frame by frame and fight by fight against the original running in DOSBox. Expect
  differences in AI, combat and the DOS look until it is.
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
