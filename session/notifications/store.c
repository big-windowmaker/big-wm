/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Notifications in memory, the history on disk and the settings.
 *
 * History: $XDG_STATE_HOME/big-wm/notifications-history, a GVariant written
 * atomically with mode 0600 (directory 0700). Images and actions are not
 * written (actions cannot be invoked once the sender is gone). Settings:
 * $XDG_CONFIG_HOME/big-wm/notifications.conf ([Notifications] group); only
 * keys that differ from the defaults are written.
 */
#include "bwn.h"
#include <glib/gstdio.h>
#include <string.h>

#define HISTORY_FORMAT "(ua(usssssyxb))"
#define HISTORY_VERSION 1

static BwnSettings settings;
static GList *items;    /* BwnNotif*, newest first */
static guint save_id;
static GList *watchers; /* struct watcher* */

struct watcher {
    BwnChangedFunc f;
    gpointer data;
};

BwnNotif *
bwn_notif_new(void)
{
    BwnNotif *n = g_new0(BwnNotif, 1);
    n->refs = 1;
    n->urgency = BWN_URGENCY_NORMAL;
    n->timeout = -1;
    n->time = g_get_real_time();
    return n;
}

BwnNotif *
bwn_notif_ref(BwnNotif *n)
{
    n->refs++;
    return n;
}

void
bwn_notif_unref(BwnNotif *n)
{
    if (!n || --n->refs > 0)
        return;
    g_free(n->app_name);
    g_free(n->app_icon);
    g_free(n->desktop_entry);
    g_free(n->summary);
    g_free(n->body);
    g_free(n->body_plain);
    g_strfreev(n->actions);
    g_free(n->gapp_id);
    g_free(n->gtk_id);
    g_free(n->gtk_default);
    g_clear_pointer(&n->gtk_default_target, g_variant_unref);
    g_clear_pointer(&n->gtk_buttons, g_ptr_array_unref);
    g_clear_pointer(&n->gtk_targets, g_ptr_array_unref);
    g_clear_object(&n->image);
    g_free(n->sender);
    g_free(n);
}

/* The application a notification belongs to: its desktop entry (or GApplication
 * id), else its name. Notifications with the same key are grouped. */
char *
bwn_group_key(BwnNotif *n)
{
    const char *k = n->gapp_id ? n->gapp_id : n->desktop_entry;
    if (k && *k) {
        char *lower = g_ascii_strdown(k, -1);
        if (g_str_has_suffix(lower, ".desktop"))
            lower[strlen(lower) - 8] = '\0';
        return lower;
    }
    if (n->app_name && *n->app_name)
        return g_utf8_casefold(n->app_name, -1);
    return g_strdup(n->sender ? n->sender : "");
}

/* == Settings ============================================================= */

BwnSettings *
bwn_settings(void)
{
    return &settings;
}

static char *
settings_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "big-wm", "notifications.conf", NULL);
}

static void
settings_defaults(void)
{
    g_free(settings.position);
    g_strfreev(settings.muted_apps);
    settings = (BwnSettings){
        .toasts = TRUE,
        .timeout_s = 6,
        .position = g_strdup("top-right"),
        .monitor = -1,
        .max_visible = 3,
        .margin_x = 76, /* the Dock column on the right edge */
        .margin_y = 12,
        .dnd = FALSE,
        .dnd_critical = TRUE,
        .history = TRUE,
        .retention_days = 14,
        .max_history = 200,
        .sound = FALSE,
        .animations = TRUE,
        .hide_on_lock = TRUE,
        .muted_apps = NULL,
    };
}

static int
get_int(GKeyFile *kf, const char *key, int def, int lo, int hi)
{
    GError *e = NULL;
    int v = g_key_file_get_integer(kf, "Notifications", key, &e);
    if (e) {
        g_error_free(e);
        return def;
    }
    return CLAMP(v, lo, hi);
}

