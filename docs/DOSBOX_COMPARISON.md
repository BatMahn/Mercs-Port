# Comparing the port with the original in DOSBox

The DOS sim (`cd/MERCS/MW2.EXE`) is the reference for Mercenaries' behaviour and its DOS look. This file explains how
to run it headless in DOSBox, straight into a mission, and how to capture the matching frame from the port. All of
it runs on the user's own game folder; nothing from it is distributed.

## 1. Setting up the reference install

    sudo apt install dosbox genisoimage xvfb xdotool imagemagick
    make prjtool
    scripts/dosbox/setup_mercs_dosbox.sh /path/to/Mercs-game /tmp/mercsref BET1SCN1             # 320 x 200
    scripts/dosbox/setup_mercs_dosbox.sh /path/to/Mercs-game /tmp/mercsref480 BET1SCN1 VESA480.DLL   # 640 x 480
    scripts/dosbox/setup_mercs_dosbox.sh /path/to/Mercs-game /tmp/mercsref768 BET1SCN1 VESA768.DLL   # 1024 x 768

The script does the following. Each step fixes a startup failure the DOS sim hit without it.

| Problem | Cause in MW2.EXE | What the script does |
|---|---|---|
| fatal #4D `SYSERR_CANTFINDVFXDRIVER` | 0x1005d: the video driver is `vfx\<name>`; MCGA.DLL when no `-VG` switch is given | Copies `cd/VFX/*` into `C:\` and `C:\VFX\`, and `cd/GIDDI` into `C:\GIDDI` |
| fatal #53 `SYSERR_CDNOTFOUND` (the file check) | 0x20620: `opendir("%c:\")` on the MSCDEX drive must list `OLD_MERC.DRV` **with the hidden attribute** (bit 2 at +0x15) | Builds an ISO of `cd/` with `genisoimage -hidden OLD_MERC.DRV` |
| fatal #53 again (the audio check) | 0x549e5: 0x533cc requires CD audio tracks | Writes a CUE: the ISO as track 1, plus `music/Track02-22.mp3` as audio tracks (DOSBox 0.74-3 reads MP3 in a CUE) |
| fatal #21 `mek\PLY00MER.mek` | `USERSTAR.BWD` names the player's Mech Lab file, which the shell normally writes | Extracts the stock `COM00STD` MEK as `MEK\PLY00MER.MEK` |
| resolution | Command switch `-VG:<driver>` (0x551a8: switch letter V, then G; sets bit 1 of 0x207efc and copies the driver name) | Appends `-VG:VESA480.DLL` or `-VG:VESA768.DLL` to the `MW2 <scene>` line |

The game's own executable is not patched; the PRJ is not touched either.

## 2. Running and capturing

    printf 'sleep 34\nshot a\nkey c\nsleep 2\nshot b\n' > steps.ds
    DOSCONF=/tmp/mercsref480/mercs.conf SHOTDIR=/tmp/shots flock /tmp/dosbox.lock scripts/dosbox/dosrun.sh steps.ds

Step commands: `sleep N`, `key K` (xdotool names: `c`, `Escape`, `F12`, ...), `down K` / `up K`, `type TEXT`, and
`shot NAME`, which writes `ds_NAME.png` (the whole 1280 x 1024 Xvfb screen).

- **Where the game is in the capture.** DOSBox sits at 0,0. The game fills 0,0-639,479 (VESA480), 0,0-1023,767
  (VESA768), or 0,0-639,399 for the default 320 x 200 mode, which DOSBox doubles in both directions.
- **When the mission is up.** The boot and load take 6-30 s of wall time depending on the host (check with an early shot). DOSBox runs slower than real
  time, so match frames by game events, never by wall seconds.
- **Useful keys:** `c` switches to the external view, which shows the player's mech. `Esc` opens the main menu;
  its option 4 is Combat Variables.
