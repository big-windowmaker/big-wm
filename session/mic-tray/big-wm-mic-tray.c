/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Microfone na bandeja das sessões Big WindowMaker: o filtro de ruído do
 * BigLinux (biglinux-noise-reduction-pipewire, "Filtrar ruídos") como item
 * StatusNotifierItem, na bandeja de cada sessão (wmsystemtray + snixembed no
 * X11, big-wm-tray no Wayland). No Plasma o mesmo papel é do plasmoide
 * br.com.biglinux.micnoise do pacote; o aplicativo GTK (biglinux-microphone)
 * não tem modo bandeja.
 *
 *   big-wm-mic-tray            inicia o item (um só por sessão)
 *   big-wm-mic-tray --state    mostra o estado lido agora, sem bandeja (testes)
 *
 * Em C (GIO, libdbusmenu-glib, json-glib) desde novembro de 2026: a versão
 * em Python (PyGObject) ocupava ~16 MiB de PSS em toda sessão, X11 e
 * Wayland, para um ícone. O comportamento é o mesmo, coberto pelo
 * tests/mic-tray/test_mic_tray.py (caixa-preta).
 *
 * O que aparece vem só de fontes confiáveis:
 *   - "biglinux-microphone-cli status" (o mesmo JSON do plasmoide):
 *     mic_enabled/mic_running, output_enabled/output_running,
 *     audio_available;
 *   - "wpctl get-volume @DEFAULT_AUDIO_SOURCE@": [MUTED] no microfone padrão.
 * Ícone: microfone (audio-input-microphone-symbolic) ou microfone silenciado
 * (microphone-sensitivity-muted-symbolic); com um filtro ligado que não está
 * rodando, ou sem áudio, o item pede atenção (dialog-warning-symbolic). Sem
 * wpctl o estado de silêncio não é mostrado (nada inventado).
 *
 * Cliques, iguais nas duas bandejas: esquerdo abre as configurações
 * (biglinux-microphone), o do meio silencia ou reativa o microfone padrão
 * (wpctl), o direito abre o menu (dbusmenu): Filtro do microfone, Filtro do
 * som do sistema ("biglinux-microphone-cli set mic|output on|off", que grava
 * no settings.json do aplicativo: as escolhas valem nas próximas sessões),
 * Silenciar microfone, Abrir configurações..., Iniciar com a sessão e
 * Fechar. (O snixembed do X11 não abre o menu no botão esquerdo nem com
 * ItemIsMenu; por isso o item não usa ItemIsMenu.)
 *
 * "Iniciar com a sessão" desmarcado grava ~/.config/autostart/
 * big-wm-mic-tray.desktop com Hidden=true (a forma do XDG de desligar um
 * autostart do sistema, que o systemd-xdg-autostart-generator respeita).
 *
 * Atualização: o diretório de configuração do aplicativo é vigiado
 * (inotify, sem processo) e, com o pactl, um "pactl subscribe" avisa na hora
 * de mudanças nas fontes de áudio e de saídas que aparecem ou somem: os
 * filtros são uma fonte (mic-biglinux) e uma saída do PipeWire, e um filtro
 * que cai some com elas. Sem o pactl (ou enquanto ele é reiniciado), uma
 * leitura a cada 7 s, como o plasmoide.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Textos: catálogo "big-wm" (po/ na raiz do big-wm), msgids em inglês.
 * BIGWM_LOCALEDIR: catálogos compilados fora de /usr/share/locale (testes). */
#define _(s) gettext(s)
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#define BUS_NAME "org.bigwindowmaker.MicTray"
#define ITEM_PATH "/StatusNotifierItem"
#define ITEM_IFACE "org.kde.StatusNotifierItem"
#define MENU_PATH "/MenuBar"
#define CLI "biglinux-microphone-cli"
#define APP "biglinux-microphone"

#define ICON_LIVE "audio-input-microphone-symbolic"
#define ICON_MUTED "microphone-sensitivity-muted-symbolic"
#define ICON_ATTENTION "dialog-warning-symbolic"

