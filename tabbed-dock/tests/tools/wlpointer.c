/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Test tool: drives a virtual pointer through zwlr_virtual_pointer_v1, the
 * Wayland counterpart of `xdotool mousemove/click` in the integration tests.
 *
 *   wlpointer WIDTH HEIGHT cmd...
 *     move X Y | down [BTN] | up [BTN] | click [BTN] | scroll N | sleep MS
 * BTN: 1 left, 2 middle, 3 right. Coordinates are in the output layout.
 */
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

static struct wl_seat *seat;
static struct zwlr_virtual_pointer_manager_v1 *mgr;

static void
global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t v)
{
    if (!strcmp(iface, wl_seat_interface.name))
        seat = wl_registry_bind(r, name, &wl_seat_interface, 1);
    else if (!strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name))
        mgr = wl_registry_bind(r, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
}
static void global_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener rl = { global, global_remove };

static uint32_t
now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint32_t
btn(const char *s)
{
    int b = s ? atoi(s) : 1;
    return b == 3 ? BTN_RIGHT : b == 2 ? BTN_MIDDLE : BTN_LEFT;
}

int
main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: wlpointer W H cmd...\n");
        return 2;
    }
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy)
        return 1;
    struct wl_registry *reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!mgr) {
        fprintf(stderr, "no zwlr_virtual_pointer_manager_v1\n");
        return 1;
    }
    struct zwlr_virtual_pointer_v1 *p = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(mgr, seat);
    uint32_t w = (uint32_t)atoi(argv[1]), h = (uint32_t)atoi(argv[2]);
    for (int i = 3; i < argc; i++) {
        const char *c = argv[i];
        const char *next = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(c, "move") && i + 2 < argc) {
            zwlr_virtual_pointer_v1_motion_absolute(p, now_ms(), (uint32_t)atoi(argv[i + 1]),
                                                    (uint32_t)atoi(argv[i + 2]), w, h);
            zwlr_virtual_pointer_v1_frame(p);
            i += 2;
        } else if (!strcmp(c, "down") || !strcmp(c, "up") || !strcmp(c, "click")) {
            int has = next && next[0] >= '1' && next[0] <= '3' && !next[1];
            uint32_t b = btn(has ? next : NULL);
            if (strcmp(c, "up")) {
                zwlr_virtual_pointer_v1_button(p, now_ms(), b, WL_POINTER_BUTTON_STATE_PRESSED);
                zwlr_virtual_pointer_v1_frame(p);
            }
            if (!strcmp(c, "click")) {
                wl_display_roundtrip(dpy);
                usleep(80000);
            }
            if (strcmp(c, "down")) {
                zwlr_virtual_pointer_v1_button(p, now_ms(), b, WL_POINTER_BUTTON_STATE_RELEASED);
                zwlr_virtual_pointer_v1_frame(p);
            }
            if (has)
                i++;
        } else if (!strcmp(c, "scroll") && next) {
            zwlr_virtual_pointer_v1_axis_source(p, WL_POINTER_AXIS_SOURCE_WHEEL);
            zwlr_virtual_pointer_v1_axis_discrete(p, now_ms(), WL_POINTER_AXIS_VERTICAL_SCROLL,
                                                  wl_fixed_from_int(atoi(next) * 15), atoi(next));
            zwlr_virtual_pointer_v1_frame(p);
            i++;
        } else if (!strcmp(c, "sleep") && next) {
            wl_display_roundtrip(dpy);
            usleep((useconds_t)atoi(next) * 1000);
            i++;
        }
        wl_display_roundtrip(dpy);
    }
    zwlr_virtual_pointer_v1_destroy(p);
    wl_display_roundtrip(dpy);
    wl_display_disconnect(dpy);
    return 0;
}
