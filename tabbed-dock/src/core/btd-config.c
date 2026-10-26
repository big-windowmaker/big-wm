/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-config.h"
#include "btd-desktop.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

#define GROUP_DOCK "Dock"
#define TAB_PREFIX "Tab "
#define ITEM_PREFIX "Item "
#define MAX_CONFIG_SIZE (4 * 1024 * 1024)
#define MAX_PATH_LEN 4096

static const char *edge_names[] = { "bottom", "top" };
static const char *align_names[] = { "center", "left", "right", "fill" };
static const char *style_names[] = { "classic", "modern" };

const char *btd_edge_to_string(BtdEdge e) { return edge_names[e]; }
const char *btd_align_to_string(BtdAlign a) { return align_names[a]; }
const char *btd_style_to_string(BtdStyle s) { return style_names[s]; }

static int
parse_enum(const char *value, const char **names, guint n, int fallback)
{
    for (guint i = 0; value && i < n; i++)
        if (g_ascii_strcasecmp(value, names[i]) == 0)
            return (int)i;
    return fallback;
}

void
btd_prefs_init_defaults(BtdPrefs *p)
{
    memset(p, 0, sizeof(*p));
    p->edge = BTD_EDGE_BOTTOM;
    p->align = BTD_ALIGN_CENTER;
    p->monitor = g_strdup("primary");
    p->icon_size = 48;
    p->autohide = FALSE;
    p->reserve_space = TRUE;
    p->animations = TRUE;
    p->show_running = TRUE;
    p->click_minimizes = TRUE;
    p->global_shortcuts = TRUE;
    p->style = BTD_STYLE_CLASSIC;
}

void
btd_prefs_clear(BtdPrefs *p)
{
    g_clear_pointer(&p->monitor, g_free);
}

void
btd_prefs_copy(BtdPrefs *dst, const BtdPrefs *src)
{
    char *monitor = g_strdup(src->monitor);
    btd_prefs_clear(dst);
    *dst = *src;
    dst->monitor = monitor;
}

char *
btd_config_default_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "big-tabbed-dock", "dock.conf", NULL);
}

gboolean
btd_icon_name_is_valid(const char *icon)
{
    if (!icon || !*icon || strlen(icon) >= MAX_PATH_LEN)
        return FALSE;
    if (g_path_is_absolute(icon))
        return g_utf8_validate(icon, -1, NULL) && !strstr(icon, "/../");
    for (const char *p = icon; *p; p++)
        if (!g_ascii_isalnum(*p) && !strchr("._-+", *p))
            return FALSE;
    return TRUE;
}

static gboolean
path_is_valid(const char *path)
{
    return path && g_path_is_absolute(path) && strlen(path) < MAX_PATH_LEN &&
           g_utf8_validate(path, -1, NULL) && !strchr(path, '\n');
}

gboolean
btd_item_is_valid(const BtdItem *item)
{
    if (!item)
        return FALSE;
    if (item->icon && !btd_icon_name_is_valid(item->icon))
        return FALSE;
    switch (item->type) {
    case BTD_ITEM_SEPARATOR:
        return TRUE;
    case BTD_ITEM_APP:
        if (!item->target)
            return FALSE;
        if (g_path_is_absolute(item->target))
            return path_is_valid(item->target) && g_str_has_suffix(item->target, ".desktop");
        return btd_desktop_id_is_valid(item->target);
    case BTD_ITEM_FILE:
    case BTD_ITEM_FOLDER:
    case BTD_ITEM_APPIMAGE:
        return path_is_valid(item->target);
    }
    return FALSE;
}

