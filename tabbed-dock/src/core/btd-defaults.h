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

G_END_DECLS
