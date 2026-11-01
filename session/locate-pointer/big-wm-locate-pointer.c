/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-locate-pointer — sacudir o mouse aumenta o ponteiro por um
 * instante, para achá-lo na tela (como no KDE Plasma e no macOS). X11.
 *
 *   big-wm-locate-pointer             daemon da sessão (autostart)
 *   big-wm-locate-pointer --show      mostra o ponteiro grande uma vez e sai
 *   big-wm-locate-pointer --enable | --disable | --toggle
 *                                     liga/desliga (grava a configuração,
 *                                     avisa o daemon em execução e, no
 *                                     --toggle do menu, mostra uma
 *                                     notificação)
 *   big-wm-locate-pointer --selftest  testa o detector com movimentos
 *                                     sintéticos, sem X
 *
 * Configuração: ~/.config/big-wm/locate-pointer.conf
 *   enabled=true|false
 *   sensitivity=low|medium|high
 *   max-scale=3          (1.5 a 8; vezes o tamanho atual do cursor)
 * Lida na partida e de novo com SIGHUP.
 *
 * Custo: nenhum polling. O XInput2 acorda o processo só quando o mouse se
 * move (XI_RawMotion na raiz); a posição na tela é lida no máximo a cada
 * 16 ms enquanto há movimento (o processo dorme entre uma leitura e
 * outra). Com uma janela em tela cheia ativa (jogos, vídeo), a
 * assinatura dos eventos de movimento é desfeita: o processo
 * dorme até a janela ativa mudar. Nenhuma trajetória é guardada: o
 * detector só conta inversões de direção.
 *
 * Detector: em cada eixo, um "trecho" é um deslocamento numa só direção.
 * Uma sacudida são N inversões seguidas de trechos longos (em pixels,
 * proporcionais à altura do monitor) e rápidos, dentro de uma janela de
 * tempo; um intervalo sem movimento, um botão apertado ou um trecho lento
 * ou curto zera a contagem. Movimentos normais, arrastar e desenhar não
 * disparam.
 *
 * O ponteiro grande é uma janela override-redirect que segue o ponteiro,
 * transparente aos cliques (região de entrada vazia), com a imagem do
 * tema de cursores no tamanho grande (Xcursor) ou, sem ela, a imagem atual
 * ampliada; o cursor real fica escondido (XFixes) só durante a animação.
 * Com compositor, alfa de verdade; sem ele, máscara de forma (XShape).
 */
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef SELFTEST_ONLY
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#endif

/* intervalo entre amostras da posição enquanto o mouse se move */
#define SAMPLE_MS 16

/* ---- configuração ------------------------------------------------------- */

struct config {
    bool enabled;
    int sensitivity; /* 0 baixa, 1 média, 2 alta */
    double max_scale;
};

static void config_defaults(struct config *c)
{
    c->enabled = true;
    c->sensitivity = 1;
    c->max_scale = 3.0;
}

static char *config_path(void)
{
    static char path[4096];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/big-wm/locate-pointer.conf", xdg);
    else
        snprintf(path, sizeof path, "%s/.config/big-wm/locate-pointer.conf", home ? home : ".");
    return path;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = 0;
    return s;
}

static void config_load(struct config *c)
{
    config_defaults(c);
    FILE *f = fopen(config_path(), "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (!*s || *s == '#')
            continue;
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "enabled"))
            c->enabled = !(!strcmp(v, "false") || !strcmp(v, "no") || !strcmp(v, "0"));
        else if (!strcmp(k, "sensitivity"))
            c->sensitivity = !strcmp(v, "low") ? 0 : !strcmp(v, "high") ? 2 : 1;
        else if (!strcmp(k, "max-scale")) {
            double m = strtod(v, NULL);
            if (m >= 1.5 && m <= 8.0)
                c->max_scale = m;
        }
    }
    fclose(f);
}

