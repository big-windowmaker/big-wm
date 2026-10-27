/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-tabbed-dock — Big Tabbed Dock for Window Maker (X11) and wlmaker
 * (Wayland). A separate client process: if it crashes, the session and the
 * window manager are unaffected.
 *
 * A second invocation forwards its options to the running dock over D-Bus
 * (GApplication), e.g. `big-wm-tabbed-dock --tab next` from a key binding.
 */
#include <glib-unix.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <stdio.h>

#include "btd-coexist.h"
#include "btd-dock.h"

#define APP_ID "io.github.big_windowmaker.TabbedDock"

gint64 btd_start_time;
static BtdDock *the_dock;
static char *opt_config;

static gboolean
on_term(gpointer app)
{
    if (the_dock)
        btd_dock_save_now(the_dock);
    g_application_quit(G_APPLICATION(app));
    return G_SOURCE_CONTINUE;
}

static gboolean
on_hup(gpointer app)
{
    if (the_dock)
        btd_dock_reload(the_dock);
    return G_SOURCE_CONTINUE;
}

static void
print_state(GApplicationCommandLine *cl)
{
    BtdDock *d = the_dock;
    BtdTab *active = btd_model_get_active_tab(d->model);
    GString *s = g_string_new(NULL);
    g_string_append_printf(s, "backend=%s caps=%u session=%s layer_shell=%d composited=%d\n", d->tracker->name,
                           d->tracker->caps, d->wayland ? "wayland" : "x11", d->layer_shell, d->composited);
    g_string_append_printf(s, "geometry=%dx%d hidden=%d edge=%s align=%s style=%s icon_size=%d\n", d->width,
                           d->height, d->hidden, btd_edge_to_string(d->prefs.edge),
                           btd_align_to_string(d->prefs.align), btd_style_to_string(d->prefs.style),
                           d->prefs.icon_size);
    g_string_append_printf(s, "theme=%s\n", d->dark_mode ? "dark" : "light");
    int ww = 0, wh = 0;
    gtk_window_get_size(GTK_WINDOW(d->window), &ww, &wh);
    g_string_append_printf(s, "surface=%dx%d menu_open=%d\n", ww, wh, d->menu && gtk_widget_get_visible(d->menu));
    if (d->monitor) {
        GdkRectangle g;
        gdk_monitor_get_geometry(d->monitor, &g);
        g_string_append_printf(s, "monitor=%s %d,%d %dx%d\n", btd_monitor_name(d->monitor), g.x, g.y, g.width,
                               g.height);
    }
    GdkDisplay *disp = gtk_widget_get_display(d->window);
    g_string_append(s, "monitors=");
    for (int i = 0; i < gdk_display_get_n_monitors(disp); i++) {
        GdkMonitor *m = gdk_display_get_monitor(disp, i);
        g_string_append_printf(s, "%s%d:%s/%s", i ? "," : "", i, btd_monitor_name(m), gdk_monitor_get_model(m));
    }
    g_string_append_c(s, '\n');
    if (!d->layer_shell && gtk_widget_get_window(d->window)) {
        int x, y;
        gdk_window_get_root_origin(gtk_widget_get_window(d->window), &x, &y);
        g_string_append_printf(s, "origin=%d,%d\n", x, y);
    }
    for (guint i = 0; i < btd_model_n_tabs(d->model); i++) {
        BtdTab *t = btd_model_get_tab(d->model, i);
        g_string_append_printf(s, "tab %s%s \"%s\" items=%u\n", t == active ? "*" : "", t->id,
                               btd_dock_tab_label(t), t->items->len);
        for (guint j = 0; j < t->items->len; j++) {
            BtdItem *it = t->items->pdata[j];
            AppGroup *g = btd_dock_item_group(d, it);
            g_string_append_printf(s, "  item %u %s %s windows=%u\n", j, btd_item_type_to_string(it->type),
                                   it->target ? it->target : "-", g ? g->windows->len : 0);
        }
    }
    for (guint i = 0; i < d->running->len; i++) {
        AppGroup *g = d->running->pdata[i];
        g_string_append_printf(s, "running %s windows=%u\n", g->key, g->windows->len);
    }
    static const char *kinds[] = { "tab", "add-tab", "item", "running", "groove", "hide" };
    for (guint i = 0; i < d->slots->len; i++) {
        Slot *sl = btd_dock_slot(d, (int)i);
        g_string_append_printf(s, "slot %s %d %d %d %d %d\n", kinds[sl->kind], sl->index, sl->rect.x, sl->rect.y,
                               sl->rect.width, sl->rect.height);
    }
    GList *wins = btd_window_tracker_list(d->tracker);
    for (GList *l = wins; l; l = l->next) {
        BtdWindow *w = l->data;
        g_string_append_printf(s, "window app_id=%s class=%s desktop=%s active=%d minimized=%d title=%s\n",
                               w->app_id ? w->app_id : "-", w->wm_class ? w->wm_class : "-",
                               w->desktop_id ? w->desktop_id : "-", w->active, w->minimized,
                               w->title ? w->title : "-");
    }
    g_list_free(wins);
    g_application_command_line_print(cl, "%s", s->str);
    g_string_free(s, TRUE);
}

