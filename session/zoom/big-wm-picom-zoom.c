/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-picom-zoom.so — ampliação da tela (acessibilidade) na sessão X11,
 * como plugin do picom (o compositor da sessão).
 *
 *   Ctrl+Alt+rolagem para cima    amplia
 *   Ctrl+Alt+rolagem para baixo   reduz
 *   Ctrl+Alt+0                    volta ao normal
 *
 * Carregado por big-wm-picom (picom --plugins ...). Sem os modificadores, a
 * rolagem continua indo para os programas: só a combinação é capturada
 * (grab passivo na raiz, também com Num Lock e Caps Lock ligados).
 *
 * Como amplia: o picom 13 desenha cada quadro numa imagem própria (inteira,
 * com o controle de dano dele) e copia a parte que mudou para o back buffer
 * com copy_area_quantize. O plugin troca essa cópia, enquanto ampliado, por
 * duas operações do próprio backend do picom em cada monitor: copia a área
 * vista para uma imagem do tamanho do monitor e a desenha ampliada (blit com
 * escala) no back buffer. A imagem do picom continua sem ampliação e com o
 * controle de dano dele. É a GPU (ou o XRender) do compositor que amplia:
 * sem capturas de tela, sem janela por cima, com qualquer janela, menus e
 * painéis incluídos.
 *
 * Ponteiro e entrada: a área vista é proporcional à posição do ponteiro no
 * monitor (origem = ponteiro * (1 - 1/nível)). Assim o que está sob o
 * ponteiro aparece exatamente onde o ponteiro está: o cursor do servidor X
 * aponta para o mesmo conteúdo, e os cliques chegam ao lugar certo sem
 * tradução de coordenadas (o X11 não redireciona entrada). O cursor em si
 * não é ampliado.
 *
 * Custo: em repouso, nada (o plugin só acorda com a roda capturada; o
 * XInput2 RawMotion é assinado só enquanto há ampliação). Com ampliação, um
 * movimento do mouse pede um quadro ao picom (uma PropertyNotify na raiz: o
 * picom redesenha a cada evento) e o quadro lê a posição do ponteiro.
 *
 * Configuração: ~/.config/big-wm/zoom.conf (ou /usr/share/big-wm/zoom.conf)
 *   enabled=true|false       os atalhos de ampliação
 *   max-level=8              ampliação máxima (1 a 16)
 *   step=1.25                fator por entalhe da roda (1.05 a 4)
 *   animation-ms=120         tempo para deslizar ao novo nível (0 a 2000)
 * Lida quando o picom começa.
 *
 * Limitações: só com o picom compondo a tela (com unredir-if-possible, uma
 * janela em tela cheia sem nada por cima é mostrada sem o compositor e sem a
 * ampliação); o backend xrender amplia sem suavizar (pixels em blocos).
 */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef ZOOM_SELFTEST
#include <ev.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xinput.h>

/* Cabeçalhos da API de plugins do picom 13 (MPL-2.0), copiados em picom/:
 * o pacote do picom não instala o types.h. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
#include "picom/api.h"
#include "picom/backend.h"
#pragma GCC diagnostic pop
#endif

/* ---- configuração ------------------------------------------------------- */

struct zoom_config {
    bool enabled;
    double max_level;
    double step;
    double animation_ms;
};

static void config_defaults(struct zoom_config *c)
{
    c->enabled = true;
    c->max_level = 8.0;
    c->step = 1.25;
    c->animation_ms = 120;
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

/* Lê "chave=valor" de f; valores fora da faixa ficam com o padrão. */
static void config_read(struct zoom_config *c, FILE *f)
{
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (*s == '#' || !*s)
            continue;
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1), *end = NULL;
        double d = strtod(v, &end);
        bool num = end && end != v && !*end && isfinite(d);
        if (!strcmp(k, "enabled"))
            c->enabled = strcmp(v, "false") && strcmp(v, "0") && strcmp(v, "no");
        else if (!strcmp(k, "max-level") && num && d >= 1 && d <= 16)
            c->max_level = d;
        else if (!strcmp(k, "step") && num && d >= 1.05 && d <= 4)
            c->step = d;
        else if (!strcmp(k, "animation-ms") && num && d >= 0 && d <= 2000)
            c->animation_ms = d;
    }
}

