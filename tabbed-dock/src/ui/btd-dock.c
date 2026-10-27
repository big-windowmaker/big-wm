/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — the dock window: layout, drawing, input, positioning.
 */
#include "btd-dock.h"

#include <glib/gi18n.h>
#include <math.h>
#include <string.h>

#include "btd-defaults.h"
#include "btd-launch.h"

#include <gio/gdesktopappinfo.h>

#ifdef BTD_HAVE_X11
#include <gdk/gdkx.h>
#endif
#ifdef BTD_HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#endif

#define SEP_W 14
#define PAD 6
#define HIDDEN_H 4
#define HIDE_DELAY_MS 700
#define SAVE_DELAY_MS 500
#define LAUNCH_FEEDBACK_US (2 * G_USEC_PER_SEC)
#define TAB_ANIM_US (140 * 1000)

static void dock_queue_draw(BtdDock *dock);
static void position_window(BtdDock *dock);
static void update_groups(BtdDock *dock);

/* ---- helpers ------------------------------------------------------------- */

const char *
btd_dock_tab_label(BtdTab *tab)
{
    if (tab->label && *tab->label)
        return tab->label;
    const char *def = btd_tab_default_label(tab->id);
    return def ? _(def) : tab->id;
}

const char *
btd_dock_item_desktop_id(BtdDock *dock, const BtdItem *item)
{
    if (item->type != BTD_ITEM_APP || !item->target)
        return NULL;
    if (!g_path_is_absolute(item->target))
        return item->target;
    const BtdDesktopEntry *e = btd_desktop_index_lookup_path(dock->index, item->target);
    return e ? e->id : item->target; /* a .desktop outside the data dirs keys by path */
}

char *
btd_dock_item_name(BtdDock *dock, const BtdItem *item)
{
    if (item->label && *item->label)
        return g_strdup(item->label);
    switch (item->type) {
    case BTD_ITEM_APP: {
        const char *id = btd_dock_item_desktop_id(dock, item);
        const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, id);
        if (e && e->name)
            return g_strdup(e->name);
        if (g_path_is_absolute(item->target)) {
            BtdDesktopEntry *tmp = btd_desktop_entry_load(item->target, NULL);
            char *name = g_strdup(tmp && tmp->name ? tmp->name : item->target);
            btd_desktop_entry_free(tmp);
            return name;
        }
        return g_strdup(item->target);
    }
    case BTD_ITEM_FOLDER:
        if (g_strcmp0(item->target, g_get_home_dir()) == 0)
            return g_strdup(_("Home"));
        /* fall through */
    case BTD_ITEM_FILE:
        return g_filename_display_basename(item->target);
    case BTD_ITEM_APPIMAGE: {
        char *base = g_filename_display_basename(item->target);
        char *dot = g_strrstr(base, ".");
        if (dot && g_ascii_strcasecmp(dot, ".AppImage") == 0)
            *dot = '\0';
        return base;
    }
    case BTD_ITEM_SEPARATOR:
        return g_strdup(_("Separator"));
    }
    return g_strdup("?");
}

AppGroup *
btd_dock_item_group(BtdDock *dock, const BtdItem *item)
{
    const char *id = btd_dock_item_desktop_id(dock, item);
    return id ? g_hash_table_lookup(dock->groups, id) : NULL;
}

static int
scale_factor(BtdDock *dock)
{
    return gtk_widget_get_scale_factor(dock->window);
}

/* ---- persistence ---------------------------------------------------------- */

static gboolean
save_timeout(gpointer data)
{
    BtdDock *dock = data;
    dock->save_id = 0;
    btd_dock_save_now(dock);
    return G_SOURCE_REMOVE;
}

void
btd_dock_save_now(BtdDock *dock)
{
    if (dock->save_id) {
        g_source_remove(dock->save_id);
        dock->save_id = 0;
    }
    GError *err = NULL;
    if (!btd_config_save(dock->config_path, dock->model, &dock->prefs, &err)) {
        g_warning("could not save %s: %s", dock->config_path, err->message);
        g_error_free(err);
    }
}

void
btd_dock_queue_save(BtdDock *dock)
{
    if (dock->save_id)
        g_source_remove(dock->save_id);
    dock->save_id = g_timeout_add(SAVE_DELAY_MS, save_timeout, dock);
}

static void
on_model_changed(BtdModel *model, gpointer data)
{
    BtdDock *dock = data;
    static char *last_active;
    BtdTab *active = btd_model_get_active_tab(model);
    if (active && g_strcmp0(last_active, active->id) != 0) {
        g_free(last_active);
        last_active = g_strdup(active->id);
        if (dock->prefs.animations && dock->area && gtk_widget_get_mapped(dock->area)) {
            dock->tab_anim = 0;
            dock->tab_anim_start = g_get_monotonic_time();
        }
    }
    update_groups(dock); /* running section depends on the active tab */
    btd_dock_relayout(dock);
    btd_dock_queue_save(dock);
}

static void
load_config(BtdDock *dock)
{
    GError *err = NULL;
    btd_model_freeze(dock->model);
    BtdLoadResult r = btd_config_load(dock->config_path, dock->model, &dock->prefs, &err);
    switch (r) {
    case BTD_LOAD_OK:
        break;
    case BTD_LOAD_MISSING:
        btd_defaults_apply(dock->model, dock->index);
        break;
    case BTD_LOAD_FROM_BACKUP:
        g_message("configuration restored from %s.bak", dock->config_path);
        dock->config_broken_notice = err != NULL;
        break;
    case BTD_LOAD_CORRUPT_DEFAULTS:
        btd_prefs_clear(&dock->prefs);
        btd_prefs_init_defaults(&dock->prefs);
        btd_defaults_apply(dock->model, dock->index);
        dock->config_broken_notice = TRUE;
        break;
    }
    g_clear_error(&err);
    btd_model_thaw(dock->model);
}

void
btd_dock_reload(BtdDock *dock)
{
    if (dock->save_id) {
        g_source_remove(dock->save_id);
        dock->save_id = 0;
    }
    load_config(dock);
    btd_dock_apply_prefs(dock);
}

void
btd_dock_restore_defaults(BtdDock *dock)
{
    btd_defaults_apply(dock->model, dock->index);
}

/* ---- window tracking ------------------------------------------------------ */

static void
group_free(AppGroup *g)
{
    g_free(g->key);
    g_free(g->desktop_id);
    g_ptr_array_unref(g->windows);
    g_free(g);
}

static gint
group_by_serial(gconstpointer a, gconstpointer b)
{
    const AppGroup *ga = *(AppGroup *const *)a, *gb = *(AppGroup *const *)b;
    guint64 sa = ((BtdWindow *)ga->windows->pdata[0])->serial, sb = ((BtdWindow *)gb->windows->pdata[0])->serial;
    return sa < sb ? -1 : sa > sb;
}

static void
update_groups(BtdDock *dock)
{
    g_hash_table_remove_all(dock->groups);
    g_ptr_array_set_size(dock->running, 0);
    GList *wins = btd_window_tracker_list(dock->tracker);
    const char *own_id = g_application_get_application_id(G_APPLICATION(dock->app));
    for (GList *l = wins; l; l = l->next) {
        BtdWindow *w = l->data;
        /* our own dialogs (Wayland has no pid to filter them by) */
        if (w->app_id && (g_strcmp0(w->app_id, own_id) == 0 || g_strcmp0(w->app_id, g_get_prgname()) == 0))
            continue;
        if (!w->resolved) {
            BtdWindowKeys k = { .app_id = w->app_id, .wm_class = w->wm_class, .wm_instance = w->wm_instance,
                                .launched_desktop_file = w->launched_desktop_file, .exe = w->exe };
            g_free(w->desktop_id);
            w->desktop_id = g_strdup(btd_desktop_index_match(dock->index, &k));
            w->resolved = TRUE;
        }
        char *key = w->desktop_id ? g_strdup(w->desktop_id)
                                  : g_strconcat("class:", w->app_id ? w->app_id
                                                          : w->wm_class ? w->wm_class
                                                          : w->wm_instance ? w->wm_instance : "unknown", NULL);
        AppGroup *g = g_hash_table_lookup(dock->groups, key);
        if (!g) {
            g = g_new0(AppGroup, 1);
            g->key = key;
            g->desktop_id = g_strdup(w->desktop_id);
            g->windows = g_ptr_array_new();
            g_hash_table_insert(dock->groups, g->key, g);
        } else {
            g_free(key);
        }
        g_ptr_array_add(g->windows, w);
    }
    g_list_free(wins);

    if (dock->launching_key && g_hash_table_contains(dock->groups, dock->launching_key))
        g_clear_pointer(&dock->launching_key, g_free);

    /* running section: groups not represented in the active tab */
    if (dock->prefs.show_running) {
        BtdTab *tab = btd_model_get_active_tab(dock->model);
        GHashTable *pinned = g_hash_table_new(g_str_hash, g_str_equal);
        for (guint i = 0; tab && i < tab->items->len; i++) {
            const char *id = btd_dock_item_desktop_id(dock, tab->items->pdata[i]);
            if (id)
                g_hash_table_add(pinned, (gpointer)id);
        }
        GHashTableIter it;
        AppGroup *g;
        g_hash_table_iter_init(&it, dock->groups);
        while (g_hash_table_iter_next(&it, NULL, (gpointer *)&g))
            if (!g_hash_table_contains(pinned, g->key))
                g_ptr_array_add(dock->running, g);
        g_hash_table_unref(pinned);
        g_ptr_array_sort(dock->running, group_by_serial);
    }
}

