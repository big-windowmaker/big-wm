/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The notification center: opened only on purpose (the bell, a key,
 * big-wm-notifications --toggle). Being asked for, it may take the keyboard
 * while open (Escape, the search field); it closes on Escape, on a click
 * outside, on the bell or on its close button, and then gives everything
 * back. It never opens by itself.
 *
 *   X11:     a popup with a pointer+keyboard grab while open, as a menu.
 *   Wayland: a layer-shell panel (keyboard EXCLUSIVE while open) over a
 *            transparent full-screen catcher that turns a click outside
 *            into "close".
 */
#include "bwn.h"
#include <gtk-layer-shell.h>
#include <malloc.h>
#include <string.h>

#define CENTER_WIDTH 380

static GtkWidget *win, *catcher, *list, *empty, *search, *undo_bar, *undo_label, *dnd_switch,
    *scroll, *clear_btn;
static GList *undo_items;
static guint undo_id, read_id, rebuild_id, clock_id;
static gboolean building;

gboolean
bwn_center_visible(void)
{
    return win != NULL;
}

static void center_close(void);

static void rebuild(void);

/* == Rows =================================================================== */

/* Groups (one per application) whose entries are all shown. */
static GHashTable *expanded;

static void
on_row_delete(GtkButton *b, gpointer data)
{
    (void)b;
    guint32 id = GPOINTER_TO_UINT(data);
    BwnNotif *n = bwn_store_lookup(id);
    if (n && n->active)
        bwn_server_close(n, BWN_CLOSED_DISMISSED);
    bwn_store_remove(id);
}

static void
on_row_action(GtkButton *b, gpointer data)
{
    guint32 id = GPOINTER_TO_UINT(data);
    const char *key = g_object_get_data(G_OBJECT(b), "bwn-key");
    bwn_server_invoke(bwn_store_lookup(id), key, GTK_WIDGET(b));
}

static void
on_expand_text(GtkButton *b, gpointer data)
{
    GtkLabel *body = data;
    gboolean full = gtk_label_get_lines(body) == -1;
    gtk_label_set_lines(body, full ? 3 : -1);
    gtk_label_set_ellipsize(body, full ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
    gtk_button_set_label(b, full ? "⌄" : "⌃");
    gtk_widget_set_tooltip_text(GTK_WIDGET(b), full ? _("Show the whole text") : _("Show less"));
}

static void
toggle_group(const char *group)
{
    if (!expanded)
        expanded = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    if (!g_hash_table_remove(expanded, group))
        g_hash_table_add(expanded, g_strdup(group));
    rebuild();
}

static void
on_group_toggle(GtkButton *b, gpointer data)
{
    (void)data;
    toggle_group(g_object_get_data(G_OBJECT(b), "bwn-group"));
}

/* × on an application's group: all of its entries leave the history. */
static void
on_group_clear(GtkButton *b, gpointer data)
{
    (void)data;
    const char *group = g_object_get_data(G_OBJECT(b), "bwn-group");
    GArray *ids = g_array_new(FALSE, FALSE, sizeof(guint32));
    for (GList *l = bwn_store_list(); l; l = l->next) {
        g_autofree char *k = bwn_group_key(l->data);
        if (g_str_equal(k, group))
            g_array_append_val(ids, ((BwnNotif *)l->data)->id);
    }
    for (guint i = 0; i < ids->len; i++) {
        guint32 id = g_array_index(ids, guint32, i);
        BwnNotif *n = bwn_store_lookup(id);
        if (n && n->active)
            bwn_server_close(n, BWN_CLOSED_DISMISSED);
        bwn_store_remove(id);
    }
    g_array_unref(ids);
}

/* A click on an entry takes the user to it (as a click on its balloon) and
 * the entry leaves the center, as on GNOME. On a collapsed group: expands. */
static void
on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data)
{
    (void)box; (void)data;
    const char *group = g_object_get_data(G_OBJECT(row), "bwn-collapsed-group");
    if (group) {
        g_autofree char *g = g_strdup(group);
        toggle_group(g);
        return;
    }
    guint32 id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "bwn-id"));
    BwnNotif *n = id ? bwn_store_lookup(id) : NULL;
    if (!n)
        return;
    bwn_notif_ref(n);
    bwn_activate(n, GTK_WIDGET(row));
    bwn_store_remove(id);
    bwn_notif_unref(n);
    center_close(); /* the user goes to the application */
}

