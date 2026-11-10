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
 *       A mesma janela com o campo de senha: a senha aparece só como
 *       quadradinhos e vai para o authproto_pam do xsecurelock, que fala
 *       com o PAM. Aqui não há autenticação própria: o processo só desenha,
 *       recebe as teclas que o xsecurelock repassa e conversa com o helper.
 *       Teclado na tela (botão ⌨ no campo, ou aberto de saída com tela de
 *       toque): desenhado aqui, lê só cliques e movimento do ponteiro
 *       (XInput2 raw) e escreve no mesmo buffer da senha; Cancelar e
 *       Desbloquear funcionam com o mouse ou o toque.
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

typedef struct {
    int background;
    char image[1024];
    int blur;   /* 1..10: raio do desfoque */
    int dim;    /* 0..90: escurecimento em % */
    int show_logo, show_clock, show_date, show_user, show_caps, show_layout;
    /* Teclado na tela (o mesmo lock.conf do gtklock no Wayland). */
    int vk;         /* VirtualKeyboard: o teclado existe */
    int vk_button;  /* VirtualKeyboardButton: o botão ⌨ no campo de senha */
    int vk_layout;  /* KeyboardLayout: 0 auto, 1 ABNT2, 2 US */
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
    *c = (lockcfg){ .background = BG_BLUR, .blur = 4, .dim = 45, .show_logo = 1, .show_clock = 1,
                    .show_date = 1, .show_user = 1, .show_caps = 1, .show_layout = 1,
                    .vk = 1, .vk_button = 1, .vk_size = 1, .vk_touch = 1, .vk_dark = -1 };
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
            c->vk_layout = !strcmp(v, "br") || !strcmp(v, "abnt2") ? 1 : !strcmp(v, "us") ? 2 : 0;
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
    T_SESSION, T_LOCKED, T_USER, T_PASSWORD, T_HINT, T_CHECKING, T_WRONG, T_CAPS, T_LAYOUT, T_CANCEL, T_UNLOCK,
    T_WAIT, T_KEYBOARD, T_N
};

static const char *const texts[T_N] = {
    N_("Session"), N_("Session locked"), N_("User:"), N_("Password:"), N_("Type your password to unlock"),
    N_("Checking..."), N_("Wrong password"), N_("Caps Lock is on"), N_("Keyboard:"), N_("Cancel"),
    N_("Unlock"), N_("Please wait..."), N_("On-screen keyboard"),
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
    unsigned hi, mid, button, button_ink;
    double tile0[3], tile1[3];
} palette;

static const palette PAL_LIGHT = {
    .body = 0xaaaaaa, .ink = 0x000000, .ink2 = 0x202020, .hint = 0x6a6a6a, .err = 0x8b0000, .info = 0x303030,
    .field = 0xffffff, .field_off = 0xe2e2e2, .field_ink = 0x1a1a1a, .cursor = 0x000000, .key_hl = 0xd0d0d0, .key_ink = 0x404040,
    .hi = 0xffffff, .mid = 0x555555, .button = 0xaeaeae, .button_ink = 0x000000,
    .tile0 = { 0xa6 / 255.0, 0xa6 / 255.0, 0xb6 / 255.0 }, .tile1 = { 0x51 / 255.0, 0x55 / 255.0, 0x61 / 255.0 },
};

