/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-gamepad: veja big-wm-gamepad.h.
 */
#define _GNU_SOURCE
#include "big-wm-gamepad.h"

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define REPEAT_FIRST_MS 400
#define REPEAT_NEXT_MS 110
#define STICK_ON 0.6
#define STICK_OFF 0.35

#define NBITS(x) ((((x) - 1) / (8 * sizeof(unsigned long))) + 1)
#define TEST_BIT(bit, array) (((array)[(bit) / (8 * sizeof(unsigned long))] >> ((bit) % (8 * sizeof(unsigned long)))) & 1)

long long
bwg_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int
add_pad(bwg *g, int fd, const char *node, int ax_min, int ax_max)
{
    if (g->npads >= BWG_MAX_PADS)
        return -1;
    bwg_pad *p = &g->pad[g->npads++];
    memset(p, 0, sizeof *p);
    p->fd = fd;
    p->ax_min[0] = p->ax_min[1] = ax_min;
    p->ax_max[0] = p->ax_max[1] = ax_max;
    snprintf(p->node, sizeof p->node, "%s", node ? node : "");
    g->generation++;
    return 0;
}

int
bwg_add_fd(bwg *g, int fd, int ax_min, int ax_max)
{
    return add_pad(g, fd, NULL, ax_min, ax_max);
}

/* Um gamepad de verdade: botões de gamepad e nenhuma tecla de letra. */
static int
open_pad(bwg *g, const char *node)
{
    if (!node || strncmp(node, "/dev/input/event", 16) != 0)
        return -1;
    for (int i = 0; i < g->npads; i++)
        if (!strcmp(g->pad[i].node, node))
            return 0;
    int fd = open(node, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -1;
    unsigned long keys[NBITS(KEY_MAX + 1)];
    memset(keys, 0, sizeof keys);
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) < 0 ||
        !(TEST_BIT(BTN_SOUTH, keys) || TEST_BIT(BTN_GAMEPAD, keys)) ||
        TEST_BIT(KEY_Q, keys) || TEST_BIT(KEY_A, keys) || TEST_BIT(KEY_Z, keys) || TEST_BIT(KEY_ENTER, keys)) {
        close(fd);
        return -1;
    }
    int idx = g->npads;
    if (add_pad(g, fd, node, -32768, 32767) < 0) {
        close(fd);
        return -1;
    }
    for (int a = 0; a < 2; a++) {
        struct input_absinfo ai;
        if (ioctl(fd, EVIOCGABS(a == 0 ? ABS_X : ABS_Y), &ai) == 0 && ai.maximum > ai.minimum) {
            g->pad[idx].ax_min[a] = ai.minimum;
            g->pad[idx].ax_max[a] = ai.maximum;
        }
    }
    /* O xpad (Xbox) manda o X como BTN_X (= BTN_NORTH) e o Y como BTN_Y
     * (= BTN_WEST); o hid-playstation segue a posição: □ é BTN_WEST e △ é
     * BTN_NORTH. Sem isto, no DualShock/DualSense □ dava espaço e △ apagava. */
    struct input_id id;
    if (ioctl(fd, EVIOCGID, &id) == 0 && id.vendor == 0x054c)
        g->pad[idx].positional = 1;
    return 1;
}

static void
remove_pad(bwg *g, int i)
{
    if (g->pad[i].fd >= 0)
        close(g->pad[i].fd);
    g->pad[i] = g->pad[--g->npads];
    g->generation++;
}

static int
is_pad_device(struct udev_device *d)
{
    const char *js = udev_device_get_property_value(d, "ID_INPUT_JOYSTICK");
    const char *kb = udev_device_get_property_value(d, "ID_INPUT_KEYBOARD");
    return js && !strcmp(js, "1") && !(kb && !strcmp(kb, "1"));
}

int
bwg_open(bwg *g)
{
    memset(g, 0, sizeof *g);
    struct udev *udev = udev_new();
    if (!udev)
        return -1;
    g->udev = udev;
    struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
    if (mon && udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL) == 0 &&
        udev_monitor_enable_receiving(mon) == 0) {
        g->monitor = mon;
    } else if (mon) {
        udev_monitor_unref(mon);
    }
    struct udev_enumerate *en = udev_enumerate_new(udev);
    if (en) {
        udev_enumerate_add_match_subsystem(en, "input");
        udev_enumerate_add_match_property(en, "ID_INPUT_JOYSTICK", "1");
        udev_enumerate_scan_devices(en);
        struct udev_list_entry *e;
        udev_list_entry_foreach(e, udev_enumerate_get_list_entry(en))
        {
            struct udev_device *d = udev_device_new_from_syspath(udev, udev_list_entry_get_name(e));
            if (!d)
                continue;
            if (is_pad_device(d))
                open_pad(g, udev_device_get_devnode(d));
            udev_device_unref(d);
        }
        udev_enumerate_unref(en);
    }
    return g->npads;
}

void
bwg_close(bwg *g)
{
    while (g->npads > 0)
        remove_pad(g, g->npads - 1);
    if (g->monitor)
        udev_monitor_unref(g->monitor);
    if (g->udev)
        udev_unref(g->udev);
    memset(g, 0, sizeof *g);
}

