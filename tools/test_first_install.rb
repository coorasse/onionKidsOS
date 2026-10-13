#!/usr/bin/env ruby
# Plays the first start of a fresh card with the KidsOS zip on it, the way the stock firmware does it: it runs
# miyoo354/app/MainUI, which hands over to Onion's installer (install.sh as patched by build_dist.rb). The
# installer unpacks the real paks with 7z in an Alpine container; the screen binaries are stubs that append
# to /mnt/SDCARD/test.log. Checks that the install needs no button press from the screen to the reboot, and
# that the install screen is the KidsOS picture.
#
# usage: ruby tools/test_first_install.rb build/KidsOS-<version>.zip
require "fileutils"
require "digest"

ZIP = File.expand_path(ARGV.fetch(0) { abort "usage: #{$0} <KidsOS zip>" })
ROOT = File.expand_path("..", __dir__)
WORK = File.join(ROOT, "build", "first-install-test")
SD = File.join(WORK, "sd")
STUBS = File.join(WORK, "stubs")
LOG = File.join(SD, "test.log")
# the screens that wait for a button
INTERACTIVE = %w[packageManager Onion_Manual prompt infoPanel]

def stub(name, body)
  File.write(File.join(STUBS, name), "#!/bin/sh\necho \"#{name} $*\" >> /mnt/SDCARD/test.log\n#{body}\n")
  File.chmod(0755, File.join(STUBS, name))
end

FileUtils.rm_rf(WORK)
FileUtils.mkdir_p([SD, STUBS])
system("unzip", "-q", ZIP, "-d", SD) || abort("cannot unzip #{ZIP}")
installer = File.read(File.join(SD, "miyoo/app/.tmp_update/install.sh"))

stub("installUI", <<~SH)
  res=/mnt/SDCARD/.tmp_update/res
  echo "installUI background $(md5sum < $res/waitingBG.png | cut -d' ' -f1) slides $(ls $res | grep -c installSlide)" >> /mnt/SDCARD/test.log
SH
stub("Onion_Manual", "exit 0")
stub("jsonval", "echo /mnt/SDCARD/Themes/KidsOS")
%w[packageManager prompt infoPanel themeSwitcher batmon bootScreen freemma detectKey playActivity].each { |b| stub(b, "exit 0") }
# a reboot ends every process; PID 1 (the test script) carries on and looks at the card
stub("reboot", "kill -9 -1")
# the real 7z, then the stubs again over the ARM binaries the pak just unpacked
File.write(File.join(STUBS, "7z.sh"), <<~SH)
  #!/bin/sh
  echo "7z $*" >> /mnt/SDCARD/test.log
  /usr/bin/7z "$@"
  status=$?
  sh /stubs/apply.sh
  exit $status
SH
File.write(File.join(STUBS, "apply.sh"), <<~SH)
  bin=/mnt/SDCARD/.tmp_update/bin
  mkdir -p $bin
  for b in installUI packageManager prompt infoPanel themeSwitcher batmon bootScreen freemma detectKey playActivity reboot; do
    cp /stubs/$b $bin/$b
  done
  echo 'exec sh /stubs/7z.sh "$@"' > $bin/7z
  chmod +x $bin/*
  [ -d /mnt/SDCARD/App/Onion_Manual ] && cp /stubs/Onion_Manual /mnt/SDCARD/App/Onion_Manual/launch.sh
  exit 0
SH

script = <<~SH
  trap 'chown -R #{Process.uid}:#{Process.gid} /mnt/SDCARD' EXIT
  apk add -q p7zip || exit 1
  # FAT32: every file on the card is executable
  chmod -R a+x /mnt/SDCARD
  mkdir -p /customer/app /customer/lib
  touch /customer/app/axp_test /customer/lib/libpadsp.so
  cp /stubs/jsonval /customer/app/jsonval
  mkdir -p /mnt/SDCARD/miyoo/app/.tmp_update/bin
  sed -i 's#^bin=.*#bin=/mnt/SDCARD/miyoo/app/.tmp_update/bin#' /stubs/apply.sh
  sh /stubs/apply.sh
  sed -i 's#^bin=.*#bin=/mnt/SDCARD/.tmp_update/bin#' /stubs/apply.sh
  (timeout 900 sh /mnt/SDCARD/miyoo354/app/MainUI > /mnt/SDCARD/firmware.log 2>&1)
  echo "after reboot" >> /mnt/SDCARD/test.log
SH
puts "== first start of a fresh card (#{File.basename(ZIP)}), this unpacks both paks"
system("docker", "run", "--rm", "-v", "#{SD}:/mnt/SDCARD", "-v", "#{STUBS}:/stubs", "alpine:3.20", "sh", "-c", script) ||
  abort("container failed")

log = File.read(LOG).lines.map(&:chomp)
install_log = File.read(File.join(SD, ".tmp_update/logs/install.log")) rescue ""
failures = []
check = lambda do |name, ok, detail = nil|
  puts "#{ok ? "ok  " : "FAIL"} #{name}"
  failures << [name, detail] unless ok
end

kids_bg = Digest::MD5.file(File.join(ROOT, "onion/installScreen.png")).hexdigest
screens = log.grep(/^installUI background/)
check.("install screen shown", screens.any?, log.join("\n"))
check.("install screen is the KidsOS picture without Onion's slides",
       screens.any? && screens.all? { |l| l == "installUI background #{kids_bg} slides 0" }, screens.join("\n"))
check.("progress says KidsOS, not Onion", installer.include?('install_core "1/2: $verb KidsOS..."') &&
         !installer.include?("$verb Onion"))
check.("both paks unpacked", log.grep(/^7z x -aoa -o.*\.pak/).size == 2, log.join("\n"))
waiting = log.select { |l| INTERACTIVE.include?(l.split.first) }
check.("nothing waits for a button", waiting.empty?, waiting.join("\n"))
check.("installer reboots by itself", log.index("reboot ") && log.index("reboot ") < log.index("after reboot"), log.join("\n"))
check.("installation did not fail", !install_log.include?("Installation failed") &&
         !File.exist?(File.join(SD, ".tmp_update/.installFailed")), install_log)
check.("installer cleaned up, the next boot is Onion's", !File.exist?(File.join(SD, "miyoo/app/MainUI")) &&
         File.exist?(File.join(SD, ".tmp_update/runtime.sh")) && File.exist?(File.join(SD, ".tmp_update/startup/kidsos.sh")))

if failures.empty?
  puts "all checks passed"
else
  failures.each { |name, detail| puts "\n-- #{name}\n#{detail}" if detail }
  exit 1
end
