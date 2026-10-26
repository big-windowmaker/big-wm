/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Context menus of tabs, items, running applications and the empty shelf.
 */
#include "btd-dock.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#include "btd-defaults.h"
#include "btd-launch.h"

typedef struct {
    BtdDock *dock;
    char *tab_id;    /* tab the menu refers to */
    int item;        /* item index in tab_id, or -1 */
    AppGroup *group; /* borrowed; valid while the menu is up */
    char *group_key;
} MenuCtx;

static void
menu_ctx_free(gpointer data)
{
    MenuCtx *m = data;
    g_free(m->tab_id);
    g_free(m->group_key);
    g_free(m);
}

static BtdTab *
ctx_tab(MenuCtx *m)
{
    return btd_model_find_tab(m->dock->model, m->tab_id, NULL);
}

static BtdItem *
ctx_item(MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    return tab && m->item >= 0 && (guint)m->item < tab->items->len ? tab->items->pdata[m->item] : NULL;
}

/* The group pointer may be stale after window changes: look it up again. */
static AppGroup *
ctx_group(MenuCtx *m)
{
    return m->group_key ? g_hash_table_lookup(m->dock->groups, m->group_key) : NULL;
}

static GtkWidget *
add_item(GtkWidget *menu, const char *label, GCallback cb, gpointer data)
{
    GtkWidget *mi = gtk_menu_item_new_with_mnemonic(label);
    if (cb)
        g_signal_connect_swapped(mi, "activate", cb, data);
    else
        gtk_widget_set_sensitive(mi, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    return mi;
}

static void
add_separator(GtkWidget *menu)
{
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

/* ---- item actions --------------------------------------------------------- */

static void
do_launch(MenuCtx *m)
{
    BtdItem *it = ctx_item(m);
    if (it)
        btd_dock_launch(m->dock, it, NULL, NULL);
    else if (ctx_group(m) && ctx_group(m)->desktop_id) {
        BtdItem tmp = { .type = BTD_ITEM_APP, .target = ctx_group(m)->desktop_id };
        btd_dock_launch(m->dock, &tmp, NULL, NULL);
    }
}

static void
do_action(GtkWidget *mi, MenuCtx *m)
{
    const char *action = g_object_get_data(G_OBJECT(mi), "action");
    BtdItem *it = ctx_item(m);
    BtdItem tmp = { .type = BTD_ITEM_APP };
    if (!it && ctx_group(m)) {
        tmp.target = ctx_group(m)->desktop_id;
        it = &tmp;
    }
    if (it && action)
        btd_dock_launch(m->dock, it, NULL, action);
}

static void
do_activate_window(GtkWidget *mi, MenuCtx *m)
{
    guint64 id = *(guint64 *)g_object_get_data(G_OBJECT(mi), "window-id");
    BtdWindow *w = g_hash_table_lookup(m->dock->tracker->windows, &id);
    if (w && m->dock->tracker->activate)
        m->dock->tracker->activate(m->dock->tracker, w, gtk_get_current_event_time());
}

static void
for_each_window(MenuCtx *m, void (*fn)(BtdWindowTracker *, BtdWindow *))
{
    AppGroup *g = ctx_group(m);
    if (!g)
        return;
    GPtrArray *copy = g_ptr_array_copy(g->windows, NULL, NULL);
    for (guint i = 0; i < copy->len; i++)
        fn(m->dock->tracker, copy->pdata[i]);
    g_ptr_array_unref(copy);
}

static void
restore_one(BtdWindowTracker *t, BtdWindow *w)
{
    if (w->minimized)
        t->activate(t, w, gtk_get_current_event_time());
}

static void do_minimize_all(MenuCtx *m) { for_each_window(m, m->dock->tracker->minimize); }
static void do_restore_windows(MenuCtx *m) { for_each_window(m, restore_one); }

static void
do_close_all(MenuCtx *m)
{
    AppGroup *g = ctx_group(m);
    if (!g)
        return;
    if (g->windows->len > 1) {
        char *msg = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE, "Close %u window?", "Close %u windows?",
                                                g->windows->len),
                                    g->windows->len);
        gboolean ok = btd_dock_ask(m->dock, msg, _("Applications may ask to save changes."), _("C_lose"));
        g_free(msg);
        if (!ok)
            return;
    }
    for_each_window(m, m->dock->tracker->close);
}

