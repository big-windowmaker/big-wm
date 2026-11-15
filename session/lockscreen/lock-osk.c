/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * lock-osk: veja lock-osk.h. O desenho das teclas segue o draw_key() do
 * big-keyboard (src/ui/bk-osk.c): rosto cinza, chanfro do OPENSTEP com a
 * linha interna escura, símbolo do Shift no canto de cima, do AltGr no de
 * baixo, LED verde nos modificadores travados e no Caps.
 */
#define _GNU_SOURCE
#include "lock-osk.h"

#include <math.h>
#include <pango/pangocairo.h>
#include <string.h>

/* ---- layouts --------------------------------------------------------------- */

static int
layout_index(const char *id)
{
    for (int i = 0; i < bk_layout_count(); i++)
        if (!strcmp(bk_layout_get(i)->id, id))
            return i;
    return 0;
}

int
lock_osk_layout_from_conf(const char *v)
{
    if (!v)
        return -1;
    if (!strcmp(v, "br") || !strcmp(v, "abnt2"))
        return layout_index("br");
    if (!strcmp(v, "us"))
        return layout_index("us");
    if (!strcmp(v, "us-intl") || !strcmp(v, "intl"))
        return layout_index("us-intl");
    return -1;
}

int
lock_osk_layout_auto(const char *xkb, const char *lang)
{
    if (xkb && *xkb) {
        /* o primeiro layout da lista (br,us...); a variante intl do us */
        if (!strncmp(xkb, "br", 2) && (xkb[2] == 0 || xkb[2] == ','))
            return layout_index("br");
        return layout_index("us");
    }
    return lang && !strncmp(lang, "pt", 2) ? layout_index("br") : layout_index("us");
}

/* ---- teclas de uma senha --------------------------------------------------- */

/* Fica no teclado da tela de bloqueio? */
static int
for_password(const BkKey *k)
{
    switch (k->kind) {
    case BK_KEY_CHAR:
    case BK_KEY_CAPS:
    case BK_KEY_LAYOUT:
    case BK_KEY_HIDE:
        return 1;
    case BK_KEY_SPECIAL:
        return !strcmp(k->keysym, "BackSpace") || !strcmp(k->keysym, "Return");
    case BK_KEY_MOD:
        return k->mod == BK_MOD_SHIFT || k->mod == BK_MOD_ALTGR;
    case BK_KEY_PANEL:
        return k->panel != BK_PANEL_FN;
    }
    return 0;
}

static void
relayout(LockOsk *k)
{
    if (k->st.panel == BK_PANEL_FN)
        k->st.panel = BK_PANEL_MAIN;
    const BkKey(*src)[BK_MAX_KEYS] = bk_panel_rows(bk_layout_get(k->st.layout), k->st.panel);
    memset(k->rows, 0, sizeof k->rows);
    int r = 0;
    for (int sr = 0; sr < BK_MAX_ROWS && src[sr][0].width > 0; sr++) {
        int c = 0;
        for (int sc = 0; src[sr][sc].width > 0 && c < BK_MAX_KEYS - 1; sc++)
            if (for_password(&src[sr][sc]))
                k->rows[r][c++] = src[sr][sc];
        if (c > 0)
            r++;
    }
    double s = k->scale;
    k->u = round(40 * s);
    k->gap = round(4 * s);
    k->pad = round(8 * s);
    k->title = round(20 * s);
    bk_geom_build(&k->geom, (const BkKey(*)[BK_MAX_KEYS])k->rows, k->u, k->gap);
    k->width = k->geom.width + 2 * k->pad;
    k->height = k->title + k->geom.height + 2 * k->pad;
    if (k->cursor >= k->geom.n)
        k->cursor = k->geom.n - 1;
    k->point[0] = k->point[1] = -1;
    k->pressed = k->flash = -1;
}

void
lock_osk_init(LockOsk *k, int layout, int dark, double scale)
{
    memset(k, 0, sizeof *k);
    bk_state_init(&k->st, layout);
    k->dark = dark;
    k->scale = scale > 0 ? scale : 1;
    k->cursor = -1;
    relayout(k);
}

void
lock_osk_clear(LockOsk *k)
{
    bk_state_release_all(&k->st);
    explicit_bzero(k->st.dead, sizeof k->st.dead);
    k->st.caps = 0;
    k->pressed = k->flash = -1;
    k->repeated = 0;
}

/* ---- desenho --------------------------------------------------------------- */