- **Run one at a time.** Runs share display :99, so always use `flock`.
- **Restore the install before every run.** When a mission ends (the player dies, or it is won or lost), the DOS sim
  writes the player's mech back to `MEK\PLY00MER.MEK` and leaves `MW2CAR.CFG`, `MW2MSN.CFG` and `SALVAGE.MW2`
  behind. After one death every later run starts with a wrecked mech: no cockpit, an empty weapon list, the camera in
  the ground. Copy the extracted MEK back and delete those three files before each run. (A sweep that skipped this
  reported "dead at the start" on GRA2-4, DOW2 and IVA2-4; they all match once the mech is restored.)

## 3. The port's matching frame

    printf 'game=/path/to/Mercs-game\nrender=320x200\n' > r320.cfg     # only for the 320 x 200 comparison
    SDL_VIDEODRIVER=offscreen MERCS_SIZE=1024x768 MERCS_NO_STARTUP=1 \
      MERCS_AUTOPILOT=3,0,0,0,out.ppm,1 [MERCS_KEYS=1:c] ./mercs /path/to/Mercs-game --mission BET1SCN1

The player's mech must be the same on both sides. Put the same `PLY00MER.MEK` into the game folder's
`install/retail/MEK/`. Diff with PIL: look at the mean difference, the share of pixels more than 32 levels off,
and the difference mask.

## 4. Results so far (BET1SCN1, 2026-11-06)

At 640 x 480 the HUD, cockpit outline, compass, weapon list, radar and skyline line up. 1.0 % of pixels are more than
32 levels off.

**Fixed after comparison:**

- **Camouflage and insignia by star.** A unit's star value (STAR +0) picks a camouflage set through the mission's
  CAMO chunks: player star 6 → set 3, lancemates 1 → set 4. The set remaps the even plate slots 0-0xa
  (MW2.EXE 0x62e74, 0x62bf6). The insignia uses slot 0x3f plus the star value. The port drew every unit with
  plate set 0. Checked by patching a copy of the PRJ: changing the player's STAR value in DOSBox changed its plates.
- **Unit shadows.** These follow 0x509e8 → 0x50948 → 0x507b4. Polygons whose normal does not point down are
  dropped to the ground under the unit (+5, or 0 when the ground is 0). They are filled through LUMA row
  `shadowtable` (7 on BET1). The port uses a multiply by that row's mean darkening (0.746 on BET1). The shape and
  position match; DOS's shade is slightly greener.
- **Radar nav blips.** The sprite table at 0xc9d14 has rows of 3 and is the same in MERCSW.DLL. Navs use row 4,
  RTGTNP / RRTGTNP (a diamond). The selected nav adds row 5 on top, RNP / RRNP; off the radar it uses row 6,
  RTGTOF. Pixel-identical at 1024 x 768, before and after `n`.
- **Removed:** the "ramp-3 texels take the polygon's ramp" plate rule. LUT1SCN1 contradicted it: its grey panel
  lines came out yellow.
- **Lighting.**
  - DOS lights textured polygons per vertex (0x46168, 0x4f6c8, 0x49f20).
  - The per-vertex call (0x4f79f) passes the vertex normal as z, y, x.
  - The mission's MLIT point lights add to everything inside their boxes (0x62ba9, 0x4fb3b).
  - The old "darker than DOS" finding came from missing all three; the paint rule and the ×2 idea were dead ends.
  - The DOS levels now match per pixel across the BET1 mech. The remaining mismatches sit on stripe edges, where
    the texture sampling falls a texel off.
