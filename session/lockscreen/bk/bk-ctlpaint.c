/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Desenho do controle na tela, nos estilos de cada layout (bk-controls.h,
 * BkCtlLook). O mesmo código desenha o controle de verdade e a prévia das
 * configurações.
 *
 *   classic  relevo OPENSTEP cinza (o visual de sempre)
 *   modern   vidro escuro, contorno fino, rótulo na cor da marca
 *   retro    cores fortes, contorno grosso e sombra, como num 16 bits
 *   neon     fundo escuro e contorno luminoso
 *   minimal  só o contorno
 *
 * O analógico tem um pino que segue o dedo (o flutuante leva a base junto);
 * o direcional acende o lado apertado (cruz, quatro setas ou disco). A
 * animação de toque: afundar, brilho, onda (um anel que se abre, 320 ms),
 * encolher, ou nenhuma.
 *
 * Só cairo, Pango e GLib (sem GTK): a tela de bloqueio do big-wm copia este
 * arquivo e o bk-controls para mostrar o controle na tela por cima do
 * bloqueio (session/lockscreen/bk/ no big-wm).
 */
#include "bk-ctlpaint.h"

#include <linux/input-event-codes.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <string.h>

#define RIPPLE_US 320000

typedef struct {
    const BkCtlLayout *l;
    double accent[3];
    gboolean classic, modern, retro, neon, minimal;
} Look;

static void
look_of(const BkCtlLayout *l, Look *k)
{
    k->l = l;
    bk_ctl_accent(l, k->accent);
    const char *s = l->look.style;
    k->modern = !strcmp(s, "modern");
    k->retro = !strcmp(s, "retro");
    k->neon = !strcmp(s, "neon");
    k->minimal = !strcmp(s, "minimal");
    k->classic = !(k->modern || k->retro || k->neon || k->minimal);
}

static gboolean
anim(const Look *k, const char *a)
{
    return !strcmp(k->l->look.anim, a);
}

static void
text_at(cairo_t *cr, const char *t, double cx, double cy, double fs, gboolean mono, double r, double g, double b,
        double a)
{
    PangoLayout *pl = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(
        mono ? "DejaVu Sans Mono, Liberation Mono, Monospace Bold" : "Helvetica, Nimbus Sans, DejaVu Sans, Sans Bold");
    pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
    pango_font_description_set_absolute_size(fd, fs * PANGO_SCALE);
    pango_layout_set_font_description(pl, fd);
    pango_layout_set_text(pl, t, -1);
    int w, h;
    pango_layout_get_pixel_size(pl, &w, &h);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_move_to(cr, round(cx - w / 2.0), round(cy - h / 2.0));
    pango_cairo_show_layout(cr, pl);
    pango_font_description_free(fd);
    g_object_unref(pl);
}

/* Anéis cada vez mais largos e transparentes: o brilho do neon. */
static void
glow(cairo_t *cr, double cx, double cy, double r, const double c[3], double a)
{
    static const double width[] = { 0.34, 0.20, 0.10, 0.05 }, alpha[] = { 0.08, 0.16, 0.35, 0.95 };
    for (int i = 0; i < 4; i++) {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_set_line_width(cr, MAX(1.0, r * width[i]));
        cairo_set_source_rgba(cr, c[0], c[1], c[2], a * alpha[i]);
        cairo_stroke(cr);
    }
}

