#!/bin/bash
# usage: dosrun.sh script   (sleep N | key K | down K | up K | type TEXT | shot NAME)
export DISPLAY=:99
Xvfb :99 -screen 0 1280x1024x24 >/tmp/mercs-xvfb.log 2>&1 &
XP=$!
sleep 2
SDL_VIDEO_WINDOW_POS=0,0 SDL_AUDIODRIVER=dummy dosbox -conf ${DOSCONF:?set DOSCONF to the conf setup_mercs_dosbox.sh wrote} >/tmp/mercs-dosbox.log 2>&1 &
DP=$!
sleep 3
W=$(xdotool search --name DOSBox | head -1)
xset m 1 1 2>/dev/null; xset m 1/1 0 2>/dev/null
while read -r cmd a b; do
  case "$cmd" in
    sleep) sleep "$a" ;;
    key) xdotool key --window "$W" "$a" ;;
    down) xdotool keydown --window "$W" "$a" ;;
    up) xdotool keyup --window "$W" "$a" ;;
    type) xdotool type --delay 60 --window "$W" "$a" ;;
    shot) import -window root "${SHOTDIR:-.}/ds_$a.png" ;;
  esac
done < "$1"
kill $DP $XP 2>/dev/null; wait 2>/dev/null
