/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-lockscreen: a tela de bloqueio do Big WindowMaker no estilo
 * OPENSTEP. Um binário, quatro papéis (pelo nome com que é chamado):
 *
 *   big-wm-lockscreen background [--x11 | --ppm ARQ] SAÍDA.png
 *       Monta o fundo antes de bloquear: a área de trabalho capturada
 *       (X11, ou a PPM do grim no Wayland), desfocada de verdade e
 *       escurecida; ou o fundo preto clássico, o cinza OPENSTEP, o papel de
 *       parede escurecido ou uma imagem, conforme ~/.config/big-wm/lock.conf.
 *       A captura bruta nunca vai para o disco: só a imagem desfocada, em
 *       tamanho reduzido, num arquivo 0600 do $XDG_RUNTIME_DIR (memória),
 *       apagado pelo big-wm-lock no desbloqueio.
 *   saver_big_wm   (módulo de tela do xsecurelock, um por monitor)
 *       Fundo e a janela de sessão bloqueada, com o relógio.
 *   auth_big_wm    (módulo de autenticação do xsecurelock)
 *       A mesma janela com o campo de senha, em cada monitor: a senha
 *       aparece só como sinais (PasswordMask: bolinhas, quadradinhos,
 *       asteriscos... ou nada) e vai para o authproto_pam do xsecurelock,
 *       que fala com o PAM. Aqui não há autenticação própria: o processo só
 *       desenha, recebe as teclas que o xsecurelock repassa e conversa com o
 *       helper. Um buffer só, digitado de qualquer monitor.
 *       Teclado na tela (botão ⌨ no campo, ou aberto de saída com tela de
 *       toque): o do big-keyboard (lock-osk.c), desenhado aqui sob o
 *       diálogo de cada monitor; lê só cliques e movimento do ponteiro
 *       (XInput2 raw) e escreve no mesmo buffer da senha; Cancelar e
 *       Desbloquear funcionam com o mouse ou o toque. Um controle físico
 *       (big-wm-gamepad.c, só joysticks do udev) move um cursor pelas
 *       teclas (direcional) ou aponta com os analógicos (gatilhos apertam):
 *       A aperta, X apaga, Y espaço, LB Shift, RB layout, Start
 *       desbloqueia, B fecha o teclado.
 *       O saver_big_wm também lê o controle: o primeiro botão acorda o
 *       xsecurelock (SIGUSR2, o despertar remoto dele), que abre este
 *       diálogo; sem isso, só uma tecla ou o mouse o abririam.
 *   big-wm-lockscreen preview SAÍDA.png [LxA]
 *       Imagem de exemplo para as preferências: papel de parede, nunca a
 *       tela capturada, e nenhuma senha.
 *
 * O xsecurelock cuida do que é crítico (grabs, todas as saídas de vídeo,
 * monitores conectados durante o bloqueio, suspensão pelo xss-lock): um
 * erro aqui nunca desbloqueia a tela, só esconde o diálogo.
 */
#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include "big-wm-gamepad.h"
#include "lock-osk.h"

#include <cairo-xlib.h>
#include <cairo.h>
#include <dirent.h>
#include <errno.h>
#include <fontconfig/fontconfig.h>
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <langinfo.h>
#include <libintl.h>
#include <libgen.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ---- configuração -------------------------------------------------------- */

enum { BG_BLUR = 0, BG_BLACK, BG_GRAY, BG_WALLPAPER, BG_IMAGE };

/* PasswordMask: o sinal de cada caractere digitado (o mesmo no gtklock). */
enum { MASK_SQUARE = 0, MASK_DOT, MASK_BULLET, MASK_ASTERISK, MASK_QUESTION, MASK_DIAMOND, MASK_NONE };

static int
parse_mask(const char *v)
{
    static const char *const names[] = { "square", "dot", "bullet", "asterisk", "question", "diamond", "none" };
    for (int i = 0; i < (int)(sizeof names / sizeof *names); i++)
        if (!strcmp(v, names[i]))
            return i;
    return MASK_DOT; /* desconhecido: o padrão */
}

typedef struct {
    int background;
    char image[1024];
    int blur;   /* 1..10: raio do desfoque */
    int dim;    /* 0..90: escurecimento em % */
    int show_logo, show_clock, show_date, show_user, show_caps, show_layout;
    /* Teclado na tela (o mesmo lock.conf do gtklock no Wayland). */
    int vk;         /* VirtualKeyboard: o teclado existe */
    int vk_button;  /* VirtualKeyboardButton: o botão ⌨ no campo de senha */
    int mask;       /* PasswordMask (MASK_*) */
    int vk_layout;  /* KeyboardLayout: índice do layout do big-keyboard; -1 auto */
    double vk_size; /* KeyboardSize: small 0.85, medium 1, large 1.3 */
    int vk_dark;    /* KeyboardTheme=dark|light; -1: o modo do Big WindowMaker */
    int vk_touch;   /* KeyboardTouch: aberto de saída com tela de toque */
} lockcfg;

static const unsigned GRAY_OPENSTEP = 0x555555;

static int
parse_bool(const char *v, int def)
{
    if (!strcasecmp(v, "true") || !strcmp(v, "1") || !strcasecmp(v, "yes"))
        return 1;
    if (!strcasecmp(v, "false") || !strcmp(v, "0") || !strcasecmp(v, "no"))
        return 0;
    return def;
}

/* O modo do Big WindowMaker (cores do painel; ver "cores" mais abaixo). */
static int mode_dark;
static int theme_dark(void);

static void
cfg_load(lockcfg *c)
{
    mode_dark = theme_dark();
    *c = (lockcfg){ .mask = MASK_DOT, .background = BG_BLUR, .blur = 4, .dim = 45, .show_logo = 1, .show_clock = 1,
                    .show_date = 1, .show_user = 1, .show_caps = 1, .show_layout = 1,
                    .vk = 1, .vk_button = 1, .vk_layout = -1, .vk_size = 1, .vk_touch = 1, .vk_dark = -1 };
    char path[1100];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/big-wm/lock.conf", xdg);
    else
        snprintf(path, sizeof path, "%s/.config/big-wm/lock.conf", home ? home : "");
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[1200];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == '[')
            continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "Background"))
            c->background = !strcmp(v, "black") ? BG_BLACK : !strcmp(v, "gray") ? BG_GRAY
                            : !strcmp(v, "wallpaper") ? BG_WALLPAPER : !strcmp(v, "image") ? BG_IMAGE : BG_BLUR;
        else if (!strcmp(k, "Image"))
            snprintf(c->image, sizeof c->image, "%s", v);
        else if (!strcmp(k, "Blur"))
            c->blur = atoi(v) < 1 ? 1 : atoi(v) > 10 ? 10 : atoi(v);
        else if (!strcmp(k, "Dim"))
            c->dim = atoi(v) < 0 ? 0 : atoi(v) > 90 ? 90 : atoi(v);
        else if (!strcmp(k, "ShowLogo"))
            c->show_logo = parse_bool(v, 1);
        else if (!strcmp(k, "ShowClock"))
            c->show_clock = parse_bool(v, 1);
        else if (!strcmp(k, "ShowDate"))
            c->show_date = parse_bool(v, 1);
        else if (!strcmp(k, "ShowUser"))
            c->show_user = parse_bool(v, 1);
        else if (!strcmp(k, "ShowCapsLock"))
            c->show_caps = parse_bool(v, 1);
        else if (!strcmp(k, "ShowKeyboardLayout"))
            c->show_layout = parse_bool(v, 1);
        else if (!strcmp(k, "VirtualKeyboard"))
            c->vk = parse_bool(v, 1);
        else if (!strcmp(k, "VirtualKeyboardButton"))
            c->vk_button = parse_bool(v, 1);
        else if (!strcmp(k, "KeyboardLayout"))
            c->vk_layout = lock_osk_layout_from_conf(v);
        else if (!strcmp(k, "PasswordMask"))
            c->mask = parse_mask(v);
        else if (!strcmp(k, "KeyboardSize"))
            c->vk_size = !strcmp(v, "small") ? 0.85 : !strcmp(v, "large") ? 1.3 : 1;
        else if (!strcmp(k, "KeyboardTheme"))
            c->vk_dark = !strcmp(v, "dark") ? 1 : !strcmp(v, "light") ? 0 : -1;
        else if (!strcmp(k, "KeyboardTouch"))
            c->vk_touch = parse_bool(v, 1);
    }
    fclose(f);
}

/* ---- textos ------------------------------------------------------------- */

/* Catálogo "big-wm" (po/ na raiz do big-wm), msgids em inglês; o espanhol
 * e o português vêm dos catálogos.
 * BIGWM_LOCALEDIR: catálogos compilados fora de /usr/share/locale (testes). */
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif
#define N_(s) (s)

enum {
    T_LOCKED, T_PASSWORD, T_HINT, T_CHECKING, T_WRONG, T_CAPS, T_LAYOUT, T_CANCEL, T_UNLOCK,
    T_WAIT, T_KEYBOARD, T_DATE, T_N
};

static const char *const texts[T_N] = {
    N_("Session locked"), N_("Password:"), N_("Type your password"),
    N_("Checking..."), N_("Wrong password"), N_("Caps Lock is on"), N_("Keyboard"), N_("Cancel"),
    N_("Unlock"), N_("Please wait..."), N_("On-screen keyboard"), N_("Date"),
};

/* Traduzidos uma vez, na partida (antes do primeiro desenho). */
static const char *tr_texts[T_N];

static void
init_i18n(void)
{
    const char *dir = getenv("BIGWM_LOCALEDIR");
    bindtextdomain("big-wm", dir && *dir ? dir : LOCALEDIR);
    bind_textdomain_codeset("big-wm", "UTF-8");
    for (int i = 0; i < T_N; i++)
        tr_texts[i] = dgettext("big-wm", texts[i]);
}

#define TR(k) (tr_texts[k])

/* ---- imagens ------------------------------------------------------------- */

static cairo_surface_t *
surface_from_pixbuf(GdkPixbuf *pb)
{
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int nch = gdk_pixbuf_get_n_channels(pb), rs = gdk_pixbuf_get_rowstride(pb);
    const guchar *px = gdk_pixbuf_read_pixels(pb);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    unsigned char *d = cairo_image_surface_get_data(s);
    int ds = cairo_image_surface_get_stride(s);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const guchar *p = px + y * rs + x * nch;
            unsigned a = nch == 4 ? p[3] : 255;
            uint32_t v = a << 24 | (p[0] * a / 255) << 16 | (p[1] * a / 255) << 8 | (p[2] * a / 255);
            *(uint32_t *)(d + y * ds + x * 4) = v;
        }
    cairo_surface_mark_dirty(s);
    return s;
}