static const palette PAL_DARK = {
    .body = 0x3c3c3c, .ink = 0xe6e6e6, .ink2 = 0xc8c8c8, .hint = 0x9a9a9a, .err = 0xff7070, .info = 0xb8b8b8,
    .field = 0x262626, .field_off = 0x2e2e2e, .field_ink = 0xe6e6e6, .cursor = 0xf0f0f0, .key_hl = 0x5a5a5a, .key_ink = 0xd0d0d0,
    .hi = 0x787878, .mid = 0x1e1e1e, .button = 0x4a4a4a, .button_ink = 0xe6e6e6,
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
 * transparente durante a senha: com o mouse em uso, desenhamos uma seta. */

enum { VK_ABNT2, VK_US, VK_NLAYOUTS };

typedef struct {
    const char *normal, *shift;
    double width; /* em unidades de tecla */
} vkey;

#define VK(a, b) { a, b, 1 }
#define VKW(a, b, w) { a, b, w }
#define VK_END { NULL, NULL, 0 }

/* Teclas especiais pelo rótulo: \b apaga, \r confirma, \x0e Shift. */
static const vkey vk_rows[VK_NLAYOUTS][4][15] = {
    { { VK("'", "\""), VK("1", "!"), VK("2", "@"), VK("3", "#"), VK("4", "$"), VK("5", "%"), VK("6", "\xc2\xa8"),
        VK("7", "&"), VK("8", "*"), VK("9", "("), VK("0", ")"), VK("-", "_"), VK("=", "+"), VKW("\b", "\b", 1.6),
        VK_END },
      { VK("q", "Q"), VK("w", "W"), VK("e", "E"), VK("r", "R"), VK("t", "T"), VK("y", "Y"), VK("u", "U"),
        VK("i", "I"), VK("o", "O"), VK("p", "P"), VK("\xc2\xb4", "`"), VK("[", "{"), VK_END },
      { VK("a", "A"), VK("s", "S"), VK("d", "D"), VK("f", "F"), VK("g", "G"), VK("h", "H"), VK("j", "J"),
        VK("k", "K"), VK("l", "L"), VK("\xc3\xa7", "\xc3\x87"), VK("~", "^"), VK("]", "}"), VK_END },
      { VKW("\x0e", "\x0e", 1.4), VK("\\", "|"), VK("z", "Z"), VK("x", "X"), VK("c", "C"), VK("v", "V"),
        VK("b", "B"), VK("n", "N"), VK("m", "M"), VK(",", "<"), VK(".", ">"), VK(";", ":"), VK("/", "?"),
        VK_END } },
    { { VK("`", "~"), VK("1", "!"), VK("2", "@"), VK("3", "#"), VK("4", "$"), VK("5", "%"), VK("6", "^"),
        VK("7", "&"), VK("8", "*"), VK("9", "("), VK("0", ")"), VK("-", "_"), VK("=", "+"), VKW("\b", "\b", 1.6),
        VK_END },
      { VK("q", "Q"), VK("w", "W"), VK("e", "E"), VK("r", "R"), VK("t", "T"), VK("y", "Y"), VK("u", "U"),
        VK("i", "I"), VK("o", "O"), VK("p", "P"), VK("[", "{"), VK("]", "}"), VK("\\", "|"), VK_END },
      { VK("a", "A"), VK("s", "S"), VK("d", "D"), VK("f", "F"), VK("g", "G"), VK("h", "H"), VK("j", "J"),
        VK("k", "K"), VK("l", "L"), VK(";", ":"), VK("'", "\""), VK_END },
      { VKW("\x0e", "\x0e", 1.4), VK("z", "Z"), VK("x", "X"), VK("c", "C"), VK("v", "V"), VK("b", "B"),
        VK("n", "N"), VK("m", "M"), VK(",", "<"), VK(".", ">"), VK("/", "?"), VK_END } },
};

/* O que um toque faz. */
enum { VA_NONE, VA_KEY, VA_CAPS, VA_LAYOUT, VA_SPACE, VA_ENTER, VA_TOGGLE, VA_CANCEL, VA_UNLOCK };

typedef struct {
    double x, y, w, h;
    int action;
    const vkey *key;
} vhit;

#define VK_MAXHITS 512

typedef struct vkbd {
    int enabled, button; /* da configuração */
    int shown, layout, shift, caps;
    char dead[8];        /* tecla morta pendente (ABNT2) */
    double size;
    int dark;
    vhit hits[VK_MAXHITS];
    int nhits;
    int pressed;         /* índice em hits do toque em curso; -1 nenhum */
    int ptr_x, ptr_y, ptr_shown, ptr_drawn_x, ptr_drawn_y, ptr_drawn;
} vkbd;

static void
vk_hit(vkbd *k, double x, double y, double w, double h, int action, const vkey *key)
{
    if (k->nhits < VK_MAXHITS)
        k->hits[k->nhits++] = (vhit){ x, y, w, h, action, key };
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

/* Rótulo de uma tecla de caractere: Caps vale para letras; Shift para tudo. */
static const char *
vk_label(const vkbd *k, const vkey *key)
{
    unsigned char c0 = (unsigned char)key->normal[0];
    int letter = (c0 >= 'a' && c0 <= 'z' && !key->normal[1]) || !strcmp(key->normal, "\xc3\xa7");
    int up = letter ? k->shift != k->caps : k->shift;
    return up ? key->shift : key->normal;
}

/* ´`~^¨ + vogal (ABNT2): o caractere composto; NULL se não compõe. */
static const char *
vk_compose(const char *dead, const char *c)
{
    static const struct { const char *dead, *base, *out; } t[] = {
        { "\xc2\xb4", "a", "\xc3\xa1" }, { "\xc2\xb4", "e", "\xc3\xa9" }, { "\xc2\xb4", "i", "\xc3\xad" },
        { "\xc2\xb4", "o", "\xc3\xb3" }, { "\xc2\xb4", "u", "\xc3\xba" }, { "\xc2\xb4", "A", "\xc3\x81" },
        { "\xc2\xb4", "E", "\xc3\x89" }, { "\xc2\xb4", "I", "\xc3\x8d" }, { "\xc2\xb4", "O", "\xc3\x93" },
        { "\xc2\xb4", "U", "\xc3\x9a" }, { "`", "a", "\xc3\xa0" }, { "`", "A", "\xc3\x80" },
        { "~", "a", "\xc3\xa3" }, { "~", "o", "\xc3\xb5" }, { "~", "A", "\xc3\x83" }, { "~", "O", "\xc3\x95" },
        { "~", "n", "\xc3\xb1" }, { "~", "N", "\xc3\x91" }, { "^", "a", "\xc3\xa2" }, { "^", "e", "\xc3\xaa" },
        { "^", "o", "\xc3\xb4" }, { "^", "A", "\xc3\x82" }, { "^", "E", "\xc3\x8a" }, { "^", "O", "\xc3\x94" },
        { "\xc2\xa8", "u", "\xc3\xbc" }, { "\xc2\xa8", "U", "\xc3\x9c" },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++)
        if (!strcmp(t[i].dead, dead) && !strcmp(t[i].base, c))
            return t[i].out;
    return NULL;
}

static int
vk_is_dead(const vkbd *k, const char *s)
{
    return k->layout == VK_ABNT2 && (!strcmp(s, "\xc2\xb4") || !strcmp(s, "`") || !strcmp(s, "~") ||
                                     !strcmp(s, "^") || !strcmp(s, "\xc2\xa8"));
}

/* Layout de saída: o do lock.conf, senão o do XKB do servidor (o
 * _XKB_RULES_NAMES da raiz: "br" primeiro é ABNT2), senão o idioma. */
static int
vk_default_layout(Display *dpy, const lockcfg *c)
{
    if (c->vk_layout)
        return c->vk_layout == 1 ? VK_ABNT2 : VK_US;
    Atom a = XInternAtom(dpy, "_XKB_RULES_NAMES", True), type;
    int fmt;
    unsigned long n = 0, after;
    unsigned char *data = NULL;
    int layout = -1;
    if (a != None &&
        XGetWindowProperty(dpy, DefaultRootWindow(dpy), a, 0, 1024, False, XA_STRING, &type, &fmt, &n, &after,
                           &data) == Success && data && fmt == 8) {
        /* rules\0model\0layout\0variant\0options */
        const char *p = (const char *)data, *end = p + n;
        for (int i = 0; i < 2 && p < end; i++)
            p += strlen(p) + 1;
        if (p < end && *p)
            layout = !strncmp(p, "br", 2) && (p[2] == 0 || p[2] == ',') ? VK_ABNT2 : VK_US;
    }
    if (data)
        XFree(data);
    if (layout >= 0)
        return layout;
    const char *lang_env = getenv("LANG");
    return lang_env && !strncmp(lang_env, "pt", 2) ? VK_ABNT2 : VK_US;
}

/* Ícones desenhados (a fonte pode não ter ⌫ ⇧ ⌨). */
static void
vk_icon_backspace(cairo_t *cr, double cx, double cy, double s, unsigned c)
{
    rgb(cr, c);
    cairo_set_line_width(cr, 1.4 * s);
    cairo_move_to(cr, cx - 9 * s, cy);
    cairo_line_to(cr, cx - 4 * s, cy - 5 * s);
    cairo_line_to(cr, cx + 8 * s, cy - 5 * s);
    cairo_line_to(cr, cx + 8 * s, cy + 5 * s);
    cairo_line_to(cr, cx - 4 * s, cy + 5 * s);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - 1 * s, cy - 2.5 * s);
    cairo_line_to(cr, cx + 4 * s, cy + 2.5 * s);
    cairo_move_to(cr, cx + 4 * s, cy - 2.5 * s);
    cairo_line_to(cr, cx - 1 * s, cy + 2.5 * s);
    cairo_stroke(cr);
}

static void
vk_icon_shift(cairo_t *cr, double cx, double cy, double s, unsigned c, int filled)
{
    rgb(cr, c);
    cairo_set_line_width(cr, 1.4 * s);
    cairo_move_to(cr, cx, cy - 7 * s);
    cairo_line_to(cr, cx + 7 * s, cy);
    cairo_line_to(cr, cx + 3 * s, cy);
    cairo_line_to(cr, cx + 3 * s, cy + 6 * s);
    cairo_line_to(cr, cx - 3 * s, cy + 6 * s);
    cairo_line_to(cr, cx - 3 * s, cy);
    cairo_line_to(cr, cx - 7 * s, cy);
    cairo_close_path(cr);
    if (filled)
        cairo_fill(cr);
    else
        cairo_stroke(cr);
}

static void
vk_icon_enter(cairo_t *cr, double cx, double cy, double s, unsigned c)
{
    rgb(cr, c);
    cairo_set_line_width(cr, 1.4 * s);
    cairo_move_to(cr, cx + 6 * s, cy - 6 * s);
    cairo_line_to(cr, cx + 6 * s, cy + 2 * s);
    cairo_line_to(cr, cx - 4 * s, cy + 2 * s);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - 7 * s, cy + 2 * s);
    cairo_line_to(cr, cx - 2 * s, cy - 2 * s);
    cairo_line_to(cr, cx - 2 * s, cy + 6 * s);
    cairo_close_path(cr);
    cairo_fill(cr);
}

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