static void
on_windows_changed(BtdWindowTracker *t, gpointer data)
{
    BtdDock *dock = data;
    update_groups(dock);
    btd_dock_relayout(dock);
}

static gboolean
rebuild_index(gpointer data)
{
    BtdDock *dock = data;
    dock->index_rebuild_id = 0;
    char **dirs = btd_desktop_data_dirs();
    BtdDesktopIndex *fresh = btd_desktop_index_new((const char *const *)dirs);
    g_strfreev(dirs);
    btd_desktop_index_free(dock->index);
    dock->index = fresh;
    GList *wins = btd_window_tracker_list(dock->tracker);
    for (GList *l = wins; l; l = l->next)
        ((BtdWindow *)l->data)->resolved = FALSE;
    g_list_free(wins);
    btd_icon_cache_clear(dock->icons);
    update_groups(dock);
    btd_dock_relayout(dock);
    return G_SOURCE_REMOVE;
}

static void
on_apps_changed(GAppInfoMonitor *monitor, gpointer data)
{
    BtdDock *dock = data;
    /* installs touch many files: rebuild once, after things settle */
    if (dock->index_rebuild_id)
        g_source_remove(dock->index_rebuild_id);
    dock->index_rebuild_id = g_timeout_add_seconds(2, rebuild_index, dock);
}

/* ---- actions -------------------------------------------------------------- */

static GAppLaunchContext *
launch_context(BtdDock *dock, guint32 time)
{
    GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gtk_widget_get_display(dock->window));
    gdk_app_launch_context_set_timestamp(ctx, time ? time : GDK_CURRENT_TIME);
    if (dock->monitor) {
        /* GdkAppLaunchContext has no per-monitor API in GTK 3; the screen is enough */
        gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(dock->window));
    }
    return G_APP_LAUNCH_CONTEXT(ctx);
}

void
btd_dock_launch(BtdDock *dock, const BtdItem *item, GList *uris, const char *action)
{
    GAppLaunchContext *ctx = launch_context(dock, gtk_get_current_event_time());
    GError *err = NULL;
    gboolean ok = action ? btd_launch_action(item, action, ctx, &err) : btd_launch_item(item, ctx, uris, &err);
    g_object_unref(ctx);
    if (!ok) {
        char *name = btd_dock_item_name(dock, item);
        char *primary = g_strdup_printf(_("Could not open “%s”"), name);
        btd_dock_error(dock, primary, err ? err->message : NULL);
        g_free(primary);
        g_free(name);
        g_clear_error(&err);
        return;
    }
    const char *id = btd_dock_item_desktop_id(dock, item);
    if (id && !btd_dock_item_group(dock, item)) {
        g_free(dock->launching_key);
        dock->launching_key = g_strdup(id);
        dock->launching_until = g_get_monotonic_time() + LAUNCH_FEEDBACK_US;
        dock_queue_draw(dock);
    }
}

static BtdWindow *
most_recent(AppGroup *g)
{
    BtdWindow *best = NULL;
    for (guint i = 0; i < g->windows->len; i++) {
        BtdWindow *w = g->windows->pdata[i];
        if (!best || (best->minimized && !w->minimized))
            best = w;
    }
    return best;
}

/* Without foreign-toplevel activation (wlmaker), a running application can
 * still raise itself: DBusActivatable applications get
 * org.freedesktop.Application.Activate with an xdg-activation token, the
 * same request a launcher sends. Returns FALSE when that is not possible. */
static gboolean raise_impl(BtdDock *dock, AppGroup *group, gboolean dry_run);

gboolean
btd_dock_raise_via_application(BtdDock *dock, AppGroup *group)
{
    return raise_impl(dock, group, FALSE);
}

static gboolean
raise_impl(BtdDock *dock, AppGroup *group, gboolean dry_run)
{
    const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, group ? group->desktop_id : NULL);
    if (!e || !e->dbus_activatable)
        return FALSE;
    char *bus_name = g_strndup(e->id, strlen(e->id) - strlen(".desktop"));
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    gboolean ok = FALSE;
    if (bus && g_dbus_is_name(bus_name)) {
        GVariant *owner = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                      "org.freedesktop.DBus", "NameHasOwner",
                                                      g_variant_new("(s)", bus_name), G_VARIANT_TYPE("(b)"),
                                                      G_DBUS_CALL_FLAGS_NONE, 500, NULL, NULL);
        gboolean has_owner = FALSE;
        if (owner) {
            g_variant_get(owner, "(b)", &has_owner);
            g_variant_unref(owner);
        }
        if (has_owner && dry_run) {
            ok = TRUE;
        } else if (has_owner) {
            GAppLaunchContext *ctx = launch_context(dock, gtk_get_current_event_time());
            GDesktopAppInfo *info = g_desktop_app_info_new(e->id);
            char *token = info ? g_app_launch_context_get_startup_notify_id(ctx, G_APP_INFO(info), NULL) : NULL;
            GVariantBuilder pd;
            g_variant_builder_init(&pd, G_VARIANT_TYPE_VARDICT);
            if (token) {
                g_variant_builder_add(&pd, "{sv}", "activation-token", g_variant_new_string(token));
                g_variant_builder_add(&pd, "{sv}", "desktop-startup-id", g_variant_new_string(token));
            }
            char *path = g_strdelimit(g_strconcat("/", bus_name, NULL), ".-", '/');
            g_dbus_connection_call(bus, bus_name, path, "org.freedesktop.Application", "Activate",
                                   g_variant_new("(a{sv})", &pd), NULL, G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL,
                                   NULL);
            g_debug("Activate %s via D-Bus (token %s)", bus_name, token ? token : "none");
            g_free(path);
            g_free(token);
            g_clear_object(&info);
            g_object_unref(ctx);
            ok = TRUE;
        }
    }
    g_clear_object(&bus);
    g_free(bus_name);
    return ok;
}

void
btd_dock_activate_group(BtdDock *dock, AppGroup *group, guint32 time)
{
    BtdWindowTracker *t = dock->tracker;
    if (!group || group->windows->len == 0)
        return;
    if (!(t->caps & BTD_WIN_CAP_ACTIVATE)) {
        btd_dock_raise_via_application(dock, group);
        return;
    }
    int active = -1;
    for (guint i = 0; i < group->windows->len; i++)
        if (((BtdWindow *)group->windows->pdata[i])->active)
            active = (int)i;
    if (active >= 0) {
        if (group->windows->len > 1) {
            BtdWindow *next = group->windows->pdata[(active + 1) % group->windows->len];
            t->activate(t, next, time);
        } else if (dock->prefs.click_minimizes && (t->caps & BTD_WIN_CAP_MINIMIZE)) {
            t->minimize(t, group->windows->pdata[active]);
        }
        return;
    }
    t->activate(t, most_recent(group), time);
}

void
btd_dock_activate_item(BtdDock *dock, const BtdItem *item, AppGroup *group, guint32 time, gboolean new_instance)
{
    if (item->type == BTD_ITEM_SEPARATOR)
        return;
    if (!new_instance && group && group->windows->len > 0) {
        if (dock->tracker->caps & BTD_WIN_CAP_ACTIVATE) {
            btd_dock_activate_group(dock, group, time);
            return;
        }
        if (btd_dock_raise_via_application(dock, group))
            return;
    }
    btd_dock_launch(dock, item, NULL, NULL);
}

void
btd_dock_select_tab(BtdDock *dock, const char *spec)
{
    if (!spec)
        return;
    if (g_str_equal(spec, "next"))
        btd_model_cycle_active_tab(dock->model, 1);
    else if (g_str_equal(spec, "prev") || g_str_equal(spec, "previous"))
        btd_model_cycle_active_tab(dock->model, -1);
    else if (g_ascii_isdigit(spec[0])) {
        guint64 n = g_ascii_strtoull(spec, NULL, 10);
        BtdTab *tab = n >= 1 ? btd_model_get_tab(dock->model, (guint)n - 1) : NULL;
        if (tab)
            btd_model_set_active_tab(dock->model, tab->id);
    } else {
        btd_model_set_active_tab(dock->model, spec);
    }
    if (dock->hidden) {
        /* show briefly so the user sees which tab is active */
        btd_dock_set_hidden(dock, FALSE);
        btd_dock_set_hidden(dock, TRUE);
    }
}

#ifdef BTD_HAVE_X11
static void
on_shortcut(int action, gpointer data)
{
    BtdDock *dock = data;
    if (action == -1)
        btd_dock_select_tab(dock, "prev");
    else if (action == -2)
        btd_dock_select_tab(dock, "next");
    else {
        char n[8];
        g_snprintf(n, sizeof(n), "%d", action);
        btd_dock_select_tab(dock, n);
    }
}
#endif

/* ---- dialogs ------------------------------------------------------------- */

static void
dialog_parent(BtdDock *dock, GtkWindow *dialog)
{
    /* A layer-shell surface cannot parent an xdg_toplevel. */
    if (!dock->layer_shell)
        gtk_window_set_transient_for(dialog, GTK_WINDOW(dock->window));
    gtk_window_set_position(dialog, GTK_WIN_POS_CENTER);
}

void
btd_dock_error(BtdDock *dock, const char *primary, const char *secondary)
{
    GtkWidget *d = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, "%s", primary);
    if (secondary)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", secondary);
    gtk_window_set_title(GTK_WINDOW(d), _("Tabbed Dock"));
    dialog_parent(dock, GTK_WINDOW(d));
    g_signal_connect(d, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show(d);
}

