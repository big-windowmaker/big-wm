// SPDX-License-Identifier: GPL-2.0-or-later
//
// big-wm-kwin-screenshot: o Spectacle na sessão Wayland do Big WindowMaker.
//
// No Wayland o Spectacle (KDE) só tira a captura pelo KWin: a interface
// D-Bus org.kde.KWin.ScreenShot2. Este serviço a oferece fora do Plasma,
// com o grim (protocolo de captura do wlmaker) e o slurp: tela inteira, uma
// tela, uma área, a janela ativa e a escolha interativa (janela ou tela).
//
//   big-wm-kwin-screenshot          serviço (autostart da sessão Wayland)
//   big-wm-kwin-screenshot --check  sai com 0 se este ambiente o aceita
//
// Só roda numa sessão wlmaker e só quando nenhum KWin de verdade tem os
// nomes: no Plasma, o KWin os tem e este sai sem fazer nada.
//
// Em C (GIO) desde 2026-11: a versão em Python ficava residente com ~16 MiB
// de PSS em toda sessão Wayland, para um serviço usado só ao capturar a tela
// (docs/performance/07-otimizacoes-estruturais.md).

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SCREENSHOT_PATH "/org/kde/KWin/ScreenShot2"
#define EFFECTS_PATH "/Effects"
#define ERROR_PREFIX "org.kde.KWin.ScreenShot2.Error."
#define VERSION 4         // CaptureWorkspace (Plasma 6)
#define FORMAT_RGB32 4    // QImage::Format_RGB32: B G R A na memória
// O Spectacle espera a CaptureInteractive por 60 s: o slurp termina antes,
// para não ficar segurando o teclado e o ponteiro depois que ele desistiu.
#define SLURP_TIMEOUT 50
#define GRIM_TIMEOUT 20
#define ZONES_TIMEOUT 5

static const char introspection_xml[] =
    "<node>"
    " <interface name='org.kde.KWin.ScreenShot2'>"
    "  <property name='Version' type='u' access='read'/>"
    "  <method name='CaptureWindow'>"
    "   <arg name='handle' type='s' direction='in'/>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureActiveWindow'>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureArea'>"
    "   <arg name='x' type='i' direction='in'/>"
    "   <arg name='y' type='i' direction='in'/>"
    "   <arg name='width' type='u' direction='in'/>"
    "   <arg name='height' type='u' direction='in'/>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureScreen'>"
    "   <arg name='name' type='s' direction='in'/>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureActiveScreen'>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureWorkspace'>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='CaptureInteractive'>"
    "   <arg name='kind' type='u' direction='in'/>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='pipe' type='h' direction='in'/>"
    "   <arg name='results' type='a{sv}' direction='out'/>"
    "  </method>"
    " </interface>"
    " <interface name='org.kde.kwin.Effects'>"
    "  <method name='isEffectLoaded'>"
    "   <arg name='name' type='s' direction='in'/>"
    "   <arg name='loaded' type='b' direction='out'/>"
    "  </method>"
    "  <property name='loadedEffects' type='as' access='read'/>"
    " </interface>"
    "</node>";

static GMainLoop *loop;
static GDBusNodeInfo *node_info;
static bool registered;

/* == Subprocessos ========================================================= */

/** O slurp em andamento (um pedido novo o encerra), sob slurp_lock. */
static GMutex slurp_lock;
static pid_t slurp_pid;

/**
 * Roda `argv` (pelo PATH), com `input` na entrada padrão, e lê a saída
 * padrão até `timeout_s` segundos (depois disso, SIGKILL). Com `track`,
 * guarda o pid em slurp_pid enquanto roda.
 * @return A saída (g_free), ou NULL se falhou, passou do prazo ou saiu com
 *     erro. `len_ptr` recebe o tamanho.
 */