- **Building window grid.** This was fixed by the x/z swap.
- **Shadow tint.** The shadow now darkens each channel separately.
- **Gouraud cockpits (2026-11-09).** Mercenaries' Gouraud fill (0x495d0, colour type 0x4000) draws a ramp-shade corner
  as the polygon's ramp + the corner's level, or ramp + 15 - level when the LUMA table is a fog table (its first byte
  not 0, 0x49d06 -> 0xb3f6c). The port had used MW2's ramp + intensity x level. Checked by patching a copy of the PRJ:
  IVA1 with ambient 20 instead of 95 turns DOS's cockpit light; replacing the FOG LUMA table changes nothing there (so
  the table is not looked up). With a fog table the MLIT point lights are not added per vertex (0x4f84a). The
  re-captured cockpit frames of IVA1-4, MCC3/6/7, MED1, REC1/2 and LUT3/6 went from 5-30 % of pixels off to under 3 %.
- **Night ground (BET2, BET4).** TL1PLAIN is drawn (patching it to untextured removes BET1's green patch in DOS), but
  black at night: its corners are hundreds of metres off, so the depth term takes their light to 0, and DOS
  interpolates the corner levels in screen space, where the near-plane corners lie far off the screen (raising the
  fog distance 100 x in a PRJ copy shows it). The port now interpolates the vertex levels in screen space and lights
  the corners it makes on the near plane itself. Its 4 cm drawing lift of flat ground pieces is no longer in the
  collision (DOSBox BET2: Alt 0.00 m).
- **External camera floor (DOU1).** The orbit camera's height floor left out moving objects: the DropShip leaving over
  DOU1's start had pushed the port's camera 50 m up for two seconds. Shots after `c` at 0.15-4.5 s match DOSBox.
- **Capture timing.** On this machine DOSBox has the mission up about 6 s after launch, so `sleep 34` frames are ~25 s
  into the mission. Compare against the port at the same mission time (REC2's exhaust puffs and MYB1's Dragon were
  only that).
- **Damage shading in the DOS look.** 0x4689b darkens a damaged mech location's levels by (16 - n) / 16 (or brightens
  them, PLNT +52); the port had applied it only to the 3D editions. Traced, not yet captured in DOSBox.

**Still different (open):**

1. **Shadow interior.** DOS remaps each pixel through the LUMA row, which flattens the ground texture. The port's
   multiply keeps the texture's contrast.
2. **Camo stripe edges.** DOS's texture spans are perspective-correct every 16 pixels and pick the nearest texel
   (MW2.EXE 0x7b43e); the DOS look now walks them the same way. What remains is the external view's geometry,
   about a pixel lower in the port than in DOSBox.

## 5. Full campaign sweep (v1.2.28, 2026-11-20)

All 68 campaign missions that start straight from `MW2 <scene>` were captured in DOSBox at 640 x 480 and in the port
at the same mission time, one cockpit frame and one external frame each. Per-scene sheets (DOS | port | pixels more
than 32 levels off) come from `scripts/dosbox/sweep_compare.py`; they stay on the user's machine.

**Timing.** DOS draws the full HUD only once the player's mech is up, 0x43e + rand(0x16a) ticks into the mission
(6.0-8.0 s). The DOS harness waits for that HUD, so "sync + 3 s" is mission time ~10 s, not 3 s. The port frames are
taken at mission 10 s (cockpit) and 13 s (external, `c` at 10.3 s), with the start-up left on
(`MERCS_NO_STARTUP` unset). An earlier pass at 3 s / 5 s compared different moments and is void.

