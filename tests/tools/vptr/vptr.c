// vptr: moves/presses a virtual pointer (wlr-virtual-pointer-unstable-v1).
// Commands (in sequence): extent W H | move X Y | down | up | sleep MS | wheel N
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "wlr-virtual-pointer-unstable-v1-client.h"
static struct zwlr_virtual_pointer_manager_v1 *mgr;
static struct wl_seat *seat;
static void reg(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v) {
    if (!strcmp(i, zwlr_virtual_pointer_manager_v1_interface.name))
        mgr = wl_registry_bind(r, n, &zwlr_virtual_pointer_manager_v1_interface, 1);
    else if (!strcmp(i, wl_seat_interface.name) && !seat)
        seat = wl_registry_bind(r, n, &wl_seat_interface, 1);
}
static void unreg(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener rl = { reg, unreg };
static uint32_t now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }
int main(int argc, char **argv) {
    struct wl_display *d = wl_display_connect(NULL);
    if (!d) { fprintf(stderr, "no display\n"); return 1; }
    struct wl_registry *r = wl_display_get_registry(d);
    wl_registry_add_listener(r, &rl, NULL);
    wl_display_roundtrip(d);
    if (!mgr) { fprintf(stderr, "no virtual pointer manager\n"); return 1; }
    struct zwlr_virtual_pointer_v1 *p = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(mgr, seat);
    uint32_t w = 1, h = 1;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "extent") && i + 2 < argc) { w = atoi(argv[++i]); h = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "move") && i + 2 < argc) {
            int x = atoi(argv[++i]), y = atoi(argv[++i]);
            zwlr_virtual_pointer_v1_motion_absolute(p, now(), x, y, w, h);
            zwlr_virtual_pointer_v1_frame(p);
        } else if (!strcmp(argv[i], "down") || !strcmp(argv[i], "up")) {
            zwlr_virtual_pointer_v1_button(p, now(), 0x110, !strcmp(argv[i], "down"));
            zwlr_virtual_pointer_v1_frame(p);
        } else if (!strcmp(argv[i], "sleep") && i + 1 < argc) {
            wl_display_flush(d); usleep(atoi(argv[++i]) * 1000);
        } else if (!strcmp(argv[i], "wheel") && i + 1 < argc) {
            int n = atoi(argv[++i]);
            zwlr_virtual_pointer_v1_axis_source(p, 0);
            zwlr_virtual_pointer_v1_axis_discrete(p, now(), 0, wl_fixed_from_int(15 * n), n);
            zwlr_virtual_pointer_v1_frame(p);
        } else { fprintf(stderr, "bad arg %s\n", argv[i]); return 2; }
        wl_display_flush(d);
    }
    wl_display_roundtrip(d);
    zwlr_virtual_pointer_v1_destroy(p);
    wl_display_roundtrip(d);
    return 0;
}
