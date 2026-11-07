/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-appicons — the appicons of Window Maker, for the Wayland session.
 *
 * Window Maker shows an icon (appicon) for each running application that is
 * not docked, in a row from the bottom-left corner of the screen. wlmaker
 * has none: this is that row, as a layer surface below the windows. One
 * tile per application (its windows grouped like the tabbed dock groups
 * them), with the icon of WMWindowAttributes or of its .desktop file:
 *
 *   double-click    shows the application (restores and raises its windows)
 *   right-click     menu: Show, Hide, Close
 *   hover           balloon with the name
 *
 * An application with a launcher in the Dock or the Clip (AppID in
 * ~/.config/wlmaker/State.plist) is shown there, not here, as in Window
 * Maker. On X11 Window Maker draws its own appicons: this exits.
 */
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <string.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "btd-desktop.h"
#include "btd-draw.h"
#include "btd-icons.h"
#include "btd-windows.h"
#include "btd-wmattrs.h"

#define TILE 64
#define ICON 48

typedef struct {
    char *key;
    char *desktop_id;
    char *name;
    GPtrArray *windows; /* BtdWindow*, owned by the tracker */
} AppIcon;

typedef struct {
    GtkWidget *window;
    GtkWidget *area;
    BtdWindowTracker *tracker;
    BtdDesktopIndex *index;
    BtdIconCache *icons;
    BtdWmAttrs *wmattrs;
    BtdPalette palette;
    gboolean dark;
    GPtrArray *apps;        /* AppIcon* */
    GHashTable *docked;     /* AppIDs of the Dock's and the Clip's launchers */
    GFileMonitor *state_monitor;
    GFileMonitor *theme_monitor;
    int hover;
    int pressed;
} Appicons;

static void
app_icon_free(AppIcon *a)
{
    g_free(a->key);
    g_free(a->desktop_id);
    g_free(a->name);
    g_ptr_array_unref(a->windows);
    g_free(a);
}

static char *
config_path(const char *a, const char *b)
{
    return g_build_filename(g_get_user_config_dir(), a, b, NULL);
}

/* AppIDs of the launchers on the Dock and the Clip. */
static void
load_docked(Appicons *s)
{
    g_hash_table_remove_all(s->docked);
    char *path = config_path("wlmaker", "State.plist");
    char *data = NULL;
    if (g_file_get_contents(path, &data, NULL, NULL)) {
        GRegex *re = g_regex_new("AppID\\s*=\\s*\"?([^\";\\s]+)\"?\\s*;", 0, 0, NULL);
        GMatchInfo *m = NULL;
        g_regex_match(re, data, 0, &m);
        while (g_match_info_matches(m)) {
            g_hash_table_add(s->docked, g_match_info_fetch(m, 1));
            g_match_info_next(m, NULL);
        }
        g_match_info_free(m);
        g_regex_unref(re);
    }
    g_free(data);
    g_free(path);
}

static gboolean
read_dark(void)
{
    char *path = config_path("big-wm", "theme");
    char *data = NULL;
    gboolean dark = FALSE;
    if (g_file_get_contents(path, &data, NULL, NULL))
        dark = g_strcmp0(g_strstrip(data), "dark") == 0;
    g_free(data);
    g_free(path);
    return dark;
}

static gboolean
is_docked(Appicons *s, BtdWindow *w, const char *desktop_id)
{
    const char *keys[] = { w->app_id, w->wm_class, w->wm_instance, NULL };
    for (int i = 0; keys[i]; i++) {
        if (g_hash_table_contains(s->docked, keys[i]))
            return TRUE;
        char *lower = g_ascii_strdown(keys[i], -1);
        gboolean hit = g_hash_table_contains(s->docked, lower);
        g_free(lower);
        if (hit)
            return TRUE;
    }
    if (desktop_id) {
        char *base = g_strdup(desktop_id);
        if (g_str_has_suffix(base, ".desktop"))
            base[strlen(base) - strlen(".desktop")] = '\0';
        gboolean hit = g_hash_table_contains(s->docked, base);
        g_free(base);
        return hit;
    }
    return FALSE;
}

static void
resize(Appicons *s)
{
    int n = MAX(1, (int)s->apps->len);
    gtk_widget_set_size_request(s->area, n * TILE, TILE);
    gtk_window_resize(GTK_WINDOW(s->window), n * TILE, TILE);
    if (s->apps->len == 0)
        gtk_widget_hide(s->window);
    else
        gtk_widget_show_all(s->window);
}

