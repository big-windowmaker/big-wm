/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-defaults.h"
#include "btd-desktop.h"

#include <glib/gstdio.h>

static char *tmpdir, *home_data, *sys_data, *flatpak_data;

static void
write_desktop(const char *datadir, const char *relpath, const char *body)
{
    char *path = g_build_filename(datadir, "applications", relpath, NULL);
    char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    char *content = g_strconcat("[Desktop Entry]\nType=Application\n", body, NULL);
    g_assert_true(g_file_set_contents(path, content, -1, NULL));
    g_free(content);
    g_free(dir);
    g_free(path);
}

static void
setup_fixtures(void)
{
    home_data = g_build_filename(tmpdir, "home", NULL);
    sys_data = g_build_filename(tmpdir, "sys", NULL);
    flatpak_data = g_build_filename(tmpdir, "flatpak", NULL);
    /* "sh" exists everywhere: use it as an available program */
    write_desktop(sys_data, "org.gnome.Calculator.desktop",
                  "Name=Calculator\nName[pt_BR]=Calculadora\nExec=sh %U\nIcon=accessories-calculator\n"
                  "DBusActivatable=true\n");
    write_desktop(sys_data, "xterm.desktop", "Name=XTerm\nExec=xterm-not-installed\n");
    write_desktop(sys_data, "thunar.desktop", "Name=Thunar\nExec=sh %F\nStartupWMClass=Thunar\n");
    write_desktop(sys_data, "thunar-settings.desktop", "Name=Thunar prefs\nExec=thunar --settings\nNoDisplay=true\n");
    write_desktop(sys_data, "code.desktop", "Name=Code\nExec=/usr/bin/env FOO=1 code --new-window %F\n");
    write_desktop(sys_data, "wrapped.desktop", "Name=Wrapped\nExec=sh -c 'echo hi'\n");
    write_desktop(sys_data, "hidden.desktop", "Name=Hidden\nExec=sh\nHidden=true\n");
    write_desktop(sys_data, "link.desktop", "Name=Link\nExec=sh\n");
    write_desktop(sys_data, "kde/konsole.desktop", "Name=Konsole\nExec=sh\n");
    write_desktop(sys_data, "tryexec.desktop", "Name=TryExec\nExec=sh\nTryExec=/nonexistent/bin\n");
    write_desktop(sys_data, "com.biglinux.calamares-config.desktop",
                  "Name=Install System\nName[pt_BR]=Instalar o sistema no HD\nExec=sh\nIcon=icon-big-hd-install\n");
    write_desktop(sys_data, "installer-wrapper.desktop",
                  "Name=Installer\nExec=sh-wrapper-not-installed\nStartupWMClass=calamares\n");
    write_desktop(home_data, "link.desktop", "Name=User link\nExec=sh\n"); /* overrides sys */
    write_desktop(flatpak_data, "org.mozilla.firefox.desktop",
                  "Name=Firefox\nExec=/usr/bin/flatpak run --branch=stable --command=firefox org.mozilla.firefox @@u %u @@\n"
                  "X-Flatpak=org.mozilla.firefox\n");
    char *notapp = g_build_filename(sys_data, "applications", "dir.desktop", NULL);
    g_file_set_contents(notapp, "[Desktop Entry]\nType=Directory\nName=x\n", -1, NULL);
    g_free(notapp);
    char *bad = g_build_filename(sys_data, "applications", "bad.desktop", NULL);
    g_file_set_contents(bad, "\x01\x02 not a key file", -1, NULL);
    g_free(bad);
}

static BtdDesktopIndex *
make_index(void)
{
    const char *dirs[] = { home_data, sys_data, flatpak_data, NULL };
    return btd_desktop_index_new(dirs);
}