static gboolean
get_bool(GKeyFile *kf, const char *key, gboolean def)
{
    GError *e = NULL;
    gboolean v = g_key_file_get_boolean(kf, "Notifications", key, &e);
    if (e) {
        g_error_free(e);
        return def;
    }
    return v;
}

void
bwn_settings_load(void)
{
    settings_defaults();
    g_autofree char *path = settings_path();
    g_autoptr(GKeyFile) kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
        return;
    settings.toasts = get_bool(kf, "Toasts", settings.toasts);
    settings.timeout_s = get_int(kf, "Timeout", settings.timeout_s, 2, 120);
    settings.monitor = get_int(kf, "Monitor", settings.monitor, -1, 16);
    settings.max_visible = get_int(kf, "MaxVisible", settings.max_visible, 1, 8);
    settings.margin_x = get_int(kf, "MarginX", settings.margin_x, 0, 2000);
    settings.margin_y = get_int(kf, "MarginY", settings.margin_y, 0, 2000);
    settings.dnd = get_bool(kf, "DoNotDisturb", settings.dnd);
    settings.dnd_critical = get_bool(kf, "DoNotDisturbAllowsCritical", settings.dnd_critical);
    settings.history = get_bool(kf, "History", settings.history);
    settings.retention_days = get_int(kf, "RetentionDays", settings.retention_days, 0, 3650);
    settings.max_history = get_int(kf, "MaxHistory", settings.max_history, 10, 5000);
    settings.sound = get_bool(kf, "Sound", settings.sound);
    settings.animations = get_bool(kf, "Animations", settings.animations);
    settings.hide_on_lock = get_bool(kf, "HideOnLock", settings.hide_on_lock);
    g_autofree char *pos = g_key_file_get_string(kf, "Notifications", "Position", NULL);
    if (pos && (g_str_equal(pos, "top-right") || g_str_equal(pos, "top-left") ||
                g_str_equal(pos, "bottom-right") || g_str_equal(pos, "bottom-left"))) {
        g_free(settings.position);
        settings.position = g_steal_pointer(&pos);
    }
    settings.muted_apps = g_key_file_get_string_list(kf, "Notifications", "MutedApps", NULL, NULL);
}

void
bwn_settings_save(void)
{
    BwnSettings def = { 0 };
    BwnSettings cur = settings;
    settings.position = NULL;
    settings.muted_apps = NULL;
    settings_defaults();
    def = settings;
    settings = cur;

    g_autofree char *path = settings_path();
    g_autoptr(GKeyFile) kf = g_key_file_new();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
#define SET_BOOL(k, f)                                                                     \
    if (cur.f != def.f) g_key_file_set_boolean(kf, "Notifications", k, cur.f);             \
    else g_key_file_remove_key(kf, "Notifications", k, NULL);
#define SET_INT(k, f)                                                                      \
    if (cur.f != def.f) g_key_file_set_integer(kf, "Notifications", k, cur.f);             \
    else g_key_file_remove_key(kf, "Notifications", k, NULL);
    SET_BOOL("Toasts", toasts);
    SET_INT("Timeout", timeout_s);
    SET_INT("Monitor", monitor);
    SET_INT("MaxVisible", max_visible);
    SET_INT("MarginX", margin_x);
    SET_INT("MarginY", margin_y);
    SET_BOOL("DoNotDisturb", dnd);
    SET_BOOL("DoNotDisturbAllowsCritical", dnd_critical);
    SET_BOOL("History", history);
    SET_INT("RetentionDays", retention_days);
    SET_INT("MaxHistory", max_history);
    SET_BOOL("Sound", sound);
    SET_BOOL("Animations", animations);
    SET_BOOL("HideOnLock", hide_on_lock);
    if (g_strcmp0(cur.position, def.position) != 0)
        g_key_file_set_string(kf, "Notifications", "Position", cur.position);
    else
        g_key_file_remove_key(kf, "Notifications", "Position", NULL);
    g_free(def.position);
#undef SET_BOOL
#undef SET_INT
    g_autofree char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    gsize len;
    g_autofree char *data = g_key_file_to_data(kf, &len, NULL);
    g_file_set_contents_full(path, data, len, G_FILE_SET_CONTENTS_CONSISTENT, 0644, NULL);
}