#ifndef ZOOM_SELFTEST
static void config_load(struct zoom_config *c)
{
    config_defaults(c);
    char path[4096];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/big-wm/zoom.conf", xdg);
    else
        snprintf(path, sizeof path, "%s/.config/big-wm/zoom.conf", home ? home : ".");
    FILE *f = fopen(path, "r");
    if (!f)
        f = fopen("/usr/share/big-wm/zoom.conf", "r");
    if (f) {
        config_read(c, f);
        fclose(f);
    }
}
#endif

/* ---- contas (as mesmas do wlmaker, src/backend/zoom.c) ------------------ */

/* Novo nível alvo depois de rolar `steps` entalhes: alvo * passo^steps,
 * entre 1 e o máximo; o finzinho perto de 1 vira 1 exato. */
static double level_scrolled(double target, double steps, double step, double max)
{
    double t = target * pow(step, steps);
    if (!isfinite(t))
        t = target;
    t = fmin(fmax(1.0, t), fmax(1.0, max));
    if (t < 1.0 + 1e-3)
        t = 1.0;
    return t;
}

/* Avança o nível mostrado em direção ao alvo (exponencial no logaritmo: cada
 * entalhe parece igual; ~95% do caminho no tempo da animação) e encosta no
 * alvo a 0,5% dele. */
static double level_advance(double level, double target, double elapsed_ms, double anim_ms)
{
    if (anim_ms <= 0 || level <= 0)
        return target;
    if (elapsed_ms <= 0)
        return level;
    double k = 1.0 - exp(-3.0 * elapsed_ms / anim_ms);
    double l = exp(log(level) + (log(target) - log(level)) * k);
    if (fabs(l - target) <= 0.005 * target)
        return target;
    return l;
}

/* Origem da área vista num monitor w x h (coordenadas do monitor): o ponto
 * sob o ponteiro (px, py) continua sob ele; a área fica dentro do monitor. */
static void view_origin(double w, double h, double level, double px, double py,
                        double *ox, double *oy)
{
    level = fmax(1.0, level);
    px = fmin(fmax(0.0, px), w);
    py = fmin(fmax(0.0, py), h);
    *ox = fmin(fmax(0.0, px * (1.0 - 1.0 / level)), w - w / level);
    *oy = fmin(fmax(0.0, py * (1.0 - 1.0 / level)), h - h / level);
}

#ifdef ZOOM_SELFTEST
/* ---- testes das contas (sem X): cc -DZOOM_SELFTEST ... ----------------- */

static int failures;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            failures++;                                                        \
        }                                                                      \
    } while (0)

