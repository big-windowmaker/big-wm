/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Shared look: the OPENSTEP style (light and dark, following
 * ~/.config/big-wm/theme as big-wm-theme writes it), icons, sound.
 */
#include "bwn.h"
#include <gio/gdesktopappinfo.h>
#include <string.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

static gboolean dark;
static GtkCssProvider *provider;
static GFileMonitor *theme_monitor;

gboolean
bwn_wayland(void)
{
#ifdef GDK_WINDOWING_WAYLAND
    return GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default());
#else
    return FALSE;
#endif
}

gboolean
bwn_dark(void)
{
    return dark;
}

/* The NeXT palette: gray panels, a black title strip as Window Maker's
 * focused title bar, bevelled edges (light top-left, dark bottom-right). */
static const char css_common[] =
    ".bwn-toast, .bwn-center { border: 1px solid #000000; border-radius: 0; }"
    ".bwn-header { padding: 3px 4px 3px 6px; border-radius: 0; }"
    ".bwn-header label { font-size: 9pt; }"
    ".bwn-app { font-weight: bold; }"
    ".bwn-time { opacity: 0.75; }"
    ".bwn-summary { font-weight: bold; font-size: 10pt; }"
    ".bwn-body { font-size: 9.5pt; }"
    ".bwn-content { padding: 8px 10px 10px 10px; }"
    ".bwn-close { min-width: 16px; min-height: 16px; padding: 0; border-radius: 0;"
    "  border: 1px solid; background-image: none; box-shadow: none; }"
    ".bwn-close label { font-weight: bold; font-size: 9pt; padding: 0 3px; }"
    ".bwn-actions { padding: 0 10px 10px 10px; }"
    ".bwn-actions button, .bwn-center-header button, .bwn-row-actions button { border-radius: 0; padding: 2px 10px;"
    "  background-image: none; box-shadow: none; border: 1px solid; }"
    ".bwn-row { border-radius: 0; }"
    ".bwn-row .bwn-content { padding: 6px 8px 8px 10px; }"
    ".bwn-unread-mark { min-width: 4px; }"
    ".bwn-center-header { padding: 6px 8px; }"
    ".bwn-center-title { font-weight: bold; font-size: 11pt; }"
    ".bwn-empty { padding: 32px 16px; opacity: 0.7; }"
    ".bwn-undo { padding: 4px 8px; }"
    ".bwn-catcher { background-color: transparent; }"
    "list.bwn-list, list.bwn-list row { background: transparent; padding: 0; }"
    ".bwn-count { font-size: 8pt; font-weight: bold; padding: 0 5px; border-radius: 0;"
    "  min-width: 10px; }"
    ".bwn-image { padding: 0; border-radius: 0; }"
    ".bwn-stack { min-height: 3px; margin: 0 6px; }"
    ".bwn-stack2 { min-height: 3px; margin: 0 12px; }"
    ".bwn-group-header { padding: 4px 8px; }"
    ".bwn-group-header label { font-weight: bold; }"
    ".bwn-chevron { min-width: 18px; min-height: 16px; padding: 0 4px; border-radius: 0;"
    "  background-image: none; box-shadow: none; border: 1px solid; }"
    ".bwn-body-row { padding: 8px 10px 10px 10px; }";

