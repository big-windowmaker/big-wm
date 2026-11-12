/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-locate-pointer — sacudir o mouse aumenta o ponteiro por um
 * instante, para achá-lo na tela (como no KDE Plasma e no macOS). X11.
 *
 *   big-wm-locate-pointer             daemon da sessão (autostart)
 *   big-wm-locate-pointer --show      mostra o ponteiro grande uma vez (pede
 *                                     ao daemon deste display; sem ele,
 *                                     mostra numa janela e sai)
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
 *   grow-ms=160          (tempo para crescer)
 *   duration-ms=700      (do início até começar a voltar)
 *   return-ms=260        (tempo para voltar ao normal)
 * Lida na partida e de novo com SIGHUP.
 *
 * Wayland: quem amplia o ponteiro é o próprio wlmaker (LocatePointer, que
 * lê este mesmo arquivo e percebe as mudanças sozinho). Lá este programa só
 * grava a configuração (--enable, --disable, --toggle); o daemon não roda
 * (pelo XWayland ele só veria as janelas X11 e desenharia um segundo
 * ponteiro, fora de sincronia com o verdadeiro).
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
 * O ponteiro grande é o próprio cursor: a imagem dos cursores com o nome
 * do cursor atual (left_ptr, text...) é trocada no servidor X
 * (XFixesChangeCursorByName) pela imagem grande do tema (Xcursor) a cada
 * passo da animação e devolvida no fim. Quem desenha é o servidor, como
 * qualquer cursor: segue o mouse sem atraso e não depende do compositor
 * (o picom faz fade e, com unredir-if-possible, redireciona a tela inteira
 * quando aparece uma janela por cima de outra em tela cheia; uma janela
 * desenhando o ponteiro sumia nesses momentos). Os nomes trocados ficam
 * anotados em $XDG_RUNTIME_DIR enquanto a animação dura: se o processo
 * morrer no meio, a próxima partida devolve as imagens.
 *
 * Isso só quando o tema do daemon tem, com esse nome, a mesma imagem que
 * está na tela: um programa com outro tema de cursor (ou com imagem
 * própria sob um nome do tema) fica com a janela abaixo, com a imagem dele;
 * trocar pelo nome mudaria também o cursor dos outros programas.
 *
 * Cursor sem nome (criado pelo programa, sem tema): janela
 * override-redirect que segue o ponteiro, transparente aos cliques, com o
 * cursor real escondido (XFixes) só durante a animação; com compositor,
 * alfa de verdade, sem ele, máscara de forma (XShape). O compositor é
 * verificado a cada vez (o picom pode ter sido reiniciado ou parado). A
 * janela anda a cada movimento do mouse (não só a cada quadro) e troca de
 * imagem quando o cursor muda por baixo dela (outra janela, outro cursor).
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
#include <fcntl.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <sys/file.h>
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

/* Catálogo "big-wm" (po/ na raiz do big-wm), msgids em inglês. Só o
 * LC_MESSAGES do ambiente: o strtod da configuração fica no locale C.
 * BIGWM_LOCALEDIR: catálogos compilados fora de /usr/share/locale (testes). */
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif
#define _(s) gettext(s)

#ifndef SELFTEST_ONLY
static void init_i18n(void)
{
    const char *dir = getenv("BIGWM_LOCALEDIR");
    setlocale(LC_MESSAGES, "");
    bindtextdomain("big-wm", dir && *dir ? dir : LOCALEDIR);
    bind_textdomain_codeset("big-wm", "UTF-8");
    textdomain("big-wm");
}
#endif

/* intervalo entre amostras da posição enquanto o mouse se move */
#define SAMPLE_MS 16

/* ---- configuração ------------------------------------------------------- */

struct config {
    bool enabled;
    int sensitivity; /* 0 baixa, 1 média, 2 alta */
    double max_scale;
    int grow_ms;     /* tempo para crescer */
    int duration_ms; /* do início até começar a voltar */
    int return_ms;   /* tempo para voltar */
};

