/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * org.gtk.Notifications: the notification interface of GLib's
 * GNotification, as GNOME Shell and Plasma implement it. With this name
 * owned, GTK 3/4 and libadwaita applications (BigLinux's among them) say who
 * they are (their application id) and which GAction a click runs; the click
 * then reaches the application over org.freedesktop.Application, with the
 * activation token, even if it was not running.
 *
 *   AddNotification(s app_id, s id, a{sv} notification)
 *   RemoveNotification(s app_id, s id)
 */
#include "bwn.h"
#include <gio/gdesktopappinfo.h>
#include <string.h>

static const char gtk_xml[] =
    "<node>"
    " <interface name='org.gtk.Notifications'>"
    "  <method name='AddNotification'>"
    "   <arg type='s' direction='in'/><arg type='s' direction='in'/>"
    "   <arg type='a{sv}' direction='in'/>"
    "  </method>"
    "  <method name='RemoveNotification'>"
    "   <arg type='s' direction='in'/><arg type='s' direction='in'/>"
    "  </method>"
    " </interface>"
    "</node>";

static GDBusConnection *bus;

static gboolean
app_id_valid(const char *id)
{
    return id && *id && strlen(id) < 256 && g_application_id_is_valid(id);
}

/* Only the application's own actions ("app.name"): the name goes to
 * org.freedesktop.Application.ActivateAction. */
static gboolean
action_valid(const char *a)
{
    return a && g_str_has_prefix(a, "app.") && g_action_name_is_valid(a + 4);
}

static BwnNotif *
find(const char *app_id, const char *id)
{
    for (GList *l = bwn_store_list(); l; l = l->next) {
        BwnNotif *n = l->data;
        if (n->active && g_strcmp0(n->gapp_id, app_id) == 0 && g_strcmp0(n->gtk_id, id) == 0)
            return n;
    }
    return NULL;
}

static void
set_icon(BwnNotif *n, GVariant *serialized)
{
    g_autoptr(GIcon) icon = g_icon_deserialize(serialized);
    if (!icon)
        return;
    if (G_IS_THEMED_ICON(icon)) {
        const char *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0])
            n->app_icon = g_strdup(names[0]);
    } else if (G_IS_FILE_ICON(icon)) {
        GFile *f = g_file_icon_get_file(G_FILE_ICON(icon));
        g_autofree char *path = g_file_get_path(f);
        if (path)
            n->image = gdk_pixbuf_new_from_file_at_size(path, 128, 128, NULL);
    } else if (G_IS_BYTES_ICON(icon)) {
        g_autoptr(GInputStream) in = g_loadable_icon_load(G_LOADABLE_ICON(icon), 128, NULL, NULL, NULL);
        if (in)
            n->image = gdk_pixbuf_new_from_stream_at_scale(in, 128, 128, TRUE, NULL, NULL);
    }
}

