#!/usr/bin/env python3
"""Generates the stage-scene pictures (Assets/Scenes/*.jpg, 1200x530) in the style of Assets/cathedral_stage.jpg: dark, warm, volumetric, vignetted.
Deterministic (fixed seeds). Needs numpy, scipy and Pillow.   python3 tools/make_scene_images.py [out_dir]"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

W, H = 1200, 530
ROOT = Path(__file__).resolve().parents[1]
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
AMBER = np.array([1.0, .72, .38], np.float32)
WARM = np.array([1.0, .86, .6], np.float32)


def canvas(base=(.035, .022, .018), top=(.07, .04, .03)):
    t = (yy / H)[..., None]
    return np.array(top, np.float32) * (1 - t) + np.array(base, np.float32) * t


def blur(a, s):
    return np.stack([gaussian_filter(a[..., i], s, mode="constant") for i in range(3)], -1)


def mask_poly(pts, soft=0):
    from PIL import ImageDraw
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).polygon([tuple(p) for p in pts], fill=255)
    a = np.asarray(m, np.float32) / 255
    return gaussian_filter(a, soft) if soft else a


def mask_rect(x0, y0, x1, y1, soft=0):
    return mask_poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], soft)


def mask_ellipse(cx, cy, rx, ry, soft=0):
    from PIL import ImageDraw
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=255)
    a = np.asarray(m, np.float32) / 255
    return gaussian_filter(a, soft) if soft else a


def paint(img, m, colour, k=1.0):
    img += m[..., None] * np.array(colour, np.float32) * k


def glow(img, cx, cy, r, colour, k=1.0, squash=1.0):
    d2 = ((xx - cx) / r) ** 2 + (((yy - cy) / (r * squash))) ** 2
    img += np.exp(-d2 * 2.2)[..., None] * np.array(colour, np.float32) * k


def beam(img, x0, y0, x1a, x1b, y1, colour, k=.5, soft=14):
    m = mask_poly([(x0 - 3, y0), (x0 + 3, y0), (x1b, y1), (x1a, y1)], soft)
    m *= np.clip(1 - (yy - y0) / max(1, (y1 - y0)), 0, 1) ** .7
    paint(img, m, colour, k)


def floor_haze(img, y=.62, k=.16, colour=(1, .6, .35)):
    t = np.clip((yy / H - y) / (1 - y), 0, 1)
    paint(img, np.sin(t * np.pi) ** 1.2 * .9, colour, k)


def finish(img, seed, vig=.85, dust=140):
    rng = np.random.default_rng(seed)
    d = np.zeros((H, W), np.float32)
    for _ in range(dust):
        x, y = rng.integers(0, W), rng.integers(0, int(H * .8))
        d[y, x] = rng.uniform(.3, 1)
    d = gaussian_filter(d, 1.1) * 14
    img += d[..., None] * WARM * .5
    r = np.sqrt(((xx - W / 2) / (W * .62)) ** 2 + ((yy - H * .45) / (H * .75)) ** 2)
    img *= np.clip(1 - vig * r ** 2.2, .04, 1)[..., None]
    img += rng.normal(0, .004, img.shape).astype(np.float32)
    img = np.clip(img, 0, 1) ** (1 / 1.12)
    return (img * 255).astype(np.uint8)


def concert_hall():
    img = canvas()
    glow(img, 600, 150, 420, AMBER, .22, .6)
    for i in range(23):                                  # the organ pipes on the back wall
        x = 430 + i * 14.5
        top = 70 + 50 * abs(i - 11) / 11
        paint(img, mask_rect(x, top, x + 9, 300, 1.2), AMBER, .34 - .01 * abs(i - 11))
    paint(img, mask_rect(420, 296, 780, 306, 2), AMBER, .22)
    for side in (-1, 1):                                 # balcony tiers running toward the stage
        for k in range(4):
            y0 = 120 + k * 52
            xo = 600 + side * 560
            xi = 600 + side * (300 + k * 18)
            paint(img, mask_poly([(xo, y0 - 14), (xi, y0 + 22), (xi, y0 + 32), (xo, y0 + 6)], 1.5), AMBER, .16 - .02 * k)
            for j in range(9):                           # rail lamps
                t = j / 8
                glow(img, xo + (xi - xo) * t, y0 + 10 + 10 * t, 11, WARM, .45 - .04 * k, 1)
    paint(img, mask_rect(300, 80, 900, 88, 3), AMBER, .12)
    for x in range(120, 1100, 90):                       # coffered ceiling hints
        paint(img, mask_rect(x, 8, x + 40, 20, 2), AMBER, .06)
    for x in (260, 940):
        beam(img, x, 0, x - 160, x + 120, 420, WARM, .13, 18)
    floor_haze(img)
    return finish(img, 11)


def studio_booth():
    img = canvas((.03, .02, .02), (.05, .03, .025))
    for r in range(6):                                   # acoustic foam wedges
        for c in range(20):
            x0, y0 = 70 + c * 53, 30 + r * 52
            tri = [(x0, y0), (x0 + 50, y0), (x0 + 25, y0 + 48)]
            paint(img, mask_poly(tri, 1.0), AMBER, .045 + .03 * ((r + c) % 2))
            tri2 = [(x0, y0 + 50), (x0 + 50, y0 + 50), (x0 + 25, y0 + 2)]
            paint(img, mask_poly(tri2, 1.0), (.8, .5, .3), .025)
    paint(img, mask_rect(480, 150, 720, 300, 2), (.12, .1, .1), 1)     # control-room glass
    paint(img, mask_rect(486, 156, 714, 294, 3), (.5, .55, .6), .11)
    glow(img, 600, 225, 150, (.7, .75, .85), .12, .6)
    glow(img, 230, 120, 120, WARM, .55, 1)                                # a warm lamp
    glow(img, 230, 120, 18, (1, .95, .8), 1.1, 1)
    beam(img, 230, 120, 120, 340, 470, WARM, .2, 22)
    paint(img, mask_ellipse(980, 330, 26, 34, 1.5), (.02, .02, .02), -.5)   # a mic on a stand
    paint(img, mask_rect(978, 360, 982, 470, 1), (.02, .02, .02), -.5)
    glow(img, 980, 322, 60, AMBER, .12, 1)
    floor_haze(img, .7, .12)
    return finish(img, 22, .9, 80)


def chapel():
    img = canvas((.03, .02, .016), (.055, .032, .024))
    for k, x in enumerate((130, 330, 870, 1070)):        # round stone arches
        paint(img, mask_ellipse(x, 250, 70, 150, 2), (.9, .6, .38), .10)
        paint(img, mask_ellipse(x, 252, 52, 140, 2), (.9, .6, .38), -.10)
    paint(img, mask_rect(0, 0, W, 40, 6), (.9, .6, .38), .05)
    paint(img, np.clip(mask_rect(560, 100, 640, 240, 1) + mask_ellipse(600, 100, 40, 40, 1), 0, 1), WARM, .9)   # a small arched window
    for x in (586, 600, 614):
        paint(img, mask_rect(x - 1, 70, x + 1, 240, 1), (.1, .05, .02), -.6)
    glow(img, 600, 170, 260, AMBER, .30, .9)
    beam(img, 600, 140, 470, 740, 520, WARM, .45, 16)
    rng = np.random.default_rng(3)
    for x in np.concatenate([np.linspace(160, 440, 5), np.linspace(760, 1040, 5)]):   # candles
        y = 410 + rng.integers(-10, 12)
        glow(img, x, y, 8, (1, .9, .7), 1.0, 1.6)
        glow(img, x, y, 46, AMBER, .30, 1)
        paint(img, mask_rect(x - 3, y + 6, x + 3, y + 34, .8), (.8, .65, .5), .14)
    floor_haze(img, .62, .18)
    return finish(img, 33)


def gospel_stage():
    img = canvas((.03, .018, .02), (.06, .03, .04))
    for x in range(0, W, 14):                            # a curtain
        a = .5 + .5 * np.sin(x * .21)
        paint(img, mask_rect(x, 40, x + 14, 400, 1), (.55, .22, .2), .08 * a + .02)
    paint(img, mask_rect(0, 28, W, 40, 1.5), (.7, .6, .5), .18)         # the truss
    cols = [(1, .62, .3), (1, .85, .6), (1, .45, .35), (1, .85, .6), (1, .62, .3)]
    for i, x in enumerate(np.linspace(140, 1060, 7)):
        glow(img, x, 40, 18, (1, .95, .85), .9, 1)
        c = cols[i % 5]
        beam(img, x, 40, x - 120 + i * 24, x + 80 + i * 24, 470, c, .26, 14)
    paint(img, mask_rect(0, 395, W, H, 4), (.18, .1, .07), .3)           # the stage floor
    glow(img, 600, 440, 380, AMBER, .22, .25)
    for x in (80, 1120):                                 # speakers at the sides
        paint(img, mask_rect(x - 30, 200, x + 30, 400, 2), (.1, .08, .08), .6)
        glow(img, x, 300, 70, AMBER, .08, 2)
    return finish(img, 44, .8, 110)


def stadium_anthem():
    img = canvas((.02, .015, .02), (.04, .025, .04))
    glow(img, 600, 260, 600, (.8, .45, .3), .15, .35)
    rng = np.random.default_rng(5)
    for k in range(7):                                   # tiers of seats curving round the field
        y = 150 + k * 26
        a = mask_ellipse(600, y + 330, 760 - k * 12, 330, 1.5) - mask_ellipse(600, y + 345, 760 - k * 12, 330, 1.5)
        a = np.clip(a, 0, 1)
        paint(img, a, (.9, .6, .4), .12 + .02 * k)
        lights = (rng.random((H, W)) > .9965).astype(np.float32) * a
        paint(img, gaussian_filter(lights, 1.3) * 12, WARM, .5)
    for x in (110, 300, 900, 1090):                      # light towers
        paint(img, mask_rect(x - 3, 60, x + 3, 290, 1), (.04, .03, .03), -.0)
        glow(img, x, 62, 70, (1, .95, .85), 1.0, 1)
        glow(img, x, 62, 20, (1, 1, 1), 1.4, 1)
        beam(img, x, 62, 380 if x < 600 else 620, 560 if x < 600 else 820, 500, WARM, .16, 20)
    paint(img, mask_rect(0, 390, W, H, 3), (.12, .09, .07), .35)
    floor_haze(img, .66, .2)
    return finish(img, 55, .8, 160)


def graveyard():
    img = canvas((.02, .026, .03), (.035, .05, .075))            # a cold night sky over a field of stones
    glow(img, 840, 120, 220, (.55, .65, .9), .22, 1)
    glow(img, 840, 120, 46, (.9, .95, 1.0), .9, 1)               # the moon
    paint(img, mask_ellipse(840, 120, 34, 34, 1), (.85, .9, 1.0), .6)
    rng = np.random.default_rng(71)
    for x in rng.integers(40, W - 40, 55):                         # stars
        glow(img, x, rng.integers(10, 170), 2, (.9, .95, 1), .7, 1)
    for x in (110, 1090):                                          # bare trees
        paint(img, mask_rect(x - 6, 150, x + 6, 400, 1), (.05, .05, .07), .9)
        for dx, dy in ((-60, 190), (50, 170), (-40, 240), (70, 250), (0, 140)):
            paint(img, mask_poly([(x - 3, 300), (x + 3, 300), (x + dx, dy)], .8), (.05, .05, .07), .9)
    for x, h in [(210, 90), (330, 120), (470, 80), (700, 110), (950, 100), (1030, 70), (260, 60), (790, 60)]:   # headstones
        y = 420 - (x % 5) * 6
        paint(img, np.clip(mask_rect(x - 24, y - h, x + 24, y, 1) + mask_ellipse(x, y - h, 24, 20, 1), 0, 1), (.1, .12, .15), .95)
        paint(img, mask_rect(x - 24, y - h + 6, x - 18, y, 1), (.6, .7, .9), .12)
    paint(img, mask_rect(590, 250, 610, 410, 1), (.1, .12, .15), .95)       # a cross
    paint(img, mask_rect(560, 285, 640, 303, 1), (.1, .12, .15), .95)
    for y in (330, 380):                                           # ground mist
        glow(img, 600, y, 520, (.5, .62, .8), .10, .12)
    floor_haze(img, .72, .12, (.5, .6, .8))
    return finish(img, 71, .85, 90)


def crypt():
    img = canvas((.03, .022, .02), (.05, .04, .035))              # a low stone vault lit by a few candles
    for k, x in enumerate(np.linspace(0, W, 9)):                   # ribs of the vault
        paint(img, mask_poly([(600, 30), (x - 8, 330), (x + 8, 330)], 1.5), (.7, .6, .5), .10)
    for x in (200, 420, 780, 1000):                                # pillars
        paint(img, mask_rect(x - 24, 60, x + 24, 430, 2), (.55, .45, .38), .22)
        paint(img, mask_rect(x - 24, 60, x - 16, 430, 2), (1, .8, .55), .10)
    for i, x in enumerate((90, 310, 890, 1110)):                   # burial niches with skulls
        paint(img, np.clip(mask_rect(x - 40, 250, x + 40, 330, 1.5) + mask_ellipse(x, 250, 40, 24, 1.5), 0, 1), (.01, .008, .01), .75)
        paint(img, mask_ellipse(x, 300, 15, 14, 1), (.9, .85, .72), .5)
        paint(img, mask_rect(x - 9, 308, x + 9, 322, 1), (.9, .85, .72), .4)
        for dx in (-6, 6):
            paint(img, mask_ellipse(x + dx, 298, 3.5, 4.5, .8), (0, 0, 0), .9)
    rng = np.random.default_rng(72)
    for x in (140, 380, 600, 820, 1060):                           # candles
        y = 400 + rng.integers(-8, 10)
        glow(img, x, y, 8, (1, .9, .7), 1.0, 1.6)
        glow(img, x, y, 70, AMBER, .35, 1)
        paint(img, mask_rect(x - 3, y + 6, x + 3, y + 36, .8), (.8, .65, .5), .15)
    paint(img, mask_rect(480, 400, 720, 440, 2), (.35, .3, .27), .5)         # a stone altar
    floor_haze(img, .66, .16, (.9, .6, .35))
    return finish(img, 72, .8, 110)


def opera_house():
    img = canvas((.04, .018, .02), (.08, .03, .03))              # a horseshoe house: red and gold, boxes curving round, a chandelier
    glow(img, 600, 70, 260, AMBER, .30, .7)
    for k in range(5):                                           # the tiers of boxes as arcs left and right
        y = 110 + k * 58
        for side in (-1, 1):
            for j in range(11):
                t = j / 10
                x = 600 + side * (330 + 250 * t ** 1.4)
                yj = y + 28 * t ** 1.6 - 8 * k * t
                paint(img, mask_rect(x - 17, yj - 15, x + 17, yj + 18, 1.4), (.55, .1, .1), .30 - .035 * k)
                glow(img, x, yj - 4, 12, WARM, .30 - .04 * k, 1)
            paint(img, mask_poly([(600 + side * 330, y + 18), (600 + side * 590, y + 46 - 8 * k), (600 + side * 590, y + 52 - 8 * k), (600 + side * 330, y + 24)], 1.2), AMBER, .16)
    for x in (230, 970):                                         # the proscenium
        paint(img, mask_rect(x - 20, 0, x + 20, 460, 2), (.7, .45, .2), .16)
    paint(img, mask_rect(230, 0, 970, 36, 2), (.7, .12, .1), .22)
    glow(img, 600, 52, 18, WARM, 1.0, 1.3)                       # the chandelier
    for dx in (-90, -45, 45, 90):
        glow(img, 600 + dx, 70 + abs(dx) * .18, 9, WARM, .6, 1.2)
    beam(img, 600, 60, 330, 870, 440, WARM, .10, 22)
    floor_haze(img, .62, .16, (1, .5, .3))
    return finish(img, 81, .82, 120)


def cave():
    img = canvas((.012, .02, .028), (.02, .035, .045))           # a limestone cavern: dripping teeth, cold blue light, black water
    rng = np.random.default_rng(82)
    for x in np.linspace(20, W - 20, 30):                        # stalactites
        h = rng.integers(50, 250) * (1 - .45 * np.exp(-((x - 600) / 230) ** 2))
        w = rng.integers(14, 36)
        paint(img, mask_poly([(x - w, 0), (x + w, 0), (x + rng.integers(-6, 7), h)], 1.4), (.3, .38, .42), .30)
    for x in np.linspace(40, W - 40, 16):                        # stalagmites
        h = rng.integers(40, 140) * (1 - .5 * np.exp(-((x - 600) / 250) ** 2))
        w = rng.integers(14, 32)
        paint(img, mask_poly([(x - w, 520), (x + w, 520), (x, 520 - h)], 1.4), (.22, .28, .3), .30)
    for x, y in ((140, 380), (1060, 400), (610, 300)):           # a lantern and two glints of cold light
        glow(img, x, y, 130, (.2, .55, .75), .30, .8)
    glow(img, 140, 380, 10, (.9, .95, 1), .9, 1.2)
    for x in rng.integers(60, W - 60, 40):
        glow(img, x, rng.integers(20, 200), 2.2, (.6, .85, 1), .55, 1)
    paint(img, mask_rect(0, 440, W, 530, 2), (.05, .12, .17), .45)  # the dark pool
    floor_haze(img, .74, .14, (.3, .6, .8))
    return finish(img, 82, .85, 90)


def bathroom():
    img = canvas((.045, .05, .05), (.07, .075, .08))             # a tiled bathroom: white tile, a flickering tube, steam
    for x in range(0, W, 60):                                    # the tile grid
        paint(img, mask_rect(x, 0, x + 2, 440, .6), (.7, .78, .82), .14)
    for y in range(0, 440, 60):
        paint(img, mask_rect(0, y, W, y + 2, .6), (.7, .78, .82), .14)
    paint(img, mask_rect(0, 440, W, 530, 1.5), (.35, .4, .42), .30)       # the floor
    paint(img, mask_rect(260, 90, 520, 330, 2), (.5, .6, .65), .16)       # a mirror
    glow(img, 390, 200, 160, (.6, .8, 1), .12, .9)
    paint(img, mask_rect(760, 250, 1130, 430, 3), (.55, .6, .62), .26)     # a bath
    paint(img, mask_ellipse(945, 262, 190, 20, 2), (.8, .85, .9), .22)
    paint(img, mask_rect(380, 36, 820, 46, 2), (.9, .95, 1), .50)         # the tube light
    glow(img, 600, 41, 360, (.75, .88, 1), .38, .26)
    for x in (410, 600, 790):
        beam(img, x, 46, x - 120, x + 120, 430, (.75, .9, 1), .09, 18)
    floor_haze(img, .55, .22, (.8, .9, 1))
    return finish(img, 83, .8, 80)


def parking_garage():
    img = canvas((.03, .03, .03), (.05, .05, .045))              # a concrete car park: beams, pillars, strip lights, painted lines
    for k in range(7):                                           # the ceiling beams running into the distance
        y = 6 + k * 18
        paint(img, mask_rect(0, y, W, y + 6, 1.5), (.6, .6, .55), .14 - .013 * k)
    for x in (150, 420, 780, 1050):                              # pillars with hazard stripes
        paint(img, mask_rect(x - 32, 70, x + 32, 450, 2), (.5, .5, .46), .30)
        for yb in range(330, 450, 26):
            paint(img, mask_poly([(x - 32, yb), (x + 32, yb - 14), (x + 32, yb + 2), (x - 32, yb + 16)], .8), (1, .8, .1), .38)
        paint(img, mask_rect(x - 32, 70, x - 22, 450, 2), (1, 1, .9), .08)
    for x in (260, 600, 940):                                    # strip lights
        paint(img, mask_rect(x - 70, 52, x + 70, 60, 1.5), (1, 1, .85), .75)
        glow(img, x, 56, 150, (1, .98, .8), .30, .4)
        beam(img, x, 60, x - 160, x + 160, 450, (1, .95, .8), .10, 20)
    for x in (30, 330, 700, 1000, 1170):                         # parking bay lines on the floor
        paint(img, mask_poly([(x, 450), (x + 8, 450), (x + 60, 530), (x + 42, 530)], .8), (.9, .9, .8), .28)
    paint(img, mask_rect(0, 448, W, 453, 1), (.7, .7, .65), .20)
    floor_haze(img, .70, .16, (1, .9, .6))
    return finish(img, 84, .82, 100)


def marble_dome():
    img = canvas((.04, .035, .035), (.09, .075, .06))            # a marble dome: ribs, a bright oculus, pale columns
    cx, cy = 600, 250
    for k in range(18):                                          # ribs radiating from the oculus
        a = np.pi * (k / 17)
        x1, y1 = cx + 640 * np.cos(a), cy - 420 * np.sin(a)
        paint(img, mask_poly([(cx - 3, 40), (cx + 3, 40), (x1 + 9, y1 + 400 * (1 - np.sin(a)) + 120), (x1 - 9, y1 + 400 * (1 - np.sin(a)) + 120)], 1.8), (.85, .78, .62), .08)
    for k in range(1, 5):                                        # the rings of the dome
        paint(img, mask_ellipse(cx, 150, 150 * k, 36 * k, 2.2) - mask_ellipse(cx, 150, 150 * k - 5, 36 * k - 4, 2.2), (.9, .82, .65), .10)
    glow(img, cx, 38, 120, (1, .95, .8), .95, .55)               # the oculus
    beam(img, cx, 40, 360, 840, 470, (1, .93, .75), .13, 26)
    for x in (110, 280, 920, 1090):                              # columns
        paint(img, mask_rect(x - 20, 120, x + 20, 450, 2), (.85, .8, .7), .22)
        paint(img, mask_rect(x - 20, 120, x - 11, 450, 2), (1, 1, .9), .10)
        paint(img, mask_rect(x - 30, 110, x + 30, 124, 1.5), (.9, .85, .7), .24)
        paint(img, mask_rect(x - 30, 446, x + 30, 460, 1.5), (.9, .85, .7), .20)
    floor_haze(img, .62, .18, (1, .85, .6))
    return finish(img, 85, .8, 130)


def stairwell():
    img = canvas((.03, .03, .032), (.05, .05, .055))             # a concrete stairwell: flights zig-zag up the walls, one bare bulb
    for side in (-1, 1):
        for k in range(6):
            y = 440 - k * 70
            x0 = 600 + side * 560
            x1 = 600 + side * 250
            paint(img, mask_poly([(x0, y), (x1, y - 38), (x1, y - 28), (x0, y + 10)], 1.2), (.55, .55, .5), .26)
            for j in range(8):                                   # the railing posts
                t = j / 7
                xp = x0 + (x1 - x0) * t
                yp = y - 38 * t
                paint(img, mask_rect(xp - 1.5, yp - 36, xp + 1.5, yp - 2, .6), (.5, .5, .48), .20)
            paint(img, mask_poly([(x0, y - 36), (x1, y - 74), (x1, y - 70), (x0, y - 32)], .8), (.55, .55, .5), .22)
    paint(img, mask_rect(598, 0, 602, 90, .6), (.5, .5, .5), .30)       # the cable
    glow(img, 600, 96, 12, (1, .95, .75), 1.0, 1.2)                     # the bare bulb
    glow(img, 600, 96, 330, (1, .85, .55), .34, .9)
    beam(img, 600, 100, 300, 900, 470, (1, .9, .65), .11, 24)
    paint(img, mask_rect(0, 462, W, 530, 2), (.3, .3, .28), .30)
    floor_haze(img, .68, .16, (1, .85, .55))
    return finish(img, 86, .84, 100)


SCENES = {"scene_concert_hall": concert_hall, "scene_studio_booth": studio_booth, "scene_chapel": chapel,
          "scene_gospel_stage": gospel_stage, "scene_stadium_anthem": stadium_anthem, "scene_opera_house": opera_house,
          "scene_graveyard": graveyard, "scene_crypt": crypt, "scene_cave": cave, "scene_bathroom": bathroom,
          "scene_parking_garage": parking_garage, "scene_marble_dome": marble_dome, "scene_stairwell": stairwell}

if __name__ == "__main__":
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "Assets" / "Scenes"
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in SCENES.items():
        Image.fromarray(fn()).save(out / f"{name}.jpg", quality=88, optimize=True)
        print("wrote", out / f"{name}.jpg")