/* Whether the text takes more than three lines in the card's text column
 * (the panel's width less the avatar and the paddings). */
static gboolean
more_than_three_lines(GtkWidget *label, const char *markup, gboolean avatar)
{
    int width = CENTER_WIDTH - 4 - 20 - (avatar ? 32 + 10 : 0) - 12;
    PangoLayout *layout = gtk_widget_create_pango_layout(label, NULL);
    pango_layout_set_markup(layout, markup, -1);
    pango_layout_set_width(layout, MAX(width, 50) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    gboolean more = pango_layout_get_line_count(layout) > 3;
    g_object_unref(layout);
    return more;
}

static GtkWidget *
label_wrapped(const char *text, gboolean markup, int lines, const char *css)
{
    GtkWidget *l = gtk_label_new(NULL);
    if (markup)
        gtk_label_set_markup(GTK_LABEL(l), text);
    else
        gtk_label_set_text(GTK_LABEL(l), text);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
    if (lines > 0) {
        gtk_label_set_lines(GTK_LABEL(l), lines);
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    }
    gtk_label_set_max_width_chars(GTK_LABEL(l), 1);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), css);
    return l;
}

/* One entry: header line (the application's icon and the title, count of a
 * collapsed group, time, ×), the avatar and the text, the image below them,
 * the sender's buttons. stacked: entries of a
 * collapsed group (1: alone; 0: a member of an expanded group). */
