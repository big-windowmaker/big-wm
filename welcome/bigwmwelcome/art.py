# SPDX-License-Identifier: GPL-2.0-or-later
"""Big WindowMaker Welcome: the illustration and its demonstrations.

Everything is drawn here with cairo (no images but the official logo): a
1990s "introducing" scene, purple to cyan, and miniatures of the real
desktop (Window Maker windows, the Dock, the Clip, the menu, zones). Each
scene is a function of t in [0, 1): the same frame for the same t, so a
paused or reduced-motion scene is still a correct picture.
"""

import math

# Window Maker colors (the BigWindowMaker style).
GRAY = (0xae / 255, 0xaa / 255, 0xae / 255)
LIGHT = (1, 1, 1)
DARK = (0x51 / 255, 0x55 / 255, 0x51 / 255)
BLACK = (0, 0, 0)
TITLE_FOCUS = (0x00 / 255, 0x00 / 255, 0x00 / 255)
TITLE_TEXT = (1, 1, 1)
TILE = (0x9a / 255, 0x96 / 255, 0x9a / 255)
CYAN = (0x04 / 255, 0xba / 255, 0xcc / 255)
BLUE = (0x2d / 255, 0x85 / 255, 0xc6 / 255)
PURPLE = (0x5b / 255, 0x2a / 255, 0x86 / 255)
LCD = (0x0b / 255, 0x2a / 255, 0x24 / 255)
LCD_ON = (0x37 / 255, 0xe0 / 255, 0xb0 / 255)


