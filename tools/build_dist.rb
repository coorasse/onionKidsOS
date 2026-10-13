#!/usr/bin/env ruby
# Builds the KidsOS zip that parents copy onto an empty SD card: the official Onion release with the kids
# launcher added to the package that Onion's installer unpacks on the card (onion.pak), plus the boot hook that
# keeps the launcher installed across Onion updates. Both of Onion's packages (onion.pak and retroarch.pak) are
# repacked with fast LZMA2 and without the parts no kid will use, which makes the first boot a lot shorter.
#
# usage: ruby tools/build_dist.rb <kidsos version> <onion version> <onion zip sha256 | ->
#   needs build/onion/dist/.kidsos (make onion-dist); the Onion zip is downloaded once into build/onion-release
require "fileutils"
require "digest"
require_relative "sevenzip"

KIDSOS_VERSION, ONION_VERSION, ONION_SHA256 = ARGV
abort "usage: #{$0} <kidsos version> <onion version> <onion zip sha256 | ->" unless ONION_SHA256

ROOT = File.expand_path("..", __dir__)
LAYER_SRC = File.join(ROOT, "build/onion/dist/.kidsos")
CACHE = File.join(ROOT, "build/onion-release")
ONION_ZIP = File.join(CACHE, "Onion-v#{ONION_VERSION}.zip")
ONION_URL = "https://github.com/OnionUI/Onion/releases/download/v#{ONION_VERSION}/Onion-v#{ONION_VERSION}.zip"
WORK = File.join(ROOT, "build/dist")
CARD = File.join(WORK, "card") # what the parent copies to the SD card
PAK = "card/miyoo/app/.tmp_update/onion.pak"
RA_PAK = "card/RetroArch/retroarch.pak"
OUT = File.join(ROOT, "build", "KidsOS-#{KIDSOS_VERSION}-onion#{ONION_VERSION}.zip")

def step(msg) = puts("-- #{msg}")

abort "#{LAYER_SRC} missing: run make onion-dist first" unless File.exist?(File.join(LAYER_SRC, "kidsos.elf"))

unless File.exist?(ONION_ZIP)
  step "downloading #{ONION_URL}"
  FileUtils.mkdir_p(CACHE)
  system("curl", "-fL", "--progress-bar", "-o", "#{ONION_ZIP}.part", ONION_URL) || abort("download failed")
  FileUtils.mv("#{ONION_ZIP}.part", ONION_ZIP)
end
sha = Digest::SHA256.file(ONION_ZIP).hexdigest
if ONION_SHA256 == "-"
  puts "Onion zip sha256: #{sha} (pin it as ONION_SHA256 in the Makefile)"
elsif sha != ONION_SHA256
  abort "Onion zip checksum mismatch: expected #{ONION_SHA256}, got #{sha}"
end

step "unpacking the Onion release"
FileUtils.rm_rf(WORK)
FileUtils.mkdir_p(CARD)
system("unzip", "-q", ONION_ZIP, "-d", CARD) || abort("unzip failed")
[PAK, RA_PAK].each { |pak| abort "#{pak} not found in the Onion zip" unless File.exist?(File.join(WORK, pak)) }

