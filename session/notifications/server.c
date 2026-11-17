/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * org.freedesktop.Notifications (specification 1.2) and the control
 * interface org.bigwindowmaker.Notifications (bell, command line).
 */
#include "bwn.h"
#include <string.h>
#include <unistd.h>

#ifndef BWN_VERSION
#define BWN_VERSION "1.0"
#endif

static const char fdo_xml[] =
    "<node>"
    " <interface name='org.freedesktop.Notifications'>"
    "  <method name='GetCapabilities'><arg type='as' direction='out'/></method>"
    "  <method name='Notify'>"
    "   <arg type='s' direction='in'/><arg type='u' direction='in'/>"
    "   <arg type='s' direction='in'/><arg type='s' direction='in'/>"
    "   <arg type='s' direction='in'/><arg type='as' direction='in'/>"
    "   <arg type='a{sv}' direction='in'/><arg type='i' direction='in'/>"
    "   <arg type='u' direction='out'/>"
    "  </method>"
    "  <method name='CloseNotification'><arg type='u' direction='in'/></method>"
    "  <method name='GetServerInformation'>"
    "   <arg type='s' direction='out'/><arg type='s' direction='out'/>"
    "   <arg type='s' direction='out'/><arg type='s' direction='out'/>"
    "  </method>"
    "  <signal name='NotificationClosed'><arg type='u'/><arg type='u'/></signal>"
    "  <signal name='ActionInvoked'><arg type='u'/><arg type='s'/></signal>"
    "  <signal name='ActivationToken'><arg type='u'/><arg type='s'/></signal>"
    " </interface>"
    "</node>";

static const char ctl_xml[] =
    "<node>"
    " <interface name='org.bigwindowmaker.Notifications'>"
    "  <method name='ToggleCenter'/>"
    "  <method name='SetDoNotDisturb'><arg type='b' direction='in'/></method>"
    "  <method name='Clear'/>"
    "  <method name='GetStatus'>"
    "   <arg type='b' direction='out' name='dnd'/>"
    "   <arg type='u' direction='out' name='unread'/>"
    "   <arg type='u' direction='out' name='total'/>"
    "   <arg type='u' direction='out' name='toasts'/>"
    "  </method>"
    "  <method name='Reload'/>"
    " </interface>"
    "</node>";

static GDBusConnection *bus;
static GDBusConnection *system_bus;
static gboolean locked;
static int dnd_missed;
static GHashTable *rate; /* sender -> struct rate* */

struct rate {
    gint64 window_start;
    int count;
};

gboolean
bwn_session_locked(void)
{
    return locked;
}

/* == Signals =============================================================== */

static void
emit(const char *signal, GVariant *params)
{
    g_dbus_connection_emit_signal(bus, NULL, "/org/freedesktop/Notifications",
                                  "org.freedesktop.Notifications", signal, params, NULL);
}

void
bwn_server_close(BwnNotif *n, BwnCloseReason reason)
{
    if (!n || !n->active)
        return;
    n->active = FALSE;
    bwn_toast_close(n->id);
    emit("NotificationClosed", g_variant_new("(uu)", n->id, (guint32)reason));
    bwn_store_prune();
    bwn_store_save_later();
    bwn_store_changed();
}

void
bwn_server_invoke(BwnNotif *n, const char *key, GtkWidget *from)
{
    if (!n || !n->active || !bwn_action_key_valid(key))
        return;
    /* Spec 1.2: the token first, so the application can raise its window.
     * On Wayland, the compositor only honors a token that stems from this
     * click (see wlmaker 0159). */
    g_autofree char *token = from ? bwn_wl_activation_token(from) : NULL;
    if (token)
        emit("ActivationToken", g_variant_new("(us)", n->id, token));
    emit("ActionInvoked", g_variant_new("(us)", n->id, key));
    if (!n->resident)
        bwn_server_close(n, BWN_CLOSED_DISMISSED);
}

/* == Notify ================================================================ */