gboolean
btd_dock_ask(BtdDock *dock, const char *primary, const char *secondary, const char *ok_label)
{
    GtkWidget *d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
                                          primary);
    if (secondary)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", secondary);
    gtk_dialog_add_buttons(GTK_DIALOG(d), _("_Cancel"), GTK_RESPONSE_CANCEL, ok_label, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    gtk_window_set_title(GTK_WINDOW(d), _("Tabbed Dock"));
    dialog_parent(dock, GTK_WINDOW(d));
    dock->menu_open = TRUE;
    gboolean ok = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK;
    dock->menu_open = FALSE;
    gtk_widget_destroy(d);
    return ok;
}

char *
btd_dock_ask_text(BtdDock *dock, const char *title, const char *label, const char *initial)
{
    GtkWidget *d = gtk_dialog_new_with_buttons(title, NULL, GTK_DIALOG_MODAL, _("_Cancel"), GTK_RESPONSE_CANCEL,
                                               _("_OK"), GTK_RESPONSE_OK, NULL);
    dialog_parent(dock, GTK_WINDOW(d));
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_box_set_spacing(GTK_BOX(box), 8);
    gtk_container_add(GTK_CONTAINER(box), gtk_label_new(label));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), BTD_MAX_LABEL);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    if (initial)
        gtk_entry_set_text(GTK_ENTRY(entry), initial);
    gtk_container_add(GTK_CONTAINER(box), entry);
    gtk_widget_show_all(d);
    dock->menu_open = TRUE;
    char *result = NULL;
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK)
        result = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    dock->menu_open = FALSE;
    gtk_widget_destroy(d);
    return result;
}

void
btd_dock_show_preferences(BtdDock *dock)
{
    if (!dock->prefs_dialog) {
        dock->prefs_dialog = btd_prefs_dialog_new(dock);
        dialog_parent(dock, GTK_WINDOW(dock->prefs_dialog));
        g_signal_connect(dock->prefs_dialog, "destroy", G_CALLBACK(gtk_widget_destroyed), &dock->prefs_dialog);
    }
    gtk_window_present(GTK_WINDOW(dock->prefs_dialog));
}

/* ---- layout --------------------------------------------------------------- */

static PangoLayout *
make_layout(BtdDock *dock, const char *text, PangoFontDescription *font)
{
    PangoLayout *l = gtk_widget_create_pango_layout(dock->area, text);
    pango_layout_set_font_description(l, font);
    return l;
}

static BtdTabDirection
tab_dir(BtdDock *dock)
{
    return dock->prefs.edge == BTD_EDGE_BOTTOM ? BTD_TAB_UP : BTD_TAB_DOWN;
}

static void
add_slot(BtdDock *dock, SlotKind kind, int x, int y, int w, int h, int index)
{
    Slot s = { .kind = kind, .rect = { x, y, w, h }, .index = index };
    g_array_append_val(dock->slots, s);
}

static GdkMonitor *
choose_monitor(BtdDock *dock)
{
    GdkDisplay *display = gtk_widget_get_display(dock->window);
    int n = gdk_display_get_n_monitors(display);
    const char *want = dock->prefs.monitor;
    if (n <= 0)
        return NULL;
    if (want && g_ascii_isdigit(want[0])) {
        guint64 i = g_ascii_strtoull(want, NULL, 10);
        if (i < (guint64)n)
            return gdk_display_get_monitor(display, (int)i);
    } else if (want && g_strcmp0(want, "primary") != 0) {
        for (int i = 0; i < n; i++) {
            GdkMonitor *m = gdk_display_get_monitor(display, i);
            if (g_strcmp0(btd_monitor_name(m), want) == 0)
                return m;
        }
    }
    /* "primary", or the configured monitor is gone (unplugged): never end up
     * off-screen. Wayland has no primary output: use the one at the layout
     * origin (top-left most). */
    GdkMonitor *primary = dock->wayland ? NULL : gdk_display_get_primary_monitor(display);
    if (!primary) {
        for (int i = 0; i < n; i++) {
            GdkMonitor *m = gdk_display_get_monitor(display, i);
            GdkRectangle g, best;
            gdk_monitor_get_geometry(m, &g);
            if (primary)
                gdk_monitor_get_geometry(primary, &best);
            if (!primary || g.y < best.y || (g.y == best.y && g.x < best.x))
                primary = m;
        }
    }
    return primary;
}

void
btd_dock_relayout(BtdDock *dock)
{
    if (!dock->area)
        return;
    g_array_set_size(dock->slots, 0);
    BtdTabDirection dir = tab_dir(dock);
    BtdStyle style = dock->prefs.style;
    int icon = dock->prefs.icon_size;
    int tile = dock->tile = icon + 16;
    gboolean labels = dock->prefs.show_labels;
    int lw, lh;
    PangoLayout *probe = make_layout(dock, "Ág", dock->tab_font);
    pango_layout_get_pixel_size(probe, &lw, &lh);
    g_object_unref(probe);
    dock->tab_h = style == BTD_STYLE_TILES ? MAX(22, lh + 8) : MAX(26, lh + 12);
    dock->stub_w = dock->tab_h + 10;
    probe = make_layout(dock, "Ág", dock->label_font);
    pango_layout_get_pixel_size(probe, &lw, &lh);
    g_object_unref(probe);
    dock->label_h = labels ? lh : 0;
    if (style == BTD_STYLE_TILES) {
        dock->cell_w = labels ? MAX(tile, icon + 44) : tile;
        dock->cell_h = tile + (labels ? dock->label_h + 2 : 0);
    } else {
        /* OPENSTEP shelf: icons on the marble, names underneath, room for
         * the running studs at the bottom */
        dock->cell_w = labels ? icon + 52 : icon + 20;
        dock->cell_h = 5 + icon + (labels ? 3 + dock->label_h : 0) + 9;
    }
    dock->shelf_h = dock->cell_h + 8;
    dock->shadow = dock->composited ? 4 : 0;
    dock->shelf_y = dir == BTD_TAB_UP ? dock->shadow + dock->tab_h : 0;

    /* shelf contents, left-aligned */
    int ty = dock->shelf_y + 4;
    int x = PAD + 2;
    BtdTab *tab = btd_model_get_active_tab(dock->model);
    for (guint i = 0; tab && i < tab->items->len; i++) {
        BtdItem *it = tab->items->pdata[i];
        int w = it->type == BTD_ITEM_SEPARATOR ? SEP_W : dock->cell_w;
        add_slot(dock, SLOT_ITEM, x, ty, w, dock->cell_h, (int)i);
        x += w;
    }
    if (dock->running->len > 0) {
        add_slot(dock, SLOT_RUNNING_GROOVE, x, ty, SEP_W, dock->cell_h, -1);
        x += SEP_W;
        for (guint i = 0; i < dock->running->len; i++) {
            add_slot(dock, SLOT_RUNNING, x, ty, dock->cell_w, dock->cell_h, (int)i);
            x += dock->cell_w;
        }
    }
    int content_end = x;
    if (tab && tab->items->len == 0 && dock->running->len == 0) {
        PangoLayout *l = make_layout(dock, _("Drop applications, files or folders here"), dock->hint_font);
        pango_layout_get_pixel_size(l, &lw, &lh);
        g_object_unref(l);
        content_end = x + lw + 24;
    }

    /* tab row: [arrow] [tab][tab]… [new-tab], tabs sharing the width equally */
    guint n_tabs = btd_model_n_tabs(dock->model);
    int max_label = 0;
    for (guint i = 0; i < n_tabs; i++) {
        PangoLayout *l = make_layout(dock, btd_dock_tab_label(btd_model_get_tab(dock->model, i)), dock->tab_font);
        pango_layout_get_pixel_size(l, &lw, &lh);
        g_object_unref(l);
        max_label = MAX(max_label, lw);
    }
    int natural_tab = MAX(110, max_label + 2 * (int)BTD_BETA_SLANT + 28);
    int width = MAX(content_end + PAD + 2, 2 * dock->stub_w + (int)n_tabs * natural_tab);
    width = MAX(width, 3 * dock->cell_w);
    dock->monitor = choose_monitor(dock);
    if (dock->monitor) {
        GdkRectangle geo;
        gdk_monitor_get_geometry(dock->monitor, &geo);
        if (dock->prefs.align == BTD_ALIGN_FILL || width > geo.width)
            width = geo.width;
    }
    /* On Wayland a surface anchored to both sides gets its width from the
     * compositor (the usable area, e.g. minus the Clip): draw to that. */
    if (dock->layer_shell && dock->prefs.align == BTD_ALIGN_FILL && dock->fill_width > 0)
        width = dock->fill_width;
    int tab_y = dir == BTD_TAB_UP ? dock->shadow : dock->shelf_h;
    add_slot(dock, SLOT_HIDE, 0, tab_y, dock->stub_w, dock->tab_h, -1);
    int tabs_w = width - 2 * dock->stub_w;
    x = dock->stub_w;
    for (guint i = 0; i < n_tabs; i++) {
        int w = (int)((guint64)tabs_w * (i + 1) / n_tabs) - (int)((guint64)tabs_w * i / n_tabs);
        add_slot(dock, SLOT_TAB, x, tab_y, w, dock->tab_h, (int)i);
        x += w;
    }
    add_slot(dock, SLOT_ADD_TAB, width - dock->stub_w, tab_y, dock->stub_w, dock->tab_h, -1);
    dock->tabs_end = width;
    dock->width = width;
    int full_h = dock->shadow + dock->tab_h + dock->shelf_h;
    /* Wayland: the layer surface itself shrinks to the strip. X11: Window
     * Maker ignores moves of dock windows, so the window keeps its place and
     * is reduced to the strip by its shape. */
    dock->height = dock->hidden && dock->layer_shell ? HIDDEN_H : full_h;
    dock->strip_y = dock->layer_shell || dock->prefs.edge == BTD_EDGE_TOP ? 0 : full_h - HIDDEN_H;

    position_window(dock);
    dock_queue_draw(dock);
}