static const char css_light[] =
    ".bwn-toast, .bwn-center { background-color: #b3b3b3; color: #000000;"
    "  box-shadow: inset 1px 1px #ffffff, inset -1px -1px #555555; }"
    ".bwn-header { background-color: #000000; color: #ffffff; }"
    ".bwn-critical .bwn-header { background-color: #7a1414; }"
    ".bwn-close { background-color: #b3b3b3; color: #000000; border-color: #ffffff #555555 #555555 #ffffff; }"
    ".bwn-close:hover { background-color: #c8c8c8; }"
    ".bwn-close:active { border-color: #555555 #ffffff #ffffff #555555; }"
    ".bwn-actions button, .bwn-center-header button, .bwn-row-actions button { background-color: #aaaaaa; color: #000000;"
    "  border-color: #ffffff #555555 #555555 #ffffff; }"
    ".bwn-actions button:hover, .bwn-center-header button:hover { background-color: #c0c0c0; }"
    ".bwn-actions button:active, .bwn-center-header button:active { border-color: #555555 #ffffff #ffffff #555555; }"
    ".bwn-row { background-color: #c4c4c4; border-bottom: 1px solid #8a8a8a;"
    "  box-shadow: inset 1px 1px #e6e6e6; }"
    ".bwn-row:hover { background-color: #cdcdcd; }"
    ".bwn-unread-mark { background-color: #1f5aa6; }"
    ".bwn-center-header { background-color: #000000; color: #ffffff; }"
    ".bwn-center-header button { background-color: #b3b3b3; }"
    ".bwn-undo { background-color: #fff4c2; color: #000000; border-bottom: 1px solid #8a8a8a; }"
    ".bwn-scroll { background-color: #9a9a9a; }"
    ".bwn-count { background-color: #c8c8c8; color: #000000; border: 1px solid #555555; }"
    ".bwn-header .bwn-count { background-color: #3a3a3a; color: #ffffff; border-color: #8a8a8a; }"
    ".bwn-image { border: 1px solid; border-color: #555555 #ffffff #ffffff #555555; }"
    ".bwn-stack { background-color: #a8a8a8; border: 1px solid #6a6a6a; border-top: none; }"
    ".bwn-stack2 { background-color: #9e9e9e; border: 1px solid #6a6a6a; border-top: none; }"
    ".bwn-group-header { background-color: #a0a0a0; border-bottom: 1px solid #6a6a6a; }"
    ".bwn-chevron { background-color: #b3b3b3; color: #000000; border-color: #ffffff #555555 #555555 #ffffff; }"
    ".bwn-chevron:hover { background-color: #c8c8c8; }";

static const char css_dark[] =
    ".bwn-toast, .bwn-center { background-color: #3c3c3c; color: #e8e8e8;"
    "  box-shadow: inset 1px 1px #6a6a6a, inset -1px -1px #161616; }"
    ".bwn-header { background-color: #000000; color: #ffffff; }"
    ".bwn-critical .bwn-header { background-color: #8a1c1c; }"
    ".bwn-close { background-color: #3c3c3c; color: #e8e8e8; border-color: #6a6a6a #161616 #161616 #6a6a6a; }"
    ".bwn-close:hover { background-color: #4c4c4c; }"
    ".bwn-close:active { border-color: #161616 #6a6a6a #6a6a6a #161616; }"
    ".bwn-actions button, .bwn-center-header button, .bwn-row-actions button { background-color: #4a4a4a; color: #e8e8e8;"
    "  border-color: #6a6a6a #161616 #161616 #6a6a6a; }"
    ".bwn-actions button:hover, .bwn-center-header button:hover { background-color: #575757; }"
    ".bwn-actions button:active, .bwn-center-header button:active { border-color: #161616 #6a6a6a #6a6a6a #161616; }"
    ".bwn-row { background-color: #464646; border-bottom: 1px solid #222222;"
    "  box-shadow: inset 1px 1px #5c5c5c; }"
    ".bwn-row:hover { background-color: #4e4e4e; }"
    ".bwn-unread-mark { background-color: #5b9be6; }"
    ".bwn-center-header { background-color: #000000; color: #ffffff; }"
    ".bwn-undo { background-color: #5a5226; color: #f2f2f2; border-bottom: 1px solid #222222; }"
    ".bwn-scroll { background-color: #2a2a2a; }"
    ".bwn-count { background-color: #5a5a5a; color: #ffffff; border: 1px solid #8a8a8a; }"
    ".bwn-header .bwn-count { background-color: #3a3a3a; color: #ffffff; border-color: #8a8a8a; }"
    ".bwn-image { border: 1px solid; border-color: #161616 #6a6a6a #6a6a6a #161616; }"
    ".bwn-stack { background-color: #3a3a3a; border: 1px solid #161616; border-top: none; }"
    ".bwn-stack2 { background-color: #333333; border: 1px solid #161616; border-top: none; }"
    ".bwn-group-header { background-color: #2e2e2e; border-bottom: 1px solid #161616; }"
    ".bwn-chevron { background-color: #4a4a4a; color: #e8e8e8; border-color: #6a6a6a #161616 #161616 #6a6a6a; }"
    ".bwn-chevron:hover { background-color: #575757; }";