static GtkWidget *
make_row(BwnNotif *n, gint64 now, int stacked, const char *group)
{
    GtkWidget *row = gtk_list_box_row_new();
    g_object_set_data(G_OBJECT(row), "bwn-id", GUINT_TO_POINTER(n->id));
    if (stacked > 1)
        g_object_set_data_full(G_OBJECT(row), "bwn-collapsed-group", g_strdup(group), g_free);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(h), "bwn-row");
    GtkWidget *mark = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (!n->read)
        gtk_style_context_add_class(gtk_widget_get_style_context(mark), "bwn-unread-mark");
    gtk_widget_set_size_request(mark, 4, -1);
    gtk_box_pack_start(GTK_BOX(h), mark, FALSE, FALSE, 0);

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(v), "bwn-content");
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_autofree char *app = bwn_app_label(n);
    GtkWidget *icon = bwn_app_icon_widget(n, 16);
    gtk_widget_set_tooltip_text(icon, app);
    gtk_box_pack_start(GTK_BOX(top), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), bwn_title_label(n), TRUE, TRUE, 0);
    if (stacked > 1) {
        gtk_box_pack_start(GTK_BOX(top), bwn_count_tag(stacked), FALSE, FALSE, 0);
        GtkWidget *open = gtk_button_new_with_label("⌄");
        gtk_style_context_add_class(gtk_widget_get_style_context(open), "bwn-chevron");
        gtk_widget_set_tooltip_text(open, _("Show all"));
        g_object_set_data_full(G_OBJECT(open), "bwn-group", g_strdup(group), g_free);
        g_signal_connect(open, "clicked", G_CALLBACK(on_group_toggle), NULL);
        gtk_box_pack_start(GTK_BOX(top), open, FALSE, FALSE, 0);
    }
    g_autofree char *rel = bwn_reltime(n->time, now);
    GtkWidget *time_l = gtk_label_new(rel);
    gtk_style_context_add_class(gtk_widget_get_style_context(time_l), "bwn-time");
    GDateTime *dt = g_date_time_new_from_unix_local(n->time / G_USEC_PER_SEC);
    g_autofree char *full = g_date_time_format(dt, "%c");
    g_date_time_unref(dt);
    gtk_widget_set_tooltip_text(time_l, full);
    g_object_set_data(G_OBJECT(row), "bwn-time", time_l);
    g_object_set_data_full(G_OBJECT(row), "bwn-when", g_memdup2(&n->time, sizeof n->time), g_free);
    gtk_box_pack_start(GTK_BOX(top), time_l, FALSE, FALSE, 0);
    GtkWidget *del = gtk_button_new_with_label("×");
    gtk_widget_set_tooltip_text(del, _("Remove from the history"));
    gtk_style_context_add_class(gtk_widget_get_style_context(del), "bwn-close");
    gtk_widget_set_focus_on_click(del, FALSE);
    g_signal_connect(del, "clicked", G_CALLBACK(on_row_delete), GUINT_TO_POINTER(n->id));
    gtk_box_pack_end(GTK_BOX(top), del, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), top, FALSE, FALSE, 0);

    GtkWidget *avatar = bwn_avatar_widget(n, 32);
    gboolean text = n->body && *n->body;
    if (avatar || text) {
        GtkWidget *body_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        if (avatar)
            gtk_box_pack_start(GTK_BOX(body_row), avatar, FALSE, FALSE, 0);
        if (text) {
            GtkWidget *texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            GtkWidget *b = label_wrapped(n->body, TRUE, 3, "bwn-body");
            gtk_box_pack_start(GTK_BOX(texts), b, FALSE, FALSE, 0);
            /* Long text: a button shows all of it (the click on the entry
             * is for going to the application). */
            if (more_than_three_lines(b, n->body, avatar != NULL)) {
                GtkWidget *more = gtk_button_new_with_label("⌄");
                gtk_style_context_add_class(gtk_widget_get_style_context(more), "bwn-chevron");
                gtk_widget_set_tooltip_text(more, _("Show the whole text"));
                gtk_widget_set_halign(more, GTK_ALIGN_START);
                g_signal_connect(more, "clicked", G_CALLBACK(on_expand_text), b);
                gtk_box_pack_start(GTK_BOX(texts), more, FALSE, FALSE, 0);
            }
            gtk_widget_set_valign(texts, avatar ? GTK_ALIGN_CENTER : GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(body_row), texts, TRUE, TRUE, 0);
        }
        gtk_box_pack_start(GTK_BOX(v), body_row, FALSE, FALSE, 0);
    }
    GtkWidget *picture = bwn_picture_widget(n, CENTER_WIDTH - 40, 160);
    if (picture)
        gtk_box_pack_start(GTK_BOX(v), picture, FALSE, FALSE, 2);

    if (n->active || n->gapp_id) {
        GtkWidget *acts = NULL;
        for (char **a = n->actions; a && a[0] && a[1]; a += 2) {
            if (g_str_equal(a[0], "default"))
                continue; /* the click on the entry */
            if (!acts)
                acts = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            GtkWidget *btn = gtk_button_new_with_label(a[1]);
            g_object_set_data_full(G_OBJECT(btn), "bwn-key", g_strdup(a[0]), g_free);
            g_signal_connect(btn, "clicked", G_CALLBACK(on_row_action), GUINT_TO_POINTER(n->id));
            gtk_box_pack_start(GTK_BOX(acts), btn, FALSE, FALSE, 0);
        }
        if (acts) {
            gtk_style_context_add_class(gtk_widget_get_style_context(acts), "bwn-row-actions");
            gtk_box_pack_start(GTK_BOX(v), acts, FALSE, FALSE, 2);
        }
    }
    gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), h, FALSE, FALSE, 0);
    if (stacked > 1) { /* the entries behind, as a stack of cards */
        GtkWidget *s1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(s1), "bwn-stack");
        gtk_box_pack_start(GTK_BOX(outer), s1, FALSE, FALSE, 0);
        if (stacked > 2) {
            GtkWidget *s2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_style_context_add_class(gtk_widget_get_style_context(s2), "bwn-stack2");
            gtk_box_pack_start(GTK_BOX(outer), s2, FALSE, FALSE, 0);
        }
    }
    gtk_container_add(GTK_CONTAINER(row), outer);
    gtk_widget_set_tooltip_text(row, stacked > 1 ? _("Click to show all") : _("Click to open"));

    AtkObject *acc = gtk_widget_get_accessible(row);
    g_autofree char *aname = g_strdup_printf("%s: %s", app, n->summary ? n->summary : "");
    atk_object_set_name(acc, aname);
    atk_object_set_description(acc, n->body_plain);
    return row;
}

