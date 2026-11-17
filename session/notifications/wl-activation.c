/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Wayland: an xdg-activation token for the application whose action was
 * clicked (ActivationToken, notification specification 1.2). The compositor
 * honors only tokens that stem from user input on a focused surface (wlmaker
 * 0159): the token carries the serial of this very click on the toast.
 *
 * Our own wl_seat/wl_pointer objects on a private event queue see the same
 * button events GTK sees on our surfaces; their serials are valid for us.
 */
#include "bwn.h"
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"

static struct wl_display *display;
static struct wl_event_queue *queue;
static struct xdg_activation_v1 *activation;
static struct wl_seat *seat;
static struct wl_pointer *pointer;
static struct wl_keyboard *keyboard;
static uint32_t last_serial;
static struct wl_registry *registry;
static uint32_t toplevel_global, toplevel_version;
static char *token_done;

static void
p_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
    (void)d; (void)p; (void)s; (void)x; (void)y;
    last_serial = serial;
}
static void
p_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
    (void)d; (void)p; (void)serial; (void)s;
}
static void
p_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{
    (void)d; (void)p; (void)t; (void)x; (void)y;
}
static void
p_button(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t b, uint32_t state)
{
    (void)d; (void)p; (void)t; (void)b; (void)state;
    last_serial = serial;
}
static void
p_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{
    (void)d; (void)p; (void)t; (void)a; (void)v;
}
static void p_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void p_axis_source(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void p_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { (void)d; (void)p; (void)t; (void)a; }
static void p_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v) { (void)d; (void)p; (void)a; (void)v; }

static const struct wl_pointer_listener pointer_listener = {
    .enter = p_enter, .leave = p_leave, .motion = p_motion, .button = p_button, .axis = p_axis,
    .frame = p_frame, .axis_source = p_axis_source, .axis_stop = p_axis_stop,
    .axis_discrete = p_axis_discrete,
};

static void
k_keymap(void *d, struct wl_keyboard *k, uint32_t f, int32_t fd, uint32_t size)
{
    (void)d; (void)k; (void)f; (void)size;
    close(fd);
}
static void
k_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys)
{
    (void)d; (void)k; (void)s; (void)keys;
    last_serial = serial;
}
static void
k_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s)
{
    (void)d; (void)k; (void)serial; (void)s;
}
static void
k_key(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t t, uint32_t key, uint32_t state)
{
    (void)d; (void)k; (void)t; (void)key; (void)state;
    last_serial = serial;
}
static void
k_mods(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t a, uint32_t b, uint32_t c, uint32_t g)
{
    (void)d; (void)k; (void)serial; (void)a; (void)b; (void)c; (void)g;
}
static void
k_repeat(void *d, struct wl_keyboard *k, int32_t rate, int32_t delay)
{
    (void)d; (void)k; (void)rate; (void)delay;
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = k_keymap, .enter = k_enter, .leave = k_leave, .key = k_key,
    .modifiers = k_mods, .repeat_info = k_repeat,
};

static void
seat_caps(void *d, struct wl_seat *s, uint32_t caps)
{
    (void)d;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
        pointer = wl_seat_get_pointer(s);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
        keyboard = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    }
}
static void seat_name(void *d, struct wl_seat *s, const char *n) { (void)d; (void)s; (void)n; }
static const struct wl_seat_listener seat_listener = { seat_caps, seat_name };

static void
reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver)
{
    (void)d;
    if (!strcmp(iface, xdg_activation_v1_interface.name))
        activation = wl_registry_bind(r, name, &xdg_activation_v1_interface, 1);
    else if (!strcmp(iface, zwlr_foreign_toplevel_manager_v1_interface.name)) {
        /* Bound only for a click (see bwn_wl_raise): no events meanwhile. */
        toplevel_global = name;
        toplevel_version = MIN(ver, 3u);
    }
    else if (!strcmp(iface, wl_seat_interface.name) && !seat) {
        seat = wl_registry_bind(r, name, &wl_seat_interface, MIN(ver, 5u));
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static void
token_done_cb(void *d, struct xdg_activation_token_v1 *t, const char *token)
{
    (void)d; (void)t;
    g_free(token_done);
    token_done = g_strdup(token);
}
static const struct xdg_activation_token_v1_listener token_listener = { token_done_cb };

void
bwn_wl_activation_init(void)
{
    if (!bwn_wayland())
        return;
    display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    queue = wl_display_create_queue(display);
    struct wl_display *wrapper = wl_proxy_create_wrapper(display);
    wl_proxy_set_queue((struct wl_proxy *)wrapper, queue);
    registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    wl_registry_add_listener(registry, &reg_listener, NULL);
    wl_display_roundtrip_queue(display, queue);
    wl_display_roundtrip_queue(display, queue);
}

/* == Raising a window (wlr-foreign-toplevel) =============================== */

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    char *app_id;
    gboolean activated;
} Top;

static GPtrArray *tops;

static void
t_title(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, const char *t)
{
    (void)d; (void)h; (void)t;
}
static void
t_app_id(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, const char *id)
{
    (void)h;
    Top *t = d;
    g_free(t->app_id);
    t->app_id = g_strdup(id);
}
static void
t_output_enter(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o)
{
    (void)d; (void)h; (void)o;
}
static void
t_output_leave(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o)
{
    (void)d; (void)h; (void)o;
}
static void
t_state(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_array *state)
{
    (void)h;
    Top *t = d;
    t->activated = FALSE;
    uint32_t *s;
    wl_array_for_each(s, state) if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) t->activated = TRUE;
}
static void t_done(void *d, struct zwlr_foreign_toplevel_handle_v1 *h) { (void)d; (void)h; }
static void
t_closed(void *d, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    (void)h;
    Top *t = d;
    g_clear_pointer(&t->app_id, g_free);
}
static void
t_parent(void *d, struct zwlr_foreign_toplevel_handle_v1 *h, struct zwlr_foreign_toplevel_handle_v1 *p)
{
    (void)d; (void)h; (void)p;
}
static const struct zwlr_foreign_toplevel_handle_v1_listener top_listener = {
    .title = t_title, .app_id = t_app_id, .output_enter = t_output_enter,
    .output_leave = t_output_leave, .state = t_state, .done = t_done, .closed = t_closed,
    .parent = t_parent,
};

