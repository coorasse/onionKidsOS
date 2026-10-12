#!/bin/sh
# Runs on the device. Puts the kids launcher in place of Onion's MainUI and GameSwitcher without touching
# runtime.sh: Onion starts both from files in .tmp_update/bin, so each is replaced by a tiny shell shim and
# the original is kept next to it as <name>.real. A broken shim can at worst leave the screen empty after
# boot; Wi-Fi and SSH are started by runtime.sh before it ever launches the UI.
SYS=/mnt/SDCARD/.tmp_update
BIN=$SYS/bin
CONFIG=$SYS/config
KIDS=/mnt/SDCARD/.kidsos
RT=$SYS/runtime.sh
MARK="# KIDSOS"

fail() {
	echo "install failed: $1"
	exit 1
}

# $1 = file name in $BIN, $2 = shell lines run while kids mode is active
write_shim() {
	target="$BIN/$1"
	[ -f "$target" ] || return 0
	cat > "$target.new" << EOS
#!/bin/sh
$MARK
if [ -f $KIDS/launch.sh ] && [ ! -f /tmp/kidsos_exit ]; then
$2
fi
exec $target.real "\$@"
EOS
	chmod +x "$target.new"
	sh -n "$target.new" || { rm -f "$target.new"; fail "shim for $1 has a syntax error"; }
	if grep -q "$MARK" "$target"; then
		if cmp -s "$target" "$target.new"; then
			rm -f "$target.new"
			echo "$1 already shimmed"
			return 0
		fi
		[ -f "$target.real" ] || { rm -f "$target.new"; fail "$1 is shimmed but $1.real is missing"; }
	else
		cp "$target" "$target.real" || fail "cannot back up $1"
	fi
	mv "$target.new" "$target" || fail "cannot install shim for $1"
	echo "$1 shimmed"
}

# $1 = key, $2 = default: integer value of a key in Onion's keymap.json
keymap_value() {
	v=$(grep -o "\"$1\": *[0-9]*" "$CONFIG/keymap.json" 2> /dev/null | grep -o '[0-9]*$')
	echo "${v:-$2}"
}

# $1 = key: string value of a key in Onion's keymap.json
keymap_string() {
	grep -o "\"$1\": *\"[^\"]*\"" "$CONFIG/keymap.json" 2> /dev/null | sed 's/.*: *"//; s/"$//'
}

# Onion's keymon decides what the MENU button does inside a game. Its default single press pauses RetroArch and
# shows the GameSwitcher overlay; in kids mode MENU must save and quit the game so Onion comes back to the kids
# UI. The MainUI actions are kept as they are, the original file comes back with uninstall.sh.
write_keymap() {
	if [ ! -f "$CONFIG/keymap.json.kidsos-orig" ] && [ ! -f "$CONFIG/keymap.json.kidsos-none" ]; then
		if [ -f "$CONFIG/keymap.json" ]; then
			cp "$CONFIG/keymap.json" "$CONFIG/keymap.json.kidsos-orig" || fail "cannot back up keymap.json"
		else
			touch "$CONFIG/keymap.json.kidsos-none"
		fi
	fi
	cat > "$CONFIG/keymap.json.new" << EOS
{
    "mainui_single_press": $(keymap_value mainui_single_press 1),
    "mainui_long_press": $(keymap_value mainui_long_press 0),
    "mainui_double_press": $(keymap_value mainui_double_press 2),
    "ingame_single_press": 2,
    "ingame_long_press": 0,
    "ingame_double_press": 0,
    "mainui_button_x": "$(keymap_string mainui_button_x)",
    "mainui_button_y": "$(keymap_string mainui_button_y)"
}
EOS
	if cmp -s "$CONFIG/keymap.json" "$CONFIG/keymap.json.new"; then
		rm -f "$CONFIG/keymap.json.new"
		echo "keymap already set"
	else
		mv "$CONFIG/keymap.json.new" "$CONFIG/keymap.json" || fail "cannot write keymap.json"
		touch /tmp/settings_changed
		echo "keymap set: MENU in a game saves and returns to the kids UI"
	fi
}

# Onion runs .tmp_update/startup/*.sh at every boot and its updates wipe .tmp_update/bin: the hook runs this
# installer again so the shims come back by themselves after an Onion update
write_startup_hook() {
	hook="$SYS/startup/kidsos.sh"
	mkdir -p "$SYS/startup"
	if cmp -s "$KIDS/startup.sh" "$hook"; then
		echo "startup hook already installed"
	else
		cp "$KIDS/startup.sh" "$hook" || fail "cannot install the startup hook"
		echo "startup hook installed"
	fi
}

[ -f "$RT" ] || fail "runtime.sh not found, is this Onion?"
chmod +x "$KIDS/launch.sh" "$KIDS/game.sh" "$KIDS/startup.sh" "$KIDS/kidsos.elf" "$KIDS/coinmon.elf"

# an earlier version of this installer patched runtime.sh itself
if grep -q "$MARK" "$RT"; then
	[ -f "$RT.orig" ] || fail "runtime.sh is patched but has no .orig to restore"
	sh -n "$RT.orig" || fail "runtime.sh.orig is broken"
	cp "$RT.orig" "$RT" && echo "runtime.sh restored from .orig"
fi
sh -n "$RT" || fail "runtime.sh has a syntax error, restore it first"

# the launcher exits 0 after writing /tmp/cmd_to_run.sh like MainUI does; it only falls through to the
# real MainUI when it asked for that itself (parent menu "Exit to Onion") or crashed (launch.sh sets the flag)
for ui in MainUI-283-clean MainUI-283-expert MainUI-354-clean MainUI-354-expert; do
	write_shim "$ui" "	$KIDS/launch.sh
	[ -f /tmp/kidsos_exit ] || exit 0"
done
# after a game Onion runs the GameSwitcher, which in kids mode must not appear: it drops cmd_to_run.sh like the
# real one does when the user leaves the game, otherwise Onion would relaunch it. The overlay variant is what
# keymon starts on top of a paused RetroArch; only the real one knows how to resume the game, so it runs.
write_shim gameSwitcher "	if [ \"\$1\" != \"--overlay\" ]; then
		rm -f $SYS/cmd_to_run.sh
		exit 0
	fi"
write_keymap
write_startup_hook
# the resume screen shows RetroArch's own picture of the auto save state; Onion ships with thumbnails off
if grep -q 'savestate_thumbnail_enable = "true"' /mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg 2> /dev/null; then
	echo "RetroArch save state thumbnails already enabled"
else
	printf 'savestate_thumbnail_enable = "true"\n' > /tmp/kidsos_ra.cfg
	sh "$SYS/script/patch_ra_cfg.sh" /tmp/kidsos_ra.cfg > /dev/null && echo "RetroArch save state thumbnails enabled"
	rm -f /tmp/kidsos_ra.cfg
fi
# Onion bind-mounts the MainUI binary once per boot; drop the mount so the shim is picked up without a reboot
umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
sync
echo "installed; reboot to start in kids mode"
