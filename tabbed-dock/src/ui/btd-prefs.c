/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Preferences window. Plain GTK 3 widgets in framed groups (the WPrefs
 * layout), no extra dependency. Every change applies at once and is saved.
 */
#include "btd-dock.h"

#include <glib/gi18n.h>

#include "btd-coexist.h"

typedef struct {
    BtdDock *dock;
    GtkWidget *edge, *align, *monitor, *style, *size, *autohide, *reserve, *anim, *running, *minimize, *shortcuts;
    GtkWidget *mode[4];
    gboolean updating;
} Prefs;

static void
prefs_from_widgets(Prefs *p)
{
    if (p->updating)
        return;
    BtdPrefs *pr = &p->dock->prefs;
    pr->edge = gtk_combo_box_get_active(GTK_COMBO_BOX(p->edge));
    pr->align = gtk_combo_box_get_active(GTK_COMBO_BOX(p->align));
    pr->style = gtk_combo_box_get_active(GTK_COMBO_BOX(p->style));
    const char *mon = gtk_combo_box_get_active_id(GTK_COMBO_BOX(p->monitor));
    g_free(pr->monitor);
    pr->monitor = g_strdup(mon ? mon : "primary");
    pr->icon_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->size));
    pr->autohide = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->autohide));
    pr->reserve_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->reserve));
    pr->animations = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->anim));
    pr->show_running = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->running));
    pr->click_minimizes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->minimize));
    pr->global_shortcuts = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->shortcuts));
    btd_dock_apply_prefs(p->dock);
    btd_dock_queue_save(p->dock);
}

static void
fill_monitors(Prefs *p)
{
    GtkComboBoxText *c = GTK_COMBO_BOX_TEXT(p->monitor);
    gtk_combo_box_text_remove_all(c);
    gtk_combo_box_text_append(c, "primary", _("Primary monitor"));
    GdkDisplay *d = gdk_display_get_default();
    for (int i = 0; i < gdk_display_get_n_monitors(d); i++) {
        GdkMonitor *m = gdk_display_get_monitor(d, i);
        GdkRectangle g;
        gdk_monitor_get_geometry(m, &g);
        const char *model = gdk_monitor_get_model(m);
        char *label = g_strdup_printf("%s (%d×%d)", model ? model : "?", g.width, g.height);
        char *id = model && *model ? g_strdup(model) : g_strdup_printf("%d", i);
        gtk_combo_box_text_append(c, id, label);
        g_free(label);
        g_free(id);
    }
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), p->dock->prefs.monitor))
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), "primary");
}

static void
widgets_from_prefs(Prefs *p)
{
    BtdPrefs *pr = &p->dock->prefs;
    p->updating = TRUE;
    gtk_combo_box_set_active(GTK_COMBO_BOX(p->edge), pr->edge);
    gtk_combo_box_set_active(GTK_COMBO_BOX(p->align), pr->align);
    gtk_combo_box_set_active(GTK_COMBO_BOX(p->style), pr->style);
    fill_monitors(p);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->size), pr->icon_size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->autohide), pr->autohide);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->reserve), pr->reserve_space);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->anim), pr->animations);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->running), pr->show_running);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->minimize), pr->click_minimizes);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->shortcuts), pr->global_shortcuts);
    p->updating = FALSE;
}

static GtkWidget *
frame(const char *title, GtkWidget **grid_out)
{
    GtkWidget *f = gtk_frame_new(title);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
    gtk_container_add(GTK_CONTAINER(f), grid);
    *grid_out = grid;
    return f;
}

static GtkWidget *
combo(Prefs *p, const char *const *labels)
{
    GtkWidget *c = gtk_combo_box_text_new();
    for (int i = 0; labels[i]; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c), labels[i]);
    g_signal_connect_swapped(c, "changed", G_CALLBACK(prefs_from_widgets), p);
    return c;
}

static GtkWidget *
check(Prefs *p, const char *label)
{
    GtkWidget *c = gtk_check_button_new_with_mnemonic(label);
    g_signal_connect_swapped(c, "toggled", G_CALLBACK(prefs_from_widgets), p);
    return c;
}

static void
row(GtkWidget *grid, int r, const char *label, GtkWidget *w)
{
    GtkWidget *l = gtk_label_new_with_mnemonic(label);
    gtk_label_set_xalign(GTK_LABEL(l), 1.0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(l), w);
    gtk_grid_attach(GTK_GRID(grid), l, 0, r, 1, 1);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_grid_attach(GTK_GRID(grid), w, 1, r, 1, 1);
}