static int
on_command_line(GApplication *app, GApplicationCommandLine *cl, gpointer data)
{
    GVariantDict *opts = g_application_command_line_get_options_dict(cl);
    const char *tab = NULL, *activate = NULL;
    gboolean prefs = FALSE, reload = FALSE, quit = FALSE, state = FALSE;
    g_variant_dict_lookup(opts, "tab", "&s", &tab);
    g_variant_dict_lookup(opts, "activate-item", "&s", &activate);
    g_variant_dict_lookup(opts, "preferences", "b", &prefs);
    g_variant_dict_lookup(opts, "reload", "b", &reload);
    g_variant_dict_lookup(opts, "quit", "b", &quit);
    g_variant_dict_lookup(opts, "print-state", "b", &state);

    if (!the_dock && (quit || state)) {
        g_application_command_line_printerr(cl, "big-wm-tabbed-dock is not running\n");
        return 1;
    }
    if (quit) {
        btd_dock_save_now(the_dock);
        g_application_quit(app);
        return 0;
    }
    if (!the_dock) {
        the_dock = btd_dock_new(GTK_APPLICATION(app), opt_config);
        btd_dock_show(the_dock);
    }
    if (tab)
        btd_dock_select_tab(the_dock, tab);
    if (reload)
        btd_dock_reload(the_dock);
    if (prefs)
        btd_dock_show_preferences(the_dock);
    if (activate) {
        /* test hook: same path as a click on the n-th item of the active tab */
        BtdTab *t = btd_model_get_active_tab(the_dock->model);
        guint64 n = g_ascii_strtoull(activate, NULL, 10);
        if (t && n < t->items->len) {
            BtdItem *it = t->items->pdata[n];
            btd_dock_activate_item(the_dock, it, btd_dock_item_group(the_dock, it), GDK_CURRENT_TIME, FALSE);
        } else {
            g_application_command_line_printerr(cl, "no item %s\n", activate);
            return 1;
        }
    }
    if (state)
        print_state(cl);
    return 0;
}