static void
bevel(cairo_t *cr, double x, double y, double w, double h, int raised, int dark)
{
    double hi = dark ? 0.47 : 1.0, lo = dark ? 0.0 : 0.33;
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgb(cr, raised ? hi : lo, raised ? hi : lo, raised ? hi : lo);
    cairo_move_to(cr, x + 0.5, y + h - 0.5);
    cairo_line_to(cr, x + 0.5, y + 0.5);
    cairo_line_to(cr, x + w - 0.5, y + 0.5);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, raised ? 0 : hi, raised ? 0 : hi, raised ? 0 : hi);
    cairo_move_to(cr, x + w - 0.5, y + 0.5);
    cairo_line_to(cr, x + w - 0.5, y + h - 0.5);
    cairo_line_to(cr, x + 0.5, y + h - 0.5);
    cairo_stroke(cr);
    if (raised) { /* a linha interna escura do OPENSTEP */
        double m = dark ? 0.12 : 0.33;
        cairo_set_source_rgb(cr, m, m, m);
        cairo_move_to(cr, x + w - 1.5, y + 1.5);
        cairo_line_to(cr, x + w - 1.5, y + h - 1.5);
        cairo_line_to(cr, x + 1.5, y + h - 1.5);
        cairo_stroke(cr);
    }
}

/* Texto com o Pango (emojis e símbolos com as fontes de reserva). */
static double
text(cairo_t *cr, const char *t, double cx, double cy, double fs, int bold, double r, double g, double b,
     int align /* -1 esquerda, 0 centro, 1 direita */)
{
    PangoLayout *l = pango_cairo_create_layout(cr);
    PangoFontDescription *fd =
        pango_font_description_from_string("Helvetica, Nimbus Sans, Liberation Sans, DejaVu Sans, Sans");
    pango_font_description_set_absolute_size(fd, fs * PANGO_SCALE);
    pango_font_description_set_weight(fd, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(l, fd);
    pango_layout_set_text(l, t, -1);
    int w, h;
    pango_layout_get_pixel_size(l, &w, &h);
    double x = align < 0 ? cx : align > 0 ? cx - w : cx - w / 2.0;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, round(x), round(cy - h / 2.0));
    pango_cairo_show_layout(cr, l);
    pango_font_description_free(fd);
    g_object_unref(l);
    return w;
}

static int
is_letter(const char *t)
{
    if (!t || !t[0])
        return 0;
    unsigned char c = (unsigned char)t[0];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return t[1] == 0;
    return !strcmp(t, "ç") || !strcmp(t, "Ç");
}

static void
draw_key(LockOsk *k, cairo_t *cr, double ox, double oy, int i, int down)
{
    const BkKey *key = k->geom.key[i];
    const BkRect *r = &k->geom.rect[i];
    int dark = k->dark;
    double x = ox + r->x, y = oy + r->y, w = r->w, h = r->h;
    unsigned mods = bk_state_mods(&k->st);
    int latched = key->kind == BK_KEY_MOD && (k->st.latched & key->mod);
    int locked = (key->kind == BK_KEY_MOD && (k->st.locked & key->mod)) || (key->kind == BK_KEY_CAPS && k->st.caps);
    int panel_on = key->kind == BK_KEY_PANEL && k->st.panel == (BkPanel)key->panel;
    int space = key->kind == BK_KEY_CHAR && !strcmp(key->level[0], " ");
    down = down || latched || locked || panel_on;
    int special = key->kind != BK_KEY_CHAR || space;
    double face = dark ? (down ? 0.17 : special ? 0.24 : 0.29) : (down ? 0.56 : special ? 0.66 : 0.75);
    cairo_set_source_rgb(cr, face, face, face);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    bevel(cr, x, y, w, h, !down, dark);
    double ink = dark ? 0.95 : 0.0, fs = k->u * 0.36;
    const char *label = bk_state_label(&k->st, key);
    if (space)
        label = bk_layout_get(k->st.layout)->name; /* a barra de espaço mostra o layout, discreta */
    if (label && *label) {
        int small = g_utf8_strlen(label, -1) > 2 || space;
        double c = space ? (dark ? 0.6 : 0.35) : ink;
        text(cr, label, x + w / 2, y + h / 2 + (down ? 1 : 0), small ? fs * 0.72 : fs, 0, c, c, c, 0);
    }
    /* símbolos do Shift (canto de cima) e do AltGr (canto de baixo) */
    if (key->kind == BK_KEY_CHAR && key->level[1] && !(mods & BK_MOD_SHIFT) && !is_letter(key->level[0])) {
        double c = dark ? 0.7 : 0.25;
        text(cr, key->level[1], x + 4 * k->scale, y + fs * 0.45 + 1, fs * 0.5, 0, c, c, c, -1);
    }
    if (key->kind == BK_KEY_CHAR && key->level[2] && !(mods & BK_MOD_ALTGR))
        text(cr, key->level[2], x + w - 4 * k->scale, y + h - fs * 0.45 - 1, fs * 0.5, 0, dark ? 0.45 : 0.05,
             dark ? 0.7 : 0.25, dark ? 0.95 : 0.55, 1);
    if (locked) { /* LED verde: travado (dois toques) ou Caps */
        double led = fmax(4, k->u * 0.12);
        cairo_set_source_rgb(cr, 0.19, 0.82, 0.19);
        cairo_rectangle(cr, x + 5 * k->scale, y + 5 * k->scale, led, led);
        cairo_fill(cr);
    }
}