static void
do_remove(MenuCtx *m)
{
    /* Only the shortcut goes away; the file or application is untouched. */
    btd_model_remove_item(m->dock->model, m->tab_id, (guint)m->item);
}

static void
do_insert_separator(MenuCtx *m)
{
    btd_model_insert_item(m->dock->model, m->tab_id, btd_item_new(BTD_ITEM_SEPARATOR, NULL), m->item);
}

static void
do_append_separator(MenuCtx *m)
{
    btd_model_insert_item(m->dock->model, m->tab_id, btd_item_new(BTD_ITEM_SEPARATOR, NULL), -1);
}

static void
do_rename_item(MenuCtx *m)
{
    BtdItem *it = ctx_item(m);
    if (!it)
        return;
    char *current = btd_dock_item_name(m->dock, it);
    char *name = btd_dock_ask_text(m->dock, _("Rename"), _("Label (empty restores the original name):"), current);
    if (name && (it = ctx_item(m))) {
        char *clean = btd_sanitize_label(name);
        g_free(it->label);
        it->label = *clean ? clean : NULL;
        if (!it->label)
            g_free(clean);
        btd_model_freeze(m->dock->model);
        m->dock->model->pending = TRUE;
        btd_model_thaw(m->dock->model);
    }
    g_free(name);
    g_free(current);
}

static void
do_change_icon(MenuCtx *m)
{
    BtdDock *dock = m->dock;
    GtkWidget *d = gtk_file_chooser_dialog_new(_("Choose an Icon"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, _("_Cancel"),
                                               GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, _("Images"));
    gtk_file_filter_add_pixbuf_formats(f);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);
    if (g_file_test("/usr/share/icons", G_FILE_TEST_IS_DIR))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), "/usr/share/icons");
    dock->menu_open = TRUE;
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        BtdItem *it = ctx_item(m);
        if (it && path && btd_icon_name_is_valid(path)) {
            g_free(it->icon);
            it->icon = path;
            path = NULL;
            btd_icon_cache_clear(dock->icons);
            btd_model_freeze(dock->model);
            dock->model->pending = TRUE;
            btd_model_thaw(dock->model);
        }
        g_free(path);
    }
    dock->menu_open = FALSE;
    gtk_widget_destroy(d);
}

static void
do_reset_icon(MenuCtx *m)
{
    BtdItem *it = ctx_item(m);
    if (!it)
        return;
    g_clear_pointer(&it->icon, g_free);
    btd_icon_cache_clear(m->dock->icons);
    btd_model_freeze(m->dock->model);
    m->dock->model->pending = TRUE;
    btd_model_thaw(m->dock->model);
}

static void
do_open_folder(MenuCtx *m)
{
    BtdItem *it = ctx_item(m);
    if (!it)
        return;
    char *dir = g_path_get_dirname(it->target);
    BtdItem tmp = { .type = BTD_ITEM_FOLDER, .target = dir };
    btd_dock_launch(m->dock, &tmp, NULL, NULL);
    g_free(dir);
}

static void
do_move_copy(GtkWidget *mi, MenuCtx *m)
{
    const char *dest = g_object_get_data(G_OBJECT(mi), "tab-id");
    gboolean copy = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mi), "copy"));
    if (m->item >= 0) {
        if (!btd_model_move_item(m->dock->model, m->tab_id, (guint)m->item, dest, -1, copy))
            btd_dock_error(m->dock, _("Not added"), _("That tab already has this item."));
    } else if (ctx_group(m) && ctx_group(m)->desktop_id) {
        btd_model_insert_item(m->dock->model, dest, btd_item_new(BTD_ITEM_APP, ctx_group(m)->desktop_id), -1);
    }
}

static void
do_keep(MenuCtx *m)
{
    AppGroup *g = ctx_group(m);
    if (g && g->desktop_id)
        btd_model_insert_item(m->dock->model, m->tab_id, btd_item_new(BTD_ITEM_APP, g->desktop_id), -1);
}