static void
rebuild(Appicons *s)
{
    g_ptr_array_set_size(s->apps, 0);
    GHashTable *by_key = g_hash_table_new(g_str_hash, g_str_equal);
    GList *list = btd_window_tracker_list(s->tracker);
    for (GList *l = list; l; l = l->next) {
        BtdWindow *w = l->data;
        if (!w->resolved) {
            BtdWindowKeys k = { .app_id = w->app_id, .wm_class = w->wm_class, .wm_instance = w->wm_instance,
                                .launched_desktop_file = w->launched_desktop_file, .exe = w->exe };
            g_free(w->desktop_id);
            w->desktop_id = g_strdup(btd_desktop_index_match(s->index, &k));
            w->resolved = TRUE;
        }
        if (is_docked(s, w, w->desktop_id))
            continue;
        const char *fallback = w->app_id ? w->app_id : w->wm_class ? w->wm_class : w->wm_instance;
        char *key = w->desktop_id ? g_strdup(w->desktop_id) : g_strdup(fallback ? fallback : "?");
        AppIcon *a = g_hash_table_lookup(by_key, key);
        if (!a) {
            a = g_new0(AppIcon, 1);
            a->key = key;
            a->desktop_id = g_strdup(w->desktop_id);
            const BtdDesktopEntry *e = w->desktop_id ? btd_desktop_index_lookup(s->index, w->desktop_id) : NULL;
            a->name = g_strdup(e && e->name ? e->name : fallback ? fallback : "?");
            a->windows = g_ptr_array_new();
            g_hash_table_insert(by_key, a->key, a);
            g_ptr_array_add(s->apps, a);
        } else {
            g_free(key);
        }
        g_ptr_array_add(a->windows, w);
    }
    g_list_free(list);
    g_hash_table_destroy(by_key);
    resize(s);
    gtk_widget_queue_draw(s->area);
}

static void
on_windows_changed(BtdWindowTracker *t, gpointer data)
{
    (void)t;
    rebuild(data);
}

static const char *
icon_name(Appicons *s, AppIcon *a)
{
    for (guint i = 0; i < a->windows->len; i++) {
        BtdWindow *w = a->windows->pdata[i];
        const char *icon = btd_wmattrs_icon_for_window(s->wmattrs, w->wm_instance, w->wm_class);
        if (icon)
            return icon;
    }
    const BtdDesktopEntry *e = a->desktop_id ? btd_desktop_index_lookup(s->index, a->desktop_id) : NULL;
    if (e) {
        const char *icon = btd_wmattrs_icon_for_class(s->wmattrs, e->startup_wm_class);
        if (icon)
            return icon;
        if (e->icon)
            return e->icon;
    }
    BtdWindow *w = a->windows->len ? a->windows->pdata[0] : NULL;
    return w && w->app_id ? w->app_id : "application-x-executable";
}

static gboolean
all_minimized(AppIcon *a)
{
    for (guint i = 0; i < a->windows->len; i++)
        if (!((BtdWindow *)a->windows->pdata[i])->minimized)
            return FALSE;
    return a->windows->len > 0;
}

static gboolean
on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    Appicons *s = data;
    int scale = gtk_widget_get_scale_factor(widget);
    for (guint i = 0; i < s->apps->len; i++) {
        AppIcon *a = s->apps->pdata[i];
        double x = i * TILE;
        btd_draw_tile(cr, &s->palette, x, 0, TILE, FALSE, s->pressed == (int)i, s->hover == (int)i);
        cairo_surface_t *icon = btd_icon_cache_lookup_name(s->icons, icon_name(s, a), ICON, scale);
        if (!icon)
            icon = btd_icon_cache_lookup_name(s->icons, "application-x-executable", ICON, scale);
        if (icon) {
            cairo_save(cr);
            cairo_set_source_surface(cr, icon, x + (TILE - ICON) / 2.0, (TILE - ICON) / 2.0);
            /* Hidden (all windows minimised): as Window Maker shades it. */
            cairo_paint_with_alpha(cr, all_minimized(a) ? 0.45 : 1.0);
            cairo_restore(cr);
        }
    }
    return TRUE;
}

static int
index_at(Appicons *s, double x)
{
    int i = (int)(x / TILE);
    return i >= 0 && i < (int)s->apps->len ? i : -1;
}

static void
show_app(Appicons *s, AppIcon *a, guint32 time)
{
    for (guint i = 0; i < a->windows->len; i++)
        if (s->tracker->activate)
            s->tracker->activate(s->tracker, a->windows->pdata[i], time);
}

static void
hide_app(Appicons *s, AppIcon *a)
{
    for (guint i = 0; i < a->windows->len; i++)
        if (s->tracker->minimize)
            s->tracker->minimize(s->tracker, a->windows->pdata[i]);
}

