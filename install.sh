#!/bin/sh
# Puts KidsOS on a microSD card. A card that already has KidsOS or Onion is updated in place, games and saves
# included; any other card is erased, formatted FAT32 and gets the release copied onto it. Mac and Linux;
# Windows has its own script.
#
#   curl -fsSL https://raw.githubusercontent.com/coorasse/onionKidsOS/main/install.sh | sh
#
# Options, after "sh -s --" when piped from curl:
#   --zip FILE   use this KidsOS zip instead of asking for a version and downloading it
#   --disk ID    the card (disk4 on a Mac, /dev/sdb or /dev/mmcblk0 on Linux) instead of looking for it
set -eu

REPO=coorasse/onionKidsOS
LABEL=KIDSOS
ZIP=
DISK=
MOUNT=
MODE=
LANGUAGE=
LANGUAGE_NAME=
WIFI_NAME=
WIFI_PASSWORD=

say() { printf '%s\n' "$*"; }
die() { printf '\n%s\n' "$*" >&2; exit 1; }
# stdin is the script itself when piped from curl, so questions are read from the terminal
ask() { printf '%s' "$1"; read -r REPLY </dev/tty || die "This needs a terminal to ask questions."; }
need() {
  for tool in "$@"; do command -v "$tool" >/dev/null 2>&1 || die "$tool is missing, install it and try again."; done
}
# runs a command with a spinner after the message; what the command prints lands in $SPIN_OUT. The command
# runs in the background, where Ctrl-C does not reach it, so the trap below ends it.
SPIN_OUT=$(mktemp)
SPIN_PID=
cleanup() {
  [ -n "$SPIN_PID" ] && kill "$SPIN_PID" 2>/dev/null
  stty echo </dev/tty 2>/dev/null || true
  rm -f "$SPIN_OUT"
}
trap cleanup EXIT
trap 'printf "\n\nStopped.\n"; exit 130' INT
spin() {
  msg=$1
  shift
  "$@" >"$SPIN_OUT" &
  SPIN_PID=$!
  i=0
  while kill -0 "$SPIN_PID" 2>/dev/null; do
    i=$(((i + 1) % 4))
    printf '\r%s %s' "$msg" "$(printf '|/-\\' | cut -c$((i + 1)))"
    sleep 0.2
  done
  status=0
  wait "$SPIN_PID" || status=$?
  SPIN_PID=
  printf '\r%s  \n' "$msg"
  return $status
}

banner() {
  cat <<'EOF'

  _  __ _     _       ___  ____
 | |/ /(_) __| |___  / _ \/ ___|
 | ' / | |/ _` / __|| | | \___ \
 | . \ | | (_| \__ \| |_| |___) |
 |_|\_\|_|\__,_|___/ \___/|____/

 Card setup. Three steps: get KidsOS, prepare the card, copy it over.
 Ctrl-C stops it at any time.

EOF
}

while [ $# -gt 0 ]; do
  case $1 in
    --zip) ZIP=$2; shift 2 ;;
    --disk) DISK=$2; shift 2 ;;
    -h | --help) sed -n '2,10s/^# \{0,1\}//p' "$0" 2>/dev/null; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

case $(uname -s) in
  Darwin) OS=mac ;;
  Linux) OS=linux ;;
  *) die "This script runs on Mac and Linux. On Windows use install.ps1." ;;
esac

# ---- the card: one "id|size|name" line per removable disk ----

mac_cards() {
  diskutil list physical | sed -n 's|^/dev/\(disk[0-9]*\) .*|\1|p' | while read -r id; do
    info=$(diskutil info "$id")
    printf '%s\n' "$info" | grep -q 'Removable Media: *Removable' || continue
    size=$(printf '%s\n' "$info" | sed -n 's/^ *Disk Size: *\([0-9.]* [A-Z]*\).*/\1/p')
    name=$(printf '%s\n' "$info" | sed -n 's|^ *Device / Media Name: *||p')
    printf '%s|%s|%s\n' "$id" "$size" "$name"
  done
}

