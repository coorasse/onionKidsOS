#!/bin/sh
# Runs on the device. Puts Onion's original MainUI, GameSwitcher and keymap back and removes the launcher.
SYS=/mnt/SDCARD/.tmp_update
BIN=$SYS/bin
CONFIG=$SYS/config
for real in "$BIN"/*.real; do
	[ -f "$real" ] || continue
	mv "$real" "${real%.real}" && echo "$(basename "${real%.real}") restored"
done
if [ -f "$CONFIG/keymap.json.kidsos-orig" ]; then
	mv "$CONFIG/keymap.json.kidsos-orig" "$CONFIG/keymap.json" && echo "keymap.json restored"
	touch /tmp/settings_changed
elif [ -f "$CONFIG/keymap.json.kidsos-none" ]; then
	rm -f "$CONFIG/keymap.json" "$CONFIG/keymap.json.kidsos-none" && echo "keymap.json removed"
	touch /tmp/settings_changed
fi
printf 'savestate_thumbnail_enable = "false"\n' > /tmp/kidsos_ra.cfg
sh "$SYS/script/patch_ra_cfg.sh" /tmp/kidsos_ra.cfg > /dev/null && echo "RetroArch thumbnails back to default"
rm -f /tmp/kidsos_ra.cfg
mw=$CONFIG/display/meterWidth
if [ -f "$mw.kidsos-orig" ]; then mv "$mw.kidsos-orig" "$mw"; elif [ -f "$mw.kidsos-none" ]; then rm -f "$mw" "$mw.kidsos-none"; fi
for orig in /mnt/SDCARD/Saves/CurrentProfile/config/*/*.cfg.kidsos-orig; do
	[ -f "$orig" ] && mv -f "$orig" "${orig%.kidsos-orig}"
done
for none in /mnt/SDCARD/Saves/CurrentProfile/config/*/*.cfg.kidsos-none; do
	[ -f "$none" ] && rm -f "$none" "${none%.kidsos-none}"
done
rm -f "$SYS/startup/kidsos.sh" && echo "startup hook removed"
umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
rm -rf /mnt/SDCARD/.kidsos
sync
echo "uninstalled; reboot to get MainUI back"
