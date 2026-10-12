#!/bin/sh
# Wraps an arcade game launch in kids mode. Called by Onion's runtime.sh as:
#   game.sh "<Emu/<TAG>/launch.sh>" "<rom>"
# coinmon counts the coins (SELECT presses) and, once the daily coins are spent, raises a flag; the
# coinlock library preloaded into RetroArch then hides SELECT from the game, which goes on with the
# credits it has. Onion already preloads libpadsp.so, the two are listed together.
dir=/mnt/SDCARD/.kidsos
rm -f /tmp/kidsos_no_coins
export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD }$dir/libcoinlock.so"
"$dir/coinmon.elf" /mnt/SDCARD >> "$dir/coinmon.log" 2>&1 &
mon=$!
"$1" "$2"
rc=$?
kill $mon 2> /dev/null
rm -f /tmp/kidsos_no_coins
exit $rc