/* Um disco no estilo do layout; brand: cor de marca (NULL: sem). */
static void
face(cairo_t *cr, const Look *k, double cx, double cy, double r, gboolean down, const double *brand, double a)
{
    cairo_new_path(cr);
    if (k->classic) {
        /* relevo OPENSTEP: luz em cima à esquerda, sombra embaixo */
        cairo_pattern_t *g = cairo_pattern_create_linear(cx - r, cy - r, cx + r, cy + r);
        double top = down ? 0.45 : 0.86, bot = down ? 0.70 : 0.42;
        cairo_pattern_add_color_stop_rgba(g, 0, top, top, top, a);
        cairo_pattern_add_color_stop_rgba(g, 1, bot, bot, bot, a);
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        cairo_set_line_width(cr, MAX(1.5, r * 0.06));
        cairo_set_source_rgba(cr, 0, 0, 0, MIN(1, a + 0.2));
        cairo_stroke(cr);
    } else if (k->modern) {
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        if (down)
            cairo_set_source_rgba(cr, k->accent[0], k->accent[1], k->accent[2], MIN(1, a + 0.2));
        else
            cairo_set_source_rgba(cr, 0.07, 0.07, 0.09, a * 0.75);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, MAX(1.5, r * 0.07));
        if (brand)
            cairo_set_source_rgba(cr, brand[0], brand[1], brand[2], MIN(1, a + 0.3));
        else
            cairo_set_source_rgba(cr, 1, 1, 1, MIN(1, a + 0.1));
        cairo_stroke(cr);
    } else if (k->retro) {
        double sh = r * 0.14, dy = down ? sh : 0;
        if (!down) { /* sombra: o botão "sobe"; apertado, afunda sobre ela */
            cairo_arc(cr, cx, cy + sh, r, 0, 2 * M_PI);
            cairo_set_source_rgba(cr, 0, 0, 0, a * 0.55);
            cairo_fill(cr);
        }
        cairo_arc(cr, cx, cy + dy, r, 0, 2 * M_PI);
        if (brand)
            cairo_set_source_rgba(cr, brand[0] * (down ? 0.75 : 1), brand[1] * (down ? 0.75 : 1),
                                  brand[2] * (down ? 0.75 : 1), MIN(1, a + 0.35));
        else
            cairo_set_source_rgba(cr, down ? 0.62 : 0.80, down ? 0.62 : 0.80, down ? 0.66 : 0.84, MIN(1, a + 0.35));
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, MAX(2.0, r * 0.11));
        cairo_set_source_rgba(cr, 0.10, 0.08, 0.14, MIN(1, a + 0.4));
        cairo_stroke(cr);
    } else if (k->neon) {
        const double *c = brand ? brand : k->accent;
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, down ? c[0] * 0.5 : 0.02, down ? c[1] * 0.5 : 0.02, down ? c[2] * 0.5 : 0.05,
                              a * 0.7);
        cairo_fill(cr);
        glow(cr, cx, cy, r * 0.94, c, MIN(1, a + (down ? 0.45 : 0.25)));
    } else { /* minimal */
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        if (down) {
            cairo_set_source_rgba(cr, 1, 1, 1, a * 0.35);
            cairo_fill_preserve(cr);
        }
        cairo_set_line_width(cr, MAX(1.5, r * 0.05));
        cairo_set_source_rgba(cr, 1, 1, 1, MIN(1, a + 0.15));
        cairo_stroke(cr);
    }
}

/* Cor do rótulo sobre o disco. */
static void
ink(const Look *k, const double *brand, double c[3])
{
    if (k->classic) {
        c[0] = c[1] = c[2] = 0.05;
    } else if (k->retro) {
        c[0] = c[1] = c[2] = brand ? 1.0 : 0.12;
    } else if ((k->modern || k->neon) && brand) {
        memcpy(c, brand, sizeof(double) * 3);
    } else if (k->neon) {
        memcpy(c, k->accent, sizeof(double) * 3);
    } else {
        c[0] = c[1] = c[2] = 1.0;
    }
}

static void
label_on(cairo_t *cr, const Look *k, const char *t, double cx, double cy, double fs, const double *brand,
         gboolean down, double a)
{
    (void)down;
    double c[3];
    ink(k, brand, c);
    text_at(cr, t, cx, cy, fs, k->retro, c[0], c[1], c[2], MIN(1, a + 0.4));
}

/* O símbolo do botão Guia, em vetor (fontes variam; ◉ e ⌂ nem sempre
 * existem): Xbox, uma esfera cortada por um X curvo; PlayStation, "PS";
 * Nintendo, a casinha do botão Home. */