/* Qualquer formato que o gdk-pixbuf leia (JPEG dos papéis de parede). */
static cairo_surface_t *
load_image(const char *path)
{
    if (!path || !*path)
        return NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, NULL);
    if (!pb)
        return NULL;
    cairo_surface_t *s = surface_from_pixbuf(pb);
    g_object_unref(pb);
    return s;
}

/* PPM binária (P6), como "grim -t ppm" escreve. */
static cairo_surface_t *
load_ppm(FILE *f)
{
    int w, h, maxv;
    if (fscanf(f, "P6 %d %d %d", &w, &h, &maxv) != 3 || w <= 0 || h <= 0 || w > 32768 || h > 32768 ||
        maxv != 255)
        return NULL;
    fgetc(f);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    unsigned char *d = cairo_image_surface_get_data(s);
    int ds = cairo_image_surface_get_stride(s);
    unsigned char *row = malloc((size_t)w * 3);
    if (!row) {
        cairo_surface_destroy(s);
        return NULL;
    }
    for (int y = 0; y < h; y++) {
        if (fread(row, 3, (size_t)w, f) != (size_t)w) {
            free(row);
            cairo_surface_destroy(s);
            return NULL;
        }
        for (int x = 0; x < w; x++)
            *(uint32_t *)(d + y * ds + x * 4) =
                (uint32_t)row[x * 3] << 16 | (uint32_t)row[x * 3 + 1] << 8 | row[x * 3 + 2];
    }
    free(row);
    cairo_surface_mark_dirty(s);
    return s;
}

/* A janela raiz inteira (todas as saídas). */
static cairo_surface_t *
capture_x11(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
        return NULL;
    int scr = DefaultScreen(dpy);
    int w = DisplayWidth(dpy, scr), h = DisplayHeight(dpy, scr);
    cairo_surface_t *xs = cairo_xlib_surface_create(dpy, RootWindow(dpy, scr), DefaultVisual(dpy, scr), w, h);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_surface(cr, xs, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(xs);
    XCloseDisplay(dpy);
    return s;
}

/* Cópia reduzida por factor, com o filtro bom do cairo (média das áreas). */
static cairo_surface_t *
shrink(cairo_surface_t *src, double factor)
{
    int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
    int sw = (int)lround(w * factor), sh = (int)lround(h * factor);
    sw = sw < 1 ? 1 : sw;
    sh = sh < 1 ? 1 : sh;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, sw, sh);
    cairo_t *cr = cairo_create(s);
    cairo_scale(cr, (double)sw / w, (double)sh / h);
    cairo_set_source_surface(cr, src, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint(cr);
    cairo_destroy(cr);
    return s;
}

/* Uma passada de média móvel (caixa) de raio r numa direção; três
 * passadas de caixa nas duas direções dão um gaussiano (teorema central do
 * limite), sem o quadriculado de reduzir e ampliar. O(n) por passada. */
static void
box_pass(uint32_t *px, int w, int h, int stride_px, int r, int horizontal, uint32_t *tmp)
{
    int n = horizontal ? w : h, lines = horizontal ? h : w;
    int step = horizontal ? 1 : stride_px, lstep = horizontal ? stride_px : 1;
    for (int l = 0; l < lines; l++) {
        uint32_t *base = px + (size_t)l * lstep;
        long sr = 0, sg = 0, sb = 0;
        int win = 2 * r + 1;
        for (int i = -r; i <= r; i++) {
            int k = i < 0 ? 0 : i >= n ? n - 1 : i;
            uint32_t v = base[(size_t)k * step];
            sr += v >> 16 & 0xff;
            sg += v >> 8 & 0xff;
            sb += v & 0xff;
        }
        for (int i = 0; i < n; i++) {
            tmp[i] = (uint32_t)(sr / win) << 16 | (uint32_t)(sg / win) << 8 | (uint32_t)(sb / win);
            int out = i - r < 0 ? 0 : i - r, in = i + r + 1 >= n ? n - 1 : i + r + 1;
            uint32_t vo = base[(size_t)out * step], vi = base[(size_t)in * step];
            sr += (long)(vi >> 16 & 0xff) - (vo >> 16 & 0xff);
            sg += (long)(vi >> 8 & 0xff) - (vo >> 8 & 0xff);
            sb += (long)(vi & 0xff) - (vo & 0xff);
        }
        for (int i = 0; i < n; i++)
            base[(size_t)i * step] = 0xff000000u | tmp[i];
    }
}

static void
gaussian(cairo_surface_t *s, int radius)
{
    if (radius < 1)
        return;
    cairo_surface_flush(s);
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int stride = cairo_image_surface_get_stride(s) / 4;
    uint32_t *px = (uint32_t *)cairo_image_surface_get_data(s);
    uint32_t *tmp = malloc(sizeof *tmp * (size_t)(w > h ? w : h));
    if (!tmp)
        return;
    for (int pass = 0; pass < 3; pass++) {
        box_pass(px, w, h, stride, radius, 1, tmp);
        box_pass(px, w, h, stride, radius, 0, tmp);
    }
    free(tmp);
    cairo_surface_mark_dirty(s);
}

static void
darken(cairo_surface_t *s, int dim)
{
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgba(cr, 0x10 / 255.0, 0x12 / 255.0, 0x14 / 255.0, dim / 100.0);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static cairo_surface_t *
solid(unsigned rgb)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 16, 9);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, (rgb >> 16 & 0xff) / 255.0, (rgb >> 8 & 0xff) / 255.0, (rgb & 0xff) / 255.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return s;
}

static int
wallpaper_path(char *out, size_t len)
{
    char path[1100];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/big-wm/wallpaper", xdg);
    else
        snprintf(path, sizeof path, "%s/.config/big-wm/wallpaper", home ? home : "");
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(out, (int)len, f)) {
            out[strcspn(out, "\r\n\t")] = 0;
            fclose(f);
            if (*out && access(out, R_OK) == 0)
                return 1;
        } else {
            fclose(f);
        }
    }
    snprintf(out, len, "/usr/share/backgrounds/big-wm/default.jpg");
    return access(out, R_OK) == 0;
}

static int
write_private_png(cairo_surface_t *s, const char *out)
{
    mode_t old = umask(077);
    cairo_status_t st = cairo_surface_write_to_png(s, out);
    umask(old);
    return st == CAIRO_STATUS_SUCCESS ? 0 : -1;
}

/* Fundo processado, em quarto de resolução (quem desenha amplia com o filtro
 * bom do cairo: o desfoque já tirou os detalhes, nada fica quadriculado). */
static cairo_surface_t *
make_background(const lockcfg *c, cairo_surface_t *shot)
{
    if (c->background == BG_BLACK)
        return solid(0x000000);
    if (c->background == BG_GRAY)
        return solid(GRAY_OPENSTEP);
    cairo_surface_t *src = shot;
    int owned = 0;
    char cache[1200] = "";
    if (c->background == BG_WALLPAPER || c->background == BG_IMAGE || !src) {
        char wp[1024];
        if (c->background == BG_IMAGE && *c->image)
            snprintf(wp, sizeof wp, "%s", c->image);
        else if (!wallpaper_path(wp, sizeof wp))
            return solid(0x202a2e);
        /* O papel de parede quase nunca muda: o fundo pronto fica num cache
         * na memória ($XDG_RUNTIME_DIR, só do usuário). Decodificar o JPEG
         * pelo gdk-pixbuf (glycin, num processo isolado) leva uns 250 ms;
         * do cache, poucos. */
        struct stat st;
        const char *rt = getenv("XDG_RUNTIME_DIR");
        if (rt && *rt && stat(wp, &st) == 0) {
            unsigned long h = 1469598103934665603UL;
            char key[1400];
            snprintf(key, sizeof key, "%s|%ld|%ld|%d|%d|%d", wp, (long)st.st_mtime, (long)st.st_size,
                     c->background == BG_BLUR, c->dim, c->blur);
            for (const char *k = key; *k; k++)
                h = (h ^ (unsigned char)*k) * 1099511628211UL;
            snprintf(cache, sizeof cache, "%s/big-wm-lock/wallpaper-%016lx.png", rt, h);
            cairo_surface_t *cached = cairo_image_surface_create_from_png(cache);
            if (cairo_surface_status(cached) == CAIRO_STATUS_SUCCESS)
                return cached;
            cairo_surface_destroy(cached);
        }
        src = load_image(wp);
        owned = 1;
        if (!src)
            return solid(0x202a2e);
    }
    cairo_surface_t *small = shrink(src, 0.25);
    if (owned)
        cairo_surface_destroy(src);
    if (c->background == BG_BLUR)
        gaussian(small, c->blur * 2);
    else
        gaussian(small, 1); /* papel de parede: só suaviza a redução */
    darken(small, c->dim);
    if (*cache) {
        /* Só um cache por vez: os de outros papéis de parede saem. */
        char dir[1200], *slash;
        snprintf(dir, sizeof dir, "%s", cache);
        slash = strrchr(dir, '/');
        if (slash) {
            *slash = 0;
            mkdir(dir, 0700);
            DIR *d = opendir(dir);
            for (struct dirent *e; d && (e = readdir(d));)
                if (!strncmp(e->d_name, "wallpaper-", 10)) {
                    char old[1500];
                    snprintf(old, sizeof old, "%s/%s", dir, e->d_name);
                    unlink(old);
                }
            if (d)
                closedir(d);
        }
        write_private_png(small, cache);
    }
    return small;
}


/* ---- desenho da janela OPENSTEP ------------------------------------------- */

typedef struct {
    const lockcfg *cfg;
    const char *user;
    const char *host;
    int npw;            /* caracteres digitados (só a contagem) */
    int input;          /* campo ativo (diálogo de senha) */
    int caps;
    char layout[64];
    char message[512];  /* informação do PAM, ou erro */
    int error;
    int busy;
    struct vkbd *vk;    /* teclado na tela (só no auth) */
} panel_state;