int main(void)
{
    /* O conteúdo sob o ponteiro aparece onde o ponteiro está. */
    const double levels[] = {1.0, 1.5, 2.0, 3.7, 8.0};
    const double pts[][2] = {{0, 0}, {960, 540}, {1919, 1079}, {100, 1000}, {1500, 7}};
    for (size_t l = 0; l < 5; l++)
        for (size_t p = 0; p < 5; p++) {
            double ox, oy;
            view_origin(1920, 1080, levels[l], pts[p][0], pts[p][1], &ox, &oy);
            CHECK(fabs(ox + pts[p][0] / levels[l] - pts[p][0]) < 1e-9);
            CHECK(fabs(oy + pts[p][1] / levels[l] - pts[p][1]) < 1e-9);
            CHECK(ox >= 0 && ox + 1920 / levels[l] <= 1920 + 1e-9);
            CHECK(oy >= 0 && oy + 1080 / levels[l] <= 1080 + 1e-9);
        }
    /* Fora do monitor: presa à borda. */
    double ox, oy;
    view_origin(1000, 800, 2.0, -40, 5000, &ox, &oy);
    CHECK(ox == 0 && oy == 400);
    view_origin(1000, 800, 0.5, 300, 300, &ox, &oy);
    CHECK(ox == 0 && oy == 0);

    /* Rolagem: multiplica pelo passo, com limites; meio entalhe duas vezes
     * é um entalhe. */
    CHECK(fabs(level_scrolled(1.0, 1, 1.25, 8) - 1.25) < 1e-12);
    CHECK(fabs(level_scrolled(2.5, -1, 1.25, 8) - 2.0) < 1e-12);
    CHECK(fabs(level_scrolled(level_scrolled(1, 0.5, 1.25, 8), 0.5, 1.25, 8) - 1.25) < 1e-12);
    CHECK(level_scrolled(1.0, -3, 1.25, 8) == 1.0);
    CHECK(level_scrolled(7.0, 5, 1.25, 8) == 8.0);
    CHECK(level_scrolled(1.0005, -0.001, 1.25, 8) == 1.0);

    /* Animação: sobe sem passar do alvo e chega exata, em ~2x o tempo. */
    CHECK(level_advance(1.0, 2.0, 16, 0) == 2.0);
    double l = 1.0;
    int frames = 0;
    for (; l != 2.0 && frames < 1000; frames++) {
        double n = level_advance(l, 2.0, 16, 120);
        CHECK(n > l && n <= 2.0);
        l = n;
    }
    CHECK(l == 2.0 && frames <= 16);
    for (frames = 0; l != 1.0 && frames < 1000; frames++)
        l = level_advance(l, 1.0, 16, 120);
    CHECK(l == 1.0);

    /* Configuração: valores válidos entram, inválidos ficam no padrão. */
    struct zoom_config c;
    config_defaults(&c);
    const char *text = "# comentário\nenabled = false\nmax-level=4\nstep=1.5\n"
                       "animation-ms=0\n";
    FILE *f = fmemopen((void *)text, strlen(text), "r");
    config_read(&c, f);
    fclose(f);
    CHECK(!c.enabled && c.max_level == 4 && c.step == 1.5 && c.animation_ms == 0);
    config_defaults(&c);
    text = "max-level=0.5\nstep=1\nanimation-ms=abc\nenabled=true\n";
    f = fmemopen((void *)text, strlen(text), "r");
    config_read(&c, f);
    fclose(f);
    CHECK(c.enabled && c.max_level == 8 && c.step == 1.25 && c.animation_ms == 120);

    if (failures)
        return 1;
    printf("ok: big-wm-picom-zoom (contas e configuração)\n");
    return 0;
}

#else /* !ZOOM_SELFTEST */

/* ---- estado do plugin --------------------------------------------------- */

#define MAX_MONITORS 16

struct monitor {
    int x, y, w, h;
    double px, py;       /* última posição do ponteiro no monitor */
    image_handle image;  /* área vista, antes de ampliar (w x h) */
};

static struct {
    struct zoom_config cfg;

    backend_t *backend;
    struct backend_operations orig;
    struct ev_loop *loop;

    xcb_connection_t *c;
    xcb_window_t root;
    xcb_atom_t atom;
    uint8_t xi_opcode;
    uint8_t randr_event;
    bool has_randr;
    bool motion_selected;
    xcb_keycode_t key_zero;
    ev_io io;
    ev_timer anim;

    double level, target;
    double last_ms;
    bool frame_requested;
    int unzoom_frames;
    uint32_t serial;

    int root_w, root_h;
    struct monitor mon[MAX_MONITORS];
    int nmon;
} z = {.level = 1.0, .target = 1.0};

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static bool zooming(void)
{
    return z.level > 1.0 || z.target > 1.0;
}

/* Pede um quadro ao picom: ele redesenha a cada evento X que recebe, e uma
 * propriedade na raiz é um evento barato. Um pedido por quadro. */
static void request_frame(void)
{
    if (z.frame_requested || !z.c)
        return;
    z.frame_requested = true;
    z.serial++;
    xcb_change_property(z.c, XCB_PROP_MODE_REPLACE, z.root, z.atom, XCB_ATOM_CARDINAL,
                        32, 1, &z.serial);
    xcb_flush(z.c);
}

/* O RawMotion só é assinado enquanto há ampliação: em repouso, sem zoom, o
 * plugin não acorda com o mouse. */
static void update_motion_selection(void)
{
    bool want = zooming();
    if (want == z.motion_selected || !z.xi_opcode)
        return;
    struct {
        xcb_input_event_mask_t head;
        uint32_t mask;
    } m = {
        .head = {.deviceid = XCB_INPUT_DEVICE_ALL_MASTER, .mask_len = 1},
        .mask = want ? XCB_INPUT_XI_EVENT_MASK_RAW_MOTION : 0,
    };
    xcb_input_xi_select_events(z.c, z.root, 1, &m.head);
    xcb_flush(z.c);
    z.motion_selected = want;
}

