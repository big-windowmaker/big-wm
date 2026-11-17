/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Toasts: the balloons that pop up by themselves. They never take the
 * keyboard, never become the active window and never grab the pointer:
 *
 *   X11:     a GTK_WINDOW_POPUP (override-redirect, so the window manager
 *            neither manages nor focuses it), _NET_WM_WINDOW_TYPE_NOTIFICATION,
 *            WM_HINTS input=False (accept-focus FALSE), never focused on map.
 *   Wayland: a wlr-layer-shell surface on the overlay layer with keyboard
 *            interactivity NONE; the compositor keeps the pointer with the
 *            surface of a press until release (wlmaker 0158), so a toast that
 *            appears under a held button does not take it.
 *
 * Only a deliberate click on a toast does something (close, an action).
 */
#include "bwn.h"
#include <gtk-layer-shell.h>
#include <malloc.h>

#define TOAST_WIDTH 340
#define TOAST_GAP 8

typedef struct {
    BwnNotif *n;
    GtkWidget *win;
    GtkWidget *time_label;
    guint timer;
    gint64 deadline;   /* monotonic, 0: no expiry */
    gint64 remaining;  /* while paused */
    gboolean hover;
    guint fade;
    int count;         /* notifications of the application stacked here */
    char *group;       /* bwn_group_key() */
} Toast;

static GList *toasts;  /* Toast*, newest first (closest to the corner) */
static GQueue queue = G_QUEUE_INIT; /* BwnNotif* waiting for room */
static guint clock_id;

static void relayout(void);
static void toast_free(Toast *t);

int
bwn_toast_count(void)
{
    return g_list_length(toasts);
}

static Toast *
find(guint32 id)
{
    for (GList *l = toasts; l; l = l->next)
        if (((Toast *)l->data)->n->id == id)
            return l->data;
    return NULL;
}

/* == Timers ================================================================= */

static gint64
duration_ms(BwnNotif *n)
{
    if (n->urgency == BWN_URGENCY_CRITICAL || n->timeout == 0)
        return 0; /* stays until closed */
    if (n->timeout > 0)
        return CLAMP(n->timeout, 1500, 10 * 60 * 1000);
    gint64 ms = (gint64)bwn_settings()->timeout_s * 1000;
    if (n->actions && n->actions[0])
        ms += 2000; /* time to reach a button */
    return ms;
}

static gboolean
expire(gpointer data)
{
    Toast *t = data;
    t->timer = 0;
    bwn_server_close(t->n, BWN_CLOSED_EXPIRED); /* calls bwn_toast_close */
    return G_SOURCE_REMOVE;
}

static void
arm(Toast *t, gint64 ms)
{
    g_clear_handle_id(&t->timer, g_source_remove);
    if (ms <= 0) {
        t->deadline = 0;
        return;
    }
    t->deadline = g_get_monotonic_time() + ms * 1000;
    t->timer = g_timeout_add((guint)ms, expire, t);
}

static gboolean
on_enter(GtkWidget *w, GdkEventCrossing *e, gpointer data)
{
    (void)w;
    Toast *t = data;
    if (e->detail == GDK_NOTIFY_INFERIOR || t->hover)
        return FALSE;
    t->hover = TRUE;
    if (t->timer) { /* pause */
        t->remaining = MAX(0, (t->deadline - g_get_monotonic_time()) / 1000);
        g_clear_handle_id(&t->timer, g_source_remove);
    }
    return FALSE;
}

static gboolean
on_leave(GtkWidget *w, GdkEventCrossing *e, gpointer data)
{
    (void)w;
    Toast *t = data;
    if (e->detail == GDK_NOTIFY_INFERIOR || !t->hover)
        return FALSE;
    t->hover = FALSE;
    if (t->deadline) /* resume, with time to read the rest */
        arm(t, MAX(t->remaining, 1500));
    return FALSE;
}

