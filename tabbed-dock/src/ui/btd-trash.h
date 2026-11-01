/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — the trash (FreeDesktop, through GIO), a special element
 * at the right end of the shelf.
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _BtdDock BtdDock;
typedef struct _BtdTrash BtdTrash;

BtdTrash *btd_trash_new(BtdDock *dock);
void btd_trash_free(BtdTrash *trash);
gboolean btd_trash_is_full(BtdTrash *trash);
guint btd_trash_count(BtdTrash *trash);
/* Opens the trash in the default file manager. */
void btd_trash_open(BtdTrash *trash, guint32 time);
/* Asks first (Cancel is the default answer), then deletes in a thread. */
void btd_trash_empty(BtdTrash *trash, GtkWindow *parent);

G_END_DECLS
