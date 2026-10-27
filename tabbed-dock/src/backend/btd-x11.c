/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * X11 window tracking through EWMH/ICCCM, as implemented by Window Maker
 * (src/wmspec.c): _NET_CLIENT_LIST, _NET_ACTIVE_WINDOW (source 2 = pager,
 * honoured across workspaces), _NET_WM_STATE_HIDDEN, _NET_CLOSE_WINDOW and
 * WM_CHANGE_STATE. No polling: everything comes from PropertyNotify.
 */
#include "btd-windows.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <unistd.h>

enum {
    A_NET_CLIENT_LIST,
    A_NET_ACTIVE_WINDOW,
    A_NET_WM_STATE,
    A_NET_WM_STATE_HIDDEN,
    A_NET_WM_STATE_SKIP_TASKBAR,
    A_NET_WM_WINDOW_TYPE,
    A_NET_WM_WINDOW_TYPE_NORMAL,
    A_NET_WM_WINDOW_TYPE_DIALOG,
    A_NET_WM_NAME,
    A_UTF8_STRING,
    A_WM_STATE,
    A_NET_WM_PID,
    A_GTK_APPLICATION_ID,
    A_NET_CLOSE_WINDOW,
    A_WM_CHANGE_STATE,
    A_COUNT
};

static const char *atom_names[A_COUNT] = {
    "_NET_CLIENT_LIST", "_NET_ACTIVE_WINDOW", "_NET_WM_STATE", "_NET_WM_STATE_HIDDEN",
    "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_DIALOG", "_NET_WM_NAME", "UTF8_STRING", "WM_STATE", "_NET_WM_PID",
    "_GTK_APPLICATION_ID", "_NET_CLOSE_WINDOW", "WM_CHANGE_STATE",
};

typedef struct {
    BtdWindowTracker base;
    GdkDisplay *gdk_display;
    Display *dpy;
    Window root;
    Atom atoms[A_COUNT];
    Window active;
    GHashTable *ignored; /* windows that are not taskbar candidates (Window -> 1) */
    BtdShortcutFunc shortcut_func;
    gpointer shortcut_data;
} X11Tracker;

static X11Tracker *the_tracker; /* the filter and shortcut grabs need it */

static gboolean
get_property(X11Tracker *x, Window w, Atom prop, Atom type, int format_want, unsigned char **data,
             unsigned long *n_items)
{
    Atom actual_type;
    int actual_format;
    unsigned long after;
    *data = NULL;
    *n_items = 0;
    gdk_x11_display_error_trap_push(x->gdk_display);
    int status = XGetWindowProperty(x->dpy, w, prop, 0, 1 << 16, False, type, &actual_type, &actual_format,
                                    n_items, &after, data);
    if (gdk_x11_display_error_trap_pop(x->gdk_display) || status != Success || !*data ||
        (type != AnyPropertyType && actual_type != type) || actual_format != format_want) {
        if (*data)
            XFree(*data);
        *data = NULL;
        *n_items = 0;
        return FALSE;
    }
    return TRUE;
}

static char *
get_utf8(X11Tracker *x, Window w, Atom prop)
{
    unsigned char *data;
    unsigned long n;
    if (!get_property(x, w, prop, x->atoms[A_UTF8_STRING], 8, &data, &n))
        return NULL;
    char *s = g_utf8_make_valid((const char *)data, (gssize)n);
    XFree(data);
    return s;
}

static gboolean
has_atom(X11Tracker *x, Window w, Atom prop, Atom value)
{
    unsigned char *data;
    unsigned long n;
    gboolean found = FALSE;
    if (get_property(x, w, prop, XA_ATOM, 32, &data, &n)) {
        Atom *atoms = (Atom *)data;
        for (unsigned long i = 0; i < n && !found; i++)
            found = atoms[i] == value;
        XFree(data);
    }
    return found;
}

/* A window belongs in the dock if it is a normal, non-transient,
 * non-skip-taskbar top-level that is not ours. */