char *
btd_config_to_data(BtdModel *model, const BtdPrefs *p, gsize *length)
{
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_integer(kf, GROUP_DOCK, "Version", BTD_CONFIG_VERSION);
    g_key_file_set_string(kf, GROUP_DOCK, "Position", btd_edge_to_string(p->edge));
    g_key_file_set_string(kf, GROUP_DOCK, "Alignment", btd_align_to_string(p->align));
    g_key_file_set_string(kf, GROUP_DOCK, "Monitor", p->monitor ? p->monitor : "primary");
    g_key_file_set_integer(kf, GROUP_DOCK, "IconSize", p->icon_size);
    g_key_file_set_boolean(kf, GROUP_DOCK, "AutoHide", p->autohide);
    g_key_file_set_boolean(kf, GROUP_DOCK, "ReserveSpace", p->reserve_space);
    g_key_file_set_boolean(kf, GROUP_DOCK, "Animations", p->animations);
    g_key_file_set_boolean(kf, GROUP_DOCK, "ShowRunning", p->show_running);
    g_key_file_set_boolean(kf, GROUP_DOCK, "ClickMinimizes", p->click_minimizes);
    g_key_file_set_boolean(kf, GROUP_DOCK, "GlobalShortcuts", p->global_shortcuts);
    g_key_file_set_string(kf, GROUP_DOCK, "Style", btd_style_to_string(p->style));

    BtdTab *active = btd_model_get_active_tab(model);
    if (active)
        g_key_file_set_string(kf, GROUP_DOCK, "ActiveTab", active->id);

    GPtrArray *order = g_ptr_array_new();
    for (guint i = 0; i < model->tabs->len; i++)
        g_ptr_array_add(order, ((BtdTab *)g_ptr_array_index(model->tabs, i))->id);
    g_key_file_set_string_list(kf, GROUP_DOCK, "TabOrder", (const char *const *)order->pdata, order->len);
    g_ptr_array_unref(order);

    for (guint i = 0; i < model->tabs->len; i++) {
        BtdTab *tab = g_ptr_array_index(model->tabs, i);
        char *group = g_strconcat(TAB_PREFIX, tab->id, NULL);
        g_key_file_set_boolean(kf, group, "Builtin", tab->builtin);
        if (tab->label && *tab->label)
            g_key_file_set_string(kf, group, "Label", tab->label);
        g_key_file_set_integer(kf, group, "Items", (int)tab->items->len);
        g_free(group);
        for (guint j = 0; j < tab->items->len; j++) {
            BtdItem *item = g_ptr_array_index(tab->items, j);
            char *ig = g_strdup_printf(ITEM_PREFIX "%s %03u", tab->id, j);
            g_key_file_set_string(kf, ig, "Type", btd_item_type_to_string(item->type));
            if (item->target)
                g_key_file_set_string(kf, ig, "Target", item->target);
            if (item->icon)
                g_key_file_set_string(kf, ig, "Icon", item->icon);
            if (item->label)
                g_key_file_set_string(kf, ig, "Label", item->label);
            g_free(ig);
        }
    }
    char *data = g_key_file_to_data(kf, length, NULL);
    g_key_file_unref(kf);
    return data;
}

static gboolean
get_bool(GKeyFile *kf, const char *key, gboolean fallback)
{
    GError *err = NULL;
    gboolean v = g_key_file_get_boolean(kf, GROUP_DOCK, key, &err);
    if (err) {
        g_error_free(err);
        return fallback;
    }
    return v;
}

static int
compare_item_groups(gconstpointer a, gconstpointer b)
{
    const char *ga = *(const char *const *)a, *gb = *(const char *const *)b;
    const char *na = strrchr(ga, ' '), *nb = strrchr(gb, ' ');
    guint64 ia = na ? g_ascii_strtoull(na + 1, NULL, 10) : 0;
    guint64 ib = nb ? g_ascii_strtoull(nb + 1, NULL, 10) : 0;
    return ia < ib ? -1 : ia > ib;
}

static void
load_tab_items(GKeyFile *kf, char **groups, BtdModel *model, BtdTab *tab)
{
    char *prefix = g_strconcat(ITEM_PREFIX, tab->id, " ", NULL);
    GPtrArray *mine = g_ptr_array_new();
    for (char **g = groups; *g; g++) {
        if (g_str_has_prefix(*g, prefix)) {
            const char *num = *g + strlen(prefix);
            gboolean digits = *num != '\0';
            for (const char *c = num; *c; c++)
                digits = digits && g_ascii_isdigit(*c);
            if (digits)
                g_ptr_array_add(mine, *g);
        }
    }
    g_ptr_array_sort(mine, compare_item_groups);
    for (guint i = 0; i < mine->len; i++) {
        const char *g = g_ptr_array_index(mine, i);
        char *type_s = g_key_file_get_string(kf, g, "Type", NULL);
        BtdItemType type;
        if (!btd_item_type_from_string(type_s, &type)) {
            g_free(type_s);
            continue;
        }
        g_free(type_s);
        BtdItem *item = btd_item_new(type, NULL);
        item->target = g_key_file_get_string(kf, g, "Target", NULL);
        item->icon = g_key_file_get_string(kf, g, "Icon", NULL);
        char *label = g_key_file_get_string(kf, g, "Label", NULL);
        if (label) {
            item->label = btd_sanitize_label(label);
            if (!*item->label)
                g_clear_pointer(&item->label, g_free);
            g_free(label);
        }
        if (!btd_item_is_valid(item)) {
            g_message("dock.conf: ignoring invalid item [%s]", g);
            btd_item_free(item);
            continue;
        }
        btd_model_insert_item(model, tab->id, item, -1);
    }
    g_ptr_array_unref(mine);
    g_free(prefix);
}