static void release_monitor_images(void)
{
    for (int i = 0; i < z.nmon; i++) {
        if (z.mon[i].image && z.backend)
            z.orig.release_image(z.backend, z.mon[i].image);
        z.mon[i].image = NULL;
    }
}

/* Monitores (RandR 1.5) e tamanho da raiz. Sem RandR: a raiz inteira. */
static void read_monitors(void)
{
    release_monitor_images();
    struct monitor old[MAX_MONITORS];
    int nold = z.nmon;
    memcpy(old, z.mon, sizeof old);
    z.nmon = 0;

    xcb_get_geometry_reply_t *g = xcb_get_geometry_reply(z.c, xcb_get_geometry(z.c, z.root), NULL);
    if (g) {
        z.root_w = g->width;
        z.root_h = g->height;
        free(g);
    }
    if (z.has_randr) {
        xcb_randr_get_monitors_reply_t *r =
            xcb_randr_get_monitors_reply(z.c, xcb_randr_get_monitors(z.c, z.root, 1), NULL);
        if (r) {
            xcb_randr_monitor_info_iterator_t it = xcb_randr_get_monitors_monitors_iterator(r);
            for (; it.rem && z.nmon < MAX_MONITORS; xcb_randr_monitor_info_next(&it)) {
                struct monitor *m = &z.mon[z.nmon];
                *m = (struct monitor){.x = it.data->x, .y = it.data->y,
                                      .w = it.data->width, .h = it.data->height};
                if (m->w <= 0 || m->h <= 0)
                    continue;
                m->px = m->w / 2.0;
                m->py = m->h / 2.0;
                for (int i = 0; i < nold; i++)
                    if (old[i].x == m->x && old[i].y == m->y && old[i].w == m->w &&
                        old[i].h == m->h) {
                        m->px = old[i].px;
                        m->py = old[i].py;
                    }
                z.nmon++;
            }
            free(r);
        }
    }
    /* Um monitor que contém outro (o "screen" de um Xvfb dividido com
     * --setmonitor, ou uma divisão virtual de um monitor) e cópias exatas
     * (espelhamento) saem: cada pedaço da tela é desenhado uma vez. */
    for (int i = 0; i < z.nmon; i++) {
        struct monitor *a = &z.mon[i];
        bool drop = false;
        for (int j = 0; j < z.nmon && !drop; j++) {
            struct monitor *b = &z.mon[j];
            if (i == j)
                continue;
            bool contains = a->x <= b->x && a->y <= b->y && a->x + a->w >= b->x + b->w &&
                            a->y + a->h >= b->y + b->h;
            bool same = a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h;
            drop = same ? j < i : contains;
        }
        if (drop) {
            memmove(a, a + 1, (size_t)(z.nmon - i - 1) * sizeof *a);
            z.nmon--;
            i--;
        }
    }
    if (!z.nmon && z.root_w > 0 && z.root_h > 0) {
        z.mon[0] = (struct monitor){.w = z.root_w, .h = z.root_h,
                                    .px = z.root_w / 2.0, .py = z.root_h / 2.0};
        z.nmon = 1;
    }
}

static void level_changed(void)
{
    update_motion_selection();
    request_frame();
}

static void anim_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
    (void)revents;
    double now = now_ms();
    z.level = level_advance(z.level, z.target, now - z.last_ms, z.cfg.animation_ms);
    z.last_ms = now;
    if (z.level == z.target)
        ev_timer_stop(loop, w);
    level_changed();
}

static void set_target(double target)
{
    if (target == z.target)
        return;
    if (z.level == z.target)
        z.last_ms = now_ms();
    z.target = target;
    if (z.cfg.animation_ms <= 0) {
        z.level = target;
    } else if (z.loop && !ev_is_active(&z.anim)) {
        ev_timer_set(&z.anim, 0, 1.0 / 60.0);
        ev_timer_start(z.loop, &z.anim);
    }
    level_changed();
}

