/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-launch.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static gboolean use_scopes = TRUE;

void
btd_launch_set_use_scopes(gboolean use)
{
    use_scopes = use;
}

char *
btd_launch_escape_unit(const char *s)
{
    GString *out = g_string_new(NULL);
    for (const char *p = s; p && *p; p++) {
        if (g_ascii_isalnum(*p) || *p == ':' || *p == '_' || (*p == '.' && p != s))
            g_string_append_c(out, *p);
        else
            g_string_append_printf(out, "\\x%02x", (guchar)*p);
    }
    return g_string_free(out, FALSE);
}

static void
scope_done(GObject *source, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    GVariant *ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
    if (ret)
        g_variant_unref(ret);
    else {
        g_debug("transient scope not created: %s", error->message);
        g_error_free(error);
    }
}

/* Moves pid into app-big\x2dwm\x2dtabbed\x2ddock-<app>-<pid>.scope. */
static void
move_to_scope(const char *app, GPid pid)
{
    if (!use_scopes || pid <= 0)
        return;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    if (!bus)
        return;
    char *base = g_strdup(app ? app : "app");
    if (g_str_has_suffix(base, ".desktop"))
        base[strlen(base) - strlen(".desktop")] = '\0';
    char *esc_app = btd_launch_escape_unit(base);
    char *esc_launcher = btd_launch_escape_unit("big-wm-tabbed-dock");
    char *unit = g_strdup_printf("app-%s-%s-%d.scope", esc_launcher, esc_app, (int)pid);

    GVariantBuilder props;
    g_variant_builder_init(&props, G_VARIANT_TYPE("a(sv)"));
    guint32 p = (guint32)pid;
    g_variant_builder_add(&props, "(sv)", "PIDs",
                          g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, &p, 1, sizeof(guint32)));
    g_variant_builder_add(&props, "(sv)", "CollectMode", g_variant_new_string("inactive-or-failed"));
    g_variant_builder_add(&props, "(sv)", "Description", g_variant_new_string(base));
    g_dbus_connection_call(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager", "StartTransientUnit",
                           g_variant_new("(ssa(sv)a(sa(sv)))", unit, "fail", &props, NULL), G_VARIANT_TYPE("(o)"),
                           G_DBUS_CALL_FLAGS_NO_AUTO_START, 3000, NULL, scope_done, NULL);
    g_free(unit);
    g_free(esc_launcher);
    g_free(esc_app);
    g_free(base);
    g_object_unref(bus);
}

static void
pid_callback(GDesktopAppInfo *info, GPid pid, gpointer data)
{
    move_to_scope(g_app_info_get_id(G_APP_INFO(info)), pid);
}

static void
child_setup(gpointer data)
{
    /* New session/process group: the child is not tied to the dock's terminal or signals. */
    setsid();
}

GDesktopAppInfo *
btd_launch_app_info(const BtdItem *item)
{
    if (!item || item->type != BTD_ITEM_APP || !item->target)
        return NULL;
    if (g_path_is_absolute(item->target)) {
        GStatBuf st;
        if (!g_str_has_suffix(item->target, ".desktop") || g_stat(item->target, &st) != 0 || !S_ISREG(st.st_mode) ||
            st.st_size > 256 * 1024)
            return NULL;
        return g_desktop_app_info_new_from_filename(item->target);
    }
    if (!btd_desktop_id_is_valid(item->target))
        return NULL;
    return g_desktop_app_info_new(item->target);
}

static gboolean
launch_info(GDesktopAppInfo *info, GList *uris, GAppLaunchContext *ctx, GError **error)
{
    GList *uri_list = NULL;
    for (GList *l = uris; l; l = l->next)
        uri_list = g_list_append(uri_list, l->data);
    gboolean ok = g_desktop_app_info_launch_uris_as_manager(info, uri_list, ctx,
                                                            G_SPAWN_SEARCH_PATH,
                                                            child_setup, NULL, pid_callback, NULL, error);
    g_list_free(uri_list);
    return ok;
}

gboolean
btd_launch_desktop_id(const char *id, GAppLaunchContext *ctx, GError **error)
{
    BtdItem item = { .type = BTD_ITEM_APP, .target = (char *)id };
    return btd_launch_item(&item, ctx, NULL, error);
}

gboolean
btd_launch_appimage_is_safe(const char *path, GError **error)
{
    GStatBuf st;
    if (!path || !g_path_is_absolute(path) || g_stat(path, &st) != 0) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "%s not found", path ? path : "(null)");
        return FALSE;
    }
    if (!S_ISREG(st.st_mode) || access(path, X_OK) != 0) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "%s is not an executable file", path);
        return FALSE;
    }
    if ((st.st_mode & S_IWOTH) || (st.st_uid != getuid() && st.st_uid != 0)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                    "%s is writable by others or owned by another user; refusing to run it", path);
        return FALSE;
    }
    return TRUE;
}

