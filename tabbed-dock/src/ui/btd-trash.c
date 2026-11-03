/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — the trash, at the right end of the shelf.
 *
 * The FreeDesktop trash through GIO ("trash:///", gvfs), which counts the
 * trash of every mounted volume; without gvfs, the home trash directory.
 * Watched with a file monitor (inotify), no polling. Opening hands
 * trash:/// to the default file manager; emptying asks first and deletes
 * in a worker thread, so the dock never freezes on a large trash.
 */
#include "btd-trash.h"

#include <glib/gi18n.h>

#include "btd-dock.h"

struct _BtdTrash {
    BtdDock *dock;
    GFile *trash;          /* trash:/// or ~/.local/share/Trash/files */
    gboolean gvfs;
    GFileMonitor *monitor;
    GCancellable *cancel;
    guint count;
    gboolean emptying;
};

static void refresh(BtdTrash *t);

static void
count_done(GObject *source, GAsyncResult *res, gpointer data)
{
    BtdTrash *t = data;
    GError *err = NULL;
    GFileInfo *info = g_file_query_info_finish(G_FILE(source), res, &err);
    if (!info) {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_message("trash: %s", err->message);
        g_clear_error(&err);
        return;
    }
    guint count = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
    g_object_unref(info);
    if ((count > 0) != (t->count > 0) || count != t->count) {
        gboolean redraw = (count > 0) != (t->count > 0);
        t->count = count;
        if (redraw)
            btd_dock_queue_redraw(t->dock);
    }
}

/* Without gvfs: entries of ~/.local/share/Trash/files (only the first one
 * matters for "full"). */
static guint
count_directory(GFile *dir)
{
    GFileEnumerator *e = g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE,
                                                   NULL, NULL);
    guint n = 0;
    if (!e)
        return 0;
    GFileInfo *info;
    while ((info = g_file_enumerator_next_file(e, NULL, NULL)) != NULL) {
        n++;
        g_object_unref(info);
    }
    g_object_unref(e);
    return n;
}

static void
refresh(BtdTrash *t)
{
    if (t->gvfs) {
        g_file_query_info_async(t->trash, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT, G_FILE_QUERY_INFO_NONE,
                                G_PRIORITY_LOW, t->cancel, count_done, t);
    } else {
        guint count = count_directory(t->trash);
        gboolean redraw = (count > 0) != (t->count > 0);
        t->count = count;
        if (redraw)
            btd_dock_queue_redraw(t->dock);
    }
}

static void
on_changed(GFileMonitor *m, GFile *file, GFile *other, GFileMonitorEvent event, gpointer data)
{
    refresh(data);
}

BtdTrash *
btd_trash_new(BtdDock *dock)
{
    BtdTrash *t = g_new0(BtdTrash, 1);
    t->dock = dock;
    t->cancel = g_cancellable_new();
    GFile *uri = g_file_new_for_uri("trash:///");
    /* gvfs answers for trash:///; without it GIO gives a dummy file */
    GFileInfo *probe = g_file_query_info(uri, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (probe && g_file_info_has_attribute(probe, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT)) {
        t->gvfs = TRUE;
        t->trash = uri;
        t->count = g_file_info_get_attribute_uint32(probe, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
    } else {
        g_object_unref(uri);
        char *path = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
        t->trash = g_file_new_for_path(path);
        g_free(path);
        t->count = count_directory(t->trash);
    }
    g_clear_object(&probe);
    t->monitor = g_file_monitor_directory(t->trash, G_FILE_MONITOR_NONE, NULL, NULL);
    if (t->monitor)
        g_signal_connect(t->monitor, "changed", G_CALLBACK(on_changed), t);
    return t;
}

void
btd_trash_free(BtdTrash *t)
{
    if (!t)
        return;
    g_cancellable_cancel(t->cancel);
    if (t->monitor) {
        g_signal_handlers_disconnect_by_data(t->monitor, t);
        g_file_monitor_cancel(t->monitor);
        g_object_unref(t->monitor);
    }
    g_object_unref(t->cancel);
    g_object_unref(t->trash);
    g_free(t);
}

gboolean
btd_trash_is_full(BtdTrash *t)
{
    return t && t->count > 0;
}

guint
btd_trash_count(BtdTrash *t)
{
    return t ? t->count : 0;
}

void
btd_trash_open(BtdTrash *t, guint32 time)
{
    GError *err = NULL;
    GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    gdk_app_launch_context_set_timestamp(ctx, time);
    char *uri = t->gvfs ? g_strdup("trash:///") : g_file_get_uri(t->trash);
    if (!g_app_info_launch_default_for_uri(uri, G_APP_LAUNCH_CONTEXT(ctx), &err)) {
        btd_dock_error(t->dock, _("Could not open the trash"), err ? err->message : NULL);
        g_clear_error(&err);
    }
    g_free(uri);
    g_object_unref(ctx);
}

/* ---- emptying ------------------------------------------------------------- */

static gboolean
delete_tree(GFile *file, GCancellable *cancel, GError **error)
{
    /* trash:/// deletes a whole entry permanently; the home directory
     * fallback needs the contents of directories removed first */
    GFileEnumerator *e = g_file_enumerate_children(file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                   cancel, NULL);
    if (e) {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(e, cancel, NULL)) != NULL) {
            GFile *child = g_file_get_child(file, g_file_info_get_name(info));
            delete_tree(child, cancel, NULL);
            g_object_unref(child);
            g_object_unref(info);
        }
        g_object_unref(e);
    }
    return g_file_delete(file, cancel, error);
}

