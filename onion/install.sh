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

# the settings app shows up in Onion's Apps tab (coins per day); copied only when something changed
write_app() {
	src="$KIDS/app/KidsOS"
	dst=/mnt/SDCARD/App/KidsOS
	[ -d "$src" ] || return 0
	if [ -d "$dst" ] && cmp -s "$src/launch.sh" "$dst/launch.sh" && cmp -s "$src/config.json" "$dst/config.json" &&
	   cmp -s "$src/icon.png" "$dst/icon.png"; then
		echo "KidsOS app already installed"
		return 0
	fi
	mkdir -p "$dst" && cp "$src/launch.sh" "$src/config.json" "$src/icon.png" "$dst/" || fail "cannot install the KidsOS app"
	chmod +x "$dst/launch.sh"
	echo "KidsOS app installed"
}

# The KidsOS theme (Onion's menu in the kids colours) is copied next to Onion's themes. Onion keeps the active
# theme in system.json and mirrors it in config/system/<device>.json at shutdown, so both get the key; only
# the first time, a parent who picked another theme in the Themes app keeps it. keymon holds system.json in
# memory and rewrites the whole file on every volume key, so it is told to reload it.
write_theme() {
	src="$KIDS/theme/KidsOS by coorasse"
	dst="/mnt/SDCARD/Themes/KidsOS by coorasse"
	[ -d "$src" ] || return 0
	if [ -d "$dst" ] && cmp -s "$src/config.json" "$dst/config.json" && cmp -s "$src/preview.png" "$dst/preview.png"; then
		echo "KidsOS theme already installed"
	else
		mkdir -p "$dst" && cp -R "$src/." "$dst/" || fail "cannot install the KidsOS theme"
		echo "KidsOS theme installed"
	fi
	[ -f "$CONFIG/theme-set" ] && return 0
	[ -f /mnt/SDCARD/system.json ] || return 0
	set_system theme '"/mnt/SDCARD/Themes/KidsOS by coorasse/"' || fail "cannot set the theme"
	touch "$CONFIG/theme-set" && echo "KidsOS theme set as default"
}

