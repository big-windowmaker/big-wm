/* SPDX-License-Identifier: GPL-2.0-or-later
 * Geometria do teclado (veja bk-geom.h). */
#include "bk-geom.h"

#include <math.h>

void
bk_geom_build(BkGeom *g, const BkKey (*rows)[BK_MAX_KEYS], double u, double gap)
{
    g->n = 0;
    g->width = 0;
    int nrows = 0;
    for (int r = 0; r < BK_MAX_ROWS && rows[r][0].width > 0; r++) {
        double w = -gap;
        for (int c = 0; rows[r][c].width > 0; c++)
            w += rows[r][c].width * u + gap;
        if (w > g->width)
            g->width = w;
        nrows++;
    }
    for (int r = 0; r < nrows; r++) {
        double w = -gap;
        for (int c = 0; rows[r][c].width > 0; c++)
            w += rows[r][c].width * u + gap;
        double x = (g->width - w) / 2, y = r * (u + gap);
        for (int c = 0; rows[r][c].width > 0 && g->n < BK_GEOM_MAX; c++) {
            double kw = rows[r][c].width * u;
            g->key[g->n] = &rows[r][c];
            g->rect[g->n] = (BkRect){ round(x), round(y), round(kw), round(u) };
            g->n++;
            x += kw + gap;
        }
    }
    g->height = nrows > 0 ? nrows * u + (nrows - 1) * gap : 0;
}

int
bk_geom_hit(const BkGeom *g, double x, double y)
{
    for (int i = 0; i < g->n; i++) {
        const BkRect *r = &g->rect[i];
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)
            return i;
    }
    return -1;
}

int
bk_geom_point(const BkGeom *g, int side, double sx, double sy)
{
    if (g->n == 0)
        return -1;
    double half = g->width / 2;
    double cx = side ? half + half / 2 : half / 2, cy = g->height / 2;
    double px = cx + sx * half / 2, py = cy + sy * g->height / 2;
    int best = -1;
    double bd = 0;
    for (int i = 0; i < g->n; i++) {
        const BkRect *r = &g->rect[i];
        double kx = r->x + r->w / 2, ky = r->y + r->h / 2;
        /* só as teclas daquela metade (a do meio conta para os dois lados) */
        if (side == 0 && r->x > half)
            continue;
        if (side == 1 && r->x + r->w < half)
            continue;
        /* a distância até o retângulo (teclas largas pegam o ponto inteiro) */
        double dx = px < r->x ? r->x - px : px > r->x + r->w ? px - r->x - r->w : 0;
        double dy = py < r->y ? r->y - py : py > r->y + r->h ? py - r->y - r->h : 0;
        double d = dx * dx + dy * dy + 1e-3 * ((kx - px) * (kx - px) + (ky - py) * (ky - py));
        if (best < 0 || d < bd) {
            best = i;
            bd = d;
        }
    }
    return best;
}

int
bk_geom_find(const BkGeom *g, const BkKey *k)
{
    for (int i = 0; i < g->n; i++)
        if (g->key[i] == k)
            return i;
    return -1;
}