/* ---- tab actions ---------------------------------------------------------- */

static void
do_new_tab(MenuCtx *m)
{
    char *name = btd_dock_ask_text(m->dock, _("New Tab"), _("Name of the new tab:"), "");
    if (name) {
        guint index = 0;
        btd_model_find_tab(m->dock->model, m->tab_id, &index);
        BtdTab *t = btd_model_add_tab(m->dock->model, name, (int)index + 1);
        if (t)
            btd_model_set_active_tab(m->dock->model, t->id);
        g_free(name);
    }
}

static void
do_rename_tab(MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    if (!tab)
        return;
    const char *hint = tab->builtin ? _("Name (empty restores the default name):") : _("Name:");
    char *name = btd_dock_ask_text(m->dock, _("Rename Tab"), hint, btd_dock_tab_label(tab));
    if (name) {
        if (!btd_model_rename_tab(m->dock->model, m->tab_id, name))
            btd_dock_error(m->dock, _("A tab needs a name"), NULL);
        g_free(name);
    }
}

static void
do_delete_tab(MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    if (!tab || tab->builtin)
        return;
    if (tab->items->len > 0) {
        char *msg = g_strdup_printf(_("Delete the tab “%s”?"), btd_dock_tab_label(tab));
        gboolean ok = btd_dock_ask(m->dock, msg,
                                   _("Its shortcuts are removed from the dock. Files and applications are not "
                                     "touched."),
                                   _("_Delete"));
        g_free(msg);
        if (!ok)
            return;
    }
    btd_model_remove_tab(m->dock->model, m->tab_id);
}

static void
do_move_tab(MenuCtx *m, int delta)
{
    guint index;
    if (btd_model_find_tab(m->dock->model, m->tab_id, &index) && (int)index + delta >= 0)
        btd_model_move_tab(m->dock->model, m->tab_id, (guint)((int)index + delta));
}
static void do_move_tab_left(MenuCtx *m) { do_move_tab(m, -1); }
static void do_move_tab_right(MenuCtx *m) { do_move_tab(m, 1); }

static void
do_clear_tab(MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    if (!tab || tab->items->len == 0)
        return;
    char *msg = g_strdup_printf(_("Remove all shortcuts from “%s”?"), btd_dock_tab_label(tab));
    gboolean ok = btd_dock_ask(m->dock, msg, _("Only the shortcuts are removed; files stay where they are."),
                               _("_Remove"));
    g_free(msg);
    if (ok) {
        btd_model_freeze(m->dock->model);
        while (tab->items->len)
            btd_model_remove_item(m->dock->model, tab->id, 0);
        btd_model_thaw(m->dock->model);
    }
}

static void
do_restore_tab(MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    if (!tab || !tab->builtin)
        return;
    char *msg = g_strdup_printf(_("Restore the default contents of “%s”?"), btd_dock_tab_label(tab));
    if (btd_dock_ask(m->dock, msg, _("The shortcuts you added to this tab will be replaced."), _("_Restore")))
        btd_defaults_fill_tab(m->dock->model, m->dock->index, m->tab_id);
    g_free(msg);
}

static void
do_restore_all(MenuCtx *m)
{
    if (btd_dock_ask(m->dock, _("Restore the default tabs?"),
                     _("Custom tabs and added shortcuts are removed from the dock (files are not touched). "
                       "Preferences are kept."),
                     _("_Restore")))
        btd_dock_restore_defaults(m->dock);
}

static void
on_add_app_response(GtkDialog *d, int response, MenuCtx *m)
{
    if (response == GTK_RESPONSE_OK) {
        GAppInfo *info = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(d));
        if (info) {
            const char *id = g_app_info_get_id(info);
            if (id)
                btd_model_insert_item(m->dock->model, m->tab_id, btd_item_new(BTD_ITEM_APP, id), -1);
            g_object_unref(info);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(d));
    menu_ctx_free(m);
}