static BtdTab *
load_tab(GKeyFile *kf, char **groups, BtdModel *model, const char *id)
{
    char *group = g_strconcat(TAB_PREFIX, id, NULL);
    BtdTab *tab = NULL;
    if (g_key_file_has_group(kf, group) && btd_tab_id_is_valid(id) && !btd_model_find_tab(model, id, NULL)) {
        char *raw = g_key_file_get_string(kf, group, "Label", NULL);
        char *label = btd_sanitize_label(raw);
        g_free(raw);
        gboolean builtin = btd_tab_id_is_builtin(id);
        if (builtin || *label)
            tab = btd_model_insert_tab(model, id, label, builtin, -1);
        if (tab)
            load_tab_items(kf, groups, model, tab);
        g_free(label);
    }
    g_free(group);
    return tab;
}

gboolean
btd_config_from_data(const char *data, gssize length, BtdModel *model, BtdPrefs *prefs, GError **error)
{
    GKeyFile *kf = g_key_file_new();
    if (length < 0)
        length = (gssize)strlen(data);
    if (!g_key_file_load_from_data(kf, data, (gsize)length, G_KEY_FILE_NONE, error)) {
        g_key_file_unref(kf);
        return FALSE;
    }
    if (!g_key_file_has_group(kf, GROUP_DOCK)) {
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND, "missing [Dock] group");
        g_key_file_unref(kf);
        return FALSE;
    }
    int version = g_key_file_get_integer(kf, GROUP_DOCK, "Version", NULL);
    if (version < 1) {
        g_set_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE, "missing or invalid Version");
        g_key_file_unref(kf);
        return FALSE;
    }

    BtdPrefs p;
    btd_prefs_init_defaults(&p);
    char *s;
    s = g_key_file_get_string(kf, GROUP_DOCK, "Position", NULL);
    p.edge = parse_enum(s, edge_names, G_N_ELEMENTS(edge_names), p.edge);
    g_free(s);
    s = g_key_file_get_string(kf, GROUP_DOCK, "Alignment", NULL);
    p.align = parse_enum(s, align_names, G_N_ELEMENTS(align_names), p.align);
    g_free(s);
    s = g_key_file_get_string(kf, GROUP_DOCK, "Style", NULL);
    p.style = parse_enum(s, style_names, G_N_ELEMENTS(style_names), p.style);
    g_free(s);
    s = g_key_file_get_string(kf, GROUP_DOCK, "Monitor", NULL);
    if (s && *s && strlen(s) < 128 && g_utf8_validate(s, -1, NULL)) {
        g_free(p.monitor);
        p.monitor = s;
    } else {
        g_free(s);
    }
    GError *err = NULL;
    int size = g_key_file_get_integer(kf, GROUP_DOCK, "IconSize", &err);
    if (!err && size >= BTD_ICON_SIZE_MIN && size <= BTD_ICON_SIZE_MAX)
        p.icon_size = size;
    g_clear_error(&err);
    p.autohide = get_bool(kf, "AutoHide", p.autohide);
    p.reserve_space = get_bool(kf, "ReserveSpace", p.reserve_space);
    p.animations = get_bool(kf, "Animations", p.animations);
    p.show_running = get_bool(kf, "ShowRunning", p.show_running);
    p.click_minimizes = get_bool(kf, "ClickMinimizes", p.click_minimizes);
    p.global_shortcuts = get_bool(kf, "GlobalShortcuts", p.global_shortcuts);

    btd_model_freeze(model);
    btd_model_clear(model);
    char **groups = g_key_file_get_groups(kf, NULL);
    char **order = g_key_file_get_string_list(kf, GROUP_DOCK, "TabOrder", NULL, NULL);
    for (char **id = order; id && *id; id++)
        load_tab(kf, groups, model, *id);
    /* Tabs present in the file but missing from TabOrder are not lost. */
    for (char **g = groups; *g; g++)
        if (g_str_has_prefix(*g, TAB_PREFIX))
            load_tab(kf, groups, model, *g + strlen(TAB_PREFIX));
    /* Built-in tabs always exist (they can be renamed, not removed). */
    const char *builtins[] = { BTD_TAB_GENERAL, BTD_TAB_DEVELOPER, BTD_TAB_DOCUMENTS, BTD_TAB_TEMPORARY };
    for (guint i = 0; i < G_N_ELEMENTS(builtins); i++)
        if (!btd_model_find_tab(model, builtins[i], NULL))
            btd_model_insert_tab(model, builtins[i], NULL, TRUE, (int)MIN(i, model->tabs->len));
    s = g_key_file_get_string(kf, GROUP_DOCK, "ActiveTab", NULL);
    if (!btd_model_set_active_tab(model, s))
        btd_model_set_active_tab(model, ((BtdTab *)g_ptr_array_index(model->tabs, 0))->id);
    g_free(s);
    g_strfreev(order);
    g_strfreev(groups);
    btd_model_thaw(model);

    btd_prefs_clear(prefs);
    *prefs = p;
    g_key_file_unref(kf);
    return TRUE;
}

