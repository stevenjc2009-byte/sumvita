#!/usr/bin/env python3
"""Render Sumvita logo + LiveArea art (Pillow, original art, fonts from assets/fonts).

sce_sys PNGs are written as 8-bit palettized (colour type 3) - the Vita refuses
anything else with 0x8010113D. Each is re-read from disk and its IHDR asserted.
"""
from pathlib import Path
import math
import struct
import sys

from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "assets" / "fonts"
ICON = ROOT / "sce_sys" / "icon0.png"
BG = ROOT / "sce_sys" / "livearea" / "contents" / "bg.png"
STARTUP = ROOT / "sce_sys" / "livearea" / "contents" / "startup.png"
LOGO = ROOT / "assets" / "img" / "logo.png"
LOGO_WIDE = ROOT / "assets" / "img" / "logo_wide.png"
PREVIEW = ROOT / "docs" / "art" / "preview.png"

NAVY0 = (8, 15, 32)
NAVY1 = (17, 30, 58)
TEAL = (30, 220, 200)
TEAL_D = (14, 150, 150)
WHITE = (240, 248, 255)
SS = 4


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def vgrad(w, h, top, bot):
    g = Image.linear_gradient("L").resize((w, h))
    return Image.composite(Image.new("RGB", (w, h), bot), Image.new("RGB", (w, h), top), g)


def mark(size):
    """Calculator-tile mark on a transparent RGBA canvas of size x size."""
    s = size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    pad = int(s * 0.04)
    r = int(s * 0.24)
    m = Image.new("L", (s, s), 0)
    ImageDraw.Draw(m).rounded_rectangle([pad, pad, s - pad, s - pad], r, fill=255)
    img.paste(vgrad(s, s, (60, 240, 215), TEAL_D), (0, 0), m)
    b = int(s * 0.035)
    m2 = Image.new("L", (s, s), 0)
    ImageDraw.Draw(m2).rounded_rectangle([pad + b, pad + b, s - pad - b, s - pad - b], r - b, fill=255)
    img.paste(vgrad(s, s, (26, 46, 84), (13, 24, 50)), (0, 0), m2)
    d = ImageDraw.Draw(img)
    f = font("DejaVuSans-Bold.ttf", int(s * 0.68))
    l, t, rr, bb = d.textbbox((0, 0), "Σ", font=f)
    cx, cy = s / 2, s * 0.43
    d.text((cx - (l + rr) / 2, cy - (t + bb) / 2), "Σ", font=f, fill=TEAL)
    x0, x1 = s * 0.22, s * 0.78
    pts = []
    for i in range(121):
        u = i / 120
        pts.append((x0 + (x1 - x0) * u,
                    s * 0.775 - math.sin(u * 2 * math.pi) * s * 0.075 * (0.4 + 0.6 * u)))
    w = int(s * 0.035)
    d.line(pts, fill=WHITE, width=w, joint="curve")
    p = pts[0]
    d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=WHITE)
    ex, ey = pts[-1]
    rd = s * 0.032
    d.ellipse([ex - rd, ey - rd, ex + rd, ey + rd], fill=TEAL, outline=WHITE, width=int(w * 0.6))
    return img.resize((size, size), Image.LANCZOS)