linux_cards() {
  lsblk -dnPo NAME,SIZE,RM,TRAN,MODEL | while read -r line; do
    eval "$line"
    case $NAME in
      /dev/mmcblk*) ;;
      *) [ "$RM" = 1 ] || continue ;;
    esac
    printf '%s|%s|%s\n' "$NAME" "$SIZE" "${MODEL:-$TRAN}"
  done
}

# polls until a card shows up, then prints the list like ${OS}_cards
card_appears() {
  while :; do
    found=$(${OS}_cards)
    if [ -n "$found" ]; then
      sleep 2 # give the system a moment to mount it
      printf '%s\n' "$found"
      return 0
    fi
    sleep 2
  done
}

wait_for_card() {
  cat <<'EOF'

 No memory card found. Put the microSD card in the reader:

      _______
     |  ___  |_
     | |   |   |
     | | S |   |      ===>      [ card reader ]
     | | D |   |
     | |___|   |
     |_________|

 Ctrl-C stops the installer.

EOF
  spin "Waiting for the card..." card_appears
}

pick_card() {
  spin "Looking for the memory card..." ${OS}_cards
  cards=$(cat "$SPIN_OUT")
  if [ -z "$cards" ]; then
    wait_for_card
    cards=$(cat "$SPIN_OUT")
  fi
  count=$(printf '%s\n' "$cards" | wc -l | tr -d ' ')
  if [ "$count" = 1 ]; then
    DISK=${cards%%|*}
    return
  fi
  say "Found $count memory cards:"
  say ""
  printf '%s\n' "$cards" | awk -F'|' '{ printf "  %d) %s, %s, %s\n", NR, $1, $2, $3 }'
  say ""
  ask "Which one goes into the console? [1-$count] "
  case $REPLY in *[!0-9]* | '') die "Stopped, nothing was changed." ;; esac
  DISK=$(printf '%s\n' "$cards" | sed -n "${REPLY}p" | cut -d'|' -f1)
  [ -n "$DISK" ] || die "Stopped, nothing was changed."
}

# "size|reader|names of the volumes on it", for the line that describes the card
mac_describe() {
  info=$(diskutil info "$DISK")
  size=$(printf '%s\n' "$info" | sed -n 's/^ *Disk Size: *\([0-9.]* [A-Z]*\).*/\1/p')
  name=$(printf '%s\n' "$info" | sed -n 's|^ *Device / Media Name: *||p')
  labels=$(diskutil list "$DISK" | sed -n 's/.* \('"$DISK"'s[0-9]*\)$/\1/p' | while read -r part; do
    diskutil info "$part" | sed -n 's/^ *Volume Name: *//p' | grep -v '^Not applicable' || true
  done | paste -sd, - | sed 's/,/, /g')
  printf '%s|%s|%s\n' "$size" "$name" "$labels"
}

linux_describe() {
  eval "$(lsblk -dnPo SIZE,TRAN,MODEL "$DISK")"
  labels=$(lsblk -nro LABEL "$DISK" | grep . | paste -sd, - | sed 's/,/, /g')
  printf '%s|%s|%s\n' "$SIZE" "${MODEL:-$TRAN}" "$labels"
}

# where the card's first volume is mounted, mounting it when needed; empty when it cannot be read
mac_mount() {
  MOUNT=$(diskutil info "${DISK}s1" 2>/dev/null | sed -n 's/^ *Mount Point: *//p')
  [ -n "$MOUNT" ] && return 0
  diskutil mount "${DISK}s1" >/dev/null 2>&1 || return 0
  MOUNT=$(diskutil info "${DISK}s1" | sed -n 's/^ *Mount Point: *//p')
}

linux_part() {
  case $DISK in *[0-9]) echo "${DISK}p1" ;; *) echo "${DISK}1" ;; esac
}

linux_mount() {
  part=$(linux_part)
  [ -b "$part" ] || return 0
  MOUNT=$(lsblk -nro MOUNTPOINT "$part" | grep . | head -n 1)
  [ -n "$MOUNT" ] && return 0
  MOUNT=$(mktemp -d)
  sudo mount -o "uid=$(id -u),gid=$(id -g)" "$part" "$MOUNT" 2>/dev/null || { rmdir "$MOUNT"; MOUNT=; }
}