def ease(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def phase(t, a, b):
    """0..1 while t goes from a to b (clamped)."""
    if b <= a:
        return 1.0 if t >= b else 0.0
    return ease((t - a) / (b - a))


def lerp(a, b, k):
    return a + (b - a) * k


# ---------------------------------------------------------------------------
# Primitives

def bevel(cr, x, y, w, h, fill=GRAY, raised=True):
    cr.set_source_rgb(*fill)
    cr.rectangle(x, y, w, h)
    cr.fill()
    cr.set_line_width(1)
    a, b = (LIGHT, DARK) if raised else (DARK, LIGHT)
    cr.set_source_rgb(*a)
    cr.move_to(x + 0.5, y + h - 0.5)
    cr.line_to(x + 0.5, y + 0.5)
    cr.line_to(x + w - 0.5, y + 0.5)
    cr.stroke()
    cr.set_source_rgb(*b)
    cr.move_to(x + w - 0.5, y + 0.5)
    cr.line_to(x + w - 0.5, y + h - 0.5)
    cr.line_to(x + 0.5, y + h - 0.5)
    cr.stroke()


def text(cr, x, y, s, size=11, color=BLACK, bold=False, italic=False, family="Sans"):
    cr.select_font_face(family, 1 if italic else 0, 1 if bold else 0)
    cr.set_font_size(size)
    cr.set_source_rgb(*color)
    cr.move_to(x, y)
    cr.show_text(s)
    return cr.text_extents(s).x_advance


def wm_window(cr, x, y, w, h, title, focused=True, body=GRAY, content=None):
    """A Window Maker window: title bar with the two buttons, resize bar."""
    tb = 15
    cr.set_source_rgb(*BLACK)
    cr.rectangle(x - 1, y - 1, w + 2, h + 2)
    cr.fill()
    bevel(cr, x, y, w, tb, TITLE_FOCUS if focused else (0.55, 0.55, 0.55))
    # Buttons: miniaturize (left), close (right).
    bevel(cr, x + 2, y + 2, tb - 4, tb - 4)
    bevel(cr, x + w - tb + 2, y + 2, tb - 4, tb - 4)
    cr.set_source_rgb(*BLACK)
    cr.rectangle(x + 5, y + 5, tb - 10, tb - 10)
    cr.stroke()
    cr.move_to(x + w - tb + 5, y + 5)
    cr.line_to(x + w - 5, y + tb - 5)
    cr.move_to(x + w - 5, y + 5)
    cr.line_to(x + w - tb + 5, y + tb - 5)
    cr.stroke()
    cr.save()
    cr.rectangle(x + tb, y, w - 2 * tb, tb)
    cr.clip()
    text(cr, x + tb + 4, y + 11, title, 9, TITLE_TEXT if focused else BLACK, bold=True)
    cr.restore()
    cr.set_source_rgb(*body)
    cr.rectangle(x, y + tb, w, h - tb - 6)
    cr.fill()
    bevel(cr, x, y + h - 6, w, 6)
    if content:
        cr.save()
        cr.rectangle(x, y + tb, w, h - tb - 6)
        cr.clip()
        content(cr, x, y + tb, w, h - tb - 6)
        cr.restore()


def tile(cr, x, y, s=40):
    """A Dock/Clip tile (the gradient of the icon background)."""
    bevel(cr, x, y, s, s, TILE)


def cursor(cr, x, y, pressed=False):
    cr.save()
    cr.translate(x, y)
    cr.move_to(0, 0)
    cr.line_to(0, 15)
    cr.line_to(4, 11)
    cr.line_to(7, 17)
    cr.line_to(9, 16)
    cr.line_to(6, 10)
    cr.line_to(11, 10)
    cr.close_path()
    cr.set_source_rgb(*LIGHT)
    cr.fill_preserve()
    cr.set_line_width(1)
    cr.set_source_rgb(*BLACK)
    cr.stroke()
    if pressed:
        cr.set_source_rgba(1, 1, 0.6, 0.85)
        cr.arc(0, 0, 9, 0, 2 * math.pi)
        cr.set_line_width(2)
        cr.stroke()
    cr.restore()


def logo(cr, pixbuf_paint, x, y, size):
    if pixbuf_paint:
        pixbuf_paint(cr, x, y, size)
    else:
        cr.set_source_rgb(*BLUE)
        cr.arc(x + size / 2, y + size / 2, size / 2, 0, 2 * math.pi)
        cr.fill()


def dock_column(cr, x, y, n, s, logo_paint, highlight=-1):
    for i in range(n):
        tile(cr, x, y + i * s, s)
        cx, cy = x + s / 2, y + i * s + s / 2
        if i == 0:
            logo(cr, logo_paint, x + 4, y + 4, s - 8)
        else:
            _app_glyph(cr, cx, cy, s * 0.55, i)
        if i == highlight:
            cr.set_source_rgba(1, 1, 1, 0.35)
            cr.rectangle(x + 1, y + i * s + 1, s - 2, s - 2)
            cr.fill()


def _app_glyph(cr, cx, cy, s, kind):
    """Small retro program icons: terminal, files, browser, editor..."""
    k = kind % 5
    if k == 1:  # terminal
        cr.set_source_rgb(0.1, 0.1, 0.1)
        cr.rectangle(cx - s / 2, cy - s / 2.6, s, s / 1.3)
        cr.fill()
        text(cr, cx - s / 2 + 2, cy + 2, ">_", s / 2.6, LCD_ON, bold=True, family="Monospace")
    elif k == 2:  # folder
        cr.set_source_rgb(0.85, 0.7, 0.35)
        cr.rectangle(cx - s / 2, cy - s / 3, s, s * 0.7)
        cr.fill()
        cr.rectangle(cx - s / 2, cy - s / 2.3, s / 2.4, s / 6)
        cr.fill()
    elif k == 3:  # browser: a globe
        cr.set_source_rgb(*BLUE)
        cr.arc(cx, cy, s / 2.2, 0, 2 * math.pi)
        cr.fill()
        cr.set_source_rgb(*CYAN)
        cr.set_line_width(1.2)
        cr.arc(cx, cy, s / 2.2, 0, 2 * math.pi)
        cr.move_to(cx - s / 2.2, cy)
        cr.line_to(cx + s / 2.2, cy)
        cr.stroke()
        cr.save()
        cr.translate(cx, cy)
        cr.scale(0.45, 1)
        cr.arc(0, 0, s / 2.2, 0, 2 * math.pi)
        cr.restore()
        cr.stroke()
    elif k == 4:  # editor: a page
        cr.set_source_rgb(0.96, 0.96, 0.92)
        cr.rectangle(cx - s / 3, cy - s / 2.2, s / 1.5, s / 1.1)
        cr.fill()
        cr.set_source_rgb(0.4, 0.4, 0.5)
        cr.set_line_width(1)
        for j in range(4):
            cr.move_to(cx - s / 4, cy - s / 4 + j * s / 7)
            cr.line_to(cx + s / 4, cy - s / 4 + j * s / 7)
        cr.stroke()
    else:  # a gear (settings)
        cr.set_source_rgb(0.35, 0.35, 0.4)
        cr.arc(cx, cy, s / 2.6, 0, 2 * math.pi)
        cr.fill()
        cr.set_source_rgb(*GRAY)
        cr.arc(cx, cy, s / 6, 0, 2 * math.pi)
        cr.fill()


def clip(cr, x, y, s, workspace=1):
    tile(cr, x, y, s)
    cr.set_source_rgb(0.85, 0.85, 0.88)
    cr.set_line_width(2.4)
    # The paperclip.
    cr.save()
    cr.translate(x + s / 2, y + s / 2)
    cr.rotate(-0.7)
    w, h = s * 0.18, s * 0.6
    cr.arc(0, -h / 2 + w, w, math.pi, 0)
    cr.line_to(w, h / 2 - w)
    cr.arc(0, h / 2 - w, w, 0, math.pi)
    cr.line_to(-w, -h / 4)
    cr.stroke()
    cr.restore()
    # Arrows in the corners, the workspace number.
    cr.set_source_rgb(*BLACK)
    cr.move_to(x + s - 2, y + 2)
    cr.line_to(x + s - 11, y + 2)
    cr.line_to(x + s - 2, y + 11)
    cr.close_path()
    cr.fill()
    cr.move_to(x + 2, y + s - 2)
    cr.line_to(x + 11, y + s - 2)
    cr.line_to(x + 2, y + s - 11)
    cr.close_path()
    cr.fill()
    text(cr, x + 3, y + 11, str(workspace), 9, BLACK, bold=True)


def menu(cr, x, y, items, title="Big WindowMaker", selected=-1, w=118):
    ih = 15
    bevel(cr, x, y, w, ih, TITLE_FOCUS)
    text(cr, x + 4, y + 11, title, 9, TITLE_TEXT, bold=True)
    for i, it in enumerate(items):
        yy = y + ih * (i + 1)
        bevel(cr, x, yy, w, ih, LIGHT if i == selected else GRAY)
        text(cr, x + 5, yy + 11, it, 9, BLACK)
        if it.endswith(" >"):
            pass


def lcd_tile(cr, x, y, s, draw):
    tile(cr, x, y, s)
    cr.set_source_rgb(*LCD)
    cr.rectangle(x + 4, y + 4, s - 8, s - 8)
    cr.fill()
    draw(cr, x + 4, y + 4, s - 8)


# ---------------------------------------------------------------------------
# The background: an "introducing" scene of the 1990s.

def background(cr, w, h, dark=False):
    from cairo import LinearGradient, RadialGradient
    g = LinearGradient(0, 0, w, h)
    if dark:
        g.add_color_stop_rgb(0, 0.10, 0.04, 0.18)
        g.add_color_stop_rgb(0.55, 0.06, 0.10, 0.25)
        g.add_color_stop_rgb(1, 0.02, 0.22, 0.27)
    else:
        g.add_color_stop_rgb(0, 0.27, 0.10, 0.45)
        g.add_color_stop_rgb(0.55, 0.15, 0.30, 0.66)
        g.add_color_stop_rgb(1, 0.02, 0.62, 0.72)
    cr.set_source(g)
    cr.paint()
    # A perspective grid on the floor, the 1990s "technology" look.
    horizon = h * 0.62
    cr.save()
    cr.rectangle(0, horizon, w, h - horizon)
    cr.clip()
    cr.set_source_rgba(0.6, 0.95, 1, 0.22)
    cr.set_line_width(1)
    vx = w * 0.5
    for i in range(-14, 15):
        cr.move_to(vx + i * 6, horizon)
        cr.line_to(vx + i * w / 9, h + 2)
    for j in range(1, 9):
        yy = horizon + (h - horizon) * (j / 8) ** 1.8
        cr.move_to(0, yy)
        cr.line_to(w, yy)
    cr.stroke()
    cr.restore()
    # A soft sun behind the title.
    r = RadialGradient(w * 0.82, h * 0.28, 2, w * 0.82, h * 0.28, h * 0.6)
    r.add_color_stop_rgba(0, 1, 1, 1, 0.35)
    r.add_color_stop_rgba(1, 1, 1, 1, 0)
    cr.set_source(r)
    cr.paint()


def headline(cr, w, h, title, subtitle):
    """The large title of the illustration, with a hard shadow."""
    size = max(16, min(30, w / 20))
    cr.select_font_face("Sans", 1, 1)
    cr.set_font_size(size)
    for dx, dy, col in ((2, 2, (0.05, 0.02, 0.12)), (0, 0, (1, 1, 1))):
        cr.set_source_rgb(*col)
        cr.move_to(16 + dx, 14 + size + dy)
        cr.show_text(title)
    cr.select_font_face("Sans", 1, 0)
    cr.set_font_size(max(10, size * 0.42))
    cr.set_source_rgb(0.9, 1, 1)
    cr.move_to(18, 14 + size + 6 + size * 0.45)
    cr.show_text(subtitle)


# ---------------------------------------------------------------------------
# Scenes. Each: draw(cr, w, h, t, ctx) on the area right of / below the
# headline. ctx: logo painter, dark flag, texts.

def _stage(w, h):
    """The rectangle where scenes draw: below the headline."""
    top = 64
    return 14, top, w - 28, h - top - 10


def scene_desktop(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    s = min(34, sh / 4.2)
    clip(cr, x + 2, y, s)
    dock_column(cr, x + sw - s - 2, y, 4, s, ctx["logo"])
    wm_window(cr, x + sw * 0.18, y + 6, sw * 0.42, sh * 0.62, "Terminal",
              focused=False, content=_terminal)
    wm_window(cr, x + sw * 0.38, y + sh * 0.22, sw * 0.38, sh * 0.62,
              ctx["files"], focused=True, content=_files)
    k = phase(t, 0.1, 0.5) - phase(t, 0.6, 0.95)
    cursor(cr, lerp(x + sw * 0.7, x + sw * 0.5, k), lerp(y + sh * 0.9, y + sh * 0.35, k))


def _terminal(cr, x, y, w, h):
    cr.set_source_rgb(0.08, 0.08, 0.1)
    cr.rectangle(x, y, w, h)
    cr.fill()
    text(cr, x + 4, y + 12, "$ big-wm-welcome", 8, LCD_ON, family="Monospace")


def _files(cr, x, y, w, h):
    for i in range(min(6, int(w / 30))):
        _app_glyph(cr, x + 16 + i * 28, y + 18, 18, 2)


def scene_menu(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    px, py = x + sw * 0.35, y + sh * 0.12
    k = phase(t, 0.0, 0.25)
    cx, cy = lerp(x + sw * 0.85, px, k), lerp(y + sh * 0.9, py, k)
    pressed = 0.25 <= t < 0.33
    if t >= 0.3:
        sel = int((t - 0.45) / 0.12) if t > 0.45 else -1
        menu(cr, px, py, ctx["menu_items"], selected=min(sel, len(ctx["menu_items"]) - 1))
        if t > 0.45:
            cy = py + 15 * (min(sel, len(ctx["menu_items"]) - 1) + 1) + 7
            cx = px + 50
    cursor(cr, cx, cy, pressed)
    text(cr, x, y + sh - 2, ctx["right_click"], 10, (1, 1, 1), bold=True)


def scene_window(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    ww, wh = sw * 0.42, sh * 0.62
    a = (x + sw * 0.05, y + sh * 0.08)
    b = (x + sw * 0.45, y + sh * 0.3)
    icon_at = (x + 4, y + sh - 36)
    if t < 0.45:  # drag
        k = phase(t, 0.08, 0.42)
        wx, wy = lerp(a[0], b[0], k), lerp(a[1], b[1], k)
        wm_window(cr, wx, wy, ww, wh, ctx["window"], content=_terminal)
        cursor(cr, wx + ww / 2, wy + 7, 0.08 < t < 0.42)
    elif t < 0.7:  # minimize to an icon
        k = phase(t, 0.5, 0.62)
        if k < 1:
            sx = lerp(ww, 32, k)
            sy = lerp(wh, 32, k)
            wm_window(cr, lerp(b[0], icon_at[0], k), lerp(b[1], icon_at[1], k), sx, sy,
                      ctx["window"])
        tile(cr, *icon_at, 32)
        _app_glyph(cr, icon_at[0] + 16, icon_at[1] + 16, 18, 1)
        cursor(cr, b[0] + 8, b[1] + 7, 0.46 < t < 0.5)
    else:  # double-click the icon: restored
        k = phase(t, 0.78, 0.9)
        cursor(cr, icon_at[0] + 16, icon_at[1] + 16, 0.72 < t < 0.78)
        if k > 0:
            wm_window(cr, lerp(icon_at[0], b[0], k), lerp(icon_at[1], b[1], k),
                      lerp(32, ww, k), lerp(32, wh, k), ctx["window"],
                      content=_terminal if k > 0.9 else None)
        else:
            tile(cr, *icon_at, 32)
            _app_glyph(cr, icon_at[0] + 16, icon_at[1] + 16, 18, 1)


def scene_dock(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    s = min(34, sh / 4.2)
    dx = x + sw - s - 2
    target = 2
    k = phase(t, 0.0, 0.3)
    cx = lerp(x + sw * 0.4, dx + s / 2, k)
    cy = lerp(y + sh * 0.85, y + target * s + s / 2, k)
    pressed = 0.32 < t < 0.36 or 0.4 < t < 0.44
    dock_column(cr, dx, y, 5, s, ctx["logo"], highlight=target if t > 0.3 else -1)
    k2 = phase(t, 0.48, 0.7)
    if k2 > 0:
        wm_window(cr, lerp(dx, x + sw * 0.15, k2), lerp(y + target * s, y + 10, k2),
                  lerp(s, sw * 0.5, k2), lerp(s, sh * 0.7, k2), ctx["files"],
                  content=_files if k2 > 0.95 else None)
    cursor(cr, cx, cy, pressed)


def scene_clip(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    s = 44
    ws = 1 + int(t * 3) % 3
    clip(cr, x + 4, y + 4, s, ws)
    cursor(cr, x + 4 + s - 6, y + 6, (t * 3) % 1 < 0.15)
    colors = [(0.35, 0.2, 0.55), (0.15, 0.4, 0.6), (0.05, 0.55, 0.55)]
    cr.set_source_rgba(*colors[ws - 1], 0.5)
    cr.rectangle(x + s + 20, y + 4, sw - s - 24, sh - 8)
    cr.fill()
    text(cr, x + s + 30, y + 24, f"{ctx['workspace']} {ws}", 12, (1, 1, 1), bold=True)
    wm_window(cr, x + s + 40 + ws * 20, y + 34, sw * 0.4, sh * 0.5,
              ctx["window"] + f" {ws}", content=_terminal)


def scene_keys(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    combos = ctx.get("combos") or []
    if not combos:
        text(cr, x, y + 30, ctx["no_shortcuts"], 12, (1, 1, 1), bold=True)
        return
    idx = int(t * len(combos)) % len(combos)
    name, combo = combos[idx]
    keys = combo.split("+")
    kx = x + 6
    for i, kname in enumerate(keys):
        kw = max(38, 12 + len(kname) * 9)
        down = (t * len(combos)) % 1 > 0.25 + i * 0.12
        bevel(cr, kx, y + 20 + (2 if down else 0), kw, 34, (0.85, 0.85, 0.85), not down)
        text(cr, kx + 8, y + 42 + (2 if down else 0), kname, 12, BLACK, bold=True)
        kx += kw + 8
        if i < len(keys) - 1:
            text(cr, kx - 6, y + 42, "+", 14, (1, 1, 1), bold=True)
            kx += 8
    text(cr, x + 6, y + 82, name, 13, (1, 1, 1), bold=True)


def scene_wprefs(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    ww, wh = min(sw * 0.7, 360), sh * 0.88

    def body(cr, bx, by, bw, bh):
        sel = int(t * 6) % 6
        for i in range(6):
            tx = bx + 8 + i * 40
            bevel(cr, tx, by + 6, 36, 36, LIGHT if i == sel else GRAY, i != sel)
            _app_glyph(cr, tx + 18, by + 24, 20, i)
        bevel(cr, bx + 8, by + 50, bw - 16, bh - 58, GRAY, False)
        for j in range(3):
            bevel(cr, bx + 16, by + 60 + j * 18, 12, 12, LIGHT if (sel + j) % 2 else GRAY,
                  False)
            cr.set_source_rgb(*DARK)
            cr.rectangle(bx + 34, by + 64 + j * 18, bw * 0.5, 4)
            cr.fill()
    wm_window(cr, x + (sw - ww) / 2, y + 2, ww, wh, "WPrefs", content=body)


def scene_monitors(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    k = phase(t, 0.15, 0.6) - phase(t, 0.75, 0.98)
    for i, (mx, mw) in enumerate(((x + sw * 0.12, sw * 0.34), (lerp(x + sw * 0.6, x + sw * 0.47, k), sw * 0.26))):
        mh = mw * 0.62
        my = y + sh * 0.18 + (0 if i == 0 else mh * 0.2)
        bevel(cr, mx, my, mw, mh, (0.2, 0.2, 0.25))
        cr.set_source_rgb(*(BLUE if i == 0 else CYAN))
        cr.rectangle(mx + 4, my + 4, mw - 8, mh - 8)
        cr.fill()
        text(cr, mx + mw / 2 - 4, my + mh / 2 + 6, str(i + 1), 18, (1, 1, 1), bold=True)
        bevel(cr, mx + mw / 2 - 8, my + mh, 16, 8, GRAY)
    cursor(cr, lerp(x + sw * 0.72, x + sw * 0.59, k), y + sh * 0.35, 0.15 < t < 0.6)


def scene_access(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    cx, cy = x + sw * 0.5, y + sh * 0.5
    shake = math.sin(t * 2 * math.pi * 6) * 14 * (1 - phase(t, 0.3, 0.4))
    if t > 0.35:
        for r in range(3):
            rad = ((t - 0.35) * 160 + r * 22) % 70
            cr.set_source_rgba(1, 1, 0.7, max(0, 1 - rad / 70))
            cr.set_line_width(3)
            cr.arc(cx, cy, rad + 6, 0, 2 * math.pi)
            cr.stroke()
    cursor(cr, cx + shake, cy)


def scene_browser(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)

    def body(cr, bx, by, bw, bh):
        bevel(cr, bx + 6, by + 5, bw - 12, 16, LIGHT, False)
        text(cr, bx + 10, by + 17, "https://github.com/big-windowmaker", 9, BLACK)
        _app_glyph(cr, bx + bw / 2, by + bh / 2 + 10, min(bw, bh) * 0.45, 3)
    wm_window(cr, x + sw * 0.15, y + 4, sw * 0.7, sh * 0.9, ctx.get("browser") or "Web",
              content=body)


def scene_theme(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    k = 0.5 + 0.45 * math.sin(t * 2 * math.pi)
    ww, wh = sw * 0.6, sh * 0.85
    wx, wy = x + (sw - ww) / 2, y + 4
    cr.save()
    cr.rectangle(wx - 2, wy - 2, ww * k + 2, wh + 4)
    cr.clip()
    wm_window(cr, wx, wy, ww, wh, ctx["light"], content=_lines(GRAY, BLACK))
    cr.restore()
    cr.save()
    cr.rectangle(wx + ww * k, wy - 2, ww * (1 - k) + 2, wh + 4)
    cr.clip()
    wm_window(cr, wx, wy, ww, wh, ctx["dark"], body=(0.22, 0.22, 0.24),
              content=_lines((0.22, 0.22, 0.24), (0.9, 0.9, 0.9)))
    cr.restore()
    cr.set_source_rgb(1, 1, 0.6)
    cr.set_line_width(2)
    cr.move_to(wx + ww * k, wy - 6)
    cr.line_to(wx + ww * k, wy + wh + 6)
    cr.stroke()


def _lines(bg, fg):
    def f(cr, x, y, w, h):
        cr.set_source_rgb(*bg)
        cr.rectangle(x, y, w, h)
        cr.fill()
        for i in range(4):
            bevel(cr, x + 10, y + 10 + i * 20, 14, 14, bg, i % 2 == 0)
            cr.set_source_rgb(*fg)
            cr.rectangle(x + 32, y + 15 + i * 20, w * 0.5, 4)
            cr.fill()
    return f


def scene_tabbed(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    tabs = ctx["tabs"]
    cur = int(t * len(tabs)) % len(tabs)
    by = y + sh - 46
    bevel(cr, x, by, sw, 46, (0.75, 0.75, 0.77))
    tx = x + 6
    for i, name in enumerate(tabs):
        tw = 12 + len(name) * 7
        col = (0.36, 0.62, 0.66) if i == cur else (0.55, 0.72, 0.74)
        cr.set_source_rgb(*col)
        cr.move_to(tx, by)
        cr.line_to(tx + 6, by - 14)
        cr.line_to(tx + tw - 6, by - 14)
        cr.line_to(tx + tw, by)
        cr.close_path()
        cr.fill()
        text(cr, tx + 8, by - 3, name, 9, BLACK, bold=i == cur)
        tx += tw + 2
    for j in range(6):
        _app_glyph(cr, x + 26 + j * 40, by + 23, 24, (j + cur) % 5)
    cursor(cr, x + 6 + cur * 60 + 20, by - 8, (t * len(tabs)) % 1 < 0.2)


def scene_zones(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    zones = [(0, 0, 0.5, 1), (0.5, 0, 0.5, 0.5), (0.5, 0.5, 0.5, 0.5)]
    target = 0 if t < 0.5 else 2
    tiling = t >= 0.75
    for i, (zx, zy, zw, zh) in enumerate(zones):
        rx, ry, rw, rh = x + zx * sw + 3, y + zy * sh + 3, zw * sw - 6, zh * sh - 6
        if tiling:
            wm_window(cr, rx + 2, ry + 2, rw - 4, rh - 4, f"{ctx['window']} {i + 1}",
                      focused=i == 0, content=_terminal if i == 0 else _files)
            continue
        cr.set_source_rgba(1, 1, 1, 0.28 if i == target else 0.12)
        cr.rectangle(rx, ry, rw, rh)
        cr.fill()
        cr.set_source_rgba(1, 1, 1, 0.7)
        cr.set_line_width(1.5)
        cr.rectangle(rx, ry, rw, rh)
        cr.stroke()
        text(cr, rx + rw / 2 - 5, ry + rh / 2 + 8, str(i + 1), 20, (1, 1, 1), bold=True)
    if not tiling:
        k = phase(t % 0.5, 0.05, 0.4)
        zx, zy, zw, zh = zones[target]
        tx, ty = x + zx * sw + 8, y + zy * sh + 8
        wx, wy = lerp(x + sw * 0.3, tx, k), lerp(y + sh * 0.3, ty, k)
        ww, wh = lerp(sw * 0.35, zw * sw - 16, k), lerp(sh * 0.45, zh * sh - 16, k)
        wm_window(cr, wx, wy, ww, wh, ctx["window"], content=_terminal)
        cursor(cr, wx + ww / 2, wy + 7, k < 0.98)
    text(cr, x + 4, y + sh - 4, ctx["tiling"] if tiling else ctx["zones"], 11,
         (1, 1, 1), bold=True)


def scene_clipboard(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    samples = ctx["clip_samples"]
    n = min(len(samples), 1 + int(t * 4))

    def body(cr, bx, by, bw, bh):
        text(cr, bx + 8, by + 18, samples[n - 1], 10, BLACK)
        if (t * 4) % 1 < 0.4:
            cr.set_source_rgba(0.2, 0.4, 0.9, 0.35)
            cr.rectangle(bx + 6, by + 6, min(bw - 12, len(samples[n - 1]) * 6.2), 16)
            cr.fill()
    wm_window(cr, x, y + 4, sw * 0.5, sh * 0.5, ctx["editor"], content=body)
    my = y + 10
    mx = x + sw * 0.56
    menu(cr, mx, my, list(reversed(samples[:n])), title="wmcliphist", w=sw * 0.42,
         selected=0)
    lcd_tile(cr, x + sw - 40, y + sh - 40, 38, lambda cr, a, b, s: _app_glyph(
        cr, a + s / 2, b + s / 2, s * 0.7, 4))


def scene_shutdown(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    s = 44
    tx, ty = x + sw - s - 4, y + 4
    tile(cr, tx, ty, s)
    cr.set_source_rgb(0.8, 0.15, 0.15)
    cr.set_line_width(3)
    cr.arc(tx + s / 2, ty + s / 2 + 2, s / 4, -math.pi * 0.35, math.pi * 1.35)
    cr.stroke()
    cr.move_to(tx + s / 2, ty + s / 4 - 2)
    cr.line_to(tx + s / 2, ty + s / 2)
    cr.stroke()
    if t > 0.3:
        def body(cr, bx, by, bw, bh):
            text(cr, bx + 10, by + 22, ctx["shutdown_q"], 10, BLACK)
            for i, b in enumerate(ctx["shutdown_buttons"]):
                bevel(cr, bx + 10 + i * (bw - 20) / 3, by + bh - 30, (bw - 20) / 3 - 6, 22,
                      GRAY, not (0.6 < t < 0.7 and i == 2))
                text(cr, bx + 16 + i * (bw - 20) / 3, by + bh - 15, b, 9, BLACK)
        wm_window(cr, x + sw * 0.1, y + sh * 0.2, sw * 0.62, sh * 0.6, "wmshutdown",
                  content=body)
    k = phase(t, 0.0, 0.25)
    cursor(cr, lerp(x + sw * 0.5, tx + s / 2, k), lerp(y + sh * 0.8, ty + s / 2, k),
           0.22 < t < 0.3)
    text(cr, x + 4, y + sh - 4, ctx["shutdown_note"], 10, (1, 1, 0.75), bold=True)


def scene_dockapps(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    apps = ctx.get("dockapps") or []
    s = 52
    per_row = max(1, int(sw // (s + 10)))
    for i, (prog, name) in enumerate(apps[:per_row * 2]):
        col, row = i % per_row, i // per_row
        tx, ty = x + col * (s + 10), y + row * (s + 22)
        lcd_tile(cr, tx, ty, s, _dockapp_picture(prog, t))
        text(cr, tx, ty + s + 12, name, 9, (1, 1, 1), bold=True)
    if not apps:
        text(cr, x, y + 30, ctx["no_dockapps"], 12, (1, 1, 1), bold=True)


def _dockapp_picture(prog, t):
    """An illustration of each dockapp (not the computer's data)."""
    def f(cr, x, y, s):
        cr.set_source_rgb(*LCD_ON)
        cr.set_line_width(1.5)
        if prog == "wmclock":
            text(cr, x + 3, y + s / 2 + 5, "12:00", s / 3.3, LCD_ON, bold=True, family="Monospace")
        elif prog in ("wmcpuload", "wmmemload", "wmnet", "wmgtemp"):
            cr.move_to(x, y + s - 2)
            for i in range(12):
                cr.line_to(x + i * s / 11, y + s * (0.3 + 0.5 * abs(math.sin(i * 1.7 + t * 6))))
            cr.stroke()
        elif prog == "wmeyes":
            for ex in (x + s * 0.3, x + s * 0.7):
                cr.set_source_rgb(1, 1, 1)
                cr.arc(ex, y + s / 2, s / 5, 0, 2 * math.pi)
                cr.fill()
                cr.set_source_rgb(0, 0, 0)
                cr.arc(ex + math.cos(t * 6.28) * s / 10, y + s / 2, s / 14, 0, 2 * math.pi)
                cr.fill()
        elif prog == "wmcube":
            a = t * 2 * math.pi
            pts = [(math.cos(a + k * math.pi / 2), math.sin(a + k * math.pi / 2)) for k in range(4)]
            for dz in (-0.25, 0.25):
                cr.move_to(x + s / 2 + pts[0][0] * s / 4, y + s / 2 + pts[0][1] * s / 8 + dz * s)
                for px, py in pts[1:] + pts[:1]:
                    cr.line_to(x + s / 2 + px * s / 4, y + s / 2 + py * s / 8 + dz * s)
            cr.stroke()
        elif prog == "wmglobe":
            cr.set_source_rgb(*BLUE)
            cr.arc(x + s / 2, y + s / 2, s / 2.6, 0, 2 * math.pi)
            cr.fill()
        elif prog == "wmix":
            for i in range(3):
                cr.rectangle(x + 4 + i * s / 3.3, y + s - 4 - s * 0.6 * (0.4 + 0.3 * i), 6,
                             s * 0.6 * (0.4 + 0.3 * i))
            cr.fill()
        else:
            for i in range(3):
                cr.rectangle(x + 4, y + 6 + i * s / 3.5, (s - 8) * (0.3 + 0.25 * i), 5)
            cr.fill()
    return f


def scene_community(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    size = min(sh * 0.9, 120)
    cx = x + sw * 0.5
    logo(cr, ctx["logo"], cx - size / 2, y + (sh - size) / 2, size)
    for i in range(6):
        a = t * 2 * math.pi + i * math.pi / 3
        px = cx + math.cos(a) * size * 0.95
        py = y + sh / 2 + math.sin(a) * size * 0.5
        cr.set_source_rgba(1, 1, 1, 0.5)
        cr.set_line_width(1)
        cr.move_to(cx, y + sh / 2)
        cr.line_to(px, py)
        cr.stroke()
        tile(cr, px - 13, py - 13, 26)
        _app_glyph(cr, px, py, 14, i)


def scene_tribute(cr, w, h, t, ctx):
    x, y, sw, sh = _stage(w, h)
    s = min(34, sh / 3.4)
    dock_column(cr, x + 4, y, 3, s, ctx["logo"])
    text(cr, x + s + 22, y + 26, "Alfredo Kojima", 18, (1, 1, 1), bold=True, italic=True)
    text(cr, x + s + 22, y + 46, ctx["tribute"], 11, (0.9, 1, 1))
    cr.set_source_rgba(1, 1, 1, 0.6 + 0.4 * math.sin(t * 2 * math.pi))
    cr.arc(x + s + 30, y + 70, 3, 0, 2 * math.pi)
    cr.fill()


SCENES = {
    "desktop": scene_desktop, "menu": scene_menu, "window": scene_window,
    "dock": scene_dock, "clip": scene_clip, "keys": scene_keys,
    "wprefs": scene_wprefs, "monitors": scene_monitors, "access": scene_access,
    "browser": scene_browser, "theme": scene_theme, "tabbed": scene_tabbed,
    "zones": scene_zones, "clipboard": scene_clipboard, "shutdown": scene_shutdown,
    "dockapps": scene_dockapps, "community": scene_community,
    "tribute": scene_tribute,
    # Page illustrations (before a topic is chosen).
    "settings": scene_wprefs, "tools": scene_dock,
}


def draw(cr, w, h, scene, t, ctx, title, subtitle, dark=False):
    background(cr, w, h, dark)
    headline(cr, w, h, title, subtitle)
    fn = SCENES.get(scene, scene_desktop)
    cr.save()
    fn(cr, w, h, t, ctx)
    cr.restore()
    # A thin sunken frame, the area sits in the window like a picture.
    cr.set_line_width(1)
    cr.set_source_rgb(*DARK)
    cr.move_to(0.5, h - 0.5)
    cr.line_to(0.5, 0.5)
    cr.line_to(w - 0.5, 0.5)
    cr.stroke()
    cr.set_source_rgb(*LIGHT)
    cr.move_to(w - 0.5, 0.5)
    cr.line_to(w - 0.5, h - 0.5)
    cr.line_to(0.5, h - 0.5)
    cr.stroke()
