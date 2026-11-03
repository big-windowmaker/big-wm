/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-wmattrs.h"

#include <glib/gstdio.h>
#include <string.h>

#define MAX_ATTRS_FILE (4 * 1024 * 1024)

struct _BtdWmAttrs {
    char *path;
    char **icon_dirs;
    GHashTable *icons; /* attribute key -> absolute icon path */
    char *signature;   /* sorted "key=icon" lines, to detect changes */
};

/* ---- a small reader for the GNUstep property list syntax ----------------- */

typedef enum { TOK_END, TOK_PUNCT, TOK_STRING, TOK_ERROR } TokKind;

typedef struct {
    const char *p, *end;
    TokKind kind;
    char punct;
    GString *text;
} Lexer;

static void
skip_space(Lexer *lx)
{
    while (lx->p < lx->end) {
        if (g_ascii_isspace(*lx->p)) {
            lx->p++;
        } else if (lx->p + 1 < lx->end && lx->p[0] == '/' && lx->p[1] == '/') {
            while (lx->p < lx->end && *lx->p != '\n')
                lx->p++;
        } else if (lx->p + 1 < lx->end && lx->p[0] == '/' && lx->p[1] == '*') {
            const char *e = g_strstr_len(lx->p + 2, lx->end - lx->p - 2, "*/");
            lx->p = e ? e + 2 : lx->end;
        } else {
            break;
        }
    }
}

static void
next(Lexer *lx)
{
    skip_space(lx);
    g_string_truncate(lx->text, 0);
    if (lx->p >= lx->end) {
        lx->kind = TOK_END;
        return;
    }
    char c = *lx->p;
    if (strchr("{}()=;,", c)) {
        lx->kind = TOK_PUNCT;
        lx->punct = c;
        lx->p++;
        return;
    }
    if (c == '<') { /* <data>: not used for icons */
        const char *e = memchr(lx->p, '>', lx->end - lx->p);
        lx->p = e ? e + 1 : lx->end;
        lx->kind = TOK_STRING;
        return;
    }
    lx->kind = TOK_STRING;
    if (c == '"') {
        lx->p++;
        while (lx->p < lx->end && *lx->p != '"') {
            if (*lx->p == '\\' && lx->p + 1 < lx->end) {
                lx->p++;
                switch (*lx->p) {
                case 'n': g_string_append_c(lx->text, '\n'); break;
                case 't': g_string_append_c(lx->text, '\t'); break;
                default: g_string_append_c(lx->text, *lx->p); break;
                }
            } else {
                g_string_append_c(lx->text, *lx->p);
            }
            lx->p++;
        }
        if (lx->p >= lx->end) {
            lx->kind = TOK_ERROR;
            return;
        }
        lx->p++;
        return;
    }
    while (lx->p < lx->end && !g_ascii_isspace(*lx->p) && !strchr("{}()=;,\"<", *lx->p))
        g_string_append_c(lx->text, *lx->p++);
}

static gboolean is_punct(Lexer *lx, char c) { return lx->kind == TOK_PUNCT && lx->punct == c; }

/* Skips one value (string, array or dictionary). */
static gboolean
skip_value(Lexer *lx, int depth)
{
    if (depth > 32)
        return FALSE;
    if (lx->kind == TOK_STRING) {
        next(lx);
        return TRUE;
    }
    if (is_punct(lx, '(')) {
        next(lx);
        while (!is_punct(lx, ')')) {
            if (lx->kind == TOK_END || lx->kind == TOK_ERROR || !skip_value(lx, depth + 1))
                return FALSE;
            if (is_punct(lx, ','))
                next(lx);
        }
        next(lx);
        return TRUE;
    }
    if (is_punct(lx, '{')) {
        next(lx);
        while (!is_punct(lx, '}')) {
            if (lx->kind != TOK_STRING)
                return FALSE;
            next(lx);
            if (!is_punct(lx, '='))
                return FALSE;
            next(lx);
            if (!skip_value(lx, depth + 1))
                return FALSE;
            if (is_punct(lx, ';'))
                next(lx);
        }
        next(lx);
        return TRUE;
    }
    return FALSE;
}

/* ---- icons ---------------------------------------------------------------- */

static char *
gnustep_root(void)
{
    const char *root = g_getenv("WMAKER_USER_ROOT");
    if (root && *root)
        return g_strdup(root);
    return g_build_filename(g_get_home_dir(), "GNUstep", NULL);
}

