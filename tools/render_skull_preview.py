#!/usr/bin/env python3
"""Draws the skull logo (Source/UI/SkullShape.h) for each theme (Source/Theme.h) as a PNG, so the logo can be looked at without building the plugin.

    python3 tools/render_skull_preview.py [out.png] [--size 160]

Needs Pillow (pip install pillow) for drawing; parsing the header needs nothing. The plugin paints the same shapes with juce::Graphics
(SkullButton in Source/PluginEditor.cpp), so what you see here is what the data describes. Not a pixel-exact copy of JUCE's anti-aliasing.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHAPE_RE = re.compile(r"\{\s*Kind::(\w+)\s*,\s*Role::(\w+)\s*,\s*(\d+)\s*,\s*\{([^}]*)\}\s*\}")
EXPECT = {"Ellipse": 4, "RoundRect": 5}


def parse_shapes(header=ROOT / "Source" / "UI" / "SkullShape.h"):
    shapes = []
    for kind, role, count, values in SHAPE_RE.findall(header.read_text(encoding="utf-8")):
        v = [float(x.strip().rstrip("fF")) for x in values.split(",") if x.strip()]
        n = int(count)
        if len(v) < n:
            raise ValueError(f"{kind}/{role}: says {n} values, has {len(v)}")
        if kind in EXPECT and n != EXPECT[kind]:
            raise ValueError(f"{kind} needs {EXPECT[kind]} values, says {n}")
        if kind == "Poly" and (n % 2 or n < 6):
            raise ValueError("a Poly needs at least 3 points")
        shapes.append({"kind": kind, "role": role, "v": v[:n]})
    return shapes


def parse_themes(header=ROOT / "Source" / "Theme.h"):
    """name -> dict of the colours the skull needs (0xAARRGGBB ints), read from the Palette initialisers in Theme.h."""
    text = header.read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*\{(.*?)\}\s*\}\s*,', text, re.S):
        name, body = m.group(1), m.group(2)
        body = re.sub(r"\{[^{}]*\}", lambda s: s.group(0).replace("{", "").replace("}", ""), body)   # flatten the section list
        nums = [int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]{8}", body)]
        if len(nums) < 12:
            continue
        # field order of theme::Palette: bg panel panel2 stroke track cream muted gold goldHi goldLo ink lead ... headerTop is index 25
        out.append((name, {"bg": nums[0], "panel": nums[1], "gold": nums[7], "goldHi": nums[8], "goldLo": nums[9], "ink": nums[10], "headerTop": nums[25]}))
    return out


def rgb(argb):
    return ((argb >> 16) & 255, (argb >> 8) & 255, argb & 255)


def draw_skull(shapes, size, colours, supersample=4):
    from PIL import Image, ImageDraw
    S = size * supersample
    img = Image.new("RGBA", (S, S), rgb(colours["bg"]) + (255,))
    # a header-like glow behind it, as in the plugin
    top = rgb(colours["headerTop"])
    glow = Image.new("RGBA", (S, S))
    gp = glow.load()
    bg = rgb(colours["bg"])
    for y in range(S):
        t = min(1.0, y / (S * 0.9))
        c = tuple(int(top[i] * (1 - t) + bg[i] * t) for i in range(3)) + (255,)
        for x in range(S):
            gp[x, y] = c
    img = glow
    # body: accent gradient top to bottom
    hi, lo = rgb(colours["goldHi"]), rgb(colours["goldLo"])
    gradient = Image.new("RGBA", (S, S))
    gp = gradient.load()
    for y in range(S):
        t = y / (S - 1)
        c = tuple(int(hi[i] * (1 - t) + lo[i] * t) for i in range(3)) + (255,)
        for x in range(S):
            gp[x, y] = c
    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    cut = Image.new("L", (S, S), 0)
    cd = ImageDraw.Draw(cut)
    glint = Image.new("L", (S, S), 0)
    gd = ImageDraw.Draw(glint)

    def paint(d, s, fill):
        v = s["v"]
        if s["kind"] == "Ellipse":
            cx, cy, rx, ry = v
            d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=fill)
        elif s["kind"] == "RoundRect":
            x, y, w, h, r = v
            d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], radius=r * S, fill=fill)
        else:
            d.polygon([(v[i] * S, v[i + 1] * S) for i in range(0, len(v), 2)], fill=fill)

    for s in shapes:
        paint({"Body": md, "Cut": cd, "Glint": gd}[s["role"]], s, 255)
    img = Image.composite(gradient, img, mask)
    holes = Image.new("RGBA", (S, S), rgb(colours["bg"]) + (255,))
    img = Image.composite(holes, img, cut)
    white = Image.new("RGBA", (S, S), (255, 255, 255, 255))
    glint = glint.point(lambda p: int(p * 0.34))
    img = Image.composite(white, img, glint)
    return img.resize((size, size), Image.LANCZOS)


def main(argv):
    out = Path(argv[1]) if len(argv) > 1 and not argv[1].startswith("--") else ROOT / "Preview" / "skull_themes.png"
    size = int(argv[argv.index("--size") + 1]) if "--size" in argv else 160
    from PIL import Image
    shapes = parse_shapes()
    themes = parse_themes()
    if not shapes or not themes:
        raise SystemExit("could not read the shapes or the themes")
    pad = 12
    sheet = Image.new("RGBA", (len(themes) * (size + pad) + pad, 2 * size + 3 * pad), (20, 20, 20, 255))
    for i, (name, colours) in enumerate(themes):
        sheet.paste(draw_skull(shapes, size, colours), (pad + i * (size + pad), pad))
        small = draw_skull(shapes, 30, colours)          # the size it has in the header
        sheet.paste(small.resize((size // 2, size // 2), Image.NEAREST), (pad + i * (size + pad), size + 2 * pad))
        sheet.paste(small, (pad + i * (size + pad) + size // 2 + 10, size + 2 * pad))
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(f"{len(shapes)} shapes, {len(themes)} themes -> {out}")


if __name__ == "__main__":
    main(sys.argv)