/* ---- dock mode (coexistence) ------------------------------------------------ */

static void
on_mode_toggled(GtkToggleButton *b, Prefs *p)
{
    if (p->updating || !gtk_toggle_button_get_active(b))
        return;
    BtdDockMode mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "mode"));
    BtdCoexistPaths paths;
    btd_coexist_paths_init(&paths);
    GError *err = NULL;
    if (!btd_coexist_set_mode(&paths, mode, &err)) {
        btd_dock_error(p->dock, _("Could not change the docks"), err ? err->message : NULL);
        g_clear_error(&err);
    }
    btd_coexist_paths_clear(&paths);
}

/* ---- import / export / reset ---------------------------------------------- */

static void
do_export(Prefs *p)
{
    GtkWidget *d = gtk_file_chooser_dialog_new(_("Export Dock Settings"), NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
                                               _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Save"), GTK_RESPONSE_ACCEPT,
                                               NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d), "big-tabbed-dock.conf");
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        gsize len;
        char *data = btd_config_to_data(p->dock->model, &p->dock->prefs, &len);
        GError *err = NULL;
        if (!g_file_set_contents_full(path, data, (gssize)len, G_FILE_SET_CONTENTS_CONSISTENT, 0600, &err)) {
            btd_dock_error(p->dock, _("Export failed"), err->message);
            g_error_free(err);
        }
        g_free(data);
        g_free(path);
    }
    gtk_widget_destroy(d);
}

static void
do_import(Prefs *p)
{
    GtkWidget *d = gtk_file_chooser_dialog_new(_("Import Dock Settings"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT,
                                               NULL);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        char *data = NULL;
        gsize len = 0;
        GError *err = NULL;
        BtdModel *probe = btd_model_new();
        BtdPrefs pp;
        btd_prefs_init_defaults(&pp);
        /* validate into a scratch model first: a bad file changes nothing */
        if (g_file_get_contents(path, &data, &len, &err) && len < 4 * 1024 * 1024 &&
            btd_config_from_data(data, (gssize)len, probe, &pp, &err)) {
            btd_dock_save_now(p->dock); /* the current state becomes the .bak on the next save */
            btd_config_from_data(data, (gssize)len, p->dock->model, &p->dock->prefs, NULL);
            btd_dock_apply_prefs(p->dock);
            btd_dock_queue_save(p->dock);
            widgets_from_prefs(p);
        } else {
            btd_dock_error(p->dock, _("This is not a dock settings file"), err ? err->message : NULL);
        }
        g_clear_error(&err);
        btd_prefs_clear(&pp);
        btd_model_free(probe);
        g_free(data);
        g_free(path);
    }
    gtk_widget_destroy(d);
}

static void
do_reset(Prefs *p)
{
    if (!btd_dock_ask(p->dock, _("Reset the preferences?"),
                      _("Position, size and behaviour go back to the defaults. Tabs and shortcuts are kept."),
                      _("_Reset")))
        return;
    btd_prefs_clear(&p->dock->prefs);
    btd_prefs_init_defaults(&p->dock->prefs);
    btd_dock_apply_prefs(p->dock);
    btd_dock_queue_save(p->dock);
    widgets_from_prefs(p);
}

static void
on_response(GtkDialog *d, int response, Prefs *p)
{
    switch (response) {
    case 1:
        do_import(p);
        return;
    case 2:
        do_export(p);
        return;
    case 3:
        do_reset(p);
        return;
    default:
        gtk_widget_destroy(GTK_WIDGET(d));
    }
}