/* The header of an expanded group: application, count, collapse, clear. */
static GtkWidget *
make_group_header(BwnNotif *n, int count, const char *group, gboolean searching)
{
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(h), "bwn-group-header");
    gtk_box_pack_start(GTK_BOX(h), bwn_app_icon_widget(n, 16), FALSE, FALSE, 0);
    g_autofree char *app = bwn_app_label(n);
    GtkWidget *l = gtk_label_new(app);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(h), l, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(h), bwn_count_tag(count), FALSE, FALSE, 0);
    if (!searching) { /* a search shows every match: nothing to fold */
        GtkWidget *fold = gtk_button_new_with_label("⌃");
        gtk_style_context_add_class(gtk_widget_get_style_context(fold), "bwn-chevron");
        gtk_widget_set_tooltip_text(fold, _("Show less"));
        g_object_set_data_full(G_OBJECT(fold), "bwn-group", g_strdup(group), g_free);
        g_signal_connect(fold, "clicked", G_CALLBACK(on_group_toggle), NULL);
        gtk_box_pack_start(GTK_BOX(h), fold, FALSE, FALSE, 0);
    }
    GtkWidget *clear = gtk_button_new_with_label("×");
    gtk_style_context_add_class(gtk_widget_get_style_context(clear), "bwn-close");
    gtk_widget_set_tooltip_text(clear, _("Remove these notifications"));
    g_object_set_data_full(G_OBJECT(clear), "bwn-group", g_strdup(group), g_free);
    g_signal_connect(clear, "clicked", G_CALLBACK(on_group_clear), NULL);
    gtk_box_pack_start(GTK_BOX(h), clear, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(row), h);
    return row;
}

/* Cards are built for the entries shown only: the newest PAGE (more on
 * request), among those that match the search (over the whole history). A
 * collapsed group is one card. */
#define PAGE 50
static int shown_limit = PAGE;

static gboolean
matches(BwnNotif *n, const char *needle)
{
    if (!needle || !*needle)
        return TRUE;
    g_autofree char *app = bwn_app_label(n);
    g_autofree char *hay = g_strjoin(" ", app, n->summary ? n->summary : "",
                                     n->body_plain ? n->body_plain : "", NULL);
    g_autofree char *folded = bwn_fold(hay);
    return strstr(folded, needle) != NULL;
}

static void
on_more(GtkButton *b, gpointer data)
{
    (void)b; (void)data;
    shown_limit += PAGE;
    rebuild();
}

typedef struct {
    char *key;
    GPtrArray *items; /* BwnNotif*, newest first */
} Group;

static void
group_free(Group *g)
{
    g_free(g->key);
    g_ptr_array_unref(g->items);
    g_free(g);
}

