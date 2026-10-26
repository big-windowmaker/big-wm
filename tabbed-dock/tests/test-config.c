/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-config.h"

#include <glib/gstdio.h>
#include <sys/stat.h>

static char *tmpdir;

static BtdModel *
sample_model(void)
{
    BtdModel *m = btd_model_new();
    btd_model_reset_builtin_tabs(m);
    btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_APP, "org.gnome.Calculator.desktop"), -1);
    BtdItem *f = btd_item_new(BTD_ITEM_FOLDER, "/home/user/Projetos com espaço");
    f->label = g_strdup("Projetos");
    f->icon = g_strdup("folder-documents");
    btd_model_insert_item(m, "general", f, -1);
    btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_SEPARATOR, NULL), -1);
    btd_model_insert_item(m, "temporary", btd_item_new(BTD_ITEM_FILE, "/home/user/a;b=c[d].txt"), -1);
    btd_model_add_tab(m, "Jogos & mídia", 1);
    btd_model_rename_tab(m, "developer", "Dev");
    btd_model_set_active_tab(m, "temporary");
    return m;
}

static void
test_roundtrip(void)
{
    BtdModel *m = sample_model();
    BtdPrefs p;
    btd_prefs_init_defaults(&p);
    p.edge = BTD_EDGE_TOP;
    p.align = BTD_ALIGN_RIGHT;
    p.icon_size = 64;
    p.autohide = TRUE;
    p.style = BTD_STYLE_MODERN;
    g_free(p.monitor);
    p.monitor = g_strdup("HDMI-1");
    gsize len;
    char *data = btd_config_to_data(m, &p, &len);

    BtdModel *m2 = btd_model_new();
    BtdPrefs p2;
    btd_prefs_init_defaults(&p2);
    g_assert_true(btd_config_from_data(data, (gssize)len, m2, &p2, NULL));
    g_assert_cmpint(p2.edge, ==, BTD_EDGE_TOP);
    g_assert_cmpint(p2.align, ==, BTD_ALIGN_RIGHT);
    g_assert_cmpint(p2.icon_size, ==, 64);
    g_assert_true(p2.autohide);
    g_assert_cmpint(p2.style, ==, BTD_STYLE_MODERN);
    g_assert_cmpstr(p2.monitor, ==, "HDMI-1");
    g_assert_cmpuint(btd_model_n_tabs(m2), ==, 5);
    g_assert_cmpstr(btd_model_get_tab(m2, 1)->id, ==, "custom-1");
    g_assert_cmpstr(btd_model_get_tab(m2, 1)->label, ==, "Jogos & mídia");
    g_assert_cmpstr(btd_model_find_tab(m2, "developer", NULL)->label, ==, "Dev");
    g_assert_cmpstr(btd_model_get_active_tab(m2)->id, ==, "temporary");
    BtdTab *g = btd_model_find_tab(m2, "general", NULL);
    g_assert_cmpuint(g->items->len, ==, 3);
    BtdItem *f = g->items->pdata[1];
    g_assert_cmpint(f->type, ==, BTD_ITEM_FOLDER);
    g_assert_cmpstr(f->target, ==, "/home/user/Projetos com espaço");
    g_assert_cmpstr(f->label, ==, "Projetos");
    g_assert_cmpstr(f->icon, ==, "folder-documents");
    g_assert_cmpint(((BtdItem *)g->items->pdata[2])->type, ==, BTD_ITEM_SEPARATOR);
    BtdItem *t = btd_model_find_tab(m2, "temporary", NULL)->items->pdata[0];
    g_assert_cmpstr(t->target, ==, "/home/user/a;b=c[d].txt");

    /* serialising again yields identical data (stable format) */
    gsize len2;
    char *data2 = btd_config_to_data(m2, &p2, &len2);
    g_assert_cmpstr(data, ==, data2);
    g_free(data);
    g_free(data2);
    btd_prefs_clear(&p);
    btd_prefs_clear(&p2);
    btd_model_free(m);
    btd_model_free(m2);
}