static void
empty_thread(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
    BtdTrash *t = data;
    GError *first = NULL;
    GFileEnumerator *e = g_file_enumerate_children(t->trash, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                   cancel, &first);
    if (e) {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(e, cancel, NULL)) != NULL) {
            GFile *child = g_file_get_child(t->trash, g_file_info_get_name(info));
            GError *err = NULL;
            gboolean ok = t->gvfs ? g_file_delete(child, cancel, &err) : delete_tree(child, cancel, &err);
            if (!ok && !first)
                first = err;
            else
                g_clear_error(&err);
            g_object_unref(child);
            g_object_unref(info);
        }
        g_object_unref(e);
        if (!t->gvfs) {
            /* the matching .trashinfo files */
            GFile *parent = g_file_get_parent(t->trash);
            GFile *info_dir = g_file_get_child(parent, "info");
            GFileEnumerator *ie = g_file_enumerate_children(info_dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                            G_FILE_QUERY_INFO_NONE, cancel, NULL);
            if (ie) {
                GFileInfo *ii;
                while ((ii = g_file_enumerator_next_file(ie, cancel, NULL)) != NULL) {
                    GFile *c = g_file_get_child(info_dir, g_file_info_get_name(ii));
                    g_file_delete(c, cancel, NULL);
                    g_object_unref(c);
                    g_object_unref(ii);
                }
                g_object_unref(ie);
            }
            g_object_unref(info_dir);
            g_object_unref(parent);
        }
    }
    if (first)
        g_task_return_error(task, first);
    else
        g_task_return_boolean(task, TRUE);
}

static void
empty_done(GObject *source, GAsyncResult *res, gpointer data)
{
    BtdTrash *t = data;
    GError *err = NULL;
    t->emptying = FALSE;
    if (!g_task_propagate_boolean(G_TASK(res), &err)) {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            btd_dock_error(t->dock, _("The trash could not be emptied completely"), err->message);
        g_clear_error(&err);
    }
    refresh(t);
}

void
btd_trash_empty(BtdTrash *t, GtkWindow *parent)
{
    if (!t || t->emptying || t->count == 0)
        return;
    GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
                                          "%s", _("Empty the trash?"));
    char *detail = g_strdup_printf(g_dngettext(GETTEXT_PACKAGE,
                                               "The %u item in the trash will be deleted permanently. "
                                               "This cannot be undone.",
                                               "The %u items in the trash will be deleted permanently. "
                                               "This cannot be undone.",
                                               t->count),
                                   t->count);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", detail);
    g_free(detail);
    gtk_dialog_add_button(GTK_DIALOG(d), _("_Cancel"), GTK_RESPONSE_CANCEL);
    GtkWidget *ok = gtk_dialog_add_button(GTK_DIALOG(d), _("_Empty Trash"), GTK_RESPONSE_ACCEPT);
    gtk_style_context_add_class(gtk_widget_get_style_context(ok), "destructive-action");
    /* the safe answer is the default one */
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    gtk_window_set_title(GTK_WINDOW(d), _("Trash"));
    int response = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (response != GTK_RESPONSE_ACCEPT)
        return;
    t->emptying = TRUE;
    GTask *task = g_task_new(NULL, t->cancel, empty_done, t);
    g_task_set_task_data(task, t, NULL);
    g_task_run_in_thread(task, empty_thread);
    g_object_unref(task);
}
