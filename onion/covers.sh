#!/bin/sh
# Runs on the device in the background, started by launch.sh at every start of the kids UI. Downloads the
# box art of every favourite that has none from thumbnails.libretro.com (the source of Onion's own Scraper
# app) into the Imgs folder Onion names in favourite.json, where both Onion and the kids launcher look for
# it. Needs Wi-Fi, which Onion brings up at boot once the parent has turned it on; without network nothing
# happens and the next boot tries again. Every fetched cover raises /tmp/kidsos_covers_changed, which the
# launcher polls to show the picture without a restart.
#   covers.sh [sdcard root]
SD=${1:-/mnt/SDCARD}
HOST=http://thumbnails.libretro.com
KIDS=$SD/.kidsos
LOG=$KIDS/covers.log
CACHE=$KIDS/cache
LOCK=/tmp/kidsos_covers.pid
FLAG=/tmp/kidsos_covers_changed
# seconds to wait for the network after boot (KIDSOS_NET_WAIT=0: one try)
NET_WAIT=${KIDSOS_NET_WAIT:-90}
PATH="$SD/.tmp_update/bin:$PATH"
TMP=/tmp/kidsos_cover.$$

# Onion's Roms folder tags and the libretro thumbnail systems that may hold the cover, first match wins
systems_of() {
	case $1 in
	ATARI) echo "Atari - 2600" ;;
	FIFTYTWOHUNDRED) echo "Atari - 5200" ;;
	SEVENTYEIGHTHUNDRED) echo "Atari - 7800" ;;
	LYNX) echo "Atari - Lynx" ;;
	CPC) echo "Amstrad - CPC" ;;
	COLECO) echo "Coleco - ColecoVision" ;;
	FC) printf '%s\n' "Nintendo - Nintendo Entertainment System" "Nintendo - Family Computer Disk System" ;;
	GB) echo "Nintendo - Game Boy" ;;
	GBA) echo "Nintendo - Game Boy Advance" ;;
	GBC) echo "Nintendo - Game Boy Color" ;;
	GG) echo "Sega - Game Gear" ;;
	INTELLIVISION) echo "Mattel - Intellivision" ;;
	MD) echo "Sega - Mega Drive - Genesis" ;;
	MS) echo "Sega - Master System - Mark III" ;;
	MSX) echo "Microsoft - MSX" ;;
	N64) echo "Nintendo - Nintendo 64" ;;
	NDS) echo "Nintendo - Nintendo DS" ;;
	NGP) printf '%s\n' "SNK - Neo Geo Pocket Color" "SNK - Neo Geo Pocket" ;;
	PCE) printf '%s\n' "NEC - PC Engine - TurboGrafx 16" "NEC - PC Engine CD - TurboGrafx-CD" ;;
	PCECD) echo "NEC - PC Engine CD - TurboGrafx-CD" ;;
	POKE) echo "Nintendo - Pokemon Mini" ;;
	PS) echo "Sony - PlayStation" ;;
	SEGACD) echo "Sega - Mega-CD - Sega CD" ;;
	SFC) echo "Nintendo - Super Nintendo Entertainment System" ;;
	THIRTYTWOX) echo "Sega - 32X" ;;
	VECTREX) echo "GCE - Vectrex" ;;
	ZXS) echo "Sinclair - ZX Spectrum" ;;
	ARCADE | FBNEO | CPS1 | CPS2 | CPS3 | MAME) printf '%s\n' "FBNeo - Arcade Games" "MAME" ;;
	NEOGEO) printf '%s\n' "SNK - Neo Geo" "FBNeo - Arcade Games" "MAME" ;;
	esac
}

is_arcade() {
	case $1 in ARCADE | FBNEO | CPS1 | CPS2 | CPS3 | MAME | NEOGEO) return 0 ;; esac
	return 1
}

# a JSON string field of one favourite.json line; Onion writes paths without escapes
field() {
	printf '%s\n' "$2" | sed -n "s/.*\"$1\":\"\([^\"]*\)\".*/\1/p"
}

# Onion writes rom paths through the emulator folder: /mnt/SDCARD/Emu/GB/../../Roms/GB/x.gb
normalise() {
	printf '%s\n' "$1" | sed -e "s#^/mnt/SDCARD#$SD#" -e ':a' -e 's#/[^/.][^/]*/\.\./#/#' -e 'ta'
}

urlencode() {
	printf '%s' "$1" | sed 's/%/%25/g; s/ /%20/g; s/#/%23/g; s/+/%2B/g; s/&/%26/g'
}

# libretro names the thumbnails after the roms, with the characters a filesystem may reject as _
libretro_name() {
	printf '%s' "$1" | sed 's#[&*/:`<>?\\|"]#_#g'
}

# the title without the region/revision brackets, lower case, letters and digits only
title_key() {
	printf '%s' "$1" | sed 's/ *(.*$//' | tr 'A-Z' 'a-z' | tr -cd 'a-z0-9'
}