static void
close_app(Appicons *s, AppIcon *a)
{
    for (guint i = 0; i < a->windows->len; i++)
        if (s->tracker->close)
            s->tracker->close(s->tracker, a->windows->pdata[i]);
}

typedef struct {
    Appicons *s;
    char *key;
    int action;
} MenuAction;

static void
on_menu_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    MenuAction *m = data;
    for (guint i = 0; i < m->s->apps->len; i++) {
        AppIcon *a = m->s->apps->pdata[i];
        if (g_strcmp0(a->key, m->key) != 0)
            continue;
        if (m->action == 0)
            show_app(m->s, a, gtk_get_current_event_time());
        else if (m->action == 1)
            hide_app(m->s, a);
        else
            close_app(m->s, a);
        break;
    }
}

static void
menu_action_free(gpointer data, GClosure *closure)
{
    (void)closure;
    MenuAction *m = data;
    g_free(m->key);
    g_free(m);
}

static gboolean
destroy_later(gpointer data)
{
    gtk_widget_destroy(data);
    return G_SOURCE_REMOVE;
}

static void
on_menu_deactivate(GtkMenuShell *menu, gpointer data)
{
    (void)data;
    g_idle_add(destroy_later, menu);
}

static gboolean
pt(void)
{
    const char *l = g_getenv("LC_ALL");
    if (!l || !*l)
        l = g_getenv("LC_MESSAGES");
    if (!l || !*l)
        l = g_getenv("LANG");
    return l && g_str_has_prefix(l, "pt");
}

static void
popup_menu(Appicons *s, int index, AppIcon *a, GdkEvent *event)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *title = gtk_menu_item_new_with_label(a->name);
    gtk_widget_set_sensitive(title, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), title);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    const char *labels_pt[] = { "Mostrar", "Esconder", "Fechar" };
    const char *labels_en[] = { "Unhide", "Hide", "Close" };
    for (int i = 0; i < 3; i++) {
        if ((i == 1 && !s->tracker->minimize) || (i == 2 && !s->tracker->close))
            continue;
        GtkWidget *item = gtk_menu_item_new_with_label(pt() ? labels_pt[i] : labels_en[i]);
        MenuAction *m = g_new0(MenuAction, 1);
        m->s = s;
        m->key = g_strdup(a->key);
        m->action = i;
        g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_item), m, menu_action_free, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    gtk_widget_show_all(menu);
    /* GTK deactivates the menu before it activates the item: destroyed later. */
    g_signal_connect(menu, "deactivate", G_CALLBACK(on_menu_deactivate), NULL);
    /* Above the tile: the row is at the bottom of the screen. */
    GdkRectangle tile = { index * TILE, 0, TILE, 1 };
    gtk_menu_popup_at_rect(GTK_MENU(menu), gtk_widget_get_window(s->area), &tile, GDK_GRAVITY_NORTH_WEST,
                           GDK_GRAVITY_SOUTH_WEST, event);
}

static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *ev, gpointer data)
{
    (void)widget;
    Appicons *s = data;
    int i = index_at(s, ev->x);
    if (i < 0)
        return FALSE;
    AppIcon *a = s->apps->pdata[i];
    if (ev->button == GDK_BUTTON_SECONDARY && ev->type == GDK_BUTTON_PRESS) {
        popup_menu(s, i, a, (GdkEvent *)ev);
        return TRUE;
    }
    if (ev->button != GDK_BUTTON_PRIMARY)
        return FALSE;
    s->pressed = i;
    gtk_widget_queue_draw(s->area);
    /* Window Maker: a double-click shows the application. */
    if (ev->type == GDK_2BUTTON_PRESS)
        show_app(s, a, ev->time);
    return TRUE;
}

static gboolean
on_button_release(GtkWidget *widget, GdkEventButton *ev, gpointer data)
{
    (void)widget;
    (void)ev;
    Appicons *s = data;
    s->pressed = -1;
    gtk_widget_queue_draw(s->area);
    return FALSE;
}

static gboolean
on_motion(GtkWidget *widget, GdkEventMotion *ev, gpointer data)
{
    Appicons *s = data;
    int i = index_at(s, ev->x);
    if (i != s->hover) {
        s->hover = i;
        gtk_widget_set_tooltip_text(widget, i >= 0 ? ((AppIcon *)s->apps->pdata[i])->name : NULL);
        gtk_widget_queue_draw(widget);
    }
    return FALSE;
}

