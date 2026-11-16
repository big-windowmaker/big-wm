/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Desenho do controle na tela, nos estilos de cada layout (bk-ctlpaint.c).
 * Só cairo, Pango e GLib: usado pelo controle na tela, pela prévia das
 * configurações e pela tela de bloqueio do big-wm (cópia em
 * session/lockscreen/bk/), que mostra o controle por cima do bloqueio.
 */
#ifndef BK_CTLPAINT_H
#define BK_CTLPAINT_H

#include "bk-controls.h"

#include <cairo.h>
#include <glib.h>

/* Desenha o ícone do botão do teclado (centro, tamanho, cor). */
typedef void (*BkCtlIconFn)(cairo_t *cr, double cx, double cy, double s, double r, double g, double b, double a);

/* Um controle; now e pressed_at em µs (g_get_monotonic_time; 0: sem animação). */
void bk_ctl_paint(cairo_t *cr, const BkCtlLayout *l, const BkCtlState *s, double w, double h, int i, gint64 now,
                  gint64 pressed_at);
/* A animação de toque ainda está em curso (pede outro quadro)? */
gboolean bk_ctl_paint_animating(const BkCtlLayout *l, gint64 now, gint64 pressed_at);
/* O botão do teclado, no alto ao centro; icon desenha o teclado nele. */
void bk_ctl_paint_switch(cairo_t *cr, const BkCtlLayout *l, double w, double h, gboolean down, BkCtlIconFn icon);
/* O layout inteiro, parado (prévia das configurações); icon NULL: sem o
 * botão do teclado. */
void bk_ctl_paint_layout(cairo_t *cr, const BkCtlLayout *l, const BkCtlState *s, double w, double h, BkCtlIconFn icon);

#endif
