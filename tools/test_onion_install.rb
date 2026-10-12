#!/usr/bin/env ruby
# Boots Onion's real runtime.sh in a BusyBox container against a fake SD card where onion/install.sh has
# installed the kids launcher, then checks the whole sequence without a device:
#   boot -> MainUI shim -> kidsos (writes a command) -> Onion runs it -> keymon-style GameSwitcher overlay
#   -> GameSwitcher skipped after the game -> kidsos crashes -> launch.sh flags the exit -> real MainUI -> shutdown
# and afterwards that install.sh is idempotent and updates stale shims, runtime.sh is untouched and
# uninstall.sh restores everything.
#
# Device binaries (MainUI, keymon, axp, ...) are shell stubs that append to /mnt/SDCARD/test.log;
# "mount -o bind A B" copies A over B, which is what the bind mount means for the scripts.
#
# usage: ruby tools/test_onion_install.rb ../reference/Onion [runtime.sh]
#   the optional second argument boots that runtime.sh instead of the checkout's, e.g. the one on a real card
#        ruby tools/test_onion_install.rb ../reference/Onion --dist build/KidsOS-<version>.zip
#   builds the card from the onion.pak inside the KidsOS zip instead, which is what Onion's installer unpacks:
#   nobody runs install.sh by hand, the startup hook has to install the shims at the first boot and again
#   after a simulated Onion update (which wipes .tmp_update except config/ and onionVersion/)
require "fileutils"
require "digest"
require_relative "sevenzip"

ONION = File.expand_path(ARGV.fetch(0) { abort "usage: #{$0} <path to Onion checkout> [runtime.sh | --dist <zip>]" })
DIST_ZIP = ARGV[1] == "--dist" ? File.expand_path(ARGV.fetch(2) { abort "--dist needs the KidsOS zip" }) : nil
ROOT = File.expand_path("..", __dir__)
WORK = File.join(ROOT, "build", "onion-test")
SD = File.join(WORK, "sd")
SYSDIR = File.join(SD, ".tmp_update")
RUNTIME_SH = if DIST_ZIP
               File.join(SYSDIR, "runtime.sh")
             else
               File.expand_path(ARGV.fetch(1) { File.join(ONION, "static/build/.tmp_update/runtime.sh") })
             end
STUBS = File.join(WORK, "stubs")
IMAGE = "busybox:1.37.0"
MAINUIS = %w[MainUI-283-clean MainUI-283-expert MainUI-354-clean MainUI-354-expert]
KEYMAP = File.join(SYSDIR, "config/keymap.json")
INSTALL_LOG = File.join(SD, ".kidsos/install.log")
# what Onion's Tweaks app writes when the parent had remapped the MENU button
ORIGINAL_KEYMAP = <<~JSON
  {
      "mainui_single_press": 0,
      "mainui_long_press": 1,
      "mainui_double_press": 2,
      "ingame_single_press": 4,
      "ingame_long_press": 1,
      "ingame_double_press": 3,
      "mainui_button_x": "app:Tweaks",
      "mainui_button_y": "glo"
  }
JSON

def stub(path, body, log_as: nil)
  FileUtils.mkdir_p(File.dirname(path))
  lines = ["#!/bin/sh"]
  lines << "echo \"#{log_as} $*\" >> /mnt/SDCARD/test.log" if log_as
  lines << body
  File.write(path, lines.join("\n") + "\n")
  File.chmod(0755, path)
end

# what Onion's installer does with onion.pak: unpack it over the card (install_core, 7z x -aoa)
def unpack_pak
  unless File.exist?("#{WORK}/onion.pak")
    system("unzip", "-q", "-j", DIST_ZIP, "miyoo/app/.tmp_update/onion.pak", "-d", WORK) || abort("cannot read onion.pak from #{DIST_ZIP}")
  end
  SevenZip.extract(WORK, "onion.pak", "sd")
end

def stub_scripts
  stub("#{SYSDIR}/script/network/update_networking.sh", "exit 0", log_as: "update_networking")
  # the real one edits retroarch.cfg; the stub appends the given settings to it, so install.sh sees them next time
  stub("#{SYSDIR}/script/patch_ra_cfg.sh",
       "cat \"$1\" >> /mnt/SDCARD/test.log; mkdir -p /mnt/SDCARD/RetroArch/.retroarch; cat \"$1\" >> /mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg; exit 0",
       log_as: "patch_ra_cfg")