static void config_defaults(struct config *c)
{
    c->enabled = true;
    c->sensitivity = 1;
    c->max_scale = 3.0;
    c->grow_ms = 160;
    c->duration_ms = 700;
    c->return_ms = 260;
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
        } else if (!strcmp(k, "grow-ms")) {
            long m = strtol(v, NULL, 10);
            if (m >= 0 && m <= 2000)
                c->grow_ms = (int)m;
        } else if (!strcmp(k, "duration-ms")) {
            long m = strtol(v, NULL, 10);
            if (m >= 200 && m <= 10000)
                c->duration_ms = (int)m;
        } else if (!strcmp(k, "return-ms")) {
            long m = strtol(v, NULL, 10);
            if (m >= 0 && m <= 5000)
                c->return_ms = (int)m;
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
static int xi_opcode, xfixes_event;
static bool xi_selected;
static bool composited;
static Visual *ov_visual;
static int ov_depth;
static Colormap ov_cmap;
static Atom a_active, a_state, a_fullscreen, a_cm;
static Window watched = None;
static volatile sig_atomic_t reload = 0, quit = 0, show_now = 0;
static bool force_overlay; /* --show sem daemon: não troca cursores */
static struct config cfg;
static bool debug;
#define DBG(...) do { if (debug) { fprintf(stderr, "big-wm-locate-pointer: " __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void on_signal(int sig)
{
    if (sig == SIGHUP)
        reload = 1;
    else if (sig == SIGUSR1)
        show_now = 1;
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

/* Um erro X não derruba o daemon (o padrão do Xlib encerra o processo):
 * janela que sumiu, cursor ilegível (BadAccess)... */
static int ignore_errors(Display *d, XErrorEvent *e)
{
    (void)d;
    DBG("erro X ignorado: código %d, pedido %d.%d", e->error_code, e->request_code, e->minor_code);
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

/* A janela da reserva (cursor sem nome). Com compositor ARGB; sem ele,
 * visual comum e XShape. Refeita quando o compositor aparece ou some. */
static void create_overlay(void)
{
    bool cm = XGetSelectionOwner(dpy, a_cm) != None;
    if (overlay != None && cm == composited)
        return;
    if (overlay != None) {
        XDestroyWindow(dpy, overlay);
        overlay = None;
        if (ov_cmap != DefaultColormap(dpy, DefaultScreen(dpy)))
            XFreeColormap(dpy, ov_cmap);
    }
    XVisualInfo vi;
    composited = cm;
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
    /* sem sombra nem animação do compositor: tipo DND (o picom.conf do
     * Big WindowMaker também tira fade e sombra pela classe) */
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

/* Mesma imagem (mesmo tamanho, ponto ativo e pixels)? */
static bool image_equal(const struct image *a, const struct image *b)
{
    if (!a->px || !b->px || a->w != b->w || a->h != b->h || a->xhot != b->xhot || a->yhot != b->yhot)
        return false;
    for (int i = 0; i < a->w * a->h; i++) {
        uint32_t p = a->px[i], q = b->px[i];
        for (int sh = 0; sh < 32; sh += 8) {
            int d = (int)((p >> sh) & 0xff) - (int)((q >> sh) & 0xff);
            /* a ida e volta do pré-multiplicado arredonda; pixels quase
             * transparentes perdem a cor */
            if ((d > 2 || d < -2) && ((p >> 24) > 8 || (q >> 24) > 8))
                return false;
        }
    }
    return true;
}

/* O tema do daemon tem, no tamanho da sessão, a imagem que está na tela?
 * (Um programa com outro tema de cursor, ou com imagem própria sob um nome
 * do tema, não tem: a imagem dele é que é ampliada.) */
static bool theme_matches(const char *name, const struct image *cur)
{
    struct image t = {0};
    if (!theme_cursor(name, XcursorGetDefaultSize(dpy), &t))
        return false;
    bool eq = image_equal(&t, cur);
    image_free(&t);
    return eq;
}

/* ---- cursor ampliado no servidor ----------------------------------------- */

#define MAX_NAMES 8

struct named {
    char name[64];
    struct image big;  /* a imagem a ampliar: a do tema em tamanho grande, ou a da tela */
    struct image orig; /* a imagem mostrada antes, devolvida no fim */
    int last_h;
};

/* Arquivo deste display em $XDG_RUNTIME_DIR (NULL sem ele). */
static char *runtime_path(const char *suffix)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    if (!rt || !*rt)
        return NULL;
    static char p[512];
    const char *d = getenv("DISPLAY");
    snprintf(p, sizeof p, "%s/big-wm-locate-pointer%s%s", rt, d && *d ? d : "", suffix);
    return p;
}

/* Os cursores trocados passam a ser do processo que os criou: com a
 * extensão SECURITY, se ele sair, ninguém mais lê a imagem deles
 * (XFixesGetCursorImage dá BadAccess; OBS e capturas perdem o ponteiro).
 * Por isso só o daemon, que vive a sessão inteira, troca cursores, e os
 * nomes trocados ficam anotados: um daemon novo (depois de uma queda)
 * assume todos de novo, com o tamanho normal. */
static void note_owned(const char *name)
{
    char *p = runtime_path(".names");
    if (!p)
        return;
    FILE *f = fopen(p, "a+");
    if (!f)
        return;
    char line[80];
    bool found = false;
    rewind(f);
    while (!found && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        found = !strcmp(line, name);
    }
    if (!found)
        fprintf(f, "%s\n", name);
    fclose(f);
}

/* Cursor a partir de uma imagem ARGB não pré-multiplicada. */
static Cursor cursor_from(const uint32_t *px, int w, int h, int xhot, int yhot)
{
    XcursorImage *ci = XcursorImageCreate(w, h);
    if (!ci)
        return None;
    ci->xhot = (XcursorDim)(xhot < 0 ? 0 : xhot >= w ? w - 1 : xhot);
    ci->yhot = (XcursorDim)(yhot < 0 ? 0 : yhot >= h ? h - 1 : yhot);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = px[i], a = p >> 24;
        uint32_t r = ((p >> 16) & 0xff) * a / 255, g = ((p >> 8) & 0xff) * a / 255, b = (p & 0xff) * a / 255;
        ci->pixels[i] = a << 24 | r << 16 | g << 8 | b;
    }
    Cursor c = XcursorImageLoadCursor(dpy, ci);
    XcursorImageDestroy(ci);
    return c;
}

/* Troca a imagem de todos os cursores com este nome pela de c. Os cursores
 * passam a ser c: ele precisa do mesmo nome para a próxima troca (e a
 * devolução) achá-los. */
static void replace_named(Cursor c, const char *name)
{
    XFixesSetCursorName(dpy, c, name);
    XFixesChangeCursorByName(dpy, c, name);
    XFreeCursor(dpy, c);
}

/* Devolve a imagem normal aos cursores com este nome: a do tema no tamanho
 * da sessão (animada, se for o caso) quando é a que estava na tela; senão,
 * exatamente a imagem que estava na tela. Sem a imagem de antes (cursores
 * deixados por um daemon que morreu), a do tema. */
static void restore_name(const char *name, const struct image *orig)
{
    Cursor c = None;
    XcursorImages *imgs = XcursorLibraryLoadImages(name, XcursorGetTheme(dpy), XcursorGetDefaultSize(dpy));
    bool use_theme = imgs && imgs->nimage > 0;
    if (use_theme && orig && orig->px) {
        struct image t = {0};
        use_theme = theme_cursor(name, XcursorGetDefaultSize(dpy), &t) && image_equal(&t, orig);
        image_free(&t);
    }
    if (use_theme)
        c = XcursorImagesLoadCursor(dpy, imgs);
    if (imgs)
        XcursorImagesDestroy(imgs);
    if (c == None && orig && orig->px)
        c = cursor_from(orig->px, orig->w, orig->h, orig->xhot, orig->yhot);
    if (c != None)
        replace_named(c, name);
}

/* Na partida do daemon: assume os cursores trocados por um daemon
 * anterior (e devolve o tamanho normal, se ele morreu no meio). */
static void restore_leftovers(void)
{
    char *p = runtime_path(".names");
    FILE *f = p ? fopen(p, "r") : NULL;
    if (!f)
        return;
    char line[80];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (*line) {
            DBG("assumindo o cursor \"%s\", no tamanho normal", line);
            restore_name(line, NULL);
        }
    }
    fclose(f);
    XFlush(dpy);
}

/* Prepara o nome: a imagem de antes (cur, que passa a ser dele) e a do
 * tema em tamanho grande. false (cur intacto) se o tema não tem a imagem
 * que está na tela. */
static bool named_setup(struct named *nm, const char *name, struct image *cur, int maxsize)
{
    if (!theme_matches(name, cur) || !theme_cursor(name, maxsize, &nm->big))
        return false;
    snprintf(nm->name, sizeof nm->name, "%s", name);
    nm->orig = *cur;
    *cur = (struct image){0};
    nm->last_h = 0;
    return true;
}

/* Aplica ao nome o quadro na escala dada (1 = tamanho normal). */
static void apply_frame(struct named *nm, double scale)
{
    const struct image *src = &nm->big;
    int h = (int)lround(nm->orig.h * scale);
    if (h < 1 || h == nm->last_h)
        return;
    double r = (double)h / src->h;
    int w = (int)lround(src->w * r);
    if (w < 1)
        return;
    uint32_t *buf = malloc(sizeof(uint32_t) * (size_t)w * h);
    if (!buf)
        return;
    scale_image(src, w, h, buf);
    Cursor c = cursor_from(buf, w, h, (int)lround(src->xhot * r), (int)lround(src->yhot * r));
    free(buf);
    if (c != None) {
        replace_named(c, nm->name);
        nm->last_h = h;
    }
}

/* Escala da animação no instante t: cresce em grow-ms, fica enquanto a
 * sacudida continua (até a duração mais 0,8 s), volta em return-ms; < 0 no
 * fim. */
static double anim_scale(long t, long start, long hold_until, double maxs)
{
    double k;
    long grow = cfg.grow_ms > 0 ? cfg.grow_ms : 1, ret = cfg.return_ms > 0 ? cfg.return_ms : 1;
    if (t < start + grow)
        k = ease((double)(t - start) / grow);
    else if (t < hold_until)
        k = 1;
    else if (t < hold_until + ret)
        k = 1 - ease((double)(t - hold_until) / ret);
    else
        return -1;
    return 1 + (maxs - 1) * k;
}

/* A sacudida continua: mantém o ponteiro grande por mais 0,5 s. */
static void extend_hold(struct detector *det, long t, long start, long *hold_until)
{
    if (!det || t < start + cfg.grow_ms || t >= *hold_until + cfg.return_ms)
        return;
    Window rr, cr;
    int px, py, wx, wy;
    unsigned int mask;
    XQueryPointer(dpy, root, &rr, &cr, &px, &py, &wx, &wy, &mask);
    if (detector_sample(det, px, py, t, (mask & (Button1Mask | Button2Mask | Button3Mask)) != 0)) {
        long nh = t + 500;
        if (nh > start + cfg.duration_ms + 800)
            nh = start + cfg.duration_ms + 800;
        if (nh > *hold_until)
            *hold_until = nh;
    }
}

static void frame_sleep(void)
{
    struct timespec ts = {0, 16 * 1000000L};
    nanosleep(&ts, NULL);
}

/* Cursor com nome: o servidor desenha o cursor grande. Toma posse de cur. */
static void big_pointer_server(struct detector *det, struct image *cur, const char *name, double maxs, int maxsize)
{
    struct named nm[MAX_NAMES];
    memset(nm, 0, sizeof nm);
    if (!named_setup(&nm[0], name, cur, maxsize))
        return;
    int n = 1;
    note_owned(nm[0].name);
    /* o ponteiro pode passar por cima de outro cursor (texto, mão...) */
    XFixesSelectCursorInput(dpy, root, XFixesDisplayCursorNotifyMask);
    DBG("ponteiro grande no servidor: cursor \"%s\" %dx%d, escala até %.1f", name, nm[0].orig.w, nm[0].orig.h,
        maxs);

    long start = now_ms(), hold_until = start + cfg.duration_ms;
    while (!quit) {
        long t = now_ms();
        double scale = anim_scale(t, start, hold_until, maxs);
        if (scale < 0)
            break;
        for (int i = 0; i < n; i++)
            apply_frame(&nm[i], scale);
        XFlush(dpy);
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type != xfixes_event + XFixesCursorNotify || n >= MAX_NAMES)
                continue;
            XFixesCursorNotifyEvent *ce = (XFixesCursorNotifyEvent *)&ev;
            char *cn = ce->cursor_name != None ? XGetAtomName(dpy, ce->cursor_name) : NULL;
            bool known = !cn || !*cn || strlen(cn) >= sizeof nm[n].name;
            for (int i = 0; i < n && !known; i++)
                known = !strcmp(nm[i].name, cn);
            struct image now = {0};
            char now_name[128] = "";
            /* a imagem do novo cursor, como está na tela agora */
            if (!known && current_cursor(&now, now_name, sizeof now_name) && !strcmp(now_name, cn) &&
                named_setup(&nm[n], cn, &now, maxsize)) {
                note_owned(nm[n].name);
                n++;
                DBG("cursor \"%s\" também ampliado", cn);
            }
            image_free(&now);
            if (cn)
                XFree(cn);
        }
        extend_hold(det, t, start, &hold_until);
        frame_sleep();
    }
    XFixesSelectCursorInput(dpy, root, 0);
    for (int i = 0; i < n; i++) {
        restore_name(nm[i].name, &nm[i].orig);
        image_free(&nm[i].big);
        image_free(&nm[i].orig);
    }
    XSync(dpy, False);
}