static char **
default_icon_dirs(void)
{
    char *root = gnustep_root();
    GPtrArray *a = g_ptr_array_new();
    g_ptr_array_add(a, g_build_filename(root, "Library", "Icons", NULL));
    g_ptr_array_add(a, g_build_filename(root, "Library", "WindowMaker", "Pixmaps", NULL));
    g_ptr_array_add(a, g_build_filename(root, "Library", "WindowMaker", "CachedPixmaps", NULL));
    g_ptr_array_add(a, g_strdup("/usr/share/WindowMaker/Icons"));
    g_ptr_array_add(a, g_strdup("/usr/share/WindowMaker/Pixmaps"));
    g_ptr_array_add(a, g_strdup("/usr/local/share/WindowMaker/Icons"));
    g_ptr_array_add(a, g_strdup("/usr/share/pixmaps"));
    g_ptr_array_add(a, NULL);
    g_free(root);
    return (char **)g_ptr_array_free(a, FALSE);
}

static char *
resolve_icon(BtdWmAttrs *attrs, const char *icon)
{
    if (!icon || !*icon)
        return NULL;
    if (icon[0] == '~' && icon[1] == '/') {
        char *p = g_build_filename(g_get_home_dir(), icon + 2, NULL);
        if (g_file_test(p, G_FILE_TEST_IS_REGULAR))
            return p;
        g_free(p);
        return NULL;
    }
    if (g_path_is_absolute(icon))
        return g_file_test(icon, G_FILE_TEST_IS_REGULAR) ? g_strdup(icon) : NULL;
    if (strchr(icon, '/') && strstr(icon, ".."))
        return NULL;
    for (char **d = attrs->icon_dirs; d && *d; d++) {
        char *p = g_build_filename(*d, icon, NULL);
        if (g_file_test(p, G_FILE_TEST_IS_REGULAR))
            return p;
        g_free(p);
    }
    return NULL;
}

/* One "key = { ... }" entry: keeps Icon when AlwaysUserIcon is YES. */
static gboolean
read_entry(BtdWmAttrs *attrs, Lexer *lx, const char *key, GHashTable *icons)
{
    if (!is_punct(lx, '{'))
        return skip_value(lx, 0);
    next(lx);
    char *icon = NULL;
    gboolean always = FALSE;
    while (!is_punct(lx, '}')) {
        if (lx->kind != TOK_STRING) {
            g_free(icon);
            return FALSE;
        }
        char *name = g_strdup(lx->text->str);
        next(lx);
        if (!is_punct(lx, '=')) {
            g_free(name);
            g_free(icon);
            return FALSE;
        }
        next(lx);
        if (lx->kind == TOK_STRING && g_strcmp0(name, "Icon") == 0) {
            g_free(icon);
            icon = g_strdup(lx->text->str);
            next(lx);
        } else if (lx->kind == TOK_STRING && g_strcmp0(name, "AlwaysUserIcon") == 0) {
            always = g_ascii_strcasecmp(lx->text->str, "YES") == 0 || g_strcmp0(lx->text->str, "1") == 0;
            next(lx);
        } else if (!skip_value(lx, 1)) {
            g_free(name);
            g_free(icon);
            return FALSE;
        }
        g_free(name);
        if (is_punct(lx, ';'))
            next(lx);
    }
    next(lx);
    if (always && g_strcmp0(key, "*") != 0) {
        char *path = resolve_icon(attrs, icon);
        if (path)
            g_hash_table_replace(icons, g_strdup(key), path);
    }
    g_free(icon);
    return TRUE;
}

static gint
cmp_str(gconstpointer a, gconstpointer b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static char *
make_signature(GHashTable *icons)
{
    GPtrArray *lines = g_ptr_array_new_with_free_func(g_free);
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, icons);
    while (g_hash_table_iter_next(&it, &k, &v))
        g_ptr_array_add(lines, g_strconcat(k, "=", v, NULL));
    g_ptr_array_sort(lines, cmp_str);
    GString *s = g_string_new(NULL);
    for (guint i = 0; i < lines->len; i++) {
        g_string_append(s, lines->pdata[i]);
        g_string_append_c(s, '\n');
    }
    g_ptr_array_unref(lines);
    return g_string_free(s, FALSE);
}

