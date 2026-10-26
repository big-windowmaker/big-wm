/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-defaults.h"

#include <gio/gio.h>

static const char *files_ids[] = { "thunar.desktop", "org.xfce.Thunar.desktop", "org.gnome.Nautilus.desktop",
                                   "pcmanfm.desktop", "nemo.desktop", "org.kde.dolphin.desktop", "caja.desktop", NULL };
static const char *terminal_ids[] = { "org.communitybig.ashyterm.desktop", "xfce4-terminal.desktop",
                                      "org.gnome.Console.desktop", "org.gnome.Terminal.desktop",
                                      "org.kde.konsole.desktop", "xterm.desktop", NULL };
static const char *browser_ids[] = { "brave-browser.desktop", "firefox.desktop", "org.mozilla.firefox.desktop",
                                     "chromium.desktop", "google-chrome.desktop", "com.brave.Browser.desktop", NULL };
static const char *editor_ids[] = { "org.xfce.mousepad.desktop", "org.gnome.TextEditor.desktop",
                                    "featherpad.desktop", "org.kde.kate.desktop", "org.gnome.gedit.desktop",
                                    "gedit.desktop", NULL };
static const char *calc_ids[] = { "org.gnome.Calculator.desktop", "galculator.desktop", "qalculate-gtk.desktop",
                                  "org.kde.kcalc.desktop", NULL };
static const char *software_ids[] = { "org.manjaro.pamac.manager.desktop", "pamac-manager.desktop",
                                      "org.gnome.Software.desktop", NULL };
static const char *ide_ids[] = { "code-oss.desktop", "code.desktop", "visual-studio-code.desktop", "codium.desktop",
                                 "com.visualstudio.code.desktop", "com.vscodium.codium.desktop", "geany.desktop",
                                 "org.gnome.Builder.desktop", "org.qt-project.qtcreator.desktop", NULL };
static const char *diff_ids[] = { "org.gnome.Meld.desktop", "meld.desktop", "org.kde.kompare.desktop", NULL };
static const char *git_ids[] = { "org.gnome.gitg.desktop", "gitg.desktop", "git-cola.desktop", "gitk.desktop", NULL };
static const char *monitor_ids[] = { "org.gnome.SystemMonitor.desktop", "gnome-system-monitor.desktop",
                                     "xfce4-taskmanager.desktop", "htop.desktop", "btop.desktop", NULL };
static const char *writer_ids[] = { "libreoffice-writer.desktop", "onlyoffice-desktopeditors.desktop",
                                    "org.onlyoffice.desktopeditors.desktop", "abiword.desktop", NULL };
static const char *calc_sheet_ids[] = { "libreoffice-calc.desktop", "gnumeric.desktop", NULL };
static const char *viewer_ids[] = { "org.gnome.Papers.desktop", "org.gnome.Evince.desktop", "atril.desktop",
                                    "okularApplication_pdf.desktop", "org.kde.okular.desktop", NULL };

static void
add_app(BtdModel *model, const char *tab, const BtdDesktopEntry *e)
{
    if (e)
        btd_model_insert_item(model, tab, btd_item_new(BTD_ITEM_APP, e->id), -1);
}

static const BtdDesktopEntry *
default_for(BtdDesktopIndex *index, const char *content_type, const char *scheme, const char *const *fallback)
{
    GAppInfo *info = NULL;
    if (content_type)
        info = g_app_info_get_default_for_type(content_type, FALSE);
    else if (scheme)
        info = g_app_info_get_default_for_uri_scheme(scheme);
    const BtdDesktopEntry *e = NULL;
    if (info) {
        e = btd_desktop_index_lookup(index, g_app_info_get_id(info));
        if (e && !e->available)
            e = NULL;
        g_object_unref(info);
    }
    return e ? e : btd_desktop_index_first_available(index, fallback);
}

static void
add_folder(BtdModel *model, const char *tab, const char *path)
{
    if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
        btd_model_insert_item(model, tab, btd_item_new(BTD_ITEM_FOLDER, path), -1);
}

void
btd_defaults_fill_tab(BtdModel *model, BtdDesktopIndex *index, const char *tab_id)
{
    BtdTab *tab = btd_model_find_tab(model, tab_id, NULL);
    if (!tab)
        return;
    btd_model_freeze(model);
    g_ptr_array_set_size(tab->items, 0);
    model->pending = TRUE;
    const BtdDesktopEntry *files = default_for(index, "inode/directory", NULL, files_ids);
    const BtdDesktopEntry *term = btd_desktop_index_first_available(index, terminal_ids);
    const BtdDesktopEntry *editor = default_for(index, "text/plain", NULL, editor_ids);

    if (g_str_equal(tab_id, BTD_TAB_GENERAL)) {
        add_app(model, tab_id, files);
        add_app(model, tab_id, term);
        add_app(model, tab_id, default_for(index, NULL, "https", browser_ids));
        add_app(model, tab_id, editor);
        add_app(model, tab_id, btd_desktop_index_first_available(index, calc_ids));
        add_app(model, tab_id, btd_desktop_index_first_available(index, software_ids));
    } else if (g_str_equal(tab_id, BTD_TAB_DEVELOPER)) {
        add_app(model, tab_id, term);
        add_app(model, tab_id, editor);
        add_app(model, tab_id, btd_desktop_index_first_available(index, ide_ids));
        add_app(model, tab_id, btd_desktop_index_first_available(index, git_ids));
        add_app(model, tab_id, btd_desktop_index_first_available(index, diff_ids));
        add_app(model, tab_id, btd_desktop_index_first_available(index, monitor_ids));
    } else if (g_str_equal(tab_id, BTD_TAB_DOCUMENTS)) {
        add_folder(model, tab_id, g_get_home_dir());
        add_folder(model, tab_id, g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS));
        add_folder(model, tab_id, g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD));
        add_folder(model, tab_id, g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));
        if (tab->items->len > 0)
            btd_model_insert_item(model, tab_id, btd_item_new(BTD_ITEM_SEPARATOR, NULL), -1);
        add_app(model, tab_id, btd_desktop_index_first_available(index, writer_ids));
        add_app(model, tab_id, btd_desktop_index_first_available(index, calc_sheet_ids));
        add_app(model, tab_id, default_for(index, "application/pdf", NULL, viewer_ids));
        /* Do not end with a dangling separator. */
        if (tab->items->len > 0 && ((BtdItem *)g_ptr_array_index(tab->items, tab->items->len - 1))->type ==
                                       BTD_ITEM_SEPARATOR)
            g_ptr_array_remove_index(tab->items, tab->items->len - 1);
    }
    /* Temporary starts empty: it is a scratch area for drag and drop. */
    btd_model_thaw(model);
}

void
btd_defaults_apply(BtdModel *model, BtdDesktopIndex *index)
{
    btd_model_freeze(model);
    btd_model_reset_builtin_tabs(model);
    btd_defaults_fill_tab(model, index, BTD_TAB_GENERAL);
    btd_defaults_fill_tab(model, index, BTD_TAB_DEVELOPER);
    btd_defaults_fill_tab(model, index, BTD_TAB_DOCUMENTS);
    btd_defaults_fill_tab(model, index, BTD_TAB_TEMPORARY);
    btd_model_thaw(model);
}
