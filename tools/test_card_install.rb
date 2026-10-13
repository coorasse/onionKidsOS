#!/usr/bin/env ruby
# frozen_string_literal: true

# Plays the card installer (install.sh at the root) on Linux without a Linux machine: an Ubuntu container
# with the tools the script asks for and a loop device over a sparse file as the memory card. The host's
# /dev is mounted so the partition node the kernel makes (/dev/loopNp1) is visible, and lsblk is wrapped to
# report loop devices as removable, the one thing a loop device cannot be. Answers reach the script's
# /dev/tty through `script`, like a parent typing them.
#
# Scenarios: a fresh card put in while the installer waits for one, an update that keeps the games, a card
# with plain Onion, stopping at the menu, and erasing a card that had KidsOS.
#
# usage: ruby tools/test_card_install.rb build/KidsOS-<version>.zip
require "open3"
require "timeout"

ROOT = File.expand_path("..", __dir__)
ZIP = File.expand_path(ARGV[0] || abort("usage: ruby tools/test_card_install.rb build/KidsOS-<version>.zip"))
abort "#{ZIP} does not exist, build it with make dist" unless File.file?(ZIP)
IMAGE = "kidsos-card-install-test"
CARD = "/tmp/card.img"
ZIP_ARG = "--zip /zip/#{File.basename(ZIP)}"