/* ---- cores: modo claro (OPENSTEP) ou escuro -------------------------------
 *
 * O modo do Big WindowMaker (~/.config/big-wm/theme, "dark" ou "light", o
 * mesmo do Window Maker, do GTK e do Dock com Abas). O claro é o painel
 * cinza do OPENSTEP de sempre; o escuro tira as cores do BigWindowMaker-Dark
 * (ladrilho 5c/5c/64 a 1c/1c/22, menus 4a/2e, texto d8/e6). */
typedef struct {
    unsigned body, ink, ink2, hint, err, info, field, field_off, field_ink, cursor, key_hl, key_ink;
    unsigned hi, mid, etch, button, button_ink;
    double tile0[3], tile1[3];
} palette;

static const palette PAL_LIGHT = {
    .body = 0xaeaaae, .ink = 0x000000, .ink2 = 0x202020, .hint = 0x6a6a6a, .err = 0x8b0000, .info = 0x303030,
    .field = 0xffffff, .field_off = 0xe2e2e2, .field_ink = 0x1a1a1a, .cursor = 0x000000, .key_hl = 0xd0d0d0, .key_ink = 0x404040,
    .hi = 0xffffff, .mid = 0x555555, .etch = 0x606360, .button = 0xaeaaae, .button_ink = 0x000000,
    .tile0 = { 0xa6 / 255.0, 0xa6 / 255.0, 0xb6 / 255.0 }, .tile1 = { 0x51 / 255.0, 0x55 / 255.0, 0x61 / 255.0 },
};

static const palette PAL_DARK = {
    .body = 0x3c3c3c, .ink = 0xe6e6e6, .ink2 = 0xc8c8c8, .hint = 0x9a9a9a, .err = 0xff7070, .info = 0xb8b8b8,
    .field = 0x262626, .field_off = 0x2e2e2e, .field_ink = 0xe6e6e6, .cursor = 0xf0f0f0, .key_hl = 0x5a5a5a, .key_ink = 0xd0d0d0,
    .hi = 0x787878, .mid = 0x1e1e1e, .etch = 0x1e1e1e, .button = 0x4a4a4a, .button_ink = 0xe6e6e6,
    .tile0 = { 0x5c / 255.0, 0x5c / 255.0, 0x64 / 255.0 }, .tile1 = { 0x1c / 255.0, 0x1c / 255.0, 0x22 / 255.0 },
};

#define P (mode_dark ? &PAL_DARK : &PAL_LIGHT)

/* 1 no modo escuro do Big WindowMaker. */
static int
theme_dark(void)
{
    char path[1100], buf[16] = "";
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/big-wm/theme", xdg);
    else
        snprintf(path, sizeof path, "%s/.config/big-wm/theme", home ? home : "");
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(buf, sizeof buf, f))
        buf[0] = 0;
    fclose(f);
    return !strncmp(buf, "dark", 4);
}

static void
rgb(cairo_t *cr, unsigned c)
{
    cairo_set_source_rgb(cr, (c >> 16 & 0xff) / 255.0, (c >> 8 & 0xff) / 255.0, (c & 0xff) / 255.0);
}

static void
fillr(cairo_t *cr, double x, double y, double w, double h, unsigned c)
{
    rgb(cr, c);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

/* Chanfro do OpenStep: luz em cima e à esquerda, sombra embaixo e à
 * direita, com a segunda linha cinza-escura dos botões do WPrefs. */
static void
bevel(cairo_t *cr, double x, double y, double w, double h, int raised, double s)
{
    double t = s < 1.5 ? 1 : 2;
    unsigned lt = raised ? P->hi : 0x000000, dk = raised ? 0x000000 : P->hi;
    fillr(cr, x, y, w, t, lt);
    fillr(cr, x, y, t, h, lt);
    fillr(cr, x, y + h - t, w, t, dk);
    fillr(cr, x + w - t, y, t, h, dk);
    if (raised) {
        fillr(cr, x + t, y + h - 2 * t, w - 2 * t, t, P->mid);
        fillr(cr, x + w - 2 * t, y + t, t, h - 2 * t, P->mid);
    } else {
        fillr(cr, x + t, y + t, w - 2 * t, t, P->mid);
        fillr(cr, x + t, y + t, t, h - 2 * t, P->mid);
    }
}

static double
text(cairo_t *cr, const char *s, double x, double y, double size, int bold, unsigned c)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    rgb(cr, c);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
    cairo_text_extents_t te;
    cairo_text_extents(cr, s, &te);
    return te.x_advance;
}

static double
text_width(cairo_t *cr, const char *s, double size, int bold)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t te;
    cairo_text_extents(cr, s, &te);
    return te.x_advance;
}

/* Um caractere da senha, no quadro sq × sq em (x, y). */
static void
mask_glyph(cairo_t *cr, int mask, double x, double y, double sq, double s, unsigned color)
{
    double cx = x + sq / 2, cy = y + sq / 2;
    rgb(cr, color);
    switch (mask) {
    case MASK_DOT: /* ● */
        cairo_arc(cr, cx, cy, sq * 0.6, 0, 2 * M_PI);
        cairo_fill(cr);
        break;
    case MASK_BULLET: /* • */
        cairo_arc(cr, cx, cy, sq * 0.32, 0, 2 * M_PI);
        cairo_fill(cr);
        break;
    case MASK_DIAMOND: /* ◆ */
        cairo_move_to(cr, cx, cy - sq * 0.65);
        cairo_line_to(cr, cx + sq * 0.65, cy);
        cairo_line_to(cr, cx, cy + sq * 0.65);
        cairo_line_to(cr, cx - sq * 0.65, cy);
        cairo_close_path(cr);
        cairo_fill(cr);
        break;
    case MASK_ASTERISK: /* * desenhado: seis braços */
        cairo_set_line_width(cr, 1.4 * s);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        for (int i = 0; i < 3; i++) {
            double a = M_PI / 2 + i * M_PI / 3, r = sq * 0.6;
            cairo_move_to(cr, cx - r * cos(a), cy - r * sin(a));
            cairo_line_to(cr, cx + r * cos(a), cy + r * sin(a));
        }
        cairo_stroke(cr);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        break;
    case MASK_QUESTION: { /* ? */
        double fs = 13 * s;
        double w = text_width(cr, "?", fs, 1);
        text(cr, "?", cx - w / 2, cy + fs * 0.36, fs, 1, color);
        break;
    }
    case MASK_NONE:
        break;
    default: /* quadradinho */
        fillr(cr, x, y, sq, sq, color);
    }
}

static cairo_surface_t *logo_cache;
static int logo_cache_size;

static cairo_surface_t *
logo(int size)
{
    if (logo_cache && logo_cache_size == size)
        return logo_cache;
    if (logo_cache)
        cairo_surface_destroy(logo_cache);
    logo_cache = NULL;
    const char *cands[] = { "/usr/share/icons/hicolor/256x256/apps/big-wm.png",
                            "/usr/share/icons/hicolor/128x128/apps/big-wm.png" };
    for (size_t i = 0; i < 2 && !logo_cache; i++) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(cands[size > 128 ? i : 1 - i], size, size, NULL);
        if (pb) {
            logo_cache = surface_from_pixbuf(pb);
            g_object_unref(pb);
        }
    }
    logo_cache_size = size;
    return logo_cache;
}

/* Botão do WPrefs; default: o de Enter, com o símbolo de retorno. */
static void
button(cairo_t *cr, double x, double y, double w, double h, const char *label, int def, double s)
{
    fillr(cr, x, y, w, h, P->button);
    bevel(cr, x, y, w, h, 1, s);
    double fs = 12 * s;
    double tw = text_width(cr, label, fs, 0) + (def ? 16 * s : 0);
    double tx = x + (w - tw) / 2;
    text(cr, label, tx, y + h / 2 + fs * 0.36, fs, 0, P->button_ink);
    if (def) {
        /* ⏎ desenhado (a fonte pode não ter o glifo) */
        double ax = tx + tw - 10 * s, ay = y + h / 2;
        rgb(cr, P->button_ink);
        cairo_set_line_width(cr, 1.4 * s);
        cairo_move_to(cr, ax + 8 * s, ay - 5 * s);
        cairo_line_to(cr, ax + 8 * s, ay + 2 * s);
        cairo_line_to(cr, ax, ay + 2 * s);
        cairo_stroke(cr);
        cairo_move_to(cr, ax - 1 * s, ay + 2 * s);
        cairo_line_to(cr, ax + 3 * s, ay - 1.5 * s);
        cairo_line_to(cr, ax + 3 * s, ay + 5.5 * s);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

/* ---- teclado na tela (auth do X11) ----------------------------------------
 *
 * O xsecurelock prende o ponteiro e o teclado e só repassa teclas (pela
 * entrada padrão). Os cliques chegam aqui como eventos "raw" do XInput2 na
 * raiz, que o servidor entrega mesmo com o ponteiro preso por outro cliente;
 * a posição vem do XQueryPointer. Nada de injeção de eventos, de outro
 * processo ou de leitura de teclas: só botões e movimento do ponteiro são
 * pedidos. As teclas da tela entram no mesmo buffer travado da senha, pelo
 * mesmo caminho das teclas físicas. O ponteiro do xsecurelock é
 * transparente durante a senha: com o mouse em uso, desenhamos uma seta.
 *
 * O teclado é o do big-keyboard (lock-osk.c, com o núcleo dele em bk/): o
 * mesmo estado em todos os monitores, desenhado sob o diálogo de cada um. */

/* Botões do diálogo (o teclado tem a própria geometria). */
enum { VA_NONE, VA_TOGGLE, VA_CANCEL, VA_UNLOCK };

typedef struct {
    double x, y, w, h;
    int action;
} vhit;

#define VK_MAXHITS 64
#define VK_MAXMON 16

typedef struct vkbd {
    int enabled, button; /* da configuração */
    LockOsk osk;         /* o teclado; osk.shown: aberto */
    vhit hits[VK_MAXHITS];
    int nhits;
    double ox[VK_MAXMON], oy[VK_MAXMON]; /* o teclado em cada monitor (último quadro) */
    int nkb;
    int pressed;            /* botão do diálogo apertado, -1 nenhum */
    int key_kb;             /* teclado (monitor) da tecla apertada, -1 nenhum */
    long long repeat_at;    /* próxima repetição da tecla segurada, 0 nenhuma */
    int ptr_x, ptr_y, ptr_shown, ptr_drawn_x, ptr_drawn_y, ptr_drawn;
} vkbd;

static void
vk_hit(vkbd *k, double x, double y, double w, double h, int action)
{
    if (k->nhits < VK_MAXHITS)
        k->hits[k->nhits++] = (vhit){ x, y, w, h, action };
}

static int
vk_find(const vkbd *k, int x, int y)
{
    for (int i = k->nhits - 1; i >= 0; i--) {
        const vhit *h = &k->hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h)
            return i;
    }
    return -1;
}

/* Tecla do teclado na tela em (x, y): o índice, e o monitor em *kb. */
static int
vk_find_key(const vkbd *k, int x, int y, int *kb)
{
    if (!k->osk.shown)
        return -1;
    for (int i = 0; i < k->nkb; i++) {
        int idx = lock_osk_hit(&k->osk, x - k->ox[i], y - k->oy[i]);
        if (idx >= 0) {
            *kb = i;
            return idx;
        }
    }
    return -1;
}

/* Layout de saída: o do lock.conf, senão o do XKB do servidor (o
 * _XKB_RULES_NAMES da raiz: "br" primeiro é ABNT2), senão o idioma. */
static int
vk_default_layout(Display *dpy, const lockcfg *c)
{
    if (c->vk_layout >= 0)
        return c->vk_layout;
    Atom a = XInternAtom(dpy, "_XKB_RULES_NAMES", True), type;
    int fmt;
    unsigned long n = 0, after;
    unsigned char *data = NULL;
    char xkb[64] = "";
    if (a != None &&
        XGetWindowProperty(dpy, DefaultRootWindow(dpy), a, 0, 1024, False, XA_STRING, &type, &fmt, &n, &after,
                           &data) == Success && data && fmt == 8) {
        /* rules\0model\0layout\0variant\0options */
        const char *p = (const char *)data, *end = p + n;
        for (int i = 0; i < 2 && p < end; i++)
            p += strlen(p) + 1;
        if (p < end)
            snprintf(xkb, sizeof xkb, "%.*s", (int)(end - p), p);
    }
    if (data)
        XFree(data);
    return lock_osk_layout_auto(xkb, getenv("LANG"));
}

/* Ícone ⌨ do campo de senha (a fonte pode não ter o glifo). */
static void
vk_icon_keyboard(cairo_t *cr, double x, double y, double w, double h, double s, unsigned c)
{
    rgb(cr, c);
    cairo_set_line_width(cr, 1.2 * s);
    cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1, h - 1);
    cairo_stroke(cr);
    double kw = (w - 4 * s) / 6, kh = (h - 4 * s) / 3;
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 6; i++)
            cairo_rectangle(cr, x + 2 * s + i * kw + kw * 0.2, y + 2 * s + r * kh + kh * 0.2, kw * 0.6, kh * 0.6);
    cairo_rectangle(cr, x + 2 * s + kw * 1.2, y + 2 * s + 2 * kh + kh * 0.2, kw * 3.6, kh * 0.6);
    cairo_fill(cr);
}