static void
do_add_app(MenuCtx *m)
{
    GtkWidget *d = gtk_app_chooser_dialog_new_for_content_type(NULL, 0, "application/x-big-tabbed-dock-any");
    gtk_window_set_title(GTK_WINDOW(d), _("Add Application"));
    gtk_app_chooser_dialog_set_heading(GTK_APP_CHOOSER_DIALOG(d), _("Choose an application to add to the tab"));
    GtkWidget *w = gtk_app_chooser_dialog_get_widget(GTK_APP_CHOOSER_DIALOG(d));
    gtk_app_chooser_widget_set_show_all(GTK_APP_CHOOSER_WIDGET(w), TRUE);
    gtk_app_chooser_widget_set_show_fallback(GTK_APP_CHOOSER_WIDGET(w), FALSE);
    gtk_app_chooser_widget_set_show_other(GTK_APP_CHOOSER_WIDGET(w), TRUE);
    MenuCtx *copy = g_new0(MenuCtx, 1);
    copy->dock = m->dock;
    copy->tab_id = g_strdup(m->tab_id);
    copy->item = -1;
    g_signal_connect(d, "response", G_CALLBACK(on_add_app_response), copy);
    gtk_widget_show(d);
}

static void
add_paths(MenuCtx *m, GtkFileChooserAction action, const char *title)
{
    GtkWidget *d = gtk_file_chooser_dialog_new(title, NULL, action, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Add"),
                                               GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(d), TRUE);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(d), TRUE);
    m->dock->menu_open = TRUE;
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        GSList *files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(d));
        btd_model_freeze(m->dock->model);
        for (GSList *l = files; l; l = l->next) {
            BtdItem *it = btd_dock_item_from_path(m->dock, l->data);
            if (it)
                btd_model_insert_item(m->dock->model, m->tab_id, it, -1);
        }
        btd_model_thaw(m->dock->model);
        g_slist_free_full(files, g_free);
    }
    m->dock->menu_open = FALSE;
    gtk_widget_destroy(d);
}

static void do_add_files(MenuCtx *m) { add_paths(m, GTK_FILE_CHOOSER_ACTION_OPEN, _("Add Files")); }
static void do_add_folder(MenuCtx *m) { add_paths(m, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("Add Folders")); }
static void do_prefs(MenuCtx *m) { btd_dock_show_preferences(m->dock); }

/* ---- builders ------------------------------------------------------------- */

static void
add_tab_targets(GtkWidget *menu, MenuCtx *m, const char *label, gboolean copy)
{
    GtkWidget *mi = gtk_menu_item_new_with_mnemonic(label);
    GtkWidget *sub = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), sub);
    for (guint i = 0; i < btd_model_n_tabs(m->dock->model); i++) {
        BtdTab *t = btd_model_get_tab(m->dock->model, i);
        if (g_strcmp0(t->id, m->tab_id) == 0 && m->item >= 0)
            continue;
        GtkWidget *ti = gtk_menu_item_new_with_label(btd_dock_tab_label(t));
        g_object_set_data_full(G_OBJECT(ti), "tab-id", g_strdup(t->id), g_free);
        g_object_set_data(G_OBJECT(ti), "copy", GINT_TO_POINTER(copy));
        g_signal_connect(ti, "activate", G_CALLBACK(do_move_copy), m);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), ti);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
}