static GdkPixbuf *
pixbuf_from_image_data(GVariant *v)
{
    if (!g_variant_is_of_type(v, G_VARIANT_TYPE("(iiibiiay)")))
        return NULL;
    gint32 w, h, stride, bits, channels;
    gboolean alpha;
    g_autoptr(GVariant) data_v = NULL;
    g_variant_get(v, "(iiibii@ay)", &w, &h, &stride, &alpha, &bits, &channels, &data_v);
    if (w <= 0 || h <= 0 || w > BWN_MAX_IMAGE_SIDE || h > BWN_MAX_IMAGE_SIDE || bits != 8 ||
        (channels != 3 && channels != 4) || (alpha && channels != 4) || stride < w * channels)
        return NULL;
    gsize len = g_variant_get_size(data_v);
    if ((guint64)stride * (h - 1) + (guint64)w * channels > len)
        return NULL;
    GBytes *bytes = g_variant_get_data_as_bytes(data_v);
    GdkPixbuf *pb = gdk_pixbuf_new_from_bytes(bytes, GDK_COLORSPACE_RGB, alpha, 8, w, h, stride);
    g_bytes_unref(bytes);
    /* Keep at most 128 px: the bytes of a huge image are not held. */
    if (pb && (w > 128 || h > 128)) {
        double s = 128.0 / MAX(w, h);
        GdkPixbuf *small = gdk_pixbuf_scale_simple(pb, MAX(1, (int)(w * s)), MAX(1, (int)(h * s)),
                                                   GDK_INTERP_BILINEAR);
        g_object_unref(pb);
        pb = small;
    } else if (pb) {
        GdkPixbuf *copy = gdk_pixbuf_copy(pb); /* detach from the message */
        g_object_unref(pb);
        pb = copy;
    }
    return pb;
}

static GdkPixbuf *
pixbuf_from_path(const char *path)
{
    if (!path || !*path)
        return NULL;
    g_autofree char *file = g_str_has_prefix(path, "file://") ? g_filename_from_uri(path, NULL, NULL)
                                                              : g_strdup(path);
    if (!file || !g_path_is_absolute(file))
        return NULL;
    return gdk_pixbuf_new_from_file_at_size(file, 128, 128, NULL);
}

static gboolean
desktop_entry_valid(const char *s)
{
    if (!s || !*s || strlen(s) > 128)
        return FALSE;
    for (const char *p = s; *p; p++)
        if (!g_ascii_isalnum(*p) && *p != '.' && *p != '-' && *p != '_')
            return FALSE;
    return TRUE;
}

static gboolean
hint_bool(GVariant *v)
{
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN))
        return g_variant_get_boolean(v);
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT32))
        return g_variant_get_int32(v) != 0;
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_BYTE))
        return g_variant_get_byte(v) != 0;
    return FALSE;
}

/* Too many from one sender: the rest still go to the history, without
 * toasts. 20 in 10 s. */
static gboolean
rate_ok(const char *sender)
{
    gint64 now = g_get_monotonic_time();
    struct rate *r = g_hash_table_lookup(rate, sender);
    if (!r) {
        r = g_new0(struct rate, 1);
        g_hash_table_insert(rate, g_strdup(sender), r);
    }
    if (now - r->window_start > 10 * G_USEC_PER_SEC) {
        r->window_start = now;
        r->count = 0;
    }
    return ++r->count <= 20;
}

static gboolean
muted(const char *app)
{
    char **m = bwn_settings()->muted_apps;
    for (; app && m && *m; m++)
        if (g_ascii_strcasecmp(*m, app) == 0)
            return TRUE;
    return FALSE;
}

static int
count_active(void)
{
    int c = 0;
    for (GList *l = bwn_store_list(); l; l = l->next)
        if (((BwnNotif *)l->data)->active)
            c++;
    return c;
}

/* Closes the oldest open ones beyond the limit (a sender that never closes
 * its notifications cannot grow the server without bound). */
static void
limit_active(void)
{
    while (count_active() > BWN_MAX_ACTIVE) {
        BwnNotif *oldest = NULL;
        for (GList *l = bwn_store_list(); l; l = l->next)
            if (((BwnNotif *)l->data)->active)
                oldest = l->data;
        if (!oldest)
            break;
        bwn_server_close(oldest, BWN_CLOSED_UNDEFINED);
    }
}

guint32 bwn_store_max_id(void);