# Onion's installer unpacks almost a gigabyte onto the card at the first boot, at a few MB/s. These parts are
# for grown-ups (point-and-click adventures, a video player, alternative arcade frontends and MAME builds) and
# go. Arcade stays on Final Burn Neo, the core the arcade coins are made for. Every pattern must match
# something, so a new Onion release that moved or renamed a part makes the build stop here.
DROPPED = {
  "pak" => [
    "App/PackageManager/data/RApp/SCUMM (ScummVM standalone)", "App/PackageManager/data/Emu/SCUMM (ScummVM)",
    ".tmp_update/bin/scummvm",
    "App/PackageManager/data/RApp/Arcade (AdvanceMame)", "App/PackageManager/data/App/AdvanceMENU (Alternative frontend)",
    ".tmp_update/bin/adv",
    "App/PackageManager/data/Emu/Arcade (MAME 2003-Plus)", "App/PackageManager/data/RApp/Arcade (MAME 2003-xtreme)",
    "App/PackageManager/data/App/Video Player (FFplay)",
    ".tmp_update/bin/ffmpeg", ".tmp_update/bin/ffplay", ".tmp_update/bin/ffplay_16_9",
    ".tmp_update/lib/parasyte/libav*", ".tmp_update/lib/parasyte/libsw*", ".tmp_update/lib/libx264*",
    ".tmp_update/bin/samba", ".tmp_update/lib/samba",
    "App/PackageManager/data/RApp/Sony - PlayStation (PCSX standalone)",
    "App/PackageManager/data/RApp/PICO-8 (PICO-8 standalone)", "App/PackageManager/data/RApp/PICO-8 (Fake8 standalone)",
    "App/PackageManager/data/Emu/PICO-8 (Fake8)",
    "App/PackageManager/data/Emu/Commodore - Amiga (PUAE 2021)",
    "App/PackageManager/data/RApp/Commodore - Amiga (PUAE 2021 xtreme)",
    "App/PackageManager/data/RApp/Commodore - Amiga (uae4arm)", "App/PackageManager/data/RApp/Commodore - Amiga CD32 (uae4arm)",
    "App/PackageManager/data/RApp/Arcade (MAME 2000)", "App/PackageManager/data/RApp/Game engine - EasyRPG (EasyRPG)",
    "App/PackageManager/data/Emu/Ports Collection",
    ".tmp_update/lib/parasyte/python2.7", ".tmp_update/lib/parasyte/libpython*"
  ],
  "ra" => [
    "RetroArch/.retroarch/cores/scummvm_libretro.*", "RetroArch/.retroarch/cores/mame2003_plus_libretro.*",
    "RetroArch/.retroarch/cores/km_mame2003_xtreme_libretro.*", "RetroArch/.retroarch/cores/mame2000_libretro.*",
    "RetroArch/.retroarch/cores/fake08_libretro.*", "RetroArch/.retroarch/cores/*puae*_libretro.*",
    "RetroArch/.retroarch/cores/uae4arm_libretro.*", "RetroArch/.retroarch/cores/fsuae_libretro.*",
    "RetroArch/.retroarch/cores/easyrpg_libretro.*", "RetroArch/.retroarch/cores/mrboom_libretro.*"
  ],
  "card" => ["BIOS/scummvm", "BIOS/.advance", "Media/Videos/Onion/Mame2003Plus Save State Tutorial.mp4"]
}

# $1 = directory under WORK, $2 = patterns relative to it: removes what they match and reports the saved bytes
def drop(dir, patterns)
  saved = 0
  patterns.each do |pattern|
    matches = Dir.glob(File.join(WORK, dir, pattern), File::FNM_DOTMATCH)
    abort "#{pattern} matches nothing in #{dir}: did Onion move it?" if matches.empty?
    matches.each do |path|
      saved += Dir.glob(File.join(path, "**/*"), File::FNM_DOTMATCH).push(path).sum { |f| File.file?(f) ? File.size(f) : 0 }
      FileUtils.rm_rf(path)
    end
  end
  puts "   #{dir}: #{saved / 1024 / 1024} MB dropped"
end

step "unpacking onion.pak and retroarch.pak"
SevenZip.extract(WORK, PAK, "pak")
SevenZip.extract(WORK, RA_PAK, "ra")

step "dropping what kids do not use"
DROPPED.each { |dir, patterns| drop(dir, patterns) }

step "adding the kids layer"
PAK_DIR = File.join(WORK, "pak")
FileUtils.cp_r(LAYER_SRC, File.join(PAK_DIR, ".kidsos"))
FileUtils.mkdir_p(File.join(PAK_DIR, ".tmp_update/startup"))
FileUtils.cp(File.join(ROOT, "onion/startup.sh"), File.join(PAK_DIR, ".tmp_update/startup/kidsos.sh"))
(Dir.glob(File.join(PAK_DIR, ".kidsos/**/*.sh")) + [File.join(PAK_DIR, ".tmp_update/startup/kidsos.sh")]).each do |script|
  system("sh", "-n", script) || abort("#{script} has a syntax error")
end
File.write(File.join(PAK_DIR, ".kidsos/version.txt"), "#{KIDSOS_VERSION}\n")

# Emulators every card gets without a trip through Onion's Package Manager. Their packages are copied the way
# the Package Manager copies them onto the card (Emu/<TAG> and Roms/<TAG>), so Onion lists them as installed
# at the first boot and keeps them up to date like any other package.
PREINSTALLED_EMUS = ["Nintendo - Game Boy (Gambatte)", "Nintendo - Game Boy Color (Gambatte)",
                     "Nintendo - GBA (mGBA)", "Nintendo - NES (FCEUmm)",
                     "Nintendo - SNES (Beetle Supafaust)", "Nintendo - DS (Drastic)",
                     "SNK - Neo Geo (fbalpha2012 neogeo)"]
PACKAGES = "App/PackageManager/data/Emu"

