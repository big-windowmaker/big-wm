/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * zoom-probe — ajuda do tests/run.sh para a ampliação da tela no X11
 * (big-wm-picom-zoom.so):
 *
 *   zoom-probe show        cobre a tela com uma janela de listras cuja cor
 *                          diz a posição (vermelho = x/4, verde = y/4) e
 *                          espera ser morta
 *   zoom-probe pixel X Y   imprime "R G B" do pixel da tela em (X, Y) (o que
 *                          o compositor mostra: GetImage da raiz)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

int main(int argc, char **argv)
{
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c)) {
        fprintf(stderr, "zoom-probe: sem X\n");
        return 1;
    }
    xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    if (argc == 2 && !strcmp(argv[1], "show")) {
        uint16_t w = s->width_in_pixels, h = s->height_in_pixels;
        xcb_pixmap_t pm = xcb_generate_id(c);
        xcb_create_pixmap(c, s->root_depth, pm, s->root, w, h);
        xcb_gcontext_t gc = xcb_generate_id(c);
        xcb_create_gc(c, gc, pm, 0, NULL);
        /* Quadrados de 4x4: a cor codifica a posição. */
        for (int y = 0; y < h; y += 4)
            for (int x = 0; x < w; x += 4) {
                uint32_t pixel = ((uint32_t)(x / 4 % 256) << 16) |
                                 ((uint32_t)(y / 4 % 256) << 8) | 0x80;
                xcb_change_gc(c, gc, XCB_GC_FOREGROUND, &pixel);
                xcb_rectangle_t r = {(int16_t)x, (int16_t)y, 4, 4};
                xcb_poly_fill_rectangle(c, pm, gc, 1, &r);
            }
        xcb_window_t win = xcb_generate_id(c);
        uint32_t values[] = {pm, 1};
        xcb_create_window(c, XCB_COPY_FROM_PARENT, win, s->root, 0, 0, w, h, 0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual,
                          XCB_CW_BACK_PIXMAP | XCB_CW_OVERRIDE_REDIRECT, values);
        xcb_map_window(c, win);
        xcb_flush(c);
        pause();
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "pixel")) {
        int16_t x = (int16_t)atoi(argv[2]), y = (int16_t)atoi(argv[3]);
        xcb_get_image_reply_t *r = xcb_get_image_reply(
            c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, s->root, x, y, 1, 1, ~0u), NULL);
        if (!r || xcb_get_image_data_length(r) < 4) {
            fprintf(stderr, "zoom-probe: GetImage falhou\n");
            return 1;
        }
        uint8_t *d = xcb_get_image_data(r);
        printf("%d %d %d\n", d[2], d[1], d[0]);
        free(r);
        return 0;
    }
    fprintf(stderr, "uso: zoom-probe show | pixel X Y\n");
    return 2;
}
