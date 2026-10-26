/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-model.h"

static int changes;
static void on_changed(BtdModel *m, gpointer d) { changes++; }

static BtdModel *
new_model(void)
{
    BtdModel *m = btd_model_new();
    btd_model_reset_builtin_tabs(m);
    btd_model_set_changed_func(m, on_changed, NULL);
    changes = 0;
    return m;
}

static void
test_builtin(void)
{
    BtdModel *m = new_model();
    g_assert_cmpuint(btd_model_n_tabs(m), ==, 4);
    g_assert_cmpstr(btd_model_get_tab(m, 0)->id, ==, "general");
    g_assert_cmpstr(btd_model_get_tab(m, 3)->id, ==, "temporary");
    g_assert_cmpstr(btd_model_get_active_tab(m)->id, ==, "general");
    g_assert_true(btd_model_get_tab(m, 1)->builtin);
    g_assert_cmpstr(btd_tab_default_label("documents"), ==, "Documents");
    /* built-ins cannot be removed */
    g_assert_false(btd_model_remove_tab(m, "general"));
    g_assert_cmpuint(btd_model_n_tabs(m), ==, 4);
    btd_model_free(m);
}

static void
test_custom_tabs(void)
{
    BtdModel *m = new_model();
    BtdTab *t = btd_model_add_tab(m, "  Games\n ", -1);
    g_assert_nonnull(t);
    g_assert_cmpstr(t->id, ==, "custom-1");
    g_assert_cmpstr(t->label, ==, "Games");
    g_assert_false(t->builtin);
    BtdTab *t2 = btd_model_add_tab(m, "Media", 0);
    g_assert_cmpstr(t2->id, ==, "custom-2");
    g_assert_cmpstr(btd_model_get_tab(m, 0)->id, ==, "custom-2");
    g_assert_null(btd_model_add_tab(m, "   ", -1));

    g_assert_true(btd_model_rename_tab(m, "custom-1", "Jogos"));
    g_assert_cmpstr(btd_model_find_tab(m, "custom-1", NULL)->label, ==, "Jogos");
    g_assert_false(btd_model_rename_tab(m, "custom-1", ""));      /* custom needs a name */
    g_assert_true(btd_model_rename_tab(m, "general", "Main"));
    g_assert_true(btd_model_rename_tab(m, "general", ""));       /* back to translated default */
    g_assert_null(btd_model_find_tab(m, "general", NULL)->label);

    g_assert_true(btd_model_move_tab(m, "custom-2", 5));
    g_assert_cmpstr(btd_model_get_tab(m, 5)->id, ==, "custom-2");
    g_assert_true(btd_model_move_tab(m, "temporary", 0));
    g_assert_cmpstr(btd_model_get_tab(m, 0)->id, ==, "temporary");

    g_assert_true(btd_model_set_active_tab(m, "custom-1"));
    g_assert_true(btd_model_remove_tab(m, "custom-1"));
    g_assert_nonnull(btd_model_get_active_tab(m));
    g_assert_null(btd_model_find_tab(m, "custom-1", NULL));
    g_assert_cmpint(changes, >, 0);

    /* more than four tabs are supported */
    for (int i = 0; i < 20; i++)
        g_assert_nonnull(btd_model_add_tab(m, "x", -1));
    g_assert_cmpuint(btd_model_n_tabs(m), ==, 25);
    btd_model_free(m);
}

static void
test_cycle(void)
{
    BtdModel *m = new_model();
    btd_model_cycle_active_tab(m, 1);
    g_assert_cmpstr(btd_model_get_active_tab(m)->id, ==, "developer");
    btd_model_cycle_active_tab(m, -2);
    g_assert_cmpstr(btd_model_get_active_tab(m)->id, ==, "temporary");
    btd_model_cycle_active_tab(m, 5);
    g_assert_cmpstr(btd_model_get_active_tab(m)->id, ==, "general");
    g_assert_false(btd_model_set_active_tab(m, "nope"));
    btd_model_free(m);
}