static void
guide_glyph(cairo_t *cr, const Look *k, double cx, double cy, double r, double a)
{
    double c[3];
    ink(k, NULL, c);
    a = MIN(1, a + 0.4);
    const char *f = k->l->look.buttons;
    cairo_save(cr);
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], a);
    if (!strcmp(f, "playstation")) {
        text_at(cr, "PS", cx, cy, MAX(11, r * 0.78), k->retro, c[0], c[1], c[2], a);
    } else if (!strcmp(f, "nintendo")) {
        double s = r * 0.52;
        cairo_move_to(cr, cx, cy - s);              /* cumeeira */
        cairo_line_to(cr, cx + s, cy - s * 0.05);   /* beiral direito */
        cairo_line_to(cr, cx + s * 0.68, cy - s * 0.05);
        cairo_line_to(cr, cx + s * 0.68, cy + s * 0.85);
        cairo_line_to(cr, cx - s * 0.68, cy + s * 0.85);
        cairo_line_to(cr, cx - s * 0.68, cy - s * 0.05);
        cairo_line_to(cr, cx - s, cy - s * 0.05);
        cairo_close_path(cr);
        cairo_rectangle(cr, cx - s * 0.2, cy + s * 0.85, s * 0.4, -s * 0.5); /* porta (vazada) */
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill(cr);
    } else {
        double s = r * 0.56;
        cairo_set_line_width(cr, MAX(1.5, r * 0.11));
        cairo_arc(cr, cx, cy, s, 0, 2 * M_PI);
        cairo_stroke(cr);
        /* o X: dois traços que se curvam para dentro, de borda a borda */
        cairo_set_line_width(cr, MAX(1.5, r * 0.15));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        double e = s * 0.70, m = s * 0.18;
        cairo_move_to(cr, cx - e, cy - e);
        cairo_curve_to(cr, cx - m, cy - m * 0.2, cx + m, cy + m * 0.2, cx + e, cy + e);
        cairo_move_to(cr, cx + e, cy - e);
        cairo_curve_to(cr, cx + m, cy - m * 0.2, cx - m, cy + m * 0.2, cx - e, cy + e);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

static void
triangle(cairo_t *cr, double cx, double cy, double s, int dx, int dy)
{
    /* ponta para (dx, dy) */
    double px = -dy, py = dx;
    cairo_move_to(cr, cx + dx * s, cy + dy * s);
    cairo_line_to(cr, cx - dx * s * 0.6 + px * s * 0.8, cy - dy * s * 0.6 + py * s * 0.8);
    cairo_line_to(cr, cx - dx * s * 0.6 - px * s * 0.8, cy - dy * s * 0.6 - py * s * 0.8);
    cairo_close_path(cr);
}

static void
draw_dpad(cairo_t *cr, const Look *k, double cx, double cy, double r, int hx, int hy, double a)
{
    const char *st = k->l->look.dpad;
    double base[3], lit[3];
    memcpy(lit, k->accent, sizeof lit);
    if (k->classic)
        base[0] = base[1] = base[2] = 0.30;
    else if (k->retro)
        base[0] = base[1] = base[2] = 0.14;
    else
        base[0] = base[1] = base[2] = 1.0;
    static const int dirs[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    if (!strcmp(st, "buttons")) {
        for (int i = 0; i < 4; i++) {
            int on = (dirs[i][0] && dirs[i][0] == hx) || (dirs[i][1] && dirs[i][1] == hy);
            double bx = cx + dirs[i][0] * r * 0.58, by = cy + dirs[i][1] * r * 0.58, br = r * 0.30;
            face(cr, k, bx, by, br, on, on ? lit : NULL, a);
            cairo_new_path(cr);
            triangle(cr, bx, by, br * 0.45, dirs[i][0], dirs[i][1]);
            double c[3];
            ink(k, NULL, c);
            cairo_set_source_rgba(cr, c[0], c[1], c[2], MIN(1, a + 0.4));
            cairo_fill(cr);
        }
        return;
    }
    face(cr, k, cx, cy, r, FALSE, NULL, a);
    if (!strcmp(st, "disc")) {
        if (hx || hy) { /* uma fatia de 90° na direção apertada (diagonais incluídas) */
            double ang = atan2(hy, hx);
            cairo_new_path(cr);
            cairo_move_to(cr, cx, cy);
            cairo_arc(cr, cx, cy, r * 0.92, ang - M_PI / 4, ang + M_PI / 4);
            cairo_close_path(cr);
            cairo_set_source_rgba(cr, lit[0], lit[1], lit[2], MIN(1, a + 0.15));
            cairo_fill(cr);
        }
        for (int i = 0; i < 4; i++) {
            cairo_new_path(cr);
            triangle(cr, cx + dirs[i][0] * r * 0.68, cy + dirs[i][1] * r * 0.68, r * 0.13, dirs[i][0], dirs[i][1]);
            cairo_set_source_rgba(cr, base[0], base[1], base[2], MIN(1, a + 0.3));
            cairo_fill(cr);
        }
        return;
    }
    /* cruz */
    double arm = r * 0.80, th = r * 0.30;
    cairo_new_path(cr);
    cairo_rectangle(cr, cx - arm, cy - th, 2 * arm, 2 * th);
    cairo_rectangle(cr, cx - th, cy - arm, 2 * th, 2 * arm);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    if (k->neon || k->minimal) {
        cairo_set_line_width(cr, MAX(1.5, r * 0.05));
        cairo_set_source_rgba(cr, base[0], base[1], base[2], MIN(1, a + 0.2));
        cairo_stroke(cr);
    } else {
        double fa = k->modern ? a * 0.35 : MIN(1, a + 0.3); /* moderno: a cruz é um vidro claro */
        cairo_set_source_rgba(cr, base[0], base[1], base[2], fa);
        cairo_fill(cr);
    }
    for (int i = 0; i < 4; i++) {
        int on = (dirs[i][0] && dirs[i][0] == hx) || (dirs[i][1] && dirs[i][1] == hy);
        if (!on)
            continue;
        cairo_new_path(cr);
        double x0 = dirs[i][0] ? cx + (dirs[i][0] > 0 ? th : -arm) : cx - th;
        double y0 = dirs[i][1] ? cy + (dirs[i][1] > 0 ? th : -arm) : cy - th;
        double w0 = dirs[i][0] ? arm - th : 2 * th, h0 = dirs[i][1] ? arm - th : 2 * th;
        cairo_rectangle(cr, x0, y0, w0, h0);
        cairo_set_source_rgba(cr, lit[0], lit[1], lit[2], MIN(1, a + 0.3));
        cairo_fill(cr);
    }
}

void
bk_ctl_paint(cairo_t *cr, const BkCtlLayout *l, const BkCtlState *s, double w, double h, int i, gint64 now,
             gint64 pressed_at)
{
    const BkCtl *c = &l->c[i];
    Look k;
    look_of(l, &k);
    double d = bk_ctl_px(l, c, h), r = d / 2, cx = c->x * w, cy = c->y * h;
    gboolean held = bk_ctl_touch_of(s, i) >= 0;
    gboolean down = held && !anim(&k, "none");
    double a = l->opacity * (c->enabled ? 1 : 0.35);
    double brand[3];
    gboolean has_brand = bk_ctl_color(l, c, brand);
    if (held && anim(&k, "bounce") && c->kind != BKC_STICK)
        r *= 0.9;
    if (held && anim(&k, "glow"))
        glow(cr, cx, cy, r * 1.08, has_brand ? brand : k.accent, 0.8);
    const char *lab = bk_ctl_label_for(l, c);
    double fs = MAX(12, d * (c->kind == BKC_BUTTON ? 0.38 : 0.3));
    if (c->kind == BKC_STICK) {
        double bx, by, kx, ky;
        bk_ctl_stick_visual(l, s, w, h, i, &bx, &by, &kx, &ky);
        face(cr, &k, bx, by, r, FALSE, NULL, a * (k.minimal ? 1 : 0.85));
        if (k.classic) { /* a zona morta, como antes */
            cairo_new_path(cr);
            cairo_arc(cr, bx, by, r * c->deadzone, 0, 2 * M_PI);
            cairo_set_line_width(cr, MAX(1.5, d * 0.03));
            cairo_set_source_rgba(cr, 0, 0, 0, a * 0.5);
            cairo_stroke(cr);
        }
        double kr = r * 0.46, px = bx + kx * (r - kr), py = by + ky * (r - kr);
        if (held && anim(&k, "bounce"))
            kr *= 0.9;
        face(cr, &k, px, py, kr, down, held ? k.accent : NULL, MIN(1, a + 0.15));
        label_on(cr, &k, lab, px, py + (k.retro && down ? kr * 0.14 : 0), MAX(11, kr * 0.75), NULL, down, a);
    } else if (c->kind == BKC_DPAD) {
        int hx, hy;
        bk_ctl_dpad_visual(l, s, w, h, i, &hx, &hy);
        if (anim(&k, "none"))
            hx = hy = 0;
        draw_dpad(cr, &k, cx, cy, r, hx, hy, a);
    } else if (c->code == BTN_MODE) {
        /* aceso como num controle de verdade: um halo na cor de destaque */
        if (!k.classic && !k.retro && !(held && anim(&k, "glow")))
            glow(cr, cx, cy, r * 1.04, k.accent, a * 0.45);
        face(cr, &k, cx, cy, r, down, k.modern ? k.accent : NULL, a);
        double gy = cy + (k.retro && down ? r * 0.14 : 0) + (k.classic && down ? 1 : 0);
        guide_glyph(cr, &k, cx, gy, r, a);
    } else {
        face(cr, &k, cx, cy, r, down, has_brand ? brand : NULL, a);
        double ly = cy + (k.retro && down ? r * 0.14 : 0) + (k.classic && down ? 1 : 0);
        label_on(cr, &k, lab, cx, ly, fs, has_brand ? brand : NULL, down, a);
    }
    if (pressed_at && anim(&k, "ripple") && now - pressed_at < RIPPLE_US) {
        double t = (double)(now - pressed_at) / RIPPLE_US;
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, r * (1 + 0.6 * t), 0, 2 * M_PI);
        cairo_set_line_width(cr, MAX(2, r * 0.12 * (1 - t)));
        const double *rc = has_brand ? brand : k.accent;
        cairo_set_source_rgba(cr, rc[0], rc[1], rc[2], 0.85 * (1 - t));
        cairo_stroke(cr);
    }
}

gboolean
bk_ctl_paint_animating(const BkCtlLayout *l, gint64 now, gint64 pressed_at)
{
    return pressed_at && !strcmp(l->look.anim, "ripple") && now - pressed_at < RIPPLE_US;
}

void
bk_ctl_paint_switch(cairo_t *cr, const BkCtlLayout *l, double w, double h, gboolean down, BkCtlIconFn icon)
{
    Look k;
    look_of(l, &k);
    double cx, cy, r;
    bk_ctl_switch_button(w, h, &cx, &cy, &r);
    double a = MAX(0.35, l->opacity);
    face(cr, &k, cx, cy, r, down, NULL, a);
    double c[3];
    ink(&k, NULL, c);
    if (icon)
        icon(cr, cx, cy + (down && (k.classic || k.retro) ? (k.retro ? r * 0.14 : 1) : 0), r * 1.15,
                          c[0], c[1], c[2], MIN(1, a + 0.3));
}

void
bk_ctl_paint_layout(cairo_t *cr, const BkCtlLayout *l, const BkCtlState *s, double w, double h, BkCtlIconFn icon)
{
    for (int i = 0; i < l->n; i++)
        bk_ctl_paint(cr, l, s, w, h, i, 0, 0);
    if (icon)
        bk_ctl_paint_switch(cr, l, w, h, FALSE, icon);
}