static void
draw_cursor(LockOsk *k, cairo_t *cr, double ox, double oy, int i, double r, double g, double b)
{
    if (i < 0 || i >= k->geom.n)
        return;
    const BkRect *rc = &k->geom.rect[i];
    double lw = fmax(2.5, k->u * 0.08);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_set_line_width(cr, lw);
    cairo_rectangle(cr, ox + rc->x + lw / 2, oy + rc->y + lw / 2, rc->w - lw, rc->h - lw);
    cairo_stroke(cr);
}

void
lock_osk_draw(LockOsk *k, cairo_t *cr, double x, double y, const char *title, long long now)
{
    double W = k->width, H = k->height, th = k->title, s = k->scale;
    cairo_save(cr);
    /* sombra */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_rectangle(cr, x + 6 * s, y + 6 * s, W, H);
    cairo_fill(cr);
    /* barra de título preta do Window Maker: o nome e, à direita, o painel */
    cairo_pattern_t *g = cairo_pattern_create_linear(x, y, x, y + th);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.16, 0.16, 0.18);
    cairo_pattern_add_color_stop_rgb(g, 1, 0, 0, 0);
    cairo_rectangle(cr, x, y, W, th);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    bevel(cr, x, y, W, th, 1, 1);
    double fs = 11 * s;
    if (title)
        text(cr, title, x + 10 * s, y + th / 2, fs, 1, 1, 1, 1, -1);
    const char *panel = k->st.panel == BK_PANEL_SYMBOLS ? "☺" : bk_layout_get(k->st.layout)->name;
    text(cr, panel, x + W - 10 * s, y + th / 2, fs, 1, 1, 1, 1, 1);
    /* corpo */
    double body = k->dark ? 0.2 : 0.667;
    cairo_set_source_rgb(cr, body, body, body);
    cairo_rectangle(cr, x, y + th, W, H - th);
    cairo_fill(cr);
    bevel(cr, x, y + th, W, H - th, 1, k->dark);
    double ox = x + k->pad, oy = y + th + k->pad;
    int flash = k->flash >= 0 && now < k->flash_until ? k->flash : -1;
    for (int i = 0; i < k->geom.n; i++)
        draw_key(k, cr, ox, oy, i, i == k->pressed || i == flash);
    draw_cursor(k, cr, ox, oy, k->cursor, k->dark, k->dark, k->dark);
    draw_cursor(k, cr, ox, oy, k->point[0], 0.1, 0.45, 0.95); /* esquerdo: azul */
    draw_cursor(k, cr, ox, oy, k->point[1], 0.95, 0.55, 0.1); /* direito: laranja */
    cairo_restore(cr);
}

int
lock_osk_hit(const LockOsk *k, double x, double y)
{
    return bk_geom_hit(&k->geom, x - k->pad, y - k->title - k->pad);
}

/* ---- teclas ---------------------------------------------------------------- */

