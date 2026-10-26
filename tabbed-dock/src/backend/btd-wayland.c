/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Wayland window tracking. Never uses X11.
 *
 * Preferred: zwlr_foreign_toplevel_manager_v1 (list + state + activate,
 * minimise, close). wlmaker does not implement it (checked against the
 * big-wm/protocols branch); there we use ext_foreign_toplevel_list_v1,
 * which only lists toplevels (app_id, title). The dock then reports the
 * missing capabilities and falls back to relaunching through GIO, with an
 * xdg-activation token from GTK, for "activate".
 *
 * The proxies live on GDK's default event queue, so GTK's own Wayland
 * event source dispatches them: no extra thread, no polling.
 */
#include "btd-windows.h"

#include <gdk/gdkwayland.h>
#include <string.h>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

typedef struct {
    BtdWindowTracker base;
    GdkDisplay *gdk_display;
    struct wl_display *display;
    struct wl_registry *registry;
    struct ext_foreign_toplevel_list_v1 *ext_list;
    struct zwlr_foreign_toplevel_manager_v1 *wlr_manager;
    guint32 ext_name;
    guint64 next_id;
} WlTracker;

typedef struct {
    WlTracker *tracker;
    guint64 id;
    struct ext_foreign_toplevel_handle_v1 *ext;
    struct zwlr_foreign_toplevel_handle_v1 *wlr;
    char *pending_title, *pending_app_id;
    gboolean pending_active, pending_minimized, has_state;
} Toplevel;

static BtdWindow *
toplevel_window(Toplevel *tl)
{
    return g_hash_table_lookup(tl->tracker->base.windows, &tl->id);
}

static void
toplevel_commit(Toplevel *tl)
{
    BtdWindow *w = toplevel_window(tl);
    gboolean changed = FALSE;
    if (!w) {
        w = btd_window_new(&tl->tracker->base, tl->id);
        w->handle = tl;
        changed = TRUE;
    }
    if (g_strcmp0(w->app_id, tl->pending_app_id) != 0) {
        w->resolved = FALSE; /* app_id changed: the dock re-resolves */
        btd_window_set_string(&w->app_id, tl->pending_app_id, &changed);
    }
    btd_window_set_string(&w->title, tl->pending_title, &changed);
    if (tl->has_state) {
        changed |= w->active != tl->pending_active || w->minimized != tl->pending_minimized;
        w->active = tl->pending_active;
        w->minimized = tl->pending_minimized;
    }
    if (changed)
        btd_window_tracker_queue_changed(&tl->tracker->base);
}

static void
toplevel_free(Toplevel *tl)
{
    g_hash_table_remove(tl->tracker->base.windows, &tl->id);
    btd_window_tracker_queue_changed(&tl->tracker->base);
    if (tl->ext)
        ext_foreign_toplevel_handle_v1_destroy(tl->ext);
    if (tl->wlr)
        zwlr_foreign_toplevel_handle_v1_destroy(tl->wlr);
    g_free(tl->pending_title);
    g_free(tl->pending_app_id);
    g_free(tl);
}

static void
set_pending(char **field, const char *value)
{
    g_free(*field);
    *field = value ? g_utf8_make_valid(value, -1) : NULL;
}

/* ---- ext-foreign-toplevel-list-v1 --------------------------------------- */

static void
ext_closed(void *data, struct ext_foreign_toplevel_handle_v1 *h)
{
    toplevel_free(data);
}

static void
ext_done(void *data, struct ext_foreign_toplevel_handle_v1 *h)
{
    toplevel_commit(data);
}

static void
ext_title(void *data, struct ext_foreign_toplevel_handle_v1 *h, const char *title)
{
    set_pending(&((Toplevel *)data)->pending_title, title);
}

static void
ext_app_id(void *data, struct ext_foreign_toplevel_handle_v1 *h, const char *app_id)
{
    set_pending(&((Toplevel *)data)->pending_app_id, app_id);
}

static void
ext_identifier(void *data, struct ext_foreign_toplevel_handle_v1 *h, const char *identifier)
{
}

