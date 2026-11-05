# Setting up the Mercenaries game folder

[BatMahn/Mercs-Port](https://github.com/BatMahn/Mercs-Port) reads the original
**MechWarrior 2: Mercenaries** files from a separate game folder. The source
repository does not include the game archives, movies or soundtrack. Copy the
files from your own discs or disc images.

The layout below uses **`Mercs-game`** as the folder name. Keep it separate from
`Mercs-Port`, which contains the source code. An existing working `Mercs-game`
folder can be reused when updating the port.

## 1. Copy the retail CD

Create `Mercs-game/cd/` and copy the **entire contents of the original retail
Mercenaries CD** into it, preserving the folders and filenames. Copy the disc's
contents directly into `cd/`, so `PRODUCT.INI` is immediately inside `cd/`.

The important parts of a minimum retail setup look like this. The omitted
entries still come across when copying the whole disc:

```text
Mercs-game/
  cd/
    PRODUCT.INI
    MERCS/
      MW2.PRJ
      DATABASE.MW2
      MW2SHELL.EXE
      ...remaining files...
    DOS/
    RMG/
    SMK/
    SND/
    KEATING/
    ...remaining CD files and folders...
```

| File or folder | What it supplies |
| --- | --- |
| `cd/PRODUCT.INI` | Identifies the Mercenaries disc. The validated retail disc uses `ProductID=MercsW95` and contains both DOS and Windows files. |
| `cd/MERCS/MW2.PRJ` | The retail missions, models, textures and other game resources. |
| `cd/MERCS/DATABASE.MW2` | The original menus, artwork and menu audio. |
| `cd/MERCS/MW2SHELL.EXE` | Original DOS data tables used by procedural contracts, Instant Action and original information screens. The port reads this file as data; it does not run it. |
| `cd/RMG/` | Original procedural mission templates. Keep the whole folder. |
| `cd/SMK/`, `cd/SND/`, `cd/KEATING/` and the rest | Movies, sound and the remaining original resources. Keep the complete CD tree. |

The retail disc is the base for this guide even when adding the 3dfx or Titanium
looks below. Generated contracts and Instant Action require the recognized
**retail DOS** `MW2SHELL.EXE` and its `RMG/` templates in every graphics profile.
A different executable revision can be rejected as an unrecognized version.

Use Mercenaries files throughout. The original MechWarrior 2 and Ghost Bear's
Legacy archives are different games, even when a filename is also `MW2.PRJ`.
The Titanium disc's `INSTALL/` tree does not replace the retail `cd/` tree.

The inspected discs have these volume labels:

| Edition | Disc label | Destination |
| --- | --- | --- |
| Retail Mercenaries | `MERCENARIES` | The entire CD contents in `cd/`. |
| Dedicated Mercenaries 3dfx | `MERCS3DFX` | The graphics pair in `3d/`, as described below. |
| Mercenaries Titanium | `TITAN_MERCS` | The 16-bit files in `titanium/`, as described below. |

## 2. Let the port create its writable folders

**You do not need to run the old DOS or Windows installer.** On normal launch,
the port creates the installation directories it needs after validating the
game data. The game folder must be writable by your account.

| Automatically created location | Purpose |
| --- | --- |
| `install/retail/` | Writable DOS profile state and shared career/score storage. |
| `install/3dfx/` | Writable state for the dedicated 3dfx profile, created when needed. |
| `install/direct3d/` | Writable state for the Titanium Direct3D profile, created when needed. |
| `install/retail/MERCS-CAREERS/` | Native career slots, indices and resume companions. |

Mission preparation also writes owned loadouts under the selected overlay's
`MEK/` folder and its `USERSTAR.BWD` lance file. Original-format configuration
filenames retain their `MW2` prefix for compatibility.

Keep `install/` and its edition directories as ordinary writable directories.
The port rejects links that could redirect these writes into `cd/`. The copied
CD files are the read-only fallback; they are not the save destination.

## 3. Add the optional graphics editions

The retail setup is sufficient for the **DOS** look. Add the files below for
the other two looks; the port skips choices whose required data is unavailable.
These files come from your own **Mercenaries** editions and are not bundled in
this repository.

### Dedicated 3dfx edition

Create `Mercs-game/3d/` and copy this matching pair from the dedicated
**Mercenaries Enhanced 3Dfx Interactive Edition** disc:

| On the dedicated 3dfx disc | Destination |
| --- | --- |
| `MERCS/MW2.PRJ` | `Mercs-game/3d/models.prj` |
| `MERCS/SKYGND.PAR` | `Mercs-game/3d/skygnd.par` |

Use the lowercase destination names shown above, especially on Linux and other
case-sensitive filesystems. Mercs uses this edition's complete archive for its
missions, models and textures; a separate ATI `textures.prj` is not needed.

The **3dfx** label identifies this edition's appearance. Drawing still uses the
port's modern Metal, Vulkan or Direct3D 12 backend, so no original 3dfx card is
needed. The supplied dedicated edition's original engine used Direct3D.

A complete dedicated 3dfx CD is also recognized as a base `cd/` folder, but that
setup does not provide the DOS profile and its shell executable differs from
the retail DOS image required by procedural features. The retail-base layout
above keeps those features available while adding the dedicated graphics pair.

### Titanium Direct3D edition

Create `Mercs-game/titanium/` and copy these files from the **Mercenaries
Titanium Edition** disc's **`INSTALL/16BIT/`** folder:

| On the Titanium disc | Destination | Requirement |
| --- | --- | --- |
| `INSTALL/16BIT/MW2.PRJ` | `Mercs-game/titanium/MW2.PRJ` | Required for Titanium Direct3D. |
| `INSTALL/16BIT/SKYGND.PAR` | `Mercs-game/titanium/SKYGND.PAR` | Required; keep it paired with that archive. |
| `INSTALL/16BIT/PARTICLE.PID` | `Mercs-game/titanium/particle.PID` | Optional missile-smoke asset; include it for that effect. |

Use the 16-bit files from **Mercenaries**, not the original MW2 Titanium disc or
its 8-bit installation. The selected Mercs profile keeps its mission, model,
texture and environment data together; it is not a separate texture overlay.

Select the look through **Combat Variables → Graphics Options (G) → Renderer**,
or press **F10** and use **Game Look** in Video Options. A look chosen during a mission
applies to the next mission. All three looks use the same retail/DOS soundtrack.

### Extra folders from earlier handoffs

An existing game folder may also contain `titanium/8bit/`, `mt32/` or `ultrasnd/`.
They are not required for the current Mercs port. The Titanium Direct3D profile
uses the 16-bit files listed above. The active Mercs shell reads its audio from
`DATABASE.MW2`; it does not require MT-32 ROMs, SC-55 ROMs or GUS patches.

## 4. Add the CD soundtrack

The game can start without `music/`. For the complete soundtrack, rip the
**retail/DOS Mercenaries CD's 21 audio tracks** into that folder:

```text
Mercs-game/
  music/
    Track02.flac
    Track03.flac
    ...
    Track22.flac
```

**FLAC, MP3 and WAV are supported.** The extensions in the example can be
replaced with `.mp3` or `.wav`. Keep one file for each track number and preserve
the original numbering: **track 01 is data; the music starts at track 02**.
Do not renumber the first audio file to `Track01`.

DOS, dedicated 3dfx and Titanium Direct3D all use this same `music/` directory.
Keep the full retail/DOS soundtrack when switching graphics editions; the
Titanium soundtrack is not a replacement for it. An ordinary data-only ISO
contains the CD files but not the audio tracks.

## 5. If your copy is a BIN/CUE image

The included `cdrip` tool can split a mixed-mode BIN/CUE rip into a data ISO and
numbered WAV files. Keep the CUE and the BIN files it references together. From
the source directory, build the tool and use a fresh output directory:

```sh
make cdrip
./cdrip "/path/to/Mercenaries.cue" "/path/to/Mercs-rip"
```

The parent of the output directory must already exist. The tool creates:

| Output | Next step |
| --- | --- |
| `Mercs-rip/mw2cd.iso` | Mount or extract it, then copy its **contents** into `Mercs-game/cd/`. The tool retains this older output filename. |
| `Mercs-rip/music/Track02.wav` through `Track22.wav` | Copy these files into `Mercs-game/music/`. |

The normal Mercs launcher uses the **extracted `cd/` directory**. Selecting an
ISO or BIN/CUE file directly, or merely placing it in `cd/`, does not prepare a
game folder. If you already have extracted CD files and numbered music rips,
use those directly and skip this step.

## 6. Launch and choose the folder

On macOS, build with `make app` and open **Mercenaries.app** in Finder. See
[BUILDING.md](BUILDING.md) for other platforms and build dependencies.

The app reuses a remembered valid folder or looks for `Mercs-game` beside the
application, one level above it, or in your home folder. Otherwise it opens the
system folder chooser. Select the **outer `Mercs-game` folder** containing
`cd/`, or select its parent when that parent contains a child named `Mercs-game`.
The location is remembered. A directly selected game folder can have another
name; `Mercs-game` is the convention used for automatic discovery.

Normal play enters the original intro, registration and assignment menus.
No mission name or game-path argument is required. On Linux, the folder chooser
uses `zenity` or `kdialog`; placing the game folder at a discovery location also
avoids needing a chooser. On macOS, if a temporary app location prevents nearby
folder discovery, choose the game folder explicitly or move the app with Finder.

## Saves and settings

Back up the **complete `install/retail/MERCS-CAREERS/` directory**, including its
indices and generation/resume companions. The native career is shared across
the three graphics profiles. A cold launch shows the main menu; use the original
in-game Load/Save screen to restore a chosen slot.

To import an original **DOS or retail Windows** career, copy its complete save
to `install/retail/mersave0.sav` through `mersave9.sav`, then select that slot in
the in-game Load/Save screen. If it differs from a native slot, the game asks
which version to load. A successful explicit Save also exports the slot in its
retained original format. Keep a backup of the original until it has loaded.

Display, movie and game-folder preferences are stored separately:

| OS | Port settings file |
| --- | --- |
| macOS | `~/Library/Application Support/mercsport/mercsport.cfg` |
| Linux | `$XDG_CONFIG_HOME/mercsport/mercsport.cfg`, otherwise `~/.config/mercsport/mercsport.cfg` |
| Windows | `%APPDATA%/mercsport/mercsport.cfg`, with `%USERPROFILE%` as the fallback base |

`MERCS_CONFIG=/absolute/path/to/file` selects an alternate settings file.
The per-edition `MERCS.LCK` file coordinates simultaneous launches; it can remain
on disk after the active lock is released.

## Common setup problems

| Symptom | Check |
| --- | --- |
| Folder is rejected | Select the outer folder. Check for `cd/PRODUCT.INI` and `cd/MERCS/MW2.PRJ` directly below it, without another enclosing disc folder. Use Mercenaries files. |
| Missing or invalid menu data | Restore the complete retail CD copy, including `cd/MERCS/DATABASE.MW2`. |
| Generated contracts or Instant Action are unavailable | Check the recognized retail DOS `cd/MERCS/MW2SHELL.EXE` and the complete `cd/RMG/` templates. A different executable revision is rejected. |
| 3dfx or Titanium cannot be selected | Check the matching archive and sky file for that Mercenaries edition and the destination spellings above. |
| No CD soundtrack | Check `music/`, supported formats and original `Track02`–`Track22` numbering. Avoid duplicate files for a track. |
| Titanium missile smoke is missing | Add its matching `titanium/particle.PID`. |
| Saves or installation state cannot be written | Make the game folder writable and keep `install/` and its edition directories as ordinary directories. |

For the current graphics options and remaining game issues, see
[README.md](README.md) and [KNOWN_ISSUES.md](KNOWN_ISSUES.md). The reference disc
identities and edition comparisons are recorded in
the visual-edition notes.