static int
press(LockOsk *k, const BkKey *key, const LockOskSink *sink, void *data)
{
    if (key->kind == BK_KEY_HIDE) {
        k->shown = 0;
        return LOCK_OSK_HIDE | LOCK_OSK_REDRAW;
    }
    int old_layout = k->st.layout;
    BkPanel old_panel = k->st.panel;
    BkOut out[BK_MAX_OUT];
    int n = bk_state_press(&k->st, key, out, BK_MAX_OUT);
    for (int i = 0; i < n; i++) {
        if (out[i].kind == BK_OUT_TEXT) {
            if (sink->text)
                sink->text(data, out[i].text);
        } else if (!strcmp(out[i].keysym, "BackSpace")) {
            if (sink->backspace)
                sink->backspace(data);
        } else if (!strcmp(out[i].keysym, "Return")) {
            if (sink->enter)
                sink->enter(data);
        }
        /* outras teclas (atalhos) não existem numa senha */
        explicit_bzero(&out[i], sizeof out[i]);
    }
    if (k->st.layout != old_layout || k->st.panel != old_panel) {
        int cursor = k->cursor;
        relayout(k);
        k->cursor = cursor >= 0 ? (cursor < k->geom.n ? cursor : k->geom.n - 1) : -1;
    }
    return LOCK_OSK_REDRAW;
}

int
lock_osk_down(LockOsk *k, int idx)
{
    k->pressed = idx >= 0 && idx < k->geom.n ? idx : -1;
    k->repeated = 0;
    return LOCK_OSK_REDRAW;
}

int
lock_osk_up(LockOsk *k, int idx, const LockOskSink *sink, void *data)
{
    int r = LOCK_OSK_REDRAW, p = k->pressed;
    k->pressed = -1;
    if (p >= 0 && p == idx && !k->repeated)
        r |= press(k, k->geom.key[p], sink, data);
    k->repeated = 0;
    return r;
}

int
lock_osk_repeatable(const LockOsk *k)
{
    if (k->pressed < 0 || k->pressed >= k->geom.n)
        return 0;
    const BkKey *key = k->geom.key[k->pressed];
    return key->kind == BK_KEY_CHAR || (key->kind == BK_KEY_SPECIAL && !strcmp(key->keysym, "BackSpace"));
}

int
lock_osk_repeat(LockOsk *k, const LockOskSink *sink, void *data)
{
    if (!lock_osk_repeatable(k))
        return 0;
    int p = k->pressed;
    k->repeated = 1;
    int r = press(k, k->geom.key[p], sink, data);
    k->pressed = p < k->geom.n ? p : -1;
    return r;
}

/* ---- controle físico ------------------------------------------------------- */

static int
find(const LockOsk *k, BkKeyKind kind, const char *keysym, unsigned mod)
{
    for (int i = 0; i < k->geom.n; i++) {
        const BkKey *key = k->geom.key[i];
        if (key->kind != kind)
            continue;
        if (kind == BK_KEY_SPECIAL && strcmp(key->keysym, keysym))
            continue;
        if (kind == BK_KEY_CHAR && strcmp(key->level[0], keysym))
            continue;
        if (kind == BK_KEY_MOD && key->mod != mod)
            continue;
        return i;
    }
    return -1;
}

static int
pad_press(LockOsk *k, int idx, const LockOskSink *sink, void *data, long long now)
{
    if (idx < 0 || idx >= k->geom.n)
        return 0;
    k->flash = idx;
    k->flash_until = now + LOCK_OSK_FLASH_MS;
    return press(k, k->geom.key[idx], sink, data);
}