/* ---- positioning ---------------------------------------------------------- */

#ifdef BTD_HAVE_X11
static void
x11_set_struts(BtdDock *dock, int x, int y)
{
    GdkWindow *win = gtk_widget_get_window(dock->window);
    if (!win || !GDK_IS_X11_WINDOW(win))
        return;
    GdkDisplay *display = gdk_window_get_display(win);
    if (!dock->prefs.reserve_space || dock->prefs.autohide || dock->hidden || !dock->monitor) {
        gdk_property_delete(win, gdk_atom_intern_static_string("_NET_WM_STRUT_PARTIAL"));
        gdk_property_delete(win, gdk_atom_intern_static_string("_NET_WM_STRUT"));
        return;
    }
    int scale = scale_factor(dock);
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    long root_h = DisplayHeight(dpy, DefaultScreen(dpy));
    long reserved = (long)(dock->height - dock->shadow) * scale;
    long strut[12] = { 0 };
    long x0 = (long)x * scale, x1 = (long)(x + dock->width) * scale - 1;
    if (dock->prefs.edge == BTD_EDGE_BOTTOM) {
        strut[3] = root_h - (long)(y + dock->height) * scale + reserved;
        strut[10] = x0;
        strut[11] = x1;
    } else {
        strut[2] = (long)y * scale + reserved;
        strut[8] = x0;
        strut[9] = x1;
    }
    gdk_property_change(win, gdk_atom_intern_static_string("_NET_WM_STRUT_PARTIAL"),
                        gdk_atom_intern_static_string("CARDINAL"), 32, GDK_PROP_MODE_REPLACE, (guchar *)strut, 12);
    gdk_property_change(win, gdk_atom_intern_static_string("_NET_WM_STRUT"),
                        gdk_atom_intern_static_string("CARDINAL"), 32, GDK_PROP_MODE_REPLACE, (guchar *)strut, 4);
}
#endif

static void
update_shape(BtdDock *dock)
{
    GdkWindow *win = gtk_widget_get_window(dock->window);
    if (!win)
        return;
    /* Pixel-exact mask of what is drawn: shelf + tabs (+ the add-tab button). */
    int w = dock->width, h = dock->height;
    cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A1, w, h);
    cairo_t *cr = cairo_create(mask);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    if (dock->hidden) {
        cairo_rectangle(cr, 0, dock->strip_y, w, HIDDEN_H);
    } else {
        cairo_rectangle(cr, 0, dock->shelf_y, w, dock->shelf_h);
        cairo_fill(cr);
        for (guint i = 0; i < dock->slots->len; i++) {
            Slot *s = &g_array_index(dock->slots, Slot, i);
            if (s->kind != SLOT_TAB && s->kind != SLOT_ADD_TAB && s->kind != SLOT_HIDE)
                continue;
            if (dock->prefs.style == BTD_STYLE_CLASSIC)
                btd_beta_tab_path(cr, s->rect.x, s->rect.y, s->rect.width, s->rect.height, tab_dir(dock),
                                  s->kind != SLOT_HIDE, s->kind != SLOT_ADD_TAB);
            else
                btd_tab_path(cr, s->rect.x, s->rect.y, s->rect.width, s->rect.height, tab_dir(dock),
                             dock->palette.radius, dock->palette.style);
            cairo_fill(cr);
        }
    }
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t *region = gdk_cairo_region_create_from_surface(mask);
    cairo_surface_destroy(mask);
    if (dock->composited)
        gtk_widget_input_shape_combine_region(dock->window, region);
    else
        gtk_widget_shape_combine_region(dock->window, region);
    cairo_region_destroy(region);
}

static void
position_window(BtdDock *dock)
{
    /* full width on Wayland: the compositor sets the width (anchored left
     * and right); asking for the monitor width could exceed the usable area */
    gboolean wl_fill = dock->layer_shell && dock->prefs.align == BTD_ALIGN_FILL;
    gtk_widget_set_size_request(dock->area, wl_fill ? 1 : dock->width, dock->height);
    /* A layer surface keeps its old size unless asked to shrink: request the
     * minimum and let the size request of the drawing area win. */
    gtk_window_resize(GTK_WINDOW(dock->window), dock->layer_shell ? 1 : dock->width,
                      dock->layer_shell ? 1 : dock->height);
#ifdef BTD_HAVE_WAYLAND
    if (dock->layer_shell) {
        GtkWindow *w = GTK_WINDOW(dock->window);
        gboolean bottom = dock->prefs.edge == BTD_EDGE_BOTTOM;
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_BOTTOM, bottom);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_TOP, !bottom);
        BtdAlign a = dock->prefs.align;
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_LEFT, a == BTD_ALIGN_LEFT || a == BTD_ALIGN_FILL);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_RIGHT, a == BTD_ALIGN_RIGHT || a == BTD_ALIGN_FILL);
        if (dock->monitor)
            gtk_layer_set_monitor(w, dock->monitor);
        int zone = (dock->prefs.reserve_space && !dock->prefs.autohide && !dock->hidden) ? dock->height - dock->shadow
                                                                                          : 0;
        gtk_layer_set_exclusive_zone(w, zone);
        update_shape(dock);
        return;
    }
#endif
    if (!dock->monitor)
        return;
    GdkRectangle geo;
    gdk_monitor_get_geometry(dock->monitor, &geo);
    int x = geo.x, y;
    switch (dock->prefs.align) {
    case BTD_ALIGN_CENTER:
        x = geo.x + (geo.width - dock->width) / 2;
        break;
    case BTD_ALIGN_RIGHT:
        x = geo.x + geo.width - dock->width;
        break;
    default:
        break;
    }
    y = dock->prefs.edge == BTD_EDGE_BOTTOM ? geo.y + geo.height - dock->height : geo.y;
    gtk_window_move(GTK_WINDOW(dock->window), x, y);
    /* One ConfigureRequest with position and size: done separately, Window
     * Maker resizes around the old centre and the autohide strip ends up
     * in the middle of where the dock was. */
    GdkWindow *gdkwin = gtk_widget_get_window(dock->window);
    if (gdkwin)
        gdk_window_move_resize(gdkwin, x, y, dock->width, dock->height);
#ifdef BTD_HAVE_X11
    x11_set_struts(dock, x, y);
#endif
    update_shape(dock);
}

static void
on_monitors_changed(GdkDisplay *display, GdkMonitor *monitor, gpointer data)
{
    BtdDock *dock = data;
    btd_monitor_name(monitor); /* request the connector name of a new output */
#ifdef BTD_HAVE_WAYLAND
    if (dock->layer_shell && gtk_widget_get_mapped(dock->window)) {
        /* the layer surface may have been closed with its output: map it
         * again on the monitor chosen now */
        gtk_widget_hide(dock->window);
        btd_dock_relayout(dock);
        gtk_widget_show(dock->window);
    }
#endif
    btd_dock_relayout(dock);
}

static void
on_screen_changed(GdkScreen *screen, gpointer data)
{
    btd_dock_relayout(data);
}

/* ---- autohide ------------------------------------------------------------- */

void
btd_dock_set_hidden(BtdDock *dock, gboolean hidden)
{
    if (dock->hide_id) {
        g_source_remove(dock->hide_id);
        dock->hide_id = 0;
    }
    if (hidden && dock->menu_open)
        return;
    if (hidden && !dock->prefs.autohide && !dock->collapsing)
        return;
    if (hidden == dock->hidden)
        return;
    dock->hidden = hidden;
    btd_dock_relayout(dock);
}

static gboolean
hide_timeout(gpointer data)
{
    BtdDock *dock = data;
    dock->hide_id = 0;
    btd_dock_set_hidden(dock, TRUE);
    return G_SOURCE_REMOVE;
}

static void
schedule_hide(BtdDock *dock)
{
    if (!dock->prefs.autohide || dock->hidden)
        return;
    if (dock->hide_id)
        g_source_remove(dock->hide_id);
    dock->hide_id = g_timeout_add(HIDE_DELAY_MS, hide_timeout, dock);
}

/* ---- drawing -------------------------------------------------------------- */

static gboolean tick_cb(GtkWidget *w, GdkFrameClock *clock, gpointer data);

static void
ensure_tick(BtdDock *dock)
{
    gboolean need = (dock->tab_anim < 1.0) || (dock->launching_key && dock->prefs.animations);
    if (need && !dock->tick_id)
        dock->tick_id = gtk_widget_add_tick_callback(dock->area, tick_cb, dock, NULL);
}

static void
dock_queue_draw(BtdDock *dock)
{
    if (dock->area)
        gtk_widget_queue_draw(dock->area);
}

static gboolean
tick_cb(GtkWidget *w, GdkFrameClock *clock, gpointer data)
{
    BtdDock *dock = data;
    gint64 now = g_get_monotonic_time();
    gboolean more = FALSE;
    if (dock->tab_anim < 1.0) {
        dock->tab_anim = MIN(1.0, (double)(now - dock->tab_anim_start) / TAB_ANIM_US);
        more |= dock->tab_anim < 1.0;
    }
    if (dock->launching_key) {
        if (now > dock->launching_until)
            g_clear_pointer(&dock->launching_key, g_free);
        else
            more |= dock->prefs.animations;
    }
    gtk_widget_queue_draw(w);
    if (!more) {
        dock->tick_id = 0;
        return G_SOURCE_REMOVE; /* back to 0 % CPU */
    }
    return G_SOURCE_CONTINUE;
}

