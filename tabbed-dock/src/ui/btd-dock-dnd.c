/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Drag and drop. GTK implements the transport for each session: XDND on
 * X11 and wl_data_device on Wayland; this file only deals with targets.
 *
 *  - From a file manager (text/uri-list): adds shortcuts. The dock always
 *    answers COPY/LINK, never MOVE, so the source never deletes the file.
 *  - Files dropped on an application tile open with that application.
 *  - Items between tabs (drop on a tab header): move; with Ctrl: copy.
 *  - Items within the shelf: reorder. Tabs within the strip: reorder.
 *  - Items dragged out (to a file manager) export their file URI.
 */
#include "btd-dock.h"

#include <glib/gi18n.h>
#include <string.h>

enum { TARGET_URI_LIST, TARGET_ITEM, TARGET_TAB };

static const GtkTargetEntry dest_targets[] = {
    { "application/x-big-tabbed-dock-item", GTK_TARGET_SAME_APP, TARGET_ITEM },
    { "application/x-big-tabbed-dock-tab", GTK_TARGET_SAME_APP, TARGET_TAB },
    { "text/uri-list", 0, TARGET_URI_LIST },
};

#define SPRING_DELAY_MS 600
#define MAX_DROPPED 64

/* ---- classification ------------------------------------------------------ */

static gboolean
is_appimage(const char *path)
{
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR) || !g_file_test(path, G_FILE_TEST_IS_EXECUTABLE))
        return FALSE;
    const char *dot = strrchr(path, '.');
    if (dot && g_ascii_strcasecmp(dot, ".appimage") == 0)
        return TRUE;
    /* AppImage type 1/2 magic: "AI" + version at offset 8 of an ELF file */
    FILE *f = fopen(path, "rb");
    if (!f)
        return FALSE;
    unsigned char head[11] = { 0 };
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);
    return n == sizeof(head) && memcmp(head, "\x7f" "ELF", 4) == 0 && head[8] == 'A' && head[9] == 'I' &&
           (head[10] == 1 || head[10] == 2);
}

BtdItem *
btd_dock_item_from_path(BtdDock *dock, const char *path)
{
    if (!path || !g_path_is_absolute(path))
        return NULL;
    char *canon = g_canonicalize_filename(path, "/");
    BtdItem *item = NULL;
    if (g_str_has_suffix(canon, ".desktop") && g_file_test(canon, G_FILE_TEST_IS_REGULAR)) {
        const BtdDesktopEntry *e = btd_desktop_index_lookup_path(dock->index, canon);
        if (e) {
            item = btd_item_new(BTD_ITEM_APP, e->id);
        } else {
            BtdDesktopEntry *tmp = btd_desktop_entry_load(canon, NULL);
            if (tmp)
                item = btd_item_new(BTD_ITEM_APP, canon); /* launcher outside the data dirs */
            btd_desktop_entry_free(tmp);
        }
    } else if (g_file_test(canon, G_FILE_TEST_IS_DIR)) {
        item = btd_item_new(BTD_ITEM_FOLDER, canon);
    } else if (is_appimage(canon)) {
        item = btd_item_new(BTD_ITEM_APPIMAGE, canon);
    } else if (g_file_test(canon, G_FILE_TEST_EXISTS)) {
        item = btd_item_new(BTD_ITEM_FILE, canon);
    }
    g_free(canon);
    return item;
}

/* ---- geometry ------------------------------------------------------------ */

/* Insertion index in the active tab for a pointer x position. */
static int
insertion_index(BtdDock *dock, int x)
{
    BtdTab *tab = btd_model_get_active_tab(dock->model);
    int index = tab ? (int)tab->items->len : 0;
    for (guint i = 0; i < dock->slots->len; i++) {
        Slot *s = btd_dock_slot(dock, (int)i);
        if (s->kind == SLOT_ITEM && x < s->rect.x + s->rect.width / 2) {
            index = s->index;
            break;
        }
    }
    return index;
}

static Slot *
slot_kind_at(BtdDock *dock, int x, int y, SlotKind kind)
{
    for (guint i = 0; i < dock->slots->len; i++) {
        Slot *s = btd_dock_slot(dock, (int)i);
        if (s->kind == kind && x >= s->rect.x && x < s->rect.x + s->rect.width && y >= s->rect.y &&
            y < s->rect.y + s->rect.height)
            return s;
    }
    return NULL;
}

static void
clear_feedback(BtdDock *dock)
{
    dock->drop_index = dock->drop_tab = dock->drop_on_item = -1;
    if (dock->spring_id) {
        g_source_remove(dock->spring_id);
        dock->spring_id = 0;
    }
    gtk_widget_queue_draw(dock->area);
}