/* Troca só a linha enabled=, preservando o resto do arquivo. */
static int config_set_enabled(bool on)
{
    char *path = config_path();
    char dir[4096];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        char parent[4096];
        snprintf(parent, sizeof parent, "%s", dir);
        char *p2 = strrchr(parent, '/');
        if (p2) {
            *p2 = 0;
            mkdir(parent, 0755);
        }
        mkdir(dir, 0755);
    }
    char buf[8192] = "";
    size_t used = 0;
    bool found = false;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            const char *out = line;
            char repl[32];
            const char *s = line;
            while (*s == ' ' || *s == '\t')
                s++;
            if (!strncmp(s, "enabled", 7)) {
                const char *r = s + 7;
                while (*r == ' ' || *r == '\t')
                    r++;
                if (*r == '=') {
                    snprintf(repl, sizeof repl, "enabled=%s\n", on ? "true" : "false");
                    out = repl;
                    found = true;
                }
            }
            size_t n = strlen(out);
            if (used + n + 1 < sizeof buf) {
                memcpy(buf + used, out, n);
                used += n;
                buf[used] = 0;
            }
        }
        fclose(f);
    }
    if (!found) {
        int n = snprintf(buf + used, sizeof buf - used, "enabled=%s\n", on ? "true" : "false");
        if (n > 0)
            used += (size_t)n;
    }
    char tmpname[4200];
    snprintf(tmpname, sizeof tmpname, "%s.tmp", path);
    f = fopen(tmpname, "w");
    if (!f)
        return -1;
    fputs(buf, f);
    if (fclose(f) != 0)
        return -1;
    return rename(tmpname, path);
}

/* ---- detector ------------------------------------------------------------ */

struct axis {
    int dir;          /* -1, 0, +1 */
    int seg;          /* pixels no trecho atual */
    long seg_start;   /* ms */
    int reversals;
    long first_rev;   /* ms */
};

struct detector {
    struct axis ax[2];
    int last[2];
    long last_t;
    bool have_last;
    /* parâmetros */
    int min_seg;      /* px */
    int max_seg_ms;
    int need;         /* inversões */
    int window_ms;
    int idle_ms;
};

static void detector_reset(struct detector *d)
{
    memset(d->ax, 0, sizeof d->ax);
    d->have_last = false;
}

/* min_seg cresce com a altura do monitor: em 4K o mesmo gesto percorre
 * mais pixels. */
static void detector_setup(struct detector *d, int sensitivity, int monitor_h)
{
    static const int base[3] = {150, 110, 80};   /* px a 1080 linhas */
    static const int need[3] = {5, 4, 3};
    double k = monitor_h > 0 ? monitor_h / 1080.0 : 1.0;
    if (k < 0.75)
        k = 0.75;
    d->min_seg = (int)(base[sensitivity] * k);
    d->need = need[sensitivity];
    d->max_seg_ms = 260;
    d->window_ms = 1100;
    d->idle_ms = 160;
    detector_reset(d);
}

/* Uma amostra de posição (px) no instante t (ms). Retorna true quando
 * a sacudida é reconhecida. */
static bool detector_sample(struct detector *d, int x, int y, long t, bool button)
{
    if (button) {
        detector_reset(d);
        return false;
    }
    if (!d->have_last || t - d->last_t > d->idle_ms) {
        memset(d->ax, 0, sizeof d->ax);
        d->last[0] = x;
        d->last[1] = y;
        d->last_t = t;
        d->have_last = true;
        return false;
    }
    int pos[2] = {x, y};
    bool hit = false;
    for (int i = 0; i < 2; i++) {
        struct axis *a = &d->ax[i];
        int delta = pos[i] - d->last[i];
        if (delta == 0)
            continue;
        int s = delta > 0 ? 1 : -1;
        int mag = delta > 0 ? delta : -delta;
        if (a->dir == 0) {
            a->dir = s;
            a->seg = mag;
            a->seg_start = d->last_t;
        } else if (s == a->dir) {
            a->seg += mag;
        } else {
            bool good = a->seg >= d->min_seg && d->last_t - a->seg_start <= d->max_seg_ms;
            if (good) {
                if (a->reversals == 0 || t - a->first_rev > d->window_ms) {
                    a->reversals = 0;
                    a->first_rev = t;
                }
                a->reversals++;
            } else {
                a->reversals = 0;
            }
            a->dir = s;
            a->seg = mag;
            a->seg_start = d->last_t;
            if (a->reversals >= d->need && t - a->first_rev <= d->window_ms)
                hit = true;
        }
    }
    d->last[0] = x;
    d->last[1] = y;
    d->last_t = t;
    if (hit)
        memset(d->ax, 0, sizeof d->ax);
    return hit;
}

/* ---- autoteste ------------------------------------------------------------ */