static void
draw_icon_at(cairo_t *cr, cairo_surface_t *s, int size, double x, double y, double alpha)
{
    double sx = 1, sy = 1;
    cairo_surface_get_device_scale(s, &sx, &sy);
    int iw = (int)(cairo_image_surface_get_width(s) / sx);
    int ih = (int)(cairo_image_surface_get_height(s) / sy);
    cairo_set_source_surface(cr, s, round(x + (size - iw) / 2.0), round(y + (size - ih) / 2.0));
    cairo_paint_with_alpha(cr, alpha);
}

/* the icon's square inside a cell */
static void
cell_icon_pos(BtdDock *dock, const GdkRectangle *r, double *ix, double *iy)
{
    if (dock->prefs.style == BTD_STYLE_TILES) {
        *ix = r->x + (r->width - dock->tile) / 2.0 + 8;
        *iy = r->y + 7;
    } else {
        *ix = r->x + (r->width - dock->prefs.icon_size) / 2.0;
        *iy = r->y + 5;
    }
}

static void
draw_cell(BtdDock *dock, cairo_t *cr, Slot *s, const BtdItem *item, AppGroup *group, gboolean is_hover,
          gboolean pressed)
{
    const BtdPalette *p = &dock->palette;
    int scale = scale_factor(dock), icon_size = dock->prefs.icon_size;
    gboolean active = FALSE, all_min = group && group->windows->len > 0;
    for (guint i = 0; group && i < group->windows->len; i++) {
        BtdWindow *w = group->windows->pdata[i];
        active |= w->active;
        all_min &= w->minimized;
    }
    const char *key = item ? btd_dock_item_desktop_id(dock, item) : group->key;
    gboolean launching = dock->launching_key && g_strcmp0(key, dock->launching_key) == 0;
    double ix, iy;
    cell_icon_pos(dock, &s->rect, &ix, &iy);

    if (dock->prefs.style == BTD_STYLE_TILES) {
        btd_draw_tile(cr, p, s->rect.x + (s->rect.width - dock->tile) / 2.0, s->rect.y, dock->tile, active,
                      pressed || launching, is_hover);
    } else if (dock->prefs.style == BTD_STYLE_MODERN) {
        if (active || pressed || launching || is_hover)
            btd_draw_tile(cr, p, ix - 8, iy - 4, icon_size + 16, active, pressed || launching, is_hover);
    } else if (active || pressed || launching) {
        /* OPENSTEP has no tiles: the focused application sits in a sunken well */
        btd_draw_selection_well(cr, p, ix - 5, iy - 3, icon_size + 10, icon_size + 6);
    }

    cairo_surface_t *icon;
    if (item) {
        icon = btd_icon_cache_lookup_item(dock->icons, item, dock->index, icon_size, scale);
    } else {
        const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, group->desktop_id);
        icon = btd_icon_cache_lookup_name(dock->icons, e ? e->icon : NULL, icon_size, scale);
    }
    double alpha = 1.0;
    if (launching && dock->prefs.animations)
        alpha = 0.55 + 0.45 * cos((double)g_get_monotonic_time() / 160000.0);
    if (item && (item->type == BTD_ITEM_FILE || item->type == BTD_ITEM_FOLDER || item->type == BTD_ITEM_APPIMAGE) &&
        !g_file_test(item->target, G_FILE_TEST_EXISTS))
        alpha = 0.35; /* target missing (unmounted drive?) */
    draw_icon_at(cr, icon, icon_size, ix, iy, alpha);

    double label_y;
    if (dock->prefs.style == BTD_STYLE_TILES) {
        if (group)
            btd_draw_running(cr, p, s->rect.x + s->rect.width / 2.0, s->rect.y + dock->tile - 6,
                             (int)group->windows->len, all_min);
        label_y = s->rect.y + dock->tile + 1;
    } else {
        label_y = iy + icon_size + 3;
        if (group)
            btd_draw_running(cr, p, s->rect.x + s->rect.width / 2.0, s->rect.y + s->rect.height - 6,
                             (int)group->windows->len, all_min);
    }
    if (dock->prefs.show_labels) {
        char *name = item ? btd_dock_item_name(dock, item) : NULL;
        if (!name) {
            const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, group->desktop_id);
            BtdWindow *w0 = group->windows->pdata[0];
            name = g_strdup(e && e->name ? e->name : w0->title ? w0->title : group->key);
        }
        PangoLayout *l = make_layout(dock, name, dock->label_font);
        pango_layout_set_width(l, (s->rect.width - 4) * PANGO_SCALE);
        pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
        pango_layout_set_alignment(l, PANGO_ALIGN_CENTER);
        btd_set_color(cr, p->style == BTD_STYLE_MODERN ? &p->text : &p->black);
        cairo_move_to(cr, s->rect.x + 2, round(label_y));
        pango_cairo_show_layout(cr, l);
        g_object_unref(l);
        g_free(name);
    }
}

static void
draw_shelf(BtdDock *dock, cairo_t *cr, double y, double gap_x, double gap_w)
{
    if (dock->prefs.style == BTD_STYLE_CLASSIC)
        btd_draw_beta_shelf(cr, &dock->palette, 0, y, dock->width, dock->shelf_h, gap_x, gap_w, tab_dir(dock),
                            dock->prefs.align != BTD_ALIGN_FILL);
    else
        btd_draw_shelf(cr, &dock->palette, 0, y, dock->width, dock->shelf_h, gap_x, gap_w, tab_dir(dock));
}

static void
draw_tab_slot(BtdDock *dock, cairo_t *cr, Slot *s, gboolean active, gboolean highlighted, const char *label)
{
    const BtdPalette *p = &dock->palette;
    BtdTabDirection dir = tab_dir(dock);
    PangoLayout *l = label ? make_layout(dock, label, dock->tab_font) : NULL;
    if (l) {
        int lw, lh;
        pango_layout_get_pixel_size(l, &lw, &lh);
        if (lw > s->rect.width - 2 * BTD_BETA_SLANT - 8) {
            pango_layout_set_width(l, (int)(s->rect.width - 2 * BTD_BETA_SLANT - 8) * PANGO_SCALE);
            pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
        }
    }
    /* the selected tab reaches 1 px into the shelf so it looks "in front" */
    int y = s->rect.y - (active && dir == BTD_TAB_DOWN ? 1 : 0), h = s->rect.height + (active ? 1 : 0);
    if (dock->prefs.style == BTD_STYLE_CLASSIC)
        btd_draw_beta_tab(cr, p, s->rect.x, y, s->rect.width, h, active, highlighted, dir, s->kind != SLOT_HIDE,
                          s->kind != SLOT_ADD_TAB, l);
    else
        btd_draw_tab(cr, p, s->rect.x, y, s->rect.width, h, active, highlighted, dir, l);
    if (l)
        g_object_unref(l);
    double cy = s->rect.y + s->rect.height / 2.0 + (dir == BTD_TAB_UP ? 1 : -1);
    if (s->kind == SLOT_HIDE)
        btd_draw_arrow_glyph(cr, p, s->rect.x + s->rect.width / 2.0 - 2, cy, MIN(16, s->rect.height - 8),
                             dock->prefs.edge == BTD_EDGE_BOTTOM);
    else if (s->kind == SLOT_ADD_TAB)
        btd_draw_document_glyph(cr, p, s->rect.x + s->rect.width / 2.0 + 3, cy, MIN(16, s->rect.height - 9));
}

