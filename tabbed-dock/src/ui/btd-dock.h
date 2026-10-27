/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <gtk/gtk.h>

#include "btd-config.h"
#include "btd-desktop.h"
#include "btd-draw.h"
#include "btd-icons.h"
#include "btd-model.h"
#include "btd-windows.h"

G_BEGIN_DECLS

typedef enum {
    SLOT_TAB,
    SLOT_ADD_TAB,
    SLOT_ITEM,     /* item of the active tab */
    SLOT_RUNNING,  /* running application not in the active tab */
    SLOT_RUNNING_GROOVE,
    SLOT_HIDE,     /* arrow button at the start of the tab row */
} SlotKind;

typedef struct {
    SlotKind kind;
    GdkRectangle rect;  /* hit/draw rectangle (logical px, window coords) */
    int index;          /* tab index, item index, or running-group index */
} Slot;

/* Windows grouped by application. */
typedef struct {
    char *key;          /* desktop id, or "class:<name>" when unknown */
    char *desktop_id;   /* may be NULL */
    GPtrArray *windows; /* BtdWindow* (borrowed from the tracker) */
} AppGroup;

typedef struct _BtdDock BtdDock;

struct _BtdDock {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *area;
    gboolean wayland, layer_shell, composited;

    BtdModel *model;
    BtdPrefs prefs;
    char *config_path;
    guint save_id;
    gboolean config_broken_notice;

    BtdDesktopIndex *index;
    GAppInfoMonitor *app_monitor;
    guint index_rebuild_id;
    BtdWindowTracker *tracker;
    GHashTable *groups;   /* key -> AppGroup* */
    GPtrArray *running;   /* AppGroup* shown in the running section (borrowed) */
    BtdIconCache *icons;
    BtdPalette palette;
    gboolean dark_mode;           /* Big WindowMaker dark mode (big-wm-theme) */
    GFileMonitor *theme_monitor;  /* ~/.config/big-wm/theme */
    PangoFontDescription *tab_font, *hint_font, *label_font;

    /* layout (logical px) */
    GArray *slots;        /* of Slot */
    int width, height;
    int tile, tab_h, shelf_y, shelf_h, shadow;
    int cell_w, cell_h, label_h, stub_w;
    int fill_width;       /* width given by the compositor (Wayland, full width) */
    int tabs_end;         /* x after the last tab */
    GdkRectangle active_tab_rect;
    gboolean hidden;      /* autohide: collapsed to a strip */
    int strip_y;          /* y of the strip inside the window when hidden */
    gboolean collapsing;  /* the arrow button asked to hide */
    guint hide_id;
    GdkMonitor *monitor;

    /* interaction */
    int hover_slot, press_slot;
    double press_x, press_y;
    gboolean drag_started;
    int drag_tab_index, drag_item_index; /* internal drag source */
    char *drag_tab_id;
    int drop_index;       /* insertion position in the active tab, -1 none */
    int drop_tab;         /* tab under the pointer during a drag, -1 none */
    int drop_on_item;     /* app item that would receive files, -1 none */
    guint spring_id;
    gboolean idrag;       /* in-dock drag (no data device needed) */
    SlotKind idrag_kind;
    int idrag_index;
    double idrag_x, idrag_y;
    char *idrag_app;      /* desktop id of a dragged running application */
    gboolean menu_open;
    GtkWidget *menu;
    guint menu_leave_id;

    /* launch feedback / animation */
    char *launching_key;
    gint64 launching_until;
    guint tick_id;
    double tab_anim;      /* 0..1 crossfade after a tab switch */
    gint64 tab_anim_start;

    GtkWidget *prefs_dialog;
    guint shortcuts_grabbed;
};

/* monotonic time at main(), for the opt-in BTD_TIMING=1 startup report */
extern gint64 btd_start_time;

BtdDock *btd_dock_new(GtkApplication *app, const char *config_path);
void btd_dock_free(BtdDock *dock);
void btd_dock_show(BtdDock *dock);

/* Remote actions (command line of a second instance, signals). */
void btd_dock_select_tab(BtdDock *dock, const char *spec); /* "next", "prev", 1-based index or tab id */
void btd_dock_reload(BtdDock *dock);
void btd_dock_save_now(BtdDock *dock);
void btd_dock_show_preferences(BtdDock *dock);

/* Internal, shared between btd-dock*.c */
void btd_dock_relayout(BtdDock *dock);
void btd_dock_queue_save(BtdDock *dock);
void btd_dock_apply_prefs(BtdDock *dock);
void btd_dock_restore_defaults(BtdDock *dock);
const char *btd_dock_tab_label(BtdTab *tab);
char *btd_dock_item_name(BtdDock *dock, const BtdItem *item);
AppGroup *btd_dock_item_group(BtdDock *dock, const BtdItem *item);
const char *btd_dock_item_desktop_id(BtdDock *dock, const BtdItem *item);
void btd_dock_activate_item(BtdDock *dock, const BtdItem *item, AppGroup *group, guint32 time, gboolean new_instance);
void btd_dock_activate_group(BtdDock *dock, AppGroup *group, guint32 time);
gboolean btd_dock_raise_via_application(BtdDock *dock, AppGroup *group);
void btd_dock_launch(BtdDock *dock, const BtdItem *item, GList *uris, const char *action);
void btd_dock_error(BtdDock *dock, const char *primary, const char *secondary);
gboolean btd_dock_ask(BtdDock *dock, const char *primary, const char *secondary, const char *ok_label);
char *btd_dock_ask_text(BtdDock *dock, const char *title, const char *label, const char *initial);
void btd_dock_set_hidden(BtdDock *dock, gboolean hidden);

Slot *btd_dock_slot(BtdDock *dock, int index);

/* btd-dock-menu.c */
void btd_dock_popup_menu(BtdDock *dock, int slot_index, const GdkEvent *event);

/* btd-dock-dnd.c */
void btd_dock_dnd_init(BtdDock *dock);
void btd_dock_dnd_begin(BtdDock *dock, int slot_index, const GdkEvent *event);
BtdItem *btd_dock_item_from_path(BtdDock *dock, const char *path);
void btd_dock_idrag_begin(BtdDock *dock, int slot_index, double x, double y);
void btd_dock_idrag_motion(BtdDock *dock, double x, double y);
void btd_dock_idrag_end(BtdDock *dock, double x, double y, gboolean copy, gboolean cancel);

/* btd-prefs.c */
GtkWidget *btd_prefs_dialog_new(BtdDock *dock);

G_END_DECLS
