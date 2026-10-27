/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Big Tabbed Dock — freedesktop desktop-entry index and window matcher.
 *
 * Only GLib is used here (no toolkit), so it is unit-tested in isolation.
 * Launching itself goes through GIO's GDesktopAppInfo (see btd-launch).
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    char *id;               /* desktop id, e.g. "org.gnome.Calculator.desktop" */
    char *path;             /* absolute path of the .desktop file */
    char *name;             /* localized Name */
    char *icon;             /* Icon (theme name or absolute path) */
    char *startup_wm_class; /* StartupWMClass */
    char *exec_base;        /* basename of the program in Exec (after env/flatpak) */
    char *flatpak_id;       /* X-Flatpak or "flatpak run <id>" */
    gboolean available;     /* TryExec/Exec program found */
    gboolean no_display;
    gboolean dbus_activatable;
    gboolean terminal;
} BtdDesktopEntry;

typedef struct _BtdDesktopIndex BtdDesktopIndex;

/* Data dirs in lookup order: XDG_DATA_HOME, XDG_DATA_DIRS, then the Flatpak
 * export dirs if they were not already listed. Free with g_strfreev. */
char **btd_desktop_data_dirs(void);

gboolean btd_desktop_id_is_valid(const char *id);

/* Parses a single file. Returns NULL for non-applications, Hidden=true,
 * unreadable or oversized files. */
BtdDesktopEntry *btd_desktop_entry_load(const char *path, const char *id);
void btd_desktop_entry_free(BtdDesktopEntry *entry);

/* Program basename from an Exec line, skipping env assignments and wrappers.
 * Returns NULL if it cannot be parsed. Never executes anything. */
char *btd_desktop_exec_program(const char *exec, char **flatpak_id_out);

BtdDesktopIndex *btd_desktop_index_new(const char *const *data_dirs);
void btd_desktop_index_free(BtdDesktopIndex *index);
guint btd_desktop_index_size(BtdDesktopIndex *index);
const BtdDesktopEntry *btd_desktop_index_lookup(BtdDesktopIndex *index, const char *id);
/* Resolves an absolute .desktop path to its id when it lives in an indexed dir. */
const BtdDesktopEntry *btd_desktop_index_lookup_path(BtdDesktopIndex *index, const char *path);
/* First available entry among candidate ids (NULL-terminated). */
const BtdDesktopEntry *btd_desktop_index_first_available(BtdDesktopIndex *index, const char *const *ids);
/* All entries (unordered); do not free contents. */
GList *btd_desktop_index_entries(BtdDesktopIndex *index);

/* Properties of a toplevel window used to find its application. Any field may be NULL. */
typedef struct {
    const char *app_id;         /* Wayland app_id, or _GTK_APPLICATION_ID on X11 */
    const char *wm_class;       /* X11 WM_CLASS res_class */
    const char *wm_instance;    /* X11 WM_CLASS res_name */
    const char *launched_desktop_file; /* GIO_LAUNCHED_DESKTOP_FILE of the client */
    const char *exe;            /* basename of /proc/<pid>/exe */
} BtdWindowKeys;

/* Returns the desktop id of the best match, or NULL. */
const char *btd_desktop_index_match(BtdDesktopIndex *index, const BtdWindowKeys *keys);

G_END_DECLS
