/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-model.h"

#include <string.h>

static const struct {
    const char *id;
    const char *label;
} builtin_tabs[] = {
    { BTD_TAB_GENERAL, "General" },
    { BTD_TAB_DEVELOPER, "Developer" },
    { BTD_TAB_DOCUMENTS, "Documents" },
    { BTD_TAB_TEMPORARY, "Temporary" },
};

static const char *type_names[] = {
    [BTD_ITEM_APP] = "app",
    [BTD_ITEM_FILE] = "file",
    [BTD_ITEM_FOLDER] = "folder",
    [BTD_ITEM_APPIMAGE] = "appimage",
    [BTD_ITEM_SEPARATOR] = "separator",
};

const char *
btd_item_type_to_string(BtdItemType type)
{
    if ((guint)type < G_N_ELEMENTS(type_names))
        return type_names[type];
    return "invalid";
}

gboolean
btd_item_type_from_string(const char *s, BtdItemType *out)
{
    if (!s)
        return FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(type_names); i++) {
        if (g_strcmp0(s, type_names[i]) == 0) {
            *out = (BtdItemType)i;
            return TRUE;
        }
    }
    return FALSE;
}

BtdItem *
btd_item_new(BtdItemType type, const char *target)
{
    BtdItem *item = g_new0(BtdItem, 1);
    item->type = type;
    item->target = g_strdup(target);
    return item;
}

BtdItem *
btd_item_copy(const BtdItem *item)
{
    BtdItem *copy = btd_item_new(item->type, item->target);
    copy->icon = g_strdup(item->icon);
    copy->label = g_strdup(item->label);
    return copy;
}

void
btd_item_free(BtdItem *item)
{
    if (!item)
        return;
    g_free(item->target);
    g_free(item->icon);
    g_free(item->label);
    g_free(item);
}

gboolean
btd_item_equal(const BtdItem *a, const BtdItem *b)
{
    if (a->type != b->type)
        return FALSE;
    if (a->type == BTD_ITEM_SEPARATOR)
        return FALSE; /* separators are never duplicates of each other */
    return g_strcmp0(a->target, b->target) == 0;
}

static void
tab_free(BtdTab *tab)
{
    g_free(tab->id);
    g_free(tab->label);
    g_ptr_array_unref(tab->items);
    g_free(tab);
}

static BtdTab *
tab_new(const char *id, const char *label, gboolean builtin)
{
    BtdTab *tab = g_new0(BtdTab, 1);
    tab->id = g_strdup(id);
    tab->label = (label && *label) ? btd_sanitize_label(label) : NULL;
    tab->builtin = builtin;
    tab->items = g_ptr_array_new_with_free_func((GDestroyNotify)btd_item_free);
    return tab;
}

static void
emit_changed(BtdModel *model)
{
    if (model->freeze > 0) {
        model->pending = TRUE;
        return;
    }
    if (model->changed)
        model->changed(model, model->changed_data);
}

BtdModel *
btd_model_new(void)
{
    BtdModel *model = g_new0(BtdModel, 1);
    model->tabs = g_ptr_array_new_with_free_func((GDestroyNotify)tab_free);
    return model;
}

void
btd_model_free(BtdModel *model)
{
    if (!model)
        return;
    g_ptr_array_unref(model->tabs);
    g_free(model->active_tab);
    g_free(model);
}

void
btd_model_set_changed_func(BtdModel *model, BtdModelChangedFunc func, gpointer data)
{
    model->changed = func;
    model->changed_data = data;
}

void
btd_model_freeze(BtdModel *model)
{
    model->freeze++;
}

void
btd_model_thaw(BtdModel *model)
{
    g_return_if_fail(model->freeze > 0);
    if (--model->freeze == 0 && model->pending) {
        model->pending = FALSE;
        emit_changed(model);
    }
}

void
btd_model_clear(BtdModel *model)
{
    g_ptr_array_set_size(model->tabs, 0);
    g_clear_pointer(&model->active_tab, g_free);
    emit_changed(model);
}

gboolean
btd_tab_id_is_builtin(const char *id)
{
    for (guint i = 0; i < G_N_ELEMENTS(builtin_tabs); i++)
        if (g_strcmp0(id, builtin_tabs[i].id) == 0)
            return TRUE;
    return FALSE;
}

const char *
btd_tab_default_label(const char *id)
{
    for (guint i = 0; i < G_N_ELEMENTS(builtin_tabs); i++)
        if (g_strcmp0(id, builtin_tabs[i].id) == 0)
            return builtin_tabs[i].label;
    return NULL;
}