end

# what Onion's build puts into .tmp_update/bin, and the ARM launcher, as stubs
def stub_device_binaries
  MAINUIS.each { |ui| stub("#{SYSDIR}/bin/#{ui}", "touch /tmp/.offOrder", log_as: "real #{ui}") }
  stub("#{SYSDIR}/bin/gameSwitcher", "exit 0", log_as: "real gameSwitcher")
  %w[bootScreen chargingState themeSwitcher mainUiBatPerc setState playActivity infoPanel freemma].each do |b|
    stub("#{SYSDIR}/bin/#{b}", "exit 0", log_as: b)
  end
  %w[keymon batmon].each { |b| stub("#{SYSDIR}/bin/#{b}", "while :; do sleep 5; done") }
  stub("#{SYSDIR}/bin/axp", "echo 'status:00'")
  stub("#{SYSDIR}/bin/read_uuid", "echo TESTUUID")
  stub("#{SYSDIR}/bin/detectKey", "exit 1")
  stub("#{SD}/miyoo/app/audioserver", "while :; do sleep 5; done")
  FileUtils.touch(["#{SD}/miyoo/app/MainUI", "#{SD}/miyoo/lib/libgamename.so"])
  stub("#{SD}/.kidsos/coinmon.elf", "exit 0")
  # the launcher stub: first run starts the arcade game, every later run crashes
  stub("#{SD}/.kidsos/kidsos.elf", <<~SH, log_as: "kidsos")
    n=$(cat /mnt/SDCARD/.kidsos/runs 2> /dev/null || echo 0)
    n=$((n + 1))
    echo $n > /mnt/SDCARD/.kidsos/runs
    echo "kidsos run $n meterWidth=$(cat /mnt/SDCARD/.tmp_update/config/display/meterWidth)" >> /mnt/SDCARD/test.log
    case $n in
    	1)
    		echo "LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so /mnt/SDCARD/.kidsos/game.sh \\"/mnt/SDCARD/Emu/ARCADE/launch.sh\\" \\"/mnt/SDCARD/Roms/ARCADE/game.zip\\"" > /tmp/cmd_to_run.sh
    		chmod +x /tmp/cmd_to_run.sh
    		touch /mnt/SDCARD/.tmp_update/.runGameSwitcher
    		;;
    	*) exit 3 ;;
    esac
  SH
end

