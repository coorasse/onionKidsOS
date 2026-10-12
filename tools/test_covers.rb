#!/usr/bin/env ruby
# frozen_string_literal: true

# Runs onion/covers.sh in a BusyBox container against a fake SD card and a fake thumbnails.libretro.com
# (a directory served by a wget stub) and checks that favourites without box art get one: exact names,
# another region of the same title, names with characters libretro replaces, arcade roms through Onion's
# name list, nothing for unknown titles, nothing touched when a picture exists, nothing at all offline.
#
# usage: ruby tools/test_covers.rb
require "fileutils"

ROOT = File.expand_path("..", __dir__)
WORK = File.join(ROOT, "build", "covers-test")
SD = File.join(WORK, "sd")
SERVER = File.join(WORK, "server")
STUBS = File.join(WORK, "stubs")
IMAGE = "busybox:1.37.0"
LOG = File.join(SD, ".kidsos/covers.log")

def stub(path, body)
  FileUtils.mkdir_p(File.dirname(path))
  File.write(path, "#!/bin/sh\n#{body}\n")
  File.chmod(0755, path)
end

def favourite(tag, rom)
  base = File.basename(rom, ".*")
  "{\"label\":\"#{base}\",\"launch\":\"/mnt/SDCARD/Emu/#{tag}/launch.sh\",\"type\":5," \
    "\"imgpath\":\"/mnt/SDCARD/Emu/#{tag}/../../Roms/#{tag}/Imgs/#{base}.png\"," \
    "\"rompath\":\"/mnt/SDCARD/Emu/#{tag}/../../Roms/#{tag}/#{rom}\"}"
end

def serve(system, name)
  path = File.join(SERVER, system, "Named_Boxarts", "#{name}.png")
  FileUtils.mkdir_p(File.dirname(path))
  File.write(path, "PNG of #{name}")
end

# the mounted directories are emptied, not recreated: Docker Desktop keeps serving a removed directory's
# old content to the next container for a while
def build_fixture
  [SD, SERVER, STUBS].each do |dir|
    FileUtils.mkdir_p(dir)
    FileUtils.rm_rf(Dir.children(dir).map { |c| File.join(dir, c) })
  end
  FileUtils.mkdir_p(["#{SD}/.kidsos", "#{SD}/Roms/GB/Imgs", "#{SD}/Roms/GBA", "#{SD}/Roms/SFC", "#{SD}/Roms/ARCADE",
                     "#{SD}/Roms/PORTS", "#{SD}/BIOS/arcade_lists"])
  FileUtils.cp(File.join(ROOT, "onion/covers.sh"), "#{SD}/.kidsos/covers.sh")
  favourites = [
    favourite("GB", "Tetris (World) (Rev 1).gb"),
    favourite("GB", "Tetris.gb"),
    favourite("GB", "Kirby's Dream Land.gb"),
    favourite("GBA", "Tom & Jerry.gba"),
    favourite("SFC", "Nobody Knows.sfc"),
    favourite("ARCADE", "sf2.zip"),
    favourite("PORTS", "Doom.sh"),
    "{\"label\":\"broken line without a path\"}"
  ]
  File.write("#{SD}/Roms/favourite.json", favourites.join("\n") + "\n")
  File.write("#{SD}/Roms/GB/Imgs/Kirby's Dream Land.png", "the parent's own picture")
  File.write("#{SD}/BIOS/arcade_lists/arcade-rom-names.txt",
             "mslug\t\"Metal Slug - Super Vehicle-001\"\n" \
             "sf2\t\"Street Fighter II - The World Warrior (World 910522)\"\n")

  serve("Nintendo - Game Boy", "Tetris (Japan) (En)")
  serve("Nintendo - Game Boy", "Tetris (World) (Rev 1)")
  serve("Nintendo - Game Boy", "Tetris 2 (USA)")
  serve("Nintendo - Game Boy", "Kirby's Dream Land (USA, Europe)")
  serve("Nintendo - Game Boy Advance", "Tom _ Jerry (USA)")
  serve("Nintendo - Super Nintendo Entertainment System", "Super Mario World (USA)")
  FileUtils.mkdir_p("#{SERVER}/FBNeo - Arcade Games/Named_Boxarts")
  serve("MAME", "Street Fighter II - The World Warrior (World 910522)")

  # Onion's pngScale shrinks the download in place; the stub marks what went through it
  stub("#{SD}/.tmp_update/bin/pngScale", "printf 'scaled ' > \"$2\" && cat \"$1\" >> \"$2\"")
  # busybox wget as covers.sh uses it, against /server; a directory URL gets an Apache-like listing
  stub("#{STUBS}/wget", <<~'SH')
    out=-
    spider=0
    while [ $# -gt 0 ]; do
    	case $1 in
    	-q) ;;
    	-T) shift ;;
    	--spider) spider=1 ;;
    	-O) shift; out=$1 ;;
    	*) url=$1 ;;
    	esac
    	shift
    done
    echo "wget $url" >> /mnt/SDCARD/wget.log
    [ -f /server/offline ] && exit 1
    path=$(printf '%s' "$url" | sed -e 's#^http://thumbnails.libretro.com##' \
    	-e 's/%20/ /g; s/%23/#/g; s/%2B/+/g; s/%26/\&/g; s/%25/%/g')
    case $path in
    */)
    	[ -d "/server$path" ] || exit 1
    	[ $spider = 1 ] && exit 0
    	ls "/server$path" | sed 's/ /%20/g; s/.*/<tr><td><a href="&">&<\/a><\/td><\/tr>/' > "$out"
    	;;
    *)
    	[ -f "/server$path" ] || exit 1
    	[ $spider = 1 ] && exit 0
    	cat "/server$path" > "$out"
    	;;
    esac
  SH