static void handle_event(xcb_generic_event_t *ev)
{
    uint8_t type = ev->response_type & 0x7f;
    if (type == XCB_BUTTON_PRESS) {
        xcb_button_press_event_t *b = (xcb_button_press_event_t *)ev;
        if (b->detail == 4)
            set_target(level_scrolled(z.target, 1, z.cfg.step, z.cfg.max_level));
        else if (b->detail == 5)
            set_target(level_scrolled(z.target, -1, z.cfg.step, z.cfg.max_level));
    } else if (type == XCB_KEY_PRESS) {
        if (((xcb_key_press_event_t *)ev)->detail == z.key_zero)
            set_target(1.0);
    } else if (type == XCB_GE_GENERIC) {
        xcb_ge_generic_event_t *ge = (xcb_ge_generic_event_t *)ev;
        if (ge->extension == z.xi_opcode && ge->event_type == XCB_INPUT_RAW_MOTION && zooming())
            request_frame();
    } else if (type == XCB_CONFIGURE_NOTIFY ||
               (z.has_randr && type == z.randr_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY)) {
        read_monitors();
        request_frame();
    }
}

static void io_cb(struct ev_loop *loop, ev_io *w, int revents)
{
    (void)loop;
    (void)w;
    (void)revents;
    xcb_generic_event_t *ev;
    while ((ev = xcb_poll_for_event(z.c))) {
        handle_event(ev);
        free(ev);
    }
    if (xcb_connection_has_error(z.c)) {
        fprintf(stderr, "big-wm-zoom: conexão com o X perdida; ampliação desligada\n");
        ev_io_stop(loop, w);
        ev_timer_stop(loop, &z.anim);
        z.level = z.target = 1.0;
    }
}

/* Captura só Ctrl+Alt (com e sem Num Lock e Caps Lock) da roda e do 0. */
static void grab_keys(void)
{
    static const uint16_t locks[] = {0, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2,
                                     XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2};
    const uint16_t mods = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1;

    xcb_key_symbols_t *syms = xcb_key_symbols_alloc(z.c);
    if (syms) {
        xcb_keycode_t *kc = xcb_key_symbols_get_keycode(syms, 0x30 /* XK_0 */);
        if (kc) {
            z.key_zero = kc[0];
            free(kc);
        }
        xcb_key_symbols_free(syms);
    }
    bool failed = false;
    for (size_t i = 0; i < sizeof locks / sizeof locks[0]; i++) {
        for (uint8_t button = 4; button <= 5; button++) {
            xcb_generic_error_t *e = xcb_request_check(
                z.c, xcb_grab_button_checked(z.c, 0, z.root, XCB_EVENT_MASK_BUTTON_PRESS,
                                             XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                                             XCB_NONE, XCB_NONE, button, mods | locks[i]));
            failed |= e != NULL;
            free(e);
        }
        if (z.key_zero) {
            xcb_generic_error_t *e = xcb_request_check(
                z.c, xcb_grab_key_checked(z.c, 0, z.root, mods | locks[i], z.key_zero,
                                          XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC));
            failed |= e != NULL;
            free(e);
        }
    }
    if (failed)
        fprintf(stderr, "big-wm-zoom: outro programa já usa Ctrl+Alt+rolagem ou "
                        "Ctrl+Alt+0; esses atalhos podem não funcionar\n");
}

static bool x_setup(void)
{
    z.c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(z.c)) {
        fprintf(stderr, "big-wm-zoom: sem conexão com o X\n");
        xcb_disconnect(z.c);
        z.c = NULL;
        return false;
    }
    z.root = xcb_setup_roots_iterator(xcb_get_setup(z.c)).data->root;

    static const char name[] = "_BIG_WM_ZOOM";
    xcb_intern_atom_reply_t *a =
        xcb_intern_atom_reply(z.c, xcb_intern_atom(z.c, 0, sizeof name - 1, name), NULL);
    z.atom = a ? a->atom : XCB_ATOM_NONE;
    free(a);

    const xcb_query_extension_reply_t *xi = xcb_get_extension_data(z.c, &xcb_input_id);
    if (xi && xi->present) {
        xcb_input_xi_query_version_reply_t *v = xcb_input_xi_query_version_reply(
            z.c, xcb_input_xi_query_version(z.c, 2, 0), NULL);
        if (v && v->major_version >= 2)
            z.xi_opcode = xi->major_opcode;
        free(v);
    }
    const xcb_query_extension_reply_t *rr = xcb_get_extension_data(z.c, &xcb_randr_id);
    if (rr && rr->present) {
        xcb_randr_query_version_reply_t *v =
            xcb_randr_query_version_reply(z.c, xcb_randr_query_version(z.c, 1, 5), NULL);
        if (v && (v->major_version > 1 || v->minor_version >= 5)) {
            z.has_randr = true;
            z.randr_event = rr->first_event;
            xcb_randr_select_input(z.c, z.root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
        }
        free(v);
    }
    uint32_t mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(z.c, z.root, XCB_CW_EVENT_MASK, &mask);
    if (!z.atom || !z.xi_opcode)
        fprintf(stderr, "big-wm-zoom: sem XInput2; a área ampliada só acompanha "
                        "o ponteiro quando a tela muda\n");

    grab_keys();
    read_monitors();
    xcb_flush(z.c);
    return true;
}