# what is on the card now: "kidsos <version line>", "onion <version>" or nothing
installed() {
  [ -n "$MOUNT" ] || return 0
  if [ -f "$MOUNT/kidsos_version.txt" ]; then
    echo "kidsos $(cat "$MOUNT/kidsos_version.txt")"
  elif [ -d "$MOUNT/.tmp_update/onionVersion" ]; then
    echo "onion $(cat "$MOUNT/.tmp_update/onionVersion/version.txt" 2>/dev/null || echo "")"
  fi
}

choose_mode() {
  spin "Looking at $DISK..." ${OS}_describe
  IFS='|' read -r size name labels <"$SPIN_OUT"
  [ -n "$size" ] || die "The card $DISK went away. Take it out, put it back and run this again."
  [ -n "$labels" ] && labels=", currently named \"$labels\""
  say "Found a memory card:"
  say ""
  say "  $DISK, $size, $name$labels"
  say ""
  ${OS}_mount
  found=$(installed)
  case $found in
    kidsos*)
      say "It already has ${found#kidsos }."
      update="Update it (games and saves are kept)"
      ;;
    onion*)
      say "It has Onion ${found#onion }, no KidsOS yet."
      update="Add KidsOS to it (games and saves are kept)"
      ;;
    *)
      say "Everything on it will be erased."
      ask "Type \"$DISK\" to go on, or just press Enter to stop: "
      [ "$REPLY" = "$DISK" ] || die "Stopped, nothing was changed."
      MODE=erase
      return
      ;;
  esac
  say ""
  say "  1) $update"
  say "  2) Erase it and start fresh"
  say "  3) Stop"
  say ""
  ask "What do you want? [1] "
  case $REPLY in
    '' | 1) MODE=update ;;
    2)
      ask "Type \"$DISK\" to erase it, or just press Enter to stop: "
      [ "$REPLY" = "$DISK" ] || die "Stopped, nothing was changed."
      MODE=erase
      ;;
    *) die "Stopped, nothing was changed." ;;
  esac
}

# Wi-Fi networks in reach, one per line, the one this computer is on first and marked with "*". macOS hides
# the names from a terminal without Location Services permission (they come out as <redacted>), then the list
# is empty and the name is typed.
mac_wifi_networks() {
  system_profiler SPAirPortDataType -detailLevel full 2>/dev/null | awk '
    /^        [a-z]/ { section = "" }
    /^          Current Network Information:/ { section = "current"; next }
    /^          Other Local Wi-Fi Networks:/ { section = "other"; next }
    section != "" && /^            [^ ].*:$/ {
      name = $0; sub(/^ */, "", name); sub(/:$/, "", name)
      if (name == "<redacted>" || seen[name]++) next
      if (section == "current") current = name; else others = others name "\n"
    }
    END { if (current != "") print "*" current; printf "%s", others }'
}

linux_wifi_networks() {
  command -v nmcli >/dev/null 2>&1 || return 0
  nmcli -t -f ACTIVE,SSID dev wifi list 2>/dev/null | awk -F: '
    $2 == "" || seen[$2]++ { next }
    $1 == "yes" { current = $2; next }
    { others = others $2 "\n" }
    END { if (current != "") print "*" current; printf "%s", others }'
}

ask_wifi() {
  say "With Wi-Fi the console downloads a picture for every game by itself."
  spin "Looking for Wi-Fi networks..." ${OS}_wifi_networks
  networks=$(cat "$SPIN_OUT")
  if [ -z "$networks" ]; then
    ask "Wi-Fi network name, or just press Enter for no Wi-Fi: "
    WIFI_NAME=$REPLY
  else
    count=$(printf '%s\n' "$networks" | wc -l | tr -d ' ')
    say ""
    printf '%s\n' "$networks" |
      awk '{ note = sub(/^\*/, "") ? "  (this computer is on it)" : ""; printf "  %d) %s%s\n", NR, $0, note }'
    say ""
    ask "Which one? A number, a name, or just press Enter for no Wi-Fi: "
    case $REPLY in
      '') WIFI_NAME= ;;
      *[!0-9]*) WIFI_NAME=$REPLY ;;
      *)
        WIFI_NAME=$(printf '%s\n' "$networks" | sed -n "${REPLY}p" | sed 's/^\*//')
        [ -n "$WIFI_NAME" ] || die "There is no network $REPLY in the list."
        ;;
    esac
  fi
  [ -n "$WIFI_NAME" ] || return 0
  printf 'Password for %s (not shown while typing): ' "$WIFI_NAME"
  stty -echo </dev/tty 2>/dev/null || true
  read -r WIFI_PASSWORD </dev/tty || die "This needs a terminal to ask questions."
  stty echo </dev/tty 2>/dev/null || true
  printf '\n'
}

