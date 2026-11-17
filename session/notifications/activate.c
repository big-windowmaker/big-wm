/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A click on a notification takes the user to it, as GNOME, Plasma and XFCE
 * do:
 *
 *   1. GNotification (org.gtk.Notifications): the application's default
 *      action, or its activation, over D-Bus with the click's token.
 *   2. A "default" action from the sender: ActionInvoked (with the token).
 *   3. In any case, the application's window comes to the front: the server
 *      raises it (wlr-foreign-toplevel on Wayland, _NET_ACTIVE_WINDOW on
 *      X11), since many applications do not use the activation token and the
 *      compositor no longer lets them raise themselves (wlmaker 0159).
 *   4. No window: the application is started (D-Bus activation or its
 *      desktop entry), if the notification says which one it is.
 */
#include "bwn.h"
#include <gio/gdesktopappinfo.h>
#include <string.h>
#ifdef GDK_WINDOWING_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif

static GDesktopAppInfo *
desktop_info(BwnNotif *n)
{
    const char *id = n->gapp_id ? n->gapp_id : n->desktop_entry;
    if (!id || !*id)
        return NULL;
    g_autofree char *file = g_str_has_suffix(id, ".desktop") ? g_strdup(id) : g_strconcat(id, ".desktop", NULL);
    return g_desktop_app_info_new(file);
}

static void
add_key(GPtrArray *keys, const char *k)
{
    if (!k || !*k)
        return;
    char *lower = g_ascii_strdown(k, -1);
    if (g_str_has_suffix(lower, ".desktop"))
        lower[strlen(lower) - 8] = '\0';
    for (guint i = 0; i < keys->len; i++)
        if (g_str_equal(keys->pdata[i], lower)) {
            g_free(lower);
            return;
        }
    g_ptr_array_add(keys, lower);
}

/* Names the application's windows may carry as app_id or WM_CLASS. */
char **
bwn_app_keys(BwnNotif *n)
{
    GPtrArray *keys = g_ptr_array_new();
    add_key(keys, n->gapp_id);
    add_key(keys, n->desktop_entry);
    g_autoptr(GDesktopAppInfo) info = desktop_info(n);
    if (info) {
        add_key(keys, g_desktop_app_info_get_startup_wm_class(info));
        const char *exe = g_app_info_get_executable(G_APP_INFO(info));
        if (exe) {
            g_autofree char *base = g_path_get_basename(exe);
            add_key(keys, base);
        }
    }
    if (n->app_name && *n->app_name) {
        add_key(keys, n->app_name);
        g_autofree char *dashed = g_strdup(n->app_name);
        g_strdelimit(dashed, " ", '-');
        add_key(keys, dashed);
    }
    if (n->sender_pid) { /* the sender's own executable */
        g_autofree char *link = g_strdup_printf("/proc/%u/exe", n->sender_pid);
        g_autofree char *exe = g_file_read_link(link, NULL);
        if (exe) {
            g_autofree char *base = g_path_get_basename(exe);
            /* Interpreters say nothing about the application. */
            if (!g_str_has_prefix(base, "python") && !g_str_equal(base, "gjs") &&
                !g_str_equal(base, "node") && !g_str_has_prefix(base, "perl") &&
                !g_str_equal(base, "bash") && !g_str_equal(base, "sh") &&
                !g_str_equal(base, "notify-send"))
                add_key(keys, base);
        }
    }
    g_ptr_array_add(keys, NULL);
    return (char **)g_ptr_array_free(keys, FALSE);
}

/* "org.telegram.desktop" matches "telegram-desktop"? No; but it matches
 * "org.telegram.desktop", and "Thunar" matches "thunar", and the reverse-DNS
 * "org.gnome.Nautilus" matches its last part "nautilus". */
gboolean
bwn_app_id_matches(const char *app_id, char **keys)
{
    if (!app_id || !*app_id || !keys)
        return FALSE;
    g_autofree char *id = g_ascii_strdown(app_id, -1);
    const char *last = strrchr(id, '.');
    for (char **k = keys; *k; k++) {
        if (g_str_equal(id, *k))
            return TRUE;
        if (last && g_str_equal(last + 1, *k) && strlen(*k) > 3)
            return TRUE;
        const char *klast = strrchr(*k, '.');
        if (klast && g_str_equal(klast + 1, id) && strlen(id) > 3)
            return TRUE;
    }
    return FALSE;
}

/* == X11: _NET_ACTIVE_WINDOW ================================================ */