static char *run(char *const argv[], const char *input, int timeout_s,
                 bool track, size_t *len_ptr)
{
    int in[2] = { -1, -1 }, out[2];
    if (0 != pipe2(out, O_CLOEXEC)) return NULL;
    if (NULL != input && 0 != pipe2(in, O_CLOEXEC)) {
        close(out[0]);
        close(out[1]);
        return NULL;
    }
    pid_t pid = fork();
    if (0 > pid) {
        close(out[0]);
        close(out[1]);
        if (0 <= in[0]) { close(in[0]); close(in[1]); }
        return NULL;
    }
    if (0 == pid) {
        int null = open("/dev/null", O_RDWR | O_CLOEXEC);
        dup2(0 <= in[0] ? in[0] : null, 0);
        dup2(out[1], 1);
        dup2(null, 2);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(out[1]);
    if (track) {
        g_mutex_lock(&slurp_lock);
        if (0 < slurp_pid) kill(slurp_pid, SIGTERM);
        slurp_pid = pid;
        g_mutex_unlock(&slurp_lock);
    }
    if (0 <= in[0]) {
        close(in[0]);
        // Poucas linhas (caixas das janelas): cabem no buffer do pipe.
        size_t n = strlen(input);
        ssize_t w = 0;
        while (0 < n && (0 < (w = write(in[1], input, n)) || EINTR == errno)) {
            if (0 < w) { input += w; n -= w; }
        }
        close(in[1]);
    }

    GByteArray *buf = g_byte_array_new();
    gint64 deadline = g_get_monotonic_time() + (gint64)timeout_s * G_USEC_PER_SEC;
    bool ok = true;
    for (;;) {
        gint64 left = (deadline - g_get_monotonic_time()) / 1000;
        if (0 >= left) { ok = false; break; }
        struct pollfd p = { .fd = out[0], .events = POLLIN };
        int r = poll(&p, 1, left > 1000 ? 1000 : (int)left);
        if (0 > r && EINTR != errno) { ok = false; break; }
        if (0 >= r) continue;
        guint8 chunk[65536];
        ssize_t n = read(out[0], chunk, sizeof(chunk));
        if (0 > n && EINTR == errno) continue;
        if (0 >= n) break;
        g_byte_array_append(buf, chunk, n);
    }
    close(out[0]);
    if (!ok) kill(pid, SIGKILL);
    int status = 0;
    while (0 > waitpid(pid, &status, 0) && EINTR == errno) {}
    if (track) {
        g_mutex_lock(&slurp_lock);
        if (slurp_pid == pid) slurp_pid = 0;
        g_mutex_unlock(&slurp_lock);
    }
    if (!ok || !WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
        g_byte_array_free(buf, TRUE);
        return NULL;
    }
    *len_ptr = buf->len;
    g_byte_array_append(buf, (const guint8 *)"", 1);
    return (char *)g_byte_array_free(buf, FALSE);
}

/** A saída do wmake-zones com estes argumentos, ou "" (g_free). */
static char *zones(const char *a, const char *b)
{
    char *argv[] = { "wmake-zones", (char *)a, (char *)b, NULL };
    size_t len;
    char *out = run(argv, NULL, ZONES_TIMEOUT, false, &len);
    return NULL != out ? out : g_strdup("");
}

/* == Geometria ============================================================ */

typedef struct { int x, y, w, h; } rect_t;

/** "LxA+X+Y" (também com X ou Y negativos: "1519x985+5231-2") -> rect. */
static bool rect_of(const char *s, rect_t *r)
{
    char sx, sy;
    unsigned w, h, x, y;
    int n = 0;
    if (6 != sscanf(s, "%ux%u%c%u%c%u%n", &w, &h, &sx, &x, &sy, &y, &n)) {
        return false;
    }
    if (('+' != sx && '-' != sx) || ('+' != sy && '-' != sy)) return false;
    // Só espaço em branco depois.
    for (const char *t = s + n; *t; ++t) {
        if (!g_ascii_isspace(*t)) return false;
    }
    r->w = w;
    r->h = h;
    r->x = '-' == sx ? -(int)x : (int)x;
    r->y = '-' == sy ? -(int)y : (int)y;
    return true;
}

/** "LxA+X+Y" -> "X,Y LxA" (grim -g / slurp), ou NULL (g_free). */
static char *box_of(const char *geometry)
{
    rect_t r;
    if (!rect_of(geometry, &r)) return NULL;
    return g_strdup_printf("%d,%d %dx%d", r.x, r.y, r.w, r.h);
}

typedef struct {
    char *name;
    rect_t r;
    double scale;
} output_t;

static void output_free(gpointer p)
{
    output_t *o = p;
    g_free(o->name);
    g_free(o);
}

/** As saídas, pelo compositor (wmake-zones status). GPtrArray de output_t. */
static GPtrArray *outputs(void)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(output_free);
    char *text = zones("status", NULL);
    char **lines = g_strsplit(text, "\n", -1);
    for (char **l = lines; NULL != *l; ++l) {
        if (!g_str_has_prefix(*l, "monitor=")) continue;
        char **f = g_strsplit(*l + strlen("monitor="), " ", -1);
        rect_t r;
        if (NULL != f[0] && NULL != f[1] && rect_of(f[1], &r)) {
            output_t *o = g_new0(output_t, 1);
            o->name = g_strdup(f[0]);
            o->r = r;
            o->scale = 1.0;
            for (char **k = f; NULL != *k; ++k) {
                if (g_str_has_prefix(*k, "scale=")) {
                    double s = g_ascii_strtod(*k + strlen("scale="), NULL);
                    if (0.0 < s) o->scale = s;
                }
            }
            g_ptr_array_add(a, o);
        }
        g_strfreev(f);
    }
    g_strfreev(lines);
    g_free(text);
    return a;
}

