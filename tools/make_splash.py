#!/usr/bin/env python3
"""Builds the KidsOS boot screen (onion/bootScreen.png, 640x480) from the mascot picture: the picture on
the left, the KidsOS wordmark on the right, Onion's dark background. The bottom strip stays free because
Onion's bootScreen draws the version there (left) and the shutdown message (right).

usage: make_splash.py [mascot.png]   (default: onion/app/KidsOS/mascot.png)"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..")
MASCOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "onion", "app", "KidsOS", "mascot.png")
OUT = os.path.join(ROOT, "onion", "bootScreen.png")
FONT = os.path.join(ROOT, "assets", "BPreplayBold-unhinted.otf")

img = Image.new("RGBA", (640, 480), (0x23, 0x25, 0x2E, 255))  # same dark grey as Onion's boot screen
mascot = Image.open(MASCOT).convert("RGBA")
mascot = mascot.crop(mascot.getbbox())  # the picture has transparent margins; lay out what is visible
mascot.thumbnail((260, 260), Image.LANCZOS)

big = ImageFont.truetype(FONT, 78)
small = ImageFont.truetype(FONT, 24)
draw = ImageDraw.Draw(img)
kids_w = draw.textlength("Kids", font=big)
title_w = kids_w + 6 + draw.textlength("OS", font=big)
text_w = max(title_w, 4 + draw.textlength("pick a game, play", font=small))
gap = 46
x = (640 - (mascot.width + gap + text_w)) // 2
img.alpha_composite(mascot, (int(x), 230 - mascot.height // 2))

x = int(x + mascot.width + gap)
draw.text((x, 170), "Kids", font=big, fill=(0xF5, 0xB8, 0x00, 255))
draw.text((x + kids_w + 6, 170), "OS", font=big, fill=(255, 255, 255, 255))
draw.text((x + 4, 262), "pick a game, play", font=small, fill=(0xB8, 0xB0, 0xA4, 255))

img.save(OUT)
print("splash written to", os.path.normpath(OUT))
