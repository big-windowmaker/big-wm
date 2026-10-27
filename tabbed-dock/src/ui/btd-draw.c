/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-draw.h"

#include <math.h>

static BtdRGBA
hex(guint32 rgb)
{
    return (BtdRGBA){ ((rgb >> 16) & 0xff) / 255.0, ((rgb >> 8) & 0xff) / 255.0, (rgb & 0xff) / 255.0, 1.0 };
}

void
btd_palette_clear(BtdPalette *p)
{
    g_clear_pointer(&p->marble, cairo_pattern_destroy);
    g_clear_pointer(&p->teal_marble, cairo_pattern_destroy);
}

void
btd_palette_init(BtdPalette *p, BtdStyle style, gboolean dark)
{
    btd_palette_clear(p);
    p->style = style;
    p->dark_mode = dark && style != BTD_STYLE_MODERN;
    if (style == BTD_STYLE_MODERN) {
        /* "Modern" variant: flat, dark, rounded — an option, never the default. */
        p->shelf_top = hex(0x343a3e);
        p->shelf_bottom = hex(0x2a2f33);
        p->tab_active = hex(0x343a3e);
        p->tab_inactive = hex(0x23272a);
        p->tile_a = (BtdRGBA){ 1, 1, 1, 0.0 };
        p->tile_b = (BtdRGBA){ 1, 1, 1, 0.0 };
        p->tile_active_a = (BtdRGBA){ 1, 1, 1, 0.14 };
        p->tile_active_b = (BtdRGBA){ 1, 1, 1, 0.08 };
        p->light = (BtdRGBA){ 1, 1, 1, 0.10 };
        p->mid = hex(0x4a5257);
        p->dark = (BtdRGBA){ 0, 0, 0, 0.35 };
        p->black = hex(0x111416);
        p->text = hex(0xf2f2f2);
        p->text_inactive = hex(0xa8b0b5);
        p->text_hint = hex(0x8a9297);
        p->accent = hex(0x5294e2);
        p->radius = 6;
        return;
    }
    /* Classic: OpenStep four grays + Big WindowMaker theme (menu gradient
     * #c2c0c5→#828085, IconBack dgradient #a6a6b6→#515561). */
    p->shelf_top = hex(0xc2c0c5);
    p->shelf_bottom = hex(0xaaa8ad);
    p->tab_active = hex(0xc2c0c5);
    p->tab_inactive = hex(0x939196);
    p->tile_a = hex(0xa6a6b6);
    p->tile_b = hex(0x515561);
    p->tile_active_a = hex(0xc4c4d2);
    p->tile_active_b = hex(0x6b6f7c);
    p->light = hex(0xffffff);
    p->mid = hex(0xaaaaaa);
    p->dark = hex(0x555555);
    p->black = hex(0x000000);
    p->text = hex(0x000000);
    p->text_inactive = hex(0x1e1e1e);
    p->text_hint = hex(0x4a4a4a);
    p->accent = hex(0x202a2e);
    p->radius = 0;
    if (style == BTD_STYLE_CLASSIC) {
        /* Colours measured on the OPENSTEP 4.0 beta shelf: marble body
         * around #a6a6a6, teal tabs #4d8186 with a #9ac2c4 top light, lit
         * edges #eaeaea / #d5f9fd, shadow edges almost black. */
        p->teal = hex(0x4d8186);
        p->teal_top = hex(0x9ac2c4);
        p->teal_edge = hex(0xd5f9fd);
        p->body_top = hex(0xd0d2d4);
        p->body_bottom = hex(0x8a8a8a);
        p->edge_dark = hex(0x0a0a0e);
        p->light = hex(0xeaeaea);
        p->text = hex(0x000000);
        p->text_inactive = hex(0x000000);
        p->marble = btd_marble_pattern(0xa8 / 255.0, 0xa8 / 255.0, 0xaa / 255.0, 0.03, 1);
        p->teal_marble = btd_marble_pattern(0x4d / 255.0, 0x81 / 255.0, 0x86 / 255.0, 0.035, 7);
    }
    if (!p->dark_mode)
        return;
    /* Dark mode: the dark grays of the BigWindowMaker-Dark Window Maker
     * style (menus #4a4a4a→#2e2e2e, IconBack #5c5c64→#1c1c22), the same
     * bevels (lit edge a mid gray, shadow black) and light text. */
    p->shelf_top = hex(0x4a4a4a);
    p->shelf_bottom = hex(0x383838);
    p->tab_active = hex(0x4a4a4a);
    p->tab_inactive = hex(0x2e2e2e);
    p->tile_a = hex(0x5c5c64);
    p->tile_b = hex(0x1c1c22);
    p->tile_active_a = hex(0x76767e);
    p->tile_active_b = hex(0x34343c);
    p->light = hex(0x7a7a7a);
    p->mid = hex(0x5a5a5a);
    p->dark = hex(0x1e1e1e);
    p->black = hex(0x000000);
    p->text = hex(0xe6e6e6);
    p->text_inactive = hex(0xc8c8c8);
    p->text_hint = hex(0xa0a0a0);
    p->accent = hex(0xd0d0d0);
    if (style == BTD_STYLE_CLASSIC) {
        /* the OPENSTEP beta shelf at night: darker marble, deep teal tabs */
        btd_palette_clear(p);
        p->teal = hex(0x2c4f53);
        p->teal_top = hex(0x5e8a8d);
        p->teal_edge = hex(0x86b3b7);
        p->body_top = hex(0x6a6a6c);
        p->body_bottom = hex(0x141414);
        p->edge_dark = hex(0x000000);
        p->light = hex(0x8a8a8a);
        p->text = hex(0xeaeaea);
        p->text_inactive = hex(0xeaeaea);
        p->marble = btd_marble_pattern(0x48 / 255.0, 0x48 / 255.0, 0x4a / 255.0, 0.03, 1);
        p->teal_marble = btd_marble_pattern(0x2c / 255.0, 0x4f / 255.0, 0x53 / 255.0, 0.035, 7);
    }
}

