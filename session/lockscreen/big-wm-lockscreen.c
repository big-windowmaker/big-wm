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
#include <X11/extensions/Xrandr.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <dirent.h>
#include <errno.h>
#include <fontconfig/fontconfig.h>
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <langinfo.h>
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

static void
cfg_load(lockcfg *c)
{
    *c = (lockcfg){ .background = BG_BLUR, .blur = 4, .dim = 45, .show_logo = 1, .show_clock = 1,
                    .show_date = 1, .show_user = 1, .show_caps = 1, .show_layout = 1 };
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
    }
    fclose(f);
}

/* ---- textos (pt, es, en) -------------------------------------------------- */

enum {
    T_SESSION, T_LOCKED, T_USER, T_PASSWORD, T_HINT, T_CHECKING, T_WRONG, T_CAPS, T_LAYOUT, T_CANCEL, T_UNLOCK,
    T_WAIT, T_N
};

static const char *const texts[3][T_N] = {
    { "Sessão", "Sessão bloqueada", "Usuário:", "Senha:", "Digite a senha para desbloquear", "Verificando...",
      "Senha incorreta", "Caps Lock ativado", "Teclado:", "Cancelar", "Desbloquear", "Aguarde..." },
    { "Sesión", "Sesión bloqueada", "Usuario:", "Contraseña:", "Escriba la contraseña para desbloquear",
      "Verificando...", "Contraseña incorrecta", "Bloq Mayús activado", "Teclado:", "Cancelar", "Desbloquear",
      "Espere..." },
    { "Session", "Session locked", "User:", "Password:", "Type your password to unlock", "Checking...",
      "Wrong password", "Caps Lock is on", "Keyboard:", "Cancel", "Unlock", "Please wait..." },
};

static int lang = 2;

static void
pick_language(void)
{
    const char *vars[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };
    for (size_t i = 0; i < sizeof vars / sizeof *vars; i++) {
        const char *v = getenv(vars[i]);
        if (!v || !*v)
            continue;
        lang = !strncmp(v, "pt", 2) ? 0 : !strncmp(v, "es", 2) ? 1 : 2;
        return;
    }
}