def build_sd_card
  pak = File.join(WORK, "onion.pak")
  FileUtils.rm_rf(Dir.children(WORK).map { |c| File.join(WORK, c) } - [pak]) if File.directory?(WORK)
  FileUtils.mkdir_p([SYSDIR, STUBS, "#{SD}/RetroArch", "#{SD}/miyoo/app", "#{SD}/miyoo/lib", "#{SD}/.kidsos"])

  if DIST_ZIP
    unpack_pak
  else
    onion_update = File.join(ONION, "static/build/.tmp_update")
    FileUtils.cp(RUNTIME_SH, File.join(SYSDIR, "runtime.sh"))
    FileUtils.cp_r(File.join(onion_update, "script"), SYSDIR)
    FileUtils.cp_r(File.join(onion_update, "config"), SYSDIR)
    # the launcher as the device would receive it from "make deploy"
    %w[launch.sh game.sh startup.sh install.sh uninstall.sh].each { |f| FileUtils.cp("#{ROOT}/onion/#{f}", "#{SD}/.kidsos/") }
  end
  stub_scripts
  stub_device_binaries

  %w[startup/app startup/tab].each { |f| FileUtils.mkdir_p(File.dirname("#{SYSDIR}/config/#{f}")); File.write("#{SYSDIR}/config/#{f}", "0\n") }
  File.write("#{SYSDIR}/config/.tz", "UTC\n")
  FileUtils.touch("#{SYSDIR}/config/.logging")
  FileUtils.mkdir_p("#{SYSDIR}/config/display")
  File.write("#{SYSDIR}/config/display/meterWidth", "4")
  FileUtils.mkdir_p("#{SYSDIR}/config/system")
  File.write(KEYMAP, ORIGINAL_KEYMAP)
  FileUtils.mkdir_p("#{SYSDIR}/res")
  File.write("#{SYSDIR}/res/miyoo354_system.json", "{\"vol\": 20, \"mute\": 0}\n")
  FileUtils.touch("#{SD}/cachefile") # create_swap would dd 128 MB otherwise

  # system commands the device has and the container must fake
  stub("#{STUBS}/mount", <<~SH)
    if [ "$1" = "-o" ] && [ "$2" = "bind" ]; then
    	echo "mount bind $3 -> $4" >> /mnt/SDCARD/test.log
    	rm -f "$4" && cp -p "$3" "$4" 2> /dev/null
    fi
    exit 0
  SH
  stub("#{STUBS}/shutdown", "kill $PPID", log_as: "shutdown")
  stub("#{STUBS}/reboot", "kill $PPID", log_as: "reboot")
  %w[umount sync fbset killall swapon mkswap ip ifconfig].each { |b| stub("#{STUBS}/#{b}", "exit 0") }
  stub("#{STUBS}/jsonval", <<~SH)
    case "$1" in
    	theme) echo "/mnt/SDCARD/Themes/Default/" ;;
    	vol) echo 20 ;;
    	*) echo 0 ;;
    esac
  SH

  # the game is an arcade one, launched through game.sh like the launcher does; Onion's launch script
  # names the core, whose .info gives the folder of the RetroArch override. While it runs, keymon reacts
  # to MENU by pausing RetroArch and starting the GameSwitcher overlay.
  FileUtils.mkdir_p("#{SD}/RetroArch/.retroarch/cores")
  FileUtils.mkdir_p("#{SD}/Roms/ARCADE")
  FileUtils.touch("#{SD}/Roms/ARCADE/game.zip")
  File.write("#{SD}/RetroArch/.retroarch/cores/fakearcade_libretro.info", "display_name = \"Fake\"\ncorename = \"Fake Arcade\"\n")
  stub("#{SD}/Emu/ARCADE/launch.sh", <<~SH, log_as: "fakeapp")
    echo "fake retroarch -v -L .retroarch/cores/fakearcade_libretro.so" > /dev/null
    /mnt/SDCARD/.tmp_update/bin/gameSwitcher --overlay
    [ -f /mnt/SDCARD/.tmp_update/cmd_to_run.sh ] && echo "cmd_to_run.sh kept during overlay" >> /mnt/SDCARD/test.log
    [ -f /tmp/kidsos_coin.cfg ] && echo "coin cfg during game: $(tr '\\n' ' ' < /tmp/kidsos_coin.cfg) via $0" >> /mnt/SDCARD/test.log
    exit 0
  SH
end

def in_container(script)
  cmd = ["docker", "run", "--rm", "-v", "#{SD}:/mnt/SDCARD", "-v", "#{STUBS}:/stubs:ro", IMAGE, "sh", "-c", <<~SH]
    mkdir -p /appconfigs /customer/app /customer/lib
    cp /stubs/jsonval /customer/app/jsonval
    export PATH=/stubs:$PATH
    cd /mnt/SDCARD/.tmp_update
    #{script}
  SH
  out = IO.popen(cmd, err: [:child, :out], &:read)
  [$?.success?, out]
end

$failures = []
def check(name, ok, detail = nil)
  puts "#{ok ? "ok  " : "FAIL"} #{name}"
  $failures << [name, detail] unless ok
end

def digest(path) = Digest::SHA256.file(path).hexdigest

def keymap_value(key)
  File.read(KEYMAP)[/"#{key}": *(-?\d+)/, 1]&.to_i
rescue Errno::ENOENT
  nil
end

def events = (File.read("#{SD}/test.log") rescue "").lines.map(&:strip)
def install_log = File.read(INSTALL_LOG) rescue ""

def check_boot_sequence(sequence)
  log = events
  positions = sequence.map { |e| log.index { |l| l.start_with?(e) } }
  check "boot sequence #{sequence.join(" > ")}", positions.none?(&:nil?) && positions == positions.sort, log.join("\n")
end

build_sd_card
originals = (MAINUIS + %w[gameSwitcher runtime.sh]).to_h { |f| [f, digest(File.join(SYSDIR, f == "runtime.sh" ? f : "bin/#{f}"))] }

if DIST_ZIP
  puts "== first boot from the zip's onion.pak (#{File.basename(DIST_ZIP)})"
  check "startup hook is in the pak", File.exist?("#{SYSDIR}/startup/kidsos.sh")
  check "launcher is in the pak", File.exist?("#{SD}/.kidsos/install.sh") && File.exist?("#{SD}/.kidsos/version.txt")
  ok, out = in_container(<<~SH)
    timeout 120 sh ./runtime.sh
    echo "runtime.sh exited with $?" >> /mnt/SDCARD/test.log
  SH
  install_out = install_log
  check "startup hook ran install.sh at boot", install_out.include?("installed; reboot"), install_out
  check "startup hook left itself alone", install_out.include?("startup hook already installed"), install_out
