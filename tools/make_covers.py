#!/usr/bin/env python3
"""Create two fake SD cards for desktop development, sdcard/ (MinUI layout) and sdcard-onion/ (Onion layout),
with empty roms and placeholder box art."""
import os
import sys
from PIL import Image, ImageDraw, ImageFont


def write_favourites(root, entries):
    """Onion's Roms/favourite.json: one object per line, paths as MainUI writes them."""
    lines = []
    for entry in entries:
        tag, rom = entry.split("/", 1)
        name = os.path.splitext(os.path.basename(rom))[0]
        lines.append('{"label":"%s","launch":"/mnt/SDCARD/Emu/%s/launch.sh","type":5,'
                     '"imgpath":"/mnt/SDCARD/Emu/%s/../../Roms/%s/Imgs/%s.png",'
                     '"rompath":"/mnt/SDCARD/Emu/%s/../../Roms/%s/%s"}' % (name, tag, tag, tag, name, tag, tag, rom))
    os.makedirs(os.path.join(root, "Roms"), exist_ok=True)
    with open(os.path.join(root, "Roms", "favourite.json"), "w") as f:
        f.write("\n".join(lines) + "\n")


# make_covers.py --favourites <root> <TAG/rom>...: only write that favourites file (used by the Makefile)
if len(sys.argv) > 2 and sys.argv[1] == "--favourites":
    write_favourites(sys.argv[2], sys.argv[3:])
    sys.exit(0)


ROOT = os.path.join(os.path.dirname(__file__), "..", "sdcard")
ONION_ROOT = os.path.join(os.path.dirname(__file__), "..", "sdcard-onion")

# MinUI system folder -> Onion emulator tag and its extension list
ONION_SYSTEMS = {
    "Game Boy (GB)": ("GB", "Game Boy", "gb|gbc|zip|7z"),
    "Game Boy Advance (GBA)": ("GBA", "Game Boy Advance", "bin|gba|zip|7z"),
    "Super Nintendo (SFC)": ("SFC", "Super Nintendo", "smc|fig|sfc|gd3|gd7|dx2|bsx|swc|zip|7z"),
    "Nintendo Entertainment System (FC)": ("FC", "NES", "nes|fds|unf|zip|7z"),
    "Sega Genesis (MD)": ("MD", "Mega Drive", "bin|gen|md|sg|smd|zip|7z"),
    "Sony PlayStation (PS)": ("PS", "PlayStation", "bin|cue|img|mdf|pbp|toc|cbn|m3u|ccd|chd|zip|7z"),
    "Arcade (FBN)": ("ARCADE", "Arcade", "zip|7z"),
}
FONT = "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf"

# (system folder, rom file, cover size or None, colors)
GAMES = [
    ("Game Boy (GB)", "Tetris.gb", (240, 240), ("#2b5876", "#4e4376")),
    ("Game Boy (GB)", "Kirby's Dream Land.gb", (240, 240), ("#ff9a9e", "#fad0c4")),
    ("Game Boy Advance (GBA)", "Pokemon Emerald.gba", (320, 320), ("#11998e", "#38ef7d")),
    ("Game Boy Advance (GBA)", "Mario Kart Super Circuit.gba", None, None),
    ("Game Boy Advance (GBA)", "Metroid Fusion (Europe) [!].gba", None, None),
    ("Super Nintendo (SFC)", "Super Mario World.sfc", (300, 210), ("#f7971e", "#ffd200")),
    ("Super Nintendo (SFC)", "Donkey Kong Country.sfc", None, None),
    ("Nintendo Entertainment System (FC)", "Super Mario Bros. 3.nes", (260, 360), ("#c31432", "#240b36")),
    ("Sega Genesis (MD)", "Sonic the Hedgehog 2.md", (300, 420), ("#1e3c72", "#2a5298")),
    ("Sony PlayStation (PS)", "Crash Bandicoot.cue", (300, 300), ("#f12711", "#f5af19")),
    ("Arcade (FBN)", "Street Fighter II.zip", (300, 400), ("#8e2de2", "#4a00e0")),
    ("Arcade (FBN)", "Pac-Man.zip", None, None),
    ("Sony PlayStation (PS)", "Crash Bandicoot.bin", None, None),
    ("Sony PlayStation (PS)", "Final Fantasy VII/Final Fantasy VII.m3u", None, None),
    ("Sony PlayStation (PS)", "Final Fantasy VII/Final Fantasy VII (Disc 1).cue", None, None),
    ("Sony PlayStation (PS)", "Final Fantasy VII/Final Fantasy VII (Disc 1).bin", None, None),
    ("Sony PlayStation (PS)", "Final Fantasy VII/Final Fantasy VII (Disc 2).cue", None, None),
    ("Sony PlayStation (PS)", "Final Fantasy VII/Final Fantasy VII (Disc 2).bin", None, None),
]
# cover for the multi-disc folder lives next to the folder: Sony PlayStation (PS)/.res/Final Fantasy VII.png
FOLDER_COVERS = [("Sony PlayStation (PS)", "Final Fantasy VII", (300, 300), ("#7303c0", "#ec38bc"))]