static void
rebuild(void)
{
    if (!win)
        return;
    building = TRUE;
    GList *children = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(l->data);
    g_list_free(children);
    gint64 now = g_get_real_time();
    const char *text = search ? gtk_entry_get_text(GTK_ENTRY(search)) : "";
    g_autofree char *needle = text && *text ? bwn_fold(text) : NULL;

    /* Groups by application, in the order of their newest entry. */
    GPtrArray *groups = g_ptr_array_new_with_free_func((GDestroyNotify)group_free);
    GHashTable *by_key = g_hash_table_new(g_str_hash, g_str_equal);
    int count = 0, matched = 0;
    for (GList *l = bwn_store_list(); l; l = l->next, count++) {
        if (!matches(l->data, needle))
            continue;
        matched++;
        char *key = bwn_group_key(l->data);
        Group *g = g_hash_table_lookup(by_key, key);
        if (!g) {
            g = g_new0(Group, 1);
            g->key = key;
            g->items = g_ptr_array_new();
            g_ptr_array_add(groups, g);
            g_hash_table_insert(by_key, g->key, g);
        } else {
            g_free(key);
        }
        g_ptr_array_add(g->items, l->data);
    }
    int shown = 0, hidden = 0;
    for (guint i = 0; i < groups->len; i++) {
        Group *g = groups->pdata[i];
        int n = g->items->len;
        gboolean open = n > 1 && (needle || (expanded && g_hash_table_contains(expanded, g->key)));
        if (!open) {
            if (shown >= shown_limit) {
                hidden++;
                continue;
            }
            gtk_list_box_insert(GTK_LIST_BOX(list), make_row(g->items->pdata[0], now, n, g->key), -1);
            shown++;
            continue;
        }
        if (shown >= shown_limit) {
            hidden += n;
            continue;
        }
        gtk_list_box_insert(GTK_LIST_BOX(list), make_group_header(g->items->pdata[0], n, g->key, needle != NULL),
                            -1);
        for (int k = 0; k < n; k++) {
            if (shown >= shown_limit) {
                hidden++;
                continue;
            }
            gtk_list_box_insert(GTK_LIST_BOX(list), make_row(g->items->pdata[k], now, 0, g->key), -1);
            shown++;
        }
    }
    g_hash_table_unref(by_key);
    g_ptr_array_unref(groups);
    if (hidden) {
        g_autofree char *label = g_strdup_printf(ngettext("Show %d more", "Show %d more", MIN(hidden, PAGE)),
                                                 MIN(hidden, PAGE));
        GtkWidget *more = gtk_button_new_with_label(label);
        gtk_style_context_add_class(gtk_widget_get_style_context(more), "bwn-row-actions");
        g_signal_connect(more, "clicked", G_CALLBACK(on_more), NULL);
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_container_add(GTK_CONTAINER(row), more);
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
    }
    gtk_widget_show_all(list);
    if (matched == 0) { /* no-show-all: its children are shown one by one */
        GList *kids = gtk_container_get_children(GTK_CONTAINER(empty));
        for (GList *k = kids; k; k = k->next)
            gtk_widget_show(k->data);
        g_list_free(kids);
    }
    gtk_widget_set_visible(empty, matched == 0);
    gtk_widget_set_visible(scroll, matched > 0);
    /* Wrapped labels do not propagate their height through the scrolled
     * window: the panel grows with the entries up to its limit. */
    int max_h = gtk_scrolled_window_get_max_content_height(GTK_SCROLLED_WINDOW(scroll));
    int need = 0;
    GList *rows = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *l = rows; l; l = l->next) {
        int h;
        gtk_widget_get_preferred_height_for_width(l->data, CENTER_WIDTH, NULL, &h);
        need += h;
    }
    g_list_free(rows);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), MIN(need, max_h));
    gtk_widget_set_sensitive(clear_btn, count > 0);
    gtk_switch_set_active(GTK_SWITCH(dnd_switch), bwn_settings()->dnd);
    building = FALSE;
}

static gboolean
rebuild_idle(gpointer data)
{
    (void)data;
    rebuild_id = 0;
    rebuild();
    return G_SOURCE_REMOVE;
}

static void
store_changed(gpointer data)
{
    (void)data;
    if (win && !rebuild_id)
        rebuild_id = g_idle_add(rebuild_idle, NULL);
}

static gboolean
tick_clock(gpointer data)
{
    (void)data;
    if (!win) {
        clock_id = 0;
        return G_SOURCE_REMOVE;
    }
    gint64 now = g_get_real_time();
    GList *children = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *l = children; l; l = l->next) {
        GtkLabel *t = g_object_get_data(l->data, "bwn-time");
        gint64 *when = g_object_get_data(l->data, "bwn-when");
        if (t && when) {
            g_autofree char *rel = bwn_reltime(*when, now);
            gtk_label_set_text(t, rel);
        }
    }
    g_list_free(children);
    return G_SOURCE_CONTINUE;
}

