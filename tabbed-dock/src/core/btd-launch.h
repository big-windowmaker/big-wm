/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — launching items.
 *
 * Applications go through GIO's GDesktopAppInfo, which implements the
 * Desktop Entry Spec (Exec field codes, Terminal, Path, TryExec,
 * DBusActivatable, Actions) and spawns argv directly: no launcher string is
 * ever handed to a shell. Documents and folders open with the default
 * handler (the same lookup as `gio open`/xdg-open). AppImages are executed
 * directly after validation. Each spawned process is moved to its own
 * systemd --user scope when possible, so that stopping or restarting the
 * dock never takes running applications with it.
 */
#pragma once

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "btd-desktop.h"
#include "btd-model.h"

G_BEGIN_DECLS

/* Opens an app item's GDesktopAppInfo (id via GIO lookup, or validated path). */
GDesktopAppInfo *btd_launch_app_info(const BtdItem *item);

/* Launches an item. uris (may be NULL) are passed to applications (drop of
 * files onto an app). */
gboolean btd_launch_item(const BtdItem *item, GAppLaunchContext *ctx, GList *uris, GError **error);
/* Launches a desktop action (Actions= key) of an app item. */
gboolean btd_launch_action(const BtdItem *item, const char *action, GAppLaunchContext *ctx, GError **error);
/* Launches a desktop id directly (running-app section, "new window"). */
gboolean btd_launch_desktop_id(const char *id, GAppLaunchContext *ctx, GError **error);

/* Checks that path is a regular, user-executable file not writable by others. */
gboolean btd_launch_appimage_is_safe(const char *path, GError **error);

/* systemd unit-name escaping ("-" -> "\x2d" etc.), exposed for tests. */
char *btd_launch_escape_unit(const char *s);

/* Disable the scope move (tests, or no systemd). */
void btd_launch_set_use_scopes(gboolean use);

G_END_DECLS