static void
test_invalid_values(void)
{
    const char *data = "[Dock]\nVersion=1\nPosition=sideways\nIconSize=9999\nAutoHide=maybe\n"
                       "Monitor=\nTabOrder=general;BAD ID;custom-9;custom-1;\nActiveTab=nothing\n"
                       "[Tab general]\nBuiltin=true\n"
                       "[Tab custom-1]\nBuiltin=true\nLabel=Mine\n"
                       "[Tab custom-9]\nLabel=\n"
                       "[Item general 000]\nType=app\nTarget=../../etc/passwd\n"
                       "[Item general 001]\nType=bogus\nTarget=x.desktop\n"
                       "[Item general 002]\nType=file\nTarget=relative/path\n"
                       "[Item general 003]\nType=app\nTarget=good.desktop\nIcon=$(rm -rf ~)\n"
                       "[Item general 004]\nType=app\nTarget=good.desktop\n"
                       "[Item general 010]\nType=folder\nTarget=/tmp\n"
                       "[Item general abc]\nType=folder\nTarget=/var\n";
    BtdModel *m = btd_model_new();
    BtdPrefs p;
    btd_prefs_init_defaults(&p);
    g_assert_true(btd_config_from_data(data, -1, m, &p, NULL));
    g_assert_cmpint(p.edge, ==, BTD_EDGE_BOTTOM);
    g_assert_cmpint(p.icon_size, ==, 48);
    g_assert_false(p.autohide);
    g_assert_cmpstr(p.monitor, ==, "primary");
    /* custom-1 is not builtin even if the file says so; custom-9 without label dropped */
    BtdTab *c1 = btd_model_find_tab(m, "custom-1", NULL);
    g_assert_nonnull(c1);
    g_assert_false(c1->builtin);
    g_assert_null(btd_model_find_tab(m, "custom-9", NULL));
    /* all four built-ins exist even if missing from file */
    g_assert_nonnull(btd_model_find_tab(m, "temporary", NULL));
    g_assert_cmpstr(btd_model_get_active_tab(m)->id, ==, "general");
    BtdTab *g = btd_model_find_tab(m, "general", NULL);
    g_assert_cmpuint(g->items->len, ==, 2);
    g_assert_cmpstr(((BtdItem *)g->items->pdata[0])->target, ==, "good.desktop");
    g_assert_cmpstr(((BtdItem *)g->items->pdata[1])->target, ==, "/tmp");
    btd_prefs_clear(&p);
    btd_model_free(m);
}

static void
test_not_a_dock_file(void)
{
    BtdModel *m = btd_model_new();
    btd_model_reset_builtin_tabs(m);
    BtdPrefs p;
    btd_prefs_init_defaults(&p);
    GError *err = NULL;
    g_assert_false(btd_config_from_data("garbage\x01\x02 not a keyfile [[[", -1, m, &p, &err));
    g_assert_nonnull(err);
    g_clear_error(&err);
    g_assert_false(btd_config_from_data("[Other]\nx=1\n", -1, m, &p, &err));
    g_clear_error(&err);
    g_assert_cmpuint(btd_model_n_tabs(m), ==, 4); /* untouched */
    btd_prefs_clear(&p);
    btd_model_free(m);
}

