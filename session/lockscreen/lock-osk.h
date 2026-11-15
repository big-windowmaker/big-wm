/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * lock-osk: o teclado na tela da tela de bloqueio, o mesmo do Big WM Virtual
 * Keyboard (big-keyboard), usado pelo auth_big_wm (X11, Xlib + cairo) e pelo
 * módulo do gtklock (Wayland, GTK 3).
 *
 * O modelo das teclas é o núcleo do big-keyboard, copiado sem alterações em
 * bk/ (bk-keys: layouts ABNT2, US e US internacional, painel de símbolos,
 * Shift e AltGr de um toque ou travados com LED, Caps, teclas mortas ´ ` ~ ^
 * ¨; bk-geom: geometria, toque e modo apontar). Aqui ficam o desenho (as
 * teclas OPENSTEP em relevo do big-keyboard, em cairo e Pango), o que cada
 * tecla faz num campo de senha e o controle físico (cursor de tecla em
 * tecla pelo direcional e dois cursores apontados pelos analógicos, como no
 * teclado do SteamOS).
 *
 * Teclas que não escrevem numa senha ficam de fora: o painel Fn (F1-F12,
 * navegação, mídia, brilho), Tab, Del, Ctrl, Alt e Super. O resto é o
 * teclado do big-keyboard.
 *
 * Segurança: nada aqui guarda a senha. O texto de cada tecla vai na hora
 * para quem chamou (LockOskSink), que o põe no buffer da senha; a única
 * coisa pendente é uma tecla morta (BkState.dead), apagada por
 * lock_osk_clear() junto com o resto da senha.
 */
#ifndef LOCK_OSK_H
#define LOCK_OSK_H

#include "big-wm-gamepad.h"
#include "bk/bk-geom.h"

#include <cairo.h>

/* Repetição ao segurar uma tecla (os padrões do big-keyboard). */
#define LOCK_OSK_REPEAT_DELAY 500
#define LOCK_OSK_REPEAT_RATE 60
/* Tecla apertada pelo controle: afundada por um instante. */
#define LOCK_OSK_FLASH_MS 90
/* Zona morta dos analógicos no modo apontar. */
#define LOCK_OSK_DEADZONE 0.25

/* Para onde vai o que as teclas escrevem. */
typedef struct {
    void (*text)(void *data, const char *utf8); /* um caractere, já composto */
    void (*backspace)(void *data);
    void (*enter)(void *data); /* conferir a senha */
} LockOskSink;

/* O que mudou (retorno das funções que mexem no teclado). */
enum {
    LOCK_OSK_REDRAW = 1, /* redesenhar */
    LOCK_OSK_HIDE = 2,   /* o teclado fechou (tecla ⌄, B, Select) */
    LOCK_OSK_SHOW = 4,   /* o teclado abriu (um botão do controle) */
};

typedef struct {
    BkState st;
    /* Painel atual, só com as teclas que servem numa senha. */
    BkKey rows[BK_MAX_ROWS][BK_MAX_KEYS];
    BkGeom geom;
    double scale;              /* tamanho (dpi × KeyboardSize) */
    double u, gap, pad, title; /* medidas em pixels */
    double width, height;      /* o teclado inteiro, com a moldura */
    int dark;
    int shown;
    int pressed;  /* tecla apertada pelo mouse ou toque, -1 nenhuma */
    int repeated; /* a tecla apertada já repetiu (não escreve de novo ao soltar) */
    int cursor;   /* cursor do direcional, -1 nenhum */
    int point[2]; /* modo apontar: cursores esquerdo e direito, -1 nenhum */
    int flash;    /* tecla apertada pelo controle, -1 nenhuma */
    long long flash_until;
} LockOsk;

/* Layout pelo valor do KeyboardLayout (br/abnt2, us, us-intl); -1 para
 * "auto" ou desconhecido. */
int lock_osk_layout_from_conf(const char *v);
/* Layout automático: o XKB do sistema ("br" primeiro é ABNT2), senão o
 * idioma (pt), senão US. Qualquer um pode ser NULL. */
int lock_osk_layout_auto(const char *xkb_layout, const char *lang);

void lock_osk_init(LockOsk *k, int layout, int dark, double scale);
/* Apaga o estado (tecla morta pendente, modificadores, cursores). */
void lock_osk_clear(LockOsk *k);

/* Desenha com o canto de cima à esquerda em (x, y); title é o texto da
 * barra de título (já traduzido). */
void lock_osk_draw(LockOsk *k, cairo_t *cr, double x, double y, const char *title, long long now_ms);
/* Tecla em (x, y), relativo ao canto do teclado, ou -1. */
int lock_osk_hit(const LockOsk *k, double x, double y);

/* Mouse ou toque: apertar marca a tecla; soltar na mesma tecla a escreve
 * (se ainda não repetiu). lock_osk_repeat() escreve de novo a tecla
 * segurada (quem chama cuida do relógio: LOCK_OSK_REPEAT_*). */
int lock_osk_down(LockOsk *k, int idx);
int lock_osk_up(LockOsk *k, int idx, const LockOskSink *sink, void *data);
int lock_osk_repeat(LockOsk *k, const LockOskSink *sink, void *data);
/* A tecla segurada repete (caracteres e o apagar)? */
int lock_osk_repeatable(const LockOsk *k);

/* Uma ação do controle físico; abre o teclado fechado (menos com B). */
int lock_osk_pad(LockOsk *k, enum bwg_action a, const LockOskSink *sink, void *data, long long now_ms);
/* Modo apontar: os analógicos do controle mexido por último. */
int lock_osk_sticks(LockOsk *k, const bwg_pad *p);
/* Tecla apertada pelo controle: ms até ela voltar ao normal; 0 uma vez,
 * quando volta (redesenhar); -1 sem nenhuma. */
int lock_osk_flash_ms(LockOsk *k, long long now_ms);

/* Na saída do processo, antes do FcFini(): solta as fontes do Pango. */
void lock_osk_cleanup(void);

#endif
