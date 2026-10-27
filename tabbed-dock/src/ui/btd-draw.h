/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Cairo drawing of the dock widgets: NeXT-style tabs, bevelled shelf and
 * Window Maker-style icon tiles. Original implementation (no GNUstep code):
 * it follows the documented OpenStep conventions — light from the top-left
 * (white edge), shadow at the bottom-right (black + dark gray), four grays
 * (white, #aaaaaa, #555555, black) — and the Big WindowMaker theme colours.
 */
#pragma once

#include <gtk/gtk.h>

#include "btd-config.h"

G_BEGIN_DECLS

typedef struct {
    double r, g, b, a;
} BtdRGBA;

typedef struct {
    BtdStyle style;
    gboolean dark_mode;              /* Big WindowMaker dark mode (big-wm-theme dark) */
    BtdRGBA shelf_top, shelf_bottom; /* shelf face gradient */
    BtdRGBA tab_active, tab_inactive;
    BtdRGBA tile_a, tile_b;          /* tile gradient (diagonal) */
    BtdRGBA tile_active_a, tile_active_b;
    BtdRGBA light, mid, dark, black;
    BtdRGBA text, text_inactive, text_hint;
    BtdRGBA accent;
    double radius;                   /* 0 for classic */
    /* OPENSTEP 4.0 beta shelf (classic) */
    BtdRGBA teal, teal_top, teal_edge, body_top, body_bottom, edge_dark;
    cairo_pattern_t *marble;         /* body and selected tab */
    cairo_pattern_t *teal_marble;    /* the other tabs */
} BtdPalette;

/* dark: the Big WindowMaker dark mode (~/.config/big-wm/theme), dark grays
 * with the same bevels; the modern style is dark already. */
void btd_palette_init(BtdPalette *p, BtdStyle style, gboolean dark);
void btd_palette_clear(BtdPalette *p);
void btd_set_color(cairo_t *cr, const BtdRGBA *c);

typedef enum { BTD_TAB_UP, BTD_TAB_DOWN } BtdTabDirection; /* tabs above (bottom dock) or below (top dock) */

#define BTD_TAB_SLOPE 14.0

/* Path of a NeXT-style tab: sloped S-curved sides. */
void btd_tab_path(cairo_t *cr, double x, double y, double w, double h, BtdTabDirection dir, double radius,
                  BtdStyle style);
void btd_draw_tab(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean active,
                  gboolean highlighted, BtdTabDirection dir, PangoLayout *label);

/* Raised (or sunken) bevelled rectangle, NSDrawButton-like. */
void btd_draw_bevel(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean sunken);
void btd_draw_shelf(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h,
                    double gap_x, double gap_w, BtdTabDirection dir);
void btd_draw_tile(cairo_t *cr, const BtdPalette *p, double x, double y, double size, gboolean active,
                   gboolean pressed, gboolean hover);
void btd_draw_groove(cairo_t *cr, const BtdPalette *p, double x, double y, double h);
/* windows: number of windows (0 = not running); minimized: all minimised. */
void btd_draw_running(cairo_t *cr, const BtdPalette *p, double cx, double y, int windows, gboolean minimized);
void btd_draw_drop_marker(cairo_t *cr, const BtdPalette *p, double x, double y, double h);
void btd_draw_engraved_text(cairo_t *cr, const BtdPalette *p, double x, double y, PangoLayout *layout);
void btd_draw_shadow(cairo_t *cr, double x, double y, double w, double size, BtdTabDirection dir);

PangoFontDescription *btd_font_new(gboolean bold, double px);

/* OPENSTEP 4.0 beta look (style classic): marble textures, trapezoid tabs
 * that are wider at the base, with rounded top corners, a lit left edge and a
 * dark right edge; the gaps between them show the desktop. */
#define BTD_BETA_SLANT 10.0
cairo_pattern_t *btd_marble_pattern(double r, double g, double b, double amplitude, guint seed);
void btd_beta_tab_path(cairo_t *cr, double x, double y, double w, double h, BtdTabDirection dir,
                       gboolean slant_left, gboolean slant_right);
void btd_draw_beta_tab(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, gboolean active,
                       gboolean highlighted, BtdTabDirection dir, gboolean slant_left, gboolean slant_right,
                       PangoLayout *label);
void btd_draw_beta_shelf(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h, double gap_x,
                         double gap_w, BtdTabDirection dir, gboolean edges);
/* glyphs of the two end buttons: an outlined arrow and a small document */
void btd_draw_arrow_glyph(cairo_t *cr, const BtdPalette *p, double cx, double cy, double size, gboolean pointing_right);
void btd_draw_document_glyph(cairo_t *cr, const BtdPalette *p, double cx, double cy, double size);
/* sunken frame behind the icon of the focused application (no tiles) */
void btd_draw_selection_well(cairo_t *cr, const BtdPalette *p, double x, double y, double w, double h);

G_END_DECLS
