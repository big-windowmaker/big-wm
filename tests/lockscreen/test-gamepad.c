/* SPDX-License-Identifier: GPL-2.0-or-later
 * big-wm-gamepad sem hardware: um pipe que fala o protocolo evdev. */
#define _GNU_SOURCE
#include "../../session/lockscreen/big-wm-gamepad.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c, ...)                                                                                                   \
    do {                                                                                                                \
        if (!(c)) {                                                                                                     \
            fprintf(stderr, "FALHOU %s:%d: ", __FILE__, __LINE__);                                                      \
            fprintf(stderr, __VA_ARGS__);                                                                               \
            fputc('\n', stderr);                                                                                        \
            fails++;                                                                                                    \
        }                                                                                                               \
    } while (0)

static void
send(int w, int type, int code, int value)
{
    struct input_event e = { .type = (unsigned short)type, .code = (unsigned short)code, .value = value };
    if (write(w, &e, sizeof e) != sizeof e)
        fails++;
}

static int
pump(bwg *g, int r, long long now, enum bwg_action *out)
{
    return bwg_handle(g, r, now, out, BWG_MAX_ACTIONS);
}

int
main(void)
{
    bwg g;
    memset(&g, 0, sizeof g);
    int p[2];
    if (pipe2(p, O_NONBLOCK | O_CLOEXEC) != 0)
        return 2;
    bwg_add_fd(&g, p[0], -32768, 32767);
    enum bwg_action out[BWG_MAX_ACTIONS];

    /* botões: A, X, Start; soltar não gera nada; repetição do kernel ignorada */
    send(p[1], EV_KEY, BTN_SOUTH, 1);
    send(p[1], EV_KEY, BTN_SOUTH, 2);
    send(p[1], EV_KEY, BTN_SOUTH, 0);
    send(p[1], EV_KEY, BTN_NORTH, 1);
    send(p[1], EV_KEY, BTN_START, 1);
    int n = pump(&g, p[0], 0, out);
    CHECK(n == 3 && out[0] == BWG_PRESS && out[1] == BWG_BACKSPACE && out[2] == BWG_ENTER, "botões: n=%d", n);

    /* direcional (hat): uma ação por mudança, depois repetição */
    send(p[1], EV_ABS, ABS_HAT0X, 1);
    n = pump(&g, p[0], 1000, out);
    CHECK(n == 1 && out[0] == BWG_RIGHT, "hat direita: n=%d", n);
    CHECK(bwg_timeout(&g, 1000) == 400, "primeira repetição em 400 ms: %d", bwg_timeout(&g, 1000));
    CHECK(bwg_tick(&g, 1399, out, 4) == 0, "cedo demais");
    CHECK(bwg_tick(&g, 1400, out, 4) == 1 && out[0] == BWG_RIGHT, "repetição");
    CHECK(bwg_timeout(&g, 1400) == 110, "repetições a cada 110 ms");
    send(p[1], EV_ABS, ABS_HAT0X, 0);
    n = pump(&g, p[0], 1450, out);
    CHECK(n == 0 && bwg_timeout(&g, 1450) == -1, "soltar o direcional para a repetição");

    /* analógico: liga a 60 %, não liga a 50 %, solta abaixo de 35 % */
    send(p[1], EV_ABS, ABS_Y, -16384); /* 50 % para cima */
    n = pump(&g, p[0], 2000, out);
    CHECK(n == 0, "50%% não é direção: n=%d", n);
    send(p[1], EV_ABS, ABS_Y, -26000);
    n = pump(&g, p[0], 2010, out);
    CHECK(n == 1 && out[0] == BWG_UP, "analógico para cima: n=%d", n);
    send(p[1], EV_ABS, ABS_Y, -14000); /* 43 %: ainda segurado */
    n = pump(&g, p[0], 2020, out);
    CHECK(n == 0 && g.held == BWG_UP, "histerese");
    send(p[1], EV_ABS, ABS_Y, 0);
    n = pump(&g, p[0], 2030, out);
    CHECK(n == 0 && g.held == 0, "centro solta");

    /* botões do direcional (BTN_DPAD_*), como no Steam Deck e no DualSense */
    send(p[1], EV_KEY, BTN_DPAD_DOWN, 1);
    n = pump(&g, p[0], 3000, out);
    CHECK(n == 1 && out[0] == BWG_DOWN, "dpad para baixo");
    send(p[1], EV_KEY, BTN_DPAD_DOWN, 0);
    pump(&g, p[0], 3010, out);

    /* dois botões do direcional: soltar um volta ao que ainda está apertado */
    send(p[1], EV_KEY, BTN_DPAD_UP, 1);
    send(p[1], EV_KEY, BTN_DPAD_RIGHT, 1);
    pump(&g, p[0], 3100, out);
    send(p[1], EV_KEY, BTN_DPAD_RIGHT, 0);
    pump(&g, p[0], 3110, out);
    CHECK(g.held == BWG_UP, "soltar a direita mantém o para cima: held=%d", g.held);
    send(p[1], EV_KEY, BTN_DPAD_UP, 0);
    pump(&g, p[0], 3120, out);
    CHECK(g.held == 0, "soltar tudo: held=%d", g.held);

    /* SYN_DROPPED: o que chega até o SYN_REPORT é descartado e o estado é
     * relido do dispositivo (num pipe, nada fica preso) */
    send(p[1], EV_KEY, BTN_DPAD_LEFT, 1);
    pump(&g, p[0], 3200, out);
    send(p[1], EV_SYN, SYN_DROPPED, 0);
    send(p[1], EV_KEY, BTN_SOUTH, 1); /* descartado */
    send(p[1], EV_SYN, SYN_REPORT, 0);
    n = pump(&g, p[0], 3210, out);
    CHECK(n == 0 && g.held == 0, "SYN_DROPPED não deixa direção presa: n=%d held=%d", n, g.held);
    send(p[1], EV_KEY, BTN_SOUTH, 1);
    n = pump(&g, p[0], 3220, out);
    CHECK(n == 1 && out[0] == BWG_PRESS, "depois do SYN_REPORT, os eventos voltam: n=%d", n);

    /* DualShock/DualSense (hid-playstation): □ é BTN_WEST e apaga, △ é
     * BTN_NORTH e dá espaço, como o X e o Y do Xbox */
    g.pad[0].positional = 1;
    send(p[1], EV_KEY, BTN_WEST, 1);
    send(p[1], EV_KEY, BTN_NORTH, 1);
    n = pump(&g, p[0], 3300, out);
    CHECK(n == 2 && out[0] == BWG_BACKSPACE && out[1] == BWG_SPACE, "□ apaga, △ espaço: n=%d", n);
    g.pad[0].positional = 0;

    /* o controle desligado sai da lista */
    close(p[1]);
    n = pump(&g, p[0], 4000, out);
    CHECK(g.npads == 0, "controle desligado: npads=%d", g.npads);

    /* navegação espacial: duas fileiras desalinhadas */
    bwg_rect r[] = {
        { 0, 0, 10, 10 }, { 12, 0, 10, 10 }, { 24, 0, 10, 10 }, /* fileira 0 */
        { 5, 12, 10, 10 }, { 17, 12, 20, 10 },                   /* fileira 1 */
    };
    CHECK(bwg_spatial(r, 5, 0, BWG_RIGHT) == 1, "direita");
    CHECK(bwg_spatial(r, 5, 2, BWG_RIGHT) == 0, "volta ao início da fileira");
    CHECK(bwg_spatial(r, 5, 0, BWG_LEFT) == 2, "volta ao fim da fileira");
    CHECK(bwg_spatial(r, 5, 1, BWG_DOWN) == 3 || bwg_spatial(r, 5, 1, BWG_DOWN) == 4, "para baixo");
    CHECK(bwg_spatial(r, 5, 2, BWG_DOWN) == 4, "para baixo, a mais próxima");
    CHECK(bwg_spatial(r, 5, 4, BWG_UP) == 2 || bwg_spatial(r, 5, 4, BWG_UP) == 1, "para cima");
    CHECK(bwg_spatial(r, 5, -1, BWG_RIGHT) == 0, "sem cursor: a primeira");

    if (!fails)
        printf("big-wm-gamepad: todos os casos passaram\n");
    return fails ? 1 : 0;
}
