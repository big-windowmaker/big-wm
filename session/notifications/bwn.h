/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-notifications: notification server, toasts, notification center and
 * tray bell of the Big WindowMaker sessions (X11 and Wayland).
 */
#ifndef BWN_H
#define BWN_H

#include <gtk/gtk.h>
#include <libintl.h>

#define _(s) gettext(s)
#define N_(s) (s)

/* Limits against misbehaving senders. */
#define BWN_MAX_SUMMARY 512     /* characters */
#define BWN_MAX_BODY 4096       /* characters */
#define BWN_MAX_ACTIONS 8       /* pairs */
#define BWN_MAX_IMAGE_SIDE 2048 /* pixels, image-data */
#define BWN_MAX_ACTIVE 256      /* notifications still open in the protocol */
#define BWN_IMAGE_LOAD 400      /* pixels, image-path and GNotification files */

typedef enum {
    BWN_URGENCY_LOW = 0,
    BWN_URGENCY_NORMAL = 1,
    BWN_URGENCY_CRITICAL = 2,
} BwnUrgency;

/* org.freedesktop.Notifications NotificationClosed reasons. */
typedef enum {
    BWN_CLOSED_EXPIRED = 1,
    BWN_CLOSED_DISMISSED = 2,
    BWN_CLOSED_BY_CALL = 3,
    BWN_CLOSED_UNDEFINED = 4,
} BwnCloseReason;

typedef struct {
    guint32 id;
    char *app_name;      /* as sent, or the desktop entry's name */
    char *app_icon;      /* icon name, path or file:// URI (may be NULL) */
    char *desktop_entry; /* hint desktop-entry (may be NULL) */
    char *summary;       /* plain text */
    char *body;          /* safe Pango markup */
    char *body_plain;    /* the same, without markup (search, accessibility) */
    char **actions;      /* key, label, key, label..., NULL-terminated */
    BwnUrgency urgency;
    gint32 timeout;      /* ms from the sender; -1 default, 0 never */
    gint64 time;         /* real time, microseconds */
    gboolean transient;  /* not kept in the history */
    gboolean resident;   /* stays after an action */
    gboolean read;       /* seen in the center */
    gboolean active;     /* still open in the protocol */
    GdkPixbuf *image;    /* image-data / image-path (memory only) */
    char *sender;        /* D-Bus unique name of the sender */
    guint32 sender_pid;  /* its process (X11: the window to raise), 0 unknown */
    /* org.gtk.Notifications (GNotification): the application and its own
     * id; actions are GAction names ("app.open") with optional targets. */
    char *gapp_id;
    char *gtk_id;
    char *gtk_default;          /* default action, NULL: activate the app */
    GVariant *gtk_default_target;
    GPtrArray *gtk_buttons;     /* action names, as actions "gtk:N" */
    GPtrArray *gtk_targets;     /* GVariant* or NULL, same order */
    guint refs;
} BwnNotif;

typedef struct {
    gboolean toasts;
    int timeout_s;          /* default display time */
    char *position;         /* top-right, top-left, bottom-right, bottom-left */
    int monitor;            /* -1: follow the pointer (X11) / compositor */
    int max_visible;
    int margin_x, margin_y; /* from the screen edge (Dock on the right) */
    gboolean dnd;
    gboolean dnd_critical;  /* critical ones still pop up in DND */
    gboolean history;
    int retention_days;
    int max_history;
    gboolean sound;
    gboolean animations;
    gboolean hide_on_lock;  /* no toasts while the session is locked */
    char **muted_apps;
} BwnSettings;

/* util.c: pure helpers (unit tested) */
char *bwn_markup_sanitize(const char *in, gsize max_chars);
char *bwn_markup_to_plain(const char *markup);
char *bwn_text_clean(const char *in, gsize max_chars);
char *bwn_reltime(gint64 then_us, gint64 now_us);
char *bwn_fold(const char *s);
gboolean bwn_action_key_valid(const char *key);