static void
test_index(void)
{
    BtdDesktopIndex *idx = make_index();
    const BtdDesktopEntry *e = btd_desktop_index_lookup(idx, "org.gnome.Calculator.desktop");
    g_assert_nonnull(e);
    g_assert_cmpstr(e->name, ==, "Calculator");
    g_assert_cmpstr(e->icon, ==, "accessories-calculator");
    g_assert_true(e->available);
    g_assert_true(e->dbus_activatable);
    g_assert_false(btd_desktop_index_lookup(idx, "xterm.desktop")->available);
    g_assert_false(btd_desktop_index_lookup(idx, "tryexec.desktop")->available);
    g_assert_null(btd_desktop_index_lookup(idx, "hidden.desktop"));
    g_assert_null(btd_desktop_index_lookup(idx, "dir.desktop"));
    g_assert_null(btd_desktop_index_lookup(idx, "bad.desktop"));
    g_assert_cmpstr(btd_desktop_index_lookup(idx, "link.desktop")->name, ==, "User link");
    g_assert_nonnull(btd_desktop_index_lookup(idx, "kde-konsole.desktop"));
    e = btd_desktop_index_lookup(idx, "org.mozilla.firefox.desktop");
    g_assert_cmpstr(e->flatpak_id, ==, "org.mozilla.firefox");
    g_assert_cmpstr(btd_desktop_index_lookup(idx, "code.desktop")->exec_base, ==, "code");

    const char *cands[] = { "missing.desktop", "xterm.desktop", "thunar.desktop", NULL };
    g_assert_cmpstr(btd_desktop_index_first_available(idx, cands)->id, ==, "thunar.desktop");

    char *p = g_build_filename(sys_data, "applications", "thunar.desktop", NULL);
    g_assert_cmpstr(btd_desktop_index_lookup_path(idx, p)->id, ==, "thunar.desktop");
    g_free(p);
    btd_desktop_index_free(idx);
}

static void
test_locale(void)
{
    const char *old = g_getenv("LANGUAGE");
    g_setenv("LANGUAGE", "pt_BR", TRUE);
    char *path = g_build_filename(sys_data, "applications", "org.gnome.Calculator.desktop", NULL);
    BtdDesktopEntry *e = btd_desktop_entry_load(path, NULL);
    g_assert_cmpstr(e->id, ==, "org.gnome.Calculator.desktop");
    /* GLib picks the translation from LANGUAGE/LC_MESSAGES */
    g_assert_true(g_str_equal(e->name, "Calculadora") || g_str_equal(e->name, "Calculator"));
    btd_desktop_entry_free(e);
    g_free(path);
    if (old)
        g_setenv("LANGUAGE", old, TRUE);
    else
        g_unsetenv("LANGUAGE");
}

static void
test_exec_program(void)
{
    char *fp = NULL;
    char *p = btd_desktop_exec_program("env A=1 B=2 /opt/foo/bin/foo --x %U", NULL);
    g_assert_cmpstr(p, ==, "foo");
    g_free(p);
    p = btd_desktop_exec_program("flatpak run --branch=stable org.app.Id %u", &fp);
    g_assert_cmpstr(p, ==, "flatpak");
    g_assert_cmpstr(fp, ==, "org.app.Id");
    g_free(p);
    g_free(fp);
    p = btd_desktop_exec_program("\"unterminated", NULL);
    g_assert_null(p);
    p = btd_desktop_exec_program("sh -c 'rm -rf /'", NULL); /* parsed, never run */
    g_assert_cmpstr(p, ==, "sh");
    g_free(p);
    g_assert_true(btd_desktop_id_is_valid("org.gnome.Calculator.desktop"));
    g_assert_false(btd_desktop_id_is_valid("../x.desktop"));
    g_assert_false(btd_desktop_id_is_valid("x.desktop;rm"));
    g_assert_false(btd_desktop_id_is_valid(".desktop"));
}