int
bwg_fds(const bwg *g, int *fds, int max)
{
    int n = 0;
    if (g->monitor && n < max)
        fds[n++] = udev_monitor_get_fd(g->monitor);
    for (int i = 0; i < g->npads && n < max; i++)
        fds[n++] = g->pad[i].fd;
    return n;
}

static int
pad_dir(const bwg_pad *p)
{
    for (int d = BWG_UP; d <= BWG_RIGHT; d++)
        if (p->dpad & (1u << d))
            return d;
    if (p->hat_y < 0)
        return BWG_UP;
    if (p->hat_y > 0)
        return BWG_DOWN;
    if (p->hat_x < 0)
        return BWG_LEFT;
    if (p->hat_x > 0)
        return BWG_RIGHT;
    return p->stick;
}

static int
emit(enum bwg_action *out, int n, int max, enum bwg_action a)
{
    if (n < max)
        out[n++] = a;
    return n;
}

/* Depois de um evento: a direção combinada mudou para uma nova? */
static int
update_held(bwg *g, long long now, enum bwg_action *out, int n, int max)
{
    int held = 0;
    for (int i = 0; i < g->npads && !held; i++)
        held = pad_dir(&g->pad[i]);
    if (held != g->held) {
        g->held = held;
        if (held) {
            n = emit(out, n, max, (enum bwg_action)held);
            g->next_repeat = now + REPEAT_FIRST_MS;
        }
    }
    return n;
}

static double
norm(int v, int lo, int hi)
{
    double mid = (lo + hi) / 2.0, half = (hi - lo) / 2.0;
    return half > 0 ? (v - mid) / half : 0;
}

static int
button_action(const bwg_pad *p, int code)
{
    if (p->positional && (code == BTN_NORTH || code == BTN_WEST))
        code = code == BTN_NORTH ? BTN_WEST : BTN_NORTH;
    switch (code) {
    case BTN_SOUTH:
        return BWG_PRESS;
    case BTN_EAST:
        return BWG_BACK;
    case BTN_NORTH: /* o X do Xbox no xpad (BTN_X) */
        return BWG_BACKSPACE;
    case BTN_WEST: /* o Y do Xbox no xpad (BTN_Y) */
        return BWG_SPACE;
    case BTN_TL:
        return BWG_SHIFT;
    case BTN_TR:
        return BWG_LAYOUT;
    case BTN_START:
        return BWG_ENTER;
    case BTN_SELECT:
        return BWG_TOGGLE;
    case BTN_THUMBL:
        return BWG_CAPS;
    default:
        return BWG_NONE;
    }
}

/* histerese: liga a 60 % do curso, solta abaixo de 35 % */
static void
update_stick(bwg_pad *p)
{
    double ax = fabs(p->sx), ay = fabs(p->sy);
    if (p->stick && ax < STICK_OFF && ay < STICK_OFF)
        p->stick = 0;
    else if (ax >= STICK_ON || ay >= STICK_ON)
        p->stick = ax > ay ? (p->sx > 0 ? BWG_RIGHT : BWG_LEFT) : (p->sy > 0 ? BWG_DOWN : BWG_UP);
}

/* Depois de um SYN_DROPPED: o estado vem do dispositivo (EVIOCGKEY,
 * EVIOCGABS); o que não der para ler fica solto, nunca preso. */
static void
resync_pad(bwg_pad *p)
{
    static const int dpad_codes[] = { BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT };
    unsigned long keys[NBITS(KEY_MAX + 1)];
    struct input_absinfo ai;
    p->dpad = 0;
    p->hat_x = p->hat_y = 0;
    p->sx = p->sy = 0;
    p->stick = 0;
    memset(keys, 0, sizeof keys);
    if (ioctl(p->fd, EVIOCGKEY(sizeof keys), keys) >= 0)
        for (int d = 0; d < 4; d++)
            if (TEST_BIT(dpad_codes[d], keys))
                p->dpad |= 1u << (BWG_UP + d);
    if (ioctl(p->fd, EVIOCGABS(ABS_HAT0X), &ai) == 0)
        p->hat_x = ai.value < 0 ? -1 : ai.value > 0;
    if (ioctl(p->fd, EVIOCGABS(ABS_HAT0Y), &ai) == 0)
        p->hat_y = ai.value < 0 ? -1 : ai.value > 0;
    if (ioctl(p->fd, EVIOCGABS(ABS_X), &ai) == 0)
        p->sx = norm(ai.value, p->ax_min[0], p->ax_max[0]);
    if (ioctl(p->fd, EVIOCGABS(ABS_Y), &ai) == 0)
        p->sy = norm(ai.value, p->ax_min[1], p->ax_max[1]);
    update_stick(p);
}