static gboolean
tick_clock(gpointer data)
{
    (void)data;
    gint64 now = g_get_real_time();
    for (GList *l = toasts; l; l = l->next) {
        Toast *t = l->data;
        g_autofree char *rel = bwn_reltime(t->n->time, now);
        gtk_label_set_text(GTK_LABEL(t->time_label), rel);
    }
    if (!toasts) {
        clock_id = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

/* == Content ================================================================ */

static void
on_close_clicked(GtkButton *b, gpointer data)
{
    (void)b;
    Toast *t = data;
    bwn_server_close(t->n, BWN_CLOSED_DISMISSED);
}

static void
on_action_clicked(GtkButton *b, gpointer data)
{
    Toast *t = data;
    const char *key = g_object_get_data(G_OBJECT(b), "bwn-key");
    bwn_server_invoke(t->n, key, GTK_WIDGET(b));
}

/* A click on the balloon takes the user to the notification: the
 * application's default action, its window to the front, or the
 * application started (activate.c). Right click: close. */
static gboolean
on_body_release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    Toast *t = data;
    if (e->button == 3) {
        bwn_server_close(t->n, BWN_CLOSED_DISMISSED);
        return TRUE;
    }
    if (e->button != 1)
        return FALSE;
    BwnNotif *n = bwn_notif_ref(t->n);
    bwn_activate(n, w);
    if (find(n->id)) /* resident: the balloon goes, the notification stays */
        bwn_toast_close(n->id);
    bwn_notif_unref(n);
    return TRUE;
}

static GtkWidget *
build_content(Toast *t)
{
    BwnNotif *n = t->n;
    if (t->win) { /* the window speaks for what it shows now */
        AtkObject *acc = gtk_widget_get_accessible(t->win);
        atk_object_set_name(acc, n->summary);
        atk_object_set_description(acc, n->body_plain);
    }
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Header (Window Maker's title bar): the application's icon and the
     * title, how many are stacked, time, close. */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "bwn-header");
    GtkWidget *icon = bwn_app_icon_widget(n, 16);
    g_autofree char *app = bwn_app_label(n);
    gtk_widget_set_tooltip_text(icon, app);
    gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), bwn_title_label(n), TRUE, TRUE, 0);
    if (t->count > 1)
        gtk_box_pack_start(GTK_BOX(header), bwn_count_tag(t->count), FALSE, FALSE, 0);
    g_autofree char *rel = bwn_reltime(n->time, g_get_real_time());
    t->time_label = gtk_label_new(rel);
    gtk_style_context_add_class(gtk_widget_get_style_context(t->time_label), "bwn-time");
    GDateTime *dt = g_date_time_new_from_unix_local(n->time / G_USEC_PER_SEC);
    g_autofree char *full = g_date_time_format(dt, "%c");
    g_date_time_unref(dt);
    gtk_widget_set_tooltip_text(t->time_label, full);
    gtk_box_pack_start(GTK_BOX(header), t->time_label, FALSE, FALSE, 0);
    GtkWidget *close = gtk_button_new_with_label("×");
    gtk_widget_set_focus_on_click(close, FALSE);
    gtk_widget_set_can_focus(close, FALSE);
    gtk_widget_set_tooltip_text(close, _("Close"));
    gtk_style_context_add_class(gtk_widget_get_style_context(close), "bwn-close");
    atk_object_set_name(gtk_widget_get_accessible(close), _("Close notification"));
    g_signal_connect(close, "clicked", G_CALLBACK(on_close_clicked), t);
    gtk_box_pack_end(GTK_BOX(header), close, FALSE, FALSE, 0);
    gtk_widget_set_valign(icon, GTK_ALIGN_START);
    gtk_widget_set_valign(t->time_label, GTK_ALIGN_START);
    gtk_widget_set_valign(close, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), header, FALSE, FALSE, 0);

    /* Below: the avatar and the text (four lines); the image under both. */
    GtkWidget *avatar = bwn_avatar_widget(n, 40);
    GtkWidget *picture = bwn_picture_widget(n, TOAST_WIDTH - 22, 180);
    gboolean text = n->body && *n->body;
    if (avatar || text || picture) {
        GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_style_context_add_class(gtk_widget_get_style_context(content), "bwn-content");
        if (avatar || text) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            if (avatar)
                gtk_box_pack_start(GTK_BOX(row), avatar, FALSE, FALSE, 0);
            if (text) {
                GtkWidget *b = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(b), n->body);
                gtk_label_set_line_wrap(GTK_LABEL(b), TRUE);
                gtk_label_set_line_wrap_mode(GTK_LABEL(b), PANGO_WRAP_WORD_CHAR);
                gtk_label_set_lines(GTK_LABEL(b), 4);
                gtk_label_set_ellipsize(GTK_LABEL(b), PANGO_ELLIPSIZE_END);
                gtk_label_set_max_width_chars(GTK_LABEL(b), 1);
                gtk_label_set_xalign(GTK_LABEL(b), 0);
                gtk_widget_set_valign(b, avatar ? GTK_ALIGN_CENTER : GTK_ALIGN_START);
                gtk_style_context_add_class(gtk_widget_get_style_context(b), "bwn-body");
                gtk_box_pack_start(GTK_BOX(row), b, TRUE, TRUE, 0);
            }
            gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
        }
        if (picture)
            gtk_box_pack_start(GTK_BOX(content), picture, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(outer), content, FALSE, FALSE, 0);
    }

    /* Actions given by the sender ("default" is the click on the balloon). */
    GtkWidget *actions = NULL;
    for (char **a = n->actions; a && a[0] && a[1]; a += 2) {
        if (g_str_equal(a[0], "default"))
            continue;
        if (!actions) {
            actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_style_context_add_class(gtk_widget_get_style_context(actions), "bwn-actions");
        }
        GtkWidget *btn = gtk_button_new_with_label(a[1]);
        gtk_widget_set_focus_on_click(btn, FALSE);
        gtk_widget_set_can_focus(btn, FALSE);
        g_object_set_data_full(G_OBJECT(btn), "bwn-key", g_strdup(a[0]), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_action_clicked), t);
        gtk_box_pack_start(GTK_BOX(actions), btn, FALSE, FALSE, 0);
    }
    if (actions)
        gtk_box_pack_start(GTK_BOX(outer), actions, FALSE, FALSE, 0);

    GtkWidget *ev = gtk_event_box_new();
    gtk_widget_set_tooltip_text(ev, _("Click to open"));
    gtk_widget_add_events(ev, GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(ev, "button-release-event", G_CALLBACK(on_body_release), t);
    gtk_container_add(GTK_CONTAINER(ev), outer);
    return ev;
}