static const char item_xml[] =
    "<node>"
    " <interface name='org.kde.StatusNotifierItem'>"
    "  <method name='Activate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='SecondaryActivate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='ContextMenu'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='Scroll'><arg type='i' direction='in'/><arg type='s' direction='in'/></method>"
    "  <property name='Category' type='s' access='read'/>"
    "  <property name='Id' type='s' access='read'/>"
    "  <property name='Title' type='s' access='read'/>"
    "  <property name='Status' type='s' access='read'/>"
    "  <property name='WindowId' type='i' access='read'/>"
    "  <property name='IconName' type='s' access='read'/>"
    "  <property name='IconThemePath' type='s' access='read'/>"
    "  <property name='IconPixmap' type='a(iiay)' access='read'/>"
    "  <property name='OverlayIconName' type='s' access='read'/>"
    "  <property name='AttentionIconName' type='s' access='read'/>"
    "  <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "  <property name='ItemIsMenu' type='b' access='read'/>"
    "  <property name='Menu' type='o' access='read'/>"
    "  <signal name='NewTitle'/>"
    "  <signal name='NewIcon'/>"
    "  <signal name='NewAttentionIcon'/>"
    "  <signal name='NewToolTip'/>"
    "  <signal name='NewStatus'><arg type='s'/></signal>"
    " </interface>"
    "</node>";

/* == Leitura do estado (sem D-Bus) ======================================== */

typedef struct {
    gboolean mic_enabled, output_enabled, mic_running, output_running, audio_available;
} mic_status_t;

typedef enum { MUTED_UNKNOWN = -1, MUTED_NO = 0, MUTED_YES = 1 } muted_t;

/* O que o item mostra: Status SNI, ícone, título e texto do balão. */
typedef struct {
    const char *status;
    const char *icon;
    char *title;
    char *body;
} sni_t;

static gboolean
json_bool(JsonObject *o, const char *key, gboolean *out)
{
    JsonNode *n = json_object_get_member(o, key);
    if (!n || !JSON_NODE_HOLDS_VALUE(n) || json_node_get_value_type(n) != G_TYPE_BOOLEAN)
        return FALSE;
    *out = json_node_get_boolean(n);
    return TRUE;
}

/* JSON do "biglinux-microphone-cli status"; FALSE se o formato não é o
 * esperado (o plasmoide faz a mesma checagem). */
static gboolean
parse_status(const char *text, mic_status_t *st)
{
    g_autoptr(JsonParser) parser = json_parser_new();
    if (!text || !json_parser_load_from_data(parser, text, -1, NULL))
        return FALSE;
    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root))
        return FALSE;
    JsonObject *o = json_node_get_object(root);
    mic_status_t s = {0};
    if (!json_bool(o, "mic_enabled", &s.mic_enabled) || !json_bool(o, "output_enabled", &s.output_enabled))
        return FALSE;
    gboolean v;
    s.mic_running = json_bool(o, "mic_running", &v) && v;
    s.output_running = json_bool(o, "output_running", &v) && v;
    s.audio_available = json_bool(o, "audio_available", &v) && v;
    *st = s;
    return TRUE;
}

/* Saída do "wpctl get-volume" (sem os espaços das pontas). */
static muted_t
parse_muted(const char *raw)
{
    if (!raw)
        return MUTED_UNKNOWN;
    g_autofree char *text = g_strstrip(g_strdup(raw));
    if (!g_str_has_prefix(text, "Volume:"))
        return MUTED_UNKNOWN;
    return strstr(text, "[MUTED]") ? MUTED_YES : MUTED_NO;
}

/* Linha do "pactl subscribe" que muda o que o item mostra. Fonte: tudo
 * (silêncio, fonte padrão, o filtro do microfone que cai). Saída: só a que
 * aparece ou some (o filtro do som do sistema); o volume não. */
static gboolean
event_matters(const char *line)
{
    if (strstr(line, " on source ") || strstr(line, " on server"))
        return TRUE;
    return strstr(line, " on sink ") && !g_str_has_prefix(line, "Event 'change'");
}