/* ---- o quadro ----------------------------------------------------------- */

static bool full_copy(backend_t *b, ivec2 origin, image_handle target, image_handle source)
{
    region_t full;
    pixman_region32_init_rect(&full, 0, 0, (unsigned)z.root_w, (unsigned)z.root_h);
    bool ok = z.orig.copy_area_quantize(b, origin, target, source, &full);
    pixman_region32_fini(&full);
    return ok;
}

/* Desenha um monitor ampliado: a área vista (com a fração de pixel) vai para
 * a imagem do monitor e dela, ampliada, para o back buffer, recortada ao
 * monitor. Só blit: o copy_area com origem trata a região de jeitos
 * diferentes no XRender e no GL. As coordenadas ficam pequenas (o XRender
 * usa 16 bits). */
static bool draw_monitor(backend_t *b, struct monitor *m, image_handle target, image_handle source)
{
    if (!m->image) {
        m->image = z.orig.new_image(b, BACKEND_IMAGE_FORMAT_PIXMAP,
                                    (ivec2){.width = m->w, .height = m->h});
        if (!m->image)
            return false;
    }
    double ox, oy;
    view_origin(m->w, m->h, z.level, m->px, m->py, &ox, &oy);
    double sx = m->x + ox, sy = m->y + oy;
    int sx0 = (int)floor(sx), sy0 = (int)floor(sy);
    int vw = (int)ceil(m->w / z.level) + 1, vh = (int)ceil(m->h / z.level) + 1;
    vw = vw > m->w ? m->w : vw;
    vh = vh > m->h ? m->h : vh;

    region_t view;
    pixman_region32_init_rect(&view, 0, 0, (unsigned)vw, (unsigned)vh);
    struct backend_blit_args copy = {
        .source_image = source,
        .target_mask = &view,
        .tint = {1, 1, 1, 1},
        .max_brightness = 1,
        .scale = {.x = 1.0, .y = 1.0},
        .effective_size = {.width = z.root_w, .height = z.root_h},
    };
    bool ok = z.orig.blit(b, (ivec2){.x = -sx0, .y = -sy0}, m->image, &copy);
    pixman_region32_fini(&view);
    if (!ok)
        return false;

    region_t clip;
    pixman_region32_init_rect(&clip, m->x, m->y, (unsigned)m->w, (unsigned)m->h);
    struct backend_blit_args args = {
        .source_image = m->image,
        .target_mask = &clip,
        .tint = {1, 1, 1, 1},
        .max_brightness = 1,
        .scale = {.x = z.level, .y = z.level},
        .effective_size = {.width = vw, .height = vh},
    };
    ivec2 at = {.x = m->x - (int)lround((sx - sx0) * z.level),
                .y = m->y - (int)lround((sy - sy0) * z.level)};
    ok = z.orig.blit(b, at, target, &args);
    pixman_region32_fini(&clip);
    return ok;
}