static void
test_save_load_recover(void)
{
    char *path = g_build_filename(tmpdir, "cfg", "dock.conf", NULL);
    char *bak = g_strconcat(path, ".bak", NULL);
    BtdModel *m = sample_model();
    BtdPrefs p;
    btd_prefs_init_defaults(&p);

    BtdModel *m2 = btd_model_new();
    BtdPrefs p2;
    btd_prefs_init_defaults(&p2);
    g_assert_cmpint(btd_config_load(path, m2, &p2, NULL), ==, BTD_LOAD_MISSING);

    g_assert_true(btd_config_save(path, m, &p, NULL));
    GStatBuf st;
    g_assert_cmpint(g_stat(path, &st), ==, 0);
    g_assert_cmpint(st.st_mode & 0777, ==, 0600);
    g_assert_false(g_file_test(bak, G_FILE_TEST_EXISTS)); /* nothing to back up yet */

    /* second save with a change rotates the previous good file to .bak */
    p.icon_size = 32;
    g_assert_true(btd_config_save(path, m, &p, NULL));
    g_assert_true(g_file_test(bak, G_FILE_TEST_EXISTS));

    g_assert_cmpint(btd_config_load(path, m2, &p2, NULL), ==, BTD_LOAD_OK);
    g_assert_cmpint(p2.icon_size, ==, 32);

    /* corrupt the main file: it is kept aside and the backup is used */
    g_assert_true(g_file_set_contents(path, "\x00\x01 broken [[[", 14, NULL));
    GError *err = NULL;
    g_assert_cmpint(btd_config_load(path, m2, &p2, &err), ==, BTD_LOAD_FROM_BACKUP);
    g_clear_error(&err);
    g_assert_cmpint(p2.icon_size, ==, 48);
    g_assert_cmpuint(btd_model_n_tabs(m2), ==, 5);
    g_assert_false(g_file_test(path, G_FILE_TEST_EXISTS));
    char *dir = g_path_get_dirname(path);
    GDir *d = g_dir_open(dir, 0, NULL);
    const char *name;
    gboolean found_broken = FALSE;
    while ((name = g_dir_read_name(d)))
        found_broken = found_broken || g_str_has_prefix(name, "dock.conf.broken-");
    g_dir_close(d);
    g_assert_true(found_broken);

    /* both unusable: defaults signalled, nothing deleted */
    g_assert_true(g_file_set_contents(path, "junk", -1, NULL));
    g_assert_true(g_file_set_contents(bak, "junk", -1, NULL));
    g_assert_cmpint(btd_config_load(path, m2, &p2, &err), ==, BTD_LOAD_CORRUPT_DEFAULTS);
    g_clear_error(&err);
    g_assert_true(g_file_test(bak, G_FILE_TEST_EXISTS));

    /* a save over a corrupt file does not overwrite the backup with junk */
    g_assert_true(g_file_set_contents(bak, "keep-me", -1, NULL));
    g_assert_true(g_file_set_contents(path, "junk", -1, NULL));
    g_assert_true(btd_config_save(path, m, &p, NULL));
    char *content = NULL;
    g_file_get_contents(bak, &content, NULL, NULL);
    g_assert_cmpstr(content, ==, "keep-me");
    g_free(content);

    g_free(dir);
    g_free(path);
    g_free(bak);
    btd_prefs_clear(&p);
    btd_prefs_clear(&p2);
    btd_model_free(m);
    btd_model_free(m2);
}

static void
test_validation(void)
{
    BtdItem it = { .type = BTD_ITEM_APP, .target = "firefox.desktop" };
    g_assert_true(btd_item_is_valid(&it));
    it.target = "/usr/share/applications/firefox.desktop";
    g_assert_true(btd_item_is_valid(&it));
    it.target = "/etc/passwd";
    g_assert_false(btd_item_is_valid(&it));
    it.target = "a b.desktop";
    g_assert_false(btd_item_is_valid(&it));
    it.type = BTD_ITEM_APPIMAGE;
    it.target = "/home/u/App.AppImage";
    g_assert_true(btd_item_is_valid(&it));
    it.target = "App.AppImage";
    g_assert_false(btd_item_is_valid(&it));
    g_assert_true(btd_icon_name_is_valid("utilities-terminal"));
    g_assert_true(btd_icon_name_is_valid("/usr/share/pixmaps/x.png"));
    g_assert_false(btd_icon_name_is_valid("bad;name"));
    g_assert_false(btd_icon_name_is_valid("/a/../b.png"));
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    tmpdir = g_dir_make_tmp("btd-config-XXXXXX", NULL);
    g_test_add_func("/config/roundtrip", test_roundtrip);
    g_test_add_func("/config/invalid-values", test_invalid_values);
    g_test_add_func("/config/not-a-dock-file", test_not_a_dock_file);
    g_test_add_func("/config/save-load-recover", test_save_load_recover);
    g_test_add_func("/config/validation", test_validation);
    int ret = g_test_run();
    char *argv_rm[] = { "rm", "-rf", tmpdir, NULL };
    g_spawn_sync(NULL, argv_rm, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free(tmpdir);
    return ret;
}
