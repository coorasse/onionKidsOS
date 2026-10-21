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
if [ -f "$CONFIG/.disableStandby.kidsos" ]; then
	rm -f "$CONFIG/.disableStandby" "$CONFIG/.disableStandby.kidsos" && echo "POWER back to standby"
	touch /tmp/settings_changed
fi
if [ -f "$CONFIG/hibernate-set" ]; then
	for json in /mnt/SDCARD/system.json "$CONFIG"/system/*.json; do
		[ -f "$json" ] || continue
		sed -i 's#"hibernate":[[:space:]]*[^,}]*#"hibernate":\t5#' "$json"
	done
	touch /tmp/settings_changed
	rm -f "$CONFIG/hibernate-set" && echo "idle shutdown back to Onion's default"
fi
if [ -f "$CONFIG/.sshState.kidsos" ]; then
	rm -f "$CONFIG/.sshState" "$CONFIG/.sshState_" "$CONFIG/.sshState.kidsos" && echo "SSH back to Onion's default"
fi
mw=$CONFIG/display/meterWidth
if [ -f "$mw.kidsos-orig" ]; then
	mv "$mw.kidsos-orig" "$mw"
elif [ -f "$mw.kidsos-none" ]; then
	rm -f "$mw" "$mw.kidsos-none"
fi
rm -f "$SYS/startup/kidsos.sh" && echo "startup hook removed"
rm -rf /mnt/SDCARD/App/KidsOS && echo "KidsOS app removed"
if [ -f "$CONFIG/theme-set" ]; then
	for json in /mnt/SDCARD/system.json "$CONFIG"/system/*.json; do
		[ -f "$json" ] || continue
		sed -i 's#"theme":[[:space:]]*"/mnt/SDCARD/Themes/KidsOS by coorasse/"#"theme":\t"/mnt/SDCARD/Themes/Default/"#' "$json"
	done
	rm -f "$CONFIG/theme-set"
fi
rm -rf "/mnt/SDCARD/Themes/KidsOS by coorasse" && echo "KidsOS theme removed"
rm -f /mnt/SDCARD/Saves/CurrentProfile/theme/skin/extra/bootScreen.png && echo "boot screen restored"
umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
rm -rf /mnt/SDCARD/.kidsos
sync
echo "uninstalled; reboot to get MainUI back"
