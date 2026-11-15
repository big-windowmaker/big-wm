/* SPDX-License-Identifier: GPL-2.0-or-later
 * Controle na tela: layout e toques (veja bk-controls.h). */
#include "bk-controls.h"

#include <glib/gstdio.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <string.h>

#define B(code_, x_, y_, s_) { BKC_BUTTON, code_, x_, y_, s_, 0, 1, true }

void
bk_ctl_look_default(BkCtlLook *k)
{
    g_strlcpy(k->style, "classic", sizeof k->style);
    g_strlcpy(k->buttons, "xbox", sizeof k->buttons);
    g_strlcpy(k->accent, "blue", sizeof k->accent);
    g_strlcpy(k->stick, "fixed", sizeof k->stick);
    g_strlcpy(k->dpad, "cross", sizeof k->dpad);
    g_strlcpy(k->anim, "sink", sizeof k->anim);
}

void
bk_ctl_default(BkCtlLayout *l)
{
    memset(l, 0, sizeof *l);
    g_strlcpy(l->name, "padrão", sizeof l->name);
    l->opacity = 0.55;
    l->scale = 1.0;
    bk_ctl_look_default(&l->look);
    /* como num controle: analógico e direcional à esquerda, botões e o
     * analógico direito à direita, ombros em cima, Start/Select no meio e o
     * botão Guia (Xbox, PS, Home) entre eles, abaixo do botão do teclado */
    const BkCtl def[] = {
        { BKC_STICK, 0, 0.13, 0.62, 0.30, 0.15, 1.0, true },
        { BKC_DPAD, 0, 0.26, 0.84, 0.22, 0.30, 1.0, true },
        { BKC_STICK, 1, 0.74, 0.84, 0.24, 0.15, 1.0, true },
        B(BTN_SOUTH, 0.88, 0.70, 0.11), B(BTN_EAST, 0.95, 0.58, 0.11), B(BTN_WEST, 0.81, 0.58, 0.11),
        B(BTN_NORTH, 0.88, 0.46, 0.11),
        B(BTN_TL, 0.08, 0.22, 0.10), B(BTN_TR, 0.92, 0.22, 0.10),
        { BKC_TRIGGER, 0, 0.08, 0.08, 0.10, 0, 1, true },
        { BKC_TRIGGER, 1, 0.92, 0.08, 0.10, 0, 1, true },
        B(BTN_SELECT, 0.43, 0.10, 0.08), B(BTN_START, 0.57, 0.10, 0.08),
        B(BTN_MODE, BKC_GUIDE_X, BKC_GUIDE_Y, BKC_GUIDE_SIZE),
        B(BTN_THUMBL, 0.20, 0.42, 0.07), B(BTN_THUMBR, 0.66, 0.66, 0.07),
    };
    l->n = (int)G_N_ELEMENTS(def);
    memcpy(l->c, def, sizeof def);
}

void
bk_ctl_switch_button(double w, double h, double *cx, double *cy, double *r)
{
    double d = MAX(44.0, h * 0.085); /* 44 px: o mínimo confortável para um dedo */
    *r = d / 2;
    *cx = w / 2;
    *cy = MAX(*r + 4, h * 0.06);
}

bool
bk_ctl_switch_hit(double w, double h, double px, double py)
{
    double cx, cy, r;
    bk_ctl_switch_button(w, h, &cx, &cy, &r);
    return hypot(px - cx, py - cy) <= r * 1.15;
}

double
bk_ctl_px(const BkCtlLayout *l, const BkCtl *c, double h)
{
    return c->size * h * l->scale;
}

int
bk_ctl_hit(const BkCtlLayout *l, double w, double h, double px, double py)
{
    int best = -1;
    double bd = 0;
    for (int i = 0; i < l->n; i++) {
        const BkCtl *c = &l->c[i];
        if (!c->enabled)
            continue;
        double r = bk_ctl_px(l, c, h) / 2, dx = px - c->x * w, dy = py - c->y * h;
        double d = hypot(dx, dy);
        /* um pouco de folga além do círculo (dedos), o mais próximo vence */
        if (d <= r * 1.15 && (best < 0 || d < bd)) {
            best = i;
            bd = d;
        }
    }
    return best;
}