/* Gera amostras a cada SAMPLE_MS indo de a até b em dur ms. */
static int feed_line(struct detector *d, int *t, int ax, int ay, int bx, int by, int dur, bool button)
{
    int hits = 0;
    int steps = dur / SAMPLE_MS;
    if (steps < 1)
        steps = 1;
    for (int i = 1; i <= steps; i++) {
        int x = ax + (bx - ax) * i / steps;
        int y = ay + (by - ay) * i / steps;
        *t += SAMPLE_MS;
        hits += detector_sample(d, x, y, *t, button);
    }
    return hits;
}

static int selftest(void)
{
    struct detector d;
    int fails = 0, t, hits;
#define CHECK(cond, msg)                                                                           \
    do {                                                                                           \
        if (cond)                                                                                  \
            printf("ok: %s\n", msg);                                                               \
        else {                                                                                     \
            printf("FALHOU: %s\n", msg);                                                           \
            fails++;                                                                               \
        }                                                                                          \
    } while (0)

    /* sacudida horizontal: 6 trechos de 300 px em 120 ms */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 6; i++)
        hits += feed_line(&d, &t, i % 2 ? 800 : 500, 500, i % 2 ? 500 : 800, 500, 120, false);
    CHECK(hits == 1, "sacudida horizontal rápida dispara uma vez");

    /* sacudida vertical */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 300, 500, 300, SAMPLE_MS, false);
    for (int i = 0; i < 6; i++)
        hits += feed_line(&d, &t, 500, i % 2 ? 600 : 300, 500, i % 2 ? 300 : 600, 110, false);
    CHECK(hits == 1, "sacudida vertical rápida dispara");

    /* vaivém lento (ajuste fino): 300 px em 500 ms */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 8; i++)
        hits += feed_line(&d, &t, i % 2 ? 800 : 500, 500, i % 2 ? 500 : 800, 500, 500, false);
    CHECK(hits == 0, "vaivém lento não dispara");

    /* tremor curto e rápido (30 px) */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 20; i++)
        hits += feed_line(&d, &t, i % 2 ? 530 : 500, 500, i % 2 ? 500 : 530, 500, 40, false);
    CHECK(hits == 0, "tremor curto não dispara");

    /* arrastar/desenhar com botão apertado */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    for (int i = 0; i < 8; i++)
        hits += feed_line(&d, &t, i % 2 ? 800 : 500, 500, i % 2 ? 500 : 800, 500, 120, true);
    CHECK(hits == 0, "com botão apertado não dispara");

    /* movimentos normais: atravessar a tela, ir e voltar uma vez */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 100, 100, 100, 100, SAMPLE_MS, false);
    hits += feed_line(&d, &t, 100, 100, 1800, 900, 250, false);
    hits += feed_line(&d, &t, 1800, 900, 200, 200, 250, false);
    hits += feed_line(&d, &t, 200, 200, 1000, 500, 300, false);
    CHECK(hits == 0, "movimentos largos normais não disparam");

    /* pausas entre os trechos zeram a contagem */
    detector_setup(&d, 1, 1080);
    t = 1000;
    hits = 0;
    for (int i = 0; i < 8; i++) {
        feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
        hits += feed_line(&d, &t, i % 2 ? 800 : 500, 500, i % 2 ? 500 : 800, 500, 120, false);
        t += 400;
    }
    CHECK(hits == 0, "trechos com pausas não disparam");

    /* 4K: o mesmo gesto em pixels de 1080p é curto demais */
    detector_setup(&d, 1, 2160);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 6; i++)
        hits += feed_line(&d, &t, i % 2 ? 680 : 500, 500, i % 2 ? 500 : 680, 500, 100, false);
    CHECK(hits == 0, "4K: gesto curto em pixels não dispara");
    detector_setup(&d, 1, 2160);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 6; i++)
        hits += feed_line(&d, &t, i % 2 ? 1100 : 500, 500, i % 2 ? 500 : 1100, 500, 130, false);
    CHECK(hits == 1, "4K: sacudida proporcional dispara");

    /* sensibilidade baixa pede mais inversões */
    detector_setup(&d, 0, 1080);
    t = 1000;
    hits = 0;
    feed_line(&d, &t, 500, 500, 500, 500, SAMPLE_MS, false);
    for (int i = 0; i < 4; i++)
        hits += feed_line(&d, &t, i % 2 ? 800 : 500, 500, i % 2 ? 500 : 800, 500, 120, false);
    CHECK(hits == 0, "sensibilidade baixa: 3 inversões não bastam");

    return fails ? 1 : 0;
}

#ifndef SELFTEST_ONLY

/* ---- X11 ----------------------------------------------------------------- */