static void
add_window_section(GtkWidget *menu, MenuCtx *m)
{
    AppGroup *g = ctx_group(m);
    BtdWindowTracker *t = m->dock->tracker;
    if (!g || g->windows->len == 0)
        return;
    gboolean can_activate = t->caps & BTD_WIN_CAP_ACTIVATE;
    for (guint i = 0; i < g->windows->len && i < 12; i++) {
        BtdWindow *w = g->windows->pdata[i];
        char *title = g_strdup_printf("%s%s", w->minimized ? "· " : "", w->title && *w->title ? w->title : "—");
        if (g_utf8_strlen(title, -1) > 60) {
            char *end = g_utf8_offset_to_pointer(title, 58);
            *end = '\0';
            char *t2 = g_strconcat(title, "…", NULL);
            g_free(title);
            title = t2;
        }
        GtkWidget *mi = gtk_menu_item_new_with_label(title);
        g_free(title);
        guint64 *id = g_new(guint64, 1);
        *id = w->id;
        g_object_set_data_full(G_OBJECT(mi), "window-id", id, g_free);
        if (can_activate)
            g_signal_connect(mi, "activate", G_CALLBACK(do_activate_window), m);
        else
            gtk_widget_set_sensitive(mi, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
    if (!can_activate) {
        GtkWidget *mi = add_item(menu, _("(this session cannot switch to a given window)"), NULL, NULL);
        (void)mi;
    }
    add_separator(menu);
}

static void
add_desktop_actions(GtkWidget *menu, MenuCtx *m, const char *desktop_id, const char *path)
{
    GDesktopAppInfo *info = path ? g_desktop_app_info_new_from_filename(path)
                                 : desktop_id ? g_desktop_app_info_new(desktop_id) : NULL;
    if (!info)
        return;
    const char *const *actions = g_desktop_app_info_list_actions(info);
    gboolean any = FALSE;
    for (int i = 0; actions && actions[i] && i < 12; i++) {
        char *name = g_desktop_app_info_get_action_name(info, actions[i]);
        GtkWidget *mi = gtk_menu_item_new_with_label(name ? name : actions[i]);
        g_free(name);
        g_object_set_data_full(G_OBJECT(mi), "action", g_strdup(actions[i]), g_free);
        g_signal_connect(mi, "activate", G_CALLBACK(do_action), m);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        any = TRUE;
    }
    if (any)
        add_separator(menu);
    g_object_unref(info);
}

static void
add_window_controls(GtkWidget *menu, MenuCtx *m)
{
    AppGroup *g = ctx_group(m);
    guint caps = m->dock->tracker->caps;
    if (!g || g->windows->len == 0)
        return;
    if (caps & BTD_WIN_CAP_MINIMIZE) {
        add_item(menu, _("_Minimize All"), G_CALLBACK(do_minimize_all), m);
        add_item(menu, _("Res_tore All"), G_CALLBACK(do_restore_windows), m);
    }
    if (caps & BTD_WIN_CAP_CLOSE)
        add_item(menu, _("_Close All Windows"), G_CALLBACK(do_close_all), m);
    if (caps & (BTD_WIN_CAP_MINIMIZE | BTD_WIN_CAP_CLOSE))
        add_separator(menu);
}

static void
add_tab_section(GtkWidget *menu, MenuCtx *m)
{
    BtdTab *tab = ctx_tab(m);
    add_item(menu, _("_New Tab…"), G_CALLBACK(do_new_tab), m);
    add_item(menu, _("Re_name Tab…"), G_CALLBACK(do_rename_tab), m);
    add_item(menu, _("Move Tab _Left"), G_CALLBACK(do_move_tab_left), m);
    add_item(menu, _("Move Tab _Right"), G_CALLBACK(do_move_tab_right), m);
    if (tab && tab->builtin)
        add_item(menu, _("Restore _Default Contents…"), G_CALLBACK(do_restore_tab), m);
    add_item(menu, _("Remove All _Shortcuts…"), tab && tab->items->len ? G_CALLBACK(do_clear_tab) : NULL, m);
    add_item(menu, _("_Delete Tab…"), tab && !tab->builtin ? G_CALLBACK(do_delete_tab) : NULL, m);
}

static void
on_menu_deactivate(GtkMenuShell *menu, BtdDock *dock)
{
    dock->menu_open = FALSE;
    dock->press_slot = -1;
    gtk_widget_queue_draw(dock->area);
}

void
btd_dock_popup_menu(BtdDock *dock, int slot_index, const GdkEvent *event)
{
    Slot *s = btd_dock_slot(dock, slot_index);
    BtdTab *active = btd_model_get_active_tab(dock->model);
    MenuCtx *m = g_new0(MenuCtx, 1);
    m->dock = dock;
    m->item = -1;
    m->tab_id = g_strdup(active ? active->id : NULL);
    GtkWidget *menu = gtk_menu_new();
    g_object_set_data_full(G_OBJECT(menu), "ctx", m, menu_ctx_free);

    if (s && s->kind == SLOT_TAB) {
        BtdTab *tab = btd_model_get_tab(dock->model, (guint)s->index);
        g_free(m->tab_id);
        m->tab_id = g_strdup(tab->id);
        add_tab_section(menu, m);
    } else if (s && s->kind == SLOT_ITEM) {
        BtdItem *it = active->items->pdata[s->index];
        m->item = s->index;
        if (it->type == BTD_ITEM_SEPARATOR) {
            add_item(menu, _("_Remove Separator"), G_CALLBACK(do_remove), m);
        } else {
            char *name = btd_dock_item_name(dock, it);
            add_item(menu, name, NULL, NULL);
            g_free(name);
            add_separator(menu);
            AppGroup *g = btd_dock_item_group(dock, it);
            if (g)
                m->group_key = g_strdup(g->key);
            add_window_section(menu, m);
            if (it->type == BTD_ITEM_APP) {
                add_item(menu, g ? _("New _Window") : _("_Launch"), G_CALLBACK(do_launch), m);
                add_separator(menu);
                add_desktop_actions(menu, m, g_path_is_absolute(it->target) ? NULL : it->target,
                                    g_path_is_absolute(it->target) ? it->target : NULL);
            } else {
                add_item(menu, _("_Open"), G_CALLBACK(do_launch), m);
                if (it->type == BTD_ITEM_FILE || it->type == BTD_ITEM_APPIMAGE)
                    add_item(menu, _("Open Containing _Folder"), G_CALLBACK(do_open_folder), m);
                add_separator(menu);
            }
            add_window_controls(menu, m);
            add_tab_targets(menu, m, _("_Move to Tab"), FALSE);
            add_tab_targets(menu, m, _("Cop_y to Tab"), TRUE);
            add_item(menu, _("Re_name…"), G_CALLBACK(do_rename_item), m);
            add_item(menu, _("Change _Icon…"), G_CALLBACK(do_change_icon), m);
            if (it->icon)
                add_item(menu, _("Reset Ico_n"), G_CALLBACK(do_reset_icon), m);
            add_item(menu, _("Insert _Separator Before"), G_CALLBACK(do_insert_separator), m);
            add_separator(menu);
            add_item(menu, _("_Remove from Dock"), G_CALLBACK(do_remove), m);
        }
    } else if (s && s->kind == SLOT_RUNNING) {
        AppGroup *g = dock->running->pdata[s->index];
        m->group_key = g_strdup(g->key);
        const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, g->desktop_id);
        add_item(menu, e && e->name ? e->name : g->key, NULL, NULL);
        add_separator(menu);
        add_window_section(menu, m);
        if (g->desktop_id) {
            add_item(menu, _("New _Window"), G_CALLBACK(do_launch), m);
            add_desktop_actions(menu, m, g->desktop_id, NULL);
        }
        add_window_controls(menu, m);
        if (g->desktop_id) {
            char *keep = g_strdup_printf(_("_Keep in “%s”"), btd_dock_tab_label(active));
            add_item(menu, keep, G_CALLBACK(do_keep), m);
            g_free(keep);
            add_tab_targets(menu, m, _("Add to _Tab"), FALSE);
        }
    } else {
        add_item(menu, _("Add _Application…"), G_CALLBACK(do_add_app), m);
        add_item(menu, _("Add _Files…"), G_CALLBACK(do_add_files), m);
        add_item(menu, _("Add F_olders…"), G_CALLBACK(do_add_folder), m);
        add_item(menu, _("Add _Separator"), G_CALLBACK(do_append_separator), m);
        add_separator(menu);
        add_tab_section(menu, m);
    }
    add_separator(menu);
    add_item(menu, _("Restore Default _Tabs…"), G_CALLBACK(do_restore_all), m);
    add_item(menu, _("_Preferences…"), G_CALLBACK(do_prefs), m);

    dock->menu_open = TRUE;
    g_signal_connect(menu, "deactivate", G_CALLBACK(on_menu_deactivate), dock);
    /* The previous menu (and its context) is released only now: its handlers
     * may have run nested dialogs that still used the context. */
    if (dock->menu)
        gtk_widget_destroy(dock->menu);
    dock->menu = menu;
    gtk_widget_show_all(menu);
    gtk_menu_attach_to_widget(GTK_MENU(menu), dock->area, NULL);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
}