static void
test_items(void)
{
    BtdModel *m = new_model();
    g_assert_true(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_APP, "a.desktop"), -1));
    g_assert_true(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_APP, "b.desktop"), -1));
    g_assert_true(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_FOLDER, "/home"), 0));
    /* duplicates refused, separators allowed several times */
    g_assert_false(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_APP, "a.desktop"), -1));
    g_assert_true(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_SEPARATOR, NULL), -1));
    g_assert_true(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_SEPARATOR, NULL), -1));
    g_assert_false(btd_model_insert_item(m, "general", btd_item_new(BTD_ITEM_APP, ""), -1));
    g_assert_false(btd_model_insert_item(m, "missing", btd_item_new(BTD_ITEM_APP, "x.desktop"), -1));
    BtdTab *g = btd_model_find_tab(m, "general", NULL);
    g_assert_cmpuint(g->items->len, ==, 5);
    g_assert_cmpstr(((BtdItem *)g->items->pdata[0])->target, ==, "/home");

    /* reorder within the tab: move /home (0) to after b (position 3) */
    g_assert_true(btd_model_move_item(m, "general", 0, "general", 3, FALSE));
    g_assert_cmpstr(((BtdItem *)g->items->pdata[0])->target, ==, "a.desktop");
    g_assert_cmpstr(((BtdItem *)g->items->pdata[2])->target, ==, "/home");

    /* move to another tab */
    g_assert_true(btd_model_move_item(m, "general", 0, "developer", -1, FALSE));
    BtdTab *d = btd_model_find_tab(m, "developer", NULL);
    g_assert_cmpuint(d->items->len, ==, 1);
    g_assert_cmpuint(g->items->len, ==, 4);
    /* copy keeps the source */
    g_assert_true(btd_model_move_item(m, "general", 0, "developer", 0, TRUE));
    g_assert_cmpuint(g->items->len, ==, 4);
    g_assert_cmpuint(d->items->len, ==, 2);
    /* copying again would duplicate: refused */
    g_assert_false(btd_model_move_item(m, "general", 0, "developer", 0, TRUE));
    g_assert_false(btd_model_move_item(m, "general", 0, "general", 0, TRUE));

    g_assert_true(btd_model_remove_item(m, "developer", 0));
    g_assert_false(btd_model_remove_item(m, "developer", 7));
    g_assert_cmpuint(d->items->len, ==, 1);
    btd_model_free(m);
}

static void
test_sanitize(void)
{
    char *s = btd_sanitize_label("a\tb\x01" "c");
    g_assert_cmpstr(s, ==, "a b c");
    g_free(s);
    s = btd_sanitize_label("\xff\xfe" "ok");
    g_assert_true(g_utf8_validate(s, -1, NULL));
    g_free(s);
    char *longs = g_strnfill(500, 'x');
    s = btd_sanitize_label(longs);
    g_assert_cmpint(g_utf8_strlen(s, -1), ==, BTD_MAX_LABEL);
    g_free(s);
    g_free(longs);
    g_assert_true(btd_tab_id_is_valid("custom-12"));
    g_assert_false(btd_tab_id_is_valid("Bad Id"));
    g_assert_false(btd_tab_id_is_valid(""));
}

static void
test_freeze(void)
{
    BtdModel *m = new_model();
    btd_model_freeze(m);
    btd_model_add_tab(m, "a", -1);
    btd_model_add_tab(m, "b", -1);
    g_assert_cmpint(changes, ==, 0);
    btd_model_thaw(m);
    g_assert_cmpint(changes, ==, 1);
    btd_model_free(m);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/model/builtin", test_builtin);
    g_test_add_func("/model/custom-tabs", test_custom_tabs);
    g_test_add_func("/model/cycle", test_cycle);
    g_test_add_func("/model/items", test_items);
    g_test_add_func("/model/sanitize", test_sanitize);
    g_test_add_func("/model/freeze", test_freeze);
    return g_test_run();
}