/* O teclado centrado em cx, logo abaixo de top (ou no pé do monitor, se
 * não couber); guarda onde ficou, para os toques. */
static void
vk_draw(cairo_t *cr, vkbd *k, double cx, double top, double mon_y, double mon_h, double s)
{
    double x = round(cx - k->osk.width / 2), y = round(top + 14 * s);
    if (y + k->osk.height > mon_y + mon_h - 8 * s)
        y = round(mon_y + mon_h - 8 * s - k->osk.height);
    if (k->nkb < VK_MAXMON) {
        k->ox[k->nkb] = x;
        k->oy[k->nkb] = y;
        k->nkb++;
    }
    lock_osk_draw(&k->osk, cr, x, y, TR(T_KEYBOARD), bwg_now_ms());
}

/* Seta do ponteiro, desenhada direto na janela por cima do quadro; a área
 * anterior volta do pixmap do quadro. */
#define VK_ARROW 22

/* Altura da coluna da direita (grupo de autenticação e quadro de
 * mensagens) e do painel inteiro, em unidades (escala 1). */
static double
panel_auth_h(const lockcfg *c)
{
    return c->show_user ? 102 : 54;
}

static double
panel_h(const lockcfg *c)
{
    return 14 + 6 + panel_auth_h(c) + 10 + 44 + 20 + 30 + 14;
}

/* Grupo do WINGs (o do wdm): moldura entalhada, linha escura e linha clara
 * logo depois, com o título no meio da linha de cima. */
static void
etched(cairo_t *cr, double x, double y, double w, double h, const char *title, double s)
{
    double t = s < 1.5 ? 1 : 2;
    unsigned dk = P->etch, lt = P->hi;
    fillr(cr, x, y, w - t, t, dk);
    fillr(cr, x, y, t, h - t, dk);
    fillr(cr, x, y + h - 2 * t, w - t, t, dk);
    fillr(cr, x + w - 2 * t, y, t, h - t, dk);
    fillr(cr, x + t, y + t, w - 3 * t, t, lt);
    fillr(cr, x + t, y + t, t, h - 3 * t, lt);
    fillr(cr, x, y + h - t, w, t, lt);
    fillr(cr, x + w - t, y, t, h, lt);
    if (title && *title) {
        double fs = 12 * s, tw = text_width(cr, title, fs, 0);
        double tx = x + (w - tw) / 2;
        fillr(cr, tx - 2 * s, y - fs / 2, tw + 4 * s, fs, P->body);
        text(cr, title, tx, y + fs * 0.36, fs, 0, P->ink);
    }
}

/* Texto centrado em cx. */
static void
text_center(cairo_t *cr, const char *s, double cx, double y, double size, int bold, unsigned c)
{
    text(cr, s, cx - text_width(cr, s, size, bold) / 2, y, size, bold, c);
}

/* Centrado em cx e no máximo maxw de largura: o que passa vira "…",
 * cortado no limite de um caractere UTF-8. */
static void
text_center_fit(cairo_t *cr, const char *s, double cx, double y, double size, int bold, unsigned c,
                double maxw)
{
    if (text_width(cr, s, size, bold) <= maxw) {
        text_center(cr, s, cx, y, size, bold, c);
        return;
    }
    char buf[512];
    size_t n = strlen(s);
    if (n > sizeof buf - 4)
        n = sizeof buf - 4;
    while (n > 0) {
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80)
            n--;
        memcpy(buf, s, n);
        memcpy(buf + n, "\u2026", 4);
        if (text_width(cr, buf, size, bold) <= maxw)
            break;
        n--;
    }
    if (n == 0)
        snprintf(buf, sizeof buf, "\u2026");
    text_center(cr, buf, cx, y, size, bold, c);
}

static const char *
host_name(void)
{
    static char host[256];
    if (!host[0] && gethostname(host, sizeof host - 1) != 0)
        snprintf(host, sizeof host, "localhost");
    return host;
}

/* A janela de sessão bloqueada, centrada em (cx, cy), escala s, com o
 * desenho da tela de login (wdm): o logo num quadro afundado à esquerda;
 * à direita o grupo da sessão bloqueada (usuário, máquina e senha) e o
 * quadro das mensagens; embaixo a data, o teclado e os botões. Devolve a
 * borda de baixo. */