void
btd_model_reset_builtin_tabs(BtdModel *model)
{
    btd_model_freeze(model);
    g_ptr_array_set_size(model->tabs, 0);
    for (guint i = 0; i < G_N_ELEMENTS(builtin_tabs); i++)
        g_ptr_array_add(model->tabs, tab_new(builtin_tabs[i].id, NULL, TRUE));
    g_free(model->active_tab);
    model->active_tab = g_strdup(BTD_TAB_GENERAL);
    emit_changed(model);
    btd_model_thaw(model);
}

guint
btd_model_n_tabs(BtdModel *model)
{
    return model->tabs->len;
}

BtdTab *
btd_model_get_tab(BtdModel *model, guint index)
{
    if (index >= model->tabs->len)
        return NULL;
    return g_ptr_array_index(model->tabs, index);
}

BtdTab *
btd_model_find_tab(BtdModel *model, const char *id, guint *index_out)
{
    if (!id)
        return NULL;
    for (guint i = 0; i < model->tabs->len; i++) {
        BtdTab *tab = g_ptr_array_index(model->tabs, i);
        if (g_strcmp0(tab->id, id) == 0) {
            if (index_out)
                *index_out = i;
            return tab;
        }
    }
    return NULL;
}

BtdTab *
btd_model_get_active_tab(BtdModel *model)
{
    BtdTab *tab = btd_model_find_tab(model, model->active_tab, NULL);
    if (!tab && model->tabs->len > 0)
        tab = g_ptr_array_index(model->tabs, 0);
    return tab;
}

gboolean
btd_model_set_active_tab(BtdModel *model, const char *id)
{
    if (!btd_model_find_tab(model, id, NULL))
        return FALSE;
    if (g_strcmp0(model->active_tab, id) == 0)
        return TRUE;
    g_free(model->active_tab);
    model->active_tab = g_strdup(id);
    emit_changed(model);
    return TRUE;
}

void
btd_model_cycle_active_tab(BtdModel *model, int delta)
{
    guint n = model->tabs->len, index = 0;
    if (n == 0)
        return;
    BtdTab *active = btd_model_get_active_tab(model);
    btd_model_find_tab(model, active->id, &index);
    int next = ((int)index + delta) % (int)n;
    if (next < 0)
        next += (int)n;
    btd_model_set_active_tab(model, btd_model_get_tab(model, (guint)next)->id);
}

gboolean
btd_tab_id_is_valid(const char *id)
{
    if (!id || !*id || strlen(id) > 64)
        return FALSE;
    for (const char *p = id; *p; p++)
        if (!g_ascii_islower(*p) && !g_ascii_isdigit(*p) && *p != '-')
            return FALSE;
    return TRUE;
}

char *
btd_sanitize_label(const char *label)
{
    if (!label)
        return g_strdup("");
    char *valid = g_utf8_make_valid(label, -1);
    GString *out = g_string_new(NULL);
    for (const char *p = valid; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_iscntrl(c))
            c = ' ';
        g_string_append_unichar(out, c);
    }
    g_free(valid);
    char *s = g_strstrip(g_string_free(out, FALSE));
    if (g_utf8_strlen(s, -1) > BTD_MAX_LABEL) {
        char *end = g_utf8_offset_to_pointer(s, BTD_MAX_LABEL);
        *end = '\0';
    }
    char *ret = g_strdup(s);
    g_free(s);
    return ret;
}

BtdTab *
btd_model_insert_tab(BtdModel *model, const char *id, const char *label, gboolean builtin, int position)
{
    if (!btd_tab_id_is_valid(id) || btd_model_find_tab(model, id, NULL) ||
        model->tabs->len >= BTD_MAX_TABS)
        return NULL;
    BtdTab *tab = tab_new(id, label, builtin && btd_tab_id_is_builtin(id));
    if (position < 0 || (guint)position > model->tabs->len)
        position = (int)model->tabs->len;
    g_ptr_array_insert(model->tabs, position, tab);
    emit_changed(model);
    return tab;
}

BtdTab *
btd_model_add_tab(BtdModel *model, const char *label, int position)
{
    char *clean = btd_sanitize_label(label);
    if (!*clean) {
        g_free(clean);
        return NULL;
    }
    char *id = NULL;
    for (guint n = 1; n < 10000; n++) {
        g_free(id);
        id = g_strdup_printf("custom-%u", n);
        if (!btd_model_find_tab(model, id, NULL))
            break;
    }
    BtdTab *tab = btd_model_insert_tab(model, id, clean, FALSE, position);
    g_free(id);
    g_free(clean);
    return tab;
}