void
btd_set_color(cairo_t *cr, const BtdRGBA *c)
{
    cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
}

PangoFontDescription *
btd_font_new(gboolean bold, double px)
{
    /* 1990s NeXT typography: Helvetica (or its metric clones). */
    PangoFontDescription *fd =
        pango_font_description_from_string("Helvetica, Nimbus Sans, Nimbus Sans L, Liberation Sans, Arimo, DejaVu Sans, Sans");
    pango_font_description_set_weight(fd, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);
    return fd;
}

/* Coordinates: (x, y) top-left of the tab box; for BTD_TAB_UP the tab's
 * open side (joining the shelf) is the bottom edge. */
void
btd_tab_path(cairo_t *cr, double x, double y, double w, double h, BtdTabDirection dir, double radius,
             BtdStyle style)
{
    double base = dir == BTD_TAB_UP ? y + h : y;
    double top = dir == BTD_TAB_UP ? y : y + h;
    cairo_new_path(cr);
    if (style == BTD_STYLE_MODERN) {
        double r = MIN(radius, h / 2);
        double sgn = dir == BTD_TAB_UP ? 1 : -1;
        cairo_move_to(cr, x, base);
        cairo_line_to(cr, x, top + sgn * r);
        cairo_curve_to(cr, x, top, x, top, x + r, top);
        cairo_line_to(cr, x + w - r, top);
        cairo_curve_to(cr, x + w, top, x + w, top, x + w, top + sgn * r);
        cairo_line_to(cr, x + w, base);
        cairo_close_path(cr);
        return;
    }
    double s = MIN(BTD_TAB_SLOPE, w / 3);
    cairo_move_to(cr, x, base);
    cairo_curve_to(cr, x + s * 0.55, base, x + s * 0.45, top, x + s, top);
    cairo_line_to(cr, x + w - s, top);
    cairo_curve_to(cr, x + w - s * 0.45, top, x + w - s * 0.55, base, x + w, base);
    cairo_close_path(cr);
}