static void
describe(const mic_status_t *st, muted_t muted, sni_t *out)
{
    out->title = g_strdup(_("Microphone"));
    if (!st) {
        out->status = "NeedsAttention";
        out->icon = ICON_ATTENTION;
        out->body = g_strdup(_("Could not read the audio status. Open the settings."));
        return;
    }
    GString *b = g_string_new(NULL);
    if (muted == MUTED_YES)
        g_string_append_printf(b, "%s\n", _("Microphone muted"));
    else if (muted == MUTED_NO)
        g_string_append_printf(b, "%s\n", _("Microphone active"));
    g_string_append_printf(b, "%s%s\n", _("Microphone filter: "), st->mic_enabled ? _("on") : _("off"));
    g_string_append_printf(b, "%s%s", _("System sound filter: "), st->output_enabled ? _("on") : _("off"));
    gboolean attention = !st->audio_available || (st->mic_enabled && !st->mic_running) ||
                         (st->output_enabled && !st->output_running);
    if (attention) {
        g_string_append_printf(b, "\n%s", _("Some filters are not ready: open the settings."));
        out->status = "NeedsAttention";
        out->icon = ICON_ATTENTION;
    } else {
        out->status = "Active";
        out->icon = muted == MUTED_YES ? ICON_MUTED : ICON_LIVE;
    }
    out->body = g_string_free(b, FALSE);
}

static void
sni_clear(sni_t *s)
{
    g_clear_pointer(&s->title, g_free);
    g_clear_pointer(&s->body, g_free);
}

static char *autostart_path;

static gboolean
autostart_enabled(void)
{
    g_autofree char *text = NULL;
    if (!g_file_get_contents(autostart_path, &text, NULL, NULL))
        return TRUE;
    g_auto(GStrv) lines = g_strsplit(text, "\n", -1);
    for (char **l = lines; *l; l++) {
        char *eq = strchr(*l, '=');
        if (!eq)
            continue;
        *eq = '\0';
        g_autofree char *val = g_ascii_strdown(g_strstrip(eq + 1), -1);
        if (strcmp(g_strstrip(*l), "Hidden") == 0 && strcmp(val, "true") == 0)
            return FALSE;
    }
    return TRUE;
}

static void
set_autostart(gboolean enabled)
{
    if (enabled) {
        if (unlink(autostart_path) != 0 && errno != ENOENT)
            g_warning("%s: %s", autostart_path, g_strerror(errno));
        return;
    }
    g_autofree char *dir = g_path_get_dirname(autostart_path);
    g_mkdir_with_parents(dir, 0755);
    g_autoptr(GError) err = NULL;
    if (!g_file_set_contents(autostart_path,
                             "[Desktop Entry]\nType=Application\nName=Big WindowMaker: microfone na bandeja\n"
                             "Exec=big-wm-mic-tray\nHidden=true\n",
                             -1, &err))
        g_warning("%s", err->message);
}

/* == Processos ============================================================= */

typedef void (*run_cb)(int code, const char *out, gpointer data);

typedef struct {
    run_cb cb;
    gpointer data;
} run_t;

static void
run_done(GObject *src, GAsyncResult *res, gpointer user)
{
    GSubprocess *p = G_SUBPROCESS(src);
    run_t *r = user;
    g_autoptr(GBytes) out = NULL;
    g_subprocess_communicate_finish(p, res, &out, NULL, NULL);
    int code = g_subprocess_get_if_exited(p) ? g_subprocess_get_exit_status(p) : -1;
    g_autofree char *text = NULL;
    if (out) {
        gsize n;
        const char *d = g_bytes_get_data(out, &n);
        text = g_strndup(d ? d : "", n);
    }
    if (r->cb)
        r->cb(code, text ? text : "", r->data);
    g_free(r);
    g_object_unref(p);
}

/* GSubprocess sem travar o laço; cb(código, saída). */
static void
run_async(const char *const *argv, run_cb cb, gpointer data)
{
    GSubprocess *p = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                       NULL);
    if (!p) {
        if (cb)
            cb(127, "", data);
        return;
    }
    run_t *r = g_new0(run_t, 1);
    r->cb = cb;
    r->data = data;
    g_subprocess_communicate_async(p, NULL, NULL, run_done, r);
}

/* Abre um programa sem esperar; o GLib recolhe o processo (sem zumbi). */
static void
spawn_detached(const char *prog)
{
    char *argv[] = {(char *)prog, NULL};
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
                  NULL, NULL);
}

/* == O item ================================================================ */

