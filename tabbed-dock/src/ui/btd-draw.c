/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-draw.h"

#include <math.h>

static BtdRGBA
hex(guint32 rgb)
{
    return (BtdRGBA){ ((rgb >> 16) & 0xff) / 255.0, ((rgb >> 8) & 0xff) / 255.0, (rgb & 0xff) / 255.0, 1.0 };
}

void
btd_palette_init(BtdPalette *p, BtdStyle style)
{
    p->style = style;
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
    if (p->style == BTD_STYLE_CLASSIC) {
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
    if (p->style == BTD_STYLE_CLASSIC) {
        btd_set_color(cr, &p->light);
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
