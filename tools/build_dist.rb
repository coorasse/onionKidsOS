#!/usr/bin/env ruby
# Builds the KidsOS zip that parents copy onto an empty SD card: the official Onion release, untouched,
# with the kids launcher added to the package that Onion's installer unpacks on the card (onion.pak), plus
# the boot hook that keeps the launcher installed across Onion updates.
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
CARD = File.join(WORK, "card")   # what the parent copies to the SD card
LAYER = File.join(WORK, "layer") # what gets added to onion.pak
PAK = "card/miyoo/app/.tmp_update/onion.pak"
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
FileUtils.mkdir_p([CARD, LAYER])
system("unzip", "-q", ONION_ZIP, "-d", CARD) || abort("unzip failed")
abort "#{PAK} not found in the Onion zip" unless File.exist?(File.join(WORK, PAK))

step "preparing the kids layer"
FileUtils.cp_r(LAYER_SRC, File.join(LAYER, ".kidsos"))
FileUtils.mkdir_p(File.join(LAYER, ".tmp_update/startup"))
FileUtils.cp(File.join(ROOT, "onion/startup.sh"), File.join(LAYER, ".tmp_update/startup/kidsos.sh"))
Dir.glob(File.join(LAYER, "**/*.sh")).each do |script|
  system("sh", "-n", script) || abort("#{script} has a syntax error")
end
File.write(File.join(LAYER, ".kidsos/version.txt"), "#{KIDSOS_VERSION}\n")

step "adding the layer to onion.pak"
SevenZip.add(WORK, "../#{PAK}", ".kidsos", ".tmp_update", cwd: "layer")
listed = SevenZip.list(WORK, PAK)
%w[.kidsos/kidsos.elf .kidsos/install.sh .kidsos/startup.sh .tmp_update/startup/kidsos.sh .tmp_update/runtime.sh].each do |f|
  abort "#{f} is not in onion.pak" unless listed.include?(f)
end

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