static void
m_toplevel(void *d, struct zwlr_foreign_toplevel_manager_v1 *m, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    (void)d; (void)m;
    Top *t = g_new0(Top, 1);
    t->handle = h;
    zwlr_foreign_toplevel_handle_v1_add_listener(h, &top_listener, t);
    g_ptr_array_add(tops, t);
}
static void m_finished(void *d, struct zwlr_foreign_toplevel_manager_v1 *m) { (void)d; (void)m; }
static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = m_toplevel, .finished = m_finished,
};

static void
top_free(Top *t)
{
    zwlr_foreign_toplevel_handle_v1_destroy(t->handle);
    g_free(t->app_id);
    g_free(t);
}

/* Activates the newest window whose app_id names the application (the
 * compositor raises it and gives it the keyboard; a minimized one comes
 * back). The manager is bound for this click only. */
gboolean
bwn_wl_raise(char **keys)
{
    if (!registry || !toplevel_global || !seat || !keys)
        return FALSE;
    struct zwlr_foreign_toplevel_manager_v1 *mgr =
        wl_registry_bind(registry, toplevel_global, &zwlr_foreign_toplevel_manager_v1_interface,
                         toplevel_version);
    tops = g_ptr_array_new_with_free_func((GDestroyNotify)top_free);
    zwlr_foreign_toplevel_manager_v1_add_listener(mgr, &manager_listener, NULL);
    wl_display_roundtrip_queue(display, queue); /* the toplevels */
    wl_display_roundtrip_queue(display, queue); /* their app_id, state */
    Top *found = NULL;
    for (guint i = tops->len; i-- > 0 && !found;) { /* newest last */
        Top *t = tops->pdata[i];
        if (bwn_app_id_matches(t->app_id, keys))
            found = t;
    }
    if (found)
        zwlr_foreign_toplevel_handle_v1_activate(found->handle, seat);
    zwlr_foreign_toplevel_manager_v1_stop(mgr);
    wl_display_roundtrip_queue(display, queue); /* "finished" */
    g_clear_pointer(&tops, g_ptr_array_unref);
    zwlr_foreign_toplevel_manager_v1_destroy(mgr);
    wl_display_flush(display);
    return found != NULL;
}

/* Events GDK read for our objects wait in the private queue: handle them
 * (no blocking, no reading), so it never grows. */
void
bwn_wl_activation_pump(void)
{
    if (queue)
        wl_display_dispatch_queue_pending(display, queue);
}

char *
bwn_wl_activation_token(GtkWidget *from)
{
    if (!activation || !seat || !from)
        return NULL;
    GtkWidget *top = gtk_widget_get_toplevel(from);
    GdkWindow *gw = top ? gtk_widget_get_window(top) : NULL;
    if (!gw || !GDK_IS_WAYLAND_WINDOW(gw))
        return NULL;
    wl_display_roundtrip_queue(display, queue); /* the click's serial */
    if (!last_serial)
        return NULL;
    struct xdg_activation_token_v1 *tok = xdg_activation_v1_get_activation_token(activation);
    xdg_activation_token_v1_add_listener(tok, &token_listener, NULL);
    xdg_activation_token_v1_set_serial(tok, last_serial, seat);
    xdg_activation_token_v1_set_surface(tok, gdk_wayland_window_get_wl_surface(gw));
    xdg_activation_token_v1_commit(tok);
    g_clear_pointer(&token_done, g_free);
    for (int i = 0; i < 5 && !token_done; i++)
        if (wl_display_roundtrip_queue(display, queue) < 0)
            break;
    xdg_activation_token_v1_destroy(tok);
    return g_steal_pointer(&token_done);
}

#else
gboolean bwn_wl_raise(char **keys) { (void)keys; return FALSE; }
void bwn_wl_activation_init(void) {}
void bwn_wl_activation_pump(void) {}
char *bwn_wl_activation_token(GtkWidget *from) { (void)from; return NULL; }
#endif
