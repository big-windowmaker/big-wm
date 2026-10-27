/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-desktop.h"

#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_DESKTOP_FILE_SIZE (256 * 1024)
#define MAX_SCAN_DEPTH 4

struct _BtdDesktopIndex {
    GHashTable *by_id;      /* id -> BtdDesktopEntry* (owned) */
    GHashTable *by_path;    /* path -> entry (borrowed) */
    GHashTable *by_wmclass; /* lowercase StartupWMClass -> entry */
    GHashTable *by_idbase;  /* lowercase id without ".desktop" -> entry */
    GHashTable *by_exec;    /* program basename -> entry */
    GHashTable *by_flatpak; /* lowercase flatpak id -> entry */
};

/* Programs that say nothing about which application is running. */
static const char *ambiguous_programs[] = {
    "sh", "bash", "dash", "zsh", "env", "python", "python3", "perl", "ruby", "java",
    "gtk-launch", "xdg-open", "gio", "big-wm-open", "flatpak", "pkexec", "sudo",
    "electron", "wine", "mono", "node", "exo-open", "kioclient", "kioclient5", NULL
};

gboolean
btd_desktop_id_is_valid(const char *id)
{
    if (!id || !g_str_has_suffix(id, ".desktop") || strlen(id) > 255 || strlen(id) <= 8)
        return FALSE;
    for (const char *p = id; *p; p++)
        if (!g_ascii_isalnum(*p) && !strchr("._-+@", *p))
            return FALSE;
    return id[0] != '.' && id[0] != '-';
}

char **
btd_desktop_data_dirs(void)
{
    GPtrArray *dirs = g_ptr_array_new();
    g_ptr_array_add(dirs, g_strdup(g_get_user_data_dir()));
    for (const char *const *d = g_get_system_data_dirs(); *d; d++)
        g_ptr_array_add(dirs, g_strdup(*d));
    char *user_flatpak = g_build_filename(g_get_user_data_dir(), "flatpak", "exports", "share", NULL);
    g_ptr_array_add(dirs, user_flatpak);
    g_ptr_array_add(dirs, g_strdup("/var/lib/flatpak/exports/share"));

    /* de-duplicate, keeping first occurrence (that one has precedence) */
    GPtrArray *out = g_ptr_array_new();
    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0; i < dirs->len; i++) {
        char *d = g_ptr_array_index(dirs, i);
        char *canon = g_canonicalize_filename(d, "/");
        if (*d && g_path_is_absolute(d) && !g_hash_table_contains(seen, canon)) {
            g_hash_table_add(seen, canon);
            g_ptr_array_add(out, canon);
        } else {
            g_free(canon);
        }
        g_free(d);
    }
    g_hash_table_unref(seen);
    g_ptr_array_unref(dirs);
    g_ptr_array_add(out, NULL);
    return (char **)g_ptr_array_free(out, FALSE);
}

static gboolean
is_ambiguous(const char *prog)
{
    return g_strv_contains(ambiguous_programs, prog);
}

