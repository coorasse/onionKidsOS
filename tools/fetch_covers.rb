#!/usr/bin/env ruby
# frozen_string_literal: true

# Downloads box art for every rom of an OnionOS SD card from thumbnails.libretro.com
# into Roms/<TAG>/Imgs/<rom>.png, where both Onion's MainUI and Kids OS look for it.
#
#   tools/fetch_covers.rb /Volumes/ONIONOS            # all systems
#   tools/fetch_covers.rb /Volumes/ONIONOS GB SFC     # only these
#   tools/fetch_covers.rb /Volumes/ONIONOS --dry-run  # only report what would be fetched
#
# Console roms carry No-Intro names, which are the names of the libretro thumbnails. Arcade roms are
# resolved through Onion's BIOS/arcade_lists/arcade-rom-names.txt. When no exact match exists, the
# cover of another release of the same title (other region, revision) is used.

require "cgi"
require "fileutils"
require "json"
require "net/http"
require "open3"
require "optparse"
require "tmpdir"
require "uri"

SYSTEMS = {
  "ATARI" => ["Atari - 2600"],
  "FC" => ["Nintendo - Nintendo Entertainment System", "Nintendo - Family Computer Disk System"],
  "GB" => ["Nintendo - Game Boy"],
  "GBA" => ["Nintendo - Game Boy Advance"],
  "GBC" => ["Nintendo - Game Boy Color"],
  "GG" => ["Sega - Game Gear"],
  "MD" => ["Sega - Mega Drive - Genesis"],
  "MS" => ["Sega - Master System - Mark III"],
  "NDS" => ["Nintendo - Nintendo DS"],
  "PCE" => ["NEC - PC Engine - TurboGrafx 16", "NEC - PC Engine CD - TurboGrafx-CD"],
  "PS" => ["Sony - PlayStation"],
  "SFC" => ["Nintendo - Super Nintendo Entertainment System"],
  "ARCADE" => ["MAME", "FBNeo - Arcade Games"],
  "NEOGEO" => ["SNK - Neo Geo", "MAME", "FBNeo - Arcade Games"]
}.freeze
ARCADE_TAGS = %w[ARCADE NEOGEO].freeze

HOST = "thumbnails.libretro.com"
REGION_PREFERENCE = ["(World", "(USA", "(Europe", "(Japan"].freeze

options = { width: 250, threads: 8, dry_run: false, cache: File.join(Dir.home, ".cache", "libretro-thumbnails") }
OptionParser.new do |o|
  o.banner = "usage: #{$PROGRAM_NAME} <sdcard root> [TAG ...] [options]"
  o.on("--dry-run") { options[:dry_run] = true }
  o.on("--width N", Integer) { |n| options[:width] = n }
  o.on("--threads N", Integer) { |n| options[:threads] = n }
  o.on("--cache DIR") { |d| options[:cache] = d }
end.parse!
root = ARGV.shift or abort("usage: #{$PROGRAM_NAME} <sdcard root> [TAG ...]")
tags = ARGV.empty? ? SYSTEMS.keys : ARGV

def encode(segment) = URI.encode_www_form_component(segment).gsub("+", "%20")