else
  puts "== install + boot (#{RUNTIME_SH})"
  ok, out = in_container(<<~SH)
    sh /mnt/SDCARD/.kidsos/install.sh || exit 1
    timeout 120 sh ./runtime.sh
    echo "runtime.sh exited with $?" >> /mnt/SDCARD/test.log
  SH
  install_out = out
  check "install.sh succeeded", install_out.include?("installed; reboot"), install_out
  check "install.sh wrote the startup hook", install_out.include?("startup hook installed") && File.exist?("#{SYSDIR}/startup/kidsos.sh"), install_out
end
log = events.join("\n")
check "runtime.sh untouched", digest("#{SYSDIR}/runtime.sh") == originals["runtime.sh"]
MAINUIS.each { |ui| check "#{ui}.real is the original", digest("#{SYSDIR}/bin/#{ui}.real") == originals[ui] }
check "shims pass busybox sh -n", in_container("for f in bin/MainUI-* bin/gameSwitcher startup/kidsos.sh /mnt/SDCARD/.kidsos/*.sh; do sh -n $f || exit 1; done").first
check_boot_sequence ["kidsos run 1", "fakeapp", "real gameSwitcher --overlay", "cmd_to_run.sh kept during overlay",
                     "kidsos run 2", "real MainUI-354-clean", "shutdown"]
check "GameSwitcher was requested", File.read("#{SYSDIR}/logs/runtime.log").include?("Launch switcher") rescue check("runtime.log present", false)
check "GameSwitcher after the game was skipped in kids mode", events.none? { |l| l == "real gameSwitcher" }, log
check "game ran once, not relaunched after the skipped GameSwitcher", events.count { |l| l.start_with?("fakeapp") } == 1, log
kidsos_log = File.read("#{SD}/.kidsos/kidsos.log") rescue ""
previous_log = File.read("#{SD}/.kidsos/kidsos.log.1") rescue ""
check "launch.sh flagged the crash", kidsos_log.include?("exited with 3"), kidsos_log
check "launch.sh kept the previous log", previous_log.include?("exited with 0"), previous_log
check "keymon volume bar hidden while the launcher runs", events.any? { |l| l.start_with?("kidsos run 1 meterWidth=0") }, log
check "keymon volume bar width restored afterwards", (File.read("#{SYSDIR}/config/display/meterWidth") == "4" rescue false)
check "coin key moved while the arcade game runs (game.sh runs a patched copy of the launch script)",
      events.any? { |l| l.start_with?("coin cfg during game:") && l.include?('input_player1_select = "c"') && l.include?('input_player1_select_btn = "nul"') && l.include?("launch.kidsos.sh") }, log
check "patched launch script removed after the game", !File.exist?("#{SD}/Emu/ARCADE/launch.kidsos.sh"), log
check "runtime.sh ended by shutdown, not by timeout", events.none? { |l| l.include?("exited with 124") }, log
ingame = %w[ingame_single_press ingame_long_press ingame_double_press].map { |k| keymap_value(k) }
mainui = %w[mainui_single_press mainui_long_press mainui_double_press].map { |k| keymap_value(k) }
keymap = File.read(KEYMAP) rescue ""
check "keymap: MENU in a game saves and exits, no long or double press", ingame == [2, 0, 0], keymap
check "keymap: MainUI actions kept", mainui == [0, 1, 2], keymap
check "keymap: button shortcuts kept",
      keymap.include?('"mainui_button_x": "app:Tweaks"') && keymap.include?('"mainui_button_y": "glo"'), keymap
check "keymap: original backed up", (File.read("#{KEYMAP}.kidsos-orig") == ORIGINAL_KEYMAP rescue false)
check "keymap: keymon told to reload", install_out.include?("keymap set")
check "RetroArch save state thumbnails switched on", events.include?(%q{savestate_thumbnail_enable = "true"}), log

puts "== next boot: the startup hook changes nothing"
ok, out = in_container("sh startup/kidsos.sh")
second = install_log
check "hook exits 0", ok, out
check "shims left alone", second.scan("already shimmed").size == MAINUIS.size + 1, second
check "keymap left alone", second.include?("keymap already set"), second
check "RetroArch config left alone", second.include?("thumbnails already enabled") && events.count { |l| l == "patch_ra_cfg /tmp/kidsos_ra.cfg" } == 1, second

