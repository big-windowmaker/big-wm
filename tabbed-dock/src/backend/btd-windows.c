/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-windows.h"

#ifdef BTD_HAVE_X11
#include <gdk/gdkx.h>
#endif
#ifdef BTD_HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

static void
window_free(BtdWindow *w)
{
    g_free(w->app_id);
    g_free(w->wm_class);
    g_free(w->wm_instance);
    g_free(w->title);
    g_free(w->launched_desktop_file);
    g_free(w->exe);
    g_free(w->desktop_id);
    g_free(w);
}

void
btd_window_tracker_init_base(BtdWindowTracker *t, const char *name, guint caps)
{
    t->name = name;
    t->caps = caps;
    t->windows = g_hash_table_new_full(g_int64_hash, g_int64_equal, NULL, (GDestroyNotify)window_free);
}

BtdWindow *
btd_window_new(BtdWindowTracker *t, guint64 id)
{
    BtdWindow *w = g_new0(BtdWindow, 1);
    w->id = id;
    w->serial = ++t->next_serial;
    g_hash_table_insert(t->windows, &w->id, w);
    return w;
}

void
btd_window_set_string(char **field, const char *value, gboolean *changed)
{
    if (g_strcmp0(*field, value) == 0)
        return;
    g_free(*field);
    *field = g_strdup(value);
    if (changed)
        *changed = TRUE;
}

static gboolean
emit_idle(gpointer data)
{
    BtdWindowTracker *t = data;
    t->idle_id = 0;
    if (t->changed)
        t->changed(t, t->changed_data);
    return G_SOURCE_REMOVE;
}

void
btd_window_tracker_queue_changed(BtdWindowTracker *t)
{
    /* Coalesce bursts of X/Wayland events into one dock update. */
    if (!t->idle_id)
        t->idle_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, emit_idle, t, NULL);
}

void
btd_window_tracker_set_changed_func(BtdWindowTracker *t, BtdWindowsChangedFunc func, gpointer data)
{
    t->changed = func;
    t->changed_data = data;
}

static gint
by_serial(gconstpointer a, gconstpointer b)
{
    const BtdWindow *wa = a, *wb = b;
    return wa->serial < wb->serial ? -1 : wa->serial > wb->serial;
}

GList *
btd_window_tracker_list(BtdWindowTracker *t)
{
    return g_list_sort(g_hash_table_get_values(t->windows), by_serial);
}

static BtdWindowTracker *
none_tracker_new(void)
{
    BtdWindowTracker *t = g_new0(BtdWindowTracker, 1);
    btd_window_tracker_init_base(t, "none", 0);
    return t;
}

BtdWindowTracker *
btd_window_tracker_new(GdkDisplay *display)
{
    BtdWindowTracker *t = NULL;
#ifdef BTD_HAVE_X11
    if (GDK_IS_X11_DISPLAY(display))
        t = btd_x11_tracker_new(display);
#endif
#ifdef BTD_HAVE_WAYLAND
    if (!t && GDK_IS_WAYLAND_DISPLAY(display))
        t = btd_wayland_tracker_new(display);
#endif
    return t ? t : none_tracker_new();
}

void
btd_window_tracker_free(BtdWindowTracker *t)
{
    if (!t)
        return;
    t->changed = NULL;
    if (t->destroy)
        t->destroy(t);
    if (t->idle_id)
        g_source_remove(t->idle_id);
    g_hash_table_unref(t->windows);
    g_free(t);
}
