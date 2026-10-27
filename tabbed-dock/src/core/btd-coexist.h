/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Coexistence with the classic Window Maker / wlmaker Dock.
 *
 * Nothing here edits the Dock contents (WMState, State.plist). Switching
 * only toggles:
 *   - X11: the "DisableDock" key of the WindowMaker defaults domain, through
 *     Window Maker's own `wdwrite` tool (reverted with the same tool);
 *   - Wayland: a per-user XDG autostart override that hides the wlmdock
 *     autostart (big-wm-wayland-dock.desktop);
 *   - Tabbed dock: a per-user override of big-wm-tabbed-dock.desktop.
 * Overrides written here carry X-Big-Tabbed-Dock-Managed=true; a user's own
 * override file is renamed to *.user-backup before being replaced, never
 * deleted.
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    BTD_MODE_CLASSIC, /* only the classic Dock */
    BTD_MODE_TABBED,  /* only the Big Tabbed Dock */
    BTD_MODE_BOTH,
    BTD_MODE_NONE,
} BtdDockMode;

typedef struct {
    char *autostart_dir; /* ~/.config/autostart */
    char *wm_defaults;   /* ~/GNUstep/Defaults/WindowMaker */
    char *wdwrite;       /* path to wdwrite, or NULL if unavailable */
} BtdCoexistPaths;

void btd_coexist_paths_init(BtdCoexistPaths *paths); /* from the environment */
void btd_coexist_paths_clear(BtdCoexistPaths *paths);

const char *btd_dock_mode_to_string(BtdDockMode mode);
gboolean btd_dock_mode_from_string(const char *s, BtdDockMode *mode);

gboolean btd_coexist_tabbed_enabled(const BtdCoexistPaths *paths);
gboolean btd_coexist_classic_x11_enabled(const BtdCoexistPaths *paths);
gboolean btd_coexist_classic_wayland_enabled(const BtdCoexistPaths *paths);
BtdDockMode btd_coexist_get_mode(const BtdCoexistPaths *paths, gboolean wayland);

/* Applies the mode for both sessions. Returns FALSE with error if a step
 * failed (the other steps are still attempted). */
gboolean btd_coexist_set_mode(const BtdCoexistPaths *paths, BtdDockMode mode, GError **error);

G_END_DECLS