**Message bars.** The DOSBox config has no sound card, so DOS prints voiced lines as text ("Reactor, Sensors and
Weapons on-line", "External camera engaged."). The port with voices plays them without text, as DOS does with sound.
The top and bottom 30 rows are left out of the numbers below.

**Result.** Cockpit frames: 54 of 68 have under 2 % of pixels more than 32 levels off, 62 under 5 % (median 0.4 %).
External frames: 36 under 2 %, 55 under 5 % (median 1.9 %). Nearly all of what is left is units that moved
differently.

| Scene | cockpit mean | cockpit % off | external mean | external % off |
|---|---|---|---|---|
| BET1 | 2.5 | 0.5 | 5.4 | 3.3 |
| BET2 | 1.5 | 1.0 | 2.7 | 2.7 |
| BET3 | 1.3 | 0.2 | 4.0 | 2.0 |
| BET4 | 1.0 | 0.6 | 1.9 | 2.2 |
| BET5 | 0.2 | 0.1 | 2.0 | 1.8 |
| BIG1 | 1.3 | 0.2 | 1.4 | 0.3 |
| BUZ1 | 2.0 | 0.9 | 6.4 | 4.1 |
| DOM1 | 1.3 | 0.0 | 4.2 | 1.8 |
| DOU1 | 2.8 | 1.8 | 124.0 | 56.1 |
| DOW2 | 0.4 | 0.0 | 1.3 | 1.7 |
| GRA1 | 1.4 | 0.1 | 5.0 | 2.4 |
| GRA2 | 0.6 | 0.3 | 2.0 | 0.4 |
| GRA3 | 2.5 | 1.0 | 6.2 | 3.5 |
| GRA4 | 2.2 | 0.5 | 5.2 | 3.0 |
| GRA5 | 1.1 | 0.4 | 1.7 | 0.7 |
| IVA1 | 1.2 | 0.6 | 3.5 | 2.9 |
| IVA2 | 0.8 | 0.1 | 3.0 | 1.9 |
| IVA3 | 4.3 | 0.7 | 11.2 | 7.4 |
| IVA4 | 0.8 | 0.1 | 2.9 | 1.9 |
| IVA5 | 3.5 | 0.8 | 5.0 | 3.0 |
| LUT1 | 1.5 | 0.1 | 10.8 | 3.9 |
| LUT2 | 0.8 | 0.2 | 3.8 | 2.8 |
| LUT3 | 1.0 | 0.0 | 3.0 | 0.6 |
| LUT4 | 1.0 | 0.0 | 2.8 | 0.5 |
| LUT5 | 0.8 | 0.2 | 3.8 | 2.9 |
| LUT6 | 1.0 | 0.0 | 2.9 | 0.6 |
| MCC1 | 10.0 | 12.8 | 4.2 | 2.4 |
| MCC2 | 2.3 | 1.4 | 0.3 | 0.1 |
| MCC3 | 2.4 | 1.2 | 19.6 | 37.0 |
| MCC4 | 5.5 | 0.1 | 7.5 | 2.0 |
| MCC5 | 2.5 | 0.1 | 4.1 | 1.9 |
| MCC6 | 0.8 | 0.1 | 4.3 | 2.6 |
| MCC7 | 0.9 | 0.5 | 2.5 | 0.9 |
| MED1 | 4.1 | 2.8 | 7.8 | 5.2 |
| MOU1 | 1.4 | 0.1 | 4.2 | 1.4 |
| MYB1 | 3.6 | 3.6 | 5.4 | 2.9 |
| OUT1 | 1.7 | 0.8 | 2.3 | 0.4 |
| OUT2 | 3.7 | 2.4 | 5.8 | 3.4 |
| OUT3 | 4.4 | 2.9 | 10.4 | 10.2 |
| OUT4 | 15.4 | 14.9 | 3.2 | 0.6 |
| REC1 | 1.0 | 0.0 | 36.3 | 38.3 |
| REC2 | 5.0 | 6.3 | 54.5 | 64.9 |
| REV1 | 5.2 | 4.4 | 6.0 | 5.2 |
| ROG1 | 0.2 | 0.0 | 1.1 | 0.0 |
| SIG1 | 1.6 | 0.7 | 3.8 | 1.8 |
| SIG2 | 3.2 | 2.0 | 5.3 | 3.8 |
| SIG3 | 1.3 | 0.1 | 2.5 | 1.8 |
| SIG4 | 1.1 | 0.1 | 2.9 | 1.4 |
| SOL1 | 5.0 | 5.1 | 13.2 | 22.7 |
| SOL2 | 1.5 | 0.3 | 2.1 | 1.0 |
| SOL3 | 0.9 | 0.1 | 1.5 | 0.4 |
| SOL4 | 0.4 | 0.0 | 1.5 | 0.5 |
| SOL5 | 75.3 | 87.5 | 37.5 | 26.9 |
| SOL6 | 0.9 | 0.3 | 1.8 | 0.2 |
| SPA1 | 2.2 | 0.5 | 5.9 | 3.3 |
| SPA2 | 4.2 | 3.1 | 9.3 | 7.0 |
| SPA3 | 34.0 | 57.3 | 15.4 | 9.3 |
| SUR1 | 4.2 | 2.7 | 32.5 | 64.0 |
| TRA1 | 2.0 | 0.4 | 4.0 | 1.6 |
| TRA2 | 2.5 | 0.4 | 4.1 | 1.5 |
| TRA3 | 1.8 | 0.3 | 3.2 | 0.8 |
| TRA4 | 1.8 | 0.3 | 4.1 | 1.6 |
| WOL1 | 1.5 | 0.8 | 3.1 | 0.7 |
| WOL2 | 3.2 | 0.0 | 5.6 | 1.2 |
| WOL3 | 0.9 | 0.0 | 2.8 | 0.4 |
| WOL4 | 3.1 | 0.0 | 7.0 | 0.6 |
| WOL5 | 0.9 | 0.0 | 2.9 | 0.4 |
| WOU1 | 0.9 | 0.0 | 3.3 | 0.7 |

**Differences found (open, nothing changed yet):**

1. **The end of a mission after the player dies.** DOS (MW2.EXE 0x57918, single player: 0xb5dfc = 0) puts
   "Exiting Mission Area..." (0xa30cc) on the top bar as the mech goes up and leaves the sim 0x16c ticks (2.0 s)
   later. Seen in DOU1 (killed by enemy fire) and GRA1 (Ctrl+Alt+X). The port's DOS end shows "Mission failed" and
   "Press any key to exit..." and waits about 20 s. Its addresses (0x16e80, 0x9620c) are not in this MW2.EXE and
   "Press any key to exit" is not among its strings; the port kept that end for a tester's reference picture. Left
   as is until the reference picture's source is known.
2. **BET4's rescue helicopter.** BET4ARE2 (S_1AHUEY, task `1;oneshot,rotate,helo`) flies north from 137 m in front
   of the start, 100 m up, from the first second in the port; it shows as a dark shape over the city. DOSBox shows
   no helicopter in the first 17 s. Already so in v1.2.22.
3. **Unit shadows on fog-table missions.** Where SKYGND.PAR gives `shadowtable=15` (WOU1, MED1, MCC1) DOS draws no
   visible shadow under the mech; the port draws one. The FOG LUMA row 15 maps the ground four levels darker, so
   the table alone does not explain it; 0x507b4 takes another path when 0xb3f70 is 0 (not traced).
4. **MCC3's start height.** The port stands the player on a structure (HUD Alt 7.24 m, feet 47.2 m); DOS shows Alt
   0.00 m and its external camera sits inside that same mass, so DOS starts the mech at ground level inside it.
5. **Early fights.** SOL5: the port's player is destroyed at 9.8 s by one 10-point PPC hit to the centre torso with
   51 armour points left; in DOSBox the player is still up at mission ~13 s. SPA3: port player destroyed at 8.7 s
   (two SRM volleys); DOSBox's is still up at mission ~25 s, and an enemy blows up next to it at ~12 s. DOU1 agrees (both
   destroyed at 11-13 s).
6. **SOL1's hangar.** DOS shows red beam lines and a brighter red far hall at mission 10 s; the port shows few lines
   and a darker hall.
7. **LUT1's skyline.** A building face behind the player is lit white in DOS and dark in the port.
8. **OUT4's cockpit at 10 s** is lit bright in DOS (a glow ahead of the player) and dark in the port.

The external camera beside rocks and buildings (REC1, REC2, SUR1) is the known issue in KNOWN_ISSUES.md.