static const struct ext_foreign_toplevel_handle_v1_listener ext_handle_listener = {
    .closed = ext_closed,
    .done = ext_done,
    .title = ext_title,
    .app_id = ext_app_id,
    .identifier = ext_identifier,
};

static void
ext_list_toplevel(void *data, struct ext_foreign_toplevel_list_v1 *list, struct ext_foreign_toplevel_handle_v1 *h)
{
    WlTracker *t = data;
    Toplevel *tl = g_new0(Toplevel, 1);
    tl->tracker = t;
    tl->id = ++t->next_id;
    tl->ext = h;
    ext_foreign_toplevel_handle_v1_add_listener(h, &ext_handle_listener, tl);
}

static void
ext_list_finished(void *data, struct ext_foreign_toplevel_list_v1 *list)
{
    WlTracker *t = data;
    ext_foreign_toplevel_list_v1_destroy(t->ext_list);
    t->ext_list = NULL;
}

static const struct ext_foreign_toplevel_list_v1_listener ext_list_listener = {
    .toplevel = ext_list_toplevel,
    .finished = ext_list_finished,
};

/* ---- wlr-foreign-toplevel-management-unstable-v1 ------------------------- */

static void
wlr_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *title)
{
    set_pending(&((Toplevel *)data)->pending_title, title);
}

static void
wlr_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *app_id)
{
    set_pending(&((Toplevel *)data)->pending_app_id, app_id);
}

static void
wlr_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o)
{
}

static void
wlr_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o)
{
}

static void
wlr_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_array *state)
{
    Toplevel *tl = data;
    tl->has_state = TRUE;
    tl->pending_active = FALSE;
    tl->pending_minimized = FALSE;
    guint32 *s;
    wl_array_for_each(s, state)
    {
        if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            tl->pending_active = TRUE;
        else if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            tl->pending_minimized = TRUE;
    }
}

static void
wlr_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    toplevel_commit(data);
}

static void
wlr_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    toplevel_free(data);
}

static void
wlr_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct zwlr_foreign_toplevel_handle_v1 *parent)
{
}

static const struct zwlr_foreign_toplevel_handle_v1_listener wlr_handle_listener = {
    .title = wlr_title,
    .app_id = wlr_app_id,
    .output_enter = wlr_output_enter,
    .output_leave = wlr_output_leave,
    .state = wlr_state,
    .done = wlr_done,
    .closed = wlr_closed,
    .parent = wlr_parent,
};

static void
wlr_manager_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *m, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    WlTracker *t = data;
    Toplevel *tl = g_new0(Toplevel, 1);
    tl->tracker = t;
    tl->id = ++t->next_id;
    tl->wlr = h;
    zwlr_foreign_toplevel_handle_v1_add_listener(h, &wlr_handle_listener, tl);
}