ENABLED = [
    "Game Boy (GB)/Tetris.gb",
    "Game Boy Advance (GBA)/Pokemon Emerald.gba",
    "Game Boy Advance (GBA)/Mario Kart Super Circuit.gba",
    "Super Nintendo (SFC)/Super Mario World.sfc",
    "Sega Genesis (MD)/Sonic the Hedgehog 2.md",
    "Sony PlayStation (PS)/Crash Bandicoot.cue",
    "Arcade (FBN)/Street Fighter II.zip",
]


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def cover(path, title, size, colors):
    w, h = size
    img = Image.new("RGB", size)
    top, bottom = hex_rgb(colors[0]), hex_rgb(colors[1])
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    draw = ImageDraw.Draw(img)
    draw.ellipse((w * 0.6, -h * 0.2, w * 1.3, h * 0.5), fill=tuple(min(255, c + 40) for c in bottom))
    font_size = int(min(w, h) / 7)
    font = ImageFont.truetype(FONT, font_size)
    words, lines, line = title.split(), [], ""
    for word in words:
        trial = (line + " " + word).strip()
        if draw.textlength(trial, font=font) > w - 30 and line:
            lines.append(line)
            line = word
        else:
            line = trial
    lines.append(line)
    y = h - 24 - len(lines) * font_size * 1.15
    for text in lines:
        draw.text((16, y), text, font=font, fill="white")
        y += font_size * 1.15
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)


for system, rom, size, colors in GAMES:
    rom_path = os.path.join(ROOT, "Roms", system, rom)
    os.makedirs(os.path.dirname(rom_path), exist_ok=True)
    open(rom_path, "a").close()
    if size:
        res = os.path.join(os.path.dirname(rom_path), ".res", os.path.basename(rom) + ".png")
        cover(res, os.path.splitext(os.path.basename(rom))[0], size, colors)

for system, folder, size, colors in FOLDER_COVERS:
    cover(os.path.join(ROOT, "Roms", system, ".res", folder + ".png"), folder, size, colors)

os.makedirs(os.path.join(ROOT, "kids"), exist_ok=True)
with open(os.path.join(ROOT, "kids", "enabled.txt"), "w") as f:
    f.write("# Games visible to the kid, one per line, relative to Roms/\n")
    f.write("\n".join(ENABLED) + "\n")
print("fake sdcard ready")

# ---- Onion layout: Emu/<TAG>/config.json + Roms/<TAG>/ + Roms/<TAG>/Imgs/<name>.png ----
for system, (tag, label, extlist) in ONION_SYSTEMS.items():
    emu = os.path.join(ONION_ROOT, "Emu", tag)
    os.makedirs(emu, exist_ok=True)
    with open(os.path.join(emu, "config.json"), "w") as f:
        f.write('{\n"label":"%s",\n"launch":"launch.sh",\n"rompath":"../../Roms/%s",\n'
                '"imgpath":"../../Roms/%s/Imgs",\n"extlist":"%s"\n}\n' % (label, tag, tag, extlist))
    with open(os.path.join(emu, "launch.sh"), "w") as f:
        f.write("#!/bin/sh\n# placeholder, the real one comes with Onion\n")

for system, rom, size, colors in GAMES:
    tag = ONION_SYSTEMS[system][0]
    rom_path = os.path.join(ONION_ROOT, "Roms", tag, rom)
    os.makedirs(os.path.dirname(rom_path), exist_ok=True)
    open(rom_path, "a").close()
    if size:
        name = os.path.splitext(os.path.basename(rom))[0]
        cover(os.path.join(ONION_ROOT, "Roms", tag, "Imgs", name + ".png"), name, size, colors)

for system, folder, size, colors in FOLDER_COVERS:
    tag = ONION_SYSTEMS[system][0]
    cover(os.path.join(ONION_ROOT, "Roms", tag, "Imgs", folder + ".png"), folder, size, colors)

# RetroArch auto save states (what the resume screen looks for), one with a thumbnail and one without
STATES = os.path.join(ONION_ROOT, "Saves", "CurrentProfile", "states", "mGBA")
os.makedirs(STATES, exist_ok=True)
for name, thumb in (("Pokemon Emerald", True), ("Mario Kart Super Circuit", False)):
    open(os.path.join(STATES, name + ".state.auto"), "a").close()
    if thumb:
        img = Image.new("RGB", (640, 480), hex_rgb("#1b5e20"))
        px = img.load()
        for y in range(480):
            for x in range(640):
                if (x // 40 + y // 40) % 2 == 0:
                    px[x, y] = (46, 125, 50)
        draw = ImageDraw.Draw(img)
        draw.rectangle((40, 360, 600, 440), fill=(20, 20, 60))
        draw.text((60, 375), "Saved game screenshot", font=ImageFont.truetype(FONT, 40), fill="white")
        draw.ellipse((260, 140, 380, 260), fill=(255, 202, 40))
        img.save(os.path.join(STATES, name + ".state.auto.png"))

write_favourites(ONION_ROOT, ["%s/%s" % (ONION_SYSTEMS[e.split("/", 1)[0]][0], e.split("/", 1)[1]) for e in ENABLED])
print("fake onion sdcard ready")