static gboolean
is_candidate(X11Tracker *x, Window w)
{
    Window transient = None;
    gdk_x11_display_error_trap_push(x->gdk_display);
    gboolean is_transient = XGetTransientForHint(x->dpy, w, &transient) && transient != None;
    if (gdk_x11_display_error_trap_pop(x->gdk_display))
        return FALSE;
    if (is_transient)
        return FALSE;
    if (has_atom(x, w, x->atoms[A_NET_WM_STATE], x->atoms[A_NET_WM_STATE_SKIP_TASKBAR]))
        return FALSE;
    unsigned char *data;
    unsigned long n;
    if (get_property(x, w, x->atoms[A_NET_WM_WINDOW_TYPE], XA_ATOM, 32, &data, &n)) {
        Atom type = n > 0 ? ((Atom *)data)[0] : None;
        XFree(data);
        if (type != x->atoms[A_NET_WM_WINDOW_TYPE_NORMAL] && type != x->atoms[A_NET_WM_WINDOW_TYPE_DIALOG])
            return FALSE;
    }
    return TRUE;
}

static void
read_proc(BtdWindow *bw)
{
    if (bw->pid <= 0)
        return;
    char path[64];
    g_snprintf(path, sizeof(path), "/proc/%d/exe", bw->pid);
    char *target = g_file_read_link(path, NULL);
    if (target) {
        g_free(bw->exe);
        bw->exe = g_path_get_basename(target);
        g_free(target);
    }
    /* GIO_LAUNCHED_DESKTOP_FILE is inherited by grandchildren: only trust it
     * when GIO_LAUNCHED_DESKTOP_FILE_PID is this very process. */
    g_snprintf(path, sizeof(path), "/proc/%d/environ", bw->pid);
    char *env = NULL;
    gsize len = 0;
    if (!g_file_get_contents(path, &env, &len, NULL))
        return;
    const char *file = NULL, *pid = NULL;
    for (gsize off = 0; off < len;) {
        const char *entry = env + off;
        gsize elen = strnlen(entry, len - off);
        if (g_str_has_prefix(entry, "GIO_LAUNCHED_DESKTOP_FILE="))
            file = entry + strlen("GIO_LAUNCHED_DESKTOP_FILE=");
        else if (g_str_has_prefix(entry, "GIO_LAUNCHED_DESKTOP_FILE_PID="))
            pid = entry + strlen("GIO_LAUNCHED_DESKTOP_FILE_PID=");
        off += elen + 1;
    }
    if (file && pid && atoi(pid) == bw->pid)
        btd_window_set_string(&bw->launched_desktop_file, file, NULL);
    g_free(env);
}

static gboolean
update_state(X11Tracker *x, BtdWindow *bw)
{
    Window w = (Window)bw->id;
    gboolean minimized = has_atom(x, w, x->atoms[A_NET_WM_STATE], x->atoms[A_NET_WM_STATE_HIDDEN]);
    unsigned char *data;
    unsigned long n;
    if (!minimized && get_property(x, w, x->atoms[A_WM_STATE], x->atoms[A_WM_STATE], 32, &data, &n)) {
        minimized = n > 0 && ((long *)data)[0] == IconicState;
        XFree(data);
    }
    gboolean changed = minimized != bw->minimized;
    bw->minimized = minimized;
    return changed;
}

static gboolean
update_title(X11Tracker *x, BtdWindow *bw)
{
    gboolean changed = FALSE;
    char *title = get_utf8(x, (Window)bw->id, x->atoms[A_NET_WM_NAME]);
    if (!title) {
        char *name = NULL;
        gdk_x11_display_error_trap_push(x->gdk_display);
        if (XFetchName(x->dpy, (Window)bw->id, &name) && name) {
            title = g_utf8_make_valid(name, -1);
            XFree(name);
        }
        gdk_x11_display_error_trap_pop_ignored(x->gdk_display);
    }
    btd_window_set_string(&bw->title, title, &changed);
    g_free(title);
    return changed;
}