static gboolean
on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    BtdDock *dock = data;
    const BtdPalette *p = &dock->palette;
    BtdTabDirection dir = tab_dir(dock);
    static gboolean first_frame = TRUE;
    if (first_frame) {
        first_frame = FALSE;
        if (g_getenv("BTD_TIMING"))
            g_message("first frame %.1f ms after start", (g_get_monotonic_time() - btd_start_time) / 1000.0);
    }

    if (dock->composited) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }
    if (dock->hidden) {
        cairo_save(cr);
        cairo_rectangle(cr, 0, dock->strip_y, dock->width, HIDDEN_H);
        cairo_clip(cr);
        draw_shelf(dock, cr, dir == BTD_TAB_UP ? dock->strip_y : dock->strip_y - dock->shelf_h + HIDDEN_H, 0, 0);
        cairo_restore(cr);
        return TRUE;
    }

    BtdTab *active_tab = btd_model_get_active_tab(dock->model);
    Slot *active_slot = NULL;
    /* shadow over the desktop, then inactive tabs from right to left */
    if (dock->shadow)
        btd_draw_shadow(cr, 0, dir == BTD_TAB_UP ? dock->shelf_y - dock->shadow : dock->shelf_y + dock->shelf_h,
                        dock->width, dock->shadow, dir);
    for (int i = (int)dock->slots->len - 1; i >= 0; i--) {
        Slot *s = &g_array_index(dock->slots, Slot, i);
        gboolean hover = dock->hover_slot == i;
        if (s->kind == SLOT_ADD_TAB || s->kind == SLOT_HIDE) {
            draw_tab_slot(dock, cr, s, FALSE, hover, NULL);
        } else if (s->kind == SLOT_TAB) {
            BtdTab *tab = btd_model_get_tab(dock->model, (guint)s->index);
            if (tab == active_tab) {
                active_slot = s;
                continue;
            }
            gboolean hl = dock->drop_tab == s->index || (dock->prefs.style != BTD_STYLE_TILES && hover);
            draw_tab_slot(dock, cr, s, FALSE, hl, btd_dock_tab_label(tab));
        }
    }
    double gap_x = active_slot ? active_slot->rect.x : 0, gap_w = active_slot ? active_slot->rect.width : 0;
    draw_shelf(dock, cr, dock->shelf_y, gap_x, gap_w);
    if (active_slot) {
        draw_tab_slot(dock, cr, active_slot, TRUE, FALSE, btd_dock_tab_label(active_tab));
        dock->active_tab_rect = active_slot->rect;
    }

    /* shelf content (cross-fades after a tab switch) */
    if (dock->tab_anim < 1.0)
        cairo_push_group(cr);
    gboolean any = FALSE;
    for (guint i = 0; i < dock->slots->len; i++) {
        Slot *s = &g_array_index(dock->slots, Slot, i);
        gboolean hover = dock->hover_slot == (int)i && dock->prefs.style == BTD_STYLE_MODERN;
        (void)hover;
        gboolean pressed = dock->press_slot == (int)i && !dock->drag_started;
        if (s->kind == SLOT_ITEM) {
            BtdItem *it = active_tab->items->pdata[s->index];
            any = TRUE;
            if (it->type == BTD_ITEM_SEPARATOR) {
                btd_draw_groove(cr, p, s->rect.x + SEP_W / 2.0 - 1, s->rect.y + 6, s->rect.height - 12);
                continue;
            }
            gboolean target = dock->drop_on_item == s->index;
            draw_cell(dock, cr, s, it, btd_dock_item_group(dock, it), hover, pressed || target);
        } else if (s->kind == SLOT_RUNNING_GROOVE) {
            btd_draw_groove(cr, p, s->rect.x + SEP_W / 2.0 - 1, s->rect.y + 2, s->rect.height - 4);
        } else if (s->kind == SLOT_RUNNING) {
            any = TRUE;
            draw_cell(dock, cr, s, NULL, dock->running->pdata[s->index], hover, pressed);
        }
    }
    if (!any) {
        PangoLayout *l = make_layout(dock, _("Drop applications, files or folders here"), dock->hint_font);
        int lw, lh;
        pango_layout_get_pixel_size(l, &lw, &lh);
        btd_draw_engraved_text(cr, p, PAD + 14, dock->shelf_y + (dock->shelf_h - lh) / 2.0, l);
        g_object_unref(l);
    }
    if (dock->drop_index >= 0 && active_tab) {
        int x = PAD + 2;
        for (guint i = 0; i < dock->slots->len; i++) {
            Slot *s = &g_array_index(dock->slots, Slot, i);
            if (s->kind == SLOT_ITEM && s->index == dock->drop_index) {
                x = s->rect.x;
                break;
            }
            if (s->kind == SLOT_ITEM)
                x = s->rect.x + s->rect.width;
        }
        btd_draw_drop_marker(cr, p, x, dock->shelf_y + 4, dock->cell_h);
    }
    if (dock->idrag && dock->idrag_kind != SLOT_TAB) {
        /* the source tab, not the active one: spring-loading may have switched tabs */
        BtdTab *src_tab = btd_model_find_tab(dock->model, dock->drag_tab_id, NULL);
        BtdItem *it = dock->idrag_kind == SLOT_ITEM && src_tab && dock->idrag_index >= 0 &&
                              (guint)dock->idrag_index < src_tab->items->len
                          ? src_tab->items->pdata[dock->idrag_index]
                          : NULL;
        cairo_surface_t *icon = NULL;
        if (it && it->type != BTD_ITEM_SEPARATOR)
            icon = btd_icon_cache_lookup_item(dock->icons, it, dock->index, dock->prefs.icon_size,
                                              scale_factor(dock));
        else if (!it && dock->idrag_kind == SLOT_RUNNING && dock->idrag_app) {
            const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, dock->idrag_app);
            icon = btd_icon_cache_lookup_name(dock->icons, e ? e->icon : NULL, dock->prefs.icon_size,
                                              scale_factor(dock));
        }
        if (icon)
            draw_icon_at(cr, icon, dock->prefs.icon_size, dock->idrag_x - dock->prefs.icon_size / 2.0,
                         dock->idrag_y - dock->prefs.icon_size / 2.0, 0.7);
    }
    if (dock->tab_anim < 1.0) {
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 0.25 + 0.75 * dock->tab_anim);
    }
    ensure_tick(dock);
    return TRUE;
}

/* ---- input ---------------------------------------------------------------- */

static int
slot_at(BtdDock *dock, double x, double y)
{
    /* tabs overlap: the active tab and right-most visible tab win */
    int found = -1;
    for (guint i = 0; i < dock->slots->len; i++) {
        Slot *s = &g_array_index(dock->slots, Slot, i);
        if (x >= s->rect.x && x < s->rect.x + s->rect.width && y >= s->rect.y && y < s->rect.y + s->rect.height) {
            if (s->kind == SLOT_TAB && found >= 0) {
                BtdTab *tab = btd_model_get_tab(dock->model, (guint)s->index);
                Slot *prev = &g_array_index(dock->slots, Slot, found);
                /* inside the overlapping slope, prefer the active tab, else the left one */
                if (prev->kind == SLOT_TAB && tab != btd_model_get_active_tab(dock->model) &&
                    x < prev->rect.x + prev->rect.width - BTD_TAB_SLOPE * 0.5)
                    continue;
            }
            found = (int)i;
        }
    }
    return found;
}

Slot *
btd_dock_slot(BtdDock *dock, int index)
{
    if (index < 0 || (guint)index >= dock->slots->len)
        return NULL;
    return &g_array_index(dock->slots, Slot, index);
}

static gboolean
on_button_press(GtkWidget *w, GdkEventButton *ev, gpointer data)
{
    BtdDock *dock = data;
    if (ev->type != GDK_BUTTON_PRESS)
        return TRUE;
    if (dock->menu && gtk_widget_get_visible(dock->menu)) {
        /* a click on the dock closes an open menu (needed where the
         * compositor does not end the popup grab itself) */
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(dock->menu));
        return TRUE;
    }
    int i = slot_at(dock, ev->x, ev->y);
    Slot *s = btd_dock_slot(dock, i);
    if (ev->button == GDK_BUTTON_SECONDARY) {
        btd_dock_popup_menu(dock, i, (GdkEvent *)ev);
        return TRUE;
    }
    if (s && ev->button == GDK_BUTTON_PRIMARY && !(ev->state & GDK_SHIFT_MASK) &&
        !(dock->tracker->caps & BTD_WIN_CAP_ACTIVATE) && (s->kind == SLOT_ITEM || s->kind == SLOT_RUNNING)) {
        AppGroup *g = s->kind == SLOT_RUNNING
                          ? dock->running->pdata[s->index]
                          : btd_dock_item_group(dock, btd_model_get_active_tab(dock->model)->items->pdata[s->index]);
        if (g && g->windows->len > 0 && !raise_impl(dock, g, TRUE)) {
            /* The application cannot be switched to and relaunching might
             * duplicate it: offer its windows and "New Window". Opened on
             * press — wlroots refuses popup grabs once the button is up. */
            btd_dock_popup_menu(dock, i, (GdkEvent *)ev);
            dock->press_slot = -1;
            return TRUE;
        }
    }
    dock->press_slot = i;
    dock->press_x = ev->x;
    dock->press_y = ev->y;
    dock->drag_started = FALSE;
    if (s && s->kind == SLOT_TAB && ev->button == GDK_BUTTON_PRIMARY) {
        /* NeXT tabs select on press */
        btd_model_set_active_tab(dock->model, btd_model_get_tab(dock->model, (guint)s->index)->id);
    }
    dock_queue_draw(dock);
    return TRUE;
}

static gboolean
on_button_release(GtkWidget *w, GdkEventButton *ev, gpointer data)
{
    BtdDock *dock = data;
    if (dock->idrag) {
        btd_dock_idrag_end(dock, ev->x, ev->y, (ev->state & GDK_CONTROL_MASK) != 0, FALSE);
        dock->press_slot = -1;
        dock->drag_started = FALSE;
        return TRUE;
    }
    int i = slot_at(dock, ev->x, ev->y);
    int pressed = dock->press_slot;
    dock->press_slot = -1;
    dock_queue_draw(dock);
    if (dock->drag_started || i != pressed || i < 0)
        return TRUE;
    Slot *s = btd_dock_slot(dock, i);
    gboolean new_instance = ev->button == GDK_BUTTON_MIDDLE || (ev->state & GDK_SHIFT_MASK);
    if (ev->button != GDK_BUTTON_PRIMARY && ev->button != GDK_BUTTON_MIDDLE)
        return TRUE;
    BtdTab *tab = btd_model_get_active_tab(dock->model);
    switch (s->kind) {
    case SLOT_ITEM: {
        BtdItem *item = tab->items->pdata[s->index];
        btd_dock_activate_item(dock, item, btd_dock_item_group(dock, item), ev->time, new_instance);
        break;
    }
    case SLOT_RUNNING: {
        AppGroup *g = dock->running->pdata[s->index];
        if (new_instance && g->desktop_id) {
            BtdItem item = { .type = BTD_ITEM_APP, .target = g->desktop_id };
            btd_dock_launch(dock, &item, NULL, NULL);
        } else if (!(dock->tracker->caps & BTD_WIN_CAP_ACTIVATE)) {
            btd_dock_raise_via_application(dock, g);
        } else {
            btd_dock_activate_group(dock, g, ev->time);
        }
        break;
    }
    case SLOT_HIDE:
        /* the arrow folds the shelf away until the pointer touches the edge */
        dock->collapsing = TRUE;
        btd_dock_set_hidden(dock, TRUE);
        dock->collapsing = FALSE;
        break;
    case SLOT_ADD_TAB: {
        char *name = btd_dock_ask_text(dock, _("New Tab"), _("Name of the new tab:"), "");
        if (name) {
            BtdTab *t = btd_model_add_tab(dock->model, name, -1);
            if (t)
                btd_model_set_active_tab(dock->model, t->id);
            g_free(name);
        }
        break;
    }
    default:
        break;
    }
    return TRUE;
}