static void
test_match(void)
{
    BtdDesktopIndex *idx = make_index();
    BtdWindowKeys k = { 0 };

    k.app_id = "org.gnome.Calculator"; /* Wayland app_id / GTK application id */
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "org.gnome.Calculator.desktop");

    k = (BtdWindowKeys){ .wm_class = "Thunar", .wm_instance = "thunar" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "thunar.desktop");

    k = (BtdWindowKeys){ .wm_class = "Code", .wm_instance = "code" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "code.desktop");

    k = (BtdWindowKeys){ .app_id = "org.mozilla.firefox" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "org.mozilla.firefox.desktop");

    char *p = g_build_filename(sys_data, "applications", "link.desktop", NULL);
    k = (BtdWindowKeys){ .wm_class = "Whatever", .launched_desktop_file = p };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "link.desktop");
    g_free(p);

    k = (BtdWindowKeys){ .exe = "code" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "code.desktop");

    /* a program launched by a wrapper: WM_CLASS on X11, reverse-DNS app id
     * ending in the StartupWMClass on Wayland */
    k = (BtdWindowKeys){ .wm_class = "calamares", .wm_instance = "calamares" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "installer-wrapper.desktop");
    k = (BtdWindowKeys){ .app_id = "io.calamares.calamares" };
    g_assert_cmpstr(btd_desktop_index_match(idx, &k), ==, "installer-wrapper.desktop");

    /* wrappers and shells never identify an application */
    k = (BtdWindowKeys){ .exe = "sh", .wm_instance = "sh" };
    g_assert_null(btd_desktop_index_match(idx, &k));
    k = (BtdWindowKeys){ .wm_class = "Unknown", .wm_instance = "unknown" };
    g_assert_null(btd_desktop_index_match(idx, &k));
    g_assert_null(btd_desktop_index_match(idx, NULL));
    btd_desktop_index_free(idx);
}

static void
test_defaults(void)
{
    BtdDesktopIndex *idx = make_index();
    BtdModel *m = btd_model_new();
    btd_defaults_apply(m, idx);
    g_assert_cmpuint(btd_model_n_tabs(m), ==, 4);
    BtdTab *g = btd_model_find_tab(m, "general", NULL);
    /* only installed apps: thunar and the calculator, not xterm */
    gboolean has_thunar = FALSE, has_xterm = FALSE;
    for (guint i = 0; i < g->items->len; i++) {
        BtdItem *it = g->items->pdata[i];
        has_thunar |= g_strcmp0(it->target, "thunar.desktop") == 0;
        has_xterm |= g_strcmp0(it->target, "xterm.desktop") == 0;
    }
    g_assert_true(has_thunar);
    g_assert_false(has_xterm);
    g_assert_cmpuint(btd_model_find_tab(m, "temporary", NULL)->items->len, ==, 0);
    BtdTab *docs = btd_model_find_tab(m, "documents", NULL);
    g_assert_cmpuint(docs->items->len, >=, 1);
    g_assert_cmpint(((BtdItem *)docs->items->pdata[0])->type, ==, BTD_ITEM_FOLDER);
    btd_model_free(m);
    btd_desktop_index_free(idx);
}

static const char *
first_general(BtdModel *m)
{
    BtdTab *g = btd_model_find_tab(m, BTD_TAB_GENERAL, NULL);
    return g->items->len ? ((BtdItem *)g->items->pdata[0])->target : NULL;
}

static gboolean
general_has(BtdModel *m, const char *id)
{
    BtdTab *g = btd_model_find_tab(m, BTD_TAB_GENERAL, NULL);
    for (guint i = 0; i < g->items->len; i++)
        if (g_strcmp0(((BtdItem *)g->items->pdata[i])->target, id) == 0)
            return TRUE;
    return FALSE;
}