/* Uma tecla: botão do WPrefs (claro) ou cinza-escuro (tema escuro);
 * afundada enquanto pressionada ou ligada (Caps, Shift). */
static void
vk_key(cairo_t *cr, vkbd *k, double x, double y, double w, double h, double s, int action, const vkey *key,
       const char *label, int down)
{
    unsigned face = k->dark ? (down ? 0x2a2a2a : 0x4a4a4a) : (down ? 0x8e8e8e : 0xaeaeae);
    unsigned ink = k->dark ? 0xf0f0f0 : 0x000000;
    fillr(cr, x, y, w, h, face);
    bevel(cr, x, y, w, h, !down, s);
    double cx = x + w / 2, cy = y + h / 2, fs = 14 * s;
    if (key && !strcmp(key->normal, "\b"))
        vk_icon_backspace(cr, cx, cy, s, ink);
    else if (key && !strcmp(key->normal, "\x0e"))
        vk_icon_shift(cr, cx, cy, s, ink, k->shift);
    else if (action == VA_ENTER)
        vk_icon_enter(cr, cx, cy, s, ink);
    else if (label && *label) {
        double tw = text_width(cr, label, fs, 0);
        text(cr, label, cx - tw / 2, cy + fs * 0.36, fs, 0, ink);
    }
    vk_hit(k, x, y, w, h, action, key);
}