/* Cursor sem nome: janela que segue o ponteiro, cursor real escondido. A
 * janela anda a cada movimento do mouse (XI_RawMotion), não só a cada
 * quadro; quando o cursor muda por baixo dela, a imagem também muda. */
static void big_pointer_overlay(struct detector *det, struct image *cur, double maxs)
{
    create_overlay();
    DBG("ponteiro grande em janela: cursor %dx%d, escala até %.1f, %s", cur->w, cur->h, maxs,
        composited ? "com compositor" : "XShape");
    bool had_motion = xi_selected;
    select_motion(true);
    XFixesSelectCursorInput(dpy, root, XFixesDisplayCursorNotifyMask);
    XFixesHideCursor(dpy, root);
    bool mapped = false;
    long start = now_ms(), hold_until = start + cfg.duration_ms, last_frame = 0;
    int last_w = -1, last_x = INT_MIN, last_y = INT_MIN;
    uint32_t *buf = NULL;
    struct pollfd pfd = {.fd = ConnectionNumber(dpy), .events = POLLIN};
    while (!quit) {
        long t = now_ms();
        double scale = anim_scale(t, start, hold_until, maxs);
        if (scale < 0)
            break;
        int w = (int)lround(cur->w * scale), h = (int)lround(cur->h * scale);
        if (w < 1 || h < 1)
            break;
        int xhot = (int)lround(cur->xhot * scale), yhot = (int)lround(cur->yhot * scale);
        Window rr, cr;
        int px, py, wx, wy;
        unsigned int mask;
        XQueryPointer(dpy, root, &rr, &cr, &px, &py, &wx, &wy, &mask);
        if (w != last_w) {
            free(buf);
            buf = malloc(sizeof(uint32_t) * (size_t)w * h);
            if (!buf)
                break;
            scale_image(cur, w, h, buf);
            XMoveResizeWindow(dpy, overlay, px - xhot, py - yhot, (unsigned)w, (unsigned)h);
            if (!mapped) {
                XMapRaised(dpy, overlay);
                mapped = true;
            }
            paint(buf, w, h);
            last_w = w;
            XRaiseWindow(dpy, overlay);
        } else if (px - xhot != last_x || py - yhot != last_y) {
            XMoveWindow(dpy, overlay, px - xhot, py - yhot);
        }
        last_x = px - xhot;
        last_y = py - yhot;
        XFlush(dpy);
        if (t - last_frame >= 16) {
            extend_hold(det, t, start, &hold_until);
            last_frame = t;
        }
        /* espera o próximo movimento ou o próximo quadro */
        if (!XPending(dpy))
            poll(&pfd, 1, 16);
        bool changed = false;
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == xfixes_event + XFixesCursorNotify)
                changed = true;
        }
        if (changed) {
            struct image now = {0};
            char name[128];
            if (current_cursor(&now, name, sizeof name)) {
                image_free(cur);
                *cur = now;
                last_w = -1; /* refaz a imagem */
                DBG("o cursor mudou por baixo da janela: %dx%d \"%s\"", cur->w, cur->h, name);
            }
        }
    }
    free(buf);
    if (mapped)
        XUnmapWindow(dpy, overlay);
    XFixesSelectCursorInput(dpy, root, 0);
    XFixesShowCursor(dpy, root);
    if (!had_motion)
        select_motion(false);
    XFlush(dpy);
}

