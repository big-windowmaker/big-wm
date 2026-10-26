/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-coexist.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

#define TABBED_AUTOSTART "big-wm-tabbed-dock.desktop"
#define WAYLAND_DOCK_AUTOSTART "big-wm-wayland-dock.desktop"
#define MANAGED_KEY "X-Big-Tabbed-Dock-Managed"

static const char *mode_names[] = { "classic", "tabbed", "both", "none" };

const char *
btd_dock_mode_to_string(BtdDockMode mode)
{
    return mode_names[mode];
}

gboolean
btd_dock_mode_from_string(const char *s, BtdDockMode *mode)
{
    for (guint i = 0; s && i < G_N_ELEMENTS(mode_names); i++) {
        if (g_ascii_strcasecmp(s, mode_names[i]) == 0) {
            *mode = (BtdDockMode)i;
            return TRUE;
        }
    }
    return FALSE;
}

void
btd_coexist_paths_init(BtdCoexistPaths *paths)
{
    paths->autostart_dir = g_build_filename(g_get_user_config_dir(), "autostart", NULL);
    const char *gs = g_getenv("GNUSTEP_USER_ROOT");
    char *root = gs && *gs ? g_strdup(gs) : g_build_filename(g_get_home_dir(), "GNUstep", NULL);
    paths->wm_defaults = g_build_filename(root, "Defaults", "WindowMaker", NULL);
    g_free(root);
    paths->wdwrite = g_find_program_in_path("wdwrite");
}

void
btd_coexist_paths_clear(BtdCoexistPaths *paths)
{
    g_clear_pointer(&paths->autostart_dir, g_free);
    g_clear_pointer(&paths->wm_defaults, g_free);
    g_clear_pointer(&paths->wdwrite, g_free);
}

/* An autostart entry is disabled by a user file with Hidden=true. */
static gboolean
autostart_hidden(const BtdCoexistPaths *paths, const char *name, gboolean *managed)
{
    char *path = g_build_filename(paths->autostart_dir, name, NULL);
    GKeyFile *kf = g_key_file_new();
    gboolean hidden = FALSE;
    if (managed)
        *managed = FALSE;
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL);
        if (managed)
            *managed = g_key_file_get_boolean(kf, "Desktop Entry", MANAGED_KEY, NULL);
    }
    g_key_file_unref(kf);
    g_free(path);
    return hidden;
}

static gboolean
set_autostart_enabled(const BtdCoexistPaths *paths, const char *name, gboolean enabled, GError **error)
{
    gboolean managed = FALSE;
    char *path = g_build_filename(paths->autostart_dir, name, NULL);
    gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS);
    gboolean hidden = exists && autostart_hidden(paths, name, &managed);
    gboolean ok = TRUE;

    if (enabled == !hidden) {
        g_free(path);
        return TRUE; /* already in the requested state */
    }
    if (exists && !managed) {
        /* A file the user wrote: keep it, renamed, never delete it. */
        char *backup = g_strconcat(path, ".user-backup", NULL);
        if (g_rename(path, backup) != 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "cannot back up %s: %s", path,
                        g_strerror(errno));
            ok = FALSE;
        }
        g_free(backup);
    } else if (exists && enabled) {
        if (g_unlink(path) != 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "cannot remove %s: %s", path,
                        g_strerror(errno));
            ok = FALSE;
        }
    }
    if (ok && !enabled) {
        g_mkdir_with_parents(paths->autostart_dir, 0700);
        char *content = g_strdup_printf("[Desktop Entry]\n"
                                        "# Written by big-wm-tabbed-dock --dock-mode; delete to revert.\n"
                                        "Type=Application\n"
                                        "Name=%s\n"
                                        "Hidden=true\n"
                                        MANAGED_KEY "=true\n",
                                        name);
        ok = g_file_set_contents_full(path, content, -1, G_FILE_SET_CONTENTS_CONSISTENT, 0644, error);
        g_free(content);
    }
    g_free(path);
    return ok;
}

gboolean
btd_coexist_tabbed_enabled(const BtdCoexistPaths *paths)
{
    return !autostart_hidden(paths, TABBED_AUTOSTART, NULL);
}

gboolean
btd_coexist_classic_wayland_enabled(const BtdCoexistPaths *paths)
{
    return !autostart_hidden(paths, WAYLAND_DOCK_AUTOSTART, NULL);
}

gboolean
btd_coexist_classic_x11_enabled(const BtdCoexistPaths *paths)
{
    /* Read-only scan of the property list for "DisableDock = YES". */
    char *data = NULL;
    gboolean enabled = TRUE;
    if (g_file_get_contents(paths->wm_defaults, &data, NULL, NULL)) {
        GRegex *re = g_regex_new("(^|[;{\\s])\"?DisableDock\"?\\s*=\\s*\"?(YES|Yes|yes|Y|y)\"?\\s*;", 0, 0, NULL);
        enabled = !g_regex_match(re, data, 0, NULL);
        g_regex_unref(re);
        g_free(data);
    }
    return enabled;
}

BtdDockMode
btd_coexist_get_mode(const BtdCoexistPaths *paths, gboolean wayland)
{
    gboolean tabbed = btd_coexist_tabbed_enabled(paths);
    gboolean classic = wayland ? btd_coexist_classic_wayland_enabled(paths) : btd_coexist_classic_x11_enabled(paths);
    if (tabbed && classic)
        return BTD_MODE_BOTH;
    if (tabbed)
        return BTD_MODE_TABBED;
    if (classic)
        return BTD_MODE_CLASSIC;
    return BTD_MODE_NONE;
}

static gboolean
set_classic_x11(const BtdCoexistPaths *paths, gboolean enabled, GError **error)
{
    if (btd_coexist_classic_x11_enabled(paths) == enabled)
        return TRUE;
    if (!paths->wdwrite) {
        g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT,
                    "wdwrite (Window Maker) not found; the X11 Dock was not changed");
        return FALSE;
    }
    const char *argv[] = { paths->wdwrite, "WindowMaker", "DisableDock", enabled ? "NO" : "YES", NULL };
    int status = 0;
    if (!g_spawn_sync(NULL, (char **)argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, NULL, &status,
                      error))
        return FALSE;
    return g_spawn_check_wait_status(status, error);
}

gboolean
btd_coexist_set_mode(const BtdCoexistPaths *paths, BtdDockMode mode, GError **error)
{
    gboolean tabbed = mode == BTD_MODE_TABBED || mode == BTD_MODE_BOTH;
    gboolean classic = mode == BTD_MODE_CLASSIC || mode == BTD_MODE_BOTH;
    GError *e1 = NULL, *e2 = NULL, *e3 = NULL;
    gboolean ok = set_autostart_enabled(paths, TABBED_AUTOSTART, tabbed, &e1);
    ok = set_autostart_enabled(paths, WAYLAND_DOCK_AUTOSTART, classic, &e2) && ok;
    ok = set_classic_x11(paths, classic, &e3) && ok;
    GError *first = e1 ? e1 : e2 ? e2 : e3;
    if (first)
        g_propagate_error(error, g_error_copy(first));
    g_clear_error(&e1);
    g_clear_error(&e2);
    g_clear_error(&e3);
    return ok;
}