void
btd_draw_tab(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean active,
             gboolean highlighted, BtdTabDirection dir, PangoLayout *label)
{
    cairo_save(cr);
    double inset = active ? 0 : 2; /* inactive tabs sit a little lower */
    double ty = dir == BTD_TAB_UP ? y + inset : y;
    double th = h - inset;
    btd_tab_path(cr, x, ty, w, th, dir, p->radius, p->style);
    if (p->style != BTD_STYLE_MODERN) {
        cairo_pattern_t *pat = cairo_pattern_create_linear(0, ty, 0, ty + th);
        const BtdRGBA *c = active ? &p->tab_active : &p->tab_inactive;
        double lift = highlighted ? 0.08 : 0;
        cairo_pattern_add_color_stop_rgb(pat, dir == BTD_TAB_UP ? 0 : 1, MIN(1, c->r + 0.06 + lift),
                                         MIN(1, c->g + 0.06 + lift), MIN(1, c->b + 0.06 + lift));
        cairo_pattern_add_color_stop_rgb(pat, dir == BTD_TAB_UP ? 1 : 0, MIN(1, c->r + lift), MIN(1, c->g + lift),
                                         MIN(1, c->b + lift));
        cairo_set_source(cr, pat);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(pat);

        /* Edges: lit left slope and top in white, right slope in black with
         * a dark-gray inner line. The joining edge is left open. */
        double s = MIN(BTD_TAB_SLOPE, w / 3);
        double base = dir == BTD_TAB_UP ? ty + th : ty;
        double top = dir == BTD_TAB_UP ? ty + 0.5 : ty + th - 0.5;
        cairo_new_path(cr);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, x + 0.5, base);
        cairo_curve_to(cr, x + s * 0.55, base, x + s * 0.45, top, x + s, top);
        cairo_line_to(cr, x + w - s, top);
        btd_set_color(cr, dir == BTD_TAB_UP ? &p->light : &p->dark);
        cairo_stroke(cr);
        cairo_move_to(cr, x + w - s, top);
        cairo_curve_to(cr, x + w - s * 0.45, top, x + w - s * 0.55, base, x + w - 0.5, base);
        btd_set_color(cr, &p->black);
        cairo_stroke(cr);
        cairo_move_to(cr, x + w - s - 1, top + (dir == BTD_TAB_UP ? 1 : -1));
        cairo_curve_to(cr, x + w - s * 0.45 - 1, top + (dir == BTD_TAB_UP ? 1 : -1), x + w - s * 0.55 - 1, base,
                       x + w - 1.5, base);
        btd_set_color(cr, &p->dark);
        cairo_stroke(cr);
    } else {
        const BtdRGBA *c = active ? &p->tab_active : &p->tab_inactive;
        btd_set_color(cr, c);
        cairo_fill(cr);
        if (highlighted) {
            btd_tab_path(cr, x, ty, w, th, dir, p->radius, p->style);
            cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
            cairo_fill(cr);
        }
        if (active) {
            double ly = dir == BTD_TAB_UP ? ty + 1 : ty + th - 3;
            btd_set_color(cr, &p->accent);
            cairo_rectangle(cr, x + p->radius, ly, w - 2 * p->radius, 2);
            cairo_fill(cr);
        }
    }
    if (label) {
        int lw, lh;
        pango_layout_get_pixel_size(label, &lw, &lh);
        double lx = x + (w - lw) / 2.0, ly = ty + (th - lh) / 2.0 + (dir == BTD_TAB_UP ? 0.5 : -0.5);
        btd_set_color(cr, active ? &p->text : &p->text_inactive);
        cairo_move_to(cr, round(lx), round(ly));
        pango_cairo_show_layout(cr, label);
    }
    cairo_restore(cr);
}

