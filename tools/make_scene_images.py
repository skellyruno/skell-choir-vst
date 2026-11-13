#!/usr/bin/env python3
"""Generates the six stage-scene pictures (Assets/Scenes/*.jpg, 1200x530) in the style of Assets/cathedral_stage.jpg: dark, warm, volumetric, vignetted.
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


def headphone_choir():
    img = canvas((.025, .02, .025), (.05, .03, .04))
    cx, cy = 600, 300
    for i, (rx, ry) in enumerate([(120, 36), (230, 70), (350, 106), (480, 142), (590, 175)]):
        ring = mask_ellipse(cx, cy, rx, ry, 1.3) - mask_ellipse(cx, cy, rx - 3, ry - 1.5, 1.3)
        paint(img, np.clip(ring, 0, 1), AMBER, .55 - .07 * i)
        n = 8 + i * 6
        for j in range(n):                               # singers as soft figures on each ring
            t = 2 * np.pi * (j + .5 * i) / n
            x, y = cx + rx * np.cos(t), cy + ry * np.sin(t)
            glow(img, x, y - 16, 12 + i * 2, (1, .8, .55), .6 - .07 * i, 1.8)
            glow(img, x, y - 36 - i * 3, 6 + i, WARM, .6 - .07 * i, 1)
    glow(img, cx, cy - 20, 160, (.8, .75, 1.0), .30, .9)                 # the listener
    glow(img, cx, cy - 20, 30, (1, 1, 1), .8, 1)
    floor_haze(img, .55, .1)
    return finish(img, 66, .75, 120)


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


SCENES = {"scene_concert_hall": concert_hall, "scene_studio_booth": studio_booth, "scene_chapel": chapel,
          "scene_gospel_stage": gospel_stage, "scene_stadium_anthem": stadium_anthem, "scene_headphone_choir": headphone_choir,
          "scene_graveyard": graveyard, "scene_crypt": crypt}

if __name__ == "__main__":
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "Assets" / "Scenes"
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in SCENES.items():
        Image.fromarray(fn()).save(out / f"{name}.jpg", quality=88, optimize=True)
        print("wrote", out / f"{name}.jpg")