/* == Store ================================================================= */

void
bwn_store_connect_changed(BwnChangedFunc f, gpointer data)
{
    struct watcher *w = g_new(struct watcher, 1);
    w->f = f;
    w->data = data;
    watchers = g_list_append(watchers, w);
}

void
bwn_store_changed(void)
{
    for (GList *l = watchers; l; l = l->next) {
        struct watcher *w = l->data;
        w->f(w->data);
    }
}

static char *
history_path(void)
{
    const char *state = g_getenv("XDG_STATE_HOME");
    g_autofree char *base = state && g_path_is_absolute(state)
                                ? g_strdup(state)
                                : g_build_filename(g_get_home_dir(), ".local", "state", NULL);
    return g_build_filename(base, "big-wm", "notifications-history", NULL);
}

static guint32 max_id;

guint32 bwn_store_max_id(void);
guint32
bwn_store_max_id(void)
{
    return max_id;
}

static void
history_load(void)
{
    g_autofree char *path = history_path();
    g_autofree char *data = NULL;
    gsize len = 0;
    if (!g_file_get_contents(path, &data, &len, NULL))
        return;
    g_autoptr(GBytes) bytes = g_bytes_new_take(g_steal_pointer(&data), len);
    g_autoptr(GVariant) v =
        g_variant_ref_sink(g_variant_new_from_bytes(G_VARIANT_TYPE(HISTORY_FORMAT), bytes, FALSE));
    /* A damaged file reads as empty fields, never out of bounds. */
    g_autoptr(GVariant) own = g_variant_get_normal_form(v);
    guint32 version;
    g_autoptr(GVariantIter) it = NULL;
    g_variant_get(own, "(ua(usssssyxb))", &version, &it);
    if (version != HISTORY_VERSION)
        return;
    guint32 id;
    const char *app, *icon, *desktop, *summary, *body;
    guint8 urgency;
    gint64 time;
    gboolean read;
    while (g_variant_iter_next(it, "(u&s&s&s&s&syxb)", &id, &app, &icon, &desktop, &summary,
                               &body, &urgency, &time, &read)) {
        BwnNotif *n = bwn_notif_new();
        n->id = id;
        n->app_name = g_strdup(app);
        n->app_icon = *icon ? g_strdup(icon) : NULL;
        n->desktop_entry = *desktop ? g_strdup(desktop) : NULL;
        n->summary = bwn_text_clean(summary, BWN_MAX_SUMMARY);
        n->body = bwn_markup_sanitize(body, BWN_MAX_BODY);
        n->body_plain = bwn_markup_to_plain(n->body);
        n->urgency = MIN(urgency, BWN_URGENCY_CRITICAL);
        n->time = time;
        n->read = read;
        n->active = FALSE;
        items = g_list_append(items, n);
        max_id = MAX(max_id, id);
    }
}

void
bwn_store_flush(void)
{
    g_clear_handle_id(&save_id, g_source_remove);
    g_autofree char *path = history_path();
    if (!settings.history) {
        g_unlink(path);
        return;
    }
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a(usssssyxb)"));
    for (GList *l = items; l; l = l->next) {
        BwnNotif *n = l->data;
        if (n->transient)
            continue;
        g_variant_builder_add(&b, "(usssssyxb)", n->id, n->app_name ? n->app_name : "",
                              n->app_icon ? n->app_icon : "",
                              n->desktop_entry ? n->desktop_entry : "", n->summary, n->body,
                              (guint8)n->urgency, n->time, n->read);
    }
    g_autoptr(GVariant) v =
        g_variant_ref_sink(g_variant_new("(u@a(usssssyxb))", HISTORY_VERSION, g_variant_builder_end(&b)));
    g_autofree char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    GError *e = NULL;
    if (!g_file_set_contents_full(path, g_variant_get_data(v), g_variant_get_size(v),
                                  G_FILE_SET_CONTENTS_CONSISTENT, 0600, &e)) {
        g_warning("history not saved: %s", e->message);
        g_error_free(e);
    }
}

