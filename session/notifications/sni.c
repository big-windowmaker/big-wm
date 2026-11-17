/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The bell in the tray: a StatusNotifierItem (big-wm-tray on Wayland,
 * snixembed + wmsystemtray on X11). The icon is drawn here (IconPixmap), so
 * it carries the unread count and the "do not disturb" state with any icon
 * theme. Click: the notification center. Middle click: do not disturb.
 */
#include "bwn.h"
#include <math.h>

static const char sni_xml[] =
    "<node>"
    " <interface name='org.kde.StatusNotifierItem'>"
    "  <property name='Category' type='s' access='read'/>"
    "  <property name='Id' type='s' access='read'/>"
    "  <property name='Title' type='s' access='read'/>"
    "  <property name='Status' type='s' access='read'/>"
    "  <property name='WindowId' type='i' access='read'/>"
    "  <property name='IconName' type='s' access='read'/>"
    "  <property name='IconPixmap' type='a(iiay)' access='read'/>"
    "  <property name='OverlayIconName' type='s' access='read'/>"
    "  <property name='AttentionIconName' type='s' access='read'/>"
    "  <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "  <property name='ItemIsMenu' type='b' access='read'/>"
    "  <method name='ContextMenu'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='Activate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='SecondaryActivate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
    "  <method name='Scroll'><arg type='i' direction='in'/><arg type='s' direction='in'/></method>"
    "  <signal name='NewTitle'/><signal name='NewIcon'/><signal name='NewToolTip'/>"
    "  <signal name='NewStatus'><arg type='s'/></signal>"
    " </interface>"
    "</node>";

#define SNI_PATH "/StatusNotifierItem"

static GDBusConnection *bus;
static int last_unread = -1, last_dnd = -1, last_dark = -1;

/* == The bell =============================================================== */

static void
bell_path(cairo_t *cr, double s)
{
    /* A classic bell: dome, flared rim, clapper. Unit square scaled to s. */
    cairo_save(cr);
    cairo_scale(cr, s, s);
    cairo_move_to(cr, 0.50, 0.10);
    cairo_curve_to(cr, 0.30, 0.10, 0.24, 0.28, 0.24, 0.44);
    cairo_line_to(cr, 0.24, 0.60);
    cairo_curve_to(cr, 0.24, 0.66, 0.17, 0.70, 0.13, 0.74);
    cairo_line_to(cr, 0.87, 0.74);
    cairo_curve_to(cr, 0.83, 0.70, 0.76, 0.66, 0.76, 0.60);
    cairo_line_to(cr, 0.76, 0.44);
    cairo_curve_to(cr, 0.76, 0.28, 0.70, 0.10, 0.50, 0.10);
    cairo_close_path(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0.50, 0.82, 0.08, 0, 2 * G_PI);
    cairo_restore(cr);
}

static GVariant *
pixmap(int size, int unread, gboolean dnd, gboolean dark)
{
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surf);
    double fg = dark ? 0.90 : 0.13, hi = dark ? 0.55 : 0.95;
    double lw = MAX(1.0, size / 22.0);

    /* Bevel: a light offset copy under the dark bell (the NeXT engraving). */
    cairo_translate(cr, lw * 0.6, lw * 0.6);
    bell_path(cr, size);
    cairo_set_source_rgba(cr, hi, hi, hi, dnd ? 0.35 : 0.7);
    cairo_fill(cr);
    cairo_translate(cr, -lw * 0.6, -lw * 0.6);
    bell_path(cr, size);
    cairo_set_source_rgba(cr, fg, fg, fg, dnd ? 0.55 : 1.0);
    cairo_fill(cr);

    if (dnd) { /* a slash: muted */
        cairo_set_line_width(cr, lw * 2.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, size * 0.14, size * 0.86);
        cairo_line_to(cr, size * 0.86, size * 0.14);
        cairo_set_source_rgb(cr, 0.70, 0.10, 0.10);
        cairo_stroke(cr);
    } else if (unread > 0) { /* the count on a red badge */
        char text[8];
        if (unread > 99)
            g_strlcpy(text, "99+", sizeof text);
        else
            g_snprintf(text, sizeof text, "%d", unread);
        double r = size * (strlen(text) > 1 ? 0.30 : 0.25);
        double cx = size - r, cy = r;
        cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
        cairo_set_source_rgb(cr, 0.78, 0.10, 0.10);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, MAX(1.0, size / 32.0));
        cairo_stroke(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, r * (strlen(text) > 2 ? 0.95 : strlen(text) > 1 ? 1.15 : 1.35));
        cairo_text_extents_t ext;
        cairo_text_extents(cr, text, &ext);
        cairo_move_to(cr, cx - ext.width / 2 - ext.x_bearing, cy - ext.height / 2 - ext.y_bearing);
        cairo_show_text(cr, text);
    }
    cairo_destroy(cr);
    cairo_surface_flush(surf);

    /* SNI: ARGB32 in network byte order, not premultiplied. */
    int stride = cairo_image_surface_get_stride(surf);
    const guchar *src = cairo_image_surface_get_data(surf);
    guchar *out = g_malloc(size * size * 4);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            guint32 p = *(const guint32 *)(src + y * stride + x * 4);
            guint a = p >> 24, r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;
            if (a && a < 255) {
                r = MIN(255u, r * 255 / a);
                g = MIN(255u, g * 255 / a);
                b = MIN(255u, b * 255 / a);
            }
            guchar *d = out + (y * size + x) * 4;
            d[0] = a, d[1] = r, d[2] = g, d[3] = b;
        }
    cairo_surface_destroy(surf);
    return g_variant_new("(ii@ay)", size, size,
                         g_variant_new_from_data(G_VARIANT_TYPE_BYTESTRING, out, size * size * 4, TRUE,
                                                 g_free, out));
}