gboolean
btd_wmattrs_load_data(BtdWmAttrs *attrs, const char *data, gssize length)
{
    GHashTable *icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (data) {
        Lexer lx = { data, data + (length < 0 ? (gssize)strlen(data) : length), TOK_END, 0, g_string_new(NULL) };
        next(&lx);
        if (is_punct(&lx, '{')) {
            next(&lx);
            while (lx.kind == TOK_STRING) {
                char *key = g_strdup(lx.text->str);
                next(&lx);
                gboolean ok = is_punct(&lx, '=');
                if (ok) {
                    next(&lx);
                    ok = read_entry(attrs, &lx, key, icons);
                }
                g_free(key);
                if (!ok)
                    break; /* keep what was read before the error */
                if (is_punct(&lx, ';'))
                    next(&lx);
            }
        }
        g_string_free(lx.text, TRUE);
    }
    char *sig = make_signature(icons);
    gboolean changed = g_strcmp0(sig, attrs->signature) != 0;
    g_free(attrs->signature);
    attrs->signature = sig;
    g_hash_table_unref(attrs->icons);
    attrs->icons = icons;
    return changed;
}

BtdWmAttrs *
btd_wmattrs_new(const char *path, const char *const *icon_dirs)
{
    BtdWmAttrs *attrs = g_new0(BtdWmAttrs, 1);
    if (path) {
        attrs->path = g_strdup(path);
    } else {
        char *root = gnustep_root();
        attrs->path = g_build_filename(root, "Defaults", "WMWindowAttributes", NULL);
        g_free(root);
    }
    attrs->icon_dirs = icon_dirs ? g_strdupv((char **)icon_dirs) : default_icon_dirs();
    attrs->icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    attrs->signature = g_strdup("");
    return attrs;
}

void
btd_wmattrs_free(BtdWmAttrs *attrs)
{
    if (!attrs)
        return;
    g_free(attrs->path);
    g_strfreev(attrs->icon_dirs);
    g_hash_table_unref(attrs->icons);
    g_free(attrs->signature);
    g_free(attrs);
}

const char *
btd_wmattrs_path(BtdWmAttrs *attrs)
{
    return attrs->path;
}

gboolean
btd_wmattrs_reload(BtdWmAttrs *attrs)
{
    char *data = NULL;
    gsize len = 0;
    GStatBuf st;
    if (g_stat(attrs->path, &st) != 0 || st.st_size > MAX_ATTRS_FILE ||
        !g_file_get_contents(attrs->path, &data, &len, NULL))
        return btd_wmattrs_load_data(attrs, NULL, 0);
    gboolean changed = btd_wmattrs_load_data(attrs, data, (gssize)len);
    g_free(data);
    return changed;
}

const char *
btd_wmattrs_icon_for_window(BtdWmAttrs *attrs, const char *instance, const char *wm_class)
{
    const char *icon = NULL;
    if (!attrs || g_hash_table_size(attrs->icons) == 0)
        return NULL;
    if (instance && wm_class) {
        char *both = g_strconcat(instance, ".", wm_class, NULL);
        icon = g_hash_table_lookup(attrs->icons, both);
        g_free(both);
    }
    if (!icon && instance)
        icon = g_hash_table_lookup(attrs->icons, instance);
    if (!icon && wm_class)
        icon = g_hash_table_lookup(attrs->icons, wm_class);
    return icon;
}

const char *
btd_wmattrs_icon_for_class(BtdWmAttrs *attrs, const char *wm_class)
{
    if (!attrs || !wm_class || !*wm_class || g_hash_table_size(attrs->icons) == 0)
        return NULL;
    const char *icon = g_hash_table_lookup(attrs->icons, wm_class);
    if (icon)
        return icon;
    /* level 0: whole key, case-insensitive; 1: class part; 2: instance
     * part. Within a level the smallest key wins, so the result does not
     * depend on the order of the hash table. */
    const char *best_key[3] = { NULL, NULL, NULL }, *best_icon[3] = { NULL, NULL, NULL };
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, attrs->icons);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        const char *key = k, *dot = strrchr(key, '.');
        int level = -1;
        if (g_ascii_strcasecmp(key, wm_class) == 0)
            level = 0;
        else if (dot && g_ascii_strcasecmp(dot + 1, wm_class) == 0)
            level = 1;
        else if (dot && (size_t)(dot - key) == strlen(wm_class) && g_ascii_strncasecmp(key, wm_class, dot - key) == 0)
            level = 2;
        if (level >= 0 && (!best_key[level] || strcmp(key, best_key[level]) < 0)) {
            best_key[level] = key;
            best_icon[level] = v;
        }
    }
    for (int i = 0; i < 3; i++)
        if (best_icon[i])
            return best_icon[i];
    return NULL;
}