static gboolean
on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer data)
{
    BtdDock *dock = data;
    int i = slot_at(dock, ev->x, ev->y);
    if (i != dock->hover_slot) {
        dock->hover_slot = i;
        dock_queue_draw(dock);
    }
    if (dock->idrag) {
        gboolean outside = ev->x < 0 || ev->y < 0 || ev->x >= dock->width || ev->y >= dock->height;
        Slot *src = btd_dock_slot(dock, dock->press_slot);
        if (outside && !dock->wayland && src && src->kind == SLOT_ITEM) {
            /* leaving the dock with an item: hand over to XDND (export to a file manager) */
            btd_dock_idrag_end(dock, ev->x, ev->y, FALSE, TRUE);
            btd_dock_dnd_begin(dock, dock->press_slot, (GdkEvent *)ev);
        } else {
            btd_dock_idrag_motion(dock, ev->x, ev->y);
        }
        return TRUE;
    }
    if (dock->press_slot >= 0 && !dock->drag_started && (ev->state & GDK_BUTTON1_MASK) &&
        gtk_drag_check_threshold(w, (int)dock->press_x, (int)dock->press_y, (int)ev->x, (int)ev->y)) {
        Slot *s = btd_dock_slot(dock, dock->press_slot);
        if (s && (s->kind == SLOT_ITEM || s->kind == SLOT_TAB || s->kind == SLOT_RUNNING)) {
            dock->drag_started = TRUE;
            btd_dock_idrag_begin(dock, dock->press_slot, ev->x, ev->y);
        }
    }
    return TRUE;
}

static gboolean
on_scroll(GtkWidget *w, GdkEventScroll *ev, gpointer data)
{
    BtdDock *dock = data;
    int delta = 0;
    if (ev->direction == GDK_SCROLL_UP || ev->direction == GDK_SCROLL_LEFT)
        delta = -1;
    else if (ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_RIGHT)
        delta = 1;
    else if (ev->direction == GDK_SCROLL_SMOOTH) {
        static double acc;
        acc += ev->delta_y + ev->delta_x;
        if (fabs(acc) < 1.0)
            return TRUE;
        delta = acc > 0 ? 1 : -1;
        acc = 0;
    }
    Slot *s = btd_dock_slot(dock, slot_at(dock, ev->x, ev->y));
    AppGroup *g = NULL;
    if (s && s->kind == SLOT_ITEM)
        g = btd_dock_item_group(dock, btd_model_get_active_tab(dock->model)->items->pdata[s->index]);
    else if (s && s->kind == SLOT_RUNNING)
        g = dock->running->pdata[s->index];
    if (g && g->windows->len > 0 && (dock->tracker->caps & BTD_WIN_CAP_ACTIVATE)) {
        /* scroll over an application cycles its windows */
        int active = 0;
        for (guint i = 0; i < g->windows->len; i++)
            if (((BtdWindow *)g->windows->pdata[i])->active)
                active = (int)i;
        int n = (int)g->windows->len;
        BtdWindow *next = g->windows->pdata[((active + delta) % n + n) % n];
        dock->tracker->activate(dock->tracker, next, ev->time);
        return TRUE;
    }
    btd_model_cycle_active_tab(dock->model, delta);
    return TRUE;
}

static gboolean
on_enter(GtkWidget *w, GdkEventCrossing *ev, gpointer data)
{
    BtdDock *dock = data;
    btd_dock_set_hidden(dock, FALSE);
    return FALSE;
}

static gboolean
on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer data)
{
    BtdDock *dock = data;
    if (ev->detail == GDK_NOTIFY_INFERIOR)
        return FALSE;
    dock->hover_slot = -1;
    dock_queue_draw(dock);
    schedule_hide(dock);
    return FALSE;
}

static gboolean
on_query_tooltip(GtkWidget *w, int x, int y, gboolean keyboard, GtkTooltip *tooltip, gpointer data)
{
    BtdDock *dock = data;
    Slot *s = btd_dock_slot(dock, slot_at(dock, x, y));
    if (!s || dock->drag_started)
        return FALSE;
    GString *text = g_string_new(NULL);
    AppGroup *g = NULL;
    if (s->kind == SLOT_ITEM) {
        BtdItem *it = btd_model_get_active_tab(dock->model)->items->pdata[s->index];
        if (it->type == BTD_ITEM_SEPARATOR) {
            g_string_free(text, TRUE);
            return FALSE;
        }
        char *name = btd_dock_item_name(dock, it);
        g_string_append(text, name);
        g_free(name);
        if (it->type != BTD_ITEM_APP) {
            char *disp = g_filename_display_name(it->target);
            g_string_append_printf(text, "\n%s", disp);
            g_free(disp);
        }
        g = btd_dock_item_group(dock, it);
    } else if (s->kind == SLOT_RUNNING) {
        g = dock->running->pdata[s->index];
        const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, g->desktop_id);
        BtdWindow *w0 = g->windows->pdata[0];
        g_string_append(text, e && e->name ? e->name : w0->title ? w0->title : g->key);
    } else if (s->kind == SLOT_TAB) {
        BtdTab *tab = btd_model_get_tab(dock->model, (guint)s->index);
        g_string_append(text, btd_dock_tab_label(tab));
        if (s->index < 9 && dock->shortcuts_grabbed)
            g_string_append_printf(text, "\nSuper+Alt+%d", s->index + 1);
    } else if (s->kind == SLOT_ADD_TAB) {
        g_string_append(text, _("New tab"));
    } else if (s->kind == SLOT_HIDE) {
        g_string_append(text, _("Hide the shelf (it comes back when the pointer touches the edge)"));
    } else {
        g_string_free(text, TRUE);
        return FALSE;
    }
    if (g) {
        guint n = g->windows->len;
        char *count = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE, "%u window", "%u windows", n), n);
        g_string_append_printf(text, "\n%s", count);
        g_free(count);
    }
    gtk_tooltip_set_text(tooltip, text->str);
    g_string_free(text, TRUE);
    return TRUE;
}

/* ---- setup ---------------------------------------------------------------- */

void
btd_dock_apply_prefs(BtdDock *dock)
{
    btd_palette_init(&dock->palette, dock->prefs.style, dock->dark_mode);
    if (!dock->prefs.autohide && dock->hidden)
        dock->hidden = FALSE;
#ifdef BTD_HAVE_X11
    GdkDisplay *display = gtk_widget_get_display(dock->window);
    if (GDK_IS_X11_DISPLAY(display)) {
        if (dock->prefs.global_shortcuts && !dock->shortcuts_grabbed)
            dock->shortcuts_grabbed = (guint)btd_x11_grab_shortcuts(display, on_shortcut, dock) > 0;
        else if (!dock->prefs.global_shortcuts && dock->shortcuts_grabbed) {
            btd_x11_ungrab_shortcuts(display);
            dock->shortcuts_grabbed = FALSE;
        }
    }
#endif
    update_groups(dock);
    btd_dock_relayout(dock);
    btd_icon_cache_clear(dock->icons);
    schedule_hide(dock);
}

/* Big WindowMaker light/dark mode: big-wm-theme writes "light" or "dark"
 * to ~/.config/big-wm/theme (no file: light). Read at start and followed
 * while running, so the shelf changes together with the window borders. */
static GFile *
theme_file(void)
{
    char *path = g_build_filename(g_get_user_config_dir(), "big-wm", "theme", NULL);
    GFile *f = g_file_new_for_path(path);
    g_free(path);
    return f;
}

static gboolean
read_dark_mode(void)
{
    GFile *f = theme_file();
    char *data = NULL;
    gboolean dark = FALSE;
    if (g_file_load_contents(f, NULL, &data, NULL, NULL, NULL))
        dark = g_strcmp0(g_strstrip(data), "dark") == 0;
    g_free(data);
    g_object_unref(f);
    return dark;
}