/** A escala da saída `name` (1.0 se não se sabe). */
static double output_scale(const char *name)
{
    GPtrArray *a = outputs();
    double s = 1.0;
    for (guint i = 0; i < a->len; ++i) {
        output_t *o = g_ptr_array_index(a, i);
        if (0 == g_strcmp0(o->name, name)) s = o->scale;
    }
    g_ptr_array_unref(a);
    return s;
}

/** O nome da saída em `x`, `y` (g_free), ou NULL. */
static char *output_at(GPtrArray *a, int x, int y)
{
    for (guint i = 0; i < a->len; ++i) {
        output_t *o = g_ptr_array_index(a, i);
        if (o->r.x <= x && x < o->r.x + o->r.w &&
            o->r.y <= y && y < o->r.y + o->r.h) return g_strdup(o->name);
    }
    return NULL;
}

/** A geometria da janela ativa ("LxA+X+Y", g_free), ou NULL. */
static char *active_geometry(void)
{
    char *text = zones("active", NULL);
    char *g = NULL;
    if ('\0' != text[0] && !g_str_has_prefix(text, "error=")) {
        size_t n = strcspn(text, "\t\n");
        g = g_strndup(text, n);
    }
    g_free(text);
    return g;
}

/**
 * A saída sob o ponteiro (o "monitor atual" do KWin). O wlmaker com o
 * comando "pointer" do wmake-zones a informa; um anterior não: vale a saída
 * da janela ativa, depois a primeira. g_free, ou NULL.
 */
static char *pointer_output(void)
{
    char *text = zones("pointer", NULL);
    char **f = g_strsplit_set(g_strstrip(text), " \t", -1);
    char *name = NULL;
    if (3 <= g_strv_length(f) && !g_str_has_prefix(f[0], "error=")) {
        name = g_strdup(f[2]);
    }
    g_strfreev(f);
    g_free(text);
    if (NULL != name) return name;

    GPtrArray *a = outputs();
    char *g = active_geometry();
    rect_t r;
    if (NULL != g && rect_of(g, &r)) {
        name = output_at(a, r.x + r.w / 2, r.y + r.h / 2);
    }
    g_free(g);
    if (NULL == name && 0 < a->len) {
        name = g_strdup(((output_t *)g_ptr_array_index(a, 0))->name);
    }
    g_ptr_array_unref(a);
    return name;
}