static GDBusConnection *bus;
static GMainLoop *loop;
static gboolean has_wpctl, have_status, busy, stopped;
static mic_status_t status;
static muted_t muted = MUTED_UNKNOWN;
static sni_t sni;
static int fast, pending;
static guint refresh_id;
static GSubprocess *subscribe;
static GCancellable *subscribe_cancel;

static DbusmenuMenuitem *menu_root, *notice, *notice_sep, *mic_item, *out_item, *mute_sep, *mute_item,
    *auto_item;

static void refresh(void);
static void update_menu(void);

static void
emit(const char *name, GVariant *args)
{
    g_dbus_connection_emit_signal(bus, NULL, ITEM_PATH, ITEM_IFACE, name, args, NULL);
}

static GVariant *
get_prop(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *prop,
         GError **error, gpointer data)
{
    (void)c, (void)sender, (void)path, (void)iface, (void)data;
    if (!strcmp(prop, "Category"))
        return g_variant_new_string("Hardware");
    if (!strcmp(prop, "Id"))
        return g_variant_new_string("big-wm-mic-tray");
    if (!strcmp(prop, "Title"))
        return g_variant_new_string(_("Microphone (noise filter)"));
    if (!strcmp(prop, "Status"))
        return g_variant_new_string(sni.status);
    if (!strcmp(prop, "WindowId"))
        return g_variant_new_int32(0);
    if (!strcmp(prop, "IconName"))
        return g_variant_new_string(sni.icon);
    if (!strcmp(prop, "IconThemePath") || !strcmp(prop, "OverlayIconName"))
        return g_variant_new_string("");
    if (!strcmp(prop, "IconPixmap"))
        return g_variant_new_array(G_VARIANT_TYPE("(iiay)"), NULL, 0);
    if (!strcmp(prop, "AttentionIconName"))
        return g_variant_new_string(ICON_ATTENTION);
    if (!strcmp(prop, "ToolTip"))
        return g_variant_new("(s@a(iiay)ss)", sni.icon, g_variant_new_array(G_VARIANT_TYPE("(iiay)"), NULL, 0),
                             sni.title, sni.body);
    if (!strcmp(prop, "ItemIsMenu"))
        return g_variant_new_boolean(FALSE);
    if (!strcmp(prop, "Menu"))
        return g_variant_new_object_path(MENU_PATH);
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY, "%s", prop);
    return NULL;
}

static void
mute_done(int code, const char *out, gpointer data)
{
    (void)code, (void)out, (void)data;
    refresh();
}

static void
toggle_mute(void)
{
    const char *argv[] = {"wpctl", "set-mute", "@DEFAULT_AUDIO_SOURCE@", "toggle", NULL};
    run_async(argv, mute_done, NULL);
}

static void
call(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method,
     GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    (void)c, (void)sender, (void)path, (void)iface, (void)params, (void)data;
    if (!strcmp(method, "Activate"))
        spawn_detached(APP);
    else if (!strcmp(method, "SecondaryActivate") && has_wpctl)
        toggle_mute();
    g_dbus_method_invocation_return_value(inv, NULL);
}

static void
watcher_appeared(GDBusConnection *c, const char *name, const char *owner, gpointer data)
{
    (void)name, (void)owner, (void)data;
    /* Bandeja nova (login, big-wm-tray reiniciado, snixembed): registrar de
     * novo. O nome único deste processo serve de serviço. */
    g_dbus_connection_call(c, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                           "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
                           g_variant_new("(s)", ITEM_PATH), NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL,
                           NULL);
}

/* -- menu ---------------------------------------------------------------- */

static DbusmenuMenuitem *
item(const char *label, gboolean toggle, gboolean sensitive)
{
    DbusmenuMenuitem *mi = dbusmenu_menuitem_new();
    dbusmenu_menuitem_property_set(mi, DBUSMENU_MENUITEM_PROP_LABEL, label);
    if (toggle)
        dbusmenu_menuitem_property_set(mi, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE, DBUSMENU_MENUITEM_TOGGLE_CHECK);
    dbusmenu_menuitem_property_set_bool(mi, DBUSMENU_MENUITEM_PROP_ENABLED, sensitive);
    dbusmenu_menuitem_child_append(menu_root, mi);
    g_object_unref(mi);
    return mi;
}