if DIST_ZIP
  puts "== Onion update: the installer stub wipes .tmp_update except config/ and onionVersion/, the pak is unpacked again"
  ok, out = in_container(<<~SH)
    find * -maxdepth 0 -not -name "onionVersion" -not -name "config" -exec rm -rf {} +
  SH
  check "shims and hook gone after the wipe", !File.exist?("#{SYSDIR}/bin/gameSwitcher") && !File.exist?("#{SYSDIR}/startup/kidsos.sh"), out
  unpack_pak
  stub_scripts
  stub_device_binaries
  FileUtils.rm_f("#{SD}/test.log")
  ok, out = in_container(<<~SH)
    timeout 120 sh ./runtime.sh
    echo "runtime.sh exited with $?" >> /mnt/SDCARD/test.log
  SH
  third = install_log
  check "hook reinstalled the shims after the update", third.scan(" shimmed").size == MAINUIS.size + 1 && third.include?("installed; reboot"), third
  check "keymap survived the update", third.include?("keymap already set"), third
  check_boot_sequence ["kidsos run 3", "real MainUI-354-clean", "shutdown"]
end

puts "== reinstall with a stale shim + uninstall"
File.write("#{SYSDIR}/bin/gameSwitcher", "#!/bin/sh\n# KIDSOS\nexit 0\n")
ok, out = in_container(<<~SH)
  sh /mnt/SDCARD/.kidsos/install.sh || exit 1
  sh /mnt/SDCARD/.kidsos/uninstall.sh
SH
check "second install.sh leaves current shims alone", out.scan("already shimmed").size == MAINUIS.size, out
check "second install.sh replaces the stale shim", out.include?("gameSwitcher shimmed"), out
check "second install.sh leaves the keymap alone", out.include?("keymap already set"), out
check "uninstall.sh ran", out.include?("uninstalled"), out
check "uninstall.sh switches the thumbnails off again", out.include?("thumbnails back to default"), out
check "uninstall.sh removes the startup hook", out.include?("startup hook removed") && !File.exist?("#{SYSDIR}/startup/kidsos.sh"), out
(MAINUIS + %w[gameSwitcher]).each do |b|
  check "#{b} restored", File.exist?("#{SYSDIR}/bin/#{b}") && digest("#{SYSDIR}/bin/#{b}") == originals[b]
end
check "no .real left behind", Dir.glob("#{SYSDIR}/bin/*.real").empty?
check "keymap restored", (File.read(KEYMAP) == ORIGINAL_KEYMAP rescue false) && !File.exist?("#{KEYMAP}.kidsos-orig")
check ".kidsos removed", !File.exist?("#{SD}/.kidsos")

puts "== install without a keymap.json + uninstall"
# keep the boot scenario logs around for debugging
FileUtils.mkdir_p("#{ROOT}/build/onion-test-logs")
FileUtils.cp("#{SD}/test.log", "#{ROOT}/build/onion-test-logs/boot-test.log") rescue nil
FileUtils.cp("#{SYSDIR}/logs/runtime.log", "#{ROOT}/build/onion-test-logs/boot-runtime.log") rescue nil
build_sd_card
FileUtils.rm_f(KEYMAP)
ok, out = in_container(<<~SH)
  sh /mnt/SDCARD/.kidsos/install.sh || exit 1
  cp /mnt/SDCARD/.tmp_update/config/keymap.json /mnt/SDCARD/keymap-installed.json
  sh /mnt/SDCARD/.kidsos/uninstall.sh
SH
installed = File.read("#{SD}/keymap-installed.json") rescue ""
expected = ['"mainui_single_press": 1', '"mainui_double_press": 2', '"ingame_single_press": 2', '"mainui_button_x": ""']
check "keymap written with Onion's MainUI defaults", expected.all? { |s| installed.include?(s) }, installed
check "keymap removed again", !File.exist?(KEYMAP) && !File.exist?("#{KEYMAP}.kidsos-none")

if $failures.empty?
  puts "\nall checks passed"
else
  puts "\n#{$failures.size} check(s) failed:"
  $failures.each { |name, detail| puts "- #{name}"; puts detail.to_s.lines.last(40).join if detail }
  exit 1
end