static guint32
handle_notify(GVariant *params, const char *sender)
{
    const char *app_name, *app_icon, *summary, *body;
    guint32 replaces_id;
    g_autoptr(GVariantIter) actions_it = NULL;
    g_autoptr(GVariant) hints = NULL;
    gint32 timeout;
    g_variant_get(params, "(&su&s&s&sas@a{sv}i)", &app_name, &replaces_id, &app_icon, &summary,
                  &body, &actions_it, &hints, &timeout);

    BwnNotif *n = bwn_notif_new();
    n->sender = g_strdup(sender);
    n->app_name = bwn_text_clean(app_name, 64);
    n->app_icon = *app_icon && strlen(app_icon) < 1024 ? g_strdup(app_icon) : NULL;
    n->summary = bwn_text_clean(summary, BWN_MAX_SUMMARY);
    n->body = bwn_markup_sanitize(body, BWN_MAX_BODY);
    n->body_plain = bwn_markup_to_plain(n->body);
    n->timeout = timeout < -1 ? -1 : timeout;
    n->active = TRUE;

    GPtrArray *acts = g_ptr_array_new();
    const char *key, *label;
    while (g_variant_iter_next(actions_it, "&s", &key) && g_variant_iter_next(actions_it, "&s", &label)) {
        if (acts->len / 2 >= BWN_MAX_ACTIONS || !bwn_action_key_valid(key))
            continue;
        g_ptr_array_add(acts, g_strdup(key));
        g_ptr_array_add(acts, bwn_text_clean(*label ? label : key, 40));
    }
    g_ptr_array_add(acts, NULL);
    n->actions = (char **)g_ptr_array_free(acts, FALSE);

    GVariant *v;
    if ((v = g_variant_lookup_value(hints, "urgency", NULL))) {
        guint u = g_variant_is_of_type(v, G_VARIANT_TYPE_BYTE) ? g_variant_get_byte(v)
                  : g_variant_is_of_type(v, G_VARIANT_TYPE_INT32) ? (guint)g_variant_get_int32(v)
                  : g_variant_is_of_type(v, G_VARIANT_TYPE_UINT32) ? g_variant_get_uint32(v) : 1;
        n->urgency = MIN(u, BWN_URGENCY_CRITICAL);
        g_variant_unref(v);
    }
    if ((v = g_variant_lookup_value(hints, "desktop-entry", G_VARIANT_TYPE_STRING))) {
        const char *de = g_variant_get_string(v, NULL);
        if (desktop_entry_valid(de))
            n->desktop_entry = g_strdup(de);
        g_variant_unref(v);
    }
    if ((v = g_variant_lookup_value(hints, "transient", NULL))) {
        n->transient = hint_bool(v);
        g_variant_unref(v);
    }
    if ((v = g_variant_lookup_value(hints, "resident", NULL))) {
        n->resident = hint_bool(v);
        g_variant_unref(v);
    }
    const char *image_keys[] = { "image-data", "image_data", "icon_data" };
    for (gsize i = 0; i < G_N_ELEMENTS(image_keys) && !n->image; i++)
        if ((v = g_variant_lookup_value(hints, image_keys[i], NULL))) {
            n->image = pixbuf_from_image_data(v);
            g_variant_unref(v);
        }
    const char *path_keys[] = { "image-path", "image_path" };
    for (gsize i = 0; i < G_N_ELEMENTS(path_keys) && !n->image; i++)
        if ((v = g_variant_lookup_value(hints, path_keys[i], G_VARIANT_TYPE_STRING))) {
            const char *p = g_variant_get_string(v, NULL);
            n->image = pixbuf_from_path(p);
            if (!n->image && !n->app_icon && *p && !strchr(p, '/'))
                n->app_icon = g_strdup(p); /* an icon name given as image-path */
            g_variant_unref(v);
        }

    /* replaces_id: only the sender's own (or the same application's). */
    BwnNotif *old = replaces_id ? bwn_store_lookup(replaces_id) : NULL;
    gboolean replace = old && (g_strcmp0(old->sender, sender) == 0 ||
                               g_strcmp0(old->app_name, n->app_name) == 0);
    n->id = replace ? replaces_id : bwn_store_max_id() + 1;
    if (n->id == 0)
        n->id = 1;
    if (replace)
        n->read = old->read && old->active; /* an update to a seen one stays seen */

    BwnSettings *s = bwn_settings();
    gboolean critical = n->urgency == BWN_URGENCY_CRITICAL;
    gboolean show = s->toasts && rate_ok(sender) && !muted(n->app_name) &&
                    !(s->dnd && !(critical && s->dnd_critical)) &&
                    !(locked && s->hide_on_lock && !critical);
    if (s->dnd && !show && !replace)
        dnd_missed++;

    bwn_store_add(n);
    if (show) {
        if (replace)
            bwn_toast_update(n);
        else
            bwn_toast_show(n);
        if (s->sound && !n->transient)
            bwn_play_sound(n);
    } else if (replace) {
        bwn_toast_update(n);
    } else if (n->timeout != 0 && !critical) {
        /* No toast: it expires at once for the sender, and stays in the
         * history (unless transient). */
        bwn_server_close(n, BWN_CLOSED_EXPIRED);
    }
    limit_active();
    guint32 id = n->id;
    bwn_notif_unref(n);
    return id;
}