static int
on_local_options(GApplication *app, GVariantDict *opts, gpointer data)
{
    if (g_variant_dict_contains(opts, "version")) {
        printf("big-wm-tabbed-dock %s\n", BTD_VERSION);
        return 0;
    }
    const char *mode_s = NULL;
    if (g_variant_dict_lookup(opts, "dock-mode", "&s", &mode_s)) {
        BtdCoexistPaths paths;
        btd_coexist_paths_init(&paths);
        int ret = 0;
        BtdDockMode mode;
        if (g_strcmp0(mode_s, "status") == 0) {
            printf("x11=%s wayland=%s\n", btd_dock_mode_to_string(btd_coexist_get_mode(&paths, FALSE)),
                   btd_dock_mode_to_string(btd_coexist_get_mode(&paths, TRUE)));
        } else if (btd_dock_mode_from_string(mode_s, &mode)) {
            GError *err = NULL;
            if (!btd_coexist_set_mode(&paths, mode, &err)) {
                fprintf(stderr, "big-wm-tabbed-dock: %s\n", err ? err->message : "failed");
                g_clear_error(&err);
                ret = 1;
            } else {
                printf(_("Dock mode set to “%s”; it takes effect at the next login.\n"), mode_s);
            }
        } else {
            fprintf(stderr, "big-wm-tabbed-dock: --dock-mode=classic|tabbed|both|none|status\n");
            ret = 2;
        }
        btd_coexist_paths_clear(&paths);
        return ret;
    }
    char *config = NULL;
    if (g_variant_dict_lookup(opts, "config", "s", &config)) {
        g_free(opt_config);
        opt_config = config;
    }
    return -1;
}

static void
on_shutdown(GApplication *app, gpointer data)
{
    btd_dock_free(the_dock);
    the_dock = NULL;
}

int
main(int argc, char **argv)
{
    btd_start_time = g_get_monotonic_time();
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
    g_set_prgname("big-wm-tabbed-dock");
    g_set_application_name(_("Tabbed Dock"));
    gdk_set_program_class("Big-wm-tabbed-dock");

    const char *app_id = g_getenv("BTD_APPLICATION_ID"); /* tests run private instances */
    /* Control commands only talk to a running dock: they must never become
     * the primary instance themselves (a key binding pressed while the dock
     * is starting would otherwise race with its D-Bus registration). */
    GApplicationFlags flags = G_APPLICATION_HANDLES_COMMAND_LINE;
    for (int i = 1; i < argc; i++)
        if (g_str_has_prefix(argv[i], "--tab") || g_str_equal(argv[i], "-t") || g_str_equal(argv[i], "--quit") ||
            g_str_equal(argv[i], "-q") || g_str_equal(argv[i], "--reload") ||
            g_str_equal(argv[i], "--print-state") || g_str_has_prefix(argv[i], "--activate-item"))
            flags |= G_APPLICATION_IS_LAUNCHER;
    GtkApplication *app =
        gtk_application_new(app_id && g_application_id_is_valid(app_id) ? app_id : APP_ID, flags);
    const GOptionEntry entries[] = {
        { "tab", 't', 0, G_OPTION_ARG_STRING, NULL, N_("Select a tab: next, prev, a number (1…) or a tab id"), "TAB" },
        { "preferences", 'p', 0, G_OPTION_ARG_NONE, NULL, N_("Open the preferences window"), NULL },
        { "reload", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Reload the configuration file"), NULL },
        { "quit", 'q', 0, G_OPTION_ARG_NONE, NULL, N_("Quit the running dock"), NULL },
        { "dock-mode", 0, 0, G_OPTION_ARG_STRING, NULL,
          N_("Choose the docks of the session: classic, tabbed, both, none or status"), "MODE" },
        { "config", 0, 0, G_OPTION_ARG_FILENAME, NULL, N_("Use another configuration file"), "FILE" },
        { "print-state", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Print the dock state (for tests)"), NULL },
        { "activate-item", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, NULL, NULL, "N" },
        { "version", 'v', 0, G_OPTION_ARG_NONE, NULL, N_("Print the version"), NULL },
        { NULL },
    };
    g_application_add_main_option_entries(G_APPLICATION(app), entries);
    g_application_set_option_context_summary(G_APPLICATION(app),
                                             _("Horizontal tabbed application dock for Window Maker and wlmaker."));
    g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(on_command_line), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);
    g_unix_signal_add(SIGTERM, on_term, app);
    g_unix_signal_add(SIGINT, on_term, app);
    g_unix_signal_add(SIGHUP, on_hup, app);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    g_free(opt_config);
    return status;
}
