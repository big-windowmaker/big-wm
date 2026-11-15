/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Geometria do teclado: o retângulo de cada tecla do painel atual (fileiras
 * centradas), usado pelo desenho, pelos cliques/toques e pela navegação por
 * controle. A navegação tem dois modos:
 *   cursor   direcional/analógico movem um cursor de tecla em tecla
 *            (bwg_spatial), como no teclado da tela de bloqueio;
 *   apontar  cada analógico aponta numa metade do teclado (como os
 *            trackpads do teclado do SteamOS): a tecla mais próxima do ponto
 *            vira o cursor daquele lado, e o gatilho do lado a aperta.
 */
#ifndef BK_GEOM_H
#define BK_GEOM_H

#include "bk-keys.h"

#define BK_GEOM_MAX (BK_MAX_ROWS * BK_MAX_KEYS)

typedef struct {
    double x, y, w, h;
} BkRect;

typedef struct {
    const BkKey *key[BK_GEOM_MAX];
    BkRect rect[BK_GEOM_MAX];
    int n;
    double width, height; /* das fileiras, sem margens */
} BkGeom;

/* u: lado de uma tecla de largura 1; gap: espaço entre teclas. */
void bk_geom_build(BkGeom *g, const BkKey (*rows)[BK_MAX_KEYS], double u, double gap);
/* Índice da tecla em (x, y), ou -1. */
int bk_geom_hit(const BkGeom *g, double x, double y);
/* Modo apontar: lado 0 (esquerda) ou 1 (direita), analógico em -1..1. */
int bk_geom_point(const BkGeom *g, int side, double sx, double sy);
/* Índice de uma tecla pelo ponteiro (o mesmo BkKey), ou -1. */
int bk_geom_find(const BkGeom *g, const BkKey *k);

#endif