static BtdWindow *
add_window(X11Tracker *x, Window w)
{
    BtdWindow *bw = btd_window_new(&x->base, (guint64)w);
    gdk_x11_display_error_trap_push(x->gdk_display);
    XSelectInput(x->dpy, w, PropertyChangeMask);
    XClassHint hint = { 0 };
    if (XGetClassHint(x->dpy, w, &hint)) {
        bw->wm_instance = hint.res_name ? g_utf8_make_valid(hint.res_name, -1) : NULL;
        bw->wm_class = hint.res_class ? g_utf8_make_valid(hint.res_class, -1) : NULL;
        if (hint.res_name)
            XFree(hint.res_name);
        if (hint.res_class)
            XFree(hint.res_class);
    }
    gdk_x11_display_error_trap_pop_ignored(x->gdk_display);
    bw->app_id = get_utf8(x, w, x->atoms[A_GTK_APPLICATION_ID]);
    unsigned char *data;
    unsigned long n;
    if (get_property(x, w, x->atoms[A_NET_WM_PID], XA_CARDINAL, 32, &data, &n)) {
        bw->pid = n > 0 ? (int)((long *)data)[0] : 0;
        XFree(data);
    }
    read_proc(bw);
    update_title(x, bw);
    update_state(x, bw);
    bw->active = (Window)bw->id == x->active;
    return bw;
}

static void
read_active(X11Tracker *x)
{
    unsigned char *data;
    unsigned long n;
    Window active = None;
    if (get_property(x, x->root, x->atoms[A_NET_ACTIVE_WINDOW], XA_WINDOW, 32, &data, &n)) {
        active = n > 0 ? (Window)((long *)data)[0] : None;
        XFree(data);
    }
    if (active == x->active)
        return;
    x->active = active;
    GHashTableIter it;
    BtdWindow *bw;
    g_hash_table_iter_init(&it, x->base.windows);
    while (g_hash_table_iter_next(&it, NULL, (gpointer *)&bw))
        bw->active = (Window)bw->id == active;
    btd_window_tracker_queue_changed(&x->base);
}

static void
read_client_list(X11Tracker *x)
{
    unsigned char *data;
    unsigned long n = 0;
    Window *list = NULL;
    if (get_property(x, x->root, x->atoms[A_NET_CLIENT_LIST], XA_WINDOW, 32, &data, &n))
        list = (Window *)data;

    GHashTable *present = g_hash_table_new(g_int64_hash, g_int64_equal);
    guint64 *ids = g_new(guint64, n ? n : 1);
    gboolean changed = FALSE;
    for (unsigned long i = 0; i < n; i++) {
        ids[i] = (guint64)list[i];
        g_hash_table_add(present, &ids[i]);
        if (g_hash_table_contains(x->base.windows, &ids[i]) ||
            g_hash_table_contains(x->ignored, GSIZE_TO_POINTER(list[i])))
            continue;
        if (!is_candidate(x, list[i])) {
            g_hash_table_add(x->ignored, GSIZE_TO_POINTER(list[i]));
            /* still watch it: a dialog may later become a normal window */
            gdk_x11_display_error_trap_push(x->gdk_display);
            XSelectInput(x->dpy, list[i], PropertyChangeMask);
            gdk_x11_display_error_trap_pop_ignored(x->gdk_display);
            continue;
        }
        BtdWindow *bw = add_window(x, list[i]);
        /* nothing to show for an anonymous window (no class, id or title) */
        gboolean anonymous = !bw->wm_class && !bw->wm_instance && !bw->app_id && !bw->title;
        if (bw->pid == getpid() || anonymous) {
            g_hash_table_remove(x->base.windows, &bw->id); /* our own dialogs */
            g_hash_table_add(x->ignored, GSIZE_TO_POINTER(list[i]));
            continue;
        }
        changed = TRUE;
    }
    GHashTableIter it;
    BtdWindow *bw;
    g_hash_table_iter_init(&it, x->base.windows);
    while (g_hash_table_iter_next(&it, NULL, (gpointer *)&bw)) {
        if (!g_hash_table_contains(present, &bw->id)) {
            g_hash_table_iter_remove(&it);
            changed = TRUE;
        }
    }
    GHashTableIter ii;
    gpointer key;
    g_hash_table_iter_init(&ii, x->ignored);
    while (g_hash_table_iter_next(&ii, &key, NULL)) {
        guint64 id = GPOINTER_TO_SIZE(key);
        if (!g_hash_table_contains(present, &id))
            g_hash_table_iter_remove(&ii);
    }
    g_hash_table_unref(present);
    g_free(ids);
    if (list)
        XFree(list);
    if (changed)
        btd_window_tracker_queue_changed(&x->base);
}