static void
on_theme_file_changed(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent event, gpointer data)
{
    BtdDock *dock = data;
    if (event == G_FILE_MONITOR_EVENT_CHANGED || event == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
        return; /* wait for CHANGES_DONE_HINT (or the rename that creates it) */
    gboolean dark = read_dark_mode();
    if (dark == dock->dark_mode)
        return;
    dock->dark_mode = dark;
    btd_palette_init(&dock->palette, dock->prefs.style, dark);
    dock_queue_draw(dock);
}

static void
on_icon_theme_changed(GtkIconTheme *theme, gpointer data)
{
    BtdDock *dock = data;
    btd_icon_cache_clear(dock->icons);
    dock_queue_draw(dock);
}

static gboolean
relayout_idle(gpointer data)
{
    btd_dock_relayout(data);
    return G_SOURCE_REMOVE;
}

static void
on_size_allocate(GtkWidget *w, GdkRectangle *alloc, gpointer data)
{
    BtdDock *dock = data;
    /* full width on Wayland: adopt the width the compositor configured */
    if (dock->layer_shell && dock->prefs.align == BTD_ALIGN_FILL && alloc->width > 1 &&
        alloc->width != dock->fill_width) {
        dock->fill_width = alloc->width;
        g_idle_add(relayout_idle, dock);
    }
}

static void
on_realize(GtkWidget *w, gpointer data)
{
    btd_dock_relayout(data);
}

static void
on_scale_changed(GObject *obj, GParamSpec *pspec, gpointer data)
{
    BtdDock *dock = data;
    btd_icon_cache_clear(dock->icons);
    btd_dock_relayout(dock);
}

static gboolean
show_broken_notice(gpointer data)
{
    BtdDock *dock = data;
    char *msg = g_strdup_printf(_("The file %s was damaged and has been kept aside with the suffix “.broken”. "
                                  "The last good copy (or the default tabs) is being used."),
                                dock->config_path);
    btd_dock_error(dock, _("Dock configuration recovered"), msg);
    g_free(msg);
    return G_SOURCE_REMOVE;
}

static void
add_flatpak_icon_dirs(GtkIconTheme *theme)
{
    char **dirs = btd_desktop_data_dirs();
    for (char **d = dirs; *d; d++) {
        if (strstr(*d, "flatpak")) {
            char *icons = g_build_filename(*d, "icons", NULL);
            if (g_file_test(icons, G_FILE_TEST_IS_DIR))
                gtk_icon_theme_append_search_path(theme, icons);
            g_free(icons);
        }
    }
    g_strfreev(dirs);
}

BtdDock *
btd_dock_new(GtkApplication *app, const char *config_path)
{
    BtdDock *dock = g_new0(BtdDock, 1);
    dock->app = app;
    dock->hover_slot = dock->press_slot = -1;
    dock->drop_index = dock->drop_tab = dock->drop_on_item = -1;
    dock->drag_item_index = dock->drag_tab_index = -1;
    dock->tab_anim = 1.0;
    dock->config_path = config_path ? g_strdup(config_path) : btd_config_default_path();
    dock->slots = g_array_new(FALSE, TRUE, sizeof(Slot));
    dock->groups = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)group_free);
    dock->running = g_ptr_array_new();
    dock->model = btd_model_new();
    btd_prefs_init_defaults(&dock->prefs);
    dock->tab_font = btd_font_new(TRUE, 13);
    dock->hint_font = btd_font_new(FALSE, 12);
    dock->label_font = btd_font_new(FALSE, 12);

    char **dirs = btd_desktop_data_dirs();
    dock->index = btd_desktop_index_new((const char *const *)dirs);
    g_strfreev(dirs);
    dock->app_monitor = g_app_info_monitor_get();
    g_signal_connect(dock->app_monitor, "changed", G_CALLBACK(on_apps_changed), dock);

    load_config(dock);
    btd_model_set_changed_func(dock->model, on_model_changed, dock);
    dock->dark_mode = read_dark_mode();
    GFile *tf = theme_file();
    dock->theme_monitor = g_file_monitor_file(tf, G_FILE_MONITOR_NONE, NULL, NULL);
    g_object_unref(tf);
    if (dock->theme_monitor)
        g_signal_connect(dock->theme_monitor, "changed", G_CALLBACK(on_theme_file_changed), dock);
    btd_palette_init(&dock->palette, dock->prefs.style, dock->dark_mode);

    GtkIconTheme *theme = gtk_icon_theme_get_default();
    add_flatpak_icon_dirs(theme);
    dock->icons = btd_icon_cache_new(theme, BTD_ICON_CACHE_MAX);
    g_signal_connect(theme, "changed", G_CALLBACK(on_icon_theme_changed), dock);

    GdkDisplay *display = gdk_display_get_default();
#ifdef BTD_HAVE_WAYLAND
    dock->wayland = GDK_IS_WAYLAND_DISPLAY(display);
#endif
    dock->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_application(GTK_WINDOW(dock->window), app);
    gtk_window_set_title(GTK_WINDOW(dock->window), "Big Tabbed Dock");
    gtk_window_set_decorated(GTK_WINDOW(dock->window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(dock->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dock->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(dock->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(dock->window), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(dock->window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(dock->window), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_stick(GTK_WINDOW(dock->window));
    gtk_widget_set_app_paintable(dock->window, TRUE);

    GdkScreen *screen = gtk_widget_get_screen(dock->window);
    GdkVisual *rgba = gdk_screen_get_rgba_visual(screen);
    if (rgba && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(dock->window, rgba);
        dock->composited = TRUE;
    }
#ifdef BTD_HAVE_WAYLAND
    if (dock->wayland && gtk_layer_is_supported()) {
        GtkWindow *w = GTK_WINDOW(dock->window);
        gtk_layer_init_for_window(w);
        gtk_layer_set_namespace(w, "big-tabbed-dock");
        gtk_layer_set_layer(w, GTK_LAYER_SHELL_LAYER_TOP);
        /* wlmaker refuses ON_DEMAND keyboard interactivity: never take the keyboard */
        gtk_layer_set_keyboard_mode(w, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        dock->layer_shell = TRUE;
    } else if (dock->wayland) {
        g_warning("compositor without wlr-layer-shell: the dock cannot position itself");
    }
#endif

    dock->area = gtk_drawing_area_new();
    gtk_widget_add_events(dock->area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                                          GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_ENTER_NOTIFY_MASK |
                                          GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_set_has_tooltip(dock->area, TRUE);
    gtk_container_add(GTK_CONTAINER(dock->window), dock->area);
    g_signal_connect(dock->area, "draw", G_CALLBACK(on_draw), dock);
    g_signal_connect(dock->area, "button-press-event", G_CALLBACK(on_button_press), dock);
    g_signal_connect(dock->area, "button-release-event", G_CALLBACK(on_button_release), dock);
    g_signal_connect(dock->area, "motion-notify-event", G_CALLBACK(on_motion), dock);
    g_signal_connect(dock->area, "scroll-event", G_CALLBACK(on_scroll), dock);
    g_signal_connect(dock->area, "enter-notify-event", G_CALLBACK(on_enter), dock);
    g_signal_connect(dock->area, "leave-notify-event", G_CALLBACK(on_leave), dock);
    g_signal_connect(dock->area, "query-tooltip", G_CALLBACK(on_query_tooltip), dock);
    g_signal_connect(dock->window, "realize", G_CALLBACK(on_realize), dock);
    g_signal_connect(dock->area, "size-allocate", G_CALLBACK(on_size_allocate), dock);
    g_signal_connect(dock->window, "notify::scale-factor", G_CALLBACK(on_scale_changed), dock);
    g_signal_connect(display, "monitor-added", G_CALLBACK(on_monitors_changed), dock);
    g_signal_connect(display, "monitor-removed", G_CALLBACK(on_monitors_changed), dock);
    g_signal_connect(screen, "size-changed", G_CALLBACK(on_screen_changed), dock);
    g_signal_connect(screen, "monitors-changed", G_CALLBACK(on_screen_changed), dock);
    btd_dock_dnd_init(dock);

    dock->tracker = btd_window_tracker_new(display);
#ifdef BTD_HAVE_WAYLAND
    if (dock->wayland) {
        btd_wayland_set_names_changed((void (*)(gpointer))btd_dock_relayout, dock);
        for (int i = 0; i < gdk_display_get_n_monitors(display); i++)
            btd_monitor_name(gdk_display_get_monitor(display, i)); /* request names */
    }
#endif
    btd_window_tracker_set_changed_func(dock->tracker, on_windows_changed, dock);
    g_message("window tracking: %s", dock->tracker->name);

    btd_dock_apply_prefs(dock);
    if (dock->config_broken_notice)
        g_idle_add(show_broken_notice, dock);
    return dock;
}

void
btd_dock_show(BtdDock *dock)
{
    gtk_widget_show_all(dock->window);
    btd_dock_relayout(dock);
}

void
btd_dock_free(BtdDock *dock)
{
    if (!dock)
        return;
    if (dock->save_id)
        btd_dock_save_now(dock);
    if (dock->hide_id)
        g_source_remove(dock->hide_id);
    if (dock->spring_id)
        g_source_remove(dock->spring_id);
    if (dock->index_rebuild_id)
        g_source_remove(dock->index_rebuild_id);
    g_signal_handlers_disconnect_by_data(gdk_display_get_default(), dock);
    g_signal_handlers_disconnect_by_data(gdk_screen_get_default(), dock);
    g_signal_handlers_disconnect_by_data(gtk_icon_theme_get_default(), dock);
    g_signal_handlers_disconnect_by_data(dock->app_monitor, dock);
    g_object_unref(dock->app_monitor);
    if (dock->theme_monitor) {
        g_signal_handlers_disconnect_by_data(dock->theme_monitor, dock);
        g_file_monitor_cancel(dock->theme_monitor);
        g_object_unref(dock->theme_monitor);
    }
    if (dock->prefs_dialog)
        gtk_widget_destroy(dock->prefs_dialog);
    if (dock->menu_leave_id)
        g_source_remove(dock->menu_leave_id);
    if (dock->menu)
        gtk_widget_destroy(dock->menu);
    btd_window_tracker_free(dock->tracker);
    if (dock->window)
        gtk_widget_destroy(dock->window);
    btd_icon_cache_free(dock->icons);
    btd_desktop_index_free(dock->index);
    g_hash_table_unref(dock->groups);
    g_ptr_array_unref(dock->running);
    g_array_unref(dock->slots);
    btd_model_free(dock->model);
    btd_prefs_clear(&dock->prefs);
    pango_font_description_free(dock->tab_font);
    pango_font_description_free(dock->hint_font);
    pango_font_description_free(dock->label_font);
    btd_palette_clear(&dock->palette);
    g_free(dock->launching_key);
    g_free(dock->drag_tab_id);
    g_free(dock->idrag_app);
    g_free(dock->config_path);
    g_free(dock);
}