static void
test_live_installer(void)
{
    BtdDesktopIndex *idx = make_index();
    char *marker = g_build_filename(tmpdir, "livefs-pkgs.txt", NULL);
    char *stamp = g_build_filename(tmpdir, "run", "big-tabbed-dock", "live-installer", NULL);
    btd_defaults_set_live_marker(marker);

    /* installed system: no marker, no installer (even though the entry exists) */
    g_assert_false(btd_defaults_is_live());
    BtdModel *m = btd_model_new();
    btd_defaults_apply(m, idx);
    g_assert_false(general_has(m, BTD_LIVE_INSTALLER_ID));
    g_assert_false(btd_defaults_offer_live_installer(m, idx, NULL));
    g_assert_false(g_file_test(stamp, G_FILE_TEST_EXISTS));

    /* live: first item of General, before the file manager */
    g_assert_true(g_file_set_contents(marker, "", 0, NULL));
    g_assert_true(btd_defaults_is_live());
    btd_defaults_apply(m, idx);
    g_assert_cmpstr(first_general(m), ==, BTD_LIVE_INSTALLER_ID);
    g_assert_true(general_has(m, "thunar.desktop"));
    g_assert_cmpstr(btd_desktop_index_lookup(idx, BTD_LIVE_INSTALLER_ID)->name, ==, "Install System");
    /* "restore this tab" too */
    btd_defaults_fill_tab(m, idx, BTD_TAB_GENERAL);
    g_assert_cmpstr(first_general(m), ==, BTD_LIVE_INSTALLER_ID);
    /* already there: nothing to do */
    g_assert_false(btd_defaults_offer_live_installer(m, idx, NULL));

    /* configuration saved before (persistence): offered once per boot */
    g_assert_true(btd_model_remove_item(m, BTD_TAB_GENERAL, 0));
    g_assert_false(general_has(m, BTD_LIVE_INSTALLER_ID));
    g_assert_true(btd_defaults_offer_live_installer(m, idx, stamp));
    g_assert_cmpstr(first_general(m), ==, BTD_LIVE_INSTALLER_ID);
    g_assert_true(g_file_test(stamp, G_FILE_TEST_EXISTS));
    /* removed by the user: stays removed until the next boot */
    g_assert_true(btd_model_remove_item(m, BTD_TAB_GENERAL, 0));
    g_assert_false(btd_defaults_offer_live_installer(m, idx, stamp));
    g_assert_false(general_has(m, BTD_LIVE_INSTALLER_ID));
    /* moved by the user: kept where it is */
    btd_model_insert_item(m, BTD_TAB_GENERAL, btd_item_new(BTD_ITEM_APP, BTD_LIVE_INSTALLER_ID), -1);
    g_assert_false(btd_defaults_offer_live_installer(m, idx, NULL));
    g_assert_cmpstr(first_general(m), !=, BTD_LIVE_INSTALLER_ID);
    btd_model_free(m);
    btd_desktop_index_free(idx);

    /* live, but the installer is not installed (or cannot run) */
    const char *dirs[] = { home_data, NULL };
    idx = btd_desktop_index_new(dirs);
    m = btd_model_new();
    btd_defaults_apply(m, idx);
    g_assert_false(general_has(m, BTD_LIVE_INSTALLER_ID));
    g_assert_false(btd_defaults_offer_live_installer(m, idx, NULL));
    btd_model_free(m);
    btd_desktop_index_free(idx);

    g_unlink(marker);
    btd_defaults_set_live_marker("/nonexistent/livefs-pkgs.txt");
    g_free(stamp);
    g_free(marker);
}

static void
test_data_dirs(void)
{
    char **dirs = btd_desktop_data_dirs();
    gboolean has_flatpak = FALSE;
    for (char **d = dirs; *d; d++)
        has_flatpak |= g_str_equal(*d, "/var/lib/flatpak/exports/share");
    g_assert_true(has_flatpak);
    g_assert_cmpstr(dirs[0], ==, g_get_user_data_dir());
    g_strfreev(dirs);
}

int
main(int argc, char **argv)
{
    g_setenv("XDG_DATA_DIRS", "/nonexistent", TRUE); /* keep GIO defaults deterministic */
    g_test_init(&argc, &argv, NULL);
    tmpdir = g_dir_make_tmp("btd-desktop-XXXXXX", NULL);
    g_setenv("HOME", tmpdir, TRUE);
    /* the host may itself be a live system: defaults must not depend on it */
    btd_defaults_set_live_marker("/nonexistent/livefs-pkgs.txt");
    setup_fixtures();
    g_test_add_func("/desktop/index", test_index);
    g_test_add_func("/desktop/locale", test_locale);
    g_test_add_func("/desktop/exec-program", test_exec_program);
    g_test_add_func("/desktop/match", test_match);
    g_test_add_func("/desktop/defaults", test_defaults);
    g_test_add_func("/desktop/live-installer", test_live_installer);
    g_test_add_func("/desktop/data-dirs", test_data_dirs);
    int ret = g_test_run();
    char *argv_rm[] = { "rm", "-rf", tmpdir, NULL };
    g_spawn_sync(NULL, argv_rm, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free(tmpdir);
    return ret;
}