char *
btd_desktop_exec_program(const char *exec, char **flatpak_id_out)
{
    char **argv = NULL;
    char *ret = NULL;
    if (flatpak_id_out)
        *flatpak_id_out = NULL;
    if (!exec || !g_shell_parse_argv(exec, NULL, &argv, NULL))
        return NULL;

    guint i = 0;
    char *first = argv[0] ? g_path_get_basename(argv[0]) : NULL;
    gboolean is_env = g_strcmp0(first, "env") == 0;
    g_free(first);
    if (is_env) {
        i++;
        while (argv[i] && (strchr(argv[i], '=') || argv[i][0] == '-'))
            i++;
    }
    if (argv[i]) {
        char *base = g_path_get_basename(argv[i]);
        if (g_strcmp0(base, "flatpak") == 0) {
            /* flatpak run [--opts] <app-id> */
            for (guint j = i + 1; argv[j]; j++) {
                if (g_strcmp0(argv[j], "run") == 0) {
                    for (guint k = j + 1; argv[k]; k++) {
                        if (argv[k][0] != '-') {
                            if (flatpak_id_out)
                                *flatpak_id_out = g_strdup(argv[k]);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        ret = base;
    }
    g_strfreev(argv);
    return ret;
}

static gboolean
program_available(const char *prog)
{
    if (!prog || !*prog)
        return FALSE;
    if (g_path_is_absolute(prog))
        return g_file_test(prog, G_FILE_TEST_IS_EXECUTABLE) && !g_file_test(prog, G_FILE_TEST_IS_DIR);
    char *found = g_find_program_in_path(prog);
    gboolean ok = found != NULL;
    g_free(found);
    return ok;
}

static gboolean
shown_in_current_desktop(GKeyFile *kf)
{
    const char *current = g_getenv("XDG_CURRENT_DESKTOP");
    char **desktops = current ? g_strsplit(current, ":", -1) : g_new0(char *, 1);
    char **only = g_key_file_get_string_list(kf, "Desktop Entry", "OnlyShowIn", NULL, NULL);
    char **not = g_key_file_get_string_list(kf, "Desktop Entry", "NotShowIn", NULL, NULL);
    gboolean shown = TRUE;
    if (only) {
        shown = FALSE;
        for (char **d = desktops; *d && !shown; d++)
            shown = g_strv_contains((const char *const *)only, *d);
    }
    if (shown && not)
        for (char **d = desktops; *d && shown; d++)
            shown = !g_strv_contains((const char *const *)not, *d);
    g_strfreev(desktops);
    g_strfreev(only);
    g_strfreev(not);
    return shown;
}

BtdDesktopEntry *
btd_desktop_entry_load(const char *path, const char *id)
{
    GStatBuf st;
    if (!path || g_stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > MAX_DESKTOP_FILE_SIZE)
        return NULL;

    GKeyFile *kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL) ||
        !g_key_file_has_group(kf, "Desktop Entry")) {
        g_key_file_unref(kf);
        return NULL;
    }
    const char *grp = "Desktop Entry";
    char *type = g_key_file_get_string(kf, grp, "Type", NULL);
    gboolean hidden = g_key_file_get_boolean(kf, grp, "Hidden", NULL);
    if (g_strcmp0(type, "Application") != 0 || hidden) {
        g_free(type);
        g_key_file_unref(kf);
        return NULL;
    }
    g_free(type);

    BtdDesktopEntry *e = g_new0(BtdDesktopEntry, 1);
    e->path = g_strdup(path);
    e->id = id ? g_strdup(id) : g_path_get_basename(path);
    e->name = g_key_file_get_locale_string(kf, grp, "Name", NULL, NULL);
    if (e->name) {
        char *v = g_utf8_make_valid(e->name, -1);
        g_free(e->name);
        e->name = v;
    }
    e->icon = g_key_file_get_string(kf, grp, "Icon", NULL);
    e->startup_wm_class = g_key_file_get_string(kf, grp, "StartupWMClass", NULL);
    e->no_display = g_key_file_get_boolean(kf, grp, "NoDisplay", NULL) || !shown_in_current_desktop(kf);
    e->dbus_activatable = g_key_file_get_boolean(kf, grp, "DBusActivatable", NULL);
    e->terminal = g_key_file_get_boolean(kf, grp, "Terminal", NULL);

    char *exec = g_key_file_get_string(kf, grp, "Exec", NULL);
    char *flatpak = NULL;
    e->exec_base = btd_desktop_exec_program(exec, &flatpak);
    e->flatpak_id = g_key_file_get_string(kf, grp, "X-Flatpak", NULL);
    if (!e->flatpak_id)
        e->flatpak_id = flatpak;
    else
        g_free(flatpak);

    char *tryexec = g_key_file_get_string(kf, grp, "TryExec", NULL);
    if (tryexec) {
        e->available = program_available(tryexec);
    } else if (exec) {
        char **argv = NULL;
        e->available = g_shell_parse_argv(exec, NULL, &argv, NULL) && argv[0] &&
                       (g_strcmp0(argv[0], "env") == 0 || program_available(argv[0]));
        g_strfreev(argv);
    } else {
        e->available = e->dbus_activatable;
    }
    g_free(tryexec);
    g_free(exec);
    g_key_file_unref(kf);
    return e;
}

void
btd_desktop_entry_free(BtdDesktopEntry *e)
{
    if (!e)
        return;
    g_free(e->id);
    g_free(e->path);
    g_free(e->name);
    g_free(e->icon);
    g_free(e->startup_wm_class);
    g_free(e->exec_base);
    g_free(e->flatpak_id);
    g_free(e);
}

static void
index_add(BtdDesktopIndex *index, BtdDesktopEntry *e)
{
    if (g_hash_table_contains(index->by_id, e->id)) {
        btd_desktop_entry_free(e); /* earlier data dir wins */
        return;
    }
    g_hash_table_insert(index->by_id, e->id, e);
    g_hash_table_insert(index->by_path, e->path, e);
    if (e->startup_wm_class && *e->startup_wm_class)
        g_hash_table_insert(index->by_wmclass, g_ascii_strdown(e->startup_wm_class, -1), e);
    char *base = g_ascii_strdown(e->id, strlen(e->id) - strlen(".desktop"));
    g_hash_table_insert(index->by_idbase, base, e);
    if (e->flatpak_id)
        g_hash_table_insert(index->by_flatpak, g_ascii_strdown(e->flatpak_id, -1), e);
    if (e->exec_base && !is_ambiguous(e->exec_base)) {
        BtdDesktopEntry *old = g_hash_table_lookup(index->by_exec, e->exec_base);
        /* Prefer visible entries over NoDisplay helpers that run the same program. */
        if (!old || (old->no_display && !e->no_display))
            g_hash_table_insert(index->by_exec, g_strdup(e->exec_base), e);
    }
}

static void
scan_dir(BtdDesktopIndex *index, const char *dir, const char *prefix, int depth)
{
    if (depth > MAX_SCAN_DEPTH)
        return;
    GDir *d = g_dir_open(dir, 0, NULL);
    if (!d)
        return;
    const char *name;
    while ((name = g_dir_read_name(d))) {
        char *path = g_build_filename(dir, name, NULL);
        if (g_str_has_suffix(name, ".desktop")) {
            char *id = g_strconcat(prefix, name, NULL);
            if (btd_desktop_id_is_valid(id) && !g_hash_table_contains(index->by_id, id)) {
                BtdDesktopEntry *e = btd_desktop_entry_load(path, id);
                if (e)
                    index_add(index, e);
            }
            g_free(id);
        } else if (g_file_test(path, G_FILE_TEST_IS_DIR) && !g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
            /* Subdirectories form the id with '-' (Desktop Entry Spec). */
            char *sub_prefix = g_strconcat(prefix, name, "-", NULL);
            scan_dir(index, path, sub_prefix, depth + 1);
            g_free(sub_prefix);
        }
        g_free(path);
    }
    g_dir_close(d);
}

BtdDesktopIndex *
btd_desktop_index_new(const char *const *data_dirs)
{
    BtdDesktopIndex *index = g_new0(BtdDesktopIndex, 1);
    index->by_id = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)btd_desktop_entry_free);
    index->by_path = g_hash_table_new(g_str_hash, g_str_equal);
    index->by_wmclass = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->by_idbase = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->by_exec = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->by_flatpak = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    for (const char *const *d = data_dirs; d && *d; d++) {
        char *apps = g_build_filename(*d, "applications", NULL);
        scan_dir(index, apps, "", 0);
        g_free(apps);
    }
    return index;
}

void
btd_desktop_index_free(BtdDesktopIndex *index)
{
    if (!index)
        return;
    g_hash_table_unref(index->by_path);
    g_hash_table_unref(index->by_wmclass);
    g_hash_table_unref(index->by_idbase);
    g_hash_table_unref(index->by_exec);
    g_hash_table_unref(index->by_flatpak);
    g_hash_table_unref(index->by_id);
    g_free(index);
}

guint
btd_desktop_index_size(BtdDesktopIndex *index)
{
    return g_hash_table_size(index->by_id);
}

const BtdDesktopEntry *
btd_desktop_index_lookup(BtdDesktopIndex *index, const char *id)
{
    return id ? g_hash_table_lookup(index->by_id, id) : NULL;
}

const BtdDesktopEntry *
btd_desktop_index_lookup_path(BtdDesktopIndex *index, const char *path)
{
    if (!path)
        return NULL;
    const BtdDesktopEntry *e = g_hash_table_lookup(index->by_path, path);
    if (!e) {
        char *canon = g_canonicalize_filename(path, "/");
        e = g_hash_table_lookup(index->by_path, canon);
        g_free(canon);
    }
    return e;
}

const BtdDesktopEntry *
btd_desktop_index_first_available(BtdDesktopIndex *index, const char *const *ids)
{
    for (; ids && *ids; ids++) {
        const BtdDesktopEntry *e = btd_desktop_index_lookup(index, *ids);
        if (e && e->available)
            return e;
    }
    return NULL;
}

GList *
btd_desktop_index_entries(BtdDesktopIndex *index)
{
    return g_hash_table_get_values(index->by_id);
}

static const BtdDesktopEntry *
lookup_lower(GHashTable *table, const char *key)
{
    if (!key || !*key)
        return NULL;
    char *lower = g_ascii_strdown(key, -1);
    const BtdDesktopEntry *e = g_hash_table_lookup(table, lower);
    g_free(lower);
    return e;
}

static const BtdDesktopEntry *
match_name(BtdDesktopIndex *index, const char *name)
{
    const BtdDesktopEntry *e;
    if (!name || !*name)
        return NULL;
    if ((e = lookup_lower(index->by_wmclass, name)))
        return e;
    if ((e = lookup_lower(index->by_idbase, name)))
        return e;
    if ((e = lookup_lower(index->by_flatpak, name)))
        return e;
    return NULL;
}

const char *
btd_desktop_index_match(BtdDesktopIndex *index, const BtdWindowKeys *k)
{
    const BtdDesktopEntry *e = NULL;
    if (!index || !k)
        return NULL;

    /* 1. explicit application ids (Wayland app_id, GTK application id) */
    if (k->app_id && *k->app_id) {
        char *id = g_strconcat(k->app_id, ".desktop", NULL);
        e = btd_desktop_index_lookup(index, id);
        g_free(id);
        if (!e)
            e = match_name(index, k->app_id);
    }
    /* 2. the desktop file GIO launched this client from */
    if (!e && k->launched_desktop_file) {
        e = btd_desktop_index_lookup_path(index, k->launched_desktop_file);
        if (!e) {
            char *base = g_path_get_basename(k->launched_desktop_file);
            e = btd_desktop_index_lookup(index, base);
            g_free(base);
        }
    }
    /* 3. X11 WM_CLASS (StartupWMClass first, then id-like names) */
    if (!e)
        e = match_name(index, k->wm_class);
    if (!e)
        e = match_name(index, k->wm_instance);
    /* 4. program names */
    if (!e && k->wm_instance && !is_ambiguous(k->wm_instance))
        e = g_hash_table_lookup(index->by_exec, k->wm_instance);
    if (!e && k->exe && !is_ambiguous(k->exe))
        e = g_hash_table_lookup(index->by_exec, k->exe);
    if (!e && k->app_id && !is_ambiguous(k->app_id))
        e = g_hash_table_lookup(index->by_exec, k->app_id);
    /* 5. reverse-DNS app ids whose last component is the program (org.foo.Bar -> bar)
     * or a StartupWMClass: Qt makes io.calamares.calamares on Wayland from
     * the organization domain, where X11 has WM_CLASS "calamares". */
    if (!e && k->app_id && strchr(k->app_id, '.')) {
        const char *last = strrchr(k->app_id, '.') + 1;
        char *lower = g_ascii_strdown(last, -1);
        if (!is_ambiguous(lower)) {
            e = g_hash_table_lookup(index->by_exec, lower);
            if (!e && *lower)
                e = g_hash_table_lookup(index->by_wmclass, lower);
        }
        g_free(lower);
    }
    return e ? e->id : NULL;
}
