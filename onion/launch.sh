#!/bin/sh
# Started by the MainUI shim that install.sh puts in Onion's .tmp_update/bin while kids mode is active.
dir=/mnt/SDCARD/.kidsos
cd "$dir"
# the library order Onion's runtime.sh uses for its own SDL apps (tweaks, gameSwitcher)
export LD_LIBRARY_PATH="/lib:/config/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte"
# one log per run, the previous one is kept for when the launcher never came back
# keymon paints its own thin volume bar over whatever is on screen, which flickers under a UI that redraws;
# the launcher shows its own and keymon's width is set to 0 while it runs (keymon reads it on every key)
mw=/mnt/SDCARD/.tmp_update/config/display/meterWidth
if [ ! -f "$mw.kidsos-orig" ] && [ ! -f "$mw.kidsos-none" ]; then
	if [ -f "$mw" ]; then cp "$mw" "$mw.kidsos-orig"; else touch "$mw.kidsos-none"; fi
fi
echo 0 > "$mw"
# box art for favourites that have none, over Wi-Fi; the script exits at once when there is nothing to do
sh "$dir/covers.sh" > /dev/null 2>&1 &
mv -f "$dir/kidsos.log" "$dir/kidsos.log.1" 2> /dev/null
echo "kidsos started $(date)" > "$dir/kidsos.log"
./kidsos.elf >> "$dir/kidsos.log" 2>&1
rc=$?
if [ -f "$mw.kidsos-orig" ]; then cp "$mw.kidsos-orig" "$mw"; else rm -f "$mw" "$mw.kidsos-none"; fi
echo "kidsos.elf exited with $rc $(date)" >> "$dir/kidsos.log"
# a crashing launcher must never lock the parent out: fall back to MainUI until the next reboot
if [ "$rc" != "0" ] && [ ! -f /tmp/cmd_to_run.sh ]; then
	echo "falling back to MainUI" >> "$dir/kidsos.log"
	touch /tmp/kidsos_exit
fi