static void
fdo_call(GDBusConnection *c, const char *sender, const char *path, const char *iface,
         const char *method, GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    (void)c; (void)path; (void)iface; (void)data;
    if (g_str_equal(method, "GetCapabilities")) {
        const char *caps[] = { "actions", "body", "body-markup", "icon-static", NULL };
        g_dbus_method_invocation_return_value(inv, g_variant_new("(^as)", caps));
    } else if (g_str_equal(method, "Notify")) {
        guint32 id = handle_notify(params, sender);
        g_dbus_method_invocation_return_value(inv, g_variant_new("(u)", id));
    } else if (g_str_equal(method, "CloseNotification")) {
        guint32 id;
        g_variant_get(params, "(u)", &id);
        bwn_server_close(bwn_store_lookup(id), BWN_CLOSED_BY_CALL);
        g_dbus_method_invocation_return_value(inv, NULL);
    } else if (g_str_equal(method, "GetServerInformation")) {
        g_dbus_method_invocation_return_value(
            inv, g_variant_new("(ssss)", "Big WM Notifications", "Big WindowMaker", BWN_VERSION, "1.2"));
    }
}

/* == Do Not Disturb ========================================================= */

void
bwn_set_dnd(gboolean on)
{
    BwnSettings *s = bwn_settings();
    if (s->dnd == on)
        return;
    s->dnd = on;
    bwn_settings_save();
    if (on) {
        dnd_missed = 0;
        bwn_toast_hide_all();
    } else if (dnd_missed > 0) {
        /* A quiet summary of what arrived meanwhile. */
        BwnNotif *n = bwn_notif_new();
        n->id = bwn_store_max_id() + 1;
        n->app_name = g_strdup(_("Notifications"));
        n->app_icon = g_strdup("preferences-system-notifications");
        n->summary = g_strdup(_("Do not disturb is off"));
        n->body = g_markup_printf_escaped(ngettext("%d notification arrived meanwhile.",
                                                   "%d notifications arrived meanwhile.", dnd_missed),
                                          dnd_missed);
        n->body_plain = bwn_markup_to_plain(n->body);
        n->urgency = BWN_URGENCY_LOW;
        n->transient = TRUE;
        n->active = TRUE;
        n->read = TRUE;
        dnd_missed = 0;
        bwn_store_add(n);
        bwn_toast_show(n);
        bwn_notif_unref(n);
    }
    bwn_store_changed();
}

/* == Control interface ===================================================== */

static void
ctl_call(GDBusConnection *c, const char *sender, const char *path, const char *iface,
         const char *method, GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    (void)c; (void)sender; (void)path; (void)iface; (void)data;
    if (g_str_equal(method, "ToggleCenter")) {
        bwn_center_toggle();
    } else if (g_str_equal(method, "SetDoNotDisturb")) {
        gboolean on;
        g_variant_get(params, "(b)", &on);
        bwn_set_dnd(on);
    } else if (g_str_equal(method, "Clear")) {
        GList *all = bwn_store_take_all();
        GList *keep = NULL;
        for (GList *l = all; l; l = l->next) {
            BwnNotif *n = l->data;
            if (n->active)
                bwn_server_close(n, BWN_CLOSED_DISMISSED);
        }
        g_list_free_full(all, (GDestroyNotify)bwn_notif_unref);
        (void)keep;
    } else if (g_str_equal(method, "GetStatus")) {
        g_dbus_method_invocation_return_value(
            inv, g_variant_new("(buuu)", bwn_settings()->dnd, (guint32)bwn_store_unread(),
                               (guint32)g_list_length(bwn_store_list()), (guint32)bwn_toast_count()));
        return;
    } else if (g_str_equal(method, "Reload")) {
        gboolean dnd = bwn_settings()->dnd;
        bwn_settings_load();
        if (dnd != bwn_settings()->dnd)
            bwn_toast_hide_all();
        bwn_store_prune();
        bwn_store_changed();
    }
    g_dbus_method_invocation_return_value(inv, NULL);
}