GtkWidget *
btd_prefs_dialog_new(BtdDock *dock)
{
    Prefs *p = g_new0(Prefs, 1);
    p->dock = dock;
    GtkWidget *d = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(d), _("Tabbed Dock Preferences"));
    gtk_window_set_icon_name(GTK_WINDOW(d), "preferences-desktop");
    gtk_window_set_resizable(GTK_WINDOW(d), FALSE);
    g_object_set_data_full(G_OBJECT(d), "prefs", p, g_free);
    gtk_dialog_add_button(GTK_DIALOG(d), _("_Import…"), 1);
    gtk_dialog_add_button(GTK_DIALOG(d), _("_Export…"), 2);
    gtk_dialog_add_button(GTK_DIALOG(d), _("Reset"), 3);
    gtk_dialog_add_button(GTK_DIALOG(d), _("_Close"), GTK_RESPONSE_CLOSE);
    g_signal_connect(d, "response", G_CALLBACK(on_response), p);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *outer = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(outer), 10);
    gtk_grid_set_column_spacing(GTK_GRID(outer), 10);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 10);
    gtk_container_add(GTK_CONTAINER(content), outer);

    GtkWidget *g;
    GtkWidget *f = frame(_("Position"), &g);
    const char *edges[] = { _("Bottom"), _("Top"), NULL };
    const char *aligns[] = { _("Centre"), _("Left"), _("Right"), _("Full width"), NULL };
    p->edge = combo(p, edges);
    p->align = combo(p, aligns);
    p->monitor = gtk_combo_box_text_new();
    g_signal_connect_swapped(p->monitor, "changed", G_CALLBACK(prefs_from_widgets), p);
    row(g, 0, _("_Edge:"), p->edge);
    row(g, 1, _("_Alignment:"), p->align);
    row(g, 2, _("_Monitor:"), p->monitor);
    p->reserve = check(p, _("Reserve screen _space (maximised windows stop above the dock)"));
    p->autohide = check(p, _("Auto-_hide (show when the pointer touches the edge)"));
    gtk_grid_attach(GTK_GRID(g), p->reserve, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(g), p->autohide, 0, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(outer), f, 0, 0, 1, 1);

    f = frame(_("Appearance"), &g);
    const char *styles[] = { _("Classic (OPENSTEP)"), _("Modern"), NULL };
    p->style = combo(p, styles);
    p->size = gtk_spin_button_new_with_range(BTD_ICON_SIZE_MIN, BTD_ICON_SIZE_MAX, 8);
    g_signal_connect_swapped(p->size, "value-changed", G_CALLBACK(prefs_from_widgets), p);
    row(g, 0, _("St_yle:"), p->style);
    row(g, 1, _("_Icon size:"), p->size);
    p->anim = check(p, _("Subtle a_nimations"));
    p->running = check(p, _("Show _running applications that are not in the tab"));
    gtk_grid_attach(GTK_GRID(g), p->anim, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(g), p->running, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(outer), f, 1, 0, 1, 1);

    f = frame(_("Behaviour"), &g);
    p->minimize = check(p, _("Clicking the active application _minimises it"));
    p->shortcuts = check(p, _("_Keyboard shortcuts: Super+Alt+1…9, Super+Alt+PgUp/PgDn (X11)"));
    gtk_grid_attach(GTK_GRID(g), p->minimize, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), p->shortcuts, 0, 1, 1, 1);
    GtkWidget *note = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(note),
                         _("<small>On Wayland, bind <tt>big-wm-tabbed-dock --tab next</tt> (or prev, 1…9) to keys in "
                           "the compositor configuration.</small>"));
    gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(note), 50);
    gtk_label_set_xalign(GTK_LABEL(note), 0);
    gtk_grid_attach(GTK_GRID(g), note, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(outer), f, 0, 1, 1, 1);

    f = frame(_("Docks in the session"), &g);
    const char *modes[] = { _("Classic Dock only"), _("Tabbed Dock only"), _("Both docks"), _("No dock"), NULL };
    BtdCoexistPaths paths;
    btd_coexist_paths_init(&paths);
    BtdDockMode current = btd_coexist_get_mode(&paths, dock->wayland);
    btd_coexist_paths_clear(&paths);
    GSList *group = NULL;
    p->updating = TRUE;
    for (int i = 0; modes[i]; i++) {
        p->mode[i] = gtk_radio_button_new_with_label(group, modes[i]);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(p->mode[i]));
        g_object_set_data(G_OBJECT(p->mode[i]), "mode", GINT_TO_POINTER(i));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->mode[i]), (int)current == i);
        g_signal_connect(p->mode[i], "toggled", G_CALLBACK(on_mode_toggled), p);
        gtk_grid_attach(GTK_GRID(g), p->mode[i], 0, i, 1, 1);
    }
    p->updating = FALSE;
    GtkWidget *mnote = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(mnote), _("<small>Applies at the next login. The classic Dock keeps its "
                                             "contents; nothing is deleted.</small>"));
    gtk_label_set_line_wrap(GTK_LABEL(mnote), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(mnote), 40);
    gtk_label_set_xalign(GTK_LABEL(mnote), 0);
    gtk_grid_attach(GTK_GRID(g), mnote, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(outer), f, 1, 1, 1, 1);

    widgets_from_prefs(p);
    gtk_widget_show_all(d);
    return d;
}
