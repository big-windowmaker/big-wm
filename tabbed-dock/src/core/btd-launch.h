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

/* gtk-nocsd guard. In Big WindowMaker the session preloads gtk-nocsd so GTK
 * windows get the window manager's title bar. When a GTK program crashes,
 * gtk-nocsd's watcher restarts it without the library (LD_PRELOAD=""): a
 * dock restarted that way would hand the empty preload to every application
 * it opens, and they came up with GTK header bars.
 *
 * Returns the LD_PRELOAD to give launched applications, or NULL to leave the
 * dock's own environment alone: only in a Big WindowMaker session (desktop
 * WindowMaker or wlmaker), only when the dock's own preload lacks the
 * library while the session's (systemd --user manager environment, exported
 * by big-wm-session-ctl) has it, and only when the library is installed
 * (nocsd_path; NULL means BTD_NOCSD_LIBRARY). A session where the user
 * turned gtk-nocsd off has no library in its preload and is left alone. */
#define BTD_NOCSD_LIBRARY "/usr/lib/libgtk-nocsd.so.0"
char *btd_launch_nocsd_preload(const char *desktop, const char *own_preload, const char *session_preload,
                               const char *nocsd_path);
/* LD_PRELOAD of the systemd --user manager environment, NULL if unset or unreachable. */
char *btd_launch_session_preload(void);

/* Disable the scope move (tests, or no systemd). */
void btd_launch_set_use_scopes(gboolean use);

G_END_DECLS