void
bk_ctl_state_init(BkCtlState *s)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < BKC_TOUCHES; i++)
        s->ctl[i] = -1;
}

/* Analógico: vetor do centro ao dedo, normalizado pelo raio; zona morta
 * reescalada (sem salto ao sair dela) e sensibilidade. */
static void
stick_vec(const BkCtlLayout *l, const BkCtl *c, double h, double cx, double cy, double px, double py, double *ox,
          double *oy)
{
    double r = bk_ctl_px(l, c, h) / 2;
    double dx = (px - cx) / r, dy = (py - cy) / r;
    double m = hypot(dx, dy);
    if (m < c->deadzone || m == 0) {
        *ox = *oy = 0;
        return;
    }
    double k = (m - c->deadzone) / (1 - c->deadzone) * c->sens;
    if (k > 1)
        k = 1;
    *ox = dx / m * k;
    *oy = dy / m * k;
}

static void
dpad_dir(const BkCtlLayout *l, const BkCtl *c, double w, double h, double px, double py, int *hx, int *hy)
{
    double r = bk_ctl_px(l, c, h) / 2;
    double dx = (px - c->x * w) / r, dy = (py - c->y * h) / r;
    *hx = *hy = 0;
    if (hypot(dx, dy) < c->deadzone)
        return;
    /* 8 direções: setores de 45° (diagonais apertam as duas) */
    double a = atan2(dy, dx) * 180 / M_PI; /* 0 = direita, 90 = baixo */
    int sector = (int)lround(a / 45.0);    /* -4..4 */
    static const int sx[9] = { -1, -1, 0, 1, 1, 1, 0, -1, -1 }, sy[9] = { 0, -1, -1, -1, 0, 1, 1, 1, 0 };
    *hx = sx[sector + 4];
    *hy = sy[sector + 4];
}

static void
apply(const BkCtlLayout *l, int ci, double w, double h, double px, double py, double cx, double cy, bool down,
      const BkCtlSink *k)
{
    const BkCtl *c = &l->c[ci];
    switch (c->kind) {
    case BKC_BUTTON:
        k->button(k->data, c->code, down);
        break;
    case BKC_TRIGGER:
        k->trigger(k->data, c->code, down ? 1.0 : 0.0);
        break;
    case BKC_STICK: {
        double x = 0, y = 0;
        if (down)
            stick_vec(l, c, h, cx, cy, px, py, &x, &y);
        k->stick(k->data, c->code, x, y);
        break;
    }
    case BKC_DPAD: {
        int hx = 0, hy = 0;
        if (down)
            dpad_dir(l, c, w, h, px, py, &hx, &hy);
        k->hat(k->data, hx, hy);
        break;
    }
    }
}

void
bk_ctl_touch(BkCtlState *s, const BkCtlLayout *l, double w, double h, const void *seq, BkCtlPhase phase,
             double px, double py, const BkCtlSink *sink)
{
    int t = -1;
    for (int i = 0; i < BKC_TOUCHES; i++)
        if (s->used[i] && s->seq[i] == seq)
            t = i;
    if (phase == BKC_BEGIN) {
        if (t >= 0)
            return;
        int ci = bk_ctl_hit(l, w, h, px, py);
        if (ci < 0)
            return;
        for (int i = 0; i < BKC_TOUCHES; i++)
            if (s->used[i] && s->ctl[i] == ci)
                return; /* um segundo dedo no mesmo controle não o rouba */
        for (int i = 0; i < BKC_TOUCHES && t < 0; i++)
            if (!s->used[i])
                t = i;
        if (t < 0)
            return;
        s->used[t] = true;
        s->seq[t] = seq;
        s->ctl[t] = ci;
        /* analógico flutuante: o centro é onde o dedo encostou */
        gboolean floating = l->c[ci].kind == BKC_STICK && !strcmp(l->look.stick, "floating");
        s->ox[t] = floating ? px : l->c[ci].x * w;
        s->oy[t] = floating ? py : l->c[ci].y * h;
        s->px[t] = px;
        s->py[t] = py;
        apply(l, ci, w, h, px, py, s->ox[t], s->oy[t], true, sink);
    } else if (t >= 0 && phase == BKC_MOVE) {
        const BkCtl *c = &l->c[s->ctl[t]];
        s->px[t] = px;
        s->py[t] = py;
        if (c->kind == BKC_STICK || c->kind == BKC_DPAD)
            apply(l, s->ctl[t], w, h, px, py, s->ox[t], s->oy[t], true, sink);
    } else if (t >= 0) { /* END, CANCEL: solta e o analógico volta ao centro */
        apply(l, s->ctl[t], w, h, px, py, s->ox[t], s->oy[t], false, sink);
        s->used[t] = false;
        s->ctl[t] = -1;
    } else {
        return;
    }
    sink->sync(sink->data);
}