static double
draw_panel(cairo_t *cr, double cx, double cy, double s, const panel_state *st)
{
    const lockcfg *c = st->cfg;
    double W = 570 * s, H = panel_h(c) * s;
    double x = round(cx - W / 2), y = round(cy - H / 2);
    double pad = 14 * s, gap = 12 * s;
    /* Painel cinza, saltado, sem barra de título (como o do wdm). */
    fillr(cr, x, y, W, H, P->body);
    bevel(cr, x, y, W, H, 1, s);
    double top = y + pad + 6 * s;
    double auth_h = panel_auth_h(c) * s, col_h = auth_h + 10 * s + 44 * s;
    /* Logo num quadro afundado. */
    double rx = x + pad;
    if (c->show_logo) {
        double lw = 150 * s;
        bevel(cr, rx, top, lw, col_h, 0, s);
        int ls = (int)(112 * s);
        cairo_surface_t *lg = logo(ls);
        if (lg) {
            double iw = cairo_image_surface_get_width(lg), ih = cairo_image_surface_get_height(lg);
            cairo_save(cr);
            cairo_translate(cr, round(rx + (lw - iw) / 2), round(top + (col_h - ih) / 2));
            cairo_set_source_surface(cr, lg, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        }
        rx += lw + gap;
    }
    double rw = x + W - pad - rx, mid = rx + rw / 2;
    /* Grupo da sessão bloqueada: usuário e máquina em negrito, no meio. */
    etched(cr, rx, top, rw, auth_h, TR(T_LOCKED), s);
    double ry = top + 14 * s;
    if (c->show_user && st->user) {
        text_center_fit(cr, st->user, mid, ry + 16 * s, 16 * s, 1, P->ink, rw - 24 * s);
        ry += 24 * s;
        text_center_fit(cr, host_name(), mid, ry + 16 * s, 16 * s, 1, P->ink, rw - 24 * s);
        ry += 24 * s;
    }
    /* Campo de senha: retângulo branco afundado; cada caractere é um sinal
     * (bolinha, quadradinho, asterisco...), nunca o caractere. */
    double fh = 24 * s, fy = ry + 4 * s;
    double lw = text(cr, TR(T_PASSWORD), rx + 16 * s, fy + fh / 2 + 13 * s * 0.36, 13 * s, 1, P->ink);
    double fx = rx + 16 * s + lw + 10 * s, fw = rx + rw - 14 * s - fx;
    fillr(cr, fx, fy, fw, fh, st->input ? P->field : P->field_off);
    bevel(cr, fx, fy, fw, fh, 0, s);
    double icon_w = 0;
    vkbd *vk = st->vk;
    if (vk && vk->enabled && vk->button) {
        /* ⌨ no fim do campo, como no gtklock do Wayland. */
        double iw = 20 * s, ih = 12 * s;
        icon_w = iw + 10 * s;
        double ix = fx + fw - iw - 6 * s, iy = fy + (fh - ih) / 2;
        if (vk->osk.shown)
            fillr(cr, ix - 3 * s, fy + 3 * s, iw + 6 * s, fh - 6 * s, P->key_hl);
        vk_icon_keyboard(cr, ix, iy, iw, ih, s, P->key_ink);
        vk_hit(vk, fx + fw - icon_w - 4 * s, fy - 4 * s, icon_w + 8 * s, fh + 8 * s, VA_TOGGLE);
    }
    /* Um sinal por caractere, no formato do PasswordMask; com "none", nada
     * (nem a quantidade aparece) e o cursor fica no começo. */
    double sq = 7 * s, sgap = 5 * s;
    int maxsq = (int)((fw - 16 * s - icon_w) / (sq + sgap));
    int n = c->mask == MASK_NONE ? 0 : st->npw > maxsq ? maxsq : st->npw;
    for (int i = 0; i < n; i++)
        mask_glyph(cr, c->mask, fx + 8 * s + i * (sq + sgap), fy + (fh - sq) / 2, sq, s, P->field_ink);
    if (st->input && !st->busy) {
        /* cursor de texto */
        fillr(cr, fx + 8 * s + n * (sq + sgap), fy + 5 * s, 1.5 * s, fh - 10 * s, P->cursor);
    } else if (!st->input && st->npw == 0) {
        text(cr, TR(T_HINT), fx + 8 * s, fy + fh / 2 + 4 * s, 12 * s, 0, P->hint);
    }
    /* Quadro das mensagens (o do wdm): erro em vermelho escuro, informação
     * do PAM (leitor de digital, aviso de senha a vencer) em preto, e o
     * Caps Lock com um LED. */
    double my = top + auth_h + 10 * s, mh = 44 * s;
    etched(cr, rx, my, rw, mh, NULL, s);
    const char *msg = st->message[0] ? st->message : st->busy ? TR(T_CHECKING) : NULL;
    int caps = c->show_caps && st->caps;
    double l1 = my + mh / 2 + 4 * s, l2 = l1;
    if (msg && caps) {
        l1 = my + 18 * s;
        l2 = my + 35 * s;
    }
    if (msg)
        text_center(cr, msg, mid, l1, 12 * s, 1, st->error ? P->err : P->ink);
    if (caps) {
        double tw = text_width(cr, TR(T_CAPS), 11.5 * s, 1), lx = mid - (tw + 14 * s) / 2;
        fillr(cr, lx, l2 - 8 * s, 8 * s, 8 * s, 0xf0b030);
        bevel(cr, lx - 1 * s, l2 - 9 * s, 10 * s, 10 * s, 0, s);
        text(cr, TR(T_CAPS), lx + 14 * s, l2, 11.5 * s, 1, P->ink);
    }
    /* Embaixo, como a sessão e as opções do wdm: data e teclado em grupos,
     * e os botões (Esc e Enter) no canto. */
    double gy = top + col_h + 20 * s, gh = 30 * s, gx = x + pad;
    double bw = 100 * s, bh = 26 * s, bx = x + W - pad - bw, byy = gy + gh - bh;
    char date[256] = "";
    if (c->show_clock || c->show_date) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char t1[64] = "", t2[128] = "";
        if (c->show_clock)
            strftime(t1, sizeof t1, "%H:%M", &tm);
        if (c->show_date)
            strftime(t2, sizeof t2, "%a, %x", &tm);
        snprintf(date, sizeof date, "%s%s%s", t1, *t1 && *t2 ? "  \xc2\xb7  " : "", t2);
    }
    const char *kbd = c->show_layout && st->layout[0] ? st->layout : "";
    /* Cada grupo do tamanho do texto; o que não couber antes dos botões
     * fica cortado no grupo do teclado. */
    double fs = 12 * s, room = bx - bw - 8 * s - 10 * s - gx;
    double dw = *date ? fmax(text_width(cr, date, fs, 0), text_width(cr, TR(T_DATE), fs, 0)) + 24 * s : 0;
    double kw = *kbd ? fmax(text_width(cr, kbd, fs, 0), text_width(cr, TR(T_LAYOUT), fs, 0)) + 24 * s : 0;
    if (*date && *kbd && dw + 10 * s + kw > room)
        kw = fmax(60 * s, room - dw - 10 * s);
    if (*date) {
        etched(cr, gx, gy, dw, gh, TR(T_DATE), s);
        text_center(cr, date, gx + dw / 2, gy + gh / 2 + 5 * s, fs, 0, P->ink);
        gx += dw + 10 * s;
    }
    if (*kbd) {
        etched(cr, gx, gy, kw, gh, TR(T_LAYOUT), s);
        cairo_save(cr);
        cairo_rectangle(cr, gx + 4 * s, gy + 2 * s, kw - 8 * s, gh - 4 * s);
        cairo_clip(cr);
        double tw = text_width(cr, kbd, fs, 0);
        text(cr, kbd, tw > kw - 12 * s ? gx + 6 * s : gx + (kw - tw) / 2, gy + gh / 2 + 5 * s, fs, 0, P->ink);
        cairo_restore(cr);
    }
    button(cr, bx, byy, bw, bh, TR(T_UNLOCK), 0, s);
    button(cr, bx - bw - 8 * s, byy, bw, bh, TR(T_CANCEL), 0, s);
    if (vk) {
        vk_hit(vk, bx, byy, bw, bh, VA_UNLOCK);
        vk_hit(vk, bx - bw - 8 * s, byy, bw, bh, VA_CANCEL);
    }
    return y + H;
}

/* Fundo: a imagem (quarto de resolução) esticada sobre a raiz inteira; a
 * janela (ox, oy) mostra a parte dela que está na tela. */
static void
draw_background(cairo_t *cr, cairo_surface_t *bg, int root_w, int root_h, int ox, int oy, int w, int h)
{
    if (!bg) {
        fillr(cr, 0, 0, w, h, 0x000000);
        return;
    }
    double iw = cairo_image_surface_get_width(bg), ih = cairo_image_surface_get_height(bg);
    cairo_save(cr);
    if (iw <= 16 && ih <= 9) {
        cairo_set_source_surface(cr, bg, 0, 0);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
        cairo_paint(cr);
    } else {
        /* Cobre a raiz mantendo a proporção da imagem (papel de parede de
         * outro formato fica cortado, não deformado). */
        double sc = fmax((double)root_w / iw, (double)root_h / ih);
        cairo_translate(cr, -ox + (root_w - iw * sc) / 2, -oy + (root_h - ih * sc) / 2);
        cairo_scale(cr, sc, sc);
        cairo_set_source_surface(cr, bg, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
        cairo_paint(cr);
    }
    cairo_restore(cr);
}

/* ---- X11 (saver e auth) -------------------------------------------------- */

static double
ui_scale(Display *dpy)
{
    const char *dpi = XGetDefault(dpy, "Xft", "dpi");
    double d = dpi ? atof(dpi) : 96;
    double s = d / 96.0;
    return s < 1 ? 1 : s > 3 ? 3 : s;
}

static void
keyboard_state(Display *dpy, panel_state *st)
{
    unsigned ind = 0;
    st->caps = 0;
    Atom caps = XInternAtom(dpy, "Caps Lock", True);
    if (caps != None) {
        Bool on = False;
        if (XkbGetNamedIndicator(dpy, caps, NULL, &on, NULL, NULL))
            st->caps = on;
    } else if (XkbGetIndicatorState(dpy, XkbUseCoreKbd, &ind) == Success) {
        st->caps = ind & 1;
    }
    st->layout[0] = 0;
    XkbStateRec xs;
    XkbDescPtr kb = XkbGetKeyboard(dpy, XkbGBN_AllComponentsMask, XkbUseCoreKbd);
    if (kb && XkbGetState(dpy, XkbUseCoreKbd, &xs) == Success && kb->names && kb->names->groups[xs.group]) {
        char *n = XGetAtomName(dpy, kb->names->groups[xs.group]);
        if (n) {
            snprintf(st->layout, sizeof st->layout, "%s", n);
            XFree(n);
        }
    }
    if (kb)
        XkbFreeKeyboard(kb, 0, True);
}

static cairo_surface_t *
load_background_env(void)
{
    const char *p = getenv("BIG_WM_LOCK_BACKGROUND");
    if (!p || !*p)
        return NULL;
    cairo_surface_t *s = cairo_image_surface_create_from_png(p);
    if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(s);
        return NULL;
    }
    return s;
}

static volatile sig_atomic_t quit_flag, redraw_flag;

static void
on_term(int sig)
{
    (void)sig;
    quit_flag = 1;
}

static void
on_usr1(int sig)
{
    (void)sig;
    redraw_flag = 1;
}

typedef struct {
    Display *dpy;
    Window win;
    int w, h, ox, oy, root_w, root_h;
    Pixmap buf;
    cairo_surface_t *bg;
    double scale;
} xview;

/* A janela que o xsecurelock nos deu sumiu (os monitores mudaram e ele
 * refez as janelas do saver, ou acabou o bloqueio): o trabalho deste
 * processo acabou. Sai sem o "X Error of failed request" do tratador
 * padrão, e sempre com falha: um módulo de autenticação que sai com 0
 * desbloqueia a tela. Os outros erros seguem para o tratador padrão. */
static XErrorHandler default_x_error;

static int
window_gone(Display *dpy, XErrorEvent *e)
{
    if (e->error_code == BadWindow || e->error_code == BadDrawable)
        _exit(EXIT_FAILURE);
    return default_x_error(dpy, e);
}

static int
xview_open(xview *v, int create_child)
{
    const char *ws = getenv("XSCREENSAVER_WINDOW");
    v->dpy = XOpenDisplay(NULL);
    if (!v->dpy || !ws)
        return -1;
    default_x_error = XSetErrorHandler(window_gone);
    Window parent = (Window)strtoul(ws, NULL, 0);
    XWindowAttributes a;
    if (!XGetWindowAttributes(v->dpy, parent, &a))
        return -1;
    v->w = a.width;
    v->h = a.height;
    Window child;
    XTranslateCoordinates(v->dpy, parent, a.root, 0, 0, &v->ox, &v->oy, &child);
    v->root_w = DisplayWidth(v->dpy, DefaultScreen(v->dpy));
    v->root_h = DisplayHeight(v->dpy, DefaultScreen(v->dpy));
    if (create_child) {
        /* O xsecurelock cria a janela do auth sem mapear: quem mapeia é o
         * módulo (como o auth_x11). Sem isso o diálogo fica escondido atrás
         * do saver e a senha é digitada às cegas. */
        XMapRaised(v->dpy, parent);
        /* Uma janela nossa sobre a do xsecurelock: some com o processo. */
        XSetWindowAttributes sa = { .background_pixel = BlackPixel(v->dpy, DefaultScreen(v->dpy)),
                                    .event_mask = ExposureMask | StructureNotifyMask };
        v->win = XCreateWindow(v->dpy, parent, 0, 0, (unsigned)v->w, (unsigned)v->h, 0, CopyFromParent, InputOutput,
                               CopyFromParent, CWBackPixel | CWEventMask, &sa);
        XMapRaised(v->dpy, v->win);
    } else {
        v->win = parent;
        XSelectInput(v->dpy, v->win, ExposureMask | StructureNotifyMask);
    }
    v->buf = XCreatePixmap(v->dpy, v->win, (unsigned)v->w, (unsigned)v->h, (unsigned)a.depth);
    v->bg = load_background_env();
    v->scale = ui_scale(v->dpy);
    return 0;
}

