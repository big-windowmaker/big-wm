/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — icons the user forced in Window Maker.
 *
 * Window Maker keeps per-application attributes in
 * ~/GNUstep/Defaults/WMWindowAttributes ("instance.class", "instance" or
 * "class" = { Icon = ...; AlwaysUserIcon = YES; }). An icon with
 * AlwaysUserIcon is the user's explicit choice for that application (the
 * Netscape icon for Firefox, for instance), and the dock shows it too, so
 * the application looks the same in the Dock, the Clip, its appicon and
 * here. Only GLib is used, so it is unit-tested in isolation.
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _BtdWmAttrs BtdWmAttrs;

/* path: the WMWindowAttributes file; NULL for the default one
 * ($WMAKER_USER_ROOT or ~/GNUstep, then Defaults/WMWindowAttributes).
 * icon_dirs: where relative icon names are looked up; NULL for Window
 * Maker's default IconPath. */
BtdWmAttrs *btd_wmattrs_new(const char *path, const char *const *icon_dirs);
void btd_wmattrs_free(BtdWmAttrs *attrs);
const char *btd_wmattrs_path(BtdWmAttrs *attrs);

/* Reads the file again; TRUE when the forced icons changed. A missing or
 * unreadable file means no forced icons. */
gboolean btd_wmattrs_reload(BtdWmAttrs *attrs);

/* Parses WMWindowAttributes text (exposed for tests). */
gboolean btd_wmattrs_load_data(BtdWmAttrs *attrs, const char *data, gssize length);

/* Forced icon of a window, as Window Maker looks it up: "instance.class",
 * then "instance", then "class". Absolute path of an existing file, or NULL. */
const char *btd_wmattrs_icon_for_window(BtdWmAttrs *attrs, const char *instance, const char *wm_class);

/* Forced icon of an application known only by its window class
 * (StartupWMClass of a .desktop not running yet): an entry named exactly
 * so, else one whose class or instance part is that name (case does not
 * matter: .desktop files often write the class in lower case). Only whole
 * names are compared, never parts of a name. */
const char *btd_wmattrs_icon_for_class(BtdWmAttrs *attrs, const char *wm_class);

G_END_DECLS
