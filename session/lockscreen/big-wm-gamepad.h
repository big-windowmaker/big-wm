/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-gamepad: um controle físico (Xbox, DualShock/DualSense, Steam Deck,
 * genéricos USB/Bluetooth) para o teclado na tela da tela de bloqueio.
 *
 * Segurança: lido dentro do próprio processo da tela de bloqueio, sem
 * injeção de eventos nem outro processo. Só dispositivos que o udev marca
 * como joystick (ID_INPUT_JOYSTICK, os que o logind entrega ao usuário do
 * assento ativo pela tag uaccess) e que têm botões de gamepad; qualquer
 * dispositivo com teclas de letras é recusado, para nunca ler um teclado.
 * Nada é gravado nem registrado. Sem polling: descritores para o poll() ou
 * o laço do GLib, mais um prazo para a repetição de uma direção segurada.
 *
 * C puro (libudev e linux/input.h), usado pelo módulo do xsecurelock (X11)
 * e pelo módulo do gtklock (Wayland).
 */
#ifndef BIG_WM_GAMEPAD_H
#define BIG_WM_GAMEPAD_H

enum bwg_action {
    BWG_NONE,
    BWG_UP,
    BWG_DOWN,
    BWG_LEFT,
    BWG_RIGHT,
    BWG_PRESS,     /* A / ✕: aperta a tecla do cursor */
    BWG_BACK,      /* B / ○: fecha o teclado */
    BWG_BACKSPACE, /* X / □: apaga */
    BWG_SPACE,     /* Y / △: espaço */
    BWG_SHIFT,     /* LB / L1 */
    BWG_LAYOUT,    /* RB / R1: ABNT2 / US */
    BWG_ENTER,     /* Start / Options / Menu: desbloquear */
    BWG_TOGGLE,    /* Select / View / Share: mostrar ou ocultar o teclado */
    BWG_CAPS,      /* clique do analógico esquerdo */
};

#define BWG_MAX_PADS 8
#define BWG_MAX_ACTIONS 32

typedef struct {
    int fd;
    int ax_min[2], ax_max[2]; /* ABS_X, ABS_Y */
    double sx, sy;            /* analógico esquerdo, -1..1 */
    int stick;                /* direção do analógico (BWG_UP...), 0 centro */
    int hat_x, hat_y;         /* direcional (ABS_HAT0X/Y): -1, 0, 1 */
    unsigned dpad;            /* botões BTN_DPAD_* apertados: bits 1 << BWG_UP... */
    int positional;           /* BTN_NORTH/BTN_WEST pela posição (hid-playstation) */
    int dropping;             /* SYN_DROPPED: ignora até o SYN_REPORT e relê o estado */
    char node[64];
} bwg_pad;

typedef struct bwg {
    void *udev, *monitor; /* struct udev / udev_monitor (sem expor libudev) */
    bwg_pad pad[BWG_MAX_PADS];
    int npads;
    int held;               /* direção segurada (para a repetição), 0 nenhuma */
    long long next_repeat;  /* ms do relógio monotônico */
    unsigned generation;    /* muda quando um controle entra ou sai */
} bwg;

/* Abre os controles presentes e o monitor do udev. Retorna o número de
 * controles (0 é normal: o monitor avisa quando um for ligado), -1 sem udev. */
int bwg_open(bwg *g);
void bwg_close(bwg *g);

/* Descritores a vigiar (monitor primeiro, depois os controles). */
int bwg_fds(const bwg *g, int *fds, int max);

/* Algo para ler em fd: preenche out com as ações e retorna quantas. */
int bwg_handle(bwg *g, int fd, long long now_ms, enum bwg_action *out, int max);

/* ms até a próxima repetição de uma direção segurada, -1 se nenhuma. */
int bwg_timeout(const bwg *g, long long now_ms);
/* A repetição vencida (no máximo uma ação). */
int bwg_tick(bwg *g, long long now_ms, enum bwg_action *out, int max);

/* Para testes: um descritor que fala o protocolo evdev (sem udev). */
int bwg_add_fd(bwg *g, int fd, int ax_min, int ax_max);

long long bwg_now_ms(void);

/* Navegação espacial: a partir do retângulo cur, o mais próximo na direção
 * dir (BWG_UP...). Sem candidato, dá a volta na mesma fileira ou coluna.
 * Retorna o índice, ou cur. */
typedef struct {
    double x, y, w, h;
} bwg_rect;
int bwg_spatial(const bwg_rect *r, int n, int cur, int dir);

#endif