/**
 * As caixas das janelas para a escolha, uma por linha (a ativa primeiro: o
 * slurp fica com a primeira que contém o ponteiro), só as que aparecem numa
 * saída. g_free.
 */
static char *window_boxes(void)
{
    GPtrArray *a = outputs();
    char *active = active_geometry();
    char *text = zones("windows", "-l");
    GString *first = g_string_new(NULL), *rest = g_string_new(NULL);
    char **lines = g_strsplit(text, "\n", -1);
    for (char **l = lines; NULL != *l; ++l) {
        char **f = g_strsplit(*l, "\t", -1);
        rect_t r;
        if (4 <= g_strv_length(f) && rect_of(f[3], &r) && 0 < r.w && 0 < r.h) {
            int pts[3][2] = { { r.x + r.w / 2, r.y + r.h / 2 }, { r.x, r.y },
                              { r.x + r.w - 1, r.y + r.h - 1 } };
            bool seen = 0 == a->len;  // Sem saídas conhecidas: todas.
            for (int k = 0; !seen && k < 3; ++k) {
                char *o = output_at(a, pts[k][0], pts[k][1]);
                seen = NULL != o;
                g_free(o);
            }
            if (seen) {
                GString *s = 0 == g_strcmp0(f[3], active) ? first : rest;
                g_string_append_printf(s, "%d,%d %dx%d\n", r.x, r.y, r.w, r.h);
            }
        }
        g_strfreev(f);
    }
    g_strfreev(lines);
    g_free(text);
    g_free(active);
    g_ptr_array_unref(a);
    g_string_append(first, rest->str);
    g_string_free(rest, TRUE);
    return g_string_free(first, FALSE);
}

/**
 * Escolha interativa: 0 = janela (as caixas das janelas), 1 = tela. Esc
 * cancela; um pedido novo encerra o anterior; nunca passa do prazo.
 * @return "X,Y LxA" (g_free), ou NULL se cancelada.
 */
static char *slurp(guint32 kind)
{
    char *argv1[] = { "slurp", "-o", "-f", "%x,%y %wx%h", NULL };
    char *argv0[] = { "slurp", "-r", "-f", "%x,%y %wx%h", NULL };
    char *boxes = 1 == kind ? NULL : window_boxes();
    size_t len;
    char *out = run(1 == kind ? argv1 : argv0, 1 == kind ? NULL : boxes,
                    SLURP_TIMEOUT, true, &len);
    g_free(boxes);
    if (NULL == out) return NULL;
    g_strstrip(out);
    if ('\0' == out[0]) {
        g_free(out);
        return NULL;
    }
    return out;
}

/* == Captura ============================================================== */

typedef struct {
    int w, h;
    guint8 *bgra;  // w * h * 4
} image_t;

/** O grim com `args`; a imagem em BGRA. NULL se falhou. */
static image_t *grim(const char *const *args, bool cursor)
{
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, "grim");
    g_ptr_array_add(argv, "-t");
    g_ptr_array_add(argv, "ppm");
    if (cursor) g_ptr_array_add(argv, "-c");
    for (const char *const *a = args; NULL != a && NULL != *a; ++a) {
        g_ptr_array_add(argv, (gpointer)*a);
    }
    g_ptr_array_add(argv, "-");
    g_ptr_array_add(argv, NULL);
    size_t len = 0;
    char *out = run((char *const *)argv->pdata, NULL, GRIM_TIMEOUT, false, &len);
    g_ptr_array_free(argv, TRUE);
    if (NULL == out) return NULL;

    // P6\n<w> <h>\n255\n<dados>: o cabeçalho pelo texto, o corpo pela
    // posição (um byte do corpo pode valer um espaço em branco).
    unsigned w = 0, h = 0, max = 0;
    int n = 0;
    image_t *img = NULL;
    if (3 == sscanf(out, "P6 %u %u %u%n", &w, &h, &max, &n) && 255 == max &&
        (size_t)n < len && g_ascii_isspace(out[n]) &&
        0 < w && 0 < h && w < 65536 && h < 65536) {
        const guint8 *rgb = (const guint8 *)out + n + 1;
        size_t pixels = (size_t)w * h;
        if (len - (n + 1) >= 3 * pixels) {
            img = g_new0(image_t, 1);
            img->w = w;
            img->h = h;
            img->bgra = g_malloc(4 * pixels);
            for (size_t i = 0; i < pixels; ++i) {
                img->bgra[4 * i + 0] = rgb[3 * i + 2];
                img->bgra[4 * i + 1] = rgb[3 * i + 1];
                img->bgra[4 * i + 2] = rgb[3 * i + 0];
                img->bgra[4 * i + 3] = 0xff;
            }
        }
    }
    g_free(out);
    return img;
}