static gboolean
save_timeout(gpointer data)
{
    (void)data;
    save_id = 0;
    bwn_store_flush();
    return G_SOURCE_REMOVE;
}

void
bwn_store_save_later(void)
{
    if (!save_id)
        save_id = g_timeout_add_seconds(3, save_timeout, NULL);
}

void
bwn_store_init(void)
{
    if (settings.history)
        history_load();
    bwn_store_prune();
}

BwnNotif *
bwn_store_lookup(guint32 id)
{
    for (GList *l = items; l; l = l->next)
        if (((BwnNotif *)l->data)->id == id)
            return l->data;
    return NULL;
}

void
bwn_store_add(BwnNotif *n)
{
    GList *old = NULL;
    for (GList *l = items; l; l = l->next)
        if (((BwnNotif *)l->data)->id == n->id) {
            old = l;
            break;
        }
    if (old) {
        ((BwnNotif *)old->data)->active = FALSE; /* superseded */
        bwn_notif_unref(old->data);
        items = g_list_delete_link(items, old);
    }
    items = g_list_prepend(items, bwn_notif_ref(n));
    max_id = MAX(max_id, n->id);
    bwn_store_prune();
    bwn_store_save_later();
    bwn_store_changed();
}

GList *
bwn_store_list(void)
{
    return items;
}

void
bwn_store_remove(guint32 id)
{
    for (GList *l = items; l; l = l->next) {
        BwnNotif *n = l->data;
        if (n->id == id) {
            items = g_list_delete_link(items, l);
            bwn_notif_unref(n);
            break;
        }
    }
    bwn_store_save_later();
    bwn_store_changed();
}

GList *
bwn_store_take_all(void)
{
    GList *all = items;
    items = NULL;
    bwn_store_save_later();
    bwn_store_changed();
    return all;
}

void
bwn_store_restore(GList *l)
{
    /* Newest first: what arrived after Clear stays ahead of the restored. */
    items = g_list_concat(items, l);
    bwn_store_prune();
    bwn_store_save_later();
    bwn_store_changed();
}

int
bwn_store_unread(void)
{
    int c = 0;
    for (GList *l = items; l; l = l->next)
        if (!((BwnNotif *)l->data)->read)
            c++;
    return c;
}

void
bwn_store_mark_all_read(void)
{
    gboolean any = FALSE;
    for (GList *l = items; l; l = l->next) {
        BwnNotif *n = l->data;
        if (!n->read)
            n->read = any = TRUE;
    }
    if (any) {
        bwn_store_save_later();
        bwn_store_changed();
    }
}

/* Keeps at most MaxHistory entries and drops those older than RetentionDays
 * (0: no age limit). Notifications still open in the protocol stay. Without
 * history, only the open ones are kept. */
void
bwn_store_prune(void)
{
    gint64 now = g_get_real_time();
    gint64 max_age = (gint64)settings.retention_days * 24 * 3600 * G_USEC_PER_SEC;
    int kept = 0;
    GList *l = items;
    while (l) {
        GList *next = l->next;
        BwnNotif *n = l->data;
        gboolean drop = FALSE;
        if (!n->active) {
            if (!settings.history || n->transient)
                drop = TRUE;
            else if (max_age > 0 && now - n->time > max_age)
                drop = TRUE;
            else if (kept >= settings.max_history)
                drop = TRUE;
        }
        if (drop) {
            items = g_list_delete_link(items, l);
            bwn_notif_unref(n);
        } else {
            kept++;
        }
        l = next;
    }
}