void
bk_ctl_release(BkCtlState *s, const BkCtlLayout *l, const BkCtlSink *sink)
{
    for (int i = 0; i < BKC_TOUCHES; i++)
        if (s->used[i]) {
            apply(l, s->ctl[i], 1, 1, 0, 0, 0, 0, false, sink);
            s->used[i] = false;
            s->ctl[i] = -1;
        }
    sink->sync(sink->data);
}

bool
bk_ctl_clamp(BkCtl *c, double w, double h, double scale)
{
    double r = c->size * h * scale / 2;
    double minx = r / w, maxx = 1 - r / w, miny = r / h, maxy = 1 - r / h;
    double x = c->x < minx ? minx : c->x > maxx ? maxx : c->x;
    double y = c->y < miny ? miny : c->y > maxy ? maxy : c->y;
    bool moved = fabs(x - c->x) > 1e-9 || fabs(y - c->y) > 1e-9;
    c->x = x;
    c->y = y;
    return moved;
}

const char *
bk_ctl_label(const BkCtl *c)
{
    switch (c->kind) {
    case BKC_STICK:
        return c->code ? "R" : "L";
    case BKC_DPAD:
        return "✚";
    case BKC_TRIGGER:
        return c->code ? "RT" : "LT";
    case BKC_BUTTON:
        break;
    }
    switch (c->code) {
    case BTN_SOUTH:
        return "A";
    case BTN_EAST:
        return "B";
    case BTN_WEST:
        return "X";
    case BTN_NORTH:
        return "Y";
    case BTN_TL:
        return "LB";
    case BTN_TR:
        return "RB";
    case BTN_SELECT:
        return "⧉";
    case BTN_START:
        return "☰";
    case BTN_THUMBL:
        return "L3";
    case BTN_THUMBR:
        return "R3";
    case BTN_MODE:
        return "◉";
    default:
        return "?";
    }
}

/* ---- aparência ------------------------------------------------------------- */

static bool
family(const BkCtlLayout *l, const char *f)
{
    return !strcmp(l->look.buttons, f);
}

const char *
bk_ctl_label_for(const BkCtlLayout *l, const BkCtl *c)
{
    bool ps = family(l, "playstation"), nin = family(l, "nintendo");
    if (!ps && !nin)
        return bk_ctl_label(c);
    if (c->kind == BKC_TRIGGER)
        return ps ? (c->code ? "R2" : "L2") : (c->code ? "ZR" : "ZL");
    if (c->kind != BKC_BUTTON)
        return bk_ctl_label(c);
    switch (c->code) {
    case BTN_SOUTH: /* a mesma posição; o nome muda (o jogo vê o botão de baixo) */
        return ps ? "✕" : "B";
    case BTN_EAST:
        return ps ? "○" : "A";
    case BTN_WEST:
        return ps ? "□" : "Y";
    case BTN_NORTH:
        return ps ? "△" : "X";
    case BTN_TL:
        return ps ? "L1" : "L";
    case BTN_TR:
        return ps ? "R1" : "R";
    case BTN_SELECT:
        return ps ? "◫" : "−";
    case BTN_START:
        return ps ? "☰" : "+";
    case BTN_THUMBL:
        return ps ? "L3" : "LS";
    case BTN_THUMBR:
        return ps ? "R3" : "RS";
    case BTN_MODE:
        return ps ? "PS" : "⌂";
    default:
        return bk_ctl_label(c);
    }
}

