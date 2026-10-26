/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-launch.h"

#include <glib/gstdio.h>
#include <unistd.h>

static char *tmpdir;

static void
test_escape(void)
{
    char *s = btd_launch_escape_unit("org.gnome.Calculator");
    g_assert_cmpstr(s, ==, "org.gnome.Calculator");
    g_free(s);
    s = btd_launch_escape_unit("big-wm tabbed/dock");
    g_assert_cmpstr(s, ==, "big\\x2dwm\\x20tabbed\\x2fdock");
    g_free(s);
    s = btd_launch_escape_unit(".hidden");
    g_assert_cmpstr(s, ==, "\\x2ehidden");
    g_free(s);
}

static void
test_appimage_safety(void)
{
    GError *err = NULL;
    char *p = g_build_filename(tmpdir, "App.AppImage", NULL);
    g_assert_false(btd_launch_appimage_is_safe(p, &err)); /* missing */
    g_clear_error(&err);
    g_file_set_contents(p, "\x7f" "ELF", -1, NULL);
    g_chmod(p, 0644);
    g_assert_false(btd_launch_appimage_is_safe(p, &err)); /* not executable */
    g_clear_error(&err);
    g_chmod(p, 0755);
    g_assert_true(btd_launch_appimage_is_safe(p, &err));
    g_chmod(p, 0777);
    g_assert_false(btd_launch_appimage_is_safe(p, &err)); /* world-writable */
    g_clear_error(&err);
    g_assert_false(btd_launch_appimage_is_safe(tmpdir, &err)); /* directory */
    g_clear_error(&err);
    g_assert_false(btd_launch_appimage_is_safe("relative", &err));
    g_clear_error(&err);
    g_free(p);
}

static void
test_app_info_validation(void)
{
    BtdItem it = { .type = BTD_ITEM_APP, .target = "../../evil.desktop" };
    g_assert_null(btd_launch_app_info(&it));
    it.target = "/etc/passwd";
    g_assert_null(btd_launch_app_info(&it));
    it.target = "no-such-app-anywhere.desktop";
    g_assert_null(btd_launch_app_info(&it));
    GError *err = NULL;
    g_assert_false(btd_launch_item(&it, NULL, NULL, &err));
    g_assert_error(err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error(&err);
    BtdItem f = { .type = BTD_ITEM_FILE, .target = "/nonexistent/file.txt" };
    g_assert_false(btd_launch_item(&f, NULL, NULL, &err));
    g_assert_error(err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error(&err);
}

/* A real .desktop launch: Exec with field codes, no shell. The program writes
 * its argv to a file; quotes and ';' in the file name must arrive intact. */
static void
test_exec_no_shell(void)
{
    char *builddir = g_strdup(g_getenv("G_TEST_BUILDDIR") ? g_getenv("G_TEST_BUILDDIR") : ".");
    char *out = g_build_filename(tmpdir, "argv.txt", NULL);
    char *prog = g_build_filename(builddir, "fake-app", NULL);
    char *script = g_strdup_printf("#!/bin/sh\nfor a in \"$@\"; do printf '%%s\\n' \"$a\"; done > '%s'\n", out);
    g_file_set_contents(prog, script, -1, NULL);
    g_chmod(prog, 0755);
    char *desktop = g_build_filename(tmpdir, "fake.desktop", NULL);
    char *content = g_strdup_printf("[Desktop Entry]\nType=Application\nName=Fake\nExec=%s --flag %%F\n", prog);
    g_file_set_contents(desktop, content, -1, NULL);

    char *weird = g_build_filename(tmpdir, "a b;$(touch pwned)'\".txt", NULL);
    g_file_set_contents(weird, "x", -1, NULL);
    GFile *wf = g_file_new_for_path(weird);
    GList uris = { .data = g_file_get_uri(wf) };

    btd_launch_set_use_scopes(FALSE);
    BtdItem it = { .type = BTD_ITEM_APP, .target = desktop };
    GError *err = NULL;
    g_assert_true(btd_launch_item(&it, NULL, &uris, &err));
    g_assert_no_error(err);
    for (int i = 0; i < 100 && !g_file_test(out, G_FILE_TEST_EXISTS); i++)
        g_usleep(20000);
    g_usleep(50000);
    char *got = NULL;
    g_assert_true(g_file_get_contents(out, &got, NULL, NULL));
    char *expected = g_strdup_printf("--flag\n%s\n", weird);
    g_assert_cmpstr(got, ==, expected);
    char *cwd = g_get_current_dir();
    char *pwned = g_build_filename(cwd, "pwned", NULL);
    g_free(cwd);
    g_assert_false(g_file_test(pwned, G_FILE_TEST_EXISTS));

    g_free(pwned);
    g_free(expected);
    g_free(got);
    g_free(uris.data);
    g_object_unref(wf);
    g_free(weird);
    g_free(content);
    g_free(desktop);
    g_free(script);
    g_free(prog);
    g_free(out);
    g_free(builddir);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    tmpdir = g_dir_make_tmp("btd-launch-XXXXXX", NULL);
    g_test_add_func("/launch/escape", test_escape);
    g_test_add_func("/launch/appimage-safety", test_appimage_safety);
    g_test_add_func("/launch/app-info-validation", test_app_info_validation);
    g_test_add_func("/launch/exec-no-shell", test_exec_no_shell);
    int ret = g_test_run();
    char *argv_rm[] = { "rm", "-rf", tmpdir, NULL };
    g_spawn_sync(NULL, argv_rm, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free(tmpdir);
    return ret;
}