gboolean
btd_model_rename_tab(BtdModel *model, const char *id, const char *label)
{
    BtdTab *tab = btd_model_find_tab(model, id, NULL);
    if (!tab)
        return FALSE;
    char *clean = btd_sanitize_label(label);
    if (!*clean) {
        g_free(clean);
        if (!tab->builtin)
            return FALSE; /* custom tabs need a name */
        clean = NULL;     /* built-in: empty label restores the translated default */
    }
    g_free(tab->label);
    tab->label = clean;
    emit_changed(model);
    return TRUE;
}

gboolean
btd_model_remove_tab(BtdModel *model, const char *id)
{
    guint index;
    BtdTab *tab = btd_model_find_tab(model, id, &index);
    if (!tab || tab->builtin || model->tabs->len <= 1)
        return FALSE;
    gboolean was_active = g_strcmp0(model->active_tab, id) == 0;
    g_ptr_array_remove_index(model->tabs, index);
    if (was_active) {
        BtdTab *next = btd_model_get_tab(model, index > 0 ? index - 1 : 0);
        g_free(model->active_tab);
        model->active_tab = g_strdup(next->id);
    }
    emit_changed(model);
    return TRUE;
}

gboolean
btd_model_move_tab(BtdModel *model, const char *id, guint new_index)
{
    guint index;
    if (!btd_model_find_tab(model, id, &index))
        return FALSE;
    if (new_index >= model->tabs->len)
        new_index = model->tabs->len - 1;
    if (new_index == index)
        return TRUE;
    BtdTab *tab = g_ptr_array_steal_index(model->tabs, index);
    g_ptr_array_insert(model->tabs, new_index, tab);
    emit_changed(model);
    return TRUE;
}

gboolean
btd_tab_contains(BtdTab *tab, const BtdItem *item)
{
    for (guint i = 0; i < tab->items->len; i++)
        if (btd_item_equal(g_ptr_array_index(tab->items, i), item))
            return TRUE;
    return FALSE;
}

gboolean
btd_model_insert_item(BtdModel *model, const char *tab_id, BtdItem *item, int position)
{
    BtdTab *tab = btd_model_find_tab(model, tab_id, NULL);
    if (!tab || !item || tab->items->len >= BTD_MAX_ITEMS ||
        (item->type != BTD_ITEM_SEPARATOR && (!item->target || !*item->target)) ||
        btd_tab_contains(tab, item)) {
        btd_item_free(item);
        return FALSE;
    }
    if (position < 0 || (guint)position > tab->items->len)
        position = (int)tab->items->len;
    g_ptr_array_insert(tab->items, position, item);
    emit_changed(model);
    return TRUE;
}

gboolean
btd_model_remove_item(BtdModel *model, const char *tab_id, guint index)
{
    BtdTab *tab = btd_model_find_tab(model, tab_id, NULL);
    if (!tab || index >= tab->items->len)
        return FALSE;
    g_ptr_array_remove_index(tab->items, index);
    emit_changed(model);
    return TRUE;
}

gboolean
btd_model_move_item(BtdModel *model, const char *from_tab, guint index,
                    const char *to_tab, int position, gboolean copy)
{
    BtdTab *src = btd_model_find_tab(model, from_tab, NULL);
    BtdTab *dst = btd_model_find_tab(model, to_tab, NULL);
    if (!src || !dst || index >= src->items->len)
        return FALSE;
    BtdItem *item = g_ptr_array_index(src->items, index);

    if (src == dst) {
        if (copy)
            return FALSE; /* a tab never holds the same shortcut twice */
        if (position < 0 || (guint)position > src->items->len)
            position = (int)src->items->len;
        /* position is expressed in the pre-removal indexing */
        if ((guint)position == index || (guint)position == index + 1)
            return TRUE;
        BtdItem *moved = g_ptr_array_steal_index(src->items, index);
        if ((guint)position > index)
            position--;
        g_ptr_array_insert(src->items, position, moved);
        emit_changed(model);
        return TRUE;
    }

    if (btd_tab_contains(dst, item) || dst->items->len >= BTD_MAX_ITEMS)
        return FALSE;
    btd_model_freeze(model);
    BtdItem *moved = copy ? btd_item_copy(item) : g_ptr_array_steal_index(src->items, index);
    if (position < 0 || (guint)position > dst->items->len)
        position = (int)dst->items->len;
    g_ptr_array_insert(dst->items, position, moved);
    emit_changed(model);
    btd_model_thaw(model);
    return TRUE;
}