/* == Windows ================================================================ */

static gboolean
fade_step(GtkWidget *w, GdkFrameClock *clock, gpointer data)
{
    Toast *t = data;
    double o = gtk_widget_get_opacity(w) + 0.15;
    gtk_widget_set_opacity(w, MIN(o, 1.0));
    if (o >= 1.0) {
        t->fade = 0;
        return G_SOURCE_REMOVE;
    }
    (void)clock;
    return G_SOURCE_CONTINUE;
}

/* Our private Wayland queue (wl-activation.c) sees the same input: keep it
 * drained while the user is on our windows. */
static gboolean
on_any_event(GtkWidget *w, GdkEvent *e, gpointer data)
{
    (void)w; (void)e; (void)data;
    bwn_wl_activation_pump();
    return FALSE;
}

static GtkWidget *
new_window(Toast *t)
{
    GtkWidget *win;
    if (bwn_wayland() && gtk_layer_is_supported()) {
        win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_layer_init_for_window(GTK_WINDOW(win));
        gtk_layer_set_namespace(GTK_WINDOW(win), "big-wm-notification");
        gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
        int mon = bwn_settings()->monitor;
        GdkDisplay *d = gdk_display_get_default();
        if (mon >= 0 && mon < gdk_display_get_n_monitors(d))
            gtk_layer_set_monitor(GTK_WINDOW(win), gdk_display_get_monitor(d, mon));
    } else {
        win = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
        gtk_window_set_accept_focus(GTK_WINDOW(win), FALSE);
        gtk_window_set_focus_on_map(GTK_WINDOW(win), FALSE);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
        gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
        gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    }
    gtk_window_set_title(GTK_WINDOW(win), "big-wm-notification");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_widget_set_size_request(win, TOAST_WIDTH, -1);
    gtk_widget_add_events(win, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(win, "event", G_CALLBACK(on_any_event), NULL);
    g_signal_connect(win, "enter-notify-event", G_CALLBACK(on_enter), t);
    g_signal_connect(win, "leave-notify-event", G_CALLBACK(on_leave), t);
    GtkStyleContext *sc = gtk_widget_get_style_context(win);
    gtk_style_context_add_class(sc, "bwn-toast");
    if (t->n->urgency == BWN_URGENCY_CRITICAL)
        gtk_style_context_add_class(sc, "bwn-critical");
    AtkObject *acc = gtk_widget_get_accessible(win);
    atk_object_set_role(acc, ATK_ROLE_NOTIFICATION);
    atk_object_set_name(acc, t->n->summary);
    atk_object_set_description(acc, t->n->body_plain);
    return win;
}

static GdkMonitor *
x11_monitor(void)
{
    GdkDisplay *d = gdk_display_get_default();
    int mon = bwn_settings()->monitor;
    if (mon >= 0 && mon < gdk_display_get_n_monitors(d))
        return gdk_display_get_monitor(d, mon);
    GdkDevice *ptr = gdk_seat_get_pointer(gdk_display_get_default_seat(d));
    int x = 0, y = 0;
    if (ptr)
        gdk_device_get_position(ptr, NULL, &x, &y);
    return gdk_display_get_monitor_at_point(d, x, y);
}

/* Stacks the toasts from the configured corner, newest closest to it. */
static void
relayout(void)
{
    BwnSettings *s = bwn_settings();
    gboolean right = g_str_has_suffix(s->position, "right");
    gboolean bottom = g_str_has_prefix(s->position, "bottom");
    int offset = s->margin_y;
    GdkRectangle geo = { 0 };
    if (!bwn_wayland()) {
        GdkMonitor *m = x11_monitor();
        if (m)
            gdk_monitor_get_geometry(m, &geo);
    }
    for (GList *l = toasts; l; l = l->next) {
        Toast *t = l->data;
        int h;
        /* Height for the fixed width: wrapped text measured as shown. */
        gtk_widget_get_preferred_height_for_width(t->win, TOAST_WIDTH, NULL, &h);
        if (bwn_wayland() && gtk_layer_is_layer_window(GTK_WINDOW(t->win))) {
            GtkWindow *w = GTK_WINDOW(t->win);
            gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_RIGHT, right);
            gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_LEFT, !right);
            gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_BOTTOM, bottom);
            gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_TOP, !bottom);
            gtk_layer_set_margin(w, right ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_LEFT,
                                 s->margin_x);
            gtk_layer_set_margin(w, bottom ? GTK_LAYER_SHELL_EDGE_BOTTOM : GTK_LAYER_SHELL_EDGE_TOP,
                                 offset);
        } else {
            int x = right ? geo.x + geo.width - s->margin_x - TOAST_WIDTH : geo.x + s->margin_x;
            int y = bottom ? geo.y + geo.height - offset - h : geo.y + offset;
            gtk_window_move(GTK_WINDOW(t->win), x, y);
        }
        offset += h + TOAST_GAP;
    }
}