end

def run_covers
  cmd = ["docker", "run", "--rm", "-v", "#{SD}:/mnt/SDCARD", "-v", "#{STUBS}:/stubs:ro",
         "-v", "#{SERVER}:/server", IMAGE, "sh", "-c", <<~SH]
    export PATH=/stubs:$PATH
    KIDSOS_NET_WAIT=0 sh /mnt/SDCARD/.kidsos/covers.sh
    echo "rc=$?"
    [ -f /tmp/kidsos_covers_changed ] && echo "flag raised"
    [ -f /tmp/kidsos_covers.pid ] && echo "lock left behind"
    exit 0
  SH
  IO.popen(cmd, err: [:child, :out], &:read)
end

$failures = []
def check(name, ok, detail = nil)
  puts "#{ok ? "ok  " : "FAIL"} #{name}"
  $failures << [name, detail] unless ok
end

def cover(tag, name) = File.read("#{SD}/Roms/#{tag}/Imgs/#{name}.png") rescue nil
def log = File.read(LOG) rescue ""
def wget_log = File.read("#{SD}/wget.log") rescue ""

system("docker", "image", "inspect", IMAGE, out: File::NULL, err: File::NULL) || system("docker", "pull", IMAGE) ||
  abort("docker is needed for this test")
build_fixture

puts "== offline"
FileUtils.touch("#{SERVER}/offline")
out = run_covers
check "gives up without network", log.include?("no network, giving up") && out.include?("rc=0"), out + log
check "downloads nothing offline", Dir.glob("#{SD}/Roms/*/Imgs/*.png") == ["#{SD}/Roms/GB/Imgs/Kirby's Dream Land.png"]
check "no flag and no lock offline", !out.include?("flag raised") && !out.include?("lock left behind"), out
check "5 favourites counted (picture present, unknown system and broken line skipped)",
      log.include?("5 favourites without a cover"), log

puts "== online"
FileUtils.rm_f("#{SERVER}/offline")
FileUtils.rm_f("#{SD}/wget.log")
out = run_covers
check "exact name", cover("GB", "Tetris (World) (Rev 1)") == "scaled PNG of Tetris (World) (Rev 1)"
check "other region of the same title, World preferred", cover("GB", "Tetris") == "scaled PNG of Tetris (World) (Rev 1)"
check "characters libretro replaces", cover("GBA", "Tom & Jerry") == "scaled PNG of Tom _ Jerry (USA)"
check "arcade rom through Onion's name list, second system",
      cover("ARCADE", "sf2") == "scaled PNG of Street Fighter II - The World Warrior (World 910522)"
check "unknown title gets nothing", cover("SFC", "Nobody Knows").nil? && log.include?("not found SFC/Nobody Knows"), log
check "existing picture untouched", cover("GB", "Kirby's Dream Land") == "the parent's own picture"
check "flag raised, lock released", out.include?("flag raised") && !out.include?("lock left behind"), out
check "log lists the fetched covers", %w[GB/Tetris GBA/Tom ARCADE/sf2].all? { |n| log.include?("fetched #{n}") }, log
check "listing fetched once per system", wget_log.scan(%r{Game%20Boy/Named_Boxarts/$}).size == 1, wget_log
check "listing cached on the card", Dir.glob("#{SD}/.kidsos/cache/*.txt").size >= 2
check "no temporary files left", Dir.glob("#{SD}/Roms/*/Imgs/*").none? { |f| f.end_with?(".new", ".small.png") }

puts "== again"
FileUtils.rm_f("#{SD}/wget.log")
out = run_covers
check "only the missing title is retried", log.include?("1 favourites without a cover") && !log.include?("fetched"), log
check "cached listing reused", wget_log.scan(%r{Named_Boxarts/$}).empty?, wget_log
check "no flag when nothing arrived", !out.include?("flag raised"), out

puts
if $failures.empty?
  puts "all checks passed"
else
  puts "#{$failures.size} failed:"
  $failures.each { |name, detail| puts "- #{name}#{detail ? "\n#{detail.lines.map { |l| "    #{l}" }.join}" : ""}" }
  exit 1
end