static gboolean
spring_open(gpointer data)
{
    BtdDock *dock = data;
    dock->spring_id = 0;
    BtdTab *tab = btd_model_get_tab(dock->model, (guint)dock->drop_tab);
    if (tab)
        btd_model_set_active_tab(dock->model, tab->id); /* spring-loaded tab */
    return G_SOURCE_REMOVE;
}

/* ---- destination --------------------------------------------------------- */

static gboolean
on_drag_motion(GtkWidget *w, GdkDragContext *ctx, int x, int y, guint time, gpointer data)
{
    BtdDock *dock = data;
    btd_dock_set_hidden(dock, FALSE);
    GdkAtom target = gtk_drag_dest_find_target(w, ctx, NULL);
    if (target == GDK_NONE) {
        gdk_drag_status(ctx, 0, time);
        return FALSE;
    }
    gboolean is_tab = target == gdk_atom_intern_static_string("application/x-big-tabbed-dock-tab");
    gboolean is_item = target == gdk_atom_intern_static_string("application/x-big-tabbed-dock-item");
    int old_index = dock->drop_index, old_tab = dock->drop_tab, old_on = dock->drop_on_item;
    dock->drop_index = dock->drop_on_item = -1;

    Slot *tab_slot = slot_kind_at(dock, x, y, SLOT_TAB);
    GdkDragAction action = GDK_ACTION_COPY;
    if (is_tab) {
        dock->drop_tab = tab_slot ? tab_slot->index : -1;
        action = GDK_ACTION_MOVE;
    } else if (tab_slot) {
        dock->drop_tab = tab_slot->index;
        if (dock->drop_tab != old_tab) {
            if (dock->spring_id)
                g_source_remove(dock->spring_id);
            dock->spring_id = g_timeout_add(SPRING_DELAY_MS, spring_open, dock);
        }
        if (is_item)
            action = (gdk_drag_context_get_actions(ctx) & GDK_ACTION_MOVE) &&
                             gdk_drag_context_get_suggested_action(ctx) != GDK_ACTION_COPY
                         ? GDK_ACTION_MOVE
                         : GDK_ACTION_COPY;
    } else {
        dock->drop_tab = -1;
        if (dock->spring_id) {
            g_source_remove(dock->spring_id);
            dock->spring_id = 0;
        }
        Slot *item_slot = slot_kind_at(dock, x, y, SLOT_ITEM);
        BtdTab *active = btd_model_get_active_tab(dock->model);
        BtdItem *target_item = item_slot ? active->items->pdata[item_slot->index] : NULL;
        int rel = item_slot ? x - item_slot->rect.x : 0;
        if (!is_item && target_item && target_item->type == BTD_ITEM_APP && rel > dock->tile / 4 &&
            rel < dock->tile * 3 / 4) {
            /* centre of an app tile: "open with" */
            dock->drop_on_item = item_slot->index;
            action = GDK_ACTION_COPY;
        } else {
            dock->drop_index = insertion_index(dock, x);
            action = is_item ? GDK_ACTION_MOVE : GDK_ACTION_COPY;
        }
    }
    /* External drops are never MOVE: the file manager must keep its file. */
    if (!is_item && !is_tab) {
        GdkDragAction offered = gdk_drag_context_get_actions(ctx);
        action = (offered & GDK_ACTION_COPY) ? GDK_ACTION_COPY : (offered & GDK_ACTION_LINK) ? GDK_ACTION_LINK : 0;
    }
    gdk_drag_status(ctx, action, time);
    if (old_index != dock->drop_index || old_tab != dock->drop_tab || old_on != dock->drop_on_item)
        gtk_widget_queue_draw(dock->area);
    return TRUE;
}

static void
on_drag_leave(GtkWidget *w, GdkDragContext *ctx, guint time, gpointer data)
{
    BtdDock *dock = data;
    /* drag-leave also precedes drag-drop; the feedback is cleared after it */
    dock->drop_index = dock->drop_on_item = -1;
    if (dock->spring_id) {
        g_source_remove(dock->spring_id);
        dock->spring_id = 0;
    }
    gtk_widget_queue_draw(dock->area);
}

static gboolean
on_drag_drop(GtkWidget *w, GdkDragContext *ctx, int x, int y, guint time, gpointer data)
{
    BtdDock *dock = data;
    GdkAtom target = gtk_drag_dest_find_target(w, ctx, NULL);
    if (target == GDK_NONE)
        return FALSE;
    /* recompute the drop location (drag-leave cleared it) */
    on_drag_motion(w, ctx, x, y, time, dock);
    gtk_drag_get_data(w, ctx, target, time);
    return TRUE;
}