static void show_big_pointer(struct detector *det)
{
    struct image cur = {0};
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
    if (!force_overlay && *name && strlen(name) < sizeof(((struct named *)0)->name) &&
        theme_matches(name, &cur)) {
        big_pointer_server(det, &cur, name, maxs, maxsize);
    } else {
        if (*name)
            DBG("cursor \"%s\": imagem do programa, não a do tema", name);
        big_pointer_overlay(det, &cur, maxs);
    }
    image_free(&cur);
}

/* Um daemon por display: trava no arquivo .pid em $XDG_RUNTIME_DIR. */
static int lock_fd = -1;

static bool take_lock(void)
{
    char *p = runtime_path(".pid");
    if (!p)
        return true;
    int fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0)
        return true;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%d\n", (int)getpid());
    if (ftruncate(fd, 0) != 0 || write(fd, buf, (size_t)n) != n)
        DBG("não gravou %s", p);
    lock_fd = fd;
    return true;
}

/* PID do daemon deste display, 0 se não há; -1 sem $XDG_RUNTIME_DIR. */
static pid_t running_daemon(void)
{
    char *p = runtime_path(".pid");
    if (!p)
        return -1;
    int fd = open(p, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    pid_t pid = 0;
    if (flock(fd, LOCK_SH | LOCK_NB) != 0) {
        char buf[32] = "";
        if (read(fd, buf, sizeof buf - 1) > 0)
            pid = (pid_t)atoi(buf);
    }
    close(fd);
    return pid > 0 ? pid : 0;
}

static int run_daemon(bool once)
{
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "big-wm-locate-pointer: sem display X11\n");
        return 1;
    }
    root = DefaultRootWindow(dpy);
    XSetErrorHandler(ignore_errors);
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
    xfixes_event = fev;
    a_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    a_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    a_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    char cm[32];
    snprintf(cm, sizeof cm, "_NET_WM_CM_S%d", DefaultScreen(dpy));
    a_cm = XInternAtom(dpy, cm, False);

    /* SIGTERM/SIGINT no meio da animação: termina o quadro e devolve o
     * cursor normal antes de sair */
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    if (once) {
        /* sem daemon: a janela, que não troca o dono de cursor nenhum */
        force_overlay = true;
        config_load(&cfg);
        cfg.enabled = true;
        show_big_pointer(NULL);
        XCloseDisplay(dpy);
        return 0;
    }

    config_load(&cfg);
    restore_leftovers();
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
        if (show_now) {
            /* --show (menu, atalho): mostra agora */
            show_now = 0;
            show_big_pointer(NULL);
            detector_reset(&det);
        }
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
        /* um sinal durante a animação já foi atendido: não dorme */
        if (quit || show_now || reload)
            continue;
        if (!XPending(dpy) && poll(&pfd, 1, -1) < 0 && errno != EINTR)
            break;
    }
    XCloseDisplay(dpy);
    return 0;
}