static Display *dpy;
static Window root, overlay;
static int xi_opcode;
static bool xi_selected;
static bool composited;
static Visual *ov_visual;
static int ov_depth;
static Colormap ov_cmap;
static Atom a_active, a_state, a_fullscreen, a_cm;
static Window watched = None;
static volatile sig_atomic_t reload = 0, quit = 0;
static struct config cfg;
static bool debug;
#define DBG(...) do { if (debug) { fprintf(stderr, "big-wm-locate-pointer: " __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void on_signal(int sig)
{
    if (sig == SIGHUP)
        reload = 1;
    else
        quit = 1;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void select_motion(bool on)
{
    if (on == xi_selected)
        return;
    unsigned char bits[XIMaskLen(XI_LASTEVENT)] = {0};
    if (on)
        XISetMask(bits, XI_RawMotion);
    XIEventMask m = {.deviceid = XIAllMasterDevices, .mask_len = sizeof bits, .mask = bits};
    XISelectEvents(dpy, root, &m, 1);
    xi_selected = on;
    DBG("eventos de movimento: %s", on ? "assinados" : "desligados");
}

static int ignore_errors(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

static Window active_window(void)
{
    Atom type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    Window w = None;
    if (XGetWindowProperty(dpy, root, a_active, 0, 1, False, XA_WINDOW, &type, &fmt, &n, &after, &data) ==
            Success &&
        data) {
        if (n == 1 && fmt == 32)
            w = (Window) * (unsigned long *)data;
        XFree(data);
    }
    return w;
}

static bool is_fullscreen(Window w)
{
    if (w == None)
        return false;
    Atom type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    bool fs = false;
    if (XGetWindowProperty(dpy, w, a_state, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &data) == Success &&
        data) {
        Atom *atoms = (Atom *)data;
        for (unsigned long i = 0; i < n; i++)
            if (atoms[i] == a_fullscreen)
                fs = true;
        XFree(data);
    }
    return fs;
}

/* Janela ativa em tela cheia: sem eventos de movimento. */
static void refresh_active(void)
{
    XErrorHandler old = XSetErrorHandler(ignore_errors);
    Window w = active_window();
    if (w != watched) {
        if (watched != None)
            XSelectInput(dpy, watched, NoEventMask);
        watched = w;
        /* estado (tela cheia) e fim da janela: o Window Maker não muda
         * _NET_ACTIVE_WINDOW quando a janela ativa é fechada */
        if (w != None)
            XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    }
    bool fs = is_fullscreen(w);
    XSync(dpy, False);
    XSetErrorHandler(old);
    select_motion(cfg.enabled && !fs);
}

/* Altura do monitor sob o ponteiro (para o tamanho do gesto). */
static int monitor_height_at(int x, int y)
{
    int h = DisplayHeight(dpy, DefaultScreen(dpy));
    int n = 0;
    XRRMonitorInfo *mon = XRRGetMonitors(dpy, root, True, &n);
    if (mon) {
        for (int i = 0; i < n; i++)
            if (x >= mon[i].x && x < mon[i].x + mon[i].width && y >= mon[i].y && y < mon[i].y + mon[i].height)
                h = mon[i].height;
        XRRFreeMonitors(mon);
    }
    return h;
}

/* ---- imagem do ponteiro grande ------------------------------------------- */

struct image {
    int w, h, xhot, yhot;
    uint32_t *px; /* ARGB, não pré-multiplicado */
};

static void image_free(struct image *im)
{
    free(im->px);
    im->px = NULL;
}

/* A imagem do cursor atual (XFixes); false se ele está invisível (jogos e
 * vídeos escondem o ponteiro: nada a mostrar). */
static bool current_cursor(struct image *im, char *name, size_t namelen)
{
    XFixesCursorImage *ci = XFixesGetCursorImage(dpy);
    if (!ci)
        return false;
    bool visible = false;
    im->w = ci->width;
    im->h = ci->height;
    im->xhot = ci->xhot;
    im->yhot = ci->yhot;
    im->px = malloc(sizeof(uint32_t) * (size_t)im->w * im->h);
    for (int i = 0; im->px && i < im->w * im->h; i++) {
        uint32_t p = (uint32_t)ci->pixels[i]; /* pré-multiplicado */
        uint32_t a = p >> 24;
        if (a)
            visible = true;
        if (a && a < 255) {
            uint32_t r = ((p >> 16) & 0xff) * 255 / a, g = ((p >> 8) & 0xff) * 255 / a, b = (p & 0xff) * 255 / a;
            p = (a << 24) | ((r > 255 ? 255 : r) << 16) | ((g > 255 ? 255 : g) << 8) | (b > 255 ? 255 : b);
        }
        im->px[i] = p;
    }
    snprintf(name, namelen, "%s", ci->name ? ci->name : "");
    XFree(ci);
    if (!im->px)
        return false;
    if (!visible)
        image_free(im);
    return visible;
}

/* O mesmo cursor do tema no tamanho grande, quando o tema o tem. */
static bool theme_cursor(const char *name, int size, struct image *im)
{
    if (!name || !*name)
        return false;
    XcursorImage *xi = XcursorLibraryLoadImage(name, XcursorGetTheme(dpy), size);
    if (!xi)
        return false;
    im->w = (int)xi->width;
    im->h = (int)xi->height;
    im->xhot = (int)xi->xhot;
    im->yhot = (int)xi->yhot;
    im->px = malloc(sizeof(uint32_t) * (size_t)im->w * im->h);
    if (im->px) {
        for (int i = 0; i < im->w * im->h; i++) {
            uint32_t p = xi->pixels[i];
            uint32_t a = p >> 24;
            if (a && a < 255) {
                uint32_t r = ((p >> 16) & 0xff) * 255 / a, g = ((p >> 8) & 0xff) * 255 / a,
                         b = (p & 0xff) * 255 / a;
                p = (a << 24) | ((r > 255 ? 255 : r) << 16) | ((g > 255 ? 255 : g) << 8) | (b > 255 ? 255 : b);
            }
            im->px[i] = p;
        }
    }
    XcursorImageDestroy(xi);
    return im->px != NULL;
}

/* Reamostragem bilinear de src para w×h. */
static void scale_image(const struct image *src, int w, int h, uint32_t *dst)
{
    for (int y = 0; y < h; y++) {
        double sy = (y + 0.5) * src->h / h - 0.5;
        int y0 = (int)floor(sy);
        double fy = sy - y0;
        for (int x = 0; x < w; x++) {
            double sx = (x + 0.5) * src->w / w - 0.5;
            int x0 = (int)floor(sx);
            double fx = sx - x0;
            double acc[4] = {0};
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    int xx = x0 + i, yy = y0 + j;
                    double wgt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                    uint32_t p = 0;
                    if (xx >= 0 && yy >= 0 && xx < src->w && yy < src->h)
                        p = src->px[yy * src->w + xx];
                    double a = (p >> 24) / 255.0;
                    acc[0] += wgt * a;
                    acc[1] += wgt * a * ((p >> 16) & 0xff);
                    acc[2] += wgt * a * ((p >> 8) & 0xff);
                    acc[3] += wgt * a * (p & 0xff);
                }
            uint32_t out = 0;
            if (acc[0] > 0.004) {
                uint32_t a = (uint32_t)lround(acc[0] * 255);
                uint32_t r = (uint32_t)lround(acc[1] / acc[0]), g = (uint32_t)lround(acc[2] / acc[0]),
                         b = (uint32_t)lround(acc[3] / acc[0]);
                out = (a > 255 ? 255u : a) << 24 | (r > 255 ? 255u : r) << 16 | (g > 255 ? 255u : g) << 8 |
                      (b > 255 ? 255u : b);
            }
            dst[y * w + x] = out;
        }
    }
}

static void create_overlay(void)
{
    XVisualInfo vi;
    composited = XGetSelectionOwner(dpy, a_cm) != None;
    int scr = DefaultScreen(dpy);
    if (composited && XMatchVisualInfo(dpy, scr, 32, TrueColor, &vi)) {
        ov_visual = vi.visual;
        ov_depth = 32;
        ov_cmap = XCreateColormap(dpy, root, ov_visual, AllocNone);
    } else {
        composited = false;
        ov_visual = DefaultVisual(dpy, scr);
        ov_depth = DefaultDepth(dpy, scr);
        ov_cmap = DefaultColormap(dpy, scr);
    }
    XSetWindowAttributes a = {0};
    a.override_redirect = True;
    a.colormap = ov_cmap;
    a.border_pixel = 0;
    a.background_pixel = 0;
    a.save_under = True;
    overlay = XCreateWindow(dpy, root, -10, -10, 1, 1, 0, ov_depth, InputOutput, ov_visual,
                            CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel | CWSaveUnder, &a);
    /* cliques atravessam */
    XShapeCombineRectangles(dpy, overlay, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    /* sem sombra nem animação do compositor: tipo DND */
    Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dnd = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DND", False);
    XChangeProperty(dpy, overlay, type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dnd, 1);
    XClassHint ch = {"big-wm-locate-pointer", "BigWMLocatePointer"};
    XSetClassHint(dpy, overlay, &ch);
    XStoreName(dpy, overlay, "big-wm-locate-pointer");
}

/* Desenha a imagem w×h na janela (já do tamanho certo). */
static void paint(const uint32_t *px, int w, int h)
{
    XImage *xi = XCreateImage(dpy, ov_visual, (unsigned)ov_depth, ZPixmap, 0, NULL, (unsigned)w, (unsigned)h, 32, 0);
    if (!xi)
        return;
    xi->data = malloc((size_t)xi->bytes_per_line * h);
    if (!xi->data) {
        XDestroyImage(xi);
        return;
    }
    Pixmap mask = None;
    char *bits = NULL;
    int stride = (w + 7) / 8;
    if (!composited)
        bits = calloc((size_t)stride * h, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t p = px[y * w + x];
            uint32_t a = p >> 24;
            unsigned long out;
            if (composited) {
                uint32_t r = ((p >> 16) & 0xff) * a / 255, g = ((p >> 8) & 0xff) * a / 255, b = (p & 0xff) * a / 255;
                out = (unsigned long)a << 24 | r << 16 | g << 8 | b;
            } else {
                out = p & 0xffffff;
                if (a >= 100 && bits)
                    bits[y * stride + x / 8] |= (char)(1 << (x % 8));
            }
            XPutPixel(xi, x, y, out);
        }
    GC gc = XCreateGC(dpy, overlay, 0, NULL);
    if (bits) {
        mask = XCreateBitmapFromData(dpy, overlay, bits, (unsigned)w, (unsigned)h);
        XShapeCombineMask(dpy, overlay, ShapeBounding, 0, 0, mask, ShapeSet);
        XFreePixmap(dpy, mask);
        free(bits);
    }
    XPutImage(dpy, overlay, gc, xi, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
    XFreeGC(dpy, gc);
    XDestroyImage(xi);
}

static double ease(double t)
{
    return t < 0 ? 0 : t > 1 ? 1 : 1 - pow(1 - t, 3);
}

/* Animação: cresce em 160 ms, fica enquanto a sacudida continua (até
 * ~1,5 s), volta em 260 ms. Segue o ponteiro a 60 quadros por segundo só
 * durante a animação. */
static void show_big_pointer(struct detector *det)
{
    struct image cur = {0}, big = {0};
    char name[128] = "";
    if (!current_cursor(&cur, name, sizeof name)) {
        DBG("ponteiro invisível: nada a mostrar");
        return;
    }
    int base = XcursorGetDefaultSize(dpy);
    if (base <= 0)
        base = cur.h > 0 ? cur.h : 24;
    double maxs = cfg.max_scale;
    int maxsize = (int)lround(base * maxs);
    bool themed = theme_cursor(name, maxsize, &big);
    struct image *src = themed ? &big : &cur;
    DBG("ponteiro grande: cursor \"%s\" %dx%d, %s, escala até %.1f, %s", name, cur.w, cur.h,
        themed ? "imagem do tema" : "imagem atual ampliada", maxs, composited ? "com compositor" : "XShape");

    if (overlay == None)
        create_overlay();
    XFixesHideCursor(dpy, root);
    bool mapped = false;
    long start = now_ms(), hold_until = start + 700, end_grow = start + 160;
    int last_w = -1;
    uint32_t *buf = NULL;
    for (;;) {
        long t = now_ms();
        double k;
        if (t < end_grow)
            k = ease((t - start) / 160.0);
        else if (t < hold_until)
            k = 1;
        else if (t < hold_until + 260)
            k = 1 - ease((t - hold_until) / 260.0);
        else
            break;
        double scale = 1 + (maxs - 1) * k;
        /* tamanho final da imagem: proporcional à imagem atual (cur), que
         * corresponde a escala 1 */
        int w = (int)lround(cur.w * scale), h = (int)lround(cur.h * scale);
        if (w < 1 || h < 1)
            break;
        int xhot = (int)lround(cur.xhot * scale), yhot = (int)lround(cur.yhot * scale);
        if (themed) {
            /* o cursor do tema em tamanho grande, reduzido ao tamanho do
             * quadro: proporções e ponto quente do próprio tema */
            double r = (double)h / big.h;
            w = (int)lround(big.w * r);
            xhot = (int)lround(big.xhot * r);
            yhot = (int)lround(big.yhot * r);
        }
        Window rr, cr;
        int px, py, wx, wy;
        unsigned int mask;
        XQueryPointer(dpy, root, &rr, &cr, &px, &py, &wx, &wy, &mask);
        if (w != last_w) {
            free(buf);
            buf = malloc(sizeof(uint32_t) * (size_t)w * h);
            if (!buf)
                break;
            scale_image(src, w, h, buf);
            XMoveResizeWindow(dpy, overlay, px - xhot, py - yhot, (unsigned)w, (unsigned)h);
            if (!mapped) {
                XMapRaised(dpy, overlay);
                mapped = true;
            }
            paint(buf, w, h);
            last_w = w;
        } else {
            XMoveWindow(dpy, overlay, px - xhot, py - yhot);
        }
        XRaiseWindow(dpy, overlay);
        XFlush(dpy);
        /* a sacudida continua: mantém o ponteiro grande */
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == GenericEvent && ev.xcookie.extension == xi_opcode) {
                /* só drena; a posição vem do XQueryPointer */
            }
        }
        if (det && t >= end_grow && t < hold_until + 260 &&
            detector_sample(det, px, py, t, (mask & (Button1Mask | Button2Mask | Button3Mask)) != 0)) {
            long nh = t + 500;
            if (nh > start + 1500)
                nh = start + 1500;
            if (nh > hold_until)
                hold_until = nh;
        }
        struct timespec ts = {0, 16 * 1000000L};
        nanosleep(&ts, NULL);
    }
    free(buf);
    if (mapped)
        XUnmapWindow(dpy, overlay);
    XFixesShowCursor(dpy, root);
    XFlush(dpy);
    image_free(&cur);
    image_free(&big);
}

static int run_daemon(bool once)
{
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "big-wm-locate-pointer: sem display X11\n");
        return 1;
    }
    root = DefaultRootWindow(dpy);
    int ev, err, maj = 2, min = 0;
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &ev, &err) ||
        XIQueryVersion(dpy, &maj, &min) != Success) {
        fprintf(stderr, "big-wm-locate-pointer: XInput 2 indisponível\n");
        return 1;
    }
    int fev, ferr;
    if (!XFixesQueryExtension(dpy, &fev, &ferr)) {
        fprintf(stderr, "big-wm-locate-pointer: XFixes indisponível\n");
        return 1;
    }
    int smaj = 0, smin = 0;
    XFixesQueryVersion(dpy, &smaj, &smin);
    a_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    a_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    a_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    char cm[32];
    snprintf(cm, sizeof cm, "_NET_WM_CM_S%d", DefaultScreen(dpy));
    a_cm = XInternAtom(dpy, cm, False);

    if (once) {
        config_load(&cfg);
        cfg.enabled = true;
        show_big_pointer(NULL);
        XCloseDisplay(dpy);
        return 0;
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    config_load(&cfg);
    XSelectInput(dpy, root, PropertyChangeMask);
    refresh_active();

    struct detector det;
    Window rr, cr;
    int px = 0, py = 0, wx, wy;
    unsigned int mask = 0;
    XQueryPointer(dpy, root, &rr, &cr, &px, &py, &wx, &wy, &mask);
    detector_setup(&det, cfg.sensitivity, monitor_height_at(px, py));
    int mon_h = monitor_height_at(px, py);
    long last_mon_check = 0;

    struct pollfd pfd = {.fd = ConnectionNumber(dpy), .events = POLLIN};
    while (!quit) {
        if (reload) {
            reload = 0;
            config_load(&cfg);
            detector_setup(&det, cfg.sensitivity, mon_h);
            refresh_active();
        }
        bool motion = false;
        while (XPending(dpy)) {
            XEvent e;
            XNextEvent(dpy, &e);
            if (e.type == GenericEvent && e.xcookie.extension == xi_opcode) {
                /* XI_RawMotion: só o aviso de que houve movimento */
                motion = true;
            } else if (e.type == PropertyNotify) {
                if ((e.xproperty.window == root && e.xproperty.atom == a_active) ||
                    (e.xproperty.window == watched && e.xproperty.atom == a_state))
                    refresh_active();
            } else if ((e.type == DestroyNotify && e.xdestroywindow.window == watched) ||
                       (e.type == UnmapNotify && e.xunmap.window == watched)) {
                if (e.type == DestroyNotify)
                    watched = None;
                /* janela fechada ou minimizada: sem tela cheia */
                select_motion(cfg.enabled);
            } else if (e.type == MapNotify && e.xmap.window == watched) {
                refresh_active();
            }
        }
        long t = now_ms();
        if (motion) {
            XQueryPointer(dpy, root, &rr, &cr, &px, &py, &wx, &wy, &mask);
            if (t - last_mon_check > 1000) {
                int h = monitor_height_at(px, py);
                if (h != mon_h) {
                    mon_h = h;
                    detector_setup(&det, cfg.sensitivity, mon_h);
                }
                last_mon_check = t;
            }
            bool button = (mask & (Button1Mask | Button2Mask | Button3Mask)) != 0;
            if (detector_sample(&det, px, py, t, button)) {
                DBG("sacudida em %d,%d", px, py);
                show_big_pointer(&det);
                detector_reset(&det);
            } else {
                /* Uma amostra a cada SAMPLE_MS enquanto o mouse se move:
                 * dorme até a próxima em vez de acordar a cada evento (um
                 * mouse de 1000 Hz manda 1000 por segundo); os eventos
                 * que chegarem nesse meio-tempo são lidos de uma vez. */
                struct timespec ts = {0, SAMPLE_MS * 1000000L};
                nanosleep(&ts, NULL);
                continue;
            }
        }
        if (!XPending(dpy) && poll(&pfd, 1, -1) < 0 && errno != EINTR)
            break;
    }
    XCloseDisplay(dpy);
    return 0;
}

