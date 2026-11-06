#!/bin/bash
# Builds a DOSBox reference install of Mercenaries' DOS sim (MW2.EXE) from a Mercs-game folder - the user's own
# data, written to WORKDIR and never distributed. docs/DOSBOX_COMPARISON.md explains each step.
#   scripts/dosbox/setup_mercs_dosbox.sh /path/to/Mercs-game WORKDIR [SCENE] [VESA480.DLL|VESA768.DLL]
# needs: dosbox, genisoimage, and prjtool (make prjtool) in the port's folder
set -e
G=$(cd "$1" && pwd); mkdir -p "$2"; W=$(cd "$2" && pwd); D=$W/mercsdos; SCENE=${3:-BET1SCN1}; VFX=$4
PORT=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$D/VFX" "$D/GIDDI" "$D/MEK"
cp -r "$G"/cd/MERCS/* "$D"/                       # the DOS sim's files, as the installer copies them
cp "$G"/cd/VFX/* "$D"/; cp "$G"/cd/VFX/* "$D"/VFX/ # VFX video drivers (fatal #4D without them)
cp "$G"/cd/GIDDI/* "$D"/GIDDI/
cp "$G"/install/retail/USERSTAR.BWD "$G"/install/retail/MW2DIF.CFG "$G"/install/retail/MW2SND.CFG "$D"/
# USERSTAR's player mech is mek\PLY00MER.MEK, normally written by the shell's Mech Lab: the stock Commando
"$PORT"/prjtool extract "$D"/MW2.PRJ MEK COM00STD "$D"/MEK/PLY00MER.MEK
# The CD: MW2.EXE 0x20620 wants \OLD_MERC.DRV as a HIDDEN file on the MSCDEX drive (attribute bit 2) and 0x533cc
# wants CD audio tracks: an ISO with the hidden flag plus the user's MP3 rips as tracks 2-22
genisoimage -quiet -V MERCS -iso-level 1 -hidden OLD_MERC.DRV -o "$W"/mercs.iso "$G"/cd
{ echo "FILE \"$W/mercs.iso\" BINARY"; echo '  TRACK 01 MODE1/2048'; echo '    INDEX 01 00:00:00'
  for i in $(seq 2 22); do n=$(printf %02d "$i"); echo "FILE \"$G/music/Track$n.mp3\" MP3"; echo "  TRACK $n AUDIO"; echo '    INDEX 01 00:00:00'; done; } > "$W"/mercs.cue
cat > "$W"/mercs.conf <<CONF
[sdl]
fullscreen=false
output=surface
autolock=false
sensitivity=100
[dosbox]
machine=svga_s3
memsize=32
[cpu]
core=dynamic
cycles=max
[mixer]
nosound=true
[sblaster]
sbtype=none
[gus]
gus=false
[speaker]
pcspeaker=false
[autoexec]
mount c $D
imgmount d $W/mercs.cue -t iso
c:
cd \\
MW2 $SCENE${VFX:+ -VG:$VFX}
CONF
echo "DOSBox conf: $W/mercs.conf  (run: DOSCONF=$W/mercs.conf scripts/dosbox/dosrun.sh steps.ds)"
