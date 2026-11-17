/* stealer: maps a small toplevel, waits argv[1] ms, then asks xdg-activation
 * to activate itself with a token that has NO seat/serial (no user input),
 * as an app that "presents" its window when a message arrives. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell.h"
#include "xdg-activation.h"

static struct wl_compositor *comp;
static struct wl_shm *shm;
static struct xdg_wm_base *wm;
static struct xdg_activation_v1 *act;
static const char *token_str;
static struct wl_seat *seat;
static uint32_t press_serial;
static void pe(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *f, wl_fixed_t x, wl_fixed_t y) { (void)d;(void)p;(void)s;(void)f;(void)x;(void)y; }
static void pl(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *f) { (void)d;(void)p;(void)s;(void)f; }
static void pm(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y) { (void)d;(void)p;(void)t;(void)x;(void)y; }
static void pb(void *d, struct wl_pointer *p, uint32_t s, uint32_t t, uint32_t b, uint32_t st) { (void)d;(void)p;(void)t;(void)b; if (st) press_serial = s; }
static void pa(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v) { (void)d;(void)p;(void)t;(void)a;(void)v; }
static const struct wl_pointer_listener pl_l = { .enter = pe, .leave = pl, .motion = pm, .button = pb, .axis = pa };

static void g(void *d, struct wl_registry *r, uint32_t n, const char *i, uint32_t v) {
    (void)d; (void)v;
    if (!strcmp(i, wl_compositor_interface.name)) comp = wl_registry_bind(r, n, &wl_compositor_interface, 4);
    else if (!strcmp(i, wl_shm_interface.name)) shm = wl_registry_bind(r, n, &wl_shm_interface, 1);
    else if (!strcmp(i, xdg_wm_base_interface.name)) wm = wl_registry_bind(r, n, &xdg_wm_base_interface, 1);
    else if (!strcmp(i, xdg_activation_v1_interface.name)) act = wl_registry_bind(r, n, &xdg_activation_v1_interface, 1);
    else if (!strcmp(i, wl_seat_interface.name) && !seat) seat = wl_registry_bind(r, n, &wl_seat_interface, 1);
}
static void gr(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener rl = { g, gr };
static void ping(void *d, struct xdg_wm_base *b, uint32_t s) { (void)d; xdg_wm_base_pong(b, s); }
static const struct xdg_wm_base_listener wml = { ping };
static int configured;
static void sconf(void *d, struct xdg_surface *s, uint32_t serial) { (void)d; xdg_surface_ack_configure(s, serial); configured = 1; }
static const struct xdg_surface_listener sl = { sconf };
static void tconf(void *d, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *s) { (void)d; (void)t; (void)w; (void)h; (void)s; }
static void tclose(void *d, struct xdg_toplevel *t) { (void)d; (void)t; exit(0); }
static const struct xdg_toplevel_listener tl = { tconf, tclose, NULL, NULL };
static void tdone(void *d, struct xdg_activation_token_v1 *t, const char *s) { (void)d; (void)t; token_str = strdup(s); }
static const struct xdg_activation_token_v1_listener al = { tdone };

int main(int argc, char **argv) {
    int wait_ms = argc > 1 ? atoi(argv[1]) : 3000;
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) return 1;
    struct wl_registry *reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &rl, NULL);
    wl_display_roundtrip(dpy);
    if (!comp || !shm || !wm || !act) { fprintf(stderr, "stealer: missing globals\n"); return 1; }
    xdg_wm_base_add_listener(wm, &wml, NULL);
    struct wl_surface *surf = wl_compositor_create_surface(comp);
    struct xdg_surface *xs = xdg_wm_base_get_xdg_surface(wm, surf);
    xdg_surface_add_listener(xs, &sl, NULL);
    struct xdg_toplevel *top = xdg_surface_get_toplevel(xs);
    xdg_toplevel_add_listener(top, &tl, NULL);
    xdg_toplevel_set_title(top, "stealer");
    wl_surface_commit(surf);
    while (!configured) wl_display_dispatch(dpy);
    int W = 200, H = 100, stride = W * 4, size = stride * H;
    int fd = memfd_create("buf", 0);
    if (ftruncate(fd, size)) return 1;
    uint32_t *px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    for (int i = 0; i < W * H; i++) px[i] = 0xff3366aa;
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_ARGB8888);
    wl_surface_attach(surf, buf, 0, 0);
    wl_surface_commit(surf);
    wl_display_roundtrip(dpy);
    int mode_old_serial = argc > 2 && !strcmp(argv[2], "old-serial");
    if (mode_old_serial) {
        struct wl_pointer *ptr = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(ptr, &pl_l, NULL);
        fprintf(stderr, "stealer: waiting for a click\n");
        while (!press_serial) wl_display_dispatch(dpy);
        fprintf(stderr, "stealer: got serial %u\n", press_serial);
    }
    fprintf(stderr, "stealer: mapped, waiting %d ms\n", wait_ms);
    usleep(wait_ms * 1000);
    wl_display_dispatch_pending(dpy);
    struct xdg_activation_token_v1 *tok = xdg_activation_v1_get_activation_token(act);
    xdg_activation_token_v1_add_listener(tok, &al, NULL);
    if (mode_old_serial)
        xdg_activation_token_v1_set_serial(tok, press_serial, seat); /* old serial, no surface */
    else
        xdg_activation_token_v1_set_surface(tok, surf);   /* no set_serial: no user input */
    xdg_activation_token_v1_commit(tok);
    while (!token_str) wl_display_dispatch(dpy);
    xdg_activation_v1_activate(act, token_str, surf);
    wl_display_roundtrip(dpy);
    fprintf(stderr, "stealer: activation requested\n");
    while (wl_display_dispatch(dpy) != -1) {}
    return 0;
}
