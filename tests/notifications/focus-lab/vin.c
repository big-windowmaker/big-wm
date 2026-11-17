/* vin: virtual pointer + keyboard for a nested wlmaker lab.
 * stdin commands: move X Y | down | up | scroll N | key CODE | sleep MS */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>
#include "vp.h"
#include "vk.h"

static struct wl_seat *seat;
static struct zwlr_virtual_pointer_manager_v1 *vpm;
static struct zwp_virtual_keyboard_manager_v1 *vkm;

static void reg_global(void *d, struct wl_registry *r, uint32_t name,
                       const char *iface, uint32_t ver) {
    (void)d;
    if (!strcmp(iface, wl_seat_interface.name))
        seat = wl_registry_bind(r, name, &wl_seat_interface, 1);
    else if (!strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name))
        vpm = wl_registry_bind(r, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    else if (!strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name))
        vkm = wl_registry_bind(r, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
    (void)ver;
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener rl = { reg_global, reg_remove };

static uint32_t now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(int argc, char **argv) {
    int W = argc > 2 ? atoi(argv[1]) : 1280, H = argc > 2 ? atoi(argv[2]) : 800;
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) { fprintf(stderr, "vin: no display\n"); return 1; }
    struct wl_registry *reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!vpm || !vkm || !seat) { fprintf(stderr, "vin: missing vp=%p vk=%p\n", (void*)vpm, (void*)vkm); return 1; }
    struct zwlr_virtual_pointer_v1 *vp = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(vpm, seat);
    struct zwp_virtual_keyboard_v1 *vk = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(vkm, seat);

    struct xkb_context *ctx = xkb_context_new(0);
    struct xkb_rule_names names = { .layout = "us" };
    struct xkb_keymap *km = xkb_keymap_new_from_names(ctx, &names, 0);
    char *s = xkb_keymap_get_as_string(km, XKB_KEYMAP_FORMAT_TEXT_V1);
    size_t len = strlen(s) + 1;
    int fd = memfd_create("km", 0);
    if (write(fd, s, len) != (ssize_t)len) return 1;
    zwp_virtual_keyboard_v1_keymap(vk, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, len);
    wl_display_roundtrip(dpy);

    char line[128];
    while (fgets(line, sizeof line, stdin)) {
        int a, b;
        if (sscanf(line, "move %d %d", &a, &b) == 2) {
            zwlr_virtual_pointer_v1_motion_absolute(vp, now(), a, b, W, H);
            zwlr_virtual_pointer_v1_frame(vp);
        } else if (!strncmp(line, "down", 4)) {
            zwlr_virtual_pointer_v1_button(vp, now(), BTN_LEFT, 1);
            zwlr_virtual_pointer_v1_frame(vp);
        } else if (!strncmp(line, "up", 2)) {
            zwlr_virtual_pointer_v1_button(vp, now(), BTN_LEFT, 0);
            zwlr_virtual_pointer_v1_frame(vp);
        } else if (sscanf(line, "scroll %d", &a) == 1) {
            zwlr_virtual_pointer_v1_axis_source(vp, WL_POINTER_AXIS_SOURCE_WHEEL);
            zwlr_virtual_pointer_v1_axis_discrete(vp, now(), WL_POINTER_AXIS_VERTICAL_SCROLL,
                                                  wl_fixed_from_int(15 * a), a);
            zwlr_virtual_pointer_v1_frame(vp);
        } else if (sscanf(line, "key %d", &a) == 1) {
            zwp_virtual_keyboard_v1_key(vk, now(), a, WL_KEYBOARD_KEY_STATE_PRESSED);
            zwp_virtual_keyboard_v1_key(vk, now(), a, WL_KEYBOARD_KEY_STATE_RELEASED);
        } else if (sscanf(line, "sleep %d", &a) == 1) {
            wl_display_flush(dpy); usleep(a * 1000);
        }
        wl_display_flush(dpy);
        wl_display_roundtrip(dpy);
    }
    wl_display_roundtrip(dpy);
    return 0;
}