/* == Header ================================================================= */

static gboolean
on_dnd(GtkSwitch *sw, gboolean state, gpointer data)
{
    (void)sw; (void)data;
    if (!building)
        bwn_set_dnd(state);
    return FALSE;
}

static void
undo_drop(void)
{
    g_clear_handle_id(&undo_id, g_source_remove);
    g_list_free_full(g_steal_pointer(&undo_items), (GDestroyNotify)bwn_notif_unref);
    if (undo_bar)
        gtk_widget_hide(undo_bar);
}

static gboolean
undo_timeout(gpointer data)
{
    (void)data;
    undo_id = 0;
    undo_drop();
    return G_SOURCE_REMOVE;
}

/* "Clear" empties the history; the open ones are dismissed in the
 * protocol. Undo brings the entries back (as history). */
static void
on_clear(GtkButton *b, gpointer data)
{
    (void)b; (void)data;
    undo_drop();
    GList *all = bwn_store_take_all();
    for (GList *l = all; l; l = l->next) {
        BwnNotif *n = l->data;
        if (n->active)
            bwn_server_close(n, BWN_CLOSED_DISMISSED);
    }
    int count = g_list_length(all);
    undo_items = all;
    if (count) {
        g_autofree char *msg = g_strdup_printf(ngettext("%d notification cleared",
                                                        "%d notifications cleared", count), count);
        gtk_label_set_text(GTK_LABEL(undo_label), msg);
        /* no-show-all keeps show_all() away: show each one. */
        GList *kids = gtk_container_get_children(GTK_CONTAINER(undo_bar));
        for (GList *k = kids; k; k = k->next)
            gtk_widget_show(k->data);
        g_list_free(kids);
        gtk_widget_show(undo_bar);
        undo_id = g_timeout_add_seconds(6, undo_timeout, NULL);
    }
}

static void
on_undo(GtkButton *b, gpointer data)
{
    (void)b; (void)data;
    g_clear_handle_id(&undo_id, g_source_remove);
    bwn_store_restore(g_steal_pointer(&undo_items));
    gtk_widget_hide(undo_bar);
}

static void
on_search_changed(GtkSearchEntry *e, gpointer data)
{
    (void)e; (void)data;
    shown_limit = PAGE;
    rebuild();
}

static gboolean
mark_read(gpointer data)
{
    (void)data;
    read_id = 0;
    bwn_store_mark_all_read();
    return G_SOURCE_REMOVE;
}

/* == Window ================================================================= */

static gboolean
pump(void)
{
    bwn_wl_activation_pump();
    return FALSE;
}

static gboolean
on_key(GtkWidget *w, GdkEventKey *e, gpointer data)
{
    (void)w; (void)data;
    if (e->keyval == GDK_KEY_Escape) {
        center_close();
        return TRUE;
    }
    return FALSE;
}

/* X11: with the grab, a press outside the panel arrives here. */
static gboolean
on_press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)data;
    GtkAllocation a;
    gtk_widget_get_allocation(w, &a);
    if (e->window == gtk_widget_get_window(w) &&
        (e->x < 0 || e->y < 0 || e->x >= a.width || e->y >= a.height)) {
        center_close();
        return TRUE;
    }
    return FALSE;
}

static gboolean
on_catcher_press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w; (void)e; (void)data;
    center_close();
    return TRUE;
}

static gboolean
on_grab_broken(GtkWidget *w, GdkEvent *e, gpointer data)
{
    (void)w; (void)e; (void)data;
    center_close();
    return FALSE;
}