static DbusmenuMenuitem *
separator(void)
{
    DbusmenuMenuitem *mi = dbusmenu_menuitem_new();
    dbusmenu_menuitem_property_set(mi, DBUSMENU_MENUITEM_PROP_TYPE, "separator");
    dbusmenu_menuitem_child_append(menu_root, mi);
    g_object_unref(mi);
    return mi;
}

static void
check(DbusmenuMenuitem *mi, gboolean on)
{
    dbusmenu_menuitem_property_set_int(mi, DBUSMENU_MENUITEM_PROP_TOGGLE_STATE,
                                       on ? DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED
                                          : DBUSMENU_MENUITEM_TOGGLE_STATE_UNCHECKED);
}

static void
update_menu(void)
{
    gboolean ready = have_status && status.audio_available && !busy;
    check(mic_item, have_status && status.mic_enabled);
    check(out_item, have_status && status.output_enabled);
    dbusmenu_menuitem_property_set_bool(mic_item, DBUSMENU_MENUITEM_PROP_ENABLED, ready);
    dbusmenu_menuitem_property_set_bool(out_item, DBUSMENU_MENUITEM_PROP_ENABLED, ready);
    check(mute_item, muted == MUTED_YES);
    dbusmenu_menuitem_property_set_bool(mute_item, DBUSMENU_MENUITEM_PROP_ENABLED, muted != MUTED_UNKNOWN);
    check(auto_item, autostart_enabled());
    /* Aviso no topo: a última linha do balão quando o item pede atenção. */
    const char *text = "";
    if (busy)
        text = _("Applying...");
    else if (!strcmp(sni.status, "NeedsAttention") && sni.body && *sni.body) {
        const char *nl = strrchr(sni.body, '\n');
        text = nl ? nl + 1 : sni.body;
    }
    dbusmenu_menuitem_property_set(notice, DBUSMENU_MENUITEM_PROP_LABEL, text);
    dbusmenu_menuitem_property_set_bool(notice, DBUSMENU_MENUITEM_PROP_VISIBLE, *text != '\0');
    dbusmenu_menuitem_property_set_bool(notice_sep, DBUSMENU_MENUITEM_PROP_VISIBLE, *text != '\0');
}

static void
filter_done(int code, const char *out, gpointer data)
{
    (void)code, (void)out, (void)data;
    busy = FALSE;
    fast = 3;
    refresh();
}

static void
toggle_filter(DbusmenuMenuitem *mi, guint timestamp, gpointer data)
{
    (void)mi, (void)timestamp;
    const char *key = data;
    if (busy || !have_status)
        return;
    gboolean enabled = !(strcmp(key, "mic") == 0 ? status.mic_enabled : status.output_enabled);
    busy = TRUE;
    update_menu();
    const char *argv[] = {CLI, "set", key, enabled ? "on" : "off", NULL};
    run_async(argv, filter_done, NULL);
}

static void
on_mute(DbusmenuMenuitem *mi, guint timestamp, gpointer data)
{
    (void)mi, (void)timestamp, (void)data;
    toggle_mute();
}

static void
on_settings(DbusmenuMenuitem *mi, guint timestamp, gpointer data)
{
    (void)mi, (void)timestamp, (void)data;
    spawn_detached(APP);
}

static void
on_autostart(DbusmenuMenuitem *mi, guint timestamp, gpointer data)
{
    (void)mi, (void)timestamp, (void)data;
    set_autostart(!autostart_enabled());
    update_menu();
}

