/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Big WM Virtual Keyboard: o modelo de teclas, sem interface nem backend.
 *
 * Um layout é uma lista de fileiras de teclas. Uma tecla de caractere tem
 * até três níveis (normal, Shift, AltGr), cada um com o texto que produz;
 * teclas mortas compõem com a próxima. Teclas especiais têm o nome do
 * keysym (Return, BackSpace, F1...). Modificadores: um toque vale para a
 * próxima tecla, dois toques travam, o terceiro solta; Caps é persistente.
 *
 * bk_state_press() transforma o toque numa tecla em eventos de saída (texto
 * Unicode ou keysym com modificadores), que o backend (XTest no X11,
 * virtual-keyboard no Wayland) entrega ao aplicativo em foco. Tudo aqui é C
 * puro e testado sem X11 nem Wayland (tests/test-keys.c).
 */
#ifndef BK_KEYS_H
#define BK_KEYS_H

#include <stdbool.h>

typedef enum {
    BK_MOD_SHIFT = 1 << 0,
    BK_MOD_CTRL = 1 << 1,
    BK_MOD_ALT = 1 << 2,
    BK_MOD_ALTGR = 1 << 3,
    BK_MOD_SUPER = 1 << 4,
} BkMod;

typedef enum {
    BK_KEY_CHAR,    /* produz texto (levels) */
    BK_KEY_SPECIAL, /* keysym com nome (Return, Tab, Left, F1...) */
    BK_KEY_MOD,     /* Shift, Ctrl, Alt, AltGr, Super */
    BK_KEY_CAPS,
    BK_KEY_PANEL, /* troca o painel (principal, Fn, emoji/símbolos) */
    BK_KEY_LAYOUT,
    BK_KEY_HIDE,
} BkKeyKind;

typedef enum { BK_PANEL_MAIN, BK_PANEL_FN, BK_PANEL_SYMBOLS, BK_PANEL_N } BkPanel;

typedef struct {
    BkKeyKind kind;
    const char *level[3]; /* texto em cada nível; NULL repete o normal */
    unsigned dead;        /* bit n: o nível n é tecla morta */
    const char *keysym;   /* BK_KEY_SPECIAL: nome do keysym; rótulo em level[0] */
    unsigned mod;         /* BK_KEY_MOD */
    int panel;            /* BK_KEY_PANEL */
    float width;          /* em unidades de tecla; 0 termina a fileira */
} BkKey;

#define BK_MAX_ROWS 6
#define BK_MAX_KEYS 16

typedef struct {
    const char *id;   /* br, us, us-intl */
    const char *name; /* rótulo da tecla de layout */
    BkKey rows[BK_MAX_ROWS][BK_MAX_KEYS];
} BkLayout;

int bk_layout_count(void);
const BkLayout *bk_layout_get(int i);
const BkLayout *bk_layout_by_id(const char *id);
/* As fileiras de um painel (o principal vem do layout). */
const BkKey (*bk_panel_rows(const BkLayout *l, BkPanel panel))[BK_MAX_KEYS];

/* ---- estado e saída ------------------------------------------------------ */

typedef enum { BK_OUT_TEXT, BK_OUT_KEY } BkOutKind;

typedef struct {
    BkOutKind kind;
    char text[16];      /* BK_OUT_TEXT: um caractere UTF-8 (já composto) */
    char keysym[32];    /* BK_OUT_KEY: nome do keysym (a, Return, F5...) */
    unsigned mods;      /* BK_OUT_KEY: modificadores a segurar */
} BkOut;

#define BK_MAX_OUT 4

typedef struct {
    unsigned latched; /* valem para a próxima tecla */
    unsigned locked;  /* travados (dois toques) */
    bool caps;
    char dead[8]; /* tecla morta pendente */
    int layout;   /* índice em bk_layout_get */
    BkPanel panel;
} BkState;

void bk_state_init(BkState *s, int layout);
/* Toque numa tecla: preenche out e retorna quantos eventos. */
int bk_state_press(BkState *s, const BkKey *k, BkOut *out, int max);
/* Modificadores ativos agora (travados ou para a próxima tecla). */
unsigned bk_state_mods(const BkState *s);
/* O texto que a tecla mostra agora (nível atual). */
const char *bk_state_label(const BkState *s, const BkKey *k);
/* Solta tudo (modificadores, tecla morta): troca de modo, emergência. */
void bk_state_release_all(BkState *s);

/* ´`~^¨ + caractere: o composto, ou NULL. */
const char *bk_compose(const char *dead, const char *c);

#endif