static void
read_theme(void)
{
    g_autofree char *path = g_build_filename(g_get_user_config_dir(), "big-wm", "theme", NULL);
    g_autofree char *mode = NULL;
    gboolean d = FALSE;
    if (g_file_get_contents(path, &mode, NULL, NULL))
        d = g_str_has_prefix(g_strstrip(mode), "dark");
    else
        g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &d, NULL);
    dark = d;
    g_autofree char *css = g_strconcat(css_common, dark ? css_dark : css_light, NULL);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
}

static void
theme_changed(GFileMonitor *m, GFile *f, GFile *o, GFileMonitorEvent e, gpointer data)
{
    (void)m; (void)f; (void)o; (void)data;
    if (e == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || e == G_FILE_MONITOR_EVENT_CREATED ||
        e == G_FILE_MONITOR_EVENT_DELETED || e == G_FILE_MONITOR_EVENT_RENAMED ||
        e == G_FILE_MONITOR_EVENT_MOVED_IN) {
        read_theme();
        bwn_sni_update();
    }
}

void
bwn_style_init(void)
{
    provider = gtk_css_provider_new();
    /* APPLICATION + 1: above the GTK theme, so the panels keep the
     * OPENSTEP look with any GTK theme. */
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    read_theme();
    g_autofree char *path = g_build_filename(g_get_user_config_dir(), "big-wm", "theme", NULL);
    g_autoptr(GFile) file = g_file_new_for_path(path);
    theme_monitor = g_file_monitor_file(file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
    if (theme_monitor)
        g_signal_connect(theme_monitor, "changed", G_CALLBACK(theme_changed), NULL);
}

/* == Icons ================================================================= */

static GdkPixbuf *
fit(GdkPixbuf *pb, int px)
{
    if (!pb)
        return NULL;
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    if (w == px && h == px)
        return pb;
    double s = (double)px / MAX(w, h); /* keep the aspect ratio */
    GdkPixbuf *r = gdk_pixbuf_scale_simple(pb, MAX(1, (int)(w * s + 0.5)), MAX(1, (int)(h * s + 0.5)),
                                           GDK_INTERP_HYPER);
    g_object_unref(pb);
    return r;
}

static GdkPixbuf *
load_named(const char *name, int px)
{
    if (!name || !*name)
        return NULL;
    if (g_path_is_absolute(name) || g_str_has_prefix(name, "file://")) {
        g_autofree char *file = g_str_has_prefix(name, "file://") ? g_filename_from_uri(name, NULL, NULL)
                                                                  : g_strdup(name);
        return file ? fit(gdk_pixbuf_new_from_file_at_size(file, px, px, NULL), px) : NULL;
    }
    if (strchr(name, '/'))
        return NULL;
    return fit(gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), name, px,
                                        GTK_ICON_LOOKUP_FORCE_SIZE, NULL),
               px);
}

static GDesktopAppInfo *
app_info(BwnNotif *n)
{
    if (!n->desktop_entry)
        return NULL;
    g_autofree char *id = g_str_has_suffix(n->desktop_entry, ".desktop")
                              ? g_strdup(n->desktop_entry)
                              : g_strconcat(n->desktop_entry, ".desktop", NULL);
    return g_desktop_app_info_new(id);
}

/* The sender's image, its icon, the desktop entry's icon, an icon named as
 * the application, a generic one. */
GdkPixbuf *
bwn_icon_for(BwnNotif *n, int size, int scale)
{
    int px = size * MAX(scale, 1);
    if (n->image)
        return fit(g_object_ref(n->image), px);
    return bwn_app_icon_for(n, size, scale);
}

/* The application's icon (never the sender's image: that one goes on the
 * right, as GNOME 46 shows it). */