/* store.c */
BwnNotif *bwn_notif_new(void);
BwnNotif *bwn_notif_ref(BwnNotif *n);
void bwn_notif_unref(BwnNotif *n);
BwnSettings *bwn_settings(void);
void bwn_settings_load(void);
void bwn_settings_save(void);
void bwn_store_init(void);
void bwn_store_add(BwnNotif *n);   /* takes a reference; replaces same id */
BwnNotif *bwn_store_lookup(guint32 id);
GList *bwn_store_list(void);       /* newest first, not owned */
void bwn_store_remove(guint32 id); /* from the history */
GList *bwn_store_take_all(void);   /* empties the history (undo) */
void bwn_store_restore(GList *l);
int bwn_store_unread(void);
void bwn_store_mark_all_read(void);
void bwn_store_prune(void);
void bwn_store_save_later(void);
void bwn_store_flush(void);
void bwn_store_changed(void);      /* notifies the UI (bell, center) */
typedef void (*BwnChangedFunc)(gpointer data);
void bwn_store_connect_changed(BwnChangedFunc f, gpointer data);

/* The application a notification belongs to (grouping, raising). */
char *bwn_group_key(BwnNotif *n);

/* activate.c: a click on a notification (the balloon or its entry). */
void bwn_activate(BwnNotif *n, GtkWidget *from);
gboolean bwn_raise_app(BwnNotif *n, const char *token, guint32 timestamp);
gboolean bwn_app_id_matches(const char *app_id, char **keys);
char **bwn_app_keys(BwnNotif *n);
gboolean bwn_launch_app(BwnNotif *n, const char *token);

/* gtknotif.c: org.gtk.Notifications */
void bwn_gtk_start(GDBusConnection *bus);
gboolean bwn_gtk_invoke(BwnNotif *n, const char *key, const char *token);

/* server.c */
void bwn_server_start(GDBusConnection *bus);
void bwn_server_close(BwnNotif *n, BwnCloseReason reason);
void bwn_server_invoke(BwnNotif *n, const char *key, GtkWidget *from);
void bwn_server_emit_action(BwnNotif *n, const char *key, const char *token);
guint32 bwn_server_new_id(void);
guint32 bwn_sender_pid(const char *sender);
void bwn_server_deliver(BwnNotif *n, gboolean replace, const char *sender);
gboolean bwn_session_locked(void);
void bwn_set_dnd(gboolean on);

/* ui-toast.c */
void bwn_toast_init(void);
void bwn_toast_show(BwnNotif *n);
void bwn_toast_close(guint32 id);
void bwn_toast_update(BwnNotif *n);
void bwn_toast_refresh(BwnNotif *n);
void bwn_toast_hide_all(void);
int bwn_toast_count(void);

/* ui-center.c */
void bwn_center_toggle(void);
gboolean bwn_center_visible(void);

/* ui-common.c */
gboolean bwn_wayland(void);
gboolean bwn_dark(void);
void bwn_style_init(void);
GdkPixbuf *bwn_icon_for(BwnNotif *n, int size, int scale);
GdkPixbuf *bwn_app_icon_for(BwnNotif *n, int size, int scale);
GtkWidget *bwn_icon_widget(BwnNotif *n, int size);
GtkWidget *bwn_app_icon_widget(BwnNotif *n, int size);
gboolean bwn_image_is_avatar(BwnNotif *n);
GtkWidget *bwn_avatar_widget(BwnNotif *n, int size);
GtkWidget *bwn_picture_widget(BwnNotif *n, int max_w, int max_h);
GtkWidget *bwn_title_label(BwnNotif *n);
GtkWidget *bwn_count_tag(int count);
char *bwn_app_label(BwnNotif *n);
void bwn_play_sound(BwnNotif *n);

/* sni.c */
void bwn_sni_start(GDBusConnection *bus);
void bwn_sni_update(void);

/* wl-activation.c (Wayland only; NULL elsewhere) */
void bwn_wl_activation_init(void);
void bwn_wl_activation_pump(void);
char *bwn_wl_activation_token(GtkWidget *from);
gboolean bwn_wl_raise(char **keys);

#endif