void
btd_draw_bevel(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean sunken)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    const BtdRGBA *tl = sunken ? &p->dark : &p->light;
    const BtdRGBA *br = sunken ? &p->light : &p->black;
    btd_set_color(cr, tl);
    cairo_move_to(cr, x + 0.5, y + h - 0.5);
    cairo_line_to(cr, x + 0.5, y + 0.5);
    cairo_line_to(cr, x + w - 0.5, y + 0.5);
    cairo_stroke(cr);
    btd_set_color(cr, br);
    cairo_move_to(cr, x + w - 0.5, y + 0.5);
    cairo_line_to(cr, x + w - 0.5, y + h - 0.5);
    cairo_line_to(cr, x + 0.5, y + h - 0.5);
    cairo_stroke(cr);
    if (!sunken) {
        btd_set_color(cr, &p->dark);
        cairo_move_to(cr, x + w - 1.5, y + 1.5);
        cairo_line_to(cr, x + w - 1.5, y + h - 1.5);
        cairo_line_to(cr, x + 1.5, y + h - 1.5);
        cairo_stroke(cr);
    } else {
        btd_set_color(cr, &p->black);
        cairo_move_to(cr, x + 1.5, y + h - 1.5);
        cairo_line_to(cr, x + 1.5, y + 1.5);
        cairo_line_to(cr, x + w - 1.5, y + 1.5);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

static void
rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r, gboolean top, gboolean bottom)
{
    cairo_new_path(cr);
    double rt = top ? r : 0, rb = bottom ? r : 0;
    cairo_move_to(cr, x + rt, y);
    cairo_line_to(cr, x + w - rt, y);
    if (rt)
        cairo_arc(cr, x + w - rt, y + rt, rt, -G_PI / 2, 0);
    cairo_line_to(cr, x + w, y + h - rb);
    if (rb)
        cairo_arc(cr, x + w - rb, y + h - rb, rb, 0, G_PI / 2);
    cairo_line_to(cr, x + rb, y + h);
    if (rb)
        cairo_arc(cr, x + rb, y + h - rb, rb, G_PI / 2, G_PI);
    cairo_line_to(cr, x, y + rt);
    if (rt)
        cairo_arc(cr, x + rt, y + rt, rt, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
}

void
btd_draw_shelf(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, double gap_x, double gap_w,
               BtdTabDirection dir)
{
    cairo_save(cr);
    if (p->style == BTD_STYLE_MODERN) {
        rounded_rect(cr, x, y, w, h, p->radius, dir == BTD_TAB_UP, dir == BTD_TAB_DOWN);
        cairo_pattern_t *pat = cairo_pattern_create_linear(0, y, 0, y + h);
        cairo_pattern_add_color_stop_rgba(pat, 0, p->shelf_top.r, p->shelf_top.g, p->shelf_top.b, 0.96);
        cairo_pattern_add_color_stop_rgba(pat, 1, p->shelf_bottom.r, p->shelf_bottom.g, p->shelf_bottom.b, 0.96);
        cairo_set_source(cr, pat);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);
        cairo_restore(cr);
        return;
    }
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, y, 0, y + h);
    btd_set_color(cr, &p->shelf_top);
    cairo_pattern_add_color_stop_rgb(pat, 0, p->shelf_top.r, p->shelf_top.g, p->shelf_top.b);
    cairo_pattern_add_color_stop_rgb(pat, 1, p->shelf_bottom.r, p->shelf_bottom.g, p->shelf_bottom.b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    /* Raised bevel; the edge that meets the tabs is open under the active tab. */
    cairo_set_line_width(cr, 1);
    double edge = dir == BTD_TAB_UP ? y + 0.5 : y + h - 0.5;
    btd_set_color(cr, dir == BTD_TAB_UP ? &p->light : &p->black);
    cairo_move_to(cr, x, edge);
    cairo_line_to(cr, gap_x + 1, edge);
    cairo_move_to(cr, gap_x + gap_w - 1, edge);
    cairo_line_to(cr, x + w, edge);
    cairo_stroke(cr);
    /* left edge (lit) */
    btd_set_color(cr, &p->light);
    cairo_move_to(cr, x + 0.5, y);
    cairo_line_to(cr, x + 0.5, y + h);
    cairo_stroke(cr);
    /* right edge (shadow) */
    btd_set_color(cr, &p->black);
    cairo_move_to(cr, x + w - 0.5, y);
    cairo_line_to(cr, x + w - 0.5, y + h);
    cairo_stroke(cr);
    btd_set_color(cr, &p->dark);
    cairo_move_to(cr, x + w - 1.5, y + 1);
    cairo_line_to(cr, x + w - 1.5, y + h - 1);
    cairo_stroke(cr);
    /* the far edge (screen side) */
    double far = dir == BTD_TAB_UP ? y + h - 0.5 : y + 0.5;
    btd_set_color(cr, dir == BTD_TAB_UP ? &p->black : &p->light);
    cairo_move_to(cr, x, far);
    cairo_line_to(cr, x + w, far);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void
btd_draw_tile(cairo_t *cr, const BtdPalette *p, double x, double y, double size, gboolean active, gboolean pressed,
              gboolean hover)
{
    cairo_save(cr);
    if (p->style == BTD_STYLE_MODERN) {
        if (active || pressed || hover) {
            rounded_rect(cr, x + 2, y + 2, size - 4, size - 4, 8, TRUE, TRUE);
            const BtdRGBA *c = (active || pressed) ? &p->tile_active_a : &p->tile_active_b;
            btd_set_color(cr, c);
            cairo_fill(cr);
        }
        cairo_restore(cr);
        return;
    }
    /* Window Maker tile: diagonal gradient with a one-pixel raised bevel. */
    const BtdRGBA *a = active ? &p->tile_active_a : &p->tile_a;
    const BtdRGBA *b = active ? &p->tile_active_b : &p->tile_b;
    cairo_pattern_t *pat = cairo_pattern_create_linear(x, y, x + size, y + size);
    cairo_pattern_add_color_stop_rgb(pat, pressed ? 1 : 0, a->r, a->g, a->b);
    cairo_pattern_add_color_stop_rgb(pat, pressed ? 0 : 1, b->r, b->g, b->b);
    cairo_rectangle(cr, x, y, size, size);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
    cairo_set_line_width(cr, 1);
    btd_set_color(cr, pressed ? &p->black : &p->light);
    cairo_move_to(cr, x + 0.5, y + size - 0.5);
    cairo_line_to(cr, x + 0.5, y + 0.5);
    cairo_line_to(cr, x + size - 0.5, y + 0.5);
    cairo_stroke(cr);
    btd_set_color(cr, pressed ? &p->light : &p->black);
    cairo_move_to(cr, x + size - 0.5, y + 0.5);
    cairo_line_to(cr, x + size - 0.5, y + size - 0.5);
    cairo_line_to(cr, x + 0.5, y + size - 0.5);
    cairo_stroke(cr);
    if (active) {
        /* focused application: a thin black inner frame, like a selected icon */
        btd_set_color(cr, &p->black);
        cairo_rectangle(cr, x + 2.5, y + 2.5, size - 5, size - 5);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void
btd_draw_groove(cairo_t *cr, const BtdPalette *p, double x, double y, double h)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    btd_set_color(cr, &p->dark);
    cairo_move_to(cr, round(x) + 0.5, y);
    cairo_line_to(cr, round(x) + 0.5, y + h);
    cairo_stroke(cr);
    btd_set_color(cr, &p->light);
    cairo_move_to(cr, round(x) + 1.5, y);
    cairo_line_to(cr, round(x) + 1.5, y + h);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void
btd_draw_running(cairo_t *cr, const BtdPalette *p, double cx, double y, int windows, gboolean minimized)
{
    if (windows <= 0)
        return;
    int n = MIN(windows, 3);
    cairo_save(cr);
    if (p->style == BTD_STYLE_MODERN) {
        for (int i = 0; i < n; i++) {
            double dx = cx + (i - (n - 1) / 2.0) * 7;
            cairo_arc(cr, dx, y + 2, 2, 0, 2 * G_PI);
            btd_set_color(cr, minimized ? &p->text_inactive : &p->accent);
            cairo_fill(cr);
        }
        cairo_restore(cr);
        return;
    }
    /* Classic: small raised 3D studs, one per window (max three). A minimised
     * application shows hollow (sunken) studs. */
    for (int i = 0; i < n; i++) {
        double dx = round(cx + (i - (n - 1) / 2.0) * 8 - 2.5);
        double dy = round(y);
        cairo_rectangle(cr, dx, dy, 5, 4);
        btd_set_color(cr, minimized ? &p->mid : &p->black);
        cairo_fill(cr);
        cairo_set_line_width(cr, 1);
        btd_set_color(cr, minimized ? &p->dark : &p->light);
        cairo_move_to(cr, dx + 0.5, dy + 4);
        cairo_line_to(cr, dx + 0.5, dy + 0.5);
        cairo_line_to(cr, dx + 5, dy + 0.5);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void
btd_draw_drop_marker(cairo_t *cr, const BtdPalette *p, double x, double y, double h)
{
    cairo_save(cr);
    cairo_rectangle(cr, round(x) - 1, y, 3, h);
    btd_set_color(cr, p->style == BTD_STYLE_MODERN ? &p->accent : &p->black);
    cairo_fill(cr);
    cairo_rectangle(cr, round(x) - 2, y, 1, h);
    btd_set_color(cr, &p->light);
    cairo_fill(cr);
    /* small arrow heads */
    cairo_move_to(cr, round(x) - 4, y);
    cairo_line_to(cr, round(x) + 5, y);
    cairo_line_to(cr, round(x) + 0.5, y + 5);
    cairo_close_path(cr);
    btd_set_color(cr, p->style == BTD_STYLE_MODERN ? &p->accent : &p->black);
    cairo_fill(cr);
    cairo_restore(cr);
}

void
btd_draw_engraved_text(cairo_t *cr, const BtdPalette *p, double x, double y, PangoLayout *layout)
{
    cairo_save(cr);
    if (p->style != BTD_STYLE_MODERN) {
        /* engraved on light gray, embossed (black shadow) on dark gray */
        btd_set_color(cr, p->dark_mode ? &p->black : &p->light);
        cairo_move_to(cr, round(x) + 1, round(y) + 1);
        pango_cairo_show_layout(cr, layout);
    }
    btd_set_color(cr, &p->text_hint);
    cairo_move_to(cr, round(x), round(y));
    pango_cairo_show_layout(cr, layout);
    cairo_restore(cr);
}

void
btd_draw_shadow(cairo_t *cr, double x, double y, double w, double size, BtdTabDirection dir)
{
    /* Discreet shadow cast on the desktop by the shelf edge (composited only). */
    cairo_save(cr);
    double y0 = dir == BTD_TAB_UP ? y : y + size;
    double y1 = dir == BTD_TAB_UP ? y + size : y;
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, y1, 0, y0);
    cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0.28);
    cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0);
    cairo_rectangle(cr, x, MIN(y0, y1), w, size);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
    cairo_restore(cr);
}

/* ---- OPENSTEP 4.0 beta shelf -------------------------------------------- */

static double
hash01(int x, int y, guint seed)
{
    guint h = (guint)x * 374761393u + (guint)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0;
}

static double
value_noise(double x, double y, int period, guint seed)
{
    int x0 = (int)floor(x), y0 = (int)floor(y);
    double fx = x - x0, fy = y - y0;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    int x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    x0 %= period;
    y0 %= period;
    double a = hash01(x0, y0, seed), b = hash01(x1, y0, seed), c = hash01(x0, y1, seed), d = hash01(x1, y1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

cairo_pattern_t *
btd_marble_pattern(double r, double g, double b, double amplitude, guint seed)
{
    /* Seamless procedural marble: tileable value noise in three octaves plus
     * thin bright veins. Generated once per palette (256×256). */
    const int n = 256;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, n, n);
    unsigned char *data = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s);
    cairo_surface_flush(s);
    for (int y = 0; y < n; y++) {
        guint32 *row = (guint32 *)(data + y * stride);
        for (int x = 0; x < n; x++) {
            double v = 0, amp = 1, norm = 0;
            for (int o = 0; o < 3; o++) {
                int period = 6 << o;
                v += amp * value_noise(x * period / (double)n, y * period / (double)n, period, seed + (guint)o);
                norm += amp;
                amp *= 0.5;
            }
            v = v / norm - 0.5;
            /* irregular veins: where a second, domain-warped noise crosses
             * its middle value; plus a fine grain for the mottled look */
            double wx = x * 3.0 / n + value_noise(x * 5.0 / n, y * 5.0 / n, 5, seed + 11) * 0.8;
            double wy = y * 3.0 / n + value_noise(x * 5.0 / n, y * 5.0 / n, 5, seed + 13) * 0.8;
            double vn = 0.65 * value_noise(wx, wy, 3, seed + 17) + 0.35 * value_noise(wx * 2, wy * 2, 6, seed + 19);
            double d = fabs(vn - 0.5);
            double light = d < 0.012 ? (0.012 - d) / 0.012 * 0.035 : 0;
            double grain = (hash01(x, y, seed + 23) - 0.5) * 0.035;
            double k = v * amplitude * 2 + light + grain;
            int R = CLAMP((int)((r + k) * 255), 0, 255), G = CLAMP((int)((g + k) * 255), 0, 255),
                B = CLAMP((int)((b + k) * 255), 0, 255);
            row[x] = ((guint32)R << 16) | ((guint32)G << 8) | (guint32)B;
        }
    }
    cairo_surface_mark_dirty(s);
    cairo_pattern_t *pat = cairo_pattern_create_for_surface(s);
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
    cairo_surface_destroy(s);
    return pat;
}

void
btd_beta_tab_path(cairo_t *cr, double x, double y, double w, double h, BtdTabDirection dir, gboolean slant_left,
                  gboolean slant_right)
{
    double s = MIN(BTD_BETA_SLANT * h / 28.0, w / 4), r = 4;
    double base = dir == BTD_TAB_UP ? y + h : y, top = dir == BTD_TAB_UP ? y : y + h;
    double sgn = dir == BTD_TAB_UP ? 1 : -1;
    double lt = slant_left ? x + s : x, rt = slant_right ? x + w - s : x + w;
    double fl = slant_left ? s / h : 0, fr = slant_right ? s / h : 0; /* x per y along the edges */
    cairo_new_path(cr);
    cairo_move_to(cr, x, base);
    cairo_line_to(cr, lt - fl * r, top + sgn * r);
    cairo_curve_to(cr, lt, top, lt, top, lt + r, top);
    cairo_line_to(cr, rt - r, top);
    cairo_curve_to(cr, rt, top, rt, top, rt + fr * r, top + sgn * r);
    cairo_line_to(cr, x + w, base);
    cairo_close_path(cr);
}

void
btd_draw_beta_tab(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean active,
                  gboolean highlighted, BtdTabDirection dir, gboolean slant_left, gboolean slant_right,
                  PangoLayout *label)
{
    cairo_save(cr);
    double s = MIN(BTD_BETA_SLANT * h / 28.0, w / 4), r = 4;
    double base = dir == BTD_TAB_UP ? y + h : y, top = dir == BTD_TAB_UP ? y + 0.5 : y + h - 0.5;
    double sgn = dir == BTD_TAB_UP ? 1 : -1;
    btd_beta_tab_path(cr, x, y, w, h, dir, slant_left, slant_right);
    cairo_set_source(cr, active ? p->marble : p->teal_marble);
    cairo_fill_preserve(cr);
    if (highlighted) {
        cairo_set_source_rgba(cr, 1, 1, 1, 0.12);
        cairo_fill_preserve(cr);
    }
    cairo_new_path(cr);
    cairo_set_line_width(cr, 1.2);
    double lt = slant_left ? x + s : x, rt = slant_right ? x + w - s : x + w;
    /* lit left edge and top */
    btd_set_color(cr, active ? &p->light : &p->teal_edge);
    cairo_move_to(cr, x + 0.6, base);
    cairo_line_to(cr, lt + 0.6 - (slant_left ? s / h * r : 0), top + sgn * r);
    cairo_curve_to(cr, lt + 0.6, top, lt + 0.6, top, lt + r, top);
    cairo_stroke(cr);
    btd_set_color(cr, active ? &p->light : &p->teal_top);
    cairo_move_to(cr, lt + r, top);
    cairo_line_to(cr, rt - r, top);
    cairo_stroke(cr);
    /* dark right edge */
    btd_set_color(cr, &p->edge_dark);
    cairo_set_line_width(cr, 1.6);
    cairo_move_to(cr, rt - r, top);
    cairo_curve_to(cr, rt - 0.6, top, rt - 0.6, top, rt - 0.6 + (slant_right ? s / h * r : 0), top + sgn * r);
    cairo_line_to(cr, x + w - 0.6, base);
    cairo_stroke(cr);
    if (label) {
        int lw, lh;
        pango_layout_get_pixel_size(label, &lw, &lh);
        btd_set_color(cr, &p->text);
        cairo_move_to(cr, round(x + (w - lw) / 2.0), round(y + (h - lh) / 2.0 + (dir == BTD_TAB_UP ? 1 : -1)));
        pango_cairo_show_layout(cr, label);
    }
    cairo_restore(cr);
}

void
btd_draw_beta_shelf(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, double gap_x,
                    double gap_w, BtdTabDirection dir, gboolean edges)
{
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source(cr, p->marble);
    cairo_fill(cr);
    cairo_set_line_width(cr, 1);
    /* light line where the tabs meet the body, open under the selected tab */
    double edge = dir == BTD_TAB_UP ? y + 0.5 : y + h - 0.5;
    btd_set_color(cr, &p->body_top);
    cairo_move_to(cr, x, edge);
    cairo_line_to(cr, gap_x + 1, edge);
    cairo_move_to(cr, gap_x + gap_w - 1, edge);
    cairo_line_to(cr, x + w, edge);
    cairo_stroke(cr);
    double far = dir == BTD_TAB_UP ? y + h - 0.5 : y + 0.5;
    btd_set_color(cr, &p->body_bottom);
    cairo_move_to(cr, x, far);
    cairo_line_to(cr, x + w, far);
    cairo_stroke(cr);
    if (edges) { /* not edge to edge: close the sides like a raised panel */
        btd_set_color(cr, &p->light);
        cairo_move_to(cr, x + 0.5, y);
        cairo_line_to(cr, x + 0.5, y + h);
        cairo_stroke(cr);
        btd_set_color(cr, &p->edge_dark);
        cairo_move_to(cr, x + w - 0.5, y);
        cairo_line_to(cr, x + w - 0.5, y + h);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void
btd_draw_arrow_glyph(cairo_t *cr, const BtdPalette *p, double cx, double cy, double size, gboolean pointing_right)
{
    cairo_save(cr);
    cairo_translate(cr, round(cx), round(cy));
    if (!pointing_right)
        cairo_scale(cr, -1, 1);
    double u = size / 12.0;
    cairo_move_to(cr, -5 * u, -2 * u);
    cairo_line_to(cr, 0, -2 * u);
    cairo_line_to(cr, 0, -5 * u);
    cairo_line_to(cr, 5.5 * u, 0);
    cairo_line_to(cr, 0, 5 * u);
    cairo_line_to(cr, 0, 2 * u);
    cairo_line_to(cr, -5 * u, 2 * u);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.86, 0.92, 0.93, 1);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.2);
    btd_set_color(cr, &p->edge_dark);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void
btd_draw_document_glyph(cairo_t *cr, const BtdPalette *p, double cx, double cy, double size)
{
    cairo_save(cr);
    double w = round(size * 0.8), h = round(size);
    double x = round(cx - w / 2), y = round(cy - h / 2);
    cairo_rectangle(cr, x + 2, y + 2, w, h); /* drop shadow */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    cairo_fill(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.97, 0.97, 0.98);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1);
    btd_set_color(cr, &p->edge_dark);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.55, 0.35, 0.75); /* a few "text" lines */
    for (int i = 0; i < 3; i++) {
        cairo_rectangle(cr, x + 2, y + 2.5 + i * (h - 4) / 3.0, w - 4, 1.4);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

void
btd_draw_selection_well(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h)
{
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.10);
    cairo_fill(cr);
    cairo_set_line_width(cr, 1);
    btd_set_color(cr, &p->dark);
    cairo_move_to(cr, x + 0.5, y + h);
    cairo_line_to(cr, x + 0.5, y + 0.5);
    cairo_line_to(cr, x + w, y + 0.5);
    cairo_stroke(cr);
    btd_set_color(cr, &p->light);
    cairo_move_to(cr, x + w - 0.5, y + 1);
    cairo_line_to(cr, x + w - 0.5, y + h - 0.5);
    cairo_line_to(cr, x + 1, y + h - 0.5);
    cairo_stroke(cr);
    cairo_restore(cr);
}