#ifdef GDK_WINDOWING_X11
static gboolean
x11_raise(BwnNotif *n, char **keys, guint32 timestamp)
{
    GdkDisplay *gd = gdk_display_get_default();
    if (!GDK_IS_X11_DISPLAY(gd))
        return FALSE;
    Display *dpy = GDK_DISPLAY_XDISPLAY(gd);
    Window root = DefaultRootWindow(dpy);
    Atom client_list = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    Atom pid_atom = XInternAtom(dpy, "_NET_WM_PID", False);
    Atom gtk_app = XInternAtom(dpy, "_GTK_APPLICATION_ID", False);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    Atom type;
    int format;
    unsigned long count, after;
    unsigned char *data = NULL;
    gdk_x11_display_error_trap_push(gd);
    if (XGetWindowProperty(dpy, root, client_list, 0, 4096, False, XA_WINDOW, &type, &format, &count,
                           &after, &data) != Success || !data) {
        client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
        XGetWindowProperty(dpy, root, client_list, 0, 4096, False, XA_WINDOW, &type, &format, &count,
                           &after, &data);
    }
    Window *wins = (Window *)data, found = None;
    /* From the top of the stack: the window the user saw last. */
    for (long i = (long)count - 1; data && i >= 0 && found == None; i--) {
        Window w = wins[i];
        unsigned char *p = NULL;
        unsigned long c;
        if (n->sender_pid &&
            XGetWindowProperty(dpy, w, pid_atom, 0, 1, False, XA_CARDINAL, &type, &format, &c, &after, &p) ==
                Success && p) {
            if (c == 1 && *(unsigned long *)p == n->sender_pid)
                found = w;
            XFree(p);
            p = NULL;
            if (found)
                break;
        }
        if (XGetWindowProperty(dpy, w, gtk_app, 0, 256, False, utf8, &type, &format, &c, &after, &p) ==
                Success && p) {
            if (bwn_app_id_matches((const char *)p, keys))
                found = w;
            XFree(p);
            if (found)
                break;
        }
        XClassHint hint = { 0 };
        if (XGetClassHint(dpy, w, &hint)) {
            if (bwn_app_id_matches(hint.res_class, keys) || bwn_app_id_matches(hint.res_name, keys))
                found = w;
            XFree(hint.res_name);
            XFree(hint.res_class);
        }
    }
    if (data)
        XFree(data);
    if (found) {
        XEvent ev = { 0 };
        ev.xclient.type = ClientMessage;
        ev.xclient.window = found;
        ev.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 2; /* a pager: on the user's request */
        ev.xclient.data.l[1] = timestamp ? timestamp : CurrentTime;
        XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XFlush(dpy);
    }
    gdk_x11_display_error_trap_pop_ignored(gd);
    return found != None;
}
#endif

gboolean
bwn_raise_app(BwnNotif *n, const char *token, guint32 timestamp)
{
    (void)token;
    g_auto(GStrv) keys = bwn_app_keys(n);
    if (!keys[0] && !n->sender_pid)
        return FALSE;
    if (bwn_wayland())
        return bwn_wl_raise(keys);
#ifdef GDK_WINDOWING_X11
    return x11_raise(n, keys, timestamp);
#else
    (void)timestamp;
    return FALSE;
#endif
}

/* == Starting the application ============================================== */

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

static gboolean
launch_app(BwnNotif *n, const char *token)
{
    g_autoptr(GDesktopAppInfo) info = desktop_info(n);
    if (!info || g_desktop_app_info_get_nodisplay(info) || !g_app_info_should_show(G_APP_INFO(info)))
        return FALSE;
    if (g_desktop_app_info_get_boolean(info, "DBusActivatable")) {
        const char *id = g_app_info_get_id(G_APP_INFO(info));
        g_autofree char *name = g_strndup(id, strlen(id) - strlen(".desktop"));
        g_autofree char *path = g_strconcat("/", name, NULL);
        g_strdelimit(path, ".", '/');
        g_strdelimit(path, "-", '_');
        GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        if (bus) {
            g_dbus_connection_call(bus, name, path, "org.freedesktop.Application", "Activate",
                                   g_variant_new("(@a{sv})", platform_data(token)), NULL,
                                   G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL, NULL);
            g_object_unref(bus);
            return TRUE;
        }
    }
    GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    gboolean ok = g_app_info_launch(G_APP_INFO(info), NULL, G_APP_LAUNCH_CONTEXT(ctx), NULL);
    g_object_unref(ctx);
    return ok;
}

static gboolean
has_action(BwnNotif *n, const char *key)
{
    for (char **a = n->actions; a && a[0] && a[1]; a += 2)
        if (g_str_equal(a[0], key))
            return TRUE;
    return FALSE;
}

void
bwn_activate(BwnNotif *n, GtkWidget *from)
{
    if (!n)
        return;
    bwn_notif_ref(n);
    guint32 timestamp = gtk_get_current_event_time();
    g_autofree char *token = from ? bwn_wl_activation_token(from) : NULL;
    gboolean done = FALSE;

    if (n->gapp_id) {
        /* GNotification: its default action, else the application itself. */
        done = bwn_gtk_invoke(n, "default", token);
    } else if (n->active && has_action(n, "default")) {
        bwn_server_emit_action(n, "default", token);
        done = TRUE;
    }
    /* The window to the front, also when the application was told: most do
     * not raise themselves with the token. */
    gboolean raised = bwn_raise_app(n, token, timestamp);
    if (!done && !raised)
        launch_app(n, token);

    if (n->active && !n->resident)
        bwn_server_close(n, BWN_CLOSED_DISMISSED);
    bwn_notif_unref(n);
}