static void
add_notification(const char *sender, const char *app_id, const char *id, GVariant *dict)
{
    BwnNotif *old = find(app_id, id);
    BwnNotif *n = bwn_notif_new();
    n->gapp_id = g_strdup(app_id);
    n->gtk_id = g_strdup(id); /* compared raw by RemoveNotification */
    n->desktop_entry = g_strdup(app_id);
    n->sender = g_strdup(sender);
    n->sender_pid = bwn_sender_pid(sender);
    n->active = TRUE;
    n->timeout = -1;

    g_autofree char *desktop = g_strconcat(app_id, ".desktop", NULL);
    g_autoptr(GDesktopAppInfo) info = g_desktop_app_info_new(desktop);
    if (info) {
        n->app_name = bwn_text_clean(g_app_info_get_display_name(G_APP_INFO(info)), 64);
    } else { /* "org.example.TextEditor" -> "TextEditor" */
        const char *last = strrchr(app_id, '.');
        n->app_name = bwn_text_clean(last && last[1] ? last + 1 : app_id, 64);
    }

    const char *s;
    n->summary = bwn_text_clean(g_variant_lookup(dict, "title", "&s", &s) ? s : "", BWN_MAX_SUMMARY);
    if (g_variant_lookup(dict, "markup-body", "&s", &s))
        n->body = bwn_markup_sanitize(s, BWN_MAX_BODY);
    else if (g_variant_lookup(dict, "body", "&s", &s)) {
        g_autofree char *escaped = g_markup_escape_text(s, -1);
        n->body = bwn_markup_sanitize(escaped, BWN_MAX_BODY);
    } else
        n->body = g_strdup("");
    n->body_plain = bwn_markup_to_plain(n->body);

    g_autoptr(GVariant) icon = g_variant_lookup_value(dict, "icon", NULL);
    if (icon)
        set_icon(n, icon);

    if (g_variant_lookup(dict, "priority", "&s", &s)) {
        if (g_str_equal(s, "urgent"))
            n->urgency = BWN_URGENCY_CRITICAL;
        else if (g_str_equal(s, "low"))
            n->urgency = BWN_URGENCY_LOW;
    } else {
        gboolean urgent = FALSE; /* the old key, GLib < 2.42 */
        if (g_variant_lookup(dict, "urgent", "b", &urgent) && urgent)
            n->urgency = BWN_URGENCY_CRITICAL;
    }

    GPtrArray *acts = g_ptr_array_new();
    if (g_variant_lookup(dict, "default-action", "&s", &s) && action_valid(s)) {
        n->gtk_default = g_strdup(s);
        n->gtk_default_target = g_variant_lookup_value(dict, "default-action-target", NULL);
    }
    /* The click on the balloon is "default" (the default action, or the
     * application itself). */
    g_ptr_array_add(acts, g_strdup("default"));
    g_ptr_array_add(acts, g_strdup(_("Open")));
    n->gtk_buttons = g_ptr_array_new_with_free_func(g_free);
    n->gtk_targets = g_ptr_array_new_with_free_func((GDestroyNotify)g_variant_unref);
    g_autoptr(GVariant) buttons = g_variant_lookup_value(dict, "buttons", G_VARIANT_TYPE("aa{sv}"));
    if (buttons) {
        GVariantIter it;
        GVariant *b;
        g_variant_iter_init(&it, buttons);
        while ((b = g_variant_iter_next_value(&it))) {
            const char *label, *action;
            if (n->gtk_buttons->len < BWN_MAX_ACTIONS && g_variant_lookup(b, "label", "&s", &label) &&
                g_variant_lookup(b, "action", "&s", &action) && action_valid(action)) {
                g_ptr_array_add(acts, g_strdup_printf("gtk:%u", n->gtk_buttons->len));
                g_ptr_array_add(acts, bwn_text_clean(label, 40));
                g_ptr_array_add(n->gtk_buttons, g_strdup(action));
                GVariant *t = g_variant_lookup_value(b, "target", NULL);
                g_ptr_array_add(n->gtk_targets, t ? t : g_variant_ref_sink(g_variant_new("()")));
            }
            g_variant_unref(b);
        }
    }
    g_ptr_array_add(acts, NULL);
    n->actions = (char **)g_ptr_array_free(acts, FALSE);

    n->id = old ? old->id : bwn_server_new_id();
    if (old)
        n->read = old->read;

    bwn_server_deliver(n, old != NULL, sender);
    bwn_notif_unref(n);
}

static GVariant *
platform_data(const char *token)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    if (token) {
        g_variant_builder_add(&b, "{sv}", "activation-token", g_variant_new_string(token));
        g_variant_builder_add(&b, "{sv}", "desktop-startup-id", g_variant_new_string(token));
    }
    return g_variant_builder_end(&b);
}

typedef struct {
    BwnNotif *n;
    char *token;
} Pending;

/* The application did not answer (not running and not D-Bus activatable):
 * start it from its desktop entry. */