static void
present(BwnNotif *n)
{
    Toast *t = g_new0(Toast, 1);
    t->n = bwn_notif_ref(n);
    t->count = 1;
    t->group = bwn_group_key(n);
    t->win = new_window(t);
    gtk_container_add(GTK_CONTAINER(t->win), build_content(t));
    toasts = g_list_prepend(toasts, t);
    gtk_widget_show_all(t->win);
    relayout();
    if (bwn_settings()->animations && bwn_wayland()) {
        gtk_widget_set_opacity(t->win, 0.0);
        t->fade = gtk_widget_add_tick_callback(t->win, fade_step, t, NULL);
    }
    arm(t, duration_ms(n));
    if (!clock_id)
        clock_id = g_timeout_add_seconds(30, tick_clock, NULL);
}

void
bwn_toast_init(void)
{
}

static void expire_later(BwnNotif *n, gint64 ms);

void
bwn_toast_show(BwnNotif *n)
{
    if (find(n->id))
        return bwn_toast_update(n);
    if (bwn_center_visible()) { /* it shows up in the open panel */
        expire_later(n, duration_ms(n));
        return;
    }
    /* Another one of an application whose balloon is up: the same balloon
     * shows the newest, with the count (a chat does not fill the screen).
     * The previous stays in the center and expires in its time. */
    if (n->urgency != BWN_URGENCY_CRITICAL) {
        g_autofree char *group = bwn_group_key(n);
        for (GList *l = toasts; l; l = l->next) {
            Toast *t = l->data;
            if (t->group && g_str_equal(t->group, group) && t->n->urgency != BWN_URGENCY_CRITICAL) {
                BwnNotif *old = t->n;
                expire_later(old, duration_ms(old));
                t->count++;
                t->n = bwn_notif_ref(n);
                bwn_notif_unref(old);
                gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(t->win)));
                gtk_container_add(GTK_CONTAINER(t->win), build_content(t));
                gtk_widget_show_all(t->win);
                relayout();
                gint64 ms = duration_ms(n);
                if (!t->hover) {
                    arm(t, ms);
                } else { /* paused: the new expiry applies when it leaves */
                    g_clear_handle_id(&t->timer, g_source_remove);
                    t->deadline = ms > 0 ? 1 : 0;
                    t->remaining = ms;
                }
                return;
            }
        }
    }
    if ((int)g_list_length(toasts) < bwn_settings()->max_visible) {
        present(n);
    } else if (n->urgency == BWN_URGENCY_CRITICAL) {
        g_queue_push_head(&queue, bwn_notif_ref(n));
    } else {
        g_queue_push_tail(&queue, bwn_notif_ref(n));
    }
}