GdkPixbuf *
bwn_app_icon_for(BwnNotif *n, int size, int scale)
{
    int px = size * MAX(scale, 1);
    GdkPixbuf *pb = load_named(n->app_icon, px);
    if (pb)
        return pb;
    g_autoptr(GDesktopAppInfo) info = app_info(n);
    if (info) {
        GIcon *gi = g_app_info_get_icon(G_APP_INFO(info));
        if (gi) {
            g_autoptr(GtkIconInfo) ii = gtk_icon_theme_lookup_by_gicon(
                gtk_icon_theme_get_default(), gi, px, GTK_ICON_LOOKUP_FORCE_SIZE);
            if (ii && (pb = gtk_icon_info_load_icon(ii, NULL)))
                return fit(pb, px);
        }
    }
    if (n->app_name && *n->app_name) {
        g_autofree char *lower = g_utf8_strdown(n->app_name, -1);
        g_strdelimit(lower, " ", '-');
        if ((pb = load_named(lower, px)))
            return pb;
    }
    const char *generic[] = { "preferences-system-notifications", "dialog-information",
                              "image-missing" };
    for (gsize i = 0; i < G_N_ELEMENTS(generic); i++)
        if ((pb = load_named(generic[i], px)))
            return pb;
    return NULL;
}

static int
scale_factor(void)
{
    GdkMonitor *m = gdk_display_get_monitor(gdk_display_get_default(), 0);
    return m ? gdk_monitor_get_scale_factor(m) : 1;
}

static GtkWidget *
image_from(GdkPixbuf *pb, int size, int scale)
{
    GtkWidget *img = gtk_image_new();
    if (pb) {
        cairo_surface_t *s = gdk_cairo_surface_create_from_pixbuf(pb, scale, NULL);
        gtk_image_set_from_surface(GTK_IMAGE(img), s);
        cairo_surface_destroy(s);
        g_object_unref(pb);
    }
    gtk_widget_set_size_request(img, size, size);
    gtk_widget_set_valign(img, GTK_ALIGN_START);
    return img;
}

GtkWidget *
bwn_app_icon_widget(BwnNotif *n, int size)
{
    int scale = scale_factor();
    return image_from(bwn_app_icon_for(n, size, scale), size, scale);
}

/* The sender's image (a photo, an avatar, a cover), framed in a sunken
 * bevel; NULL without one. */
GtkWidget *
bwn_image_widget(BwnNotif *n, int size)
{
    if (!n->image)
        return NULL;
    int scale = scale_factor();
    GtkWidget *img = image_from(fit(g_object_ref(n->image), size * scale), size, scale);
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_style_context_add_class(gtk_widget_get_style_context(frame), "bwn-image");
    gtk_container_add(GTK_CONTAINER(frame), img);
    gtk_widget_set_valign(frame, GTK_ALIGN_START);
    return frame;
}

/* "3": how many notifications of the application are stacked. */
GtkWidget *
bwn_count_tag(int count)
{
    char text[16];
    g_snprintf(text, sizeof text, count > 99 ? "99+" : "%d", count);
    GtkWidget *l = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "bwn-count");
    gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
    g_autofree char *tip = g_strdup_printf(ngettext("%d notification", "%d notifications", count), count);
    gtk_widget_set_tooltip_text(l, tip);
    return l;
}

GtkWidget *
bwn_icon_widget(BwnNotif *n, int size)
{
    GtkWidget *img = gtk_image_new();
    int scale = scale_factor();
    GdkPixbuf *pb = bwn_icon_for(n, size, scale);
    if (pb) {
        cairo_surface_t *s = gdk_cairo_surface_create_from_pixbuf(pb, scale, NULL);
        gtk_image_set_from_surface(GTK_IMAGE(img), s);
        cairo_surface_destroy(s);
        g_object_unref(pb);
    }
    gtk_widget_set_size_request(img, size, size);
    gtk_widget_set_valign(img, GTK_ALIGN_START);
    return img;
}

char *
bwn_app_label(BwnNotif *n)
{
    if (n->app_name && *n->app_name)
        return g_strdup(n->app_name);
    g_autoptr(GDesktopAppInfo) info = app_info(n);
    if (info)
        return g_strdup(g_app_info_get_display_name(G_APP_INFO(info)));
    return g_strdup(_("Notification"));
}

/* Optional (off by default): the freedesktop sound theme, if libcanberra's
 * player is installed. Nothing is linked for it. */
void
bwn_play_sound(BwnNotif *n)
{
    const char *id = n->urgency == BWN_URGENCY_CRITICAL ? "dialog-warning" : "message-new-instant";
    g_autofree char *player = g_find_program_in_path("canberra-gtk-play");
    if (!player)
        return;
    const char *argv[] = { player, "-i", id, NULL };
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
}