/* Desenha o quadro no pixmap e copia de uma vez (sem piscar). */
static void
xview_draw(xview *v, const panel_state *st, int panels_per_monitor)
{
    XWindowAttributes a;
    XGetWindowAttributes(v->dpy, v->win, &a);
    cairo_surface_t *xs = cairo_xlib_surface_create(v->dpy, v->buf, a.visual, v->w, v->h);
    cairo_t *cr = cairo_create(xs);
    draw_background(cr, v->bg, v->root_w, v->root_h, v->ox, v->oy, v->w, v->h);
    vkbd *vk = st->vk;
    if (vk)
        vk->nhits = vk->nkb = 0;
    int kb = vk && vk->enabled && vk->osk.shown;
    if (panels_per_monitor) {
        /* Diálogo na janela da raiz inteira: um por monitor, cada um com o
         * seu teclado; dá para digitar e tocar em qualquer um (a senha é
         * uma só, no buffer deste processo). */
        int nm = 0;
        XRRMonitorInfo *m = XRRGetMonitors(v->dpy, DefaultRootWindow(v->dpy), True, &nm);
        for (int i = 0; i < nm; i++) {
            double cx = m[i].x - v->ox + m[i].width / 2.0, cy = m[i].y - v->oy + m[i].height / 2.0;
            if (kb) {
                /* Diálogo e teclado juntos no meio do monitor. */
                double ph = panel_h(st->cfg) * v->scale, kh = vk->osk.height + 14 * v->scale;
                double top = m[i].y - v->oy + (m[i].height - ph - kh) / 2;
                cy = fmax(m[i].y - v->oy + 8 * v->scale, top) + ph / 2;
            }
            double bottom = draw_panel(cr, cx, cy, v->scale, st);
            if (kb)
                vk_draw(cr, vk, cx, bottom, m[i].y - v->oy, m[i].height, v->scale);
        }
        if (m)
            XRRFreeMonitors(m);
        if (nm <= 0) {
            double bottom = draw_panel(cr, v->w / 2.0, v->h / 2.0, v->scale, st);
            if (kb)
                vk_draw(cr, vk, v->w / 2.0, bottom, 0, v->h, v->scale);
        }
    } else {
        draw_panel(cr, v->w / 2.0, v->h / 2.0, v->scale, st);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(xs);
    GC gc = XCreateGC(v->dpy, v->win, 0, NULL);
    XCopyArea(v->dpy, v->buf, v->win, gc, 0, 0, (unsigned)v->w, (unsigned)v->h, 0, 0);
    XFreeGC(v->dpy, gc);
    XFlush(v->dpy);
}

/* Seta do ponteiro (o do xsecurelock fica transparente durante a senha). */
static void
vk_cursor(xview *v, vkbd *k)
{
    GC gc = XCreateGC(v->dpy, v->win, 0, NULL);
    int a = (int)ceil(VK_ARROW * v->scale) + 4;
    if (k->ptr_drawn)
        XCopyArea(v->dpy, v->buf, v->win, gc, k->ptr_drawn_x, k->ptr_drawn_y, (unsigned)a, (unsigned)a,
                  k->ptr_drawn_x, k->ptr_drawn_y);
    k->ptr_drawn = 0;
    if (k->ptr_shown) {
        XWindowAttributes wa;
        XGetWindowAttributes(v->dpy, v->win, &wa);
        cairo_surface_t *xs = cairo_xlib_surface_create(v->dpy, v->win, wa.visual, v->w, v->h);
        cairo_t *cr = cairo_create(xs);
        static const double pts[][2] = { { 0, 0 }, { 0, 17 }, { 4.5, 13 }, { 7.5, 20 }, { 10.5, 18.8 },
                                         { 7.6, 12 }, { 13, 12 } };
        double sc = v->scale * VK_ARROW / 21.0;
        cairo_translate(cr, k->ptr_x + 0.5, k->ptr_y + 0.5);
        cairo_move_to(cr, 0, 0);
        for (size_t i = 1; i < sizeof pts / sizeof *pts; i++)
            cairo_line_to(cr, pts[i][0] * sc, pts[i][1] * sc);
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 1.2 * v->scale);
        cairo_stroke(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(xs);
        k->ptr_drawn_x = k->ptr_x - 2;
        k->ptr_drawn_y = k->ptr_y - 2;
        k->ptr_drawn = 1;
    }
    XFreeGC(v->dpy, gc);
    XFlush(v->dpy);
}

/* Há uma tela de toque (toque direto)? */
static int
has_touchscreen(Display *dpy)
{
    int n = 0, found = 0;
    XIDeviceInfo *d = XIQueryDevice(dpy, XIAllDevices, &n);
    for (int i = 0; d && i < n && !found; i++)
        for (int c = 0; c < d[i].num_classes && !found; c++)
            if (d[i].classes[c]->type == XITouchClass &&
                ((XITouchClassInfo *)d[i].classes[c])->mode == XIDirectTouch)
                found = 1;
    if (d)
        XIFreeDeviceInfo(d);
    return found;
}

static int
seconds_to_next_minute(void)
{
    time_t now = time(NULL);
    return 60 - (int)(now % 60);
}

static const char *
user_name(void)
{
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_name : getenv("USER");
}

/* O xsecurelock acima deste saver (xsecurelock → saver_multiplex → este):
 * o processo ancestral chamado xsecurelock, do mesmo usuário. */
static pid_t
xsecurelock_pid(void)
{
    pid_t pid = getppid();
    for (int depth = 0; depth < 4 && pid > 1; depth++) {
        char path[64], comm[32] = "", line[256];
        snprintf(path, sizeof path, "/proc/%d/comm", (int)pid);
        FILE *f = fopen(path, "r");
        if (!f)
            return -1;
        if (fgets(comm, sizeof comm, f))
            comm[strcspn(comm, "\n")] = 0;
        fclose(f);
        struct stat sb;
        snprintf(path, sizeof path, "/proc/%d", (int)pid);
        if (!strcmp(comm, "xsecurelock") && stat(path, &sb) == 0 && sb.st_uid == getuid())
            return pid;
        /* o pai deste: o 4.º campo de /proc/PID/stat, depois do (comm) */
        snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
        f = fopen(path, "r");
        if (!f)
            return -1;
        pid_t ppid = -1;
        if (fgets(line, sizeof line, f)) {
            char *rp = strrchr(line, ')');
            if (rp)
                sscanf(rp + 1, " %*c %d", &ppid);
        }
        fclose(f);
        pid = ppid;
    }
    return -1;
}

static int
run_saver(void)
{
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGUSR1, on_usr1);
    xview v = { 0 };
    if (xview_open(&v, 0) != 0)
        return 1;
    lockcfg cfg;
    cfg_load(&cfg);
    panel_state st = { .cfg = &cfg, .user = user_name() };
    keyboard_state(v.dpy, &st);
    xview_draw(&v, &st, 0);
    /* Controle físico: o primeiro botão acorda o xsecurelock (SIGUSR2), que
     * abre o diálogo de senha com o teclado na tela. */
    static bwg pad;
    int have_pad = cfg.vk && bwg_open(&pad) >= 0;
    pid_t locker = have_pad ? xsecurelock_pid() : -1;
    while (!quit_flag) {
        struct pollfd p[1 + BWG_MAX_PADS + 1] = { { ConnectionNumber(v.dpy), POLLIN, 0 } };
        int pfds[BWG_MAX_PADS + 1], npfds = have_pad ? bwg_fds(&pad, pfds, BWG_MAX_PADS + 1) : 0;
        for (int i = 0; i < npfds; i++)
            p[1 + i] = (struct pollfd){ pfds[i], POLLIN, 0 };
        int r = poll(p, (nfds_t)(1 + npfds), seconds_to_next_minute() * 1000);
        if (quit_flag)
            break;
        for (int i = 0; i < npfds; i++) {
            if (!(p[1 + i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            enum bwg_action acts[BWG_MAX_ACTIONS];
            int na = bwg_handle(&pad, p[1 + i].fd, bwg_now_ms(), acts, BWG_MAX_ACTIONS);
            if (na > 0 && locker > 0)
                kill(locker, SIGUSR2);
        }
        int draw = r == 0 || redraw_flag;
        redraw_flag = 0;
        while (XPending(v.dpy)) {
            XEvent e;
            XNextEvent(v.dpy, &e);
            if (e.type == Expose && e.xexpose.count == 0)
                draw = 1;
        }
        if (draw) {
            keyboard_state(v.dpy, &st);
            xview_draw(&v, &st, 0);
        }
    }
    if (have_pad)
        bwg_close(&pad);
    XCloseDisplay(v.dpy);
    return 0;
}

/* ---- auth: conversa com o authproto do xsecurelock ------------------------- */

typedef struct {
    pid_t pid;
    int to, from; /* escrever no helper, ler do helper */
    char rbuf[4096];
    size_t rlen;
} authproto;

static int
authproto_start(authproto *a, const char *self)
{
    char dir[1024], path[1200];
    snprintf(dir, sizeof dir, "%s", self);
    const char *name = getenv("XSECURELOCK_AUTHPROTO");
    if (!name || !*name || strchr(name, '/'))
        name = "authproto_pam";
    snprintf(path, sizeof path, "%s/%s", dirname(dir), name);
    int in[2], out[2];
    if (pipe2(in, O_CLOEXEC) || pipe2(out, O_CLOEXEC))
        return -1;
    a->pid = fork();
    if (a->pid < 0)
        return -1;
    if (a->pid == 0) {
        dup2(in[0], 0);
        dup2(out[1], 1);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    a->to = in[1];
    a->from = out[0];
    a->rlen = 0;
    return 0;
}

/* Pacote do authproto: "<tipo> <tamanho>\n<mensagem>\n". */
static int
authproto_send(authproto *a, char type, const char *msg, size_t len)
{
    char hdr[32];
    int h = snprintf(hdr, sizeof hdr, "%c %zu\n", type, len);
    if (write(a->to, hdr, (size_t)h) != h)
        return -1;
    if (len && write(a->to, msg, len) != (ssize_t)len)
        return -1;
    return write(a->to, "\n", 1) == 1 ? 0 : -1;
}

/* Um pacote completo do buffer, se houver: 1 = tipo e mensagem em out. */
static int
authproto_take(authproto *a, char *type, char *out, size_t outlen)
{
    char *nl = memchr(a->rbuf, '\n', a->rlen);
    if (!nl)
        return 0;
    size_t len = 0;
    if (sscanf(a->rbuf, "%c %zu", type, &len) != 2)
        return -1;
    size_t hdr = (size_t)(nl - a->rbuf) + 1;
    if (a->rlen < hdr + len + 1)
        return 0;
    size_t n = len < outlen - 1 ? len : outlen - 1;
    memcpy(out, a->rbuf + hdr, n);
    out[n] = 0;
    memmove(a->rbuf, a->rbuf + hdr + len + 1, a->rlen - hdr - len - 1);
    a->rlen -= hdr + len + 1;
    return 1;
}

static void
authproto_stop(authproto *a)
{
    if (a->pid > 0) {
        close(a->to);
        close(a->from);
        kill(a->pid, SIGTERM);
        waitpid(a->pid, NULL, 0);
    }
    a->pid = -1;
}

static size_t
utf8_chars(const char *s, size_t len)
{
    size_t n = 0;
    for (size_t i = 0; i < len; i++)
        n += ((unsigned char)s[i] & 0xc0) != 0x80;
    return n;
}

/* O que chega para o campo de senha, de uma tecla física (pela entrada
 * padrão, do xsecurelock) ou da tela: o mesmo caminho, byte a byte. */
typedef struct {
    char *pw;
    size_t len, cap;
    panel_state *st;
    authproto *ap;
} auth_input;

static void
auth_byte(auth_input *in, unsigned char ch)
{
    panel_state *st = in->st;
    if (ch == '\r' || ch == '\n') {
        if (st->input && !st->busy) {
            authproto_send(in->ap, 'p', in->pw, in->len);
            explicit_bzero(in->pw, in->cap);
            in->len = 0;
            st->busy = 1;
            st->input = 0;
            st->message[0] = 0;
        }
    } else if (ch == 0x1b) {
        if (in->len == 0 && !st->busy)
            quit_flag = 1; /* Esc no campo vazio: cancela */
        explicit_bzero(in->pw, in->cap);
        in->len = 0;
    } else if (ch == '\b' || ch == 0x7f) {
        while (in->len > 0 && ((unsigned char)in->pw[in->len - 1] & 0xc0) == 0x80)
            in->pw[--in->len] = 0;
        if (in->len > 0)
            in->pw[--in->len] = 0;
    } else if (ch == 0x15 || ch == 0x17) {
        explicit_bzero(in->pw, in->cap);
        in->len = 0;
    } else if (ch >= 0x20 && in->len < in->cap - 1 && !st->busy) {
        in->pw[in->len++] = (char)ch;
        st->error = 0;
        if (st->message[0] && !st->input)
            st->message[0] = 0;
    }
}

static void
auth_text(auth_input *in, const char *s)
{
    for (; *s; s++)
        auth_byte(in, (unsigned char)*s);
}

/* O teclado na tela escreve pelo mesmo caminho das teclas físicas. */
static void
sink_text(void *data, const char *s)
{
    auth_text(data, s);
}

static void
sink_backspace(void *data)
{
    auth_byte(data, '\b');
}

static void
sink_enter(void *data)
{
    auth_byte(data, '\r');
}

static const LockOskSink auth_sink = { sink_text, sink_backspace, sink_enter };

/* Um botão do diálogo (⌨, Cancelar, Desbloquear). */
static void
vk_activate(vkbd *k, const vhit *h, auth_input *in)
{
    switch (h->action) {
    case VA_TOGGLE:
        k->osk.shown = !k->osk.shown;
        break;
    case VA_CANCEL:
        explicit_bzero(in->pw, in->cap);
        lock_osk_clear(&k->osk);
        in->len = 0;
        quit_flag = 1; /* como o Esc: o diálogo some, a tela continua bloqueada */
        break;
    case VA_UNLOCK:
        auth_byte(in, '\r');
        break;
    }
}

static int
run_auth(const char *self)
{
    signal(SIGTERM, on_term);
    signal(SIGPIPE, SIG_IGN);
    xview v = { 0 };
    if (xview_open(&v, 1) != 0)
        return 1;
    lockcfg cfg;
    cfg_load(&cfg);
    /* A senha fica numa página travada na memória (sem swap) e é apagada
     * assim que vai para o helper. */
    static char pw[1024];
    mlock(pw, sizeof pw);
    static vkbd vk;
    vk = (vkbd){ .enabled = cfg.vk, .button = cfg.vk_button, .pressed = -1, .key_kb = -1 };
    lock_osk_init(&vk.osk, vk_default_layout(v.dpy, &cfg), cfg.vk_dark < 0 ? mode_dark : cfg.vk_dark,
                  v.scale * cfg.vk_size);
    /* Controle físico: o mesmo teclado na tela, por um cursor (direcional)
     * ou dois (analógicos, modo apontar), como no big-keyboard. */
    static bwg pad;
    int have_pad = cfg.vk && bwg_open(&pad) >= 0;
    pad.no_stick_nav = 1;
    panel_state st = { .cfg = &cfg, .user = user_name(), .input = 0, .vk = &vk };
    keyboard_state(v.dpy, &st);
    /* Cliques e movimento do ponteiro (nunca teclas) pelo XInput2. */
    int xi_opcode = -1, xi_ev, xi_err;
    if (cfg.vk && XQueryExtension(v.dpy, "XInputExtension", &xi_opcode, &xi_ev, &xi_err)) {
        int major = 2, minor = 2;
        if (XIQueryVersion(v.dpy, &major, &minor) == Success && (major > 2 || (major == 2 && minor >= 1))) {
            unsigned char mask[XIMaskLen(XI_LASTEVENT)] = { 0 };
            XISetMask(mask, XI_RawButtonPress);
            XISetMask(mask, XI_RawButtonRelease);
            XISetMask(mask, XI_RawMotion);
            XIEventMask em = { XIAllMasterDevices, (int)sizeof mask, mask };
            XISelectEvents(v.dpy, DefaultRootWindow(v.dpy), &em, 1);
            /* Tela de toque: o teclado já aberto (KeyboardTouch). */
            if (cfg.vk_touch && has_touchscreen(v.dpy))
                vk.osk.shown = 1;
        } else {
            xi_opcode = -1;
        }
    } else {
        xi_opcode = -1;
    }
    snprintf(st.message, sizeof st.message, "%s", TR(T_WAIT));
    authproto ap = { .pid = -1 };
    if (authproto_start(&ap, self) != 0)
        return 1;
    char prompt[512] = ""; /* o último pedido do authproto */
    const char *tos = getenv("XSECURELOCK_AUTH_TIMEOUT");
    int timeout_s = tos && atoi(tos) > 0 ? atoi(tos) : 60;
    time_t last = time(NULL);
    int status = 1;
    auth_input in = { .pw = pw, .len = 0, .cap = sizeof pw, .st = &st, .ap = &ap };
    xview_draw(&v, &st, 1);
    while (!quit_flag) {
        struct pollfd p[3 + BWG_MAX_PADS + 1] = { { 0, POLLIN, 0 }, { ap.from, POLLIN, 0 },
                                                  { ConnectionNumber(v.dpy), POLLIN, 0 } };
        int pfds[BWG_MAX_PADS + 1], npfds = have_pad ? bwg_fds(&pad, pfds, BWG_MAX_PADS + 1) : 0;
        for (int i = 0; i < npfds; i++)
            p[3 + i] = (struct pollfd){ pfds[i], POLLIN, 0 };
        int wait = 1000;
        long long now = bwg_now_ms();
        if (have_pad) {
            int t = bwg_timeout(&pad, now);
            if (t >= 0 && t < wait)
                wait = t;
        }
        int flash = lock_osk_flash_ms(&vk.osk, now); /* tecla do controle afundada */
        if (flash >= 0 && flash < wait)
            wait = flash;
        if (vk.repeat_at) /* tecla segurada: repetição */
            wait = vk.repeat_at <= now ? 0 : vk.repeat_at - now < wait ? (int)(vk.repeat_at - now) : wait;
        /* Eventos que o Xlib já leu (junto com a resposta de um
         * XQueryPointer, por exemplo) não acordam o poll: sem espera. */
        int r = poll(p, (nfds_t)(3 + npfds), XEventsQueued(v.dpy, QueuedAlready) ? 0 : wait);
        if (r < 0 && errno != EINTR)
            break;
        int draw = flash == 0;
        now = bwg_now_ms();
        if (vk.repeat_at && now >= vk.repeat_at) {
            if (lock_osk_repeat(&vk.osk, &auth_sink, &in)) {
                vk.repeat_at = now + LOCK_OSK_REPEAT_RATE;
                st.npw = (int)utf8_chars(pw, in.len);
                last = time(NULL);
                draw = 1;
            } else {
                vk.repeat_at = 0;
            }
        }
        if (time(NULL) - last > timeout_s)
            break; /* sem teclas: o xsecurelock volta à tela de sessão */
        if (p[0].revents & (POLLIN | POLLHUP)) {
            char kb[64];
            ssize_t n = read(0, kb, sizeof kb);
            if (n <= 0)
                break;
            last = time(NULL);
            for (ssize_t i = 0; i < n; i++)
                auth_byte(&in, (unsigned char)kb[i]);
            explicit_bzero(kb, sizeof kb);
            st.npw = (int)utf8_chars(pw, in.len);
            keyboard_state(v.dpy, &st);
            draw = 1;
        }
        if (p[1].revents & (POLLIN | POLLHUP)) {
            ssize_t n = read(ap.from, ap.rbuf + ap.rlen, sizeof ap.rbuf - ap.rlen);
            if (n > 0)
                ap.rlen += (size_t)n;
            char type, msg[512];
            int k;
            while ((k = authproto_take(&ap, &type, msg, sizeof msg)) == 1) {
                if (type == 'P') {
                    /* Pede a senha: o campo fica ativo (sai o "Aguarde...";
                     * um "Senha incorreta" fica). O authproto_pam não sai
                     * quando a senha está errada: pede a mesma coisa de
                     * novo, sem mensagem. Um outro pedido (um segundo
                     * fator) não é erro. */
                    if (st.busy && !st.message[0] && !strcmp(msg, prompt)) {
                        snprintf(st.message, sizeof st.message, "%s", TR(T_WRONG));
                        st.error = 1;
                        st.npw = 0;
                    }
                    snprintf(prompt, sizeof prompt, "%s", msg);
                    st.input = 1;
                    st.busy = 0;
                    if (!st.error)
                        st.message[0] = 0;
                } else if (type == 'U') {
                    const char *u = st.user ? st.user : "";
                    authproto_send(&ap, 'u', u, strlen(u));
                } else if (type == 'i' || type == 'e') {
                    snprintf(st.message, sizeof st.message, "%s", msg);
                    st.error = type == 'e';
                }
            }
            if (n <= 0 || k < 0) {
                /* O helper terminou: sucesso desbloqueia; senão, tenta de
                 * novo com uma conversa nova. */
                int ws = 0;
                close(ap.to);
                close(ap.from);
                waitpid(ap.pid, &ws, 0);
                ap.pid = -1;
                if (WIFEXITED(ws) && WEXITSTATUS(ws) == 0) {
                    status = 0;
                    break;
                }
                snprintf(st.message, sizeof st.message, "%s", TR(T_WRONG));
                st.error = 1;
                st.busy = 0;
                st.npw = 0;
                prompt[0] = 0;
                if (authproto_start(&ap, self) != 0)
                    break;
            }
            draw = 1;
        }
        if (have_pad) {
            enum bwg_action acts[BWG_MAX_ACTIONS];
            long long now = bwg_now_ms();
            int na = 0;
            for (int i = 0; i < npfds; i++)
                if (p[3 + i].revents & (POLLIN | POLLHUP | POLLERR))
                    na += bwg_handle(&pad, p[3 + i].fd, now, acts + na, BWG_MAX_ACTIONS - na);
            na += bwg_tick(&pad, now, acts + na, BWG_MAX_ACTIONS - na);
            for (int i = 0; i < na; i++)
                draw |= lock_osk_pad(&vk.osk, acts[i], &auth_sink, &in, now) != 0;
            draw |= lock_osk_sticks(&vk.osk, bwg_active(&pad)) != 0;
            if (na) {
                last = time(NULL);
                st.npw = (int)utf8_chars(pw, in.len);
            }
        }
        int moved = 0;
        while (XPending(v.dpy)) {
            XEvent e;
            XNextEvent(v.dpy, &e);
            if (e.type == Expose && e.xexpose.count == 0)
                draw = 1;
            if (e.type == GenericEvent && e.xcookie.extension == xi_opcode && XGetEventData(v.dpy, &e.xcookie)) {
                const XIRawEvent *re = e.xcookie.data;
                int emulated = (re->flags & XIPointerEmulated) != 0; /* veio de um toque */
                int evtype = e.xcookie.evtype, button = re->detail;
                XFreeEventData(v.dpy, &e.xcookie);
                last = time(NULL);
                if (evtype == XI_RawMotion) {
                    if (!emulated) {
                        vk.ptr_shown = 1;
                        moved = 1;
                    }
                    continue;
                }
                if (button != 1)
                    continue;
                Window rw, cw;
                int rx, ry, wx, wy;
                unsigned mask;
                if (!XQueryPointer(v.dpy, v.win, &rw, &cw, &rx, &ry, &wx, &wy, &mask))
                    continue;
                vk.ptr_x = wx;
                vk.ptr_y = wy;
                if (emulated)
                    vk.ptr_shown = 0;
                int kbi = -1, key = vk_find_key(&vk, wx, wy, &kbi), idx = key >= 0 ? -1 : vk_find(&vk, wx, wy);
                if (evtype == XI_RawButtonPress) {
                    if (emulated && cfg.vk_touch && !vk.osk.shown) {
                        /* Primeiro toque na tela: abre o teclado. */
                        vk.osk.shown = 1;
                        vk.pressed = -1;
                    } else if (key >= 0) {
                        lock_osk_down(&vk.osk, key);
                        vk.key_kb = kbi;
                        vk.repeat_at = lock_osk_repeatable(&vk.osk) ? bwg_now_ms() + LOCK_OSK_REPEAT_DELAY : 0;
                    } else {
                        vk.pressed = idx;
                    }
                    draw = 1;
                } else if (evtype == XI_RawButtonRelease) {
                    if (vk.key_kb >= 0) {
                        /* soltar na mesma tecla (de qualquer monitor) a escreve */
                        if (lock_osk_up(&vk.osk, key, &auth_sink, &in) & LOCK_OSK_HIDE)
                            vk.osk.shown = 0;
                        st.npw = (int)utf8_chars(pw, in.len);
                    } else if (idx >= 0 && idx == vk.pressed) {
                        vhit h = vk.hits[idx];
                        vk_activate(&vk, &h, &in);
                        st.npw = (int)utf8_chars(pw, in.len);
                    }
                    vk.pressed = vk.key_kb = -1;
                    vk.repeat_at = 0;
                    draw = 1;
                }
            }
        }
        if (moved && !draw) {
            Window rw, cw;
            int rx, ry, wx, wy;
            unsigned mask;
            if (XQueryPointer(v.dpy, v.win, &rw, &cw, &rx, &ry, &wx, &wy, &mask)) {
                vk.ptr_x = wx;
                vk.ptr_y = wy;
                vk_cursor(&v, &vk);
            }
        }
        if (r == 0) {
            static int minute = -1;
            int m = (int)(time(NULL) / 60);
            draw |= m != minute;
            minute = m;
        }
        if (draw) {
            xview_draw(&v, &st, 1);
            vk.ptr_drawn = 0;
            if (vk.ptr_shown)
                vk_cursor(&v, &vk);
        }
    }
    if (have_pad)
        bwg_close(&pad);
    explicit_bzero(pw, sizeof pw);
    lock_osk_clear(&vk.osk);
    explicit_bzero(&vk, sizeof vk); /* a tecla morta pendente é parte da senha */
    authproto_stop(&ap);
    XCloseDisplay(v.dpy);
    return status;
}

/* ---- linha de comando ----------------------------------------------------- */

static int
run_background(int argc, char **argv)
{
    const char *out = NULL, *ppm = NULL;
    int x11 = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--x11"))
            x11 = 1;
        else if (!strcmp(argv[i], "--ppm") && i + 1 < argc)
            ppm = argv[++i];
        else
            out = argv[i];
    }
    if (!out)
        return 2;
    lockcfg cfg;
    cfg_load(&cfg);
    cairo_surface_t *shot = NULL;
    if (cfg.background == BG_BLUR) {
        if (ppm) {
            FILE *f = !strcmp(ppm, "-") ? stdin : fopen(ppm, "rb");
            if (f) {
                shot = load_ppm(f);
                if (f != stdin)
                    fclose(f);
            }
        } else if (x11) {
            shot = capture_x11();
        }
    }
    cairo_surface_t *bg = make_background(&cfg, shot);
    if (shot)
        cairo_surface_destroy(shot);
    int r = write_private_png(bg, out);
    cairo_surface_destroy(bg);
    return r ? 1 : 0;
}

static int
run_preview(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    int w = 960, h = 540;
    if (argc > 3)
        sscanf(argv[3], "%dx%d", &w, &h);
    lockcfg cfg;
    cfg_load(&cfg);
    if (cfg.background == BG_BLUR)
        cfg.background = BG_WALLPAPER; /* a prévia nunca captura a tela */
    cairo_surface_t *bg = make_background(&cfg, NULL);
    lockcfg shown;
    cfg_load(&shown);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t *cr = cairo_create(s);
    if (shown.background == BG_BLUR) {
        /* Desfoque de exemplo no papel de parede. */
        gaussian(bg, shown.blur * 2);
    }
    draw_background(cr, bg, w, h, 0, 0, w, h);
    panel_state st = { .cfg = &shown, .user = user_name(), .npw = 6, .input = 1 };
    snprintf(st.layout, sizeof st.layout, "br");
    double sc = w / 1280.0;
    draw_panel(cr, w / 2.0, h / 2.0, sc < 0.5 ? 0.5 : sc > 2 ? 2 : sc, &st);
    cairo_destroy(cr);
    int r = write_private_png(s, argv[2]);
    cairo_surface_destroy(s);
    cairo_surface_destroy(bg);
    return r ? 1 : 0;
}

/* Na saída, os caches do cairo e do fontconfig também: o Valgrind mostra
 * só o que for do programa. */
static int
finish(int status)
{
    if (logo_cache)
        cairo_surface_destroy(logo_cache);
    logo_cache = NULL;
    lock_osk_cleanup();
    cairo_debug_reset_static_data();
    FcFini();
    return status;
}

int
main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    init_i18n();
    char argv0[1024];
    snprintf(argv0, sizeof argv0, "%s", argv[0]);
    const char *name = basename(argv0);
    if (!strcmp(name, "saver_big_wm"))
        return finish(run_saver());
    if (!strcmp(name, "auth_big_wm")) {
        /* O xsecurelock chama os módulos pelo caminho da pasta deles; o
         * authproto está lá (um link para cá não muda o argv[0]). */
        return finish(run_auth(strchr(argv[0], '/') ? argv[0] : "/usr/lib/xsecurelock/auth_big_wm"));
    }
    if (argc > 1 && !strcmp(argv[1], "background"))
        return finish(run_background(argc, argv));
    if (argc > 1 && !strcmp(argv[1], "preview"))
        return finish(run_preview(argc, argv));
    if (argc > 1 && !strcmp(argv[1], "saver"))
        return finish(run_saver());
    fprintf(stderr,
            "uso: big-wm-lockscreen background [--x11 | --ppm ARQ] SAÍDA.png\n"
            "     big-wm-lockscreen preview SAÍDA.png [LxA]\n"
            "     (como saver_big_wm e auth_big_wm: módulos do xsecurelock)\n");
    return 2;
}