static void
handle_shortcut(X11Tracker *x, XKeyEvent *ev)
{
    if (!x->shortcut_func)
        return;
    KeySym sym = XLookupKeysym(ev, 0);
    if (sym >= XK_1 && sym <= XK_9)
        x->shortcut_func((int)(sym - XK_1) + 1, x->shortcut_data);
    else if (sym == XK_Prior)
        x->shortcut_func(-1, x->shortcut_data);
    else if (sym == XK_Next)
        x->shortcut_func(-2, x->shortcut_data);
}

static GdkFilterReturn
event_filter(GdkXEvent *gxev, GdkEvent *event, gpointer data)
{
    X11Tracker *x = data;
    XEvent *ev = gxev;
    if (ev->type == KeyPress && ev->xkey.window == x->root) {
        handle_shortcut(x, &ev->xkey);
        return GDK_FILTER_REMOVE;
    }
    if (ev->type != PropertyNotify)
        return GDK_FILTER_CONTINUE;
    Window w = ev->xproperty.window;
    Atom a = ev->xproperty.atom;
    if (w == x->root) {
        if (a == x->atoms[A_NET_CLIENT_LIST])
            read_client_list(x);
        else if (a == x->atoms[A_NET_ACTIVE_WINDOW])
            read_active(x);
        return GDK_FILTER_CONTINUE;
    }
    guint64 id = (guint64)w;
    BtdWindow *bw = g_hash_table_lookup(x->base.windows, &id);
    if (!bw) {
        if (g_hash_table_contains(x->ignored, GSIZE_TO_POINTER(w)) &&
            (a == x->atoms[A_NET_WM_STATE] || a == x->atoms[A_NET_WM_WINDOW_TYPE] || a == XA_WM_TRANSIENT_FOR)) {
            g_hash_table_remove(x->ignored, GSIZE_TO_POINTER(w));
            read_client_list(x);
        }
        return GDK_FILTER_CONTINUE;
    }
    gboolean changed = FALSE;
    if (a == x->atoms[A_NET_WM_STATE] || a == x->atoms[A_WM_STATE]) {
        if (a == x->atoms[A_NET_WM_STATE] && !is_candidate(x, w)) {
            g_hash_table_remove(x->base.windows, &id);
            g_hash_table_add(x->ignored, GSIZE_TO_POINTER(w));
            changed = TRUE;
        } else {
            changed = update_state(x, bw);
        }
    } else if (a == x->atoms[A_NET_WM_NAME] || a == XA_WM_NAME) {
        changed = update_title(x, bw);
    } else if (a == XA_WM_CLASS || a == x->atoms[A_GTK_APPLICATION_ID]) {
        /* re-read identity; the dock re-resolves the application */
        g_hash_table_remove(x->base.windows, &id);
        read_client_list(x);
        changed = TRUE;
    }
    if (changed)
        btd_window_tracker_queue_changed(&x->base);
    return GDK_FILTER_CONTINUE;
}