step "preinstalling #{PREINSTALLED_EMUS.size} emulators"
PREINSTALLED_EMUS.each do |package|
  src = File.join(PAK_DIR, PACKAGES, package)
  abort "#{package} is not in onion.pak" unless Dir.exist?(File.join(src, "Emu"))
  Dir.children(src).each do |top|
    FileUtils.mkdir_p(File.join(PAK_DIR, top))
    FileUtils.cp_r(File.join(src, top, "."), File.join(PAK_DIR, top))
  end
end
tags = Dir.children(File.join(PAK_DIR, "Emu")).sort
puts "   #{tags.join(" ")}"

# Onion shows the boot picture before it runs the startup hooks, so the kids picture also travels in the pak at
# the theme override path where install.sh would put it at the first boot, and the very first boot shows it.
BOOT_OVERRIDE = "Saves/CurrentProfile/theme/skin/extra"
FileUtils.mkdir_p(File.join(PAK_DIR, BOOT_OVERRIDE))
FileUtils.cp(File.join(ROOT, "onion/bootScreen.png"), File.join(PAK_DIR, BOOT_OVERRIDE, "bootScreen.png"))

# $1 = directory under WORK, $2 = archive path relative to WORK, $3 = paths that must be in it, $4 = patterns
# that must not; the content is byte for byte what the directory holds, only the compression is ours
def repack(dir, archive, required, dropped)
  FileUtils.rm_f(File.join(WORK, archive))
  SevenZip.create(WORK, "../#{archive}", *Dir.children(File.join(WORK, dir)).sort, cwd: dir)
  listed = SevenZip.list(WORK, archive)
  required.each { |f| abort "#{f} is not in #{File.basename(archive)}" unless listed.include?(f) }
  dropped.each do |pattern|
    left = listed.select { |f| File.fnmatch?(pattern, f, File::FNM_DOTMATCH) || f.start_with?("#{pattern}/") }
    abort "#{left.first} is still in #{File.basename(archive)}" unless left.empty?
  end
  puts "   #{File.basename(archive)}: #{File.size(File.join(WORK, archive)) / 1024 / 1024} MB, #{listed.size} entries"
end

step "repacking onion.pak"
repack("pak", PAK,
       %w[.kidsos/kidsos.elf .kidsos/install.sh .kidsos/startup.sh .tmp_update/startup/kidsos.sh .tmp_update/runtime.sh
          .tmp_update/bin/MainUI-354-clean .tmp_update/bin/gameSwitcher App/PackageManager/launch.sh] +
       ["#{BOOT_OVERRIDE}/bootScreen.png"] + tags.flat_map { |t| ["Emu/#{t}/config.json", "Roms/#{t}"] },
       DROPPED["pak"])
step "repacking retroarch.pak"
repack("ra", RA_PAK,
       %w[RetroArch/retroarch RetroArch/.retroarch/cores/fbneo_libretro.so RetroArch/.retroarch/cores/fbalpha2012_neogeo_libretro.so
          RetroArch/.retroarch/cores/gambatte_libretro.so RetroArch/.retroarch/cores/mgba_libretro.so],
       DROPPED["ra"])

# Onion's first boot creates a 128 MB swap file on the card with dd, half a minute at the card's speed. The file
# travels in the zip instead: 128 MB of zeros with the swap header, which the zip squeezes into a few hundred KB.
step "making the swap file"
SWAP = File.join(CARD, "cachefile")
File.open(SWAP, "wb") { |f| f.truncate(128 * 1024 * 1024) }
system("docker", "run", "--rm", "-v", "#{CARD}:/work", "alpine:3.20", "sh", "-c",
       "mkswap /work/cachefile > /dev/null") || abort("mkswap failed")
abort "cachefile has no swap header" unless File.binread(SWAP, 10, 4096 - 10) == "SWAPSPACE2"

step "writing the card root files"
File.write(File.join(CARD, "kidsos_version.txt"), "KidsOS #{KIDSOS_VERSION} on Onion v#{ONION_VERSION}\n")
File.write(File.join(CARD, "KIDSOS-README.txt"), <<~TXT)
  KidsOS #{KIDSOS_VERSION} for the Miyoo Mini Plus (built on Onion v#{ONION_VERSION})

  Copy everything in this folder, hidden folders included, to the root of an empty SD card
  formatted as FAT32, put the card in the Miyoo and turn it on.

  Full guide: https://github.com/coorasse/onionKidsOS
TXT

step "zipping"
FileUtils.rm_f(OUT)
system("zip", "-qr", "-X", OUT, ".", "-x", ".DS_Store", "*/.DS_Store", chdir: CARD) || abort("zip failed")
size_mb = File.size(OUT) / 1024 / 1024
puts "#{OUT} (#{size_mb} MB)"
puts "card folders: #{Dir.children(CARD).sort.join(" ")}"
