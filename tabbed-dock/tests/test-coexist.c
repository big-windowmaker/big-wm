/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-coexist.h"

#include <glib/gstdio.h>
#include <string.h>

static char *tmpdir;

/* A fake wdwrite that edits a one-line plist, recording its argv. It lives
 * in the build dir: /tmp may be mounted noexec. */
static char *
make_fake_wdwrite(const char *defaults)
{
    char *dir = g_build_filename(g_getenv("G_TEST_BUILDDIR") ? g_getenv("G_TEST_BUILDDIR") : ".", NULL);
    char *path = g_build_filename(dir, "fake-wdwrite", NULL);
    char *script = g_strdup_printf("#!/bin/sh\n"
                                   "echo \"$@\" >> '%s.log'\n"
                                   "grep -v DisableDock '%s' > '%s.new' 2>/dev/null\n"
                                   "echo \"  $2 = $3;\" >> '%s.new'\n"
                                   "mv '%s.new' '%s'\n",
                                   defaults, defaults, defaults, defaults, defaults, defaults);
    g_assert_true(g_file_set_contents(path, script, -1, NULL));
    g_chmod(path, 0755);
    g_free(script);
    g_free(dir);
    return path;
}

static void
test_modes(void)
{
    BtdCoexistPaths paths;
    paths.autostart_dir = g_build_filename(tmpdir, "autostart", NULL);
    paths.wm_defaults = g_build_filename(tmpdir, "GNUstep", "Defaults", "WindowMaker", NULL);
    char *dd = g_path_get_dirname(paths.wm_defaults);
    g_mkdir_with_parents(dd, 0755);
    g_free(dd);
    g_assert_true(g_file_set_contents(paths.wm_defaults, "{\n  IconSize = 64;\n  FocusMode = manual;\n}\n", -1, NULL));
    paths.wdwrite = make_fake_wdwrite(paths.wm_defaults);

    /* fresh account: both docks */
    g_assert_cmpint(btd_coexist_get_mode(&paths, FALSE), ==, BTD_MODE_BOTH);
    g_assert_cmpint(btd_coexist_get_mode(&paths, TRUE), ==, BTD_MODE_BOTH);

    GError *err = NULL;
    g_assert_true(btd_coexist_set_mode(&paths, BTD_MODE_TABBED, &err));
    g_assert_no_error(err);
    g_assert_cmpint(btd_coexist_get_mode(&paths, FALSE), ==, BTD_MODE_TABBED);
    g_assert_cmpint(btd_coexist_get_mode(&paths, TRUE), ==, BTD_MODE_TABBED);
    char *content = NULL;
    g_file_get_contents(paths.wm_defaults, &content, NULL, NULL);
    g_assert_nonnull(strstr(content, "DisableDock = YES;"));
    g_assert_nonnull(strstr(content, "FocusMode = manual;")); /* other keys kept */
    g_free(content);

    g_assert_true(btd_coexist_set_mode(&paths, BTD_MODE_CLASSIC, &err));
    g_assert_cmpint(btd_coexist_get_mode(&paths, TRUE), ==, BTD_MODE_CLASSIC);
    g_assert_cmpint(btd_coexist_get_mode(&paths, FALSE), ==, BTD_MODE_CLASSIC);
    char *wl = g_build_filename(paths.autostart_dir, "big-wm-wayland-dock.desktop", NULL);
    g_assert_false(g_file_test(wl, G_FILE_TEST_EXISTS)); /* our override removed */

    g_assert_true(btd_coexist_set_mode(&paths, BTD_MODE_NONE, &err));
    g_assert_cmpint(btd_coexist_get_mode(&paths, TRUE), ==, BTD_MODE_NONE);
    g_assert_cmpint(btd_coexist_get_mode(&paths, FALSE), ==, BTD_MODE_NONE);

    /* a user's own override file is preserved as .user-backup, never deleted */
    g_assert_true(btd_coexist_set_mode(&paths, BTD_MODE_BOTH, &err));
    g_assert_true(g_file_set_contents(wl, "[Desktop Entry]\nType=Application\nName=mine\nHidden=false\nX-Custom=1\n",
                                      -1, NULL));
    g_assert_true(btd_coexist_set_mode(&paths, BTD_MODE_TABBED, &err));
    char *backup = g_strconcat(wl, ".user-backup", NULL);
    g_assert_true(g_file_test(backup, G_FILE_TEST_EXISTS));
    g_file_get_contents(backup, &content, NULL, NULL);
    g_assert_nonnull(strstr(content, "X-Custom=1"));
    g_free(content);
    g_assert_cmpint(btd_coexist_get_mode(&paths, TRUE), ==, BTD_MODE_TABBED);
    g_free(backup);
    g_free(wl);

    /* no wdwrite: X11 step reports an error, Wayland overrides still applied */
    g_clear_pointer(&paths.wdwrite, g_free);
    g_assert_false(btd_coexist_set_mode(&paths, BTD_MODE_BOTH, &err));
    g_assert_nonnull(err);
    g_clear_error(&err);
    g_assert_true(btd_coexist_classic_wayland_enabled(&paths));

    BtdDockMode m;
    g_assert_true(btd_dock_mode_from_string("Both", &m));
    g_assert_cmpint(m, ==, BTD_MODE_BOTH);
    g_assert_false(btd_dock_mode_from_string("bogus", &m));
    btd_coexist_paths_clear(&paths);
}

static void
test_detect_variants(void)
{
    BtdCoexistPaths paths = { 0 };
    paths.autostart_dir = g_build_filename(tmpdir, "none", NULL);
    paths.wm_defaults = g_build_filename(tmpdir, "wm-variant", NULL);
    g_file_set_contents(paths.wm_defaults, "{DisableDock=\"YES\";}", -1, NULL);
    g_assert_false(btd_coexist_classic_x11_enabled(&paths));
    g_file_set_contents(paths.wm_defaults, "{ DisableDockX = YES; DisableDock = NO; }", -1, NULL);
    g_assert_true(btd_coexist_classic_x11_enabled(&paths));
    btd_coexist_paths_clear(&paths);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    tmpdir = g_dir_make_tmp("btd-coexist-XXXXXX", NULL);
    g_test_add_func("/coexist/modes", test_modes);
    g_test_add_func("/coexist/detect-variants", test_detect_variants);
    int ret = g_test_run();
    char *argv_rm[] = { "rm", "-rf", tmpdir, NULL };
    g_spawn_sync(NULL, argv_rm, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    return ret;
}