static GVariant *
pixmaps(void)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a(iiay)"));
    int unread = bwn_store_unread();
    gboolean dnd = bwn_settings()->dnd;
    const int sizes[] = { 22, 32, 48 };
    for (gsize i = 0; i < G_N_ELEMENTS(sizes); i++)
        g_variant_builder_add_value(&b, pixmap(sizes[i], unread, dnd, bwn_dark()));
    return g_variant_builder_end(&b);
}

static char *
tooltip_text(void)
{
    int unread = bwn_store_unread();
    if (bwn_settings()->dnd)
        return unread ? g_strdup_printf(ngettext("Do not disturb is on · %d unread",
                                                 "Do not disturb is on · %d unread", unread), unread)
                      : g_strdup(_("Do not disturb is on"));
    if (unread)
        return g_strdup_printf(ngettext("%d unread notification", "%d unread notifications", unread), unread);
    return g_strdup(_("No new notifications"));
}

/* == D-Bus ================================================================== */

static GVariant *
get_prop(GDBusConnection *c, const char *sender, const char *path, const char *iface,
         const char *prop, GError **error, gpointer data)
{
    (void)c; (void)sender; (void)path; (void)iface; (void)error; (void)data;
    if (g_str_equal(prop, "Category"))
        return g_variant_new_string("SystemServices");
    if (g_str_equal(prop, "Id"))
        return g_variant_new_string("big-wm-notifications");
    if (g_str_equal(prop, "Title"))
        return g_variant_new_string(_("Notifications"));
    if (g_str_equal(prop, "Status"))
        return g_variant_new_string("Active");
    if (g_str_equal(prop, "WindowId"))
        return g_variant_new_int32(0);
    if (g_str_equal(prop, "IconName") || g_str_equal(prop, "OverlayIconName") ||
        g_str_equal(prop, "AttentionIconName"))
        return g_variant_new_string("");
    if (g_str_equal(prop, "IconPixmap"))
        return pixmaps();
    if (g_str_equal(prop, "ToolTip")) {
        g_autofree char *text = tooltip_text();
        return g_variant_new("(s@a(iiay)ss)", "", g_variant_new_array(G_VARIANT_TYPE("(iiay)"), NULL, 0),
                             _("Notifications"), text);
    }
    if (g_str_equal(prop, "ItemIsMenu"))
        return g_variant_new_boolean(FALSE);
    return NULL;
}

static void
call(GDBusConnection *c, const char *sender, const char *path, const char *iface, const char *method,
     GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    (void)c; (void)sender; (void)path; (void)iface; (void)params; (void)data;
    if (g_str_equal(method, "Activate") || g_str_equal(method, "ContextMenu"))
        bwn_center_toggle();
    else if (g_str_equal(method, "SecondaryActivate"))
        bwn_set_dnd(!bwn_settings()->dnd);
    g_dbus_method_invocation_return_value(inv, NULL);
}

static const GDBusInterfaceVTable vtable = { call, get_prop, NULL, { 0 } };

static void
emit(const char *signal)
{
    g_dbus_connection_emit_signal(bus, NULL, SNI_PATH, "org.kde.StatusNotifierItem", signal, NULL, NULL);
}

void
bwn_sni_update(void)
{
    if (!bus)
        return;
    int unread = bwn_store_unread(), dnd = bwn_settings()->dnd, dark = bwn_dark();
    if (unread == last_unread && dnd == last_dnd && dark == last_dark)
        return;
    last_unread = unread, last_dnd = dnd, last_dark = dark;
    emit("NewIcon");
    emit("NewToolTip");
}

static void
changed(gpointer data)
{
    (void)data;
    bwn_sni_update();
}

static void
watcher_appeared(GDBusConnection *c, const char *name, const char *owner, gpointer data)
{
    (void)name; (void)owner; (void)data;
    g_dbus_connection_call(c, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                           "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
                           g_variant_new("(s)", g_dbus_connection_get_unique_name(c)), NULL,
                           G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL, NULL);
}

void
bwn_sni_start(GDBusConnection *connection)
{
    bus = connection;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(sni_xml, NULL);
    g_dbus_connection_register_object(bus, SNI_PATH, info->interfaces[0], &vtable, NULL, NULL, NULL);
    bwn_store_connect_changed(changed, NULL);
    g_bus_watch_name_on_connection(bus, "org.kde.StatusNotifierWatcher", G_BUS_NAME_WATCHER_FLAGS_NONE,
                                   watcher_appeared, NULL, NULL, NULL);
}
