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
- **When the mission is up.** About 30 s of wall time covers the boot and the load. DOSBox runs slower than real
  time, so match frames by game events, never by wall seconds.
- **Useful keys:** `c` switches to the external view, which shows the player's mech. `Esc` opens the main menu;
  its option 4 is Combat Variables.
- **Run one at a time.** Runs share display :99, so always use `flock`.

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
- **Ramp-3 texels in high colour ramps.** On a textured polygon whose colour ramp is 8-15 (the plates, 0x6cxx), the
  ramp-3 texels are drawn in the polygon's own ramp. This turns the grey 'c'/'b' plates brown. INFERRED from the
  pictures; the DOS span routine has not been traced.

**Still different (open):**

1. **Lighting level.** Mechs, the cockpit panels and the city buildings are darker in the port. The DOS mech's camo
   sits at the texels' own indices (about 0xc8-0xcc); the port's sits 3-4 shade levels lower. The cockpit's
   lower-left panels are a single colour in DOS, but several darker shades in the port. The light formula
   (0x4fa4c: ambient + max(0, f × intensity >> 7) − fog, then level = (C >> 1) × light / 0x440) matches the port's.
   The suspect is the ambient term: 0x4f5c4 is fed `[light + 0x28] >> 16`, and how the LITE loader fills that
   field has not been traced.
2. **Mech shadows.** With Shadows On, DOS draws a dark, dithered shadow on the ground under each mech. The port
   draws none.
3. **Radar contact.** The DOS enemy blip is a 3 x 3 orange diamond; the port draws a 2 x 2 light dot.
4. **The building window grid.** It shows faintly in DOS and is black in the port. Probably the same cause as 1.
