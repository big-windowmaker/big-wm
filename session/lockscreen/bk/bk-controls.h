/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Controle na tela (touchscreen): o layout dos controles e o que cada toque
 * faz, sem interface (testado em tests/test-controls.c).
 *
 * Posições e tamanhos são frações da tela (x, y do centro em 0..1; tamanho
 * em fração da altura), para o mesmo layout servir em qualquer resolução e
 * orientação. Cada dedo fica preso ao controle em que começou: escorregar
 * sobre outro controle não o aciona, e tirar o dedo (ou um cancelamento do
 * toque) solta o que ele segurava. As saídas vão para um BkCtlSink (o
 * controle virtual uinput, ou um registro nos testes).
 */
#ifndef BK_CONTROLS_H
#define BK_CONTROLS_H

#include <glib.h>
#include <stdbool.h>

typedef enum { BKC_STICK, BKC_DPAD, BKC_BUTTON, BKC_TRIGGER } BkCtlKind;

typedef struct {
    BkCtlKind kind;
    int code;          /* BKC_BUTTON: BTN_*; BKC_STICK/BKC_TRIGGER: lado 0/1 */
    double x, y, size; /* centro (fração da tela) e diâmetro (fração da altura) */
    double deadzone;   /* analógico/direcional: 0..0.8 */
    double sens;       /* analógico: 0.3..3 */
    bool enabled;
} BkCtl;

#define BKC_MAX 32

/* O botão Guia (Xbox, PS, Home): no alto ao centro, logo abaixo do botão do
 * teclado e entre Select e Start, um pouco maior que eles. */
#define BKC_GUIDE_X 0.50
#define BKC_GUIDE_Y 0.19
#define BKC_GUIDE_SIZE 0.09
#define BKC_TOUCHES 16

/* Aparência, gravada com o layout (um layout "PlayStation" já vem com
 * ✕ ○ □ △). Os valores são palavras fixas; um valor desconhecido no arquivo
 * vira o padrão, que é o visual de sempre (classic, xbox, sink). */
typedef struct {
    char style[12];   /* classic (OPENSTEP, relevo cinza), modern (vidro escuro), retro (16 bits, cores
                         fortes e contorno grosso), neon (contorno luminoso), minimal (só contorno) */
    char buttons[12]; /* xbox (A B X Y), playstation (✕ ○ □ △), nintendo (B A Y X, nas mesmas posições) */
    char accent[10];  /* blue, red, green, purple, orange, gray: destaque de toques e contornos */
    char stick[10];   /* fixed; floating: o centro do analógico vai aonde o dedo encosta */
    char dpad[10];    /* cross (cruz), buttons (quatro setas), disc (disco) */
    char anim[10];    /* none, sink (afunda), glow (brilho), ripple (onda), bounce (encolhe) */
} BkCtlLook;

typedef struct {
    char name[64];
    double opacity, scale;
    BkCtlLook look;
    BkCtl c[BKC_MAX];
    int n;
} BkCtlLayout;

typedef struct {
    void (*button)(void *d, int code, bool down);
    void (*stick)(void *d, int side, double x, double y);
    void (*trigger)(void *d, int side, double v);
    void (*hat)(void *d, int x, int y);
    void (*sync)(void *d);
    void *data;
} BkCtlSink;

typedef struct {
    const void *seq[BKC_TOUCHES]; /* identidade do toque (GdkEventSequence) */
    int ctl[BKC_TOUCHES];         /* controle preso a cada toque; -1 livre */
    bool used[BKC_TOUCHES];
    double px[BKC_TOUCHES], py[BKC_TOUCHES]; /* onde o dedo está (pixels) */
    double ox[BKC_TOUCHES], oy[BKC_TOUCHES]; /* centro do analógico deste toque (flutuante: onde encostou) */
} BkCtlState;

typedef enum { BKC_BEGIN, BKC_MOVE, BKC_END, BKC_CANCEL } BkCtlPhase;

void bk_ctl_default(BkCtlLayout *l);
/* Diâmetro de um controle em pixels numa tela w×h. */
double bk_ctl_px(const BkCtlLayout *l, const BkCtl *c, double h);
/* O controle sob (px, py), ou -1. */
int bk_ctl_hit(const BkCtlLayout *l, double w, double h, double px, double py);
/* No editor: os desligados também (um Guia que a migração desligou por
 * estar num lugar ocupado se liga ao ser tocado). */
int bk_ctl_hit_edit(const BkCtlLayout *l, double w, double h, double px, double py);
void bk_ctl_state_init(BkCtlState *s);
/* Um evento de toque; px/py em pixels da tela w×h. */
void bk_ctl_touch(BkCtlState *s, const BkCtlLayout *l, double w, double h, const void *seq, BkCtlPhase phase,
                  double px, double py, const BkCtlSink *sink);
/* Solta tudo o que os toques seguravam. */
void bk_ctl_release(BkCtlState *s, const BkCtlLayout *l, const BkCtlSink *sink);
/* Mantém o controle inteiro dentro da tela; TRUE se precisou mexer. */
bool bk_ctl_clamp(BkCtl *c, double w, double h, double scale);
const char *bk_ctl_label(const BkCtl *c);

/* ---- aparência ---- */
void bk_ctl_look_default(BkCtlLook *k);
/* O rótulo na família de botões do layout (A/✕/B, LB/L1/L, LT/L2/ZL...). */
const char *bk_ctl_label_for(const BkCtlLayout *l, const BkCtl *c);
/* Cor de marca de um botão de ação (verde do A, azul do ✕...); FALSE: sem cor. */
bool bk_ctl_color(const BkCtlLayout *l, const BkCtl *c, double rgb[3]);
void bk_ctl_accent(const BkCtlLayout *l, double rgb[3]);
/* O toque que segura o controle ci, ou -1. */
int bk_ctl_touch_of(const BkCtlState *s, int ci);
/* Para desenhar: centro da base do analógico (pixels; o flutuante segue o
 * dedo que encostou) e o pino, de -1 a 1 em cada eixo (dentro do círculo). */
void bk_ctl_stick_visual(const BkCtlLayout *l, const BkCtlState *s, double w, double h, int ci, double *cx,
                         double *cy, double *kx, double *ky);
/* A direção apertada do direcional (cada eixo -1, 0 ou 1). */
void bk_ctl_dpad_visual(const BkCtlLayout *l, const BkCtlState *s, double w, double h, int ci, int *hx, int *hy);

/* ---- modelos ---- */
typedef struct {
    const char *id, *name, *description;
} BkCtlTemplate;
/* Modelos prontos (Xbox, PlayStation, Nintendo Switch, retrô, arcade,
 * corrida, mínimo); a lista termina com id NULL. */
const BkCtlTemplate *bk_ctl_templates(void);
/* Preenche o layout com o modelo (controles e aparência); FALSE: id desconhecido. */
bool bk_ctl_template(BkCtlLayout *l, const char *id);

/* O botão que troca o controle pelo teclado: fixo no alto, ao centro (entre
 * os ombros e acima de Select/Start no layout padrão), fora do layout para
 * nenhum layout salvo o perder. Centro e raio em pixels numa tela w×h. */
void bk_ctl_switch_button(double w, double h, double *cx, double *cy, double *r);
bool bk_ctl_switch_hit(double w, double h, double px, double py);

gboolean bk_ctl_save(const BkCtlLayout *l, const char *path, GError **err);
gboolean bk_ctl_load(BkCtlLayout *l, const char *path, GError **err);
/* ~/.local/share/big-keyboard/gamepad-layouts/<nome>.ini */
char *bk_ctl_path(const char *name);

#endif