# the choices a fresh card gets at its first start; applied on the console by the kids layer
ask_setup() {
  say ""
  say "Two questions for the console, so it is ready at the first start."
  say ""
  say "Language:  1) English  2) Deutsch  3) Italiano  4) Français  5) Español  6) Nederlands"
  ask "Which one? [1] "
  case $REPLY in
    '' | 1) LANGUAGE=en LANGUAGE_NAME=English ;;
    2) LANGUAGE=de LANGUAGE_NAME=Deutsch ;;
    3) LANGUAGE=it LANGUAGE_NAME=Italiano ;;
    4) LANGUAGE=fr LANGUAGE_NAME=Français ;;
    5) LANGUAGE=es LANGUAGE_NAME=Español ;;
    6) LANGUAGE=nl LANGUAGE_NAME=Nederlands ;;
    *) die "Stopped, nothing was changed." ;;
  esac
  say ""
  ask_wifi
}

write_setup() {
  mkdir -p "$MOUNT/kids"
  {
    echo "language=$LANGUAGE"
    if [ -n "$WIFI_NAME" ]; then
      echo "wifi_name=$WIFI_NAME"
      echo "wifi_password=$WIFI_PASSWORD"
    fi
  } >"$MOUNT/kids/setup.txt"
}

# ---- format, mount, eject ----

mac_format() {
  # the card was just mounted and read, and Spotlight may still be indexing it: eraseDisk only tries a polite
  # unmount and gives up with "Couldn't unmount disk", so the unmount is forced first and the erase retried
  tries=0
  until diskutil unmountDisk force "/dev/$DISK" >/dev/null 2>&1 &&
    diskutil eraseDisk FAT32 "$LABEL" MBRFormat "/dev/$DISK" >/dev/null 2>&1; do
    tries=$((tries + 1))
    [ $tries -lt 5 ] ||
      die "The card could not be formatted. Close any window showing it, take it out, put it back and try again."
    sleep 2
  done
  mount=
  tries=0
  while [ -z "$mount" ]; do
    mount=$(diskutil info "${DISK}s1" | sed -n 's/^ *Mount Point: *//p')
    [ -n "$mount" ] && break
    tries=$((tries + 1))
    [ $tries -lt 20 ] || die "The card did not come back after formatting. Take it out, put it back and try again."
    sleep 1
  done
  echo "$mount"
}

mac_eject() { diskutil eject "/dev/$DISK" >/dev/null; }

linux_format() {
  say "     Linux needs administrator rights to format the card."
  for mounted in $(lsblk -nro MOUNTPOINT "$DISK"); do sudo umount "$mounted"; done
  printf 'label: dos\ntype=c\n' | sudo sfdisk -q "$DISK"
  sleep 2
  sudo mkfs.vfat -F 32 -n "$LABEL" "$(linux_part)" >/dev/null
  mount=$(mktemp -d)
  sudo mount -o "uid=$(id -u),gid=$(id -g)" "$(linux_part)" "$mount"
  echo "$mount"
}

linux_eject() {
  sync
  sudo umount "$MOUNT"
  rmdir "$MOUNT" 2>/dev/null || true
  sudo eject "$DISK" 2>/dev/null || true
}

# ---- the release ----

# every release with a KidsOS zip, newest first, as "tag url" lines
releases() {
  curl -fsSL "https://api.github.com/repos/$REPO/releases?per_page=20" |
    sed -n 's/.*"tag_name": *"\([^"]*\)".*/tag \1/p
            s/.*"browser_download_url": *"\([^"]*\/KidsOS-[0-9][^"\/]*\.zip\)".*/url \1/p' |
    awk '$1 == "tag" { tag = $2; next } $1 == "url" && tag != "" { print tag, $2; tag = "" }'
}

