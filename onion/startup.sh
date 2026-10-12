#!/bin/sh
# KIDSOS
# Onion runs every .tmp_update/startup/*.sh at boot, before its UI loop. Onion updates wipe .tmp_update/bin,
# so the kids installer runs on every boot and puts the shims back when they are missing; when everything is
# in place it changes nothing. An error here must never stop the boot.
[ -f /mnt/SDCARD/.kidsos/install.sh ] && sh /mnt/SDCARD/.kidsos/install.sh > /mnt/SDCARD/.kidsos/install.log 2>&1
exit 0