static gboolean
on_leave(GtkWidget *widget, GdkEventCrossing *ev, gpointer data)
{
    (void)ev;
    Appicons *s = data;
    s->hover = -1;
    s->pressed = -1;
    gtk_widget_queue_draw(widget);
    return FALSE;
}

static void
on_state_changed(GFileMonitor *m, GFile *f, GFile *o, GFileMonitorEvent e, gpointer data)
{
    (void)m;
    (void)f;
    (void)o;
    if (e != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT && e != G_FILE_MONITOR_EVENT_CREATED)
        return;
    Appicons *s = data;
    load_docked(s);
    rebuild(s);
}

static void
on_theme_changed(GFileMonitor *m, GFile *f, GFile *o, GFileMonitorEvent e, gpointer data)
{
    (void)m;
    (void)f;
    (void)o;
    if (e == G_FILE_MONITOR_EVENT_CHANGED || e == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
        return;
    Appicons *s = data;
    gboolean dark = read_dark();
    if (dark == s->dark)
        return;
    s->dark = dark;
    btd_palette_clear(&s->palette);
    btd_palette_init(&s->palette, BTD_STYLE_TILES, dark);
    gtk_widget_queue_draw(s->area);
}

static GFileMonitor *
watch(const char *a, const char *b, GCallback cb, Appicons *s)
{
    char *path = config_path(a, b);
    GFile *f = g_file_new_for_path(path);
    GFileMonitor *m = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, NULL);
    if (m)
        g_signal_connect(m, "changed", cb, s);
    g_object_unref(f);
    g_free(path);
    return m;
}

int
main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    g_set_prgname("big-wm-appicons");
    GdkDisplay *display = gdk_display_get_default();
#ifdef GDK_WINDOWING_WAYLAND
    if (!GDK_IS_WAYLAND_DISPLAY(display) || !gtk_layer_is_supported())
#endif
    {
        /* X11: Window Maker draws its own appicons. */
        g_message("big-wm-appicons: only for the Wayland session (layer shell)");
        return 0;
    }

    Appicons s = { .hover = -1, .pressed = -1 };
    s.apps = g_ptr_array_new_with_free_func((GDestroyNotify)app_icon_free);
    s.docked = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char **dirs = btd_desktop_data_dirs();
    s.index = btd_desktop_index_new((const char *const *)dirs);
    g_strfreev(dirs);
    s.icons = btd_icon_cache_new(gtk_icon_theme_get_default(), BTD_ICON_CACHE_MAX);
    s.wmattrs = btd_wmattrs_new(NULL, NULL);
    s.dark = read_dark();
    btd_palette_init(&s.palette, BTD_STYLE_TILES, s.dark);
    load_docked(&s);

    s.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(s.window), "big-wm-appicons");
    gtk_layer_init_for_window(GTK_WINDOW(s.window));
    gtk_layer_set_namespace(GTK_WINDOW(s.window), "big-wm-appicons");
    /* Below the windows, as Window Maker's appicons. */
    gtk_layer_set_layer(GTK_WINDOW(s.window), GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_anchor(GTK_WINDOW(s.window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(s.window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(s.window), 0);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(s.window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    s.area = gtk_drawing_area_new();
    gtk_widget_add_events(s.area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                                      GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(s.area, "draw", G_CALLBACK(on_draw), &s);
    g_signal_connect(s.area, "button-press-event", G_CALLBACK(on_button_press), &s);
    g_signal_connect(s.area, "button-release-event", G_CALLBACK(on_button_release), &s);
    g_signal_connect(s.area, "motion-notify-event", G_CALLBACK(on_motion), &s);
    g_signal_connect(s.area, "leave-notify-event", G_CALLBACK(on_leave), &s);
    gtk_container_add(GTK_CONTAINER(s.window), s.area);
    g_signal_connect(s.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    s.state_monitor = watch("wlmaker", "State.plist", G_CALLBACK(on_state_changed), &s);
    s.theme_monitor = watch("big-wm", "theme", G_CALLBACK(on_theme_changed), &s);

    s.tracker = btd_window_tracker_new(display);
    if (!(s.tracker->caps & BTD_WIN_CAP_LIST)) {
        g_message("big-wm-appicons: the compositor does not list its windows");
        return 0;
    }
    btd_window_tracker_set_changed_func(s.tracker, on_windows_changed, &s);
    rebuild(&s);

    gtk_main();
    btd_window_tracker_free(s.tracker);
    g_ptr_array_unref(s.apps);
    g_hash_table_destroy(s.docked);
    btd_icon_cache_free(s.icons);
    btd_wmattrs_free(s.wmattrs);
    btd_desktop_index_free(s.index);
    btd_palette_clear(&s.palette);
    return 0;
}
