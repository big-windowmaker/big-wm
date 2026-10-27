/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "btd-desktop.h"
#include "btd-model.h"

G_BEGIN_DECLS

/* Recreates the four built-in tabs, filled with applications that are really
 * installed (index) and the user's XDG folders. Custom tabs are removed. */
void btd_defaults_apply(BtdModel *model, BtdDesktopIndex *index);

/* Fills only one built-in tab (used by "restore this tab"). */
void btd_defaults_fill_tab(BtdModel *model, BtdDesktopIndex *index, const char *tab_id);

/* Live system installer ("Install System"), shipped by biglinux-livecd only on
 * the live medium. In live mode it is the first item of the General tab. */
#define BTD_LIVE_INSTALLER_ID "com.biglinux.calamares-config.desktop"
#define BTD_LIVE_MARKER "/livefs-pkgs.txt"

/* TRUE when running from the live medium (the marker file exists). */
gboolean btd_defaults_is_live(void);
/* Tests only: another marker path; NULL restores BTD_LIVE_MARKER. */
void btd_defaults_set_live_marker(const char *path);

/* Configuration that already existed (live with persistence, dock restarted):
 * in live mode, puts the installer first in General when it is installed and
 * not already in that tab. Done once per boot: stamp_path (in the runtime
 * dir, which is emptied on every boot) is created on the first call, so an
 * item removed by the user does not come back until the next boot. NULL
 * stamp_path checks every time. Returns TRUE when the model changed. */
gboolean btd_defaults_offer_live_installer(BtdModel *model, BtdDesktopIndex *index, const char *stamp_path);

G_END_DECLS
