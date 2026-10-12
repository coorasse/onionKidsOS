#!/usr/bin/env python3
"""Draws the KidsOS picture, a child seen from the front playing a Game Boy, with flat shapes in the style
of Onion's app icons (thick white outline, soft shadow). Writes the 74x74 app icon for Onion's Apps tab
(onion/app/KidsOS/icon.png) and a large version (onion/app/KidsOS/mascot.png) that the boot screen and
the README use. Everything is drawn at 4x and scaled down."""
import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(__file__)
APP = os.path.join(HERE, "..", "onion", "app", "KidsOS")

SKIN = (255, 213, 170, 255)
HAIR = (120, 72, 40, 255)
DARK = (60, 40, 30, 255)
SHIRT = (90, 150, 255, 255)
SHIRT_DARK = (60, 110, 210, 255)
CHEEK = (255, 150, 120, 150)
GREY = (200, 202, 208, 255)
GREY_DARK = (120, 122, 130, 255)
SCREEN = (120, 190, 110, 255)
SCREEN_DARK = (70, 120, 70, 255)
MAGENTA = (190, 60, 120, 255)
WHITE = (255, 255, 255, 255)


def figure(s):
    """The child, on a transparent s x s canvas, coordinates in fractions of s."""
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    box = lambda x0, y0, x1, y1: (x0 * s, y0 * s, x1 * s, y1 * s)

    # body (shirt) with a collar line
    d.rounded_rectangle(box(0.30, 0.56, 0.70, 0.92), radius=0.10 * s, fill=SHIRT)
    d.rounded_rectangle(box(0.30, 0.84, 0.70, 0.92), radius=0.04 * s, fill=SHIRT_DARK)
    # head and hair
    d.ellipse(box(0.30, 0.16, 0.70, 0.58), fill=SKIN)
    d.chord(box(0.30, 0.16, 0.70, 0.58), start=190, end=350, fill=HAIR)
    d.ellipse(box(0.30, 0.22, 0.42, 0.40), fill=HAIR)   # side tufts
    d.ellipse(box(0.58, 0.22, 0.70, 0.40), fill=HAIR)
    d.ellipse(box(0.33, 0.28, 0.67, 0.58), fill=SKIN)   # face again, over the tufts
    # face
    d.ellipse(box(0.41, 0.39, 0.46, 0.44), fill=DARK)
    d.ellipse(box(0.54, 0.39, 0.59, 0.44), fill=DARK)
    d.arc(box(0.44, 0.42, 0.56, 0.52), start=20, end=160, fill=DARK, width=max(1, int(0.015 * s)))
    d.ellipse(box(0.34, 0.45, 0.41, 0.50), fill=CHEEK)
    d.ellipse(box(0.59, 0.45, 0.66, 0.50), fill=CHEEK)
    # the Game Boy, held in front
    d.rounded_rectangle(box(0.37, 0.60, 0.63, 0.88), radius=0.03 * s, fill=GREY)
    d.rounded_rectangle(box(0.405, 0.625, 0.595, 0.73), radius=0.015 * s, fill=GREY_DARK)
    d.rectangle(box(0.43, 0.64, 0.57, 0.715), fill=SCREEN)
    d.rectangle(box(0.43, 0.64, 0.57, 0.715), outline=SCREEN_DARK, width=max(1, int(0.006 * s)))
    d.rectangle(box(0.425, 0.775, 0.475, 0.79), fill=GREY_DARK)   # d-pad
    d.rectangle(box(0.4425, 0.757, 0.4575, 0.808), fill=GREY_DARK)
    d.ellipse(box(0.52, 0.78, 0.55, 0.81), fill=MAGENTA)        # buttons
    d.ellipse(box(0.555, 0.765, 0.585, 0.795), fill=MAGENTA)
    # hands
    d.ellipse(box(0.31, 0.72, 0.42, 0.83), fill=SKIN)
    d.ellipse(box(0.58, 0.72, 0.69, 0.83), fill=SKIN)
    return img


def render(size, ss=4):
    s = size * ss
    fig = figure(s)
    alpha = fig.getchannel("A")
    # white outline: the silhouette grown by a few pixels
    grow = int(0.035 * s) | 1
    outline = alpha.filter(ImageFilter.MaxFilter(grow))
    outline_img = Image.new("RGBA", (s, s), WHITE)
    outline_img.putalpha(outline)
    # soft shadow underneath, like Onion's icons
    shadow = Image.new("RGBA", (s, s), (40, 30, 20, 0))
    shadow.putalpha(outline.point(lambda a: a * 90 // 255))
    shadow = shadow.transform((s, s), Image.AFFINE, (1, 0, 0, 0, 1, -0.02 * s)).filter(
        ImageFilter.GaussianBlur(0.02 * s))
    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.alpha_composite(shadow)
    out.alpha_composite(outline_img)
    out.alpha_composite(fig)
    return out.resize((size, size), Image.LANCZOS)


render(74).save(os.path.join(APP, "icon.png"))
render(400).save(os.path.join(APP, "mascot.png"))
print("icon and mascot written to", os.path.normpath(APP))