static gboolean
quit_idle(gpointer data)
{
    (void)data;
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void
on_close(DbusmenuMenuitem *mi, guint timestamp, gpointer data)
{
    (void)mi, (void)timestamp, (void)data;
    /* Depois da resposta ao Event do dbusmenu (o clique em Fechar): saindo
     * dentro dele, quem clicou recebia NoReply. */
    g_idle_add(quit_idle, NULL);
}

static DbusmenuServer *
build_menu(void)
{
    DbusmenuServer *server = dbusmenu_server_new(MENU_PATH);
    menu_root = dbusmenu_menuitem_new();
    notice = item("", FALSE, FALSE);
    notice_sep = separator();
    mic_item = item(_("Microphone filter"), TRUE, TRUE);
    g_signal_connect(mic_item, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(toggle_filter), "mic");
    out_item = item(_("System sound filter"), TRUE, TRUE);
    g_signal_connect(out_item, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(toggle_filter), "output");
    mute_sep = separator();
    mute_item = item(_("Mute microphone"), TRUE, TRUE);
    g_signal_connect(mute_item, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(on_mute), NULL);
    separator();
    g_signal_connect(item(_("Open settings..."), FALSE, TRUE), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
                     G_CALLBACK(on_settings), NULL);
    auto_item = item(_("Start with the session"), TRUE, TRUE);
    g_signal_connect(auto_item, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(on_autostart), NULL);
    g_signal_connect(item(_("Close"), FALSE, TRUE), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
                     G_CALLBACK(on_close), NULL);
    dbusmenu_menuitem_property_set_bool(mute_sep, DBUSMENU_MENUITEM_PROP_VISIBLE, has_wpctl);
    dbusmenu_menuitem_property_set_bool(mute_item, DBUSMENU_MENUITEM_PROP_VISIBLE, has_wpctl);
    dbusmenu_server_set_root(server, menu_root);
    update_menu();
    return server;
}

/* -- leitura do estado ---------------------------------------------------- */

static void
finish(void)
{
    if (--pending > 0)
        return;
    sni_t old = sni;
    sni = (sni_t){0};
    describe(have_status ? &status : NULL, muted, &sni);
    if (strcmp(sni.status, old.status))
        emit("NewStatus", g_variant_new("(s)", sni.status));
    if (strcmp(sni.icon, old.icon))
        emit("NewIcon", NULL);
    if (strcmp(sni.icon, old.icon) || strcmp(sni.title, old.title) || strcmp(sni.body, old.body))
        emit("NewToolTip", NULL);
    sni_clear(&old);
    update_menu();
}

static void
got_status(int code, const char *out, gpointer data)
{
    (void)data;
    have_status = code == 0 && parse_status(out, &status);
    finish();
}

static void
got_mute(int code, const char *out, gpointer data)
{
    (void)data;
    muted = code == 0 ? parse_muted(out) : MUTED_UNKNOWN;
    finish();
}

static void
refresh(void)
{
    if (pending)
        return;
    pending = has_wpctl ? 2 : 1;
    const char *st_argv[] = {CLI, "status", NULL};
    run_async(st_argv, got_status, NULL);
    if (has_wpctl) {
        const char *vol_argv[] = {"wpctl", "get-volume", "@DEFAULT_AUDIO_SOURCE@", NULL};
        run_async(vol_argv, got_mute, NULL);
    }
}

static gboolean
refresh_once(gpointer data)
{
    (void)data;
    refresh_id = 0;
    refresh();
    return G_SOURCE_REMOVE;
}

static void
refresh_soon(guint delay)
{
    if (refresh_id)
        g_source_remove(refresh_id);
    refresh_id = g_timeout_add(delay, refresh_once, NULL);
}

static void
config_changed(GFileMonitor *m, GFile *file, GFile *other, GFileMonitorEvent ev, gpointer data)
{
    (void)m, (void)other, (void)ev, (void)data;
    g_autofree char *name = g_file_get_basename(file);
    if (name && !strcmp(name, "settings.json")) {
        fast = 3;
        refresh_soon(300);
    }
}

static gboolean
refresh_later(gpointer data)
{
    (void)data;
    refresh();
    return G_SOURCE_REMOVE;
}

static gboolean
on_timer(gpointer data)
{
    (void)data;
    /* Só sem o "pactl subscribe" (ou com ele morto), logo depois de uma
     * mudança nas configurações (o filtro leva uns segundos para subir) ou
     * enquanto a última leitura falhou (até voltar). */
    if (!subscribe || fast > 0 || !have_status)
        refresh();
    if (fast > 0) {
        fast--;
        g_timeout_add(1500, refresh_later, NULL);
    }
    return G_SOURCE_CONTINUE;
}

/* -- pactl subscribe ------------------------------------------------------ */

static void start_subscribe(void);

static gboolean
resubscribe(gpointer data)
{
    (void)data;
    if (!subscribe && !stopped) {
        start_subscribe();
        refresh();
    }
    return G_SOURCE_REMOVE;
}

static void
on_line(GObject *src, GAsyncResult *res, gpointer data)
{
    (void)data;
    GDataInputStream *s = G_DATA_INPUT_STREAM(src);
    g_autoptr(GError) err = NULL;
    g_autofree char *line = g_data_input_stream_read_line_finish_utf8(s, res, NULL, &err);
    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_object_unref(s);
        return;
    }
    if (!line) {
        /* pactl saiu (o PipeWire reiniciou ou parou): o timer cobre, e uma
         * nova tentativa em 10 s. */
        g_object_unref(s);
        g_clear_object(&subscribe);
        g_clear_object(&subscribe_cancel);
        g_timeout_add_seconds(10, resubscribe, NULL);
        return;
    }
    if (event_matters(line))
        refresh_soon(200);
    g_data_input_stream_read_line_async(s, G_PRIORITY_LOW, subscribe_cancel, on_line, NULL);
}