static void
wlr_manager_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *m)
{
    WlTracker *t = data;
    zwlr_foreign_toplevel_manager_v1_destroy(t->wlr_manager);
    t->wlr_manager = NULL;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener wlr_manager_listener = {
    .toplevel = wlr_manager_toplevel,
    .finished = wlr_manager_finished,
};

/* ---- registry ------------------------------------------------------------ */

static void
use_wlr(WlTracker *t, struct wl_registry *reg, guint32 name, guint32 version)
{
    t->wlr_manager = wl_registry_bind(reg, name, &zwlr_foreign_toplevel_manager_v1_interface, MIN(version, 3));
    zwlr_foreign_toplevel_manager_v1_add_listener(t->wlr_manager, &wlr_manager_listener, t);
    t->base.name = "wayland-wlr-foreign-toplevel";
    t->base.caps = BTD_WIN_CAP_LIST | BTD_WIN_CAP_STATE | BTD_WIN_CAP_ACTIVATE | BTD_WIN_CAP_MINIMIZE |
                   BTD_WIN_CAP_CLOSE;
    /* Drop the ext list if it was bound first: one source of truth. */
    if (t->ext_list) {
        ext_foreign_toplevel_list_v1_stop(t->ext_list);
        GHashTableIter it;
        BtdWindow *w;
        GPtrArray *doomed = g_ptr_array_new();
        g_hash_table_iter_init(&it, t->base.windows);
        while (g_hash_table_iter_next(&it, NULL, (gpointer *)&w))
            if (((Toplevel *)w->handle)->ext)
                g_ptr_array_add(doomed, w->handle);
        for (guint i = 0; i < doomed->len; i++)
            toplevel_free(doomed->pdata[i]);
        g_ptr_array_unref(doomed);
    }
}

static void
registry_global(void *data, struct wl_registry *reg, guint32 name, const char *iface, guint32 version)
{
    WlTracker *t = data;
    if (strcmp(iface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0 && !t->wlr_manager) {
        use_wlr(t, reg, name, version);
    } else if (strcmp(iface, ext_foreign_toplevel_list_v1_interface.name) == 0 && !t->ext_list &&
               !t->wlr_manager) {
        t->ext_list = wl_registry_bind(reg, name, &ext_foreign_toplevel_list_v1_interface, 1);
        t->ext_name = name;
        ext_foreign_toplevel_list_v1_add_listener(t->ext_list, &ext_list_listener, t);
        t->base.name = "wayland-ext-foreign-toplevel-list";
        t->base.caps = BTD_WIN_CAP_LIST;
    }
}

static void
registry_global_remove(void *data, struct wl_registry *reg, guint32 name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static struct wl_seat *
gdk_seat(WlTracker *t)
{
    GdkSeat *seat = gdk_display_get_default_seat(t->gdk_display);
    return seat ? gdk_wayland_seat_get_wl_seat(seat) : NULL;
}

static void
wl_activate(BtdWindowTracker *bt, BtdWindow *w, guint32 time)
{
    WlTracker *t = (WlTracker *)bt;
    Toplevel *tl = w->handle;
    struct wl_seat *seat = gdk_seat(t);
    if (tl && tl->wlr && seat) {
        if (w->minimized)
            zwlr_foreign_toplevel_handle_v1_unset_minimized(tl->wlr);
        zwlr_foreign_toplevel_handle_v1_activate(tl->wlr, seat);
        wl_display_flush(t->display);
    }
}

static void
wl_minimize(BtdWindowTracker *bt, BtdWindow *w)
{
    Toplevel *tl = w->handle;
    if (tl && tl->wlr) {
        zwlr_foreign_toplevel_handle_v1_set_minimized(tl->wlr);
        wl_display_flush(((WlTracker *)bt)->display);
    }
}

static void
wl_close(BtdWindowTracker *bt, BtdWindow *w)
{
    Toplevel *tl = w->handle;
    if (tl && tl->wlr) {
        zwlr_foreign_toplevel_handle_v1_close(tl->wlr);
        wl_display_flush(((WlTracker *)bt)->display);
    }
}

static void
wl_destroy(BtdWindowTracker *bt)
{
    WlTracker *t = (WlTracker *)bt;
    GList *handles = NULL;
    GHashTableIter it;
    BtdWindow *w;
    g_hash_table_iter_init(&it, t->base.windows);
    while (g_hash_table_iter_next(&it, NULL, (gpointer *)&w))
        handles = g_list_prepend(handles, w->handle);
    for (GList *l = handles; l; l = l->next)
        toplevel_free(l->data);
    g_list_free(handles);
    if (t->ext_list)
        ext_foreign_toplevel_list_v1_destroy(t->ext_list);
    if (t->wlr_manager)
        zwlr_foreign_toplevel_manager_v1_destroy(t->wlr_manager);
    if (t->registry)
        wl_registry_destroy(t->registry);
}

BtdWindowTracker *
btd_wayland_tracker_new(GdkDisplay *display)
{
    WlTracker *t = g_new0(WlTracker, 1);
    btd_window_tracker_init_base(&t->base, "wayland-none", 0);
    t->base.activate = wl_activate;
    t->base.minimize = wl_minimize;
    t->base.close = wl_close;
    t->base.destroy = wl_destroy;
    t->gdk_display = display;
    t->display = gdk_wayland_display_get_wl_display(display);
    t->registry = wl_display_get_registry(t->display);
    wl_registry_add_listener(t->registry, &registry_listener, t);
    /* Only the registry is round-tripped here, before the dock shows; the
     * toplevel events then arrive through GTK's dispatching. */
    wl_display_roundtrip(t->display);
    return &t->base;
}
