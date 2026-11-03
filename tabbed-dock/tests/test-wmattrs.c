/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "btd-wmattrs.h"

#include <glib/gstdio.h>
#include <string.h>

typedef struct {
    char *dir;
    char *icons;
    char *netscape, *folder, *own;
    BtdWmAttrs *attrs;
} Fixture;

static void
touch(const char *path)
{
    g_assert_true(g_file_set_contents(path, "x", 1, NULL));
}

static void
setup(Fixture *f, gconstpointer data)
{
    (void)data;
    f->dir = g_dir_make_tmp("btd-wmattrs-XXXXXX", NULL);
    f->icons = g_build_filename(f->dir, "Icons", NULL);
    g_mkdir(f->icons, 0700);
    f->netscape = g_build_filename(f->icons, "big-wm-netscape.png", NULL);
    f->folder = g_build_filename(f->icons, "big-wm-folder.png", NULL);
    f->own = g_build_filename(f->dir, "meu icone #1.png", NULL);
    touch(f->netscape);
    touch(f->folder);
    touch(f->own);
    const char *dirs[] = { f->icons, NULL };
    char *path = g_build_filename(f->dir, "WMWindowAttributes", NULL);
    f->attrs = btd_wmattrs_new(path, dirs);
    g_free(path);
}

static void
teardown(Fixture *f, gconstpointer data)
{
    (void)data;
    btd_wmattrs_free(f->attrs);
    g_unlink(f->netscape);
    g_unlink(f->folder);
    g_unlink(f->own);
    char *path = g_build_filename(f->dir, "WMWindowAttributes", NULL);
    g_unlink(path);
    g_free(path);
    g_rmdir(f->icons);
    g_rmdir(f->dir);
    g_free(f->netscape);
    g_free(f->folder);
    g_free(f->own);
    g_free(f->icons);
    g_free(f->dir);
}

/* As Window Maker and the Big WindowMaker write it. */
static const char *attrs_text =
    "{\n"
    "  /* comment */\n"
    "  Logo.WMClip = {\n    Icon = \"big-wm-clip.png\";\n    AlwaysUserIcon = YES;\n  };\n"
    "  Navigator.firefox = {\n    Icon = \"big-wm-netscape.png\";\n    AlwaysUserIcon = YES;\n  };\n"
    "  firefox = {Icon = \"/nao/existe/cache.png\";};\n"
    "  thunar.Thunar = {\n    Icon = big-wm-folder.png;\n    AlwaysUserIcon = YES;\n    Workspace = (1, 2);\n  };\n"
    "  \"firefox-web.whatsapp.com__-Browser\" = {Icon = \"big-wm-folder.png\";};\n"
    "  \"*\" = {\n    Icon = \"big-wm-folder.png\";\n    AlwaysUserIcon = YES;\n  };\n"
    "  Gimp = {Icon = big-wm-folder.png; AlwaysUserIcon = NO;};\n"
    "}\n";

static void
test_window(Fixture *f, gconstpointer data)
{
    (void)data;
    g_assert_true(btd_wmattrs_load_data(f->attrs, attrs_text, -1));
    /* Window Maker's order: instance.class, instance, class */
    g_assert_cmpstr(btd_wmattrs_icon_for_window(f->attrs, "Navigator", "firefox"), ==, f->netscape);
    g_assert_cmpstr(btd_wmattrs_icon_for_window(f->attrs, "thunar", "Thunar"), ==, f->folder);
    /* without AlwaysUserIcon it is only a fallback for Window Maker: not forced */
    g_assert_null(btd_wmattrs_icon_for_window(f->attrs, "firefox-web.whatsapp.com__-Browser",
                                              "firefox-web.whatsapp.com__-Browser"));
    g_assert_null(btd_wmattrs_icon_for_window(f->attrs, "gimp", "Gimp"));
    /* "*" (every window) is the generic icon, never an application's choice */
    g_assert_null(btd_wmattrs_icon_for_window(f->attrs, "xterm", "XTerm"));
    /* icon file missing: nothing */
    g_assert_null(btd_wmattrs_icon_for_window(f->attrs, "Logo", "WMClip"));
}

static void
test_class(Fixture *f, gconstpointer data)
{
    (void)data;
    btd_wmattrs_load_data(f->attrs, attrs_text, -1);
    /* StartupWMClass=firefox finds Navigator.firefox (class part) */
    g_assert_cmpstr(btd_wmattrs_icon_for_class(f->attrs, "firefox"), ==, f->netscape);
    /* case does not matter; the instance part also counts */
    g_assert_cmpstr(btd_wmattrs_icon_for_class(f->attrs, "thunar"), ==, f->folder);
    /* whole names only: no "fire" -> firefox */
    g_assert_null(btd_wmattrs_icon_for_class(f->attrs, "fire"));
    g_assert_null(btd_wmattrs_icon_for_class(f->attrs, "Navigator.fire"));
    g_assert_null(btd_wmattrs_icon_for_class(f->attrs, NULL));
}

static void
test_file(Fixture *f, gconstpointer data)
{
    (void)data;
    /* missing file: no icons, no crash */
    g_assert_false(btd_wmattrs_reload(f->attrs));
    char *text = g_strdup_printf("{ \"fx\" = { Icon = \"%s\"; AlwaysUserIcon = YES; }; }", f->own);
    g_assert_true(g_file_set_contents(btd_wmattrs_path(f->attrs), text, -1, NULL));
    g_assert_true(btd_wmattrs_reload(f->attrs));
    g_assert_cmpstr(btd_wmattrs_icon_for_window(f->attrs, "fx", "Fx"), ==, f->own);
    /* same content: no change reported */
    g_assert_false(btd_wmattrs_reload(f->attrs));
    g_free(text);
    /* broken file: what was read before the error stays, nothing crashes */
    g_assert_true(g_file_set_contents(btd_wmattrs_path(f->attrs),
                                      "{ thunar = { Icon = big-wm-folder.png; AlwaysUserIcon = YES; }; broken = {", -1,
                                      NULL));
    g_assert_true(btd_wmattrs_reload(f->attrs));
    g_assert_cmpstr(btd_wmattrs_icon_for_window(f->attrs, "thunar", "Thunar"), ==, f->folder);
    g_assert_true(btd_wmattrs_load_data(f->attrs, "\"unterminated", -1));
    g_assert_null(btd_wmattrs_icon_for_window(f->attrs, "thunar", "Thunar"));
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add("/wmattrs/window", Fixture, NULL, setup, test_window, teardown);
    g_test_add("/wmattrs/class", Fixture, NULL, setup, test_class, teardown);
    g_test_add("/wmattrs/file", Fixture, NULL, setup, test_file, teardown);
    return g_test_run();
}