static void
drop_uris(BtdDock *dock, char **uris, BtdTab *dest_tab, int position, int on_item)
{
    BtdTab *active = btd_model_get_active_tab(dock->model);
    if (on_item >= 0) {
        GList *list = NULL;
        for (int i = 0; uris[i] && i < MAX_DROPPED; i++)
            list = g_list_append(list, uris[i]);
        btd_dock_launch(dock, active->items->pdata[on_item], list, NULL);
        g_list_free(list);
        return;
    }
    int added = 0, skipped = 0;
    btd_model_freeze(dock->model);
    for (int i = 0; uris[i] && i < MAX_DROPPED; i++) {
        char *path = g_filename_from_uri(uris[i], NULL, NULL);
        BtdItem *item = path ? btd_dock_item_from_path(dock, path) : NULL;
        g_free(path);
        if (!item) {
            skipped++;
            continue;
        }
        if (btd_model_insert_item(dock->model, dest_tab->id, item, position))
            added++, position = position >= 0 ? position + 1 : position;
    }
    btd_model_thaw(dock->model);
    if (skipped && !added)
        btd_dock_error(dock, _("Nothing was added"),
                       _("Only local files, folders, applications (.desktop) and AppImages can be added."));
}

static void
on_drag_data_received(GtkWidget *w, GdkDragContext *ctx, int x, int y, GtkSelectionData *sel, guint info,
                      guint time, gpointer data)
{
    BtdDock *dock = data;
    gboolean ok = FALSE;
    gboolean delete_source = FALSE;
    BtdTab *dest = dock->drop_tab >= 0 ? btd_model_get_tab(dock->model, (guint)dock->drop_tab)
                                       : btd_model_get_active_tab(dock->model);
    int position = dock->drop_tab >= 0 ? -1 : dock->drop_index;
    int on_item = dock->drop_on_item;
    int drop_tab = dock->drop_tab;
    clear_feedback(dock);

    if (info == TARGET_URI_LIST) {
        char **uris = gtk_selection_data_get_uris(sel);
        if (uris && dest) {
            drop_uris(dock, uris, dest, position, on_item);
            ok = TRUE;
        }
        g_strfreev(uris);
    } else if (info == TARGET_ITEM && dock->drag_tab_id && dest) {
        gboolean copy = gdk_drag_context_get_selected_action(ctx) == GDK_ACTION_COPY;
        BtdTab *src = btd_model_find_tab(dock->model, dock->drag_tab_id, NULL);
        if (src && dock->drag_item_index >= 0) {
            ok = btd_model_move_item(dock->model, src->id, (guint)dock->drag_item_index, dest->id, position, copy);
            if (!ok && src != dest) {
                char *msg = g_strdup_printf(_("“%s” already has this item."), btd_dock_tab_label(dest));
                btd_dock_error(dock, _("Not added"), msg);
                g_free(msg);
            }
        } else if (dock->drag_item_index < 0 && dock->drag_tab_index >= 0) {
            /* a running (unpinned) app dropped into a tab: pin it */
            AppGroup *g = (guint)dock->drag_tab_index < dock->running->len ? dock->running->pdata[dock->drag_tab_index]
                                                                          : NULL;
            if (g && g->desktop_id)
                ok = btd_model_insert_item(dock->model, dest->id, btd_item_new(BTD_ITEM_APP, g->desktop_id), position);
        }
    } else if (info == TARGET_TAB && dock->drag_tab_id && drop_tab >= 0) {
        ok = btd_model_move_tab(dock->model, dock->drag_tab_id, (guint)drop_tab);
    }
    /* delete_source stays FALSE: the dock never asks a source to delete data */
    gtk_drag_finish(ctx, ok, delete_source, time);
}

/* ---- source -------------------------------------------------------------- */

static void
on_drag_data_get(GtkWidget *w, GdkDragContext *ctx, GtkSelectionData *sel, guint info, guint time, gpointer data)
{
    BtdDock *dock = data;
    if (info == TARGET_URI_LIST) {
        BtdTab *tab = btd_model_find_tab(dock->model, dock->drag_tab_id, NULL);
        if (!tab || dock->drag_item_index < 0 || (guint)dock->drag_item_index >= tab->items->len)
            return;
        BtdItem *it = tab->items->pdata[dock->drag_item_index];
        const char *path = NULL;
        char *desktop_path = NULL;
        if (it->type == BTD_ITEM_APP) {
            const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, btd_dock_item_desktop_id(dock, it));
            path = e ? e->path : (g_path_is_absolute(it->target) ? it->target : NULL);
        } else if (it->type != BTD_ITEM_SEPARATOR) {
            path = it->target;
        }
        if (!path) {
            g_free(desktop_path);
            return;
        }
        char *uri = g_filename_to_uri(path, NULL, NULL);
        char *uris[] = { uri, NULL };
        gtk_selection_data_set_uris(sel, uris);
        g_free(uri);
        g_free(desktop_path);
    } else {
        /* internal targets carry no data: the source state is in the dock */
        gtk_selection_data_set(sel, gtk_selection_data_get_target(sel), 8, (const guchar *)"1", 1);
    }
}

