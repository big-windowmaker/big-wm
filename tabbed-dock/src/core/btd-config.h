/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — preferences and persistence (GKeyFile, atomic writes,
 * backup of the last good file, recovery from corrupted files).
 */
#pragma once

#include "btd-model.h"

G_BEGIN_DECLS

typedef enum { BTD_EDGE_BOTTOM, BTD_EDGE_TOP } BtdEdge;
typedef enum { BTD_ALIGN_CENTER, BTD_ALIGN_LEFT, BTD_ALIGN_RIGHT, BTD_ALIGN_FILL } BtdAlign;
typedef enum { BTD_STYLE_CLASSIC, BTD_STYLE_MODERN } BtdStyle;

typedef struct {
    BtdEdge edge;
    BtdAlign align;
    char *monitor;          /* "primary", a connector/model name, or a 0-based index */
    int icon_size;          /* 24..96, icon pixels (tile adds a bevel margin) */
    gboolean autohide;
    gboolean reserve_space; /* _NET_WM_STRUT(_PARTIAL) / layer-shell exclusive zone */
    gboolean animations;
    gboolean show_running;  /* running apps that are not in the tab */
    gboolean click_minimizes; /* click on the focused app minimises it */
    gboolean global_shortcuts; /* X11: grab Super+Alt+digits/PageUp/PageDown */
    BtdStyle style;
} BtdPrefs;

#define BTD_ICON_SIZE_MIN 24
#define BTD_ICON_SIZE_MAX 96
#define BTD_CONFIG_VERSION 1

typedef enum {
    BTD_LOAD_OK,
    BTD_LOAD_MISSING,          /* no file: caller installs defaults */
    BTD_LOAD_FROM_BACKUP,      /* main file was corrupt; backup used */
    BTD_LOAD_CORRUPT_DEFAULTS, /* main and backup unusable: defaults */
} BtdLoadResult;

void btd_prefs_init_defaults(BtdPrefs *prefs);
void btd_prefs_clear(BtdPrefs *prefs);
void btd_prefs_copy(BtdPrefs *dst, const BtdPrefs *src);

const char *btd_edge_to_string(BtdEdge e);
const char *btd_align_to_string(BtdAlign a);
const char *btd_style_to_string(BtdStyle s);

/* ~/.config/big-tabbed-dock/dock.conf (honours XDG_CONFIG_HOME). */
char *btd_config_default_path(void);

/* Serialisation. from_data validates every value; invalid items are skipped,
 * invalid preferences fall back to defaults. Returns FALSE (and leaves the
 * model untouched) only when the data is not a dock file at all. */
char *btd_config_to_data(BtdModel *model, const BtdPrefs *prefs, gsize *length);
gboolean btd_config_from_data(const char *data, gssize length, BtdModel *model, BtdPrefs *prefs, GError **error);

/* Loads path; on corruption the bad file is kept as <path>.broken-<time>
 * (never deleted) and <path>.bak is tried. */
BtdLoadResult btd_config_load(const char *path, BtdModel *model, BtdPrefs *prefs, GError **error);
/* Atomic save; the previous good file becomes <path>.bak. Mode 0600. */
gboolean btd_config_save(const char *path, BtdModel *model, const BtdPrefs *prefs, GError **error);

/* Validation helpers, exposed for tests. */
gboolean btd_item_is_valid(const BtdItem *item);
gboolean btd_icon_name_is_valid(const char *icon);

G_END_DECLS