static gboolean
load_file(const char *path, BtdModel *model, BtdPrefs *prefs, GError **error)
{
    char *data = NULL;
    gsize len = 0;
    GStatBuf st;
    if (g_stat(path, &st) == 0 && st.st_size > MAX_CONFIG_SIZE) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s is too large", path);
        return FALSE;
    }
    if (!g_file_get_contents(path, &data, &len, error))
        return FALSE;
    gboolean ok = btd_config_from_data(data, (gssize)len, model, prefs, error);
    g_free(data);
    return ok;
}

BtdLoadResult
btd_config_load(const char *path, BtdModel *model, BtdPrefs *prefs, GError **error)
{
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        char *bak = g_strconcat(path, ".bak", NULL);
        gboolean from_bak = g_file_test(bak, G_FILE_TEST_EXISTS) && load_file(bak, model, prefs, NULL);
        g_free(bak);
        return from_bak ? BTD_LOAD_FROM_BACKUP : BTD_LOAD_MISSING;
    }
    GError *err = NULL;
    if (load_file(path, model, prefs, &err))
        return BTD_LOAD_OK;

    /* Keep the corrupt file for the user (never delete it), then try the backup. */
    GDateTime *now = g_date_time_new_now_local();
    char *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    char *broken = g_strdup_printf("%s.broken-%s", path, stamp);
    if (g_rename(path, broken) != 0)
        g_message("could not keep corrupt %s: %s", path, g_strerror(errno));
    else
        g_message("%s is unreadable (%s); kept as %s", path, err->message, broken);
    g_date_time_unref(now);
    g_free(stamp);
    g_free(broken);
    g_propagate_error(error, err);

    char *bak = g_strconcat(path, ".bak", NULL);
    gboolean ok = g_file_test(bak, G_FILE_TEST_EXISTS) && load_file(bak, model, prefs, NULL);
    g_free(bak);
    return ok ? BTD_LOAD_FROM_BACKUP : BTD_LOAD_CORRUPT_DEFAULTS;
}

gboolean
btd_config_save(const char *path, BtdModel *model, const BtdPrefs *prefs, GError **error)
{
    char *dir = g_path_get_dirname(path);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "cannot create %s: %s", dir,
                    g_strerror(errno));
        g_free(dir);
        return FALSE;
    }
    g_free(dir);

    gsize len;
    char *data = btd_config_to_data(model, prefs, &len);

    /* Rotate the current file to .bak only when it is valid and different. */
    char *old = NULL;
    gsize old_len = 0;
    if (g_file_get_contents(path, &old, &old_len, NULL)) {
        if (old_len == len && memcmp(old, data, len) == 0) {
            g_free(old);
            g_free(data);
            return TRUE; /* nothing changed */
        }
        BtdModel *probe = btd_model_new();
        BtdPrefs pp;
        btd_prefs_init_defaults(&pp);
        if (btd_config_from_data(old, (gssize)old_len, probe, &pp, NULL)) {
            char *bak = g_strconcat(path, ".bak", NULL);
            g_file_set_contents_full(bak, old, (gssize)old_len, G_FILE_SET_CONTENTS_CONSISTENT, 0600, NULL);
            g_free(bak);
        }
        btd_prefs_clear(&pp);
        btd_model_free(probe);
        g_free(old);
    }
    gboolean ok = g_file_set_contents_full(path, data, (gssize)len,
                                           G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE,
                                           0600, error);
    g_free(data);
    return ok;
}