static void
on_drag_end(GtkWidget *w, GdkDragContext *ctx, gpointer data)
{
    BtdDock *dock = data;
    dock->drag_started = FALSE;
    dock->press_slot = -1;
    dock->menu_open = FALSE;
    g_clear_pointer(&dock->drag_tab_id, g_free);
    dock->drag_item_index = dock->drag_tab_index = -1;
    clear_feedback(dock);
    /* dropping outside the dock removes nothing: removal is explicit (menu) */
}

static void
on_drag_begin(GtkWidget *w, GdkDragContext *ctx, gpointer data)
{
    BtdDock *dock = data;
    dock->menu_open = TRUE; /* keep the dock visible while dragging */
}

void
btd_dock_dnd_begin(BtdDock *dock, int slot_index, const GdkEvent *event)
{
    Slot *s = btd_dock_slot(dock, slot_index);
    if (!s)
        return;
    GtkTargetList *targets = gtk_target_list_new(NULL, 0);
    BtdTab *active = btd_model_get_active_tab(dock->model);
    cairo_surface_t *icon = NULL;
    int scale = gtk_widget_get_scale_factor(dock->window);
    g_free(dock->drag_tab_id);
    dock->drag_tab_id = NULL;
    dock->drag_item_index = dock->drag_tab_index = -1;
    GdkDragAction actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;

    if (s->kind == SLOT_TAB) {
        BtdTab *tab = btd_model_get_tab(dock->model, (guint)s->index);
        dock->drag_tab_id = g_strdup(tab->id);
        gtk_target_list_add(targets, gdk_atom_intern_static_string("application/x-big-tabbed-dock-tab"),
                            GTK_TARGET_SAME_APP, TARGET_TAB);
        actions = GDK_ACTION_MOVE;
    } else if (s->kind == SLOT_ITEM) {
        BtdItem *it = active->items->pdata[s->index];
        dock->drag_tab_id = g_strdup(active->id);
        dock->drag_item_index = s->index;
        gtk_target_list_add(targets, gdk_atom_intern_static_string("application/x-big-tabbed-dock-item"),
                            GTK_TARGET_SAME_APP, TARGET_ITEM);
        if (it->type != BTD_ITEM_SEPARATOR) {
            gtk_target_list_add_uri_targets(targets, TARGET_URI_LIST);
            icon = btd_icon_cache_lookup_item(dock->icons, it, dock->index, dock->prefs.icon_size, scale);
        }
    } else if (s->kind == SLOT_RUNNING) {
        AppGroup *g = dock->running->pdata[s->index];
        if (!g->desktop_id) {
            gtk_target_list_unref(targets);
            return;
        }
        dock->drag_tab_index = s->index; /* reused as running-group index */
        dock->drag_tab_id = g_strdup(active->id);
        gtk_target_list_add(targets, gdk_atom_intern_static_string("application/x-big-tabbed-dock-item"),
                            GTK_TARGET_SAME_APP, TARGET_ITEM);
        const BtdDesktopEntry *e = btd_desktop_index_lookup(dock->index, g->desktop_id);
        icon = btd_icon_cache_lookup_name(dock->icons, e ? e->icon : NULL, dock->prefs.icon_size, scale);
        actions = GDK_ACTION_COPY;
    }
    GdkDragContext *ctx = gtk_drag_begin_with_coordinates(dock->area, targets, actions, GDK_BUTTON_PRIMARY,
                                                          (GdkEvent *)event, (int)dock->press_x, (int)dock->press_y);
    if (ctx && icon) {
        cairo_surface_reference(icon);
        gtk_drag_set_icon_surface(ctx, icon);
        cairo_surface_destroy(icon);
    }
    gtk_target_list_unref(targets);
}

void
btd_dock_dnd_init(BtdDock *dock)
{
    gtk_drag_dest_set(dock->area, 0, dest_targets, G_N_ELEMENTS(dest_targets),
                      GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    g_signal_connect(dock->area, "drag-motion", G_CALLBACK(on_drag_motion), dock);
    g_signal_connect(dock->area, "drag-leave", G_CALLBACK(on_drag_leave), dock);
    g_signal_connect(dock->area, "drag-drop", G_CALLBACK(on_drag_drop), dock);
    g_signal_connect(dock->area, "drag-data-received", G_CALLBACK(on_drag_data_received), dock);
    g_signal_connect(dock->area, "drag-data-get", G_CALLBACK(on_drag_data_get), dock);
    g_signal_connect(dock->area, "drag-begin", G_CALLBACK(on_drag_begin), dock);
    g_signal_connect(dock->area, "drag-end", G_CALLBACK(on_drag_end), dock);
}