static void image_free(image_t *img)
{
    if (NULL == img) return;
    g_free(img->bgra);
    g_free(img);
}

/** O "include-cursor" das opções (a{sv}). */
static bool include_cursor(GVariant *options)
{
    gboolean v = FALSE;
    if (NULL != options) g_variant_lookup(options, "include-cursor", "b", &v);
    return v;
}

static void error(GDBusMethodInvocation *inv, const char *name, const char *text)
{
    char *full = g_strconcat(ERROR_PREFIX, name, NULL);
    g_dbus_method_invocation_return_dbus_error(inv, full, text);
    g_free(full);
}

/**
 * Responde com a descrição da imagem e escreve os bytes no pipe (o
 * descritor `pipe_index` da mensagem). Sempre responde: o Spectacle espera.
 */
static void reply(GDBusMethodInvocation *inv, image_t *img, gint32 pipe_index,
                  const char *screen)
{
    if (NULL == img) {
        error(inv, "InternalError", "grim falhou");
        return;
    }
    GUnixFDList *list = g_dbus_message_get_unix_fd_list(
        g_dbus_method_invocation_get_message(inv));
    if (NULL == list) {
        error(inv, "InvalidArguments", "sem o descritor do pipe");
        return;
    }
    // Os descritores passam a ser só nossos (steal): o do pipe é fechado ao
    // fim da escrita, e quem lê recebe o fim do arquivo na hora.
    int count = 0;
    int *fds = g_unix_fd_list_steal_fds(list, &count);
    int fd = (0 <= pipe_index && pipe_index < count) ? fds[pipe_index] : -1;
    for (int i = 0; i < count; ++i) {
        if (fds[i] != fd) close(fds[i]);
    }
    g_free(fds);
    if (0 > fd) {
        error(inv, "InvalidArguments", "descritor do pipe inválido");
        return;
    }
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&b, "{sv}", "type", g_variant_new_string("raw"));
    g_variant_builder_add(&b, "{sv}", "width", g_variant_new_uint32(img->w));
    g_variant_builder_add(&b, "{sv}", "height", g_variant_new_uint32(img->h));
    g_variant_builder_add(&b, "{sv}", "stride", g_variant_new_uint32(4 * img->w));
    g_variant_builder_add(&b, "{sv}", "format", g_variant_new_uint32(FORMAT_RGB32));
    g_variant_builder_add(&b, "{sv}", "scale", g_variant_new_double(
                              NULL != screen ? output_scale(screen) : 1.0));
    // Sem a saída, o Spectacle põe cada tela em (0, 0) ao juntar "Todas as
    // telas" (uma cobria as outras).
    if (NULL != screen) {
        g_variant_builder_add(&b, "{sv}", "screen", g_variant_new_string(screen));
    }
    g_dbus_method_invocation_return_value(inv, g_variant_new("(a{sv})", &b));

    // Os bytes depois da resposta (já numa thread de trabalho).
    const guint8 *p = img->bgra;
    size_t left = (size_t)img->w * img->h * 4;
    while (0 < left) {
        ssize_t n = write(fd, p, left);
        if (0 > n && EINTR == errno) continue;
        if (0 >= n) break;
        p += n;
        left -= n;
    }
    close(fd);
}

