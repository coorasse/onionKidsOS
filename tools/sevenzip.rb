# 7-Zip for the build scripts: Onion's onion.pak is a 7z archive. Uses 7zz/7z/7za when installed
# (brew install sevenzip), otherwise runs 7z in an Alpine container with root mounted as /work.
# All paths are relative to root; cwd is the subdirectory of root to run in.
module SevenZip
  module_function

  def local_binary
    @local_binary ||= %w[7zz 7z 7za].find { |b| system("which #{b} > /dev/null 2>&1") }
  end

  def run(root, *args, cwd: ".")
    cmd = if local_binary
            [local_binary, *args, { chdir: File.join(root, cwd) }]
          else
            quoted = args.map { |a| "'#{a}'" }.join(" ")
            ["docker", "run", "--rm", "-v", "#{File.expand_path(root)}:/work", "-w", "/work/#{cwd}", "alpine:3.20",
             "sh", "-c", "apk add -q p7zip && 7z #{quoted}"]
          end
    out = IO.popen(cmd, err: [:child, :out], &:read).force_encoding(Encoding::UTF_8).scrub
    raise "7z #{args.first} failed:\n#{out}" unless $?.success?
    out
  end

  def list(root, archive) = run(root, "l", "-ba", "-slt", archive).scan(/^Path = (.*)$/).flatten

  # a new solid archive with fast LZMA2: the Miyoo unpacks it almost twice as fast as Onion's default level
  def create(root, archive, *paths, cwd: ".") = run(root, "a", "-m0=lzma2", "-mx=1", "-bso0", "-bsp0", archive, *paths, cwd: cwd)

  def extract(root, archive, dest, *paths) = run(root, "x", "-aoa", "-bso0", "-bsp0", "-o#{dest}", archive, *paths)
end