/* Altura do teclado com a moldura (para centrar diálogo + teclado). */
static double
vk_height(const vkbd *k, double s0)
{
    double s = s0 * k->size;
    return 20 * s + 16 * s + 5 * 40 * s + 4 * 4 * s + 14 * s0;
}

/* O teclado, centrado em cx, logo abaixo de top (ou no pé do monitor, se
 * não couber). Moldura do OPENSTEP, com a barra de título preta. */
static void
vk_draw(cairo_t *cr, vkbd *k, double cx, double top, double mon_y, double mon_h, double s0)
{
    double s = s0 * k->size;
    double u = 40 * s, gap = 4 * s, pad = 8 * s, th = 20 * s;
    double kw = 14 * (u + gap) + 0.6 * u + gap; /* a fileira mais larga */
    double W = kw + 2 * pad, H = th + pad * 2 + 5 * u + 4 * gap;
    double x = round(cx - W / 2), y = round(top + 14 * s);
    if (y + H > mon_y + mon_h - 8 * s)
        y = round(mon_y + mon_h - 8 * s - H);
    unsigned body = k->dark ? 0x333333 : 0xaaaaaa;
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_rectangle(cr, x + 6 * s, y + 6 * s, W, H);
    cairo_fill(cr);
    cairo_pattern_t *g = cairo_pattern_create_linear(x, y, x, y + th);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.16, 0.16, 0.18);
    cairo_pattern_add_color_stop_rgb(g, 1, 0, 0, 0);
    cairo_rectangle(cr, x, y, W, th);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    bevel(cr, x, y, W, th, 1, s);
    double fs = 11 * s;
    text(cr, TR(T_KEYBOARD), x + 10 * s, y + th / 2 + fs * 0.36, fs, 1, 0xffffff);
    const char *lname = k->layout == VK_ABNT2 ? "ABNT2" : "US";
    text(cr, lname, x + W - 10 * s - text_width(cr, lname, fs, 1), y + th / 2 + fs * 0.36, fs, 1, 0xffffff);
    fillr(cr, x, y + th, W, H - th, body);
    bevel(cr, x, y + th, W, H - th, 1, s);
    double ky = y + th + pad;
    for (int r = 0; r < 4; r++) {
        const vkey *row = vk_rows[k->layout][r];
        double rw = -gap;
        for (int c = 0; row[c].normal; c++)
            rw += row[c].width * u + gap;
        double kx = round(cx - rw / 2);
        for (int c = 0; row[c].normal; c++) {
            const vkey *key = &row[c];
            double w = key->width * u;
            int idx = k->nhits;
            int down = (k->pressed == idx) || (!strcmp(key->normal, "\x0e") && k->shift);
            vk_key(cr, k, kx, ky, w, u, s, VA_KEY, key, vk_label(k, key), down);
            kx += w + gap;
        }
        ky += u + gap;
    }
    /* Caps, layout, espaço, Enter. */
    double widths[4] = { 1.8 * u, 1.6 * u, 6.5 * u, 2 * u };
    int actions[4] = { VA_CAPS, VA_LAYOUT, VA_SPACE, VA_ENTER };
    const char *labels[4] = { "Caps", k->layout == VK_ABNT2 ? "BR" : "US", "", "" };
    double rw = widths[0] + widths[1] + widths[2] + widths[3] + 3 * gap, kx = round(cx - rw / 2);
    for (int i = 0; i < 4; i++) {
        int idx = k->nhits;
        int down = k->pressed == idx || (actions[i] == VA_CAPS && k->caps);
        vk_key(cr, k, kx, ky, widths[i], u, s, actions[i], NULL, labels[i], down);
        if (actions[i] == VA_CAPS && k->caps)
            fillr(cr, kx + 6 * s, ky + 6 * s, 6 * s, 6 * s, 0x30d030); /* LED */
        kx += widths[i] + gap;
    }
}