static gboolean
open_path(const char *path, GAppLaunchContext *ctx, GError **error)
{
    GFile *file = g_file_new_for_path(path);
    if (!g_file_query_exists(file, NULL)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "%s does not exist (removable drive?)", path);
        g_object_unref(file);
        return FALSE;
    }
    gboolean ok;
    GAppInfo *handler = g_file_query_default_handler(file, NULL, NULL);
    GList uris = { .data = g_file_get_uri(file) };
    if (handler && G_IS_DESKTOP_APP_INFO(handler)) {
        ok = launch_info(G_DESKTOP_APP_INFO(handler), &uris, ctx, error);
    } else {
        ok = g_app_info_launch_default_for_uri(uris.data, ctx, error);
    }
    g_free(uris.data);
    g_clear_object(&handler);
    g_object_unref(file);
    return ok;
}

gboolean
btd_launch_item(const BtdItem *item, GAppLaunchContext *ctx, GList *uris, GError **error)
{
    switch (item->type) {
    case BTD_ITEM_APP: {
        GDesktopAppInfo *info = btd_launch_app_info(item);
        if (!info) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "application %s is not installed", item->target);
            return FALSE;
        }
        gboolean ok = launch_info(info, uris, ctx, error);
        g_object_unref(info);
        return ok;
    }
    case BTD_ITEM_FILE:
    case BTD_ITEM_FOLDER:
        return open_path(item->target, ctx, error);
    case BTD_ITEM_APPIMAGE: {
        if (!btd_launch_appimage_is_safe(item->target, error))
            return FALSE;
        char *dir = g_path_get_dirname(item->target);
        char *argv[] = { item->target, NULL };
        char **envp = ctx ? g_app_launch_context_get_environment(ctx) : g_get_environ();
        GPid pid = 0;
        gboolean ok = g_spawn_async(dir, argv, envp, G_SPAWN_DEFAULT, child_setup, NULL, &pid, error);
        if (ok) {
            char *base = g_path_get_basename(item->target);
            move_to_scope(base, pid);
            g_free(base);
        }
        g_strfreev(envp);
        g_free(dir);
        return ok;
    }
    case BTD_ITEM_SEPARATOR:
        break;
    }
    return TRUE;
}

gboolean
btd_launch_action(const BtdItem *item, const char *action, GAppLaunchContext *ctx, GError **error)
{
    GDesktopAppInfo *info = btd_launch_app_info(item);
    if (!info) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "application %s is not installed", item->target);
        return FALSE;
    }
    gboolean found = FALSE;
    const char *const *actions = g_desktop_app_info_list_actions(info);
    for (; actions && *actions; actions++)
        found = found || g_strcmp0(*actions, action) == 0;
    if (found)
        g_desktop_app_info_launch_action(info, action, ctx);
    else
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "no action %s", action);
    g_object_unref(info);
    return found;
}

/* LD_PRELOAD lists libraries separated by colons or spaces. */
static gboolean
preload_has(const char *preload, const char *library)
{
    if (!preload || !library)
        return FALSE;
    gboolean found = FALSE;
    char **parts = g_strsplit_set(preload, ": ", -1);
    for (char **p = parts; *p && !found; p++)
        found = g_str_equal(*p, library);
    g_strfreev(parts);
    return found;
}

static gboolean
desktop_is_big_wm(const char *desktop)
{
    if (!desktop)
        return FALSE;
    gboolean found = FALSE;
    char **parts = g_strsplit(desktop, ":", -1);
    for (char **p = parts; *p && !found; p++)
        found = g_str_equal(*p, "WindowMaker") || g_str_equal(*p, "wlmaker");
    g_strfreev(parts);
    return found;
}

char *
btd_launch_nocsd_preload(const char *desktop, const char *own_preload, const char *session_preload,
                         const char *nocsd_path)
{
    const char *library = nocsd_path ? nocsd_path : BTD_NOCSD_LIBRARY;
    if (!desktop_is_big_wm(desktop) || preload_has(own_preload, library))
        return NULL;
    if (!preload_has(session_preload, library) || !g_file_test(library, G_FILE_TEST_IS_REGULAR))
        return NULL;
    return g_strdup(session_preload);
}

char *
btd_launch_session_preload(void)
{
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    if (!bus)
        return NULL;
    char *value = NULL;
    GVariant *reply = g_dbus_connection_call_sync(
        bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.freedesktop.systemd1.Manager", "Environment"), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, 3000, NULL, NULL);
    if (reply) {
        GVariant *boxed = NULL;
        g_variant_get(reply, "(v)", &boxed);
        if (boxed && g_variant_is_of_type(boxed, G_VARIANT_TYPE_STRING_ARRAY)) {
            const char **env = g_variant_get_strv(boxed, NULL);
            const char *found = g_environ_getenv((char **)env, "LD_PRELOAD");
            value = found && *found ? g_strdup(found) : NULL;
            g_free(env);
        }
        g_clear_pointer(&boxed, g_variant_unref);
        g_variant_unref(reply);
    }
    g_object_unref(bus);
    return value;
}