static void
toast_free(Toast *t)
{
    g_clear_handle_id(&t->timer, g_source_remove);
    gtk_widget_destroy(t->win);
    bwn_notif_unref(t->n);
    g_free(t->group);
    g_free(t);
}

static void
next_from_queue(void)
{
    while ((int)g_list_length(toasts) < bwn_settings()->max_visible && !g_queue_is_empty(&queue)) {
        BwnNotif *n = g_queue_pop_head(&queue);
        if (n->active && !bwn_settings()->dnd)
            present(n);
        bwn_notif_unref(n);
    }
}

void
bwn_toast_close(guint32 id)
{
    Toast *t = find(id);
    if (t) {
        toasts = g_list_remove(toasts, t);
        toast_free(t);
        relayout();
        if (!toasts && g_queue_is_empty(&queue))
            malloc_trim(0); /* a burst of balloons leaves no heap behind */
    } else {
        for (GList *l = queue.head; l; l = l->next)
            if (((BwnNotif *)l->data)->id == id) {
                bwn_notif_unref(l->data);
                g_queue_delete_link(&queue, l);
                break;
            }
    }
    next_from_queue();
}

/* replaces_id: the visible toast shows the new content in place. */
void
bwn_toast_update(BwnNotif *n)
{
    Toast *t = find(n->id);
    if (!t) {
        for (GList *l = queue.head; l; l = l->next)
            if (((BwnNotif *)l->data)->id == n->id) {
                bwn_notif_unref(l->data);
                l->data = bwn_notif_ref(n);
                return;
            }
        bwn_toast_show(n);
        return;
    }
    bwn_notif_unref(t->n);
    t->n = bwn_notif_ref(n);
    gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(t->win)));
    gtk_container_add(GTK_CONTAINER(t->win), build_content(t));
    gtk_widget_show_all(t->win);
    relayout();
    gint64 ms = duration_ms(n);
    if (!t->hover) {
        arm(t, ms);
    } else { /* paused: the new expiry applies when the pointer leaves */
        g_clear_handle_id(&t->timer, g_source_remove);
        t->deadline = ms > 0 ? 1 : 0;
        t->remaining = ms;
    }
}

/* A replacement that pops nothing up: only a balloon already shown (or
 * waiting) gets the new content. */
void
bwn_toast_refresh(BwnNotif *n)
{
    if (find(n->id)) {
        bwn_toast_update(n);
        return;
    }
    for (GList *l = queue.head; l; l = l->next)
        if (((BwnNotif *)l->data)->id == n->id) {
            bwn_notif_unref(l->data);
            l->data = bwn_notif_ref(n);
            return;
        }
}

static gboolean
expire_hidden(gpointer data)
{
    BwnNotif *n = data;
    if (n->active && bwn_store_lookup(n->id) == n)
        bwn_server_close(n, BWN_CLOSED_EXPIRED);
    return G_SOURCE_REMOVE;
}

/* Without its balloon, a notification still expires for its sender after
 * its time (an open one stays usable in the center until then). */
static void
expire_later(BwnNotif *n, gint64 ms)
{
    if (ms > 0 && n->active)
        g_timeout_add_full(G_PRIORITY_DEFAULT, (guint)ms, expire_hidden, bwn_notif_ref(n),
                           (GDestroyNotify)bwn_notif_unref);
}

/* Do not disturb, the lock, the center opening: the balloons go away. */
void
bwn_toast_hide_all(void)
{
    for (GList *l = toasts; l; l = l->next) {
        Toast *t = l->data;
        gint64 left = t->deadline ? MAX(1000, (t->deadline - g_get_monotonic_time()) / 1000)
                                  : 0;
        if (t->hover && t->deadline)
            left = MAX(t->remaining, 1000);
        expire_later(t->n, left);
    }
    g_list_free_full(g_steal_pointer(&toasts), (GDestroyNotify)toast_free);
    while (!g_queue_is_empty(&queue)) {
        BwnNotif *n = g_queue_pop_head(&queue);
        expire_later(n, duration_ms(n));
        bwn_notif_unref(n);
    }
}