typedef struct {
    GDBusMethodInvocation *inv;
    char *method;
    GVariant *params;
} job_t;

/** Uma captura, numa thread (o slurp espera o usuário; o grim, o disco). */
static gpointer capture(gpointer data)
{
    job_t *job = data;
    GVariant *params = job->params;
    const char *m = job->method;
    image_t *img = NULL;
    char *screen = NULL;
    GVariant *opts = NULL;
    gint32 pipe = -1;

    if (0 == strcmp(m, "CaptureArea")) {
        gint32 x, y;
        guint32 w, h;
        g_variant_get(params, "(iiuu@a{sv}h)", &x, &y, &w, &h, &opts, &pipe);
        char *box = g_strdup_printf("%d,%d %ux%u", x, y, w, h);
        const char *args[] = { "-g", box, NULL };
        img = grim(args, include_cursor(opts));
        g_free(box);
    } else if (0 == strcmp(m, "CaptureScreen")) {
        const char *name;
        g_variant_get(params, "(&s@a{sv}h)", &name, &opts, &pipe);
        const char *args[] = { "-o", name, NULL };
        img = grim('\0' != name[0] ? args : NULL, include_cursor(opts));
        if ('\0' != name[0]) screen = g_strdup(name);
    } else if (0 == strcmp(m, "CaptureActiveWindow") ||
               0 == strcmp(m, "CaptureWindow")) {
        if (0 == strcmp(m, "CaptureWindow")) {
            const char *handle;
            g_variant_get(params, "(&s@a{sv}h)", &handle, &opts, &pipe);
        } else {
            g_variant_get(params, "(@a{sv}h)", &opts, &pipe);
        }
        char *g = active_geometry();
        char *box = NULL != g ? box_of(g) : NULL;
        const char *args[] = { "-g", box, NULL };
        img = grim(NULL != box ? args : NULL, include_cursor(opts));
        g_free(box);
        g_free(g);
    } else if (0 == strcmp(m, "CaptureInteractive")) {
        guint32 kind;
        g_variant_get(params, "(u@a{sv}h)", &kind, &opts, &pipe);
        char *box = slurp(kind);
        if (NULL == box) {
            error(job->inv, "Cancelled", "cancelado");
            goto done;
        }
        const char *args[] = { "-g", box, NULL };
        img = grim(args, include_cursor(opts));
        if (1 == kind) {
            int x, y;
            if (2 == sscanf(box, "%d,%d", &x, &y)) {
                GPtrArray *a = outputs();
                screen = output_at(a, x, y);
                g_ptr_array_unref(a);
            }
        }
        g_free(box);
    } else if (0 == strcmp(m, "CaptureActiveScreen")) {
        // O monitor atual: o do ponteiro (não a área de trabalho toda).
        g_variant_get(params, "(@a{sv}h)", &opts, &pipe);
        screen = pointer_output();
        const char *args[] = { "-o", screen, NULL };
        img = grim(NULL != screen ? args : NULL, include_cursor(opts));
    } else {  // CaptureWorkspace: todas as telas.
        g_variant_get(params, "(@a{sv}h)", &opts, &pipe);
        img = grim(NULL, include_cursor(opts));
    }
    reply(job->inv, img, pipe, screen);

done:
    image_free(img);
    g_free(screen);
    if (NULL != opts) g_variant_unref(opts);
    g_variant_unref(job->params);
    g_free(job->method);
    g_free(job);
    return NULL;
}

/* == D-Bus ================================================================ */

