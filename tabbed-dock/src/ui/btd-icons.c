/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-icons.h"

#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_ICON_FILE (8 * 1024 * 1024)

struct _BtdIconCache {
    GtkIconTheme *theme;
    guint max;
    GHashTable *table; /* key -> cairo_surface_t* */
    GQueue lru;        /* keys, most recent at head (owned by table) */
};

BtdIconCache *
btd_icon_cache_new(GtkIconTheme *theme, guint max_entries)
{
    BtdIconCache *c = g_new0(BtdIconCache, 1);
    c->theme = g_object_ref(theme);
    c->max = MAX(8, max_entries);
    c->table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)cairo_surface_destroy);
    g_queue_init(&c->lru);
    return c;
}

void
btd_icon_cache_clear(BtdIconCache *c)
{
    g_queue_clear(&c->lru);
    g_hash_table_remove_all(c->table);
}

void
btd_icon_cache_free(BtdIconCache *c)
{
    if (!c)
        return;
    btd_icon_cache_clear(c);
    g_hash_table_unref(c->table);
    g_object_unref(c->theme);
    g_free(c);
}

guint
btd_icon_cache_size(BtdIconCache *c)
{
    return g_hash_table_size(c->table);
}

static cairo_surface_t *
cache_get(BtdIconCache *c, const char *key)
{
    char *orig = NULL;
    cairo_surface_t *s = NULL;
    if (!g_hash_table_lookup_extended(c->table, key, (gpointer *)&orig, (gpointer *)&s))
        return NULL;
    GList *link = g_queue_find(&c->lru, orig);
    if (link && link != c->lru.head) {
        g_queue_unlink(&c->lru, link);
        g_queue_push_head_link(&c->lru, link);
    }
    return s;
}

static cairo_surface_t *
cache_put(BtdIconCache *c, const char *key, cairo_surface_t *s)
{
    while (g_hash_table_size(c->table) >= c->max) {
        char *old = g_queue_pop_tail(&c->lru);
        if (!old)
            break;
        g_hash_table_remove(c->table, old);
    }
    char *k = g_strdup(key);
    g_hash_table_insert(c->table, k, s);
    g_queue_push_head(&c->lru, k);
    return s;
}

static cairo_surface_t *
load_file(const char *path, int size, int scale)
{
    GStatBuf st;
    if (!g_path_is_absolute(path) || g_stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > MAX_ICON_FILE)
        return NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, size * scale, size * scale, TRUE, NULL);
    if (!pb)
        return NULL;
    cairo_surface_t *s = gdk_cairo_surface_create_from_pixbuf(pb, scale, NULL);
    g_object_unref(pb);
    return s;
}

static cairo_surface_t *
load_theme(BtdIconCache *c, const char *name, int size, int scale)
{
    GtkIconInfo *info = gtk_icon_theme_lookup_icon_for_scale(c->theme, name, size, scale,
                                                              GTK_ICON_LOOKUP_FORCE_SIZE);
    if (!info)
        return NULL;
    cairo_surface_t *s = gtk_icon_info_load_surface(info, NULL, NULL);
    g_object_unref(info);
    return s;
}

static cairo_surface_t *
load_name(BtdIconCache *c, const char *icon, int size, int scale)
{
    cairo_surface_t *s = NULL;
    if (!icon || !*icon)
        return NULL;
    if (g_path_is_absolute(icon))
        return load_file(icon, size, scale);
    if ((s = load_theme(c, icon, size, scale)))
        return s;
    /* Legacy Icon=name.png and /usr/share/pixmaps. */
    char *base = g_strdup(icon);
    char *dot = strrchr(base, '.');
    if (dot && (g_str_equal(dot, ".png") || g_str_equal(dot, ".svg") || g_str_equal(dot, ".xpm"))) {
        *dot = '\0';
        s = load_theme(c, base, size, scale);
    }
    const char *exts[] = { "", ".png", ".svg", ".xpm", NULL };
    for (int i = 0; !s && exts[i]; i++) {
        char *p = g_strconcat("/usr/share/pixmaps/", icon, exts[i], NULL);
        if (!strchr(icon, '/'))
            s = load_file(p, size, scale);
        g_free(p);
    }
    g_free(base);
    return s;
}

