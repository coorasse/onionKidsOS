#!/usr/bin/env python3
"""Convert a raw dump of the Miyoo framebuffer (/dev/fb0, 640x480, 32 bit BGRA) to PNG."""
import sys
from PIL import Image

raw = open(sys.argv[1], "rb").read()
w, h = 640, 480
page = w * h * 4
if len(raw) < page:
    sys.exit(f"framebuffer dump too small: {len(raw)} bytes")
img = Image.frombytes("RGBA", (w, h), raw[:page], "raw", "BGRA")
# the panel is mounted upside down, the framebuffer is stored rotated by 180 degrees
img.convert("RGB").rotate(180).save(sys.argv[2])
print("saved", sys.argv[2])