static int
read_pad(bwg *g, int i, long long now, enum bwg_action *out, int n, int max)
{
    bwg_pad *p = &g->pad[i];
    struct input_event ev[64];
    for (;;) {
        ssize_t r = read(p->fd, ev, sizeof ev);
        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0 && errno == EAGAIN)
            break;
        if (r <= 0) { /* desligado (ENODEV) ou fim */
            remove_pad(g, i);
            return update_held(g, now, out, n, max);
        }
        for (size_t k = 0; k < (size_t)r / sizeof ev[0]; k++) {
            const struct input_event *e = &ev[k];
            if (e->type == EV_SYN && e->code == SYN_DROPPED) {
                p->dropping = 1;
                continue;
            }
            if (p->dropping) {
                if (e->type == EV_SYN && e->code == SYN_REPORT) {
                    p->dropping = 0;
                    resync_pad(p);
                    n = update_held(g, now, out, n, max);
                }
                continue;
            }
            if (e->type == EV_KEY) {
                int dir = e->code == BTN_DPAD_UP      ? BWG_UP
                          : e->code == BTN_DPAD_DOWN  ? BWG_DOWN
                          : e->code == BTN_DPAD_LEFT  ? BWG_LEFT
                          : e->code == BTN_DPAD_RIGHT ? BWG_RIGHT
                                                      : 0;
                if (dir) {
                    if (e->value == 1)
                        p->dpad |= 1u << dir;
                    else if (e->value == 0)
                        p->dpad &= ~(1u << dir);
                    n = update_held(g, now, out, n, max);
                } else if (e->value == 1) {
                    int a = button_action(p, e->code);
                    if (a)
                        n = emit(out, n, max, (enum bwg_action)a);
                }
            } else if (e->type == EV_ABS) {
                if (e->code == ABS_HAT0X)
                    p->hat_x = e->value < 0 ? -1 : e->value > 0;
                else if (e->code == ABS_HAT0Y)
                    p->hat_y = e->value < 0 ? -1 : e->value > 0;
                else if (e->code == ABS_X || e->code == ABS_Y) {
                    int a = e->code == ABS_X ? 0 : 1;
                    double v = norm(e->value, p->ax_min[a], p->ax_max[a]);
                    if (a == 0)
                        p->sx = v;
                    else
                        p->sy = v;
                    update_stick(p);
                }
                n = update_held(g, now, out, n, max);
            }
        }
    }
    return n;
}

int
bwg_handle(bwg *g, int fd, long long now, enum bwg_action *out, int max)
{
    int n = 0;
    if (g->monitor && fd == udev_monitor_get_fd(g->monitor)) {
        struct udev_device *d;
        while ((d = udev_monitor_receive_device(g->monitor))) {
            const char *action = udev_device_get_action(d), *node = udev_device_get_devnode(d);
            if (action && node && !strcmp(action, "add") && is_pad_device(d)) {
                open_pad(g, node);
            } else if (action && node && !strcmp(action, "remove")) {
                for (int i = 0; i < g->npads; i++)
                    if (!strcmp(g->pad[i].node, node)) {
                        remove_pad(g, i);
                        break;
                    }
            }
            udev_device_unref(d);
        }
        return update_held(g, now, out, n, max);
    }
    for (int i = 0; i < g->npads; i++)
        if (g->pad[i].fd == fd)
            return read_pad(g, i, now, out, n, max);
    return 0;
}

int
bwg_timeout(const bwg *g, long long now)
{
    if (!g->held)
        return -1;
    long long d = g->next_repeat - now;
    return d < 0 ? 0 : (int)d;
}

int
bwg_tick(bwg *g, long long now, enum bwg_action *out, int max)
{
    if (!g->held || now < g->next_repeat || max < 1)
        return 0;
    out[0] = (enum bwg_action)g->held;
    g->next_repeat = now + REPEAT_NEXT_MS;
    return 1;
}

int
bwg_spatial(const bwg_rect *r, int n, int cur, int dir)
{
    if (n <= 0)
        return cur;
    if (cur < 0 || cur >= n)
        return 0;
    double cx = r[cur].x + r[cur].w / 2, cy = r[cur].y + r[cur].h / 2;
    int best = -1;
    double best_score = 0;
    for (int pass = 0; pass < 2 && best < 0; pass++) {
        for (int i = 0; i < n; i++) {
            if (i == cur)
                continue;
            double x = r[i].x + r[i].w / 2, y = r[i].y + r[i].h / 2;
            double dx = x - cx, dy = y - cy;
            double primary, secondary;
            if (dir == BWG_LEFT || dir == BWG_RIGHT) {
                primary = dir == BWG_RIGHT ? dx : -dx;
                secondary = fabs(dy);
                /* mesma fileira: os retângulos se sobrepõem na vertical */
                if (r[i].y + r[i].h <= r[cur].y || r[i].y >= r[cur].y + r[cur].h)
                    secondary += 1e4;
            } else {
                primary = dir == BWG_DOWN ? dy : -dy;
                secondary = fabs(dx);
            }
            if (pass == 1) /* volta: o mais distante do outro lado, na mesma fileira/coluna */
                primary = -primary;
            if (primary <= 1)
                continue;
            if (pass == 1 && secondary >= 1e4)
                continue;
            double score = pass == 0 ? primary + 2.5 * secondary : -primary + 2.5 * secondary;
            if (best < 0 || score < best_score) {
                best = i;
                best_score = score;
            }
        }
    }
    return best < 0 ? cur : best;
}