static cairo_surface_t *
lookup_or_load(BtdIconCache *c, const char *key_name, int size, int scale, GIcon *gicon)
{
    char *key = g_strdup_printf("%s|%d|%d", key_name, size, scale);
    cairo_surface_t *s = cache_get(c, key);
    if (!s) {
        if (gicon) {
            GtkIconInfo *info =
                gtk_icon_theme_lookup_by_gicon_for_scale(c->theme, gicon, size, scale, GTK_ICON_LOOKUP_FORCE_SIZE);
            if (info) {
                s = gtk_icon_info_load_surface(info, NULL, NULL);
                g_object_unref(info);
            }
        } else {
            s = load_name(c, key_name, size, scale);
        }
        if (!s)
            s = load_name(c, "application-x-executable", size, scale);
        if (!s) {
            /* last resort: an empty surface so callers never get NULL */
            s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size * scale, size * scale);
            cairo_surface_set_device_scale(s, scale, scale);
        }
        cache_put(c, key, s);
    }
    g_free(key);
    return s;
}

cairo_surface_t *
btd_icon_cache_lookup_name(BtdIconCache *c, const char *icon, int size, int scale)
{
    return lookup_or_load(c, icon && *icon ? icon : "application-x-executable", size, scale, NULL);
}

cairo_surface_t *
btd_icon_cache_lookup_item(BtdIconCache *c, const BtdItem *item, BtdDesktopIndex *index, int size, int scale)
{
    if (item->icon)
        return btd_icon_cache_lookup_name(c, item->icon, size, scale);
    switch (item->type) {
    case BTD_ITEM_APP: {
        const BtdDesktopEntry *e = g_path_is_absolute(item->target)
                                       ? btd_desktop_index_lookup_path(index, item->target)
                                       : btd_desktop_index_lookup(index, item->target);
        BtdDesktopEntry *tmp = NULL;
        if (!e && g_path_is_absolute(item->target))
            e = tmp = btd_desktop_entry_load(item->target, NULL);
        cairo_surface_t *s = btd_icon_cache_lookup_name(c, e ? e->icon : NULL, size, scale);
        btd_desktop_entry_free(tmp);
        return s;
    }
    case BTD_ITEM_FOLDER: {
        /* XDG special folders get their themed icons */
        const struct {
            GUserDirectory dir;
            const char *icon;
        } special[] = {
            { G_USER_DIRECTORY_DOCUMENTS, "folder-documents" }, { G_USER_DIRECTORY_DOWNLOAD, "folder-download" },
            { G_USER_DIRECTORY_PICTURES, "folder-pictures" },   { G_USER_DIRECTORY_MUSIC, "folder-music" },
            { G_USER_DIRECTORY_VIDEOS, "folder-videos" },       { G_USER_DIRECTORY_DESKTOP, "user-desktop" },
        };
        if (g_strcmp0(item->target, g_get_home_dir()) == 0)
            return btd_icon_cache_lookup_name(c, "user-home", size, scale);
        for (guint i = 0; i < G_N_ELEMENTS(special); i++)
            if (g_strcmp0(item->target, g_get_user_special_dir(special[i].dir)) == 0) {
                cairo_surface_t *s = lookup_or_load(c, special[i].icon, size, scale, NULL);
                return s;
            }
        return btd_icon_cache_lookup_name(c, "folder", size, scale);
    }
    case BTD_ITEM_FILE: {
        /* content type from the name only: no I/O on (possibly slow) mounts */
        char *ctype = g_content_type_guess(item->target, NULL, 0, NULL);
        GIcon *gicon = g_content_type_get_icon(ctype);
        char *key = g_strconcat("ctype:", ctype, NULL);
        cairo_surface_t *s = lookup_or_load(c, key, size, scale, gicon);
        g_free(key);
        g_object_unref(gicon);
        g_free(ctype);
        return s;
    }
    case BTD_ITEM_APPIMAGE:
        return btd_icon_cache_lookup_name(c, "application-x-executable", size, scale);
    case BTD_ITEM_SEPARATOR:
        break;
    }
    return btd_icon_cache_lookup_name(c, NULL, size, scale);
}