# libretro replaces the characters a filesystem may reject with an underscore.
def libretro_name(name) = name.gsub(%r{[&*/:`<>?\\|"]}, "_")

def title_key(name) = name.sub(/\s*\(.*\z/, "").downcase.gsub(/[^a-z0-9]/, "")

class Index
  def initialize(system, cache_dir)
    @names = load(system, cache_dir)
    @by_title = @names.group_by { |n| title_key(n) }
  end

  def lookup(name)
    return name if @names.include?(name)

    candidates = @by_title[title_key(name)] or return nil
    REGION_PREFERENCE.each { |r| candidates.find { |c| c.include?(r) }&.then { |c| return c } }
    candidates.first
  end

  private

  def load(system, cache_dir)
    FileUtils.mkdir_p(cache_dir)
    cache = File.join(cache_dir, "#{system}.txt")
    unless File.exist?(cache) && File.mtime(cache) > Time.now - 7 * 24 * 3600
      html = Net::HTTP.get(URI("https://#{HOST}/#{encode(system)}/Named_Boxarts/"))
      names = html.scan(/href="([^"]+\.png)"/).flatten.map { |h| URI.decode_uri_component(CGI.unescapeHTML(h)).delete_suffix(".png") }
      File.write(cache, names.join("\n"))
    end
    File.readlines(cache, chomp: true).to_set
  end
end

def arcade_names(root)
  @arcade_names ||= File.readlines(File.join(root, "BIOS/arcade_lists/arcade-rom-names.txt"), chomp: true)
    .filter_map { |l| l.match(/\A(\S+)\t"(.*)"\z/)&.captures }.to_h
end

# Systems without an emulator installed in Onion keep Onion's default layout.
def roms_for(root, tag)
  config_file = File.join(root, "Emu", tag, "config.json")
  config = File.exist?(config_file) ? JSON.parse(File.read(config_file)) : {}
  rom_dir = File.expand_path(config.fetch("rompath", "../../Roms/#{tag}"), File.join(root, "Emu", tag))
  img_dir = File.expand_path(config.fetch("imgpath", "../../Roms/#{tag}/Imgs"), File.join(root, "Emu", tag))
  exts = config.fetch("extlist", "zip|7z").split("|").map(&:downcase)
  roms = Dir.children(rom_dir).reject { |f| f.start_with?(".") }.select do |f|
    exts.include?(File.extname(f).delete_prefix(".").downcase)
  end
  [roms.sort, img_dir]
end

def download(http, system, name)
  path = "/#{encode(system)}/Named_Boxarts/#{encode(name)}.png"
  res = http.get(path)
  res.is_a?(Net::HTTPSuccess) ? res.body : nil
end

def store(png, dest, width)
  Dir.mktmpdir do |dir|
    src = File.join(dir, "cover.png")
    File.binwrite(src, png)
    _, status = Open3.capture2e("sips", "--resampleWidth", width.to_s, src, "--out", dest)
    FileUtils.cp(src, dest) unless status.success? && File.exist?(dest)
  end
end

indexes = Hash.new { |h, system| h[system] = Index.new(system, options[:cache]) }
summary = []

tags.each do |tag|
  next unless SYSTEMS.key?(tag) && File.directory?(File.join(root, "Roms", tag))

  roms, img_dir = roms_for(root, tag)
  jobs = roms.filter_map do |rom|
    base = File.basename(rom, ".*")
    dest = File.join(img_dir, "#{base}.png")
    next if File.exist?(dest)

    wanted = ARCADE_TAGS.include?(tag) ? arcade_names(root)[base] : base
    next [rom, nil] unless wanted

    hit = SYSTEMS[tag].lazy.filter_map { |sys| indexes[sys].lookup(libretro_name(wanted))&.then { |n| [sys, n] } }.first
    [rom, hit, dest]
  end
  missing = jobs.select { |j| j[1].nil? }.map(&:first)
  todo = jobs.reject { |j| j[1].nil? }
  puts "#{tag}: #{roms.size} roms, #{roms.size - jobs.size} covers already there, #{todo.size} to fetch, #{missing.size} not found"
  missing.each { |m| puts "  missing: #{m}" }
  summary << [tag, roms.size, todo.size, missing.size]
  next if options[:dry_run] || todo.empty?

  FileUtils.mkdir_p(img_dir)
  queue = Queue.new
  todo.each { |j| queue << j }
  failed = Queue.new
  options[:threads].times.map do
    Thread.new do
      Net::HTTP.start(HOST, 443, use_ssl: true) do |http|
        loop do
          rom, (system, name), dest = queue.pop(true)
          png = download(http, system, name)
          png ? store(png, dest, options[:width]) : failed << rom
        rescue ThreadError
          break
        end
      end
    end
  end.each(&:join)
  until failed.empty?
    puts "  download failed: #{failed.pop}"
  end
end

puts
puts format("%-8s %6s %8s %8s", "system", "roms", "fetched", "missing")
summary.each { |tag, n, f, m| puts format("%-8s %6d %8d %8d", tag, n, f, m) }
