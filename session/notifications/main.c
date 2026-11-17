/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-notifications: the Big WM Notification Center.
 *
 *   big-wm-notifications            the server (one per session)
 *   big-wm-notifications --toggle   opens/closes the notification center
 *   big-wm-notifications --dnd on|off|toggle
 *   big-wm-notifications --clear    empties the history
 *   big-wm-notifications --status   do-not-disturb, unread, total
 *   big-wm-notifications --reload   rereads ~/.config/big-wm/notifications.conf
 *
 * Exactly one server owns org.freedesktop.Notifications. Started in a Big
 * WindowMaker session while dunst holds the name (D-Bus activation at login,
 * before the session's autostart), it stops that dunst and takes the name.
 */
#include "bwn.h"
#include <glib-unix.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#define CTL_NAME "org.bigwindowmaker.Notifications"
#define CTL_PATH "/org/bigwindowmaker/Notifications"
#define FDO_NAME "org.freedesktop.Notifications"

static GDBusConnection *bus;
static gboolean took_over;

static int
client(const char *method, GVariant *args)
{
    g_autoptr(GError) e = NULL;
    g_autoptr(GDBusConnection) c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
    if (!c) {
        g_printerr("big-wm-notifications: %s\n", e->message);
        return 1;
    }
    g_autoptr(GVariant) r = g_dbus_connection_call_sync(c, CTL_NAME, CTL_PATH, CTL_NAME, method, args, NULL,
                                                        G_DBUS_CALL_FLAGS_NO_AUTO_START, 5000, NULL, &e);
    if (!r) {
        g_printerr("big-wm-notifications: %s\n", e->message);
        return 1;
    }
    if (g_str_equal(method, "GetStatus")) {
        gboolean dnd;
        guint32 unread, total, toasts;
        g_variant_get(r, "(buuu)", &dnd, &unread, &total, &toasts);
        g_print("dnd=%s unread=%u total=%u toasts=%u\n", dnd ? "on" : "off", unread, total, toasts);
    }
    return 0;
}

static int
dnd_client(const char *arg)
{
    gboolean on;
    if (g_str_equal(arg, "on"))
        on = TRUE;
    else if (g_str_equal(arg, "off"))
        on = FALSE;
    else if (g_str_equal(arg, "toggle")) {
        g_autoptr(GDBusConnection) c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_autoptr(GVariant) r = c ? g_dbus_connection_call_sync(c, CTL_NAME, CTL_PATH, CTL_NAME, "GetStatus",
                                                                NULL, NULL, G_DBUS_CALL_FLAGS_NONE, 5000,
                                                                NULL, NULL)
                                  : NULL;
        if (!r)
            return 1;
        gboolean cur;
        g_variant_get(r, "(buuu)", &cur, NULL, NULL, NULL);
        on = !cur;
    } else {
        g_printerr("big-wm-notifications: --dnd on|off|toggle\n");
        return 2;
    }
    return client("SetDoNotDisturb", g_variant_new("(b)", on));
}

/* == Name ownership ========================================================= */

static gboolean
owner_is_dunst(guint32 *pid_out)
{
    g_autoptr(GVariant) r = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetConnectionUnixProcessID", g_variant_new("(s)", FDO_NAME), G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL);
    if (!r)
        return FALSE;
    guint32 pid;
    g_variant_get(r, "(u)", &pid);
    g_autofree char *path = g_strdup_printf("/proc/%u/comm", pid);
    g_autofree char *comm = NULL;
    if (!g_file_get_contents(path, &comm, NULL, NULL))
        return FALSE;
    *pid_out = pid;
    return g_str_equal(g_strstrip(comm), "dunst");
}

static void
fdo_acquired(GDBusConnection *c, const char *name, gpointer data)
{
    (void)c; (void)data;
    g_message("%s acquired", name);
}

static void
fdo_lost(GDBusConnection *c, const char *name, gpointer data)
{
    (void)data;
    guint32 pid;
    if (!c) {
        g_printerr("big-wm-notifications: lost the session bus\n");
        exit(1);
    }
    if (!took_over && owner_is_dunst(&pid)) {
        /* dunst came first (D-Bus activation at login). In a Big WindowMaker
         * session the notifications are ours: stop it, the name comes back
         * to us from the queue. */
        took_over = TRUE;
        g_message("stopping dunst (pid %u), which held %s", pid, name);
        kill((pid_t)pid, SIGTERM);
        return;
    }
    if (took_over) /* still queued: wait for dunst to exit */
        return;
    g_printerr("big-wm-notifications: %s belongs to another server; exiting\n", name);
    exit(0);
}

static void
ctl_acquired(GDBusConnection *c, const char *name, gpointer data)
{
    (void)name; (void)data;
    bwn_server_start(c);
    bwn_gtk_start(c); /* org.gtk.Notifications: GNotification applications */
    bwn_sni_start(c);
    /* Queue for the standard name: a server that already has it gets it
     * replaced if it allows that; dunst does not, see fdo_lost. */
    g_bus_own_name_on_connection(c, FDO_NAME, G_BUS_NAME_OWNER_FLAGS_REPLACE |
                                 G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT, fdo_acquired, fdo_lost,
                                 NULL, NULL);
}

static void
ctl_lost(GDBusConnection *c, const char *name, gpointer data)
{
    (void)c; (void)name; (void)data;
    /* Another big-wm-notifications runs this session. */
    exit(0);
}

static gboolean
on_term(gpointer data)
{
    (void)data;
    bwn_store_flush();
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

int
main(int argc, char **argv)
{
    const char *dir = g_getenv("BIGWM_LOCALEDIR");
    bindtextdomain("big-wm", dir && *dir ? dir : LOCALEDIR);
    bind_textdomain_codeset("big-wm", "UTF-8");
    textdomain("big-wm");

    if (argc > 1) {
        if (g_str_equal(argv[1], "--toggle"))
            return client("ToggleCenter", NULL);
        if (g_str_equal(argv[1], "--clear"))
            return client("Clear", NULL);
        if (g_str_equal(argv[1], "--status"))
            return client("GetStatus", NULL);
        if (g_str_equal(argv[1], "--reload"))
            return client("Reload", NULL);
        if (g_str_equal(argv[1], "--dnd") && argc > 2)
            return dnd_client(argv[2]);
        if (g_str_equal(argv[1], "--version")) {
            g_print("big-wm-notifications %s\n", BWN_VERSION);
            return 0;
        }
        if (!g_str_equal(argv[1], "--daemon")) {
            g_printerr("usage: big-wm-notifications [--toggle|--dnd on|off|toggle|--clear|--status|--reload]\n");
            return 2;
        }
    }

    gtk_init(&argc, &argv);
    g_set_prgname("big-wm-notifications");
    g_autoptr(GError) e = NULL;
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
    if (!bus) {
        g_printerr("big-wm-notifications: %s\n", e->message);
        return 1;
    }
    bwn_settings_load();
    bwn_style_init();
    bwn_store_init();
    bwn_toast_init();
    bwn_wl_activation_init();
    g_unix_signal_add(SIGTERM, on_term, NULL);
    g_unix_signal_add(SIGINT, on_term, NULL);
    g_bus_own_name_on_connection(bus, CTL_NAME, G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, ctl_acquired,
                                 ctl_lost, NULL, NULL);
    gtk_main();
    return 0;
}