def ss_text(text, fnt_name, px, fill):
    """Anti-aliased text on transparent RGBA (supersampled), cropped to ink."""
    f = font(fnt_name, px * SS)
    tmp = Image.new("RGBA", (px * SS * (len(text) + 1), px * SS * 2), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((10, 10), text, font=f, fill=fill)
    tmp = tmp.crop(tmp.getbbox())
    return tmp.resize((max(1, tmp.width // SS), max(1, tmp.height // SS)), Image.LANCZOS)


def wordmark(px):
    return ss_text("Sumvita", "Ubuntu-B.ttf", px, WHITE)


def math_motif(w, h):
    s = SS
    ov = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    step = 40 * s
    for x in range(0, w * s, step):
        d.line([(x, 0), (x, h * s)], fill=(60, 110, 150, 34), width=s)
    for y in range(0, h * s, step):
        d.line([(0, y), (w * s, y)], fill=(60, 110, 150, 34), width=s)
    ax, ay = int(w * 0.62) * s, int(h * 0.60) * s
    d.line([(ax, 0), (ax, h * s)], fill=(70, 150, 180, 70), width=2 * s)
    d.line([(0, ay), (w * s, ay)], fill=(70, 150, 180, 70), width=2 * s)
    for amp, ph, col, wd in ((70, 0.0, (30, 220, 200, 85), 3), (45, 1.4, (120, 170, 230, 55), 2)):
        pts = [(x * s, ay + math.sin((x / w) * 6.5 + ph) * amp * s) for x in range(w // 2 - 40, w + 1, 3)]
        d.line(pts, fill=col, width=wd * s, joint="curve")
    ov = ov.resize((w, h), Image.LANCZOS)
    syms = [("π", 560, 40, 70), ("√", 700, 300, 80), ("∫", 770, 110, 90),
            ("x²", 520, 380, 60), ("∑", 610, 190, 60), ("÷", 780, 410, 56), ("%", 470, 70, 50)]
    for ch, x, y, sz in syms:
        t = ss_text(ch, "DejaVuSans-Bold.ttf", sz, (110, 170, 215, 48))
        if x + t.width <= w and y + t.height <= h:
            ov.alpha_composite(t, (x, y))
    return ov


def glow_layer(w, h, box, alpha, blur):
    g = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse(box, fill=(20, 120, 140, alpha))
    return g.filter(ImageFilter.GaussianBlur(blur))


def make_icon():
    img = vgrad(128, 128, NAVY1, NAVY0).convert("RGBA")
    mk = mark(92)
    sh = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    sh.paste((0, 0, 0, 150), (18, 21), mk.split()[3])
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(4)))
    img.alpha_composite(mk, (18, 18))
    return img.convert("RGB")


def make_bg():
    w, h = 840, 500
    img = vgrad(w, h, NAVY1, NAVY0).convert("RGBA")
    img.alpha_composite(glow_layer(w, h, [430, 150, 960, 640], 70, 70))
    img.alpha_composite(math_motif(w, h))
    img.alpha_composite(mark(110), (60, 150))
    wm = wordmark(92)
    img.alpha_composite(wm, (190, 150 + (110 - wm.height) // 2 - 4))
    tag = ss_text("Calculate. Graph. Understand.", "Ubuntu-R.ttf", 28, (130, 200, 215, 255))
    img.alpha_composite(tag, (62, 290))
    return img.convert("RGB")


def make_startup():
    w, h = 280, 158
    img = vgrad(w, h, NAVY1, NAVY0).convert("RGBA")
    img.alpha_composite(glow_layer(w, h, [60, 20, 220, 140], 70, 30))
    mk = mark(78)
    wm = wordmark(40)
    gap = 12
    x = (w - (mk.width + gap + wm.width)) // 2
    img.alpha_composite(mk, (x, (h - 78) // 2))
    img.alpha_composite(wm, (x + mk.width + gap, (h - wm.height) // 2 - 1))
    return img.convert("RGB")


def make_logo_wide():
    w, h = 480, 120
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    img.alpha_composite(mark(112), (4, 4))
    wm = wordmark(84)
    img.alpha_composite(wm, (136, (h - wm.height) // 2 - 2))
    return img


def save_indexed(img, path, size):
    if img.size != size:
        raise RuntimeError(f"{path}: generated size {img.size}, want {size}")
    path.parent.mkdir(parents=True, exist_ok=True)
    q = img.convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(path, format="PNG", optimize=True)
    raw = path.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR":
        raise RuntimeError(f"{path}: not a PNG")
    w, h, depth, ctype = struct.unpack(">IIBB", raw[16:26])
    if (w, h) != size or depth != 8 or ctype != 3:
        raise RuntimeError(f"{path}: IHDR {w}x{h} depth={depth} colour_type={ctype}; want {size} depth 8 type 3")
    print(f"{path.relative_to(ROOT)}: {w}x{h} depth={depth} colour_type={ctype} bytes={len(raw)}")
    return Image.open(path).convert("RGB")


def make_preview(icon, bg, startup, logo, wide):
    W, H = 900, 1030
    back = (40, 44, 52)
    sh = Image.new("RGB", (W, H), back)
    d = ImageDraw.Draw(sh)
    lab = font("Ubuntu-R.ttf", 16)

    def put(im, x, y, name, bgc=None):
        if im.mode == "RGBA":
            tile = Image.new("RGBA", im.size, bgc or back + (255,))
            tile.alpha_composite(im)
            im = tile.convert("RGB")
        sh.paste(im, (x, y))
        d.text((x, y - 22), name, font=lab, fill=(200, 210, 220))

    put(bg, 20, 40, "bg.png 840x500")
    put(startup, 20, 590, "startup.png 280x158")
    put(icon, 330, 590, "icon0.png 128")
    put(icon.resize((256, 256), Image.NEAREST), 480, 590, "icon0 2x")
    circ = Image.new("L", (128, 128), 0)
    ImageDraw.Draw(circ).ellipse([0, 0, 127, 127], fill=255)
    c = Image.new("RGB", (128, 128), back)
    c.paste(icon, (0, 0), circ)
    put(c, 760, 590, "circular mask")
    put(logo.resize((128, 128), Image.LANCZOS), 20, 880, "logo.png (128 of 256)", (30, 34, 42, 255))
    put(wide, 200, 880, "logo_wide 480x120", (30, 34, 42, 255))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    sh.save(PREVIEW)
    print("preview:", PREVIEW)


def main():
    icon = save_indexed(make_icon(), ICON, (128, 128))
    bg = save_indexed(make_bg(), BG, (840, 500))
    st = save_indexed(make_startup(), STARTUP, (280, 158))
    logo = mark(256)
    LOGO.parent.mkdir(parents=True, exist_ok=True)
    logo.save(LOGO)
    wide = make_logo_wide()
    wide.save(LOGO_WIDE)
    print("logo", logo.size, logo.mode, "wide", wide.size, wide.mode)
    make_preview(icon, bg, st, logo, wide)
    return 0


if __name__ == "__main__":
    sys.exit(main())