static void
center_close(void)
{
    if (!win)
        return;
    g_clear_handle_id(&read_id, g_source_remove);
    g_clear_handle_id(&rebuild_id, g_source_remove);
    g_clear_handle_id(&clock_id, g_source_remove);
    undo_drop();
    if (!bwn_wayland())
        gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default()));
    GtkWidget *w = g_steal_pointer(&win);
    gtk_widget_destroy(w); /* nothing is kept in memory while closed */
    if (catcher)
        gtk_widget_destroy(g_steal_pointer(&catcher));
    list = empty = search = undo_bar = undo_label = dnd_switch = scroll = clear_btn = NULL;
    shown_limit = PAGE;
    if (expanded)
        g_hash_table_remove_all(expanded);
    bwn_sni_update();
    malloc_trim(0); /* the panel's widgets go back to the system */
}

static GtkWidget *
build(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "bwn-center-header");
    GtkWidget *title = gtk_label_new(_("Notifications"));
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "bwn-center-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    GtkWidget *dnd_l = gtk_label_new(_("Do not disturb"));
    gtk_box_pack_start(GTK_BOX(header), dnd_l, FALSE, FALSE, 0);
    dnd_switch = gtk_switch_new();
    gtk_widget_set_valign(dnd_switch, GTK_ALIGN_CENTER);
    atk_object_set_name(gtk_widget_get_accessible(dnd_switch), _("Do not disturb"));
    g_signal_connect(dnd_switch, "state-set", G_CALLBACK(on_dnd), NULL);
    gtk_box_pack_start(GTK_BOX(header), dnd_switch, FALSE, FALSE, 0);
    clear_btn = gtk_button_new_with_label(_("Clear"));
    gtk_widget_set_tooltip_text(clear_btn, _("Remove all notifications from the history"));
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear), NULL);
    gtk_box_pack_start(GTK_BOX(header), clear_btn, FALSE, FALSE, 0);
    GtkWidget *close = gtk_button_new_with_label("×");
    gtk_style_context_add_class(gtk_widget_get_style_context(close), "bwn-close");
    gtk_widget_set_tooltip_text(close, _("Close"));
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(center_close), NULL);
    gtk_box_pack_start(GTK_BOX(header), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);

    search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search), _("Search notifications"));
    gtk_widget_set_margin_start(search, 8);
    gtk_widget_set_margin_end(search, 8);
    gtk_widget_set_margin_top(search, 6);
    gtk_widget_set_margin_bottom(search, 6);
    g_signal_connect(search, "search-changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_pack_start(GTK_BOX(box), search, FALSE, FALSE, 0);

    undo_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(undo_bar), "bwn-undo");
    undo_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(undo_label), 0);
    gtk_box_pack_start(GTK_BOX(undo_bar), undo_label, TRUE, TRUE, 0);
    GtkWidget *undo = gtk_button_new_with_label(_("Undo"));
    g_signal_connect(undo, "clicked", G_CALLBACK(on_undo), NULL);
    gtk_box_pack_start(GTK_BOX(undo_bar), undo, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(undo_bar, TRUE);
    gtk_box_pack_start(GTK_BOX(box), undo_bar, FALSE, FALSE, 0);

    list = gtk_list_box_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(list), "bwn-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), TRUE);
    g_signal_connect(list, "row-activated", G_CALLBACK(on_row_activated), NULL);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(scroll), "bwn-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    GdkDisplay *d = gdk_display_get_default();
    GdkMonitor *m = gdk_display_get_primary_monitor(d);
    if (!m)
        m = gdk_display_get_monitor(d, 0);
    GdkRectangle geo = { .height = 800 };
    if (m)
        gdk_monitor_get_geometry(m, &geo);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), geo.height * 6 / 10);
    gtk_container_add(GTK_CONTAINER(scroll), list);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(empty), "bwn-empty");
    GtkWidget *ei = gtk_image_new_from_icon_name("preferences-system-notifications", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(empty), ei, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty), gtk_label_new(_("No notifications")), FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(empty, TRUE);
    gtk_box_pack_start(GTK_BOX(box), empty, FALSE, FALSE, 0);
    return box;
}