static void on_call(GDBusConnection *conn, const char *sender,
                    const char *path, const char *iface, const char *method,
                    GVariant *params, GDBusMethodInvocation *inv,
                    gpointer data)
{
    (void)conn; (void)sender; (void)path; (void)data;
    if (0 == strcmp(iface, "org.kde.kwin.Effects")) {
        if (0 == strcmp(method, "isEffectLoaded")) {
            const char *name;
            g_variant_get(params, "(&s)", &name);
            g_dbus_method_invocation_return_value(
                inv, g_variant_new("(b)", 0 == strcmp(name, "screenshot")));
        } else {
            g_dbus_method_invocation_return_dbus_error(
                inv, "org.freedesktop.DBus.Error.UnknownMethod", method);
        }
        return;
    }
    job_t *job = g_new0(job_t, 1);
    job->inv = inv;
    job->method = g_strdup(method);
    job->params = g_variant_ref(params);
    GThread *t = g_thread_try_new("captura", capture, job, NULL);
    if (NULL == t) {
        error(inv, "InternalError", "sem thread");
        g_variant_unref(job->params);
        g_free(job->method);
        g_free(job);
        return;
    }
    g_thread_unref(t);
}

static GVariant *on_get_property(GDBusConnection *conn, const char *sender,
                                 const char *path, const char *iface,
                                 const char *prop, GError **err, gpointer data)
{
    (void)conn; (void)sender; (void)path; (void)iface; (void)data;
    if (0 == strcmp(prop, "Version")) return g_variant_new_uint32(VERSION);
    if (0 == strcmp(prop, "loadedEffects")) {
        const char *effects[] = { "screenshot", NULL };
        return g_variant_new_strv(effects, -1);
    }
    g_set_error(err, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY, "%s", prop);
    return NULL;
}

static const GDBusInterfaceVTable vtable = {
    .method_call = on_call,
    .get_property = on_get_property,
};

static void on_bus(GDBusConnection *conn, const char *name, gpointer data)
{
    (void)name; (void)data;
    if (registered) return;
    registered = true;
    g_dbus_connection_register_object(conn, SCREENSHOT_PATH,
                                      node_info->interfaces[0], &vtable,
                                      NULL, NULL, NULL);
    g_dbus_connection_register_object(conn, EFFECTS_PATH,
                                      node_info->interfaces[1], &vtable,
                                      NULL, NULL, NULL);
}

static void on_lost(GDBusConnection *conn, const char *name, gpointer data)
{
    (void)conn; (void)name; (void)data;
    // Outro serviço (o KWin de verdade) tem o nome: nada a fazer aqui.
    g_main_loop_quit(loop);
}

static gboolean on_signal(gpointer data)
{
    (void)data;
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static bool wlmaker_session(void)
{
    const char *d = g_getenv("XDG_CURRENT_DESKTOP");
    const char *w = g_getenv("WAYLAND_DISPLAY");
    if (NULL == d || NULL == w || '\0' == w[0]) return false;
    char **parts = g_strsplit(d, ":", -1);
    bool found = g_strv_contains((const char *const *)parts, "wlmaker");
    g_strfreev(parts);
    return found;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "--check")) return wlmaker_session() ? 0 : 1;
    }
    if (!wlmaker_session()) return 0;
    // Uma escrita no pipe de quem desistiu não derruba o serviço.
    signal(SIGPIPE, SIG_IGN);

    GError *err = NULL;
    node_info = g_dbus_node_info_new_for_xml(introspection_xml, &err);
    if (NULL == node_info) {
        g_printerr("big-wm-kwin-screenshot: %s\n", err->message);
        return 1;
    }
    loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGTERM, on_signal, NULL);
    g_unix_signal_add(SIGINT, on_signal, NULL);
    // O Spectacle procura o dono de org.kde.KWin.ScreenShot2 (o KWin
    // registra esse nome à parte) e fala com org.kde.KWin (o /Effects).
    static const char *const services[] = {
        "org.kde.KWin.ScreenShot2", "org.kde.KWin" };
    guint ids[2];
    for (int i = 0; i < 2; ++i) {
        ids[i] = g_bus_own_name(G_BUS_TYPE_SESSION, services[i],
                                G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
                                on_bus, NULL, on_lost, NULL, NULL);
    }
    g_main_loop_run(loop);
    for (int i = 0; i < 2; ++i) g_bus_unown_name(ids[i]);
    g_main_loop_unref(loop);
    g_dbus_node_info_unref(node_info);
    return 0;
}
