#!/bin/sh
# KidsOS settings inside Onion's Apps, for parents who left the kids screen with SELECT + START.
# Uses Onion's own prompt and infoPanel, like Onion's scripts do. prompt exits with the index of the
# chosen line, 255 when the parent backs out.
settings=/mnt/SDCARD/kids/settings.txt
coins=/mnt/SDCARD/kids/coins.txt
# while this file exists the MainUI shim runs Onion's real MainUI; without it, the next MainUI start
# that Onion's runtime.sh does after this app ends is the kids screen again
exit_flag=/tmp/kidsos_exit
values="0 3 5 10 15 20 30"
system_json=/mnt/SDCARD/system.json
config=/mnt/SDCARD/.tmp_update/config
# Onion's idle timer in minutes (0 = never), see write_idle_shutdown in install.sh
idle_values="1 2 3 5 10 15 30 0"

ask() {
	LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so /mnt/SDCARD/.tmp_update/bin/prompt "$@"
}

say() {
	LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so /mnt/SDCARD/.tmp_update/bin/infoPanel -t "KidsOS" -m "$1" --auto
}

if [ ! -f "$settings" ]; then
	mkdir -p "$(dirname "$settings")"
	cat > "$settings" << 'EOS'
# Kids OS settings. Lines are key=value.
# Arcade coins the kid may insert per day (0 = unlimited).
arcade_coins_per_day=10
# Systems whose games use coins, by folder tag.
arcade_systems=ARCADE,CPS1,CPS2,CPS3,NEOGEO,FBN,FBNEO,MAME
# SSH into the console over Wi-Fi (root, no password, like Onion). 0 turns it off.
ssh=1
EOS
fi

current() {
	v=$(sed -n 's/^arcade_coins_per_day=//p' "$settings" | head -1)
	echo "${v:-10}"
}

idle_current() {
	v=$(sed -n 's/.*"hibernate":[[:space:]]*\([0-9]*\).*/\1/p' "$system_json" | head -1)
	echo "${v:-5}"
}

# keymon keeps system.json in memory, so it is told to reload; the per-device mirror is what survives a shutdown
set_idle() {
	for json in "$system_json" "$config"/system/*.json; do
		[ -f "$json" ] || continue
		sed -i "s#\"hibernate\":[[:space:]]*[^,}]*#\"hibernate\":\t$1#" "$json"
	done
	touch /tmp/settings_changed
	sync
}

set_coins() {
	if grep -q '^arcade_coins_per_day=' "$settings"; then
		sed -i "s/^arcade_coins_per_day=.*/arcade_coins_per_day=$1/" "$settings"
	else
		echo "arcade_coins_per_day=$1" >> "$settings"
	fi
	rm -f "$coins"
	sync
}

while :; do
	now=$(current)
	if [ "$now" -gt 0 ] 2> /dev/null; then
		label="Arcade coins per day: $now"
	else
		label="Arcade coins: no limit"
	fi
	idle=$(idle_current)
	if [ "$idle" -gt 0 ] 2> /dev/null; then
		label="$label. Switches off after $idle min idle"
	else
		label="$label. Never switches off by itself"
	fi
	ask -t "KidsOS" -m "$label" "Back to the kids screen" "Change the coins per day" "Give today's coins back" \
		"Change the idle switch-off" "Stay in Onion"
	case $? in
	0)
		rm -f "$exit_flag"
		sync
		exit 0
		;;
	1)
		i=0
		selected=0
		for v in $values; do
			[ "$v" = "$now" ] && selected=$i
			i=$((i + 1))
		done
		ask -t "Arcade coins per day" -s "$selected" "No limit" "3" "5" "10" "15" "20" "30"
		rc=$?
		[ "$rc" -eq 255 ] && continue
		i=0
		for v in $values; do
			if [ "$i" -eq "$rc" ]; then
				set_coins "$v"
				if [ "$v" -gt 0 ]; then say "$v coins a day from now on"; else say "No more coin limit"; fi
			fi
			i=$((i + 1))
		done
		;;
	2)
		rm -f "$coins"
		sync
		say "Today's coins are back"
		;;
	3)
		i=0
		selected=0
		for v in $idle_values; do
			[ "$v" = "$idle" ] && selected=$i
			i=$((i + 1))
		done
		ask -t "Switch off when idle" -s "$selected" "1 minute" "2 minutes" "3 minutes" "5 minutes" "10 minutes" "15 minutes" "30 minutes" "Never"
		rc=$?
		[ "$rc" -eq 255 ] && continue
		i=0
		for v in $idle_values; do
			if [ "$i" -eq "$rc" ]; then
				set_idle "$v"
				if [ "$v" -gt 0 ]; then say "Saves and switches off after $v min without a button press"; else say "No automatic switch-off"; fi
			fi
			i=$((i + 1))
		done
		;;
	*)
		exit 0
		;;
	esac
done