static void
center_open(void)
{
    BwnSettings *s = bwn_settings();
    bwn_toast_hide_all(); /* all of them are in the panel */
    gboolean layer = bwn_wayland() && gtk_layer_is_supported();
    if (layer) {
        /* The catcher first, below the panel: a click outside closes. */
        catcher = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_layer_init_for_window(GTK_WINDOW(catcher));
        gtk_layer_set_namespace(GTK_WINDOW(catcher), "big-wm-notification-center-catcher");
        gtk_layer_set_layer(GTK_WINDOW(catcher), GTK_LAYER_SHELL_LAYER_TOP);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(catcher), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(catcher), -1);
        for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++)
            gtk_layer_set_anchor(GTK_WINDOW(catcher), e, TRUE);
        GdkVisual *rgba = gdk_screen_get_rgba_visual(gdk_screen_get_default());
        if (rgba)
            gtk_widget_set_visual(catcher, rgba);
        gtk_widget_set_app_paintable(catcher, TRUE);
        gtk_style_context_add_class(gtk_widget_get_style_context(catcher), "bwn-catcher");
        gtk_widget_add_events(catcher, GDK_BUTTON_PRESS_MASK);
        g_signal_connect(catcher, "button-press-event", G_CALLBACK(on_catcher_press), NULL);
        gtk_widget_show(catcher);

        win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_layer_init_for_window(GTK_WINDOW(win));
        gtk_layer_set_namespace(GTK_WINDOW(win), "big-wm-notification-center");
        gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
        gboolean right = g_str_has_suffix(s->position, "right");
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(win), right ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, s->margin_y);
        gtk_layer_set_margin(GTK_WINDOW(win), right ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_LEFT,
                             s->margin_x);
    } else {
        win = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    }
    gtk_window_set_title(GTK_WINDOW(win), _("Notifications"));
    gtk_widget_set_size_request(win, CENTER_WIDTH, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "bwn-center");
    atk_object_set_role(gtk_widget_get_accessible(win), ATK_ROLE_PANEL);
    gtk_container_add(GTK_CONTAINER(win), build());
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect_swapped(win, "event", G_CALLBACK(pump), NULL);
    rebuild();
    gtk_widget_show_all(win);
    gtk_widget_set_visible(empty, bwn_store_list() == NULL);
    gtk_widget_set_visible(scroll, bwn_store_list() != NULL);

    if (!layer) {
        GdkDisplay *d = gdk_display_get_default();
        GdkDevice *ptr = gdk_seat_get_pointer(gdk_display_get_default_seat(d));
        int px = 0, py = 0;
        if (ptr)
            gdk_device_get_position(ptr, NULL, &px, &py);
        GdkRectangle geo;
        gdk_monitor_get_geometry(gdk_display_get_monitor_at_point(d, px, py), &geo);
        gboolean right = g_str_has_suffix(s->position, "right");
        gtk_window_move(GTK_WINDOW(win),
                        right ? geo.x + geo.width - s->margin_x - CENTER_WIDTH : geo.x + s->margin_x,
                        geo.y + s->margin_y);
        gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
        g_signal_connect(win, "button-press-event", G_CALLBACK(on_press), NULL);
        g_signal_connect(win, "grab-broken-event", G_CALLBACK(on_grab_broken), NULL);
        gdk_seat_grab(gdk_display_get_default_seat(d), gtk_widget_get_window(win), GDK_SEAT_CAPABILITY_ALL,
                      TRUE, NULL, NULL, NULL, NULL);
    }
    /* Seen: the unread marks go after a moment with the panel open. */
    read_id = g_timeout_add(1500, mark_read, NULL);
    clock_id = g_timeout_add_seconds(30, tick_clock, NULL);
    bwn_sni_update();
}

void
bwn_center_toggle(void)
{
    static gboolean connected;
    if (!connected) {
        bwn_store_connect_changed(store_changed, NULL);
        connected = TRUE;
    }
    if (win)
        center_close();
    else
        center_open();
}