DOCKERFILE = <<~DOCKERFILE
  FROM ubuntu:24.04
  RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \\
        sudo fdisk dosfstools eject curl unzip ca-certificates && rm -rf /var/lib/apt/lists/*
DOCKERFILE

LSBLK = <<~'SH'
  #!/bin/sh
  # loop devices pose as removable cards for the test
  /usr/bin/lsblk "$@" | sed '/NAME="\/dev\/loop/s/RM="0"/RM="1"/'
SH

$failures = []
def check(name, ok, detail = nil)
  puts "#{ok ? "ok  " : "FAIL"} #{name}"
  $failures << [name, detail] unless ok
end

def docker(*args, input: nil) = Open3.capture2e("docker", *args, stdin_data: input)

def docker!(*args, input: nil)
  out, status = docker(*args, input: input)
  abort "docker #{args.first(3).join(" ")} failed:\n#{out}" unless status.success?
  out
end

def exec!(*cmd, input: nil) = docker!("exec", "-i", $container, *cmd, input: input)

puts "building #{IMAGE}..."
docker!("build", "-q", "-t", IMAGE, "-", input: DOCKERFILE)
$container = docker!("run", "-d", "--rm", "--privileged", "-v", "/dev:/dev", "-v", "#{ROOT}:/work:ro",
                     "-v", "#{File.dirname(ZIP)}:/zip:ro", IMAGE, "sleep", "infinity").strip
DETACH = "losetup -j #{CARD} | cut -d: -f1 | xargs -r losetup -d"
at_exit do
  docker("exec", $container, "sh", "-c", DETACH)
  docker("kill", $container)
end
exec!("sh", "-c", "cat > /usr/local/bin/lsblk && chmod +x /usr/local/bin/lsblk", input: LSBLK)

# a blank 1 GB card, not in the reader yet
def new_card = exec!("sh", "-c", "#{DETACH}; rm -f #{CARD}; truncate -s 1G #{CARD}")
def insert_card = exec!("losetup", "-fP", "--show", CARD).strip
def disk = exec!("sh", "-c", "losetup -j #{CARD} | cut -d: -f1").strip

# runs a shell snippet with the card's partition mounted at /card
def on_card(script)
  out, status = docker("exec", $container, "sh", "-c",
                       "mkdir -p /card && mount #{disk}p1 /card || exit 99; (#{script}); rc=$?; umount /card; exit $rc")
  abort "the card cannot be mounted:\n#{out}" if status.exitstatus == 99
  [out, status.success?]
end

def card_has?(path) = on_card("test -e /card/#{path}").last
def card_file(path) = on_card("cat /card/#{path}").first

# runs install.sh with the arguments; the block types the answers (and may put the card in first). Returns
# the output without the spinner frames and whether the script succeeded.
def install(args)
  cmd = ["docker", "exec", "-i", $container, "script", "-qec", "sh /work/install.sh #{args}", "/dev/null"]
  Open3.popen2e(*cmd) do |stdin, out, wait|
    type = ->(*answers) { stdin.write(answers.map { |a| "#{a}\n" }.join); stdin.flush }
    yield type
    output = Timeout.timeout(600) { out.read }.force_encoding("UTF-8").scrub
    [output.lines.map { |l| l.split("\r").last }.join, wait.value.success?]
  end
rescue Timeout::Error
  abort "install.sh did not finish within 10 minutes"
end

version = IO.popen(["unzip", "-p", ZIP, "kidsos_version.txt"], &:read).strip

puts "== fresh card, put in while the installer waits"
new_card
out, ok = install(ZIP_ARG) do |type|
  sleep 3
  dev = insert_card
  type.(dev, "2", "Kids Net", "secret")
end
check "waits for the card and finds it", out.include?("No memory card found") && out.include?("Found a memory card:"), out
check "runs through", ok && out.include?("All done! #{version} is on the card"), out
check "says what the console will be", out.include?("in Deutsch and connected to Kids Net"), out
check "DOS partition table with one FAT32 partition", exec!("sfdisk", "-d", disk).include?("type=c") &&
  exec!("blkid", "-o", "export", "#{disk}p1").then { |b| b.include?("LABEL=KIDSOS") && b.include?("TYPE=vfat") }
check "card is unmounted at the end", exec!("lsblk", "-nro", "MOUNTPOINT", "#{disk}p1").strip.empty?
check "KidsOS is on the card", card_has?("kidsos_version.txt") && card_has?(".tmp_update/updater") &&
  card_file("kidsos_version.txt").strip == version
check "setup.txt has the language and the Wi-Fi", card_file("kids/setup.txt") == "language=de\nwifi_name=Kids Net\nwifi_password=secret\n"

puts "== update keeps the games"
on_card("mkdir -p /card/Roms/GB && echo game > /card/Roms/GB/test.gb && echo 'KidsOS 0.0.9' > /card/kidsos_version.txt")
out, ok = install("#{ZIP_ARG} --disk #{disk}") { |type| type.("") }
check "sees the old version", out.include?("It already has KidsOS 0.0.9."), out
check "updates by default", ok && out.include?("Keeping the games and saves") && out.include?("keeps the\ngames, saves and favourites"), out
check "games stay, version is new, setup is untouched", card_has?("Roms/GB/test.gb") &&
  card_file("kidsos_version.txt").strip == version && card_file("kids/setup.txt").start_with?("language=de")

puts "== card with plain Onion"
on_card("rm /card/kidsos_version.txt && mkdir -p /card/.tmp_update/onionVersion && echo 4.3.1-1 > /card/.tmp_update/onionVersion/version.txt")
out, ok = install(ZIP_ARG) { |type| type.("1") }
check "sees Onion", out.include?("It has Onion 4.3.1-1, no KidsOS yet.") && out.include?("Add KidsOS to it"), out
check "adds KidsOS, games stay", ok && card_has?("kidsos_version.txt") && card_has?("Roms/GB/test.gb"), out

puts "== stop at the menu"
out, ok = install(ZIP_ARG) { |type| type.("3") }
check "stops without touching the card", !ok && out.include?("Stopped, nothing was changed.") && card_has?("Roms/GB/test.gb"), out

puts "== erase a card that had KidsOS"
out, ok = install(ZIP_ARG) { |type| type.("2", disk, "1", "") }
check "asks for the card's name", out.include?("Type \"#{disk}\" to erase it"), out
check "erases and installs", ok && out.include?("in English.") && !out.include?("connected to"), out
check "games are gone, setup has only the language", !card_has?("Roms/GB/test.gb") && card_file("kids/setup.txt") == "language=en\n"

puts
if $failures.empty?
  puts "all good"
else
  $failures.each do |name, detail|
    puts "FAIL #{name}"
    puts detail.lines.last(25).map { |l| "  #{l}" }.join if detail
  end
  exit 1
end