pick_release() {
  spin "Looking for versions..." releases
  list=$(cat "$SPIN_OUT")
  [ -n "$list" ] || die "No release found at https://github.com/$REPO/releases"
  latest=$(printf '%s\n' "$list" | head -n 1 | cut -d' ' -f1)
  versions=$(printf '%s\n' "$list" |
    awk '{ v = $1; sub(/^v/, "", v); printf "%s%s%s", (NR > 1 ? ", " : ""), v, (NR == 1 ? " (latest)" : "") }')
  say "Versions: $versions"
  ask "Which one? [${latest#v}] "
  want=${REPLY:-$latest}
  url=$(printf '%s\n' "$list" | awk -v t="$want" '$1 == t || $1 == "v" t { print $2; exit }')
  [ -n "$url" ] || die "There is no version $want."
  say ""
  fetch_zip "$url"
}

fetch_zip() {
  url=$1
  cache=${TMPDIR:-/tmp}/kidsos
  mkdir -p "$cache"
  ZIP=$cache/${url##*/}
  if [ -f "$ZIP" ]; then
    say "1/3  ${ZIP##*/} is already downloaded."
    return
  fi
  mb=$(curl -sIL "$url" | tr -d '\r' | awk 'tolower($1) == "content-length:" { n = $2 } END { print int(n / 1048576) }')
  [ "${mb:-0}" -gt 0 ] && mb="$mb MB" || mb="a few hundred MB"
  say "1/3  Downloading ${ZIP##*/}, $mb..."
  curl -fL --progress-bar -o "$ZIP.part" "$url"
  mv "$ZIP.part" "$ZIP"
}

# ---- go ----

need curl unzip
case $OS in
  mac) need diskutil ;;
  linux) need lsblk sfdisk mkfs.vfat sudo ;;
esac

banner
[ -n "$DISK" ] || pick_card
choose_mode
[ "$MODE" = erase ] && ask_setup
say ""
if [ -n "$ZIP" ]; then
  [ -f "$ZIP" ] || die "$ZIP does not exist."
  say "1/3  Using ${ZIP##*/}."
else
  pick_release
fi
spin "     Checking ${ZIP##*/}..." unzip -tq "$ZIP" || die "${ZIP##*/} is damaged. Delete it and run this again."

if [ "$MODE" = erase ]; then
  spin "2/3  Formatting the card..." ${OS}_format
  MOUNT=$(cat "$SPIN_OUT")
  spin "3/3  Copying KidsOS to the card, a few minutes..." unzip -q "$ZIP" -d "$MOUNT"
  write_setup
else
  [ -n "$MOUNT" ] || die "The card cannot be read. Run this again and choose to erase it."
  say "2/3  Keeping the games and saves on the card."
  # the swap file never changes and would cost half a minute to write again
  if [ -f "$MOUNT/cachefile" ]; then
    spin "3/3  Copying the new KidsOS over the old one, a few minutes..." unzip -oq "$ZIP" -x cachefile -d "$MOUNT"
  else
    spin "3/3  Copying the new KidsOS over the old one, a few minutes..." unzip -oq "$ZIP" -d "$MOUNT"
  fi
fi
# macOS puts a "._name" file next to everything it writes on FAT32; Onion would move them around or run them
find "$MOUNT" -name '._*' -type f -delete 2>/dev/null || true
[ -f "$MOUNT/.tmp_update/updater" ] || die "The copy went wrong: .tmp_update/updater is missing on the card."
version=$(cat "$MOUNT/kidsos_version.txt" 2>/dev/null || echo "KidsOS")
spin "     Finishing the card..." ${OS}_eject

say ""
say "All done! $version is on the card, you can take it out."
say ""
if [ "$MODE" = erase ]; then
  say "Put it in the console and turn it on. KidsOS installs itself in about two minutes, there is nothing to"
  say "press. The console restarts into the kids screen, in $LANGUAGE_NAME${WIFI_NAME:+ and connected to $WIFI_NAME}."
else
  say "Put it in the console and turn it on. Onion updates itself, which takes a minute or two, and keeps the"
  say "games, saves and favourites."
fi
say ""