# $1 = key in system.json, $2 = its new value as JSON; written to system.json and its per-device mirrors
set_system() {
	for json in /mnt/SDCARD/system.json "$CONFIG"/system/*.json; do
		[ -f "$json" ] || continue
		sed -i "s#\"$1\":[[:space:]]*[^,}]*#\"$1\":\t$2#" "$json" || return 1
	done
	touch /tmp/settings_changed
}

# The card installer leaves the parent's choices in kids/setup.txt: the language and the Wi-Fi network. Both
# are applied once and the file goes, the Wi-Fi password with it. The network goes where Onion's own Wi-Fi
# settings keep it, on the console, and Onion's network checker turns Wi-Fi on when it sees the setting.
write_setup() {
	setup=/mnt/SDCARD/kids/setup.txt
	[ -f "$setup" ] || return 0
	[ -f /mnt/SDCARD/system.json ] || return 0
	lang=$(sed -n 's/^language=//p' "$setup" | head -n 1)
	ssid=$(sed -n 's/^wifi_name=//p' "$setup" | head -n 1)
	psk=$(sed -n 's/^wifi_password=//p' "$setup" | head -n 1)
	if [ -n "$lang" ]; then
		set_system language "\"$lang.lang\"" || fail "cannot set the language"
		echo "language set to $lang"
	fi
	if [ -n "$ssid" ]; then
		conf=/appconfigs/wpa_supplicant.conf
		[ -f "$conf" ] || printf 'ctrl_interface=/var/run/wpa_supplicant\nupdate_config=1\n' > "$conf"
		if grep -qF "ssid=\"$ssid\"" "$conf"; then
			echo "Wi-Fi network already known"
		elif [ -n "$psk" ]; then
			printf '\nnetwork={\n\tssid="%s"\n\tpsk="%s"\n}\n' "$ssid" "$psk" >> "$conf" ||
				fail "cannot add the Wi-Fi network"
			echo "Wi-Fi network added"
		else
			printf '\nnetwork={\n\tssid="%s"\n\tkey_mgmt=NONE\n}\n' "$ssid" >> "$conf" || fail "cannot add the Wi-Fi network"
			echo "open Wi-Fi network added"
		fi
		set_system wifi 1 || fail "cannot turn Wi-Fi on"
		net="$SYS/script/network/update_networking.sh"
		[ -f "$net" ] && sh "$net" check > /dev/null 2>&1 &
		echo "Wi-Fi on"
	fi
	rm -f "$setup" && echo "setup applied"
}

# Onion's bootScreen looks for the picture in Saves/CurrentProfile/theme/skin/extra/ first, then in the theme,
# then in .tmp_update/res/: the kids one goes to the first place so it wins over every theme. Onion's "Reset
# theme overrides" removes it and the next boot puts it back.
write_boot_screen() {
	ours="$KIDS/bootScreen.png"
	dir=/mnt/SDCARD/Saves/CurrentProfile/theme/skin/extra
	[ -f "$ours" ] || return 0
	# an earlier installer replaced .tmp_update/res/bootScreen.png and kept Onion's picture under config/
	[ -f "$CONFIG/bootScreen.png.kidsos-orig" ] && mv -f "$CONFIG/bootScreen.png.kidsos-orig" "$SYS/res/bootScreen.png"
	if cmp -s "$ours" "$dir/bootScreen.png"; then
		echo "boot screen already installed"
		return 0
	fi
	mkdir -p "$dir" && cp "$ours" "$dir/bootScreen.png" || fail "cannot install the boot screen"
	echo "boot screen installed"
}

# Saving is Onion's job: keymon ends a game (MENU, POWER, low battery) by asking RetroArch to quit, and RetroArch
# writes the auto save state on the way out only while savestate_auto_save is on. Onion ships with it on, but
# a parent can switch it off in RetroArch's own menu, so every boot puts it back. The auto load is what brings
# the game back when Onion relaunches it at power on. The resume screen shows RetroArch's own picture of the
# state, which Onion ships switched off; uninstall.sh only reverts that one, the other two are Onion's defaults.
write_retroarch_cfg() {
	cfg=/mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg
	: > /tmp/kidsos_ra.cfg
	for key in savestate_auto_save savestate_auto_load savestate_thumbnail_enable; do
		grep -q "^$key = \"true\"" "$cfg" 2> /dev/null || printf '%s = "true"\n' "$key" >> /tmp/kidsos_ra.cfg
	done
	if [ -s /tmp/kidsos_ra.cfg ]; then
		sh "$SYS/script/patch_ra_cfg.sh" /tmp/kidsos_ra.cfg > /dev/null &&
			echo "RetroArch set: $(tr '\n' ' ' < /tmp/kidsos_ra.cfg)"
	else
		echo "RetroArch auto save state and thumbnails already enabled"
	fi
	rm -f /tmp/kidsos_ra.cfg
}

# A POWER tap puts Onion in standby: the game is frozen, the screen is dark and the battery drains until the sleep
# timer saves and shuts down minutes later. For a kid, off must mean off. Onion has a setting for that (Tweaks >
# Power single press > Shutdown, the .disableStandby flag): a tap then saves the game and switches the console
# off at once. keymon keeps its settings in memory and rewrites the flags on every volume key, so it is told to
# reload them. A flag the parent had set before is left alone by uninstall.sh.
write_power_button() {
	flag="$CONFIG/.disableStandby"
	if [ -f "$flag" ]; then
		echo "POWER already set to shutdown"
		return 0
	fi
	touch "$flag" "$flag.kidsos" || fail "cannot set the power button"
	touch /tmp/settings_changed
	echo "POWER set to save and shutdown"
}

# A console forgotten by a kid must not run the battery flat. Onion has the timer already: "hibernate" in
# system.json, the minutes without a button press after which the screen goes dark and, a minute later, the same
# save-and-shutdown as a POWER tap happens (not while charging). Onion's default is 5; the first install sets
# 2 and a value the parent chooses later, in the KidsOS app or Onion's Tweaks, stays.
write_idle_shutdown() {
	[ -f "$CONFIG/hibernate-set" ] && return 0
	[ -f /mnt/SDCARD/system.json ] || return 0
	set_system hibernate 2 || fail "cannot set the idle shutdown"
	touch "$CONFIG/hibernate-set" && echo "idle shutdown set to 2 minutes"
}

# SSH is Onion's dropbear, root without a password as Onion ships it, switched by the flag .sshState. Onion's
# network check moves the flag aside at every boot with Wi-Fi off and runs before the startup hooks, so the
# flag is put back on every boot: SSH then comes up as soon as Wi-Fi is on. With Wi-Fi already on the service
# is started right away, Onion's check having run too early to see the flag. ssh=0 in kids/settings.txt leaves
# SSH to Onion's Tweaks.
write_ssh() {
	flag="$CONFIG/.sshState"
	if grep -q '^ssh=0' /mnt/SDCARD/kids/settings.txt 2> /dev/null; then
		echo "SSH left to Onion (ssh=0)"
		return 0
	fi
	if [ -f "$flag" ]; then
		echo "SSH already on"
		return 0
	fi
	touch "$flag" "$flag.kidsos" || fail "cannot set the SSH flag"
	rm -f "${flag}_"
	echo "SSH on"
	net="$SYS/script/network/update_networking.sh"
	if [ "$(/customer/app/jsonval wifi 2> /dev/null)" = "1" ] && [ -f "$net" ]; then
		sh "$net" ssh toggle > /dev/null 2>&1 &
		echo "SSH started"
	fi
}

[ -f "$RT" ] || fail "runtime.sh not found, is this Onion?"
chmod +x "$KIDS/launch.sh" "$KIDS/game.sh" "$KIDS/covers.sh" "$KIDS/startup.sh" "$KIDS/kidsos.elf" "$KIDS/coinmon.elf"

# an earlier version of this installer patched runtime.sh itself
if grep -q "$MARK" "$RT"; then
	[ -f "$RT.orig" ] || fail "runtime.sh is patched but has no .orig to restore"
	sh -n "$RT.orig" || fail "runtime.sh.orig is broken"
	cp "$RT.orig" "$RT" && echo "runtime.sh restored from .orig"
fi
sh -n "$RT" || fail "runtime.sh has a syntax error, restore it first"

# the launcher exits 0 after writing /tmp/cmd_to_run.sh like MainUI does; it only falls through to the
# real MainUI when it asked for that itself (parent menu "Exit to Onion") or crashed (launch.sh sets the flag)
for ui in MainUI-283-clean MainUI-283-expert MainUI-285-clean MainUI-285-expert MainUI-354-clean MainUI-354-expert; do
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
write_app
write_theme
write_setup
write_boot_screen
write_retroarch_cfg
write_power_button
write_idle_shutdown
write_ssh
# Onion bind-mounts the MainUI binary once per boot; drop the mount so the shim is picked up without a reboot
umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
sync
echo "installed; reboot to start in kids mode"