/* Sessão Wayland (o XWayland tem DISPLAY, mas o ponteiro é do compositor).
 * Só WAYLAND_DISPLAY conta: XDG_SESSION_TYPE pode ter vindo de outra sessão. */
static bool wayland_session(void)
{
    const char *w = getenv("WAYLAND_DISPLAY");
    return w && *w;
}

/* O valor de uma variável no ambiente do processo pid ("" sem ela). */
static void environ_of(pid_t pid, const char *var, char *out, size_t len)
{
    char path[64], buf[16384];
    *out = 0;
    snprintf(path, sizeof path, "/proc/%d/environ", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    size_t vl = strlen(var);
    for (size_t i = 0; i < n; i += strlen(buf + i) + 1)
        if (!strncmp(buf + i, var, vl) && buf[i + vl] == '=') {
            snprintf(out, len, "%s", buf + i + vl + 1);
            return;
        }
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
    init_i18n();
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
        bool wayland = wayland_session();
        const char *wd = getenv("WAYLAND_DISPLAY");
        /* o daemon em execução relê a configuração; no Wayland (o wlmaker
         * relê sozinho), um daemon antigo deste display, pelo XWayland, é
         * encerrado: ele desenharia um segundo ponteiro */
        for (int i = 0; i < n; i++) {
            char their[128];
            environ_of(pids[i], "WAYLAND_DISPLAY", their, sizeof their);
            if (wayland && *their && wd && !strcmp(their, wd))
                kill(pids[i], SIGTERM);
            else
                kill(pids[i], SIGHUP);
        }
        const char *msg = on ? _("Shake the mouse to see the big pointer.")
                             : _("The pointer no longer grows when the mouse is shaken.");
        const char *title = on ? _("Locate pointer: on") : _("Locate pointer: off");
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
        pid_t d = running_daemon();
        if (!wayland && on && (d < 0 ? n == 0 : d == 0) && session && fork() == 0) {
            setsid();
            config_load(&cfg);
            _exit(take_lock() ? run_daemon(false) : 0);
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--show") && wayland_session()) {
        fprintf(stderr, "big-wm-locate-pointer: no Wayland, o wlmaker mostra o ponteiro (ação LocatePointer)\n");
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "--show")) {
        /* o daemon deste display mostra (é ele quem pode trocar cursores) */
        pid_t d = running_daemon();
        if (d > 0 && kill(d, SIGUSR1) == 0)
            return 0;
        return run_daemon(true);
    }
    if (argc > 1) {
        fprintf(stderr, "uso: big-wm-locate-pointer [--show | --enable | --disable | --toggle | --selftest]\n");
        return 2;
    }
    /* no Wayland, quem amplia o ponteiro é o wlmaker */
    if (wayland_session()) {
        DBG("sessão Wayland: o wlmaker localiza o ponteiro");
        return 0;
    }
    /* uma instância por display (sem $XDG_RUNTIME_DIR, por usuário) */
    if (running_daemon() < 0 ? others(pids, 16) > 0 : !take_lock())
        return 0;
    config_load(&cfg);
    if (!cfg.enabled)
        return 0;
    return run_daemon(false);
#else
    return 0;
#endif
}