static const GDBusInterfaceVTable fdo_vtable = { fdo_call, NULL, NULL, { 0 } };
static const GDBusInterfaceVTable ctl_vtable = { ctl_call, NULL, NULL, { 0 } };

/* == Session lock (logind LockedHint) ===================================== */

static char *session_path;

static void
read_locked(void)
{
    g_autoptr(GVariant) v = g_dbus_connection_call_sync(
        system_bus, "org.freedesktop.login1", session_path, "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.freedesktop.login1.Session", "LockedHint"), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL);
    if (!v)
        return;
    g_autoptr(GVariant) inner = NULL;
    g_variant_get(v, "(v)", &inner);
    gboolean was = locked;
    locked = g_variant_get_boolean(inner);
    if (locked && !was && bwn_settings()->hide_on_lock)
        bwn_toast_hide_all();
}

static void
on_session_props(GDBusConnection *c, const char *s, const char *p, const char *i, const char *sig,
                 GVariant *params, gpointer data)
{
    (void)c; (void)s; (void)p; (void)i; (void)sig; (void)params; (void)data;
    read_locked();
}

static void
watch_lock(void)
{
    system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (!system_bus)
        return;
    const char *id = g_getenv("XDG_SESSION_ID");
    g_autoptr(GVariant) v = NULL;
    if (id && *id)
        v = g_dbus_connection_call_sync(system_bus, "org.freedesktop.login1", "/org/freedesktop/login1",
                                        "org.freedesktop.login1.Manager", "GetSession",
                                        g_variant_new("(s)", id), G_VARIANT_TYPE("(o)"),
                                        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL);
    if (!v)
        v = g_dbus_connection_call_sync(system_bus, "org.freedesktop.login1", "/org/freedesktop/login1",
                                        "org.freedesktop.login1.Manager", "GetSessionByPID",
                                        g_variant_new("(u)", (guint32)getpid()), G_VARIANT_TYPE("(o)"),
                                        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL);
    if (!v)
        return; /* no logind session (a nested test): never locked */
    g_variant_get(v, "(o)", &session_path);
    g_dbus_connection_signal_subscribe(system_bus, "org.freedesktop.login1",
                                       "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                       session_path, NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_session_props,
                                       NULL, NULL);
    read_locked();
}

/* == Start ================================================================= */

static void
sender_vanished(GDBusConnection *c, const char *s, const char *p, const char *i, const char *sig,
                GVariant *params, gpointer data)
{
    (void)c; (void)s; (void)p; (void)i; (void)sig; (void)data;
    const char *name, *old_owner, *new_owner;
    g_variant_get(params, "(&s&s&s)", &name, &old_owner, &new_owner);
    if (*new_owner == '\0' && name[0] == ':')
        g_hash_table_remove(rate, name);
}

void
bwn_server_start(GDBusConnection *connection)
{
    bus = connection;
    rate = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    GDBusNodeInfo *fdo = g_dbus_node_info_new_for_xml(fdo_xml, NULL);
    GDBusNodeInfo *ctl = g_dbus_node_info_new_for_xml(ctl_xml, NULL);
    g_dbus_connection_register_object(bus, "/org/freedesktop/Notifications", fdo->interfaces[0],
                                      &fdo_vtable, NULL, NULL, NULL);
    g_dbus_connection_register_object(bus, "/org/bigwindowmaker/Notifications", ctl->interfaces[0],
                                      &ctl_vtable, NULL, NULL, NULL);
    g_dbus_connection_signal_subscribe(bus, "org.freedesktop.DBus", "org.freedesktop.DBus",
                                       "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
                                       G_DBUS_SIGNAL_FLAGS_NONE, sender_vanished, NULL, NULL);
    watch_lock();
}