/* Seta do ponteiro, desenhada direto na janela por cima do quadro; a área
 * anterior volta do pixmap do quadro. */
#define VK_ARROW 22

/* A janela de sessão bloqueada, centrada em (cx, cy), escala s. Devolve a
 * borda de baixo. */
static double
draw_panel(cairo_t *cr, double cx, double cy, double s, const panel_state *st)
{
    const lockcfg *c = st->cfg;
    double W = 560 * s, H = 268 * s;
    double x = round(cx - W / 2), y = round(cy - H / 2);
    double th = 22 * s;
    /* Sombra discreta. */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_rectangle(cr, x + 6 * s, y + 6 * s, W, H);
    cairo_fill(cr);
    /* Barra de título: preta, como a da janela em foco no Window Maker,
     * com o menu à esquerda e o nome do sistema à direita. */
    cairo_pattern_t *g = cairo_pattern_create_linear(x, y, x, y + th);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.16, 0.16, 0.18);
    cairo_pattern_add_color_stop_rgb(g, 1, 0, 0, 0);
    cairo_rectangle(cr, x, y, W, th);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    bevel(cr, x, y, W, th, 1, s);
    double fs = 12 * s;
    text(cr, TR(T_SESSION), x + 10 * s, y + th / 2 + fs * 0.36, fs, 1, 0xffffff);
    const char *brand = "Big WindowMaker";
    text(cr, brand, x + W - 10 * s - text_width(cr, brand, fs, 1), y + th / 2 + fs * 0.36, fs, 1, 0xffffff);
    /* Corpo cinza do OPENSTEP. */
    double by = y + th;
    fillr(cr, x, by, W, H - th, P->body);
    bevel(cr, x, by, W, H - th, 1, s);
    /* Ladrilho do Dock com o logo. */
    double pad = 18 * s, tile = 112 * s;
    double tx = x + pad, ty = by + pad;
    if (c->show_logo) {
        cairo_pattern_t *tg = cairo_pattern_create_linear(tx, ty, tx + tile, ty + tile);
        cairo_pattern_add_color_stop_rgb(tg, 0, P->tile0[0], P->tile0[1], P->tile0[2]);
        cairo_pattern_add_color_stop_rgb(tg, 1, P->tile1[0], P->tile1[1], P->tile1[2]);
        cairo_rectangle(cr, tx, ty, tile, tile);
        cairo_set_source(cr, tg);
        cairo_fill(cr);
        cairo_pattern_destroy(tg);
        bevel(cr, tx, ty, tile, tile, 1, s);
        int ls = (int)(84 * s);
        cairo_surface_t *lg = logo(ls);
        if (lg) {
            double lw = cairo_image_surface_get_width(lg), lh = cairo_image_surface_get_height(lg);
            cairo_save(cr);
            cairo_translate(cr, tx + (tile - lw) / 2, ty + (tile - lh) / 2);
            cairo_set_source_surface(cr, lg, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        }
    }
    /* Coluna da direita. */
    double rx = c->show_logo ? tx + tile + 22 * s : x + pad, ry = by + pad + 16 * s;
    double rw = x + W - pad - rx;
    text(cr, "Big WindowMaker", rx, ry, 18 * s, 1, P->ink);
    ry += 20 * s;
    text(cr, TR(T_LOCKED), rx, ry, 12 * s, 0, P->ink2);
    char buf[256];
    if (c->show_clock || c->show_date) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char t1[64] = "", t2[128] = "";
        if (c->show_clock)
            strftime(t1, sizeof t1, "%H:%M", &tm);
        if (c->show_date)
            strftime(t2, sizeof t2, "%A, %x", &tm);
        snprintf(buf, sizeof buf, "%s%s%s", t1, *t1 && *t2 ? "  \xc2\xb7  " : "", t2);
        double w = text_width(cr, buf, 12 * s, 0);
        text(cr, buf, x + W - pad - w, by + pad + 16 * s, 12 * s, 0, P->ink2);
    }
    ry += 26 * s;
    if (c->show_user && st->user) {
        double lw = text(cr, TR(T_USER), rx, ry, 13 * s, 0, P->ink);
        text(cr, st->user, rx + lw + 8 * s, ry, 13 * s, 1, P->ink);
        ry += 26 * s;
    }
    /* Campo de senha: retângulo branco afundado; cada caractere é um
     * quadradinho, nunca o caractere. */
    double lw = text(cr, TR(T_PASSWORD), rx, ry + 4 * s, 13 * s, 0, P->ink);
    double fx = rx + lw + 8 * s, fy = ry - 13 * s, fw = rw - lw - 8 * s, fh = 24 * s;
    fillr(cr, fx, fy, fw, fh, st->input ? P->field : P->field_off);
    bevel(cr, fx, fy, fw, fh, 0, s);
    double icon_w = 0;
    vkbd *vk = st->vk;
    if (vk && vk->enabled && vk->button) {
        /* ⌨ no fim do campo, como no gtklock do Wayland. */
        double iw = 20 * s, ih = 12 * s;
        icon_w = iw + 10 * s;
        double ix = fx + fw - iw - 6 * s, iy = fy + (fh - ih) / 2;
        if (vk->shown)
            fillr(cr, ix - 3 * s, fy + 3 * s, iw + 6 * s, fh - 6 * s, P->key_hl);
        vk_icon_keyboard(cr, ix, iy, iw, ih, s, P->key_ink);
        vk_hit(vk, fx + fw - icon_w - 4 * s, fy - 4 * s, icon_w + 8 * s, fh + 8 * s, VA_TOGGLE, NULL);
    }
    double sq = 7 * s, gap = 5 * s;
    int maxsq = (int)((fw - 16 * s - icon_w) / (sq + gap));
    int n = st->npw > maxsq ? maxsq : st->npw;
    for (int i = 0; i < n; i++)
        fillr(cr, fx + 8 * s + i * (sq + gap), fy + (fh - sq) / 2, sq, sq, P->field_ink);
    if (st->input && !st->busy) {
        /* cursor de texto */
        fillr(cr, fx + 8 * s + n * (sq + gap), fy + 5 * s, 1.5 * s, fh - 10 * s, P->cursor);
    } else if (!st->input && st->npw == 0) {
        text(cr, TR(T_HINT), fx + 8 * s, fy + fh / 2 + 4 * s, 11 * s, 0, P->hint);
    }
    ry += 26 * s;
    /* Mensagens: erro em vermelho escuro, informação do PAM (leitor de
     * digital, aviso de senha a vencer) em preto. Caps Lock com um LED. */
    double my = ry + 6 * s;
    if (st->message[0])
        text(cr, st->message, fx, my, 11.5 * s, st->error, st->error ? P->err : P->ink2);
    else if (st->busy)
        text(cr, TR(T_CHECKING), fx, my, 11.5 * s, 0, P->ink2);
    my += 18 * s;
    if (c->show_caps && st->caps) {
        fillr(cr, fx, my - 8 * s, 8 * s, 8 * s, 0xf0b030);
        bevel(cr, fx - 1 * s, my - 9 * s, 10 * s, 10 * s, 0, s);
        text(cr, TR(T_CAPS), fx + 14 * s, my, 11 * s, 1, P->ink);
    }
    if (c->show_layout && st->layout[0]) {
        snprintf(buf, sizeof buf, "%s %s", TR(T_LAYOUT), st->layout);
        double w = text_width(cr, buf, 11 * s, 0);
        text(cr, buf, fx + fw - w, my, 11 * s, 0, P->info);
    }
    /* Botões no canto, como num painel do NeXTSTEP: Esc e Enter. */
    double bw = 110 * s, bh = 26 * s, bx = x + W - pad - bw, byy = y + H - pad - bh + 4 * s;
    button(cr, bx, byy, bw, bh, TR(T_UNLOCK), 1, s);
    button(cr, bx - bw - 10 * s, byy, bw, bh, TR(T_CANCEL), 0, s);
    if (vk) {
        vk_hit(vk, bx, byy, bw, bh, VA_UNLOCK, NULL);
        vk_hit(vk, bx - bw - 10 * s, byy, bw, bh, VA_CANCEL, NULL);
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

static int
xview_open(xview *v, int create_child)
{
    const char *ws = getenv("XSCREENSAVER_WINDOW");
    v->dpy = XOpenDisplay(NULL);
    if (!v->dpy || !ws)
        return -1;
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
        vk->nhits = 0;
    if (panels_per_monitor) {
        /* Diálogo na janela da raiz inteira: um por monitor, cada um com o
         * seu teclado (o toque vale em qualquer um). */
        int nm = 0;
        XRRMonitorInfo *m = XRRGetMonitors(v->dpy, DefaultRootWindow(v->dpy), True, &nm);
        for (int i = 0; i < nm; i++) {
            double cx = m[i].x - v->ox + m[i].width / 2.0, cy = m[i].y - v->oy + m[i].height / 2.0;
            if (vk && vk->enabled && vk->shown) {
                /* Diálogo e teclado juntos no meio do monitor. */
                double ph = 268 * v->scale, kh = vk_height(vk, v->scale);
                double top = m[i].y - v->oy + (m[i].height - ph - kh) / 2;
                cy = fmax(m[i].y - v->oy + 8 * v->scale, top) + ph / 2;
            }
            double bottom = draw_panel(cr, cx, cy, v->scale, st);
            if (vk && vk->enabled && vk->shown)
                vk_draw(cr, vk, cx, bottom, m[i].y - v->oy, m[i].height, v->scale);
        }
        if (m)
            XRRFreeMonitors(m);
        if (nm <= 0) {
            double bottom = draw_panel(cr, v->w / 2.0, v->h / 2.0, v->scale, st);
            if (vk && vk->enabled && vk->shown)
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
    while (!quit_flag) {
        struct pollfd p = { ConnectionNumber(v.dpy), POLLIN, 0 };
        int r = poll(&p, 1, seconds_to_next_minute() * 1000);
        if (quit_flag)
            break;
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

/* Um toque numa tecla da tela (ou nos botões do diálogo). */
static void
vk_activate(vkbd *k, const vhit *h, auth_input *in)
{
    switch (h->action) {
    case VA_TOGGLE:
        k->shown = !k->shown;
        break;
    case VA_CANCEL:
        explicit_bzero(in->pw, in->cap);
        explicit_bzero(k->dead, sizeof k->dead);
        in->len = 0;
        quit_flag = 1; /* como o Esc: o diálogo some, a tela continua bloqueada */
        break;
    case VA_UNLOCK:
    case VA_ENTER:
        k->dead[0] = 0;
        auth_byte(in, '\r');
        break;
    case VA_CAPS:
        k->caps = !k->caps;
        break;
    case VA_LAYOUT:
        k->layout = (k->layout + 1) % VK_NLAYOUTS;
        k->dead[0] = 0;
        break;
    case VA_SPACE:
        if (k->dead[0])
            auth_text(in, k->dead);
        k->dead[0] = 0;
        auth_byte(in, ' ');
        break;
    case VA_KEY: {
        const vkey *key = h->key;
        if (!strcmp(key->normal, "\b")) {
            k->dead[0] = 0;
            auth_byte(in, '\b');
        } else if (!strcmp(key->normal, "\x0e")) {
            k->shift = !k->shift;
        } else {
            const char *label = vk_label(k, key);
            if (vk_is_dead(k, label) && !k->dead[0]) {
                snprintf(k->dead, sizeof k->dead, "%s", label);
            } else {
                const char *c = k->dead[0] ? vk_compose(k->dead, label) : NULL;
                if (k->dead[0] && !c)
                    auth_text(in, k->dead);
                auth_text(in, c ? c : label);
                k->dead[0] = 0;
            }
            k->shift = 0; /* Shift vale para uma tecla */
        }
        break;
    }
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
    vk = (vkbd){ .enabled = cfg.vk, .button = cfg.vk_button, .size = cfg.vk_size, .dark = cfg.vk_dark < 0 ? mode_dark : cfg.vk_dark,
                 .pressed = -1 };
    vk.layout = vk_default_layout(v.dpy, &cfg);
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
                vk.shown = 1;
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
    const char *tos = getenv("XSECURELOCK_AUTH_TIMEOUT");
    int timeout_s = tos && atoi(tos) > 0 ? atoi(tos) : 60;
    time_t last = time(NULL);
    int status = 1;
    auth_input in = { .pw = pw, .len = 0, .cap = sizeof pw, .st = &st, .ap = &ap };
    xview_draw(&v, &st, 1);
    while (!quit_flag) {
        struct pollfd p[3] = { { 0, POLLIN, 0 }, { ap.from, POLLIN, 0 }, { ConnectionNumber(v.dpy), POLLIN, 0 } };
        /* Eventos que o Xlib já leu (junto com a resposta de um
         * XQueryPointer, por exemplo) não acordam o poll: sem espera. */
        int r = poll(p, 3, XEventsQueued(v.dpy, QueuedAlready) ? 0 : 1000);
        if (r < 0 && errno != EINTR)
            break;
        int draw = 0;
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
                     * um "Senha incorreta" fica). */
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
                if (authproto_start(&ap, self) != 0)
                    break;
            }
            draw = 1;
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
                int idx = vk_find(&vk, wx, wy);
                if (evtype == XI_RawButtonPress) {
                    if (emulated && cfg.vk_touch && !vk.shown) {
                        /* Primeiro toque na tela: abre o teclado. */
                        vk.shown = 1;
                        vk.pressed = -1;
                    } else {
                        vk.pressed = idx;
                    }
                    draw = 1;
                } else if (evtype == XI_RawButtonRelease) {
                    if (idx >= 0 && idx == vk.pressed) {
                        vhit h = vk.hits[idx];
                        vk_activate(&vk, &h, &in);
                        st.npw = (int)utf8_chars(pw, in.len);
                    }
                    vk.pressed = -1;
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
    explicit_bzero(pw, sizeof pw);
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