/* No lugar de copy_area_quantize(back buffer <- imagem do picom). */
static bool zoom_copy_area_quantize(backend_t *b, ivec2 origin, image_handle target,
                                    image_handle source, const region_t *region)
{
    z.frame_requested = false;
    bool to_back = z.orig.back_buffer && target == z.orig.back_buffer(b) &&
                   origin.x == 0 && origin.y == 0;
    if (!to_back || z.level <= 1.0 || z.nmon == 0) {
        if (to_back && z.unzoom_frames > 0) {
            /* Os back buffers ainda têm quadros ampliados: inteiros. */
            z.unzoom_frames--;
            return full_copy(b, origin, target, source);
        }
        return z.orig.copy_area_quantize(b, origin, target, source, region);
    }

    /* A posição do ponteiro, agora (o quadro segue o mouse sem atraso). */
    xcb_query_pointer_reply_t *p =
        xcb_query_pointer_reply(z.c, xcb_query_pointer(z.c, z.root), NULL);
    for (int i = 0; p && i < z.nmon; i++) {
        struct monitor *m = &z.mon[i];
        if (p->root_x >= m->x && p->root_x < m->x + m->w && p->root_y >= m->y &&
            p->root_y < m->y + m->h) {
            m->px = p->root_x - m->x;
            m->py = p->root_y - m->y;
        }
    }
    free(p);

    for (int i = 0; i < z.nmon; i++) {
        if (!draw_monitor(b, &z.mon[i], target, source)) {
            fprintf(stderr, "big-wm-zoom: o backend recusou a ampliação; quadro normal\n");
            return full_copy(b, origin, target, source);
        }
    }
    int age = z.orig.max_buffer_age ? z.orig.max_buffer_age(b) : 2;
    z.unzoom_frames = (age > 0 ? age : 2) + 1;
    return true;
}

static void zoom_deinit(backend_t *b)
{
    release_monitor_images();
    void (*deinit)(backend_t *) = z.orig.deinit;
    z.backend = NULL;
    if (deinit)
        deinit(b);
}

/* Chamado pelo picom depois de iniciar o backend (de novo a cada reinício
 * do backend, ex.: ao mudar o tamanho da tela). */
static void plugin_entry(backend_t *backend, void *user_data)
{
    (void)user_data;
    if (!z.cfg.enabled)
        return;
    if (!backend->ops.copy_area_quantize || !backend->ops.copy_area || !backend->ops.blit ||
        !backend->ops.new_image || !backend->ops.release_image || !backend->ops.back_buffer) {
        fprintf(stderr, "big-wm-zoom: backend do picom sem as operações necessárias\n");
        return;
    }
    z.backend = backend;
    z.orig = backend->ops;
    backend->ops.copy_area_quantize = zoom_copy_area_quantize;
    backend->ops.deinit = zoom_deinit;
    for (int i = 0; i < z.nmon; i++)
        z.mon[i].image = NULL;
    z.unzoom_frames = 0;

    if (!z.c && !x_setup())
        return;
    if (z.loop != backend->loop) {
        if (z.loop) {
            ev_io_stop(z.loop, &z.io);
            ev_timer_stop(z.loop, &z.anim);
        }
        z.loop = backend->loop;
        ev_io_init(&z.io, io_cb, xcb_get_file_descriptor(z.c), EV_READ);
        ev_io_start(z.loop, &z.io);
        ev_init(&z.anim, anim_cb);
        if (z.level != z.target)
            set_target(z.target);
    }
    read_monitors();
    request_frame();
}

/* Fraco: num picom sem a API de plugins, o plugin não faz nada em vez de
 * derrubar o compositor. */
#pragma weak picom_api_get_interfaces

__attribute__((constructor)) static void plugin_init(void)
{
    config_load(&z.cfg);
    if (!z.cfg.enabled)
        return;
    if (!picom_api_get_interfaces) {
        fprintf(stderr, "big-wm-zoom: este picom não tem a API de plugins\n");
        return;
    }
    const struct picom_api *api =
        picom_api_get_interfaces(PICOM_API_MAJOR, PICOM_API_MINOR, "big-wm-zoom");
    if (!api) {
        fprintf(stderr, "big-wm-zoom: versão do picom incompatível\n");
        return;
    }
    static const char *const backends[] = {"xrender", "glx", "egl"};
    for (size_t i = 0; i < sizeof backends / sizeof backends[0]; i++)
        api->add_backend_plugin(backends[i], PICOM_BACKEND_MAJOR, PICOM_BACKEND_MINOR,
                                plugin_entry, NULL);
}

#endif /* ZOOM_SELFTEST */