#define TR(k) (texts[lang][k])

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
} panel_state;

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
    unsigned lt = raised ? 0xffffff : 0x000000, dk = raised ? 0x000000 : 0xffffff;
    fillr(cr, x, y, w, t, lt);
    fillr(cr, x, y, t, h, lt);
    fillr(cr, x, y + h - t, w, t, dk);
    fillr(cr, x + w - t, y, t, h, dk);
    if (raised) {
        fillr(cr, x + t, y + h - 2 * t, w - 2 * t, t, 0x555555);
        fillr(cr, x + w - 2 * t, y + t, t, h - 2 * t, 0x555555);
    } else {
        fillr(cr, x + t, y + t, w - 2 * t, t, 0x555555);
        fillr(cr, x + t, y + t, t, h - 2 * t, 0x555555);
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
    fillr(cr, x, y, w, h, 0xaeaeae);
    bevel(cr, x, y, w, h, 1, s);
    double fs = 12 * s;
    double tw = text_width(cr, label, fs, 0) + (def ? 16 * s : 0);
    double tx = x + (w - tw) / 2;
    text(cr, label, tx, y + h / 2 + fs * 0.36, fs, 0, 0x000000);
    if (def) {
        /* ⏎ desenhado (a fonte pode não ter o glifo) */
        double ax = tx + tw - 10 * s, ay = y + h / 2;
        rgb(cr, 0x000000);
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

/* A janela de sessão bloqueada, centrada em (cx, cy), escala s. */
static void
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
    fillr(cr, x, by, W, H - th, 0xaaaaaa);
    bevel(cr, x, by, W, H - th, 1, s);
    /* Ladrilho do Dock com o logo. */
    double pad = 18 * s, tile = 112 * s;
    double tx = x + pad, ty = by + pad;
    if (c->show_logo) {
        cairo_pattern_t *tg = cairo_pattern_create_linear(tx, ty, tx + tile, ty + tile);
        cairo_pattern_add_color_stop_rgb(tg, 0, 0xa6 / 255.0, 0xa6 / 255.0, 0xb6 / 255.0);
        cairo_pattern_add_color_stop_rgb(tg, 1, 0x51 / 255.0, 0x55 / 255.0, 0x61 / 255.0);
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
    text(cr, "Big WindowMaker", rx, ry, 18 * s, 1, 0x000000);
    ry += 20 * s;
    text(cr, TR(T_LOCKED), rx, ry, 12 * s, 0, 0x202020);
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
        text(cr, buf, x + W - pad - w, by + pad + 16 * s, 12 * s, 0, 0x202020);
    }
    ry += 26 * s;
    if (c->show_user && st->user) {
        double lw = text(cr, TR(T_USER), rx, ry, 13 * s, 0, 0x000000);
        text(cr, st->user, rx + lw + 8 * s, ry, 13 * s, 1, 0x000000);
        ry += 26 * s;
    }
    /* Campo de senha: retângulo branco afundado; cada caractere é um
     * quadradinho, nunca o caractere. */
    double lw = text(cr, TR(T_PASSWORD), rx, ry + 4 * s, 13 * s, 0, 0x000000);
    double fx = rx + lw + 8 * s, fy = ry - 13 * s, fw = rw - lw - 8 * s, fh = 24 * s;
    fillr(cr, fx, fy, fw, fh, st->input ? 0xffffff : 0xe2e2e2);
    bevel(cr, fx, fy, fw, fh, 0, s);
    double sq = 7 * s, gap = 5 * s;
    int maxsq = (int)((fw - 16 * s) / (sq + gap));
    int n = st->npw > maxsq ? maxsq : st->npw;
    for (int i = 0; i < n; i++)
        fillr(cr, fx + 8 * s + i * (sq + gap), fy + (fh - sq) / 2, sq, sq, 0x1a1a1a);
    if (st->input && !st->busy) {
        /* cursor de texto */
        fillr(cr, fx + 8 * s + n * (sq + gap), fy + 5 * s, 1.5 * s, fh - 10 * s, 0x000000);
    } else if (!st->input && st->npw == 0) {
        text(cr, TR(T_HINT), fx + 8 * s, fy + fh / 2 + 4 * s, 11 * s, 0, 0x6a6a6a);
    }
    ry += 26 * s;
    /* Mensagens: erro em vermelho escuro, informação do PAM (leitor de
     * digital, aviso de senha a vencer) em preto. Caps Lock com um LED. */
    double my = ry + 6 * s;
    if (st->message[0])
        text(cr, st->message, fx, my, 11.5 * s, st->error, st->error ? 0x8b0000 : 0x202020);
    else if (st->busy)
        text(cr, TR(T_CHECKING), fx, my, 11.5 * s, 0, 0x202020);
    my += 18 * s;
    if (c->show_caps && st->caps) {
        fillr(cr, fx, my - 8 * s, 8 * s, 8 * s, 0xf0b030);
        bevel(cr, fx - 1 * s, my - 9 * s, 10 * s, 10 * s, 0, s);
        text(cr, TR(T_CAPS), fx + 14 * s, my, 11 * s, 1, 0x000000);
    }
    if (c->show_layout && st->layout[0]) {
        snprintf(buf, sizeof buf, "%s %s", TR(T_LAYOUT), st->layout);
        double w = text_width(cr, buf, 11 * s, 0);
        text(cr, buf, fx + fw - w, my, 11 * s, 0, 0x303030);
    }
    /* Botões no canto, como num painel do NeXTSTEP: Esc e Enter. */
    double bw = 110 * s, bh = 26 * s, bx = x + W - pad - bw, byy = y + H - pad - bh + 4 * s;
    button(cr, bx, byy, bw, bh, TR(T_UNLOCK), 1, s);
    button(cr, bx - bw - 10 * s, byy, bw, bh, TR(T_CANCEL), 0, s);
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
    if (panels_per_monitor) {
        /* Diálogo na janela da raiz inteira: um por monitor. */
        int nm = 0;
        XRRMonitorInfo *m = XRRGetMonitors(v->dpy, DefaultRootWindow(v->dpy), True, &nm);
        for (int i = 0; i < nm; i++)
            draw_panel(cr, m[i].x - v->ox + m[i].width / 2.0, m[i].y - v->oy + m[i].height / 2.0, v->scale, st);
        if (m)
            XRRFreeMonitors(m);
        if (nm <= 0)
            draw_panel(cr, v->w / 2.0, v->h / 2.0, v->scale, st);
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
    size_t pwlen = 0;
    panel_state st = { .cfg = &cfg, .user = user_name(), .input = 0 };
    keyboard_state(v.dpy, &st);
    snprintf(st.message, sizeof st.message, "%s", TR(T_WAIT));
    authproto ap = { .pid = -1 };
    if (authproto_start(&ap, self) != 0)
        return 1;
    const char *tos = getenv("XSECURELOCK_AUTH_TIMEOUT");
    int timeout_s = tos && atoi(tos) > 0 ? atoi(tos) : 60;
    time_t last = time(NULL);
    int status = 1;
    xview_draw(&v, &st, 1);
    while (!quit_flag) {
        struct pollfd p[3] = { { 0, POLLIN, 0 }, { ap.from, POLLIN, 0 }, { ConnectionNumber(v.dpy), POLLIN, 0 } };
        int r = poll(p, 3, 1000);
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
            for (ssize_t i = 0; i < n; i++) {
                unsigned char ch = (unsigned char)kb[i];
                if (ch == '\r' || ch == '\n') {
                    if (st.input && !st.busy) {
                        authproto_send(&ap, 'p', pw, pwlen);
                        explicit_bzero(pw, sizeof pw);
                        pwlen = 0;
                        st.busy = 1;
                        st.input = 0;
                        st.message[0] = 0;
                    }
                } else if (ch == 0x1b) {
                    if (pwlen == 0 && !st.busy)
                        quit_flag = 1; /* Esc no campo vazio: cancela */
                    explicit_bzero(pw, sizeof pw);
                    pwlen = 0;
                } else if (ch == '\b' || ch == 0x7f) {
                    while (pwlen > 0 && ((unsigned char)pw[pwlen - 1] & 0xc0) == 0x80)
                        pw[--pwlen] = 0;
                    if (pwlen > 0)
                        pw[--pwlen] = 0;
                } else if (ch == 0x15 || ch == 0x17) {
                    explicit_bzero(pw, sizeof pw);
                    pwlen = 0;
                } else if (ch >= 0x20 && pwlen < sizeof pw - 1 && !st.busy) {
                    pw[pwlen++] = (char)ch;
                    st.error = 0;
                    if (st.message[0] && !st.input)
                        st.message[0] = 0;
                }
            }
            st.npw = (int)utf8_chars(pw, pwlen);
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
                    /* Pede a senha: o campo fica ativo. */
                    st.input = 1;
                    st.busy = 0;
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
        while (XPending(v.dpy)) {
            XEvent e;
            XNextEvent(v.dpy, &e);
            if (e.type == Expose && e.xexpose.count == 0)
                draw = 1;
        }
        if (r == 0) {
            static int minute = -1;
            int m = (int)(time(NULL) / 60);
            draw |= m != minute;
            minute = m;
        }
        if (draw)
            xview_draw(&v, &st, 1);
    }
    explicit_bzero(pw, sizeof pw);
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
    pick_language();
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