/* Uma linha por mudança no PipeWire (pelo pipewire-pulse); interessam as das
 * fontes, do servidor (fonte padrão) e as saídas que aparecem ou somem. */
static void
start_subscribe(void)
{
    g_autofree char *pactl = g_find_program_in_path("pactl");
    if (!pactl)
        return;
    g_autoptr(GSubprocessLauncher) l =
        g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    /* event_matters lê as linhas em inglês: o pactl traduz. */
    g_subprocess_launcher_setenv(l, "LC_ALL", "C", TRUE);
    subscribe = g_subprocess_launcher_spawn(l, NULL, "pactl", "subscribe", NULL);
    if (!subscribe)
        return;
    subscribe_cancel = g_cancellable_new();
    GDataInputStream *s = g_data_input_stream_new(g_subprocess_get_stdout_pipe(subscribe));
    g_data_input_stream_read_line_async(s, G_PRIORITY_LOW, subscribe_cancel, on_line, NULL);
}

static void
stop_subscribe(void)
{
    stopped = TRUE;
    if (subscribe) {
        g_cancellable_cancel(subscribe_cancel);
        g_subprocess_force_exit(subscribe);
        g_clear_object(&subscribe);
        g_clear_object(&subscribe_cancel);
    }
}

static gboolean
on_signal(gpointer data)
{
    (void)data;
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

/* == --state =============================================================== */

typedef struct {
    GMainLoop *loop;
    int code;
    char *out;
} sync_t;

static void
sync_done(int code, const char *out, gpointer data)
{
    sync_t *s = data;
    s->code = code;
    s->out = g_strdup(out);
    g_main_loop_quit(s->loop);
}

static gboolean
sync_timeout(gpointer data)
{
    g_main_loop_quit(((sync_t *)data)->loop);
    return G_SOURCE_REMOVE;
}

/* Saída de um comando, com 15 s no máximo. */
static int
run_sync(const char *const *argv, char **out)
{
    sync_t s = {g_main_loop_new(NULL, FALSE), 127, NULL};
    GSubprocess *p = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                       NULL);
    if (p) {
        run_t *r = g_new0(run_t, 1);
        r->cb = sync_done;
        r->data = &s;
        g_object_ref(p);
        g_subprocess_communicate_async(p, NULL, NULL, run_done, r);
        guint t = g_timeout_add_seconds(15, sync_timeout, &s);
        g_main_loop_run(s.loop);
        if (!s.out) { /* tempo esgotado: o processo morre, a resposta é ignorada */
            r->cb = NULL;
            g_subprocess_force_exit(p);
        } else
            g_source_remove(t);
        g_object_unref(p);
    }
    g_main_loop_unref(s.loop);
    *out = s.out ? s.out : g_strdup("");
    return s.out ? s.code : 127;
}