static void
set_rgb(double rgb[3], double r, double g, double b)
{
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

bool
bk_ctl_color(const BkCtlLayout *l, const BkCtl *c, double rgb[3])
{
    if (c->kind != BKC_BUTTON)
        return false;
    /* posições: sul, leste, oeste, norte */
    static const double xbox[4][3] = { { 0.36, 0.72, 0.20 }, { 0.85, 0.22, 0.20 }, { 0.15, 0.48, 0.88 },
                                       { 0.95, 0.75, 0.12 } };
    static const double ps[4][3] = { { 0.45, 0.62, 0.95 }, { 0.93, 0.35, 0.38 }, { 0.88, 0.50, 0.78 },
                                     { 0.25, 0.78, 0.65 } };
    /* Super Nintendo: B amarelo (sul), A vermelho (leste), Y verde (oeste), X azul (norte) */
    static const double nin[4][3] = { { 0.95, 0.78, 0.12 }, { 0.86, 0.20, 0.20 }, { 0.20, 0.65, 0.30 },
                                      { 0.15, 0.40, 0.85 } };
    int i = c->code == BTN_SOUTH ? 0 : c->code == BTN_EAST ? 1 : c->code == BTN_WEST ? 2 : c->code == BTN_NORTH ? 3 : -1;
    if (i < 0)
        return false;
    const double(*t)[3] = family(l, "playstation") ? ps : family(l, "nintendo") ? nin : xbox;
    set_rgb(rgb, t[i][0], t[i][1], t[i][2]);
    return true;
}

void
bk_ctl_accent(const BkCtlLayout *l, double rgb[3])
{
    static const struct {
        const char *id;
        double r, g, b;
    } a[] = { { "blue", 0.20, 0.55, 0.95 },   { "red", 0.92, 0.26, 0.24 },    { "green", 0.25, 0.78, 0.35 },
              { "purple", 0.62, 0.38, 0.92 }, { "orange", 0.98, 0.58, 0.15 }, { "gray", 0.75, 0.75, 0.75 } };
    for (size_t i = 0; i < G_N_ELEMENTS(a); i++)
        if (!strcmp(l->look.accent, a[i].id)) {
            set_rgb(rgb, a[i].r, a[i].g, a[i].b);
            return;
        }
    set_rgb(rgb, a[0].r, a[0].g, a[0].b);
}

int
bk_ctl_touch_of(const BkCtlState *s, int ci)
{
    for (int i = 0; i < BKC_TOUCHES; i++)
        if (s->used[i] && s->ctl[i] == ci)
            return i;
    return -1;
}

void
bk_ctl_stick_visual(const BkCtlLayout *l, const BkCtlState *s, double w, double h, int ci, double *cx, double *cy,
                    double *kx, double *ky)
{
    const BkCtl *c = &l->c[ci];
    int t = bk_ctl_touch_of(s, ci);
    *cx = t >= 0 ? s->ox[t] : c->x * w;
    *cy = t >= 0 ? s->oy[t] : c->y * h;
    *kx = *ky = 0;
    if (t < 0)
        return;
    double r = bk_ctl_px(l, c, h) / 2, dx = (s->px[t] - *cx) / r, dy = (s->py[t] - *cy) / r, m = hypot(dx, dy);
    if (m > 1) {
        dx /= m;
        dy /= m;
    }
    *kx = dx;
    *ky = dy;
}

void
bk_ctl_dpad_visual(const BkCtlLayout *l, const BkCtlState *s, double w, double h, int ci, int *hx, int *hy)
{
    int t = bk_ctl_touch_of(s, ci);
    *hx = *hy = 0;
    if (t >= 0)
        dpad_dir(l, &l->c[ci], w, h, s->px[t], s->py[t], hx, hy);
}

/* ---- modelos ------------------------------------------------------------------ */

static const BkCtlTemplate templates[] = {
    { "xbox", "Xbox", "Analógico esquerdo em cima, direcional embaixo; A B X Y coloridos; visual moderno" },
    { "playstation", "PlayStation", "Direcional em cima, analógicos lado a lado embaixo; ✕ ○ □ △" },
    { "switch", "Nintendo Switch", "Como o Pro Controller: B A Y X, L R ZL ZR, − + e Home" },
    { "retro", "Retrô 16 bits", "Direcional grande, quatro botões, L e R, sem analógicos; cores fortes" },
    { "arcade", "Arcade", "Alavanca e seis botões em duas fileiras, com contorno luminoso" },
    { "racing", "Corrida", "Volante no analógico flutuante, acelerador e freio grandes" },
    { "minimal", "Mínimo", "Um analógico flutuante e dois botões, só o contorno" },
    { NULL, NULL, NULL },
};

const BkCtlTemplate *
bk_ctl_templates(void)
{
    return templates;
}

static void
put(BkCtlLayout *l, BkCtlKind kind, int code, double x, double y, double size, double dz)
{
    if (l->n < BKC_MAX)
        l->c[l->n++] = (BkCtl){ kind, code, x, y, size, dz, 1.0, true };
}

static void
look(BkCtlLayout *l, const char *style, const char *buttons, const char *accent, const char *stick,
     const char *dpad, const char *anim)
{
    g_strlcpy(l->look.style, style, sizeof l->look.style);
    g_strlcpy(l->look.buttons, buttons, sizeof l->look.buttons);
    g_strlcpy(l->look.accent, accent, sizeof l->look.accent);
    g_strlcpy(l->look.stick, stick, sizeof l->look.stick);
    g_strlcpy(l->look.dpad, dpad, sizeof l->look.dpad);
    g_strlcpy(l->look.anim, anim, sizeof l->look.anim);
}

/* Quatro botões de ação em losango ao redor de (x, y). */
static void
diamond(BkCtlLayout *l, double x, double y, double dx, double dy, double size)
{
    put(l, BKC_BUTTON, BTN_SOUTH, x, y + dy, size, 0);
    put(l, BKC_BUTTON, BTN_EAST, x + dx, y, size, 0);
    put(l, BKC_BUTTON, BTN_WEST, x - dx, y, size, 0);
    put(l, BKC_BUTTON, BTN_NORTH, x, y - dy, size, 0);
}

bool
bk_ctl_template(BkCtlLayout *l, const char *id)
{
    if (!id)
        return false;
    BkCtlLayout t;
    bk_ctl_default(&t);
    t.n = 0;
    if (!strcmp(id, "xbox")) {
        bk_ctl_default(&t);
        look(&t, "modern", "xbox", "green", "fixed", "cross", "glow");
    } else if (!strcmp(id, "playstation")) {
        put(&t, BKC_DPAD, 0, 0.13, 0.55, 0.24, 0.30);
        put(&t, BKC_STICK, 0, 0.32, 0.80, 0.24, 0.15);
        put(&t, BKC_STICK, 1, 0.68, 0.80, 0.24, 0.15);
        diamond(&t, 0.87, 0.55, 0.07, 0.12, 0.11);
        put(&t, BKC_BUTTON, BTN_TL, 0.08, 0.24, 0.10, 0);
        put(&t, BKC_BUTTON, BTN_TR, 0.92, 0.24, 0.10, 0);
        put(&t, BKC_TRIGGER, 0, 0.08, 0.09, 0.10, 0);
        put(&t, BKC_TRIGGER, 1, 0.92, 0.09, 0.10, 0);
        put(&t, BKC_BUTTON, BTN_SELECT, 0.40, 0.12, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.60, 0.12, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_THUMBL, 0.44, 0.93, 0.07, 0);
        put(&t, BKC_BUTTON, BTN_THUMBR, 0.56, 0.93, 0.07, 0);
        look(&t, "modern", "playstation", "blue", "fixed", "buttons", "ripple");
    } else if (!strcmp(id, "switch")) {
        put(&t, BKC_STICK, 0, 0.13, 0.55, 0.26, 0.15);
        put(&t, BKC_DPAD, 0, 0.30, 0.82, 0.22, 0.30);
        put(&t, BKC_STICK, 1, 0.70, 0.82, 0.24, 0.15);
        diamond(&t, 0.87, 0.55, 0.07, 0.12, 0.11);
        put(&t, BKC_BUTTON, BTN_TL, 0.08, 0.26, 0.10, 0);
        put(&t, BKC_BUTTON, BTN_TR, 0.92, 0.26, 0.10, 0);
        put(&t, BKC_TRIGGER, 0, 0.08, 0.10, 0.10, 0);
        put(&t, BKC_TRIGGER, 1, 0.92, 0.10, 0.10, 0);
        put(&t, BKC_BUTTON, BTN_SELECT, 0.40, 0.12, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.60, 0.12, 0.08, 0);
        look(&t, "modern", "nintendo", "red", "fixed", "disc", "bounce");
    } else if (!strcmp(id, "retro")) {
        put(&t, BKC_DPAD, 0, 0.17, 0.62, 0.32, 0.25);
        diamond(&t, 0.84, 0.62, 0.075, 0.13, 0.13);
        put(&t, BKC_BUTTON, BTN_TL, 0.10, 0.18, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_TR, 0.90, 0.18, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_SELECT, 0.42, 0.82, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.58, 0.82, 0.08, 0);
        look(&t, "retro", "nintendo", "purple", "fixed", "cross", "sink");
    } else if (!strcmp(id, "arcade")) {
        put(&t, BKC_STICK, 0, 0.18, 0.62, 0.30, 0.20);
        put(&t, BKC_BUTTON, BTN_WEST, 0.62, 0.50, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_NORTH, 0.74, 0.46, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_TR, 0.86, 0.46, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_SOUTH, 0.62, 0.72, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_EAST, 0.74, 0.68, 0.12, 0);
        put(&t, BKC_TRIGGER, 1, 0.86, 0.68, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_SELECT, 0.40, 0.12, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.60, 0.12, 0.08, 0);
        look(&t, "neon", "xbox", "red", "fixed", "cross", "glow");
    } else if (!strcmp(id, "racing")) {
        put(&t, BKC_STICK, 0, 0.20, 0.62, 0.34, 0.10);
        put(&t, BKC_TRIGGER, 1, 0.88, 0.70, 0.22, 0);
        put(&t, BKC_TRIGGER, 0, 0.68, 0.76, 0.18, 0);
        put(&t, BKC_BUTTON, BTN_SOUTH, 0.90, 0.38, 0.11, 0);
        put(&t, BKC_BUTTON, BTN_WEST, 0.78, 0.44, 0.11, 0);
        put(&t, BKC_BUTTON, BTN_TL, 0.08, 0.22, 0.11, 0);
        put(&t, BKC_BUTTON, BTN_TR, 0.92, 0.18, 0.11, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.60, 0.12, 0.08, 0);
        put(&t, BKC_BUTTON, BTN_SELECT, 0.40, 0.12, 0.08, 0);
        look(&t, "modern", "xbox", "orange", "floating", "cross", "ripple");
    } else if (!strcmp(id, "minimal")) {
        put(&t, BKC_STICK, 0, 0.18, 0.65, 0.30, 0.15);
        put(&t, BKC_BUTTON, BTN_SOUTH, 0.86, 0.74, 0.15, 0);
        put(&t, BKC_BUTTON, BTN_EAST, 0.94, 0.52, 0.12, 0);
        put(&t, BKC_BUTTON, BTN_START, 0.60, 0.12, 0.08, 0);
        look(&t, "minimal", "xbox", "gray", "floating", "cross", "ripple");
    } else {
        return false;
    }
    /* o botão Guia em todos, no mesmo lugar (o padrão e o Xbox já o têm) */
    bool has_guide = false;
    for (int i = 0; i < t.n; i++)
        has_guide |= t.c[i].kind == BKC_BUTTON && t.c[i].code == BTN_MODE;
    if (!has_guide)
        put(&t, BKC_BUTTON, BTN_MODE, BKC_GUIDE_X, BKC_GUIDE_Y, BKC_GUIDE_SIZE, 0);
    for (const BkCtlTemplate *m = templates; m->id; m++)
        if (!strcmp(m->id, id))
            g_strlcpy(t.name, m->name, sizeof t.name);
    *l = t;
    return true;
}

/* ---- arquivo ------------------------------------------------------------- */

char *
bk_ctl_path(const char *name)
{
    char *safe = g_strdup(name && *name ? name : "padrão");
    g_strdelimit(safe, "/\\:", '-');
    char *file = g_strconcat(safe, ".ini", NULL);
    char *path = g_build_filename(g_get_user_data_dir(), "big-keyboard", "gamepad-layouts", file, NULL);
    g_free(file);
    g_free(safe);
    return path;
}

static const char *kinds[] = { "stick", "dpad", "button", "trigger" };

gboolean
bk_ctl_save(const BkCtlLayout *l, const char *path, GError **err)
{
    GKeyFile *k = g_key_file_new();
    /* 2: o botão Guia faz parte do layout (um layout 1 o ganha ao ser lido) */
    g_key_file_set_integer(k, "Layout", "Version", 2);
    g_key_file_set_string(k, "Layout", "Name", l->name);
    g_key_file_set_double(k, "Layout", "Opacity", l->opacity);
    g_key_file_set_double(k, "Layout", "Scale", l->scale);
    g_key_file_set_string(k, "Layout", "Style", l->look.style);
    g_key_file_set_string(k, "Layout", "Buttons", l->look.buttons);
    g_key_file_set_string(k, "Layout", "Accent", l->look.accent);
    g_key_file_set_string(k, "Layout", "Stick", l->look.stick);
    g_key_file_set_string(k, "Layout", "Dpad", l->look.dpad);
    g_key_file_set_string(k, "Layout", "Animation", l->look.anim);
    for (int i = 0; i < l->n; i++) {
        char g[32];
        g_snprintf(g, sizeof g, "Control %02d", i);
        const BkCtl *c = &l->c[i];
        g_key_file_set_string(k, g, "Kind", kinds[c->kind]);
        g_key_file_set_integer(k, g, "Code", c->code);
        g_key_file_set_double(k, g, "X", c->x);
        g_key_file_set_double(k, g, "Y", c->y);
        g_key_file_set_double(k, g, "Size", c->size);
        g_key_file_set_double(k, g, "Deadzone", c->deadzone);
        g_key_file_set_double(k, g, "Sensitivity", c->sens);
        g_key_file_set_boolean(k, g, "Enabled", c->enabled);
    }
    char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    gboolean ok = g_key_file_save_to_file(k, path, err); /* atômico */
    g_key_file_free(k);
    return ok;
}

/* Um valor de uma lista fixa; senão fica o padrão. */
static void
look_get(GKeyFile *k, const char *key, char *field, size_t len, const char *const *ok)
{
    char *v = g_key_file_get_string(k, "Layout", key, NULL);
    for (int i = 0; v && ok[i]; i++)
        if (!strcmp(v, ok[i]))
            g_strlcpy(field, v, len);
    g_free(v);
}

static double
clampd(double v, double lo, double hi)
{
    return isnan(v) ? lo : v < lo ? lo : v > hi ? hi : v;
}

/* Layout de antes do botão Guia: ele entra no lugar de sempre; se algo do
 * layout estiver lá, entra desligado (aparece apagado no editor, para ligar
 * e mover), sem cobrir nada. */
static void
add_guide(BkCtlLayout *l)
{
    if (l->n >= BKC_MAX)
        return;
    for (int i = 0; i < l->n; i++)
        if (l->c[i].kind == BKC_BUTTON && l->c[i].code == BTN_MODE)
            return;
    BkCtl g = B(BTN_MODE, BKC_GUIDE_X, BKC_GUIDE_Y, BKC_GUIDE_SIZE);
    /* numa tela 16:9 (a mais comum); os tamanhos são frações da altura */
    const double w = 16.0 / 9;
    for (int i = 0; i < l->n; i++) {
        const BkCtl *c = &l->c[i];
        if (c->enabled && hypot((c->x - g.x) * w, c->y - g.y) < (c->size + g.size) * l->scale / 2)
            g.enabled = false;
    }
    l->c[l->n++] = g;
}

gboolean
bk_ctl_load(BkCtlLayout *l, const char *path, GError **err)
{
    GKeyFile *k = g_key_file_new();
    if (!g_key_file_load_from_file(k, path, G_KEY_FILE_NONE, err)) {
        g_key_file_free(k);
        return FALSE;
    }
    BkCtlLayout tmp;
    memset(&tmp, 0, sizeof tmp);
    char *name = g_key_file_get_string(k, "Layout", "Name", NULL);
    g_strlcpy(tmp.name, name ? name : "importado", sizeof tmp.name);
    g_free(name);
    tmp.opacity = clampd(g_key_file_get_double(k, "Layout", "Opacity", NULL), 0.15, 1.0);
    tmp.scale = clampd(g_key_file_get_double(k, "Layout", "Scale", NULL), 0.5, 2.0);
    bk_ctl_look_default(&tmp.look);
    static const char *const styles[] = { "classic", "modern", "retro", "neon", "minimal", NULL };
    static const char *const families[] = { "xbox", "playstation", "nintendo", NULL };
    static const char *const accents[] = { "blue", "red", "green", "purple", "orange", "gray", NULL };
    static const char *const sticks[] = { "fixed", "floating", NULL };
    static const char *const dpads[] = { "cross", "buttons", "disc", NULL };
    static const char *const anims[] = { "none", "sink", "glow", "ripple", "bounce", NULL };
    look_get(k, "Style", tmp.look.style, sizeof tmp.look.style, styles);
    look_get(k, "Buttons", tmp.look.buttons, sizeof tmp.look.buttons, families);
    look_get(k, "Accent", tmp.look.accent, sizeof tmp.look.accent, accents);
    look_get(k, "Stick", tmp.look.stick, sizeof tmp.look.stick, sticks);
    look_get(k, "Dpad", tmp.look.dpad, sizeof tmp.look.dpad, dpads);
    look_get(k, "Animation", tmp.look.anim, sizeof tmp.look.anim, anims);
    gsize ngroups = 0;
    char **groups = g_key_file_get_groups(k, &ngroups);
    for (gsize i = 0; i < ngroups && tmp.n < BKC_MAX; i++) {
        if (!g_str_has_prefix(groups[i], "Control "))
            continue;
        char *kind = g_key_file_get_string(k, groups[i], "Kind", NULL);
        int ki = -1;
        for (int j = 0; kind && j < 4; j++)
            if (!strcmp(kind, kinds[j]))
                ki = j;
        g_free(kind);
        if (ki < 0)
            continue; /* controle desconhecido: ignorado, não derruba o layout */
        BkCtl *c = &tmp.c[tmp.n++];
        c->kind = (BkCtlKind)ki;
        c->code = g_key_file_get_integer(k, groups[i], "Code", NULL);
        if ((c->kind == BKC_STICK || c->kind == BKC_TRIGGER) && c->code != 1)
            c->code = 0;
        c->x = clampd(g_key_file_get_double(k, groups[i], "X", NULL), 0, 1);
        c->y = clampd(g_key_file_get_double(k, groups[i], "Y", NULL), 0, 1);
        c->size = clampd(g_key_file_get_double(k, groups[i], "Size", NULL), 0.04, 0.6);
        c->deadzone = clampd(g_key_file_get_double(k, groups[i], "Deadzone", NULL), 0, 0.8);
        c->sens = clampd(g_key_file_get_double(k, groups[i], "Sensitivity", NULL), 0.3, 3);
        GError *e = NULL;
        c->enabled = g_key_file_get_boolean(k, groups[i], "Enabled", &e);
        if (e) {
            c->enabled = true;
            g_error_free(e);
        }
    }
    g_strfreev(groups);
    int version = g_key_file_get_integer(k, "Layout", "Version", NULL);
    g_key_file_free(k);
    if (tmp.n == 0) {
        g_set_error(err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE, "nenhum controle no layout");
        return FALSE;
    }
    if (version < 2)
        add_guide(&tmp);
    *l = tmp;
    return TRUE;
}