# the names of every thumbnail of a system, one per line, from the server's directory listing; kept a week
index_of() {
	file="$CACHE/$(printf '%s' "$1" | tr -c 'A-Za-z0-9\n' '_').txt"
	if [ ! -s "$file" ] || [ -n "$(find "$file" -mtime +7 2> /dev/null)" ]; then
		mkdir -p "$CACHE"
		wget -q -T 30 -O "$TMP.html" "$HOST/$(urlencode "$1")/Named_Boxarts/" || { rm -f "$TMP.html"; return 1; }
		grep -o 'href="[^"]*\.png"' "$TMP.html" |
			sed 's/^href="//; s/\.png"$//; s/&amp;/\&/g; s/&#39;/'"'"'/g; s/&quot;/"/g' |
			awk 'BEGIN { for (i = 1; i < 256; i++) h[sprintf("%02X", i)] = h[sprintf("%02x", i)] = sprintf("%c", i) }
			{ s = $0; out = ""
			  while ((p = index(s, "%")) > 0 && length(s) >= p + 2) {
			  	out = out substr(s, 1, p - 1) h[substr(s, p + 1, 2)]
			  	s = substr(s, p + 3)
			  }
			  print out s }' > "$file.new"
		rm -f "$TMP.html"
		[ -s "$file.new" ] && mv "$file.new" "$file" || { rm -f "$file.new"; return 1; }
	fi
	cat "$file"
}

# the thumbnail of another release of the same title (other region or revision), by preference
lookup() {
	index_of "$1" | awk -v want="$(title_key "$2")" '
		function key(s) { sub(/ *\(.*$/, "", s); s = tolower(s); gsub(/[^a-z0-9]/, "", s); return s }
		key($0) == want { names[++n] = $0 }
		END {
			split("(World (USA (Europe (Japan", pref, " ")
			for (p = 1; p <= 4; p++) for (i = 1; i <= n; i++) if (index(names[i], pref[p])) { print names[i]; exit }
			if (n) print names[1]
		}'
}

fetch() {
	url="$HOST/$(urlencode "$1")/Named_Boxarts/$(urlencode "$2").png"
	wget -q -T 30 -O "$TMP.png" "$url" && [ -s "$TMP.png" ] && return 0
	rm -f "$TMP.png"
	return 1
}

# $1 = tag, $2 = rom name without extension: the cover from the first system that has the title
download() {
	wanted=$2
	if is_arcade "$1"; then
		wanted=$(grep "^$2	" "$SD/BIOS/arcade_lists/arcade-rom-names.txt" 2> /dev/null | head -n 1 |
			sed 's/^[^	]*	"//; s/"$//')
		[ -n "$wanted" ] || return 1
	fi
	wanted=$(libretro_name "$wanted")
	systems_of "$1" > "$TMP.systems"
	while read -r system; do
		fetch "$system" "$wanted" && return 0
		other=$(lookup "$system" "$wanted")
		[ -n "$other" ] && [ "$other" != "$wanted" ] && fetch "$system" "$other" && return 0
	done < "$TMP.systems"
	return 1
}

store() {
	mkdir -p "$(dirname "$1")"
	if command -v pngScale > /dev/null 2>&1 && pngScale "$TMP.png" "$TMP.small.png" > /dev/null 2>&1 &&
		[ -s "$TMP.small.png" ]; then
		mv -f "$TMP.small.png" "$1"
	else
		mv -f "$TMP.png" "$1"
	fi
	rm -f "$TMP.png" "$TMP.small.png"
}

# ---- main ----
if [ -f "$LOCK" ] && kill -0 "$(cat "$LOCK")" 2> /dev/null; then exit 0; fi
echo $$ > "$LOCK"
trap 'rm -f "$LOCK" "$TMP".*' EXIT

FAV=$SD/Roms/favourite.json
[ -f "$FAV" ] || exit 0
TODO=$TMP.todo
: > "$TODO"
while IFS= read -r line || [ -n "$line" ]; do
	rom=$(normalise "$(field rompath "$line")")
	[ -n "$rom" ] || continue
	tag=$(printf '%s\n' "$rom" | sed -n 's#.*/Roms/\([^/]*\)/.*#\1#p')
	name=$(basename "$rom")
	name=${name%.*}
	img=$(normalise "$(field imgpath "$line")")
	[ -n "$img" ] || img=$SD/Roms/$tag/Imgs/$name.png
	[ -f "$img" ] && continue
	[ -n "$(systems_of "$tag")" ] || continue
	printf '%s\t%s\t%s\n' "$tag" "$name" "$img" >> "$TODO"
done < "$FAV"
[ -s "$TODO" ] || exit 0

exec > "$LOG" 2>&1
echo "$(date) $(wc -l < "$TODO") favourites without a cover"
waited=0
until wget -q -T 5 --spider "$HOST/" 2> /dev/null; do
	if [ "$waited" -ge "$NET_WAIT" ]; then
		echo "no network, giving up until the next boot"
		exit 0
	fi
	sleep 5
	waited=$((waited + 5))
done

while IFS='	' read -r tag name img; do
	if download "$tag" "$name"; then
		store "$img"
		touch "$FLAG"
		echo "fetched $tag/$name"
	else
		echo "not found $tag/$name"
	fi
done < "$TODO"
sync
echo "done"