static int
print_state(void)
{
    g_autofree char *text = NULL;
    const char *st_argv[] = {CLI, "status", NULL};
    mic_status_t st;
    int code = run_sync(st_argv, &text);
    gboolean ok = code == 0 && parse_status(text, &st);
    muted_t m = MUTED_UNKNOWN;
    g_autofree char *wpctl = g_find_program_in_path("wpctl");
    if (wpctl) {
        g_autofree char *vol = NULL;
        const char *vol_argv[] = {"wpctl", "get-volume", "@DEFAULT_AUDIO_SOURCE@", NULL};
        code = run_sync(vol_argv, &vol);
        m = code == 0 ? parse_muted(vol) : MUTED_UNKNOWN;
    }
    sni_t s = {0};
    describe(ok ? &st : NULL, m, &s);
    printf("status=%s icon=%s autostart=%s\n%s\n", s.status, s.icon, autostart_enabled() ? "True" : "False",
           s.body);
    sni_clear(&s);
    return 0;
}

/* == main ================================================================== */

int
main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    const char *localedir = g_getenv("BIGWM_LOCALEDIR");
    bindtextdomain("big-wm", localedir && *localedir ? localedir : LOCALEDIR);
    bind_textdomain_codeset("big-wm", "UTF-8");
    textdomain("big-wm");
    autostart_path = g_build_filename(g_get_user_config_dir(), "autostart", "big-wm-mic-tray.desktop", NULL);

    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--state"))
            return print_state();

    g_autofree char *cli = g_find_program_in_path(CLI);
    if (!cli) {
        fprintf(stderr, "big-wm-mic-tray: %s não está instalado\n", CLI);
        return 0;
    }
    /* Só arquivos locais (o settings.json): sem isso o GFileMonitor carrega
     * o módulo do gvfs e acorda o gvfsd. A variável não passa para os
     * programas abertos daqui. */
    g_setenv("GIO_USE_VFS", "local", TRUE);
    g_vfs_get_default();
    g_unsetenv("GIO_USE_VFS");
    g_autoptr(GError) err = NULL;
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!bus) {
        fprintf(stderr, "big-wm-mic-tray: %s\n", err->message);
        return 1;
    }
    /* Uma instância por sessão: o nome bem conhecido é de quem chegou
     * primeiro (DO_NOT_QUEUE; PRIMARY_OWNER ou ALREADY_OWNER seguem). */
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", BUS_NAME, 4), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
    if (!reply) {
        fprintf(stderr, "big-wm-mic-tray: %s\n", err->message);
        return 1;
    }
    guint32 r;
    g_variant_get(reply, "(u)", &r);
    if (r != 1 && r != 4)
        return 0;

    g_autofree char *wpctl = g_find_program_in_path("wpctl");
    has_wpctl = wpctl != NULL;
    sni = (sni_t){"Active", ICON_LIVE, g_strdup(_("Microphone")), g_strdup("")};
    loop = g_main_loop_new(NULL, FALSE);

    static const GDBusInterfaceVTable vtable = {call, get_prop, NULL, {0}};
    g_autoptr(GDBusNodeInfo) node = g_dbus_node_info_new_for_xml(item_xml, NULL);
    g_dbus_connection_register_object(bus, ITEM_PATH, node->interfaces[0], &vtable, NULL, NULL, NULL);
    DbusmenuServer *server = build_menu();
    g_bus_watch_name_on_connection(bus, "org.kde.StatusNotifierWatcher", G_BUS_NAME_WATCHER_FLAGS_NONE,
                                   watcher_appeared, NULL, NULL, NULL);

    g_autofree char *app_config = g_build_filename(g_get_user_config_dir(), "biglinux-microphone", NULL);
    g_autoptr(GFile) dir = g_file_new_for_path(app_config);
    g_autoptr(GFileMonitor) monitor = g_file_monitor_directory(dir, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
    if (monitor)
        g_signal_connect(monitor, "changed", G_CALLBACK(config_changed), NULL);
    start_subscribe();
    g_timeout_add_seconds(7, on_timer, NULL);
    refresh();

    g_unix_signal_add(SIGTERM, on_signal, NULL);
    g_unix_signal_add(SIGINT, on_signal, NULL);
    g_unix_signal_add(SIGHUP, on_signal, NULL);
    g_main_loop_run(loop);

    stop_subscribe();
    /* As respostas e os sinais na fila saem antes do fim do processo. */
    g_dbus_connection_flush_sync(bus, NULL, NULL);
    g_object_unref(server);
    g_object_unref(menu_root);
    sni_clear(&sni);
    g_main_loop_unref(loop);
    g_free(autostart_path);
    return 0;
}
