/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Sonda do teste de big-wm-locate-pointer (tests/run.sh, com Xvfb).
 *
 *   locate-pointer-probe hold <ms>   põe o cursor left_ptr do tema na raiz
 *                                    e o mantém por <ms>
 *   locate-pointer-probe max <ms>    maior cursor exibido em <ms>: "LxA"
 *   locate-pointer-probe now         cursor exibido agora: "LxA nome"
 */
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void nap(long ms)
{
    struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    Display *d = XOpenDisplay(NULL);
    if (!d) {
        fprintf(stderr, "locate-pointer-probe: sem display %s\n", getenv("DISPLAY") ? getenv("DISPLAY") : "");
        return 1;
    }
    int ev, er, ma = 6, mi = 0;
    if (!XFixesQueryExtension(d, &ev, &er))
        return 1;
    XFixesQueryVersion(d, &ma, &mi);
    long dur = argc > 2 ? atol(argv[2]) : 0;
    if (!strcmp(argv[1], "hold")) {
        Cursor c = XcursorLibraryLoadCursor(d, "left_ptr");
        if (c == None)
            return 1;
        XDefineCursor(d, DefaultRootWindow(d), c);
        XSync(d, False);
        nap(dur);
        return 0;
    }
    if (!strcmp(argv[1], "max")) {
        unsigned w = 0, h = 0;
        long t0 = now_ms();
        while (now_ms() - t0 < dur) {
            XFixesCursorImage *i = XFixesGetCursorImage(d);
            if (i) {
                if (i->height > h) {
                    w = i->width;
                    h = i->height;
                }
                XFree(i);
            }
            nap(8);
        }
        printf("%ux%u\n", w, h);
        return 0;
    }
    XFixesCursorImage *i = XFixesGetCursorImage(d);
    if (!i) {
        fprintf(stderr, "locate-pointer-probe: sem imagem do cursor\n");
        return 1;
    }
    printf("%ux%u %s\n", i->width, i->height, i->name ? i->name : "");
    XFree(i);
    return 0;
}