static void
send_client_message(X11Tracker *x, Window w, Atom type, long l0, long l1, long l2)
{
    XEvent ev = { 0 };
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = type;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = l0;
    ev.xclient.data.l[1] = l1;
    ev.xclient.data.l[2] = l2;
    gdk_x11_display_error_trap_push(x->gdk_display);
    XSendEvent(x->dpy, x->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(x->dpy);
    gdk_x11_display_error_trap_pop_ignored(x->gdk_display);
}

static void
x11_activate(BtdWindowTracker *t, BtdWindow *w, guint32 time)
{
    X11Tracker *x = (X11Tracker *)t;
    /* source 2 (pager): Window Maker then also switches workspace / deiconifies */
    send_client_message(x, (Window)w->id, x->atoms[A_NET_ACTIVE_WINDOW], 2, time ? (long)time : CurrentTime, 0);
}

static void
x11_minimize(BtdWindowTracker *t, BtdWindow *w)
{
    X11Tracker *x = (X11Tracker *)t;
    gdk_x11_display_error_trap_push(x->gdk_display);
    XIconifyWindow(x->dpy, (Window)w->id, DefaultScreen(x->dpy));
    XFlush(x->dpy);
    gdk_x11_display_error_trap_pop_ignored(x->gdk_display);
}

static void
x11_close(BtdWindowTracker *t, BtdWindow *w)
{
    X11Tracker *x = (X11Tracker *)t;
    send_client_message(x, (Window)w->id, x->atoms[A_NET_CLOSE_WINDOW], CurrentTime, 2, 0);
}

static void
x11_destroy(BtdWindowTracker *t)
{
    X11Tracker *x = (X11Tracker *)t;
    gdk_window_remove_filter(NULL, event_filter, x);
    g_hash_table_unref(x->ignored);
    if (the_tracker == x)
        the_tracker = NULL;
}

BtdWindowTracker *
btd_x11_tracker_new(GdkDisplay *display)
{
    X11Tracker *x = g_new0(X11Tracker, 1);
    btd_window_tracker_init_base(&x->base, "x11",
                                 BTD_WIN_CAP_LIST | BTD_WIN_CAP_STATE | BTD_WIN_CAP_ACTIVATE |
                                     BTD_WIN_CAP_MINIMIZE | BTD_WIN_CAP_CLOSE);
    x->base.activate = x11_activate;
    x->base.minimize = x11_minimize;
    x->base.close = x11_close;
    x->base.destroy = x11_destroy;
    x->gdk_display = display;
    x->dpy = GDK_DISPLAY_XDISPLAY(display);
    x->root = DefaultRootWindow(x->dpy);
    x->ignored = g_hash_table_new(g_direct_hash, g_direct_equal);
    XInternAtoms(x->dpy, (char **)atom_names, A_COUNT, False, x->atoms);

    GdkWindow *root = gdk_get_default_root_window();
    gdk_window_set_events(root, gdk_window_get_events(root) | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(NULL, event_filter, x);
    the_tracker = x;
    read_active(x);
    read_client_list(x);
    return &x->base;
}

/* ---- global shortcuts --------------------------------------------------- */

static const KeySym shortcut_keys[] = { XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, XK_Prior, XK_Next };

static void
grab_variants(Display *dpy, Window root, KeyCode code, unsigned int mods, gboolean grab)
{
    /* NumLock (Mod2) and CapsLock must not break the shortcut. */
    unsigned int extra[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (guint i = 0; i < G_N_ELEMENTS(extra); i++) {
        if (grab)
            XGrabKey(dpy, code, mods | extra[i], root, True, GrabModeAsync, GrabModeAsync);
        else
            XUngrabKey(dpy, code, mods | extra[i], root);
    }
}

int
btd_x11_grab_shortcuts(GdkDisplay *display, BtdShortcutFunc func, gpointer data)
{
    if (!the_tracker)
        return 0;
    the_tracker->shortcut_func = func;
    the_tracker->shortcut_data = data;
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    Window root = DefaultRootWindow(dpy);
    int ok = 0;
    for (guint i = 0; i < G_N_ELEMENTS(shortcut_keys); i++) {
        KeyCode code = XKeysymToKeycode(dpy, shortcut_keys[i]);
        if (!code)
            continue;
        gdk_x11_display_error_trap_push(display);
        grab_variants(dpy, root, code, Mod4Mask | Mod1Mask, TRUE);
        XSync(dpy, False);
        if (gdk_x11_display_error_trap_pop(display) == 0)
            ok++;
        else
            g_message("Super+Alt+%s is grabbed by another client; not used", XKeysymToString(shortcut_keys[i]));
    }
    return ok;
}

void
btd_x11_ungrab_shortcuts(GdkDisplay *display)
{
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    Window root = DefaultRootWindow(dpy);
    gdk_x11_display_error_trap_push(display);
    for (guint i = 0; i < G_N_ELEMENTS(shortcut_keys); i++) {
        KeyCode code = XKeysymToKeycode(dpy, shortcut_keys[i]);
        if (code)
            grab_variants(dpy, root, code, Mod4Mask | Mod1Mask, FALSE);
    }
    gdk_x11_display_error_trap_pop_ignored(display);
    if (the_tracker)
        the_tracker->shortcut_func = NULL;
}
