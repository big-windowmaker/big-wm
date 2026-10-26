/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — window tracking interface.
 *
 * Backends:
 *   x11      EWMH/ICCCM on the X root window (Window Maker): list, state,
 *            activate (_NET_ACTIVE_WINDOW), minimise (WM_CHANGE_STATE),
 *            close (_NET_CLOSE_WINDOW). Event driven (PropertyNotify).
 *   wlr      zwlr_foreign_toplevel_manager_v1 when the compositor has it:
 *            list, state, activate, minimise, close.
 *   ext      ext_foreign_toplevel_list_v1 (wlmaker): list only. Activation
 *            falls back to relaunching through GIO with an xdg-activation
 *            token, which single-instance applications honour.
 *   none     no tracking (unknown session).
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
    BTD_WIN_CAP_LIST = 1 << 0,
    BTD_WIN_CAP_STATE = 1 << 1,    /* knows active/minimised */
    BTD_WIN_CAP_ACTIVATE = 1 << 2,
    BTD_WIN_CAP_MINIMIZE = 1 << 3,
    BTD_WIN_CAP_CLOSE = 1 << 4,
} BtdWindowCaps;

typedef struct {
    guint64 id;
    guint64 serial;               /* creation order */
    char *app_id;                 /* Wayland app_id or _GTK_APPLICATION_ID */
    char *wm_class;
    char *wm_instance;
    char *title;
    char *launched_desktop_file;  /* GIO_LAUNCHED_DESKTOP_FILE of the client */
    char *exe;                    /* basename of /proc/<pid>/exe */
    int pid;
    gboolean active;
    gboolean minimized;
    gpointer handle;              /* backend data */
    char *desktop_id;             /* resolved by the dock, cached */
    gboolean resolved;
} BtdWindow;

typedef struct _BtdWindowTracker BtdWindowTracker;
typedef void (*BtdWindowsChangedFunc)(BtdWindowTracker *tracker, gpointer user_data);
typedef void (*BtdShortcutFunc)(int action, gpointer user_data); /* 1..9 tab, -1 prev, -2 next */

struct _BtdWindowTracker {
    const char *name;
    guint caps;
    GHashTable *windows; /* guint64* id -> BtdWindow* */
    guint64 next_serial;
    BtdWindowsChangedFunc changed;
    gpointer changed_data;
    guint idle_id;

    void (*activate)(BtdWindowTracker *t, BtdWindow *w, guint32 time);
    void (*minimize)(BtdWindowTracker *t, BtdWindow *w);
    void (*close)(BtdWindowTracker *t, BtdWindow *w);
    void (*destroy)(BtdWindowTracker *t);
};

/* Picks the backend for the GDK display in use. Never returns NULL. */
BtdWindowTracker *btd_window_tracker_new(GdkDisplay *display);
void btd_window_tracker_free(BtdWindowTracker *t);
void btd_window_tracker_set_changed_func(BtdWindowTracker *t, BtdWindowsChangedFunc func, gpointer data);

/* Sorted by creation order. Free the list (not the windows). */
GList *btd_window_tracker_list(BtdWindowTracker *t);

/* For backends. */
void btd_window_tracker_init_base(BtdWindowTracker *t, const char *name, guint caps);
BtdWindow *btd_window_new(BtdWindowTracker *t, guint64 id);
void btd_window_tracker_queue_changed(BtdWindowTracker *t);
void btd_window_set_string(char **field, const char *value, gboolean *changed);

#ifdef BTD_HAVE_X11
BtdWindowTracker *btd_x11_tracker_new(GdkDisplay *display);
/* Grabs Super+Alt+1..9 and Super+Alt+Page_Up/Page_Down on the root window.
 * Returns the number of grabs that succeeded (others belong to someone else). */
int btd_x11_grab_shortcuts(GdkDisplay *display, BtdShortcutFunc func, gpointer data);
void btd_x11_ungrab_shortcuts(GdkDisplay *display);
#endif
#ifdef BTD_HAVE_WAYLAND
BtdWindowTracker *btd_wayland_tracker_new(GdkDisplay *display);
/* Connector name of a monitor (xdg-output); NULL until known. */
const char *btd_wayland_monitor_name(GdkMonitor *monitor);
void btd_wayland_set_names_changed(void (*func)(gpointer), gpointer data);
#endif
/* Stable monitor name: RandR output on X11, xdg-output name on Wayland. */
const char *btd_monitor_name(GdkMonitor *monitor);

G_END_DECLS