static void
invoked(GObject *src, GAsyncResult *res, gpointer data)
{
    Pending *p = data;
    g_autoptr(GError) e = NULL;
    g_autoptr(GVariant) r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &e);
    if (!r)
        bwn_launch_app(p->n, p->token);
    bwn_notif_unref(p->n);
    g_free(p->token);
    g_free(p);
}

/* "default": the default action, or Activate; "gtk:N": button N. The
 * application is reached by its well-known name (D-Bus activation starts it
 * when it is not running). */
gboolean
bwn_gtk_invoke(BwnNotif *n, const char *key, const char *token)
{
    if (!bus || !n || !n->gapp_id)
        return FALSE;
    const char *action = NULL;
    GVariant *target = NULL;
    if (g_str_equal(key, "default")) {
        action = n->gtk_default;
        target = n->gtk_default_target;
    } else if (g_str_has_prefix(key, "gtk:") && n->gtk_buttons) {
        guint i = (guint)g_ascii_strtoull(key + 4, NULL, 10);
        if (i >= n->gtk_buttons->len)
            return FALSE;
        action = n->gtk_buttons->pdata[i];
        target = n->gtk_targets->pdata[i];
    } else {
        return FALSE;
    }
    g_autofree char *path = g_strconcat("/", n->gapp_id, NULL);
    g_strdelimit(path, ".", '/');
    g_strdelimit(path, "-", '_');
    if (action) {
        GVariantBuilder params;
        g_variant_builder_init(&params, G_VARIANT_TYPE("av"));
        if (target && !g_variant_is_of_type(target, G_VARIANT_TYPE_UNIT))
            g_variant_builder_add(&params, "v", target);
        Pending *p = g_new0(Pending, 1);
        p->n = bwn_notif_ref(n);
        p->token = g_strdup(token);
        g_dbus_connection_call(bus, n->gapp_id, path, "org.freedesktop.Application", "ActivateAction",
                               g_variant_new("(s@av@a{sv})", action + 4, g_variant_builder_end(&params),
                                             platform_data(token)),
                               NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, invoked, p);
    } else {
        Pending *p = g_new0(Pending, 1);
        p->n = bwn_notif_ref(n);
        p->token = g_strdup(token);
        g_dbus_connection_call(bus, n->gapp_id, path, "org.freedesktop.Application", "Activate",
                               g_variant_new("(@a{sv})", platform_data(token)), NULL, G_DBUS_CALL_FLAGS_NONE,
                               5000, NULL, invoked, p);
    }
    return TRUE;
}

static void
call(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method,
     GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    (void)c; (void)path; (void)iface; (void)data;
    const char *app_id, *id;
    if (g_str_equal(method, "AddNotification")) {
        g_autoptr(GVariant) dict = NULL;
        g_variant_get(params, "(&s&s@a{sv})", &app_id, &id, &dict);
        if (!app_id_valid(app_id) || !*id || strlen(id) > 256) {
            g_dbus_method_invocation_return_error_literal(inv, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                                          "invalid application or notification id");
            return;
        }
        add_notification(sender, app_id, id, dict);
    } else if (g_str_equal(method, "RemoveNotification")) {
        g_variant_get(params, "(&s&s)", &app_id, &id);
        BwnNotif *n = find(app_id, id);
        if (n)
            bwn_server_close(n, BWN_CLOSED_BY_CALL);
    }
    g_dbus_method_invocation_return_value(inv, NULL);
}

static const GDBusInterfaceVTable vtable = { call, NULL, NULL, { 0 } };

void
bwn_gtk_start(GDBusConnection *connection)
{
    bus = connection;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(gtk_xml, NULL);
    g_dbus_connection_register_object(bus, "/org/gtk/Notifications", info->interfaces[0], &vtable, NULL,
                                      NULL, NULL);
    g_bus_own_name_on_connection(bus, "org.gtk.Notifications", G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL,
                                 NULL, NULL);
}