int
lock_osk_pad(LockOsk *k, enum bwg_action a, const LockOskSink *sink, void *data, long long now)
{
    if (!k->shown) {
        /* Qualquer botão (menos B) abre o teclado, com o cursor na primeira tecla. */
        if (a == BWG_BACK)
            return 0;
        k->shown = 1;
        if (k->cursor < 0)
            k->cursor = 0;
        return LOCK_OSK_SHOW | LOCK_OSK_REDRAW;
    }
    int pointing = k->point[0] >= 0 || k->point[1] >= 0, idx = -1;
    switch (a) {
    case BWG_UP:
    case BWG_DOWN:
    case BWG_LEFT:
    case BWG_RIGHT: {
        bwg_rect r[BK_GEOM_MAX];
        for (int i = 0; i < k->geom.n; i++)
            r[i] = (bwg_rect){ k->geom.rect[i].x, k->geom.rect[i].y, k->geom.rect[i].w, k->geom.rect[i].h };
        k->cursor = bwg_spatial(r, k->geom.n, k->cursor, a);
        return LOCK_OSK_REDRAW;
    }
    case BWG_PRESS:
        if (k->cursor < 0) {
            k->cursor = 0;
            return LOCK_OSK_REDRAW;
        }
        idx = k->cursor;
        break;
    case BWG_LT: /* apontando: aperta a tecla do cursor esquerdo; senão Shift */
        idx = pointing ? k->point[0] : find(k, BK_KEY_MOD, NULL, BK_MOD_SHIFT);
        break;
    case BWG_RT: /* apontando: a do cursor direito; senão AltGr */
        idx = pointing ? k->point[1] : find(k, BK_KEY_MOD, NULL, BK_MOD_ALTGR);
        break;
    case BWG_BACKSPACE:
        idx = find(k, BK_KEY_SPECIAL, "BackSpace", 0);
        break;
    case BWG_SPACE:
        idx = find(k, BK_KEY_CHAR, " ", 0);
        break;
    case BWG_SHIFT:
        idx = find(k, BK_KEY_MOD, NULL, BK_MOD_SHIFT);
        break;
    case BWG_LAYOUT:
        idx = find(k, BK_KEY_LAYOUT, NULL, 0);
        break;
    case BWG_CAPS:
        idx = find(k, BK_KEY_CAPS, NULL, 0);
        break;
    case BWG_ENTER:
        idx = find(k, BK_KEY_SPECIAL, "Return", 0);
        break;
    case BWG_R3: { /* letras ↔ símbolos */
        BkKey panel = { .kind = BK_KEY_PANEL, .panel = k->st.panel == BK_PANEL_MAIN ? BK_PANEL_SYMBOLS : BK_PANEL_MAIN,
                        .width = 1 };
        return press(k, &panel, sink, data);
    }
    case BWG_BACK:
    case BWG_TOGGLE:
        k->shown = 0;
        return LOCK_OSK_HIDE | LOCK_OSK_REDRAW;
    default:
        return 0;
    }
    if (idx >= 0)
        return pad_press(k, idx, sink, data, now) | LOCK_OSK_REDRAW;
    /* fora do painel atual (símbolos sem Caps, por exemplo): a função da tecla */
    static const BkKey bs = { BK_KEY_SPECIAL, { "⌫", NULL, NULL }, 0, "BackSpace", 0, 0, 1 };
    static const BkKey ret = { BK_KEY_SPECIAL, { "⏎", NULL, NULL }, 0, "Return", 0, 0, 1 };
    static const BkKey space = { BK_KEY_CHAR, { " ", NULL, NULL }, 0, NULL, 0, 0, 1 };
    static const BkKey shift = { BK_KEY_MOD, { "⇧", NULL, NULL }, 0, NULL, BK_MOD_SHIFT, 0, 1 };
    static const BkKey altgr = { BK_KEY_MOD, { "AltGr", NULL, NULL }, 0, NULL, BK_MOD_ALTGR, 0, 1 };
    static const BkKey caps = { BK_KEY_CAPS, { "Caps", NULL, NULL }, 0, NULL, 0, 0, 1 };
    static const BkKey layout = { BK_KEY_LAYOUT, { "", NULL, NULL }, 0, NULL, 0, 0, 1 };
    const BkKey *key = a == BWG_BACKSPACE ? &bs
                       : a == BWG_ENTER   ? &ret
                       : a == BWG_SPACE   ? &space
                       : a == BWG_CAPS    ? &caps
                       : a == BWG_LAYOUT  ? &layout
                       : a == BWG_RT      ? (pointing ? NULL : &altgr)
                       : a == BWG_SHIFT || a == BWG_LT ? (pointing ? NULL : &shift)
                                                       : NULL;
    return key ? press(k, key, sink, data) : 0;
}

int
lock_osk_sticks(LockOsk *k, const bwg_pad *p)
{
    if (!k->shown || !p)
        return 0;
    double x[2] = { p->sx, p->rx }, y[2] = { p->sy, p->ry };
    int changed = 0;
    for (int side = 0; side < 2; side++) {
        if (hypot(x[side], y[side]) < LOCK_OSK_DEADZONE)
            continue; /* solto: o cursor fica onde estava */
        int idx = bk_geom_point(&k->geom, side, x[side], y[side]);
        if (idx != k->point[side]) {
            k->point[side] = idx;
            changed = 1;
        }
    }
    return changed ? LOCK_OSK_REDRAW : 0;
}

int
lock_osk_flash_ms(LockOsk *k, long long now)
{
    if (k->flash < 0)
        return -1;
    if (now >= k->flash_until) {
        k->flash = -1; /* volta ao normal: um último quadro */
        return 0;
    }
    return (int)(k->flash_until - now);
}

void
lock_osk_cleanup(void)
{
    /* O mapa de fontes do Pango segura objetos do fontconfig: sem soltá-lo,
     * o FcFini() da saída aborta (e o xsecurelock veria uma falha). */
    pango_cairo_font_map_set_default(NULL);
}