/* PIDs de outros big-wm-locate-pointer deste usuário (sem o próprio). */
static int others(pid_t *pids, int max)
{
    FILE *p = popen("pgrep -u \"$(id -u)\" -x big-wm-locate-p", "r");
    int n = 0;
    if (!p)
        return 0;
    char line[64];
    while (fgets(line, sizeof line, p)) {
        pid_t pid = (pid_t)atoi(line);
        if (pid > 0 && pid != getpid() && n < max)
            pids[n++] = pid;
    }
    pclose(p);
    return n;
}

#endif /* SELFTEST_ONLY */

int main(int argc, char **argv)
{
#ifndef SELFTEST_ONLY
    debug = getenv("BIG_WM_LOCATE_POINTER_DEBUG") != NULL;
#endif
    if (argc > 1 && !strcmp(argv[1], "--selftest"))
        return selftest();
#ifndef SELFTEST_ONLY
    pid_t pids[16];
    if (argc > 1 && (!strcmp(argv[1], "--enable") || !strcmp(argv[1], "--disable") || !strcmp(argv[1], "--toggle"))) {
        bool toggle = !strcmp(argv[1], "--toggle");
        bool on = !strcmp(argv[1], "--enable");
        if (toggle) {
            config_load(&cfg);
            on = !cfg.enabled;
        }
        if (config_set_enabled(on) != 0) {
            perror(config_path());
            return 1;
        }
        int n = others(pids, 16);
        /* o daemon em execução relê a configuração */
        for (int i = 0; i < n; i++)
            kill(pids[i], SIGHUP);
        const char *msg = on ? "Sacuda o mouse para ver o ponteiro grande." : "O ponteiro não aumenta mais ao sacudir o mouse.";
        const char *title = on ? "Localizar o ponteiro: ligado" : "Localizar o ponteiro: desligado";
        printf("%s\n", title);
        fflush(stdout);
        const char *display = getenv("DISPLAY");
        bool session = display && *display;
        /* aviso só na sessão gráfica (o menu); no terminal basta o texto */
        if (toggle && session && fork() == 0) {
            execlp("notify-send", "notify-send", "-a", "Big WindowMaker", "-i", "input-mouse", title, msg, (char *)NULL);
            _exit(0);
        }
        /* desligado na partida da sessão, não há daemon: este vira o daemon */
        if (on && n == 0 && session && fork() == 0) {
            setsid();
            config_load(&cfg);
            _exit(run_daemon(false));
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--show"))
        return run_daemon(true);
    if (argc > 1) {
        fprintf(stderr, "uso: big-wm-locate-pointer [--show | --enable | --disable | --toggle | --selftest]\n");
        return 2;
    }
    /* uma instância por usuário */
    if (others(pids, 16) > 0)
        return 0;
    config_load(&cfg);
    if (!cfg.enabled)
        return 0;
    return run_daemon(false);
#else
    return 0;
#endif
}
