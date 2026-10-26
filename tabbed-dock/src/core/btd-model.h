/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — toolkit-independent data model.
 *
 * A dock is an ordered list of tabs; each tab is an ordered list of items.
 * Items are shortcuts only: removing one never touches the file it points to.
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    BTD_ITEM_APP,        /* freedesktop .desktop entry (id or absolute path) */
    BTD_ITEM_FILE,       /* document: opened with the default handler */
    BTD_ITEM_FOLDER,     /* directory: opened with the file manager */
    BTD_ITEM_APPIMAGE,   /* executable AppImage file, run directly */
    BTD_ITEM_SEPARATOR,  /* visual groove between groups */
} BtdItemType;

typedef struct {
    BtdItemType type;
    char *target;   /* desktop id ("org.gnome.Calculator.desktop"), or absolute path */
    char *icon;     /* optional per-item icon override (theme name or absolute path) */
    char *label;    /* optional per-item label override */
} BtdItem;

/* Built-in tab identifiers are fixed; their labels are translated at display time. */
#define BTD_TAB_GENERAL   "general"
#define BTD_TAB_DEVELOPER "developer"
#define BTD_TAB_DOCUMENTS "documents"
#define BTD_TAB_TEMPORARY "temporary"

typedef struct {
    char *id;        /* stable identifier, [a-z0-9-]+ */
    char *label;     /* user label; NULL/empty = translated default for built-ins */
    gboolean builtin;
    GPtrArray *items; /* of BtdItem* */
} BtdTab;

typedef struct _BtdModel BtdModel;
typedef void (*BtdModelChangedFunc)(BtdModel *model, gpointer user_data);

struct _BtdModel {
    GPtrArray *tabs;          /* of BtdTab* */
    char *active_tab;         /* id of the selected tab */
    BtdModelChangedFunc changed;
    gpointer changed_data;
    guint freeze;
    gboolean pending;
};

#define BTD_MAX_TABS  64
#define BTD_MAX_ITEMS 256
#define BTD_MAX_LABEL 64

BtdItem *btd_item_new(BtdItemType type, const char *target);
BtdItem *btd_item_copy(const BtdItem *item);
void btd_item_free(BtdItem *item);
gboolean btd_item_equal(const BtdItem *a, const BtdItem *b);
const char *btd_item_type_to_string(BtdItemType type);
gboolean btd_item_type_from_string(const char *s, BtdItemType *out);

BtdModel *btd_model_new(void);
void btd_model_free(BtdModel *model);
void btd_model_set_changed_func(BtdModel *model, BtdModelChangedFunc func, gpointer data);
void btd_model_freeze(BtdModel *model);
void btd_model_thaw(BtdModel *model);
void btd_model_clear(BtdModel *model);

/* Builds the four built-in tabs, empty. Caller fills defaults (see btd-defaults). */
void btd_model_reset_builtin_tabs(BtdModel *model);
gboolean btd_tab_id_is_builtin(const char *id);
const char *btd_tab_default_label(const char *id); /* untranslated source string or NULL */

guint btd_model_n_tabs(BtdModel *model);
BtdTab *btd_model_get_tab(BtdModel *model, guint index);
BtdTab *btd_model_find_tab(BtdModel *model, const char *id, guint *index_out);
BtdTab *btd_model_get_active_tab(BtdModel *model);
gboolean btd_model_set_active_tab(BtdModel *model, const char *id);
/* Cycles the active tab by delta (wraps). */
void btd_model_cycle_active_tab(BtdModel *model, int delta);

/* Creates a custom tab with a fresh unique id; returns it (owned by model). */
BtdTab *btd_model_add_tab(BtdModel *model, const char *label, int position);
BtdTab *btd_model_insert_tab(BtdModel *model, const char *id, const char *label, gboolean builtin, int position);
gboolean btd_model_rename_tab(BtdModel *model, const char *id, const char *label);
/* Only custom tabs can be removed. */
gboolean btd_model_remove_tab(BtdModel *model, const char *id);
gboolean btd_model_move_tab(BtdModel *model, const char *id, guint new_index);

/* Items. Positions < 0 append. Duplicates of non-separator items are refused. */
gboolean btd_model_insert_item(BtdModel *model, const char *tab_id, BtdItem *item, int position);
gboolean btd_model_remove_item(BtdModel *model, const char *tab_id, guint index);
gboolean btd_model_move_item(BtdModel *model, const char *from_tab, guint index,
                             const char *to_tab, int position, gboolean copy);
gboolean btd_tab_contains(BtdTab *tab, const BtdItem *item);

/* Sanitises a user-entered label: strips control chars, trims, limits length. */
char *btd_sanitize_label(const char *label);
gboolean btd_tab_id_is_valid(const char *id);

G_END_DECLS
