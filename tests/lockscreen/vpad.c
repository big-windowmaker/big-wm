/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * vpad: um controle virtual (uinput) para testar o teclado da tela de
 * bloqueio com um "joystick" sem hardware. Lê comandos da entrada padrão:
 *   press A|B|X|Y|LB|RB|START|SELECT|L3   aperta e solta
 *   hat up|down|left|right                 direcional: aperta e solta
 *   stick X Y                              analógico esquerdo (-32768..32767)
 *   sleep MS
 * O dispositivo some quando a entrada termina. Vendor 0x1209 (pid.codes,
 * IDs de teste): não se passa por um controle de verdade.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static int fd;

static void
emit(int type, int code, int value)
{
    struct input_event e;
    memset(&e, 0, sizeof e);
    e.type = (unsigned short)type;
    e.code = (unsigned short)code;
    e.value = value;
    if (write(fd, &e, sizeof e) != sizeof e)
        perror("write");
}

static void
syn(void)
{
    emit(EV_SYN, SYN_REPORT, 0);
}

static void
msleep(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int
button(const char *n)
{
    static const struct { const char *name; int code; } t[] = {
        { "A", BTN_SOUTH }, { "B", BTN_EAST }, { "X", BTN_NORTH }, { "Y", BTN_WEST },
        { "LB", BTN_TL }, { "RB", BTN_TR }, { "START", BTN_START }, { "SELECT", BTN_SELECT },
        { "L3", BTN_THUMBL }, { "MODE", BTN_MODE },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].name, n))
            return t[i].code;
    return -1;
}

static void
absax(int code, int min, int max)
{
    struct uinput_abs_setup a;
    memset(&a, 0, sizeof a);
    a.code = (unsigned short)code;
    a.absinfo.minimum = min;
    a.absinfo.maximum = max;
    ioctl(fd, UI_SET_ABSBIT, code);
    ioctl(fd, UI_ABS_SETUP, &a);
}

int
main(void)
{
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        perror("/dev/uinput");
        return 2;
    }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    const char *names[] = { "A", "B", "X", "Y", "LB", "RB", "START", "SELECT", "L3", "MODE" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        ioctl(fd, UI_SET_KEYBIT, button(names[i]));
    absax(ABS_X, -32768, 32767);
    absax(ABS_Y, -32768, 32767);
    absax(ABS_RX, -32768, 32767);
    absax(ABS_RY, -32768, 32767);
    absax(ABS_HAT0X, -1, 1);
    absax(ABS_HAT0Y, -1, 1);
    struct uinput_setup us;
    memset(&us, 0, sizeof us);
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x1209;
    us.id.product = 0xb16c;
    snprintf(us.name, sizeof us.name, "Big WM test pad");
    if (ioctl(fd, UI_DEV_SETUP, &us) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("uinput");
        return 2;
    }
    /* o udev classifica o dispositivo e quem o vigia o abre: antes disso,
     * um botão se perderia (um controle de verdade já está ligado) */
    msleep(1000);
    fprintf(stderr, "vpad: pronto\n");
    char line[128];
    while (fgets(line, sizeof line, stdin)) {
        char cmd[16], a1[16] = "", a2[16] = "";
        if (sscanf(line, "%15s %15s %15s", cmd, a1, a2) < 1)
            continue;
        if (!strcmp(cmd, "press") && button(a1) >= 0) {
            emit(EV_KEY, button(a1), 1);
            syn();
            msleep(40);
            emit(EV_KEY, button(a1), 0);
            syn();
        } else if (!strcmp(cmd, "hat")) {
            int x = !strcmp(a1, "left") ? -1 : !strcmp(a1, "right") ? 1 : 0;
            int y = !strcmp(a1, "up") ? -1 : !strcmp(a1, "down") ? 1 : 0;
            emit(EV_ABS, ABS_HAT0X, x);
            emit(EV_ABS, ABS_HAT0Y, y);
            syn();
            msleep(40);
            emit(EV_ABS, ABS_HAT0X, 0);
            emit(EV_ABS, ABS_HAT0Y, 0);
            syn();
        } else if (!strcmp(cmd, "stick")) {
            emit(EV_ABS, ABS_X, atoi(a1));
            emit(EV_ABS, ABS_Y, atoi(a2));
            syn();
        } else if (!strcmp(cmd, "sleep")) {
            msleep(atoi(a1));
            continue;
        }
        msleep(60); /* o tempo de uma pessoa entre dois botões */
    }
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
