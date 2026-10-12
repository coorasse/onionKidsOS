#!/bin/sh
# Kids.pak: a launcher for small children. Runs until the parent chooses "Exit to MinUI".
# Games are started the same way MinUI starts them: kidsos.elf writes the command to
# /tmp/next and exits, this loop runs it and brings the kids launcher back afterwards.
cd "$(dirname "$0")"

NEXT_PATH=/tmp/next
EXIT_PATH=/tmp/kids_exit
LOG="${LOGS_PATH:-/tmp}/kids.txt"
rm -f "$EXIT_PATH"

while [ ! -f "$EXIT_PATH" ]; do
	[ -n "$CPU_SPEED_GAME" ] && overclock.elf $CPU_SPEED_GAME
	./kidsos.elf > "$LOG" 2>&1
	RC=$?
	sync
	if [ -f "$NEXT_PATH" ]; then
		CMD=$(cat "$NEXT_PATH")
		rm -f "$NEXT_PATH"
		eval $CMD
		sync
	elif [ "$RC" != "0" ]; then
		break
	fi
done
rm -f "$EXIT_PATH"
