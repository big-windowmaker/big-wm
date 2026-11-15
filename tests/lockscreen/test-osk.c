/* SPDX-License-Identifier: GPL-2.0-or-later
 * lock-osk sem tela: o teclado do big-keyboard na tela de bloqueio escreve
 * no "campo de senha" (um buffer de teste) só o que um campo de senha
 * aceita. */
#define _GNU_SOURCE
#include "../../session/lockscreen/lock-osk.h"

#include <stdio.h>
#include <string.h>

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

typedef struct {
    char buf[256];
    int enters;
} field;

static void
on_text(void *d, const char *t)
{
    field *f = d;
    strncat(f->buf, t, sizeof f->buf - strlen(f->buf) - 1);
}

static void
on_backspace(void *d)
{
    field *f = d;
    size_t n = strlen(f->buf);
    while (n > 0 && ((unsigned char)f->buf[n - 1] & 0xc0) == 0x80)
        f->buf[--n] = 0;
    if (n > 0)
        f->buf[--n] = 0;
}

static void
on_enter(void *d)
{
    ((field *)d)->enters++;
}

static const LockOskSink sink = { on_text, on_backspace, on_enter };

/* Índice da tecla com este rótulo (nível normal) ou keysym. */
static int
key(const LockOsk *k, const char *label)
{
    for (int i = 0; i < k->geom.n; i++) {
        const BkKey *b = k->geom.key[i];
        if ((b->level[0] && !strcmp(b->level[0], label)) || (b->keysym && !strcmp(b->keysym, label)))
            return i;
    }
    return -1;
}

static int
kind(const LockOsk *k, BkKeyKind kd, unsigned mod)
{
    for (int i = 0; i < k->geom.n; i++)
        if (k->geom.key[i]->kind == kd && (!mod || k->geom.key[i]->mod == mod))
            return i;
    return -1;
}

/* Um clique (apertar e soltar na mesma tecla). */
static int
tap(LockOsk *k, int idx, field *f)
{
    lock_osk_down(k, idx);
    return lock_osk_up(k, idx, &sink, f);
}

static void
type(LockOsk *k, const char *labels, field *f)
{
    char one[8];
    for (const char *p = labels; *p; p++) {
        one[0] = *p;
        one[1] = 0;
        int i = key(k, one);
        CHECK(i >= 0, "tecla %s", one);
        if (i >= 0)
            tap(k, i, f);
    }
}

int
main(void)
{
    LockOsk k;
    field f = { "", 0 };
    int br = lock_osk_layout_from_conf("br"), us = lock_osk_layout_from_conf("us");
    CHECK(br >= 0 && us >= 0 && br != us, "layouts br e us");
    CHECK(lock_osk_layout_from_conf("us-intl") >= 0, "layout us-intl");
    CHECK(lock_osk_layout_from_conf("auto") == -1 && lock_osk_layout_from_conf("xx") == -1, "auto/desconhecido");
    CHECK(lock_osk_layout_auto("br,us", NULL) == br, "XKB br primeiro: ABNT2");
    CHECK(lock_osk_layout_auto("us,br", "pt_BR.UTF-8") == us, "XKB us primeiro: US");
    CHECK(lock_osk_layout_auto(NULL, "pt_BR.UTF-8") == br && lock_osk_layout_auto("", "C") == us, "pelo idioma");

    lock_osk_init(&k, br, 0, 1);
    /* Só teclas de senha: nada de Tab, Del, Ctrl, Alt, Super nem Fn. */
    CHECK(key(&k, "Tab") < 0 && key(&k, "Delete") < 0, "sem Tab e Del");
    CHECK(kind(&k, BK_KEY_MOD, BK_MOD_CTRL) < 0 && kind(&k, BK_KEY_MOD, BK_MOD_ALT) < 0 &&
              kind(&k, BK_KEY_MOD, BK_MOD_SUPER) < 0,
          "sem Ctrl, Alt e Super");
    for (int i = 0; i < k.geom.n; i++)
        CHECK(!(k.geom.key[i]->kind == BK_KEY_PANEL && k.geom.key[i]->panel == BK_PANEL_FN), "sem o painel Fn");
    CHECK(kind(&k, BK_KEY_MOD, BK_MOD_SHIFT) >= 0 && kind(&k, BK_KEY_MOD, BK_MOD_ALTGR) >= 0 &&
              kind(&k, BK_KEY_CAPS, 0) >= 0 && kind(&k, BK_KEY_LAYOUT, 0) >= 0 && kind(&k, BK_KEY_HIDE, 0) >= 0 &&
              key(&k, "BackSpace") >= 0 && key(&k, "Return") >= 0 && key(&k, " ") >= 0,
          "Shift, AltGr, Caps, layout, ocultar, apagar, Enter e espaço");
    CHECK(k.width > 0 && k.height > 0, "medidas");

    /* letras, Shift de um toque, tecla morta, AltGr */
    type(&k, "abc", &f);
    int shift = kind(&k, BK_KEY_MOD, BK_MOD_SHIFT);
    tap(&k, shift, &f);
    type(&k, "dd", &f);
    CHECK(!strcmp(f.buf, "abcDd"), "Shift vale para uma tecla: %s", f.buf);
    tap(&k, key(&k, "´"), &f);
    CHECK(!strcmp(f.buf, "abcDd") && k.st.dead[0], "tecla morta pendente não escreve");
    type(&k, "a", &f);
    CHECK(!strcmp(f.buf, "abcDdá"), "´ + a = á: %s", f.buf);
    tap(&k, key(&k, "~"), &f);
    tap(&k, key(&k, " "), &f);
    CHECK(!strcmp(f.buf, "abcDdá~"), "~ + espaço = ~: %s", f.buf);
    tap(&k, kind(&k, BK_KEY_MOD, BK_MOD_ALTGR), &f);
    type(&k, "e", &f);
    CHECK(!strcmp(f.buf, "abcDdá~€"), "AltGr + e = €: %s", f.buf);
    /* Shift travado (dois toques, LED) e solto no terceiro */
    tap(&k, shift, &f);
    tap(&k, shift, &f);
    CHECK(k.st.locked & BK_MOD_SHIFT, "Shift travado");
    type(&k, "1", &f);
    CHECK(k.st.locked & BK_MOD_SHIFT, "continua travado");
    tap(&k, shift, &f);
    CHECK(!(bk_state_mods(&k.st) & BK_MOD_SHIFT), "terceiro toque solta");
    /* Caps só nas letras */
    tap(&k, kind(&k, BK_KEY_CAPS, 0), &f);
    type(&k, "q1", &f);
    tap(&k, kind(&k, BK_KEY_CAPS, 0), &f);
    CHECK(!strcmp(f.buf, "abcDdá~€!Q1"), "Caps: %s", f.buf);
    /* apagar (UTF-8 inteiro) e Enter */
    tap(&k, key(&k, "BackSpace"), &f);
    tap(&k, key(&k, "BackSpace"), &f);
    CHECK(!strcmp(f.buf, "abcDdá~€!"), "apagar: %s", f.buf);
    tap(&k, key(&k, "Return"), &f);
    CHECK(f.enters == 1, "Enter confere a senha");
    /* soltar fora da tecla não escreve */
    lock_osk_down(&k, key(&k, "z"));
    lock_osk_up(&k, key(&k, "x"), &sink, &f);
    CHECK(!strcmp(f.buf, "abcDdá~€!"), "soltar em outra tecla: %s", f.buf);
    /* repetição: a tecla segurada repete e não escreve de novo ao soltar */
    int z = key(&k, "z");
    lock_osk_down(&k, z);
    CHECK(lock_osk_repeatable(&k), "letra repete");
    lock_osk_repeat(&k, &sink, &f);
    lock_osk_repeat(&k, &sink, &f);
    lock_osk_up(&k, z, &sink, &f);
    CHECK(!strcmp(f.buf, "abcDdá~€!zz"), "repetição: %s", f.buf);
    lock_osk_down(&k, kind(&k, BK_KEY_CAPS, 0));
    CHECK(!lock_osk_repeatable(&k), "Caps não repete");
    lock_osk_up(&k, -1, &sink, &f);
    /* layouts: BR → US → INTL → BR */
    int lay = kind(&k, BK_KEY_LAYOUT, 0);
    tap(&k, lay, &f);
    CHECK(k.st.layout == us && key(&k, "ç") < 0, "US depois do BR");
    tap(&k, kind(&k, BK_KEY_LAYOUT, 0), &f);
    CHECK(!strcmp(bk_layout_get(k.st.layout)->id, "us-intl"), "INTL depois do US");
    tap(&k, kind(&k, BK_KEY_LAYOUT, 0), &f);
    CHECK(k.st.layout == br, "volta ao BR");
    /* símbolos e emojis */
    int sym = -1;
    for (int i = 0; i < k.geom.n; i++)
        if (k.geom.key[i]->kind == BK_KEY_PANEL && k.geom.key[i]->panel == BK_PANEL_SYMBOLS)
            sym = i;
    CHECK(sym >= 0, "tecla do painel de símbolos");
    tap(&k, sym, &f);
    CHECK(k.st.panel == BK_PANEL_SYMBOLS && key(&k, "€") >= 0, "painel de símbolos");
    f.buf[0] = 0;
    tap(&k, key(&k, "😀"), &f);
    CHECK(!strcmp(f.buf, "😀"), "emoji: %s", f.buf);
    for (int i = 0; i < k.geom.n; i++)
        if (k.geom.key[i]->kind == BK_KEY_PANEL && k.geom.key[i]->panel == BK_PANEL_MAIN)
            sym = i;
    tap(&k, sym, &f);
    CHECK(k.st.panel == BK_PANEL_MAIN, "volta às letras");
    /* ocultar */
    k.shown = 1;
    CHECK(tap(&k, kind(&k, BK_KEY_HIDE, 0), &f) & LOCK_OSK_HIDE && !k.shown, "⌄ fecha o teclado");
    /* a tecla morta pendente é parte da senha: lock_osk_clear apaga */
    tap(&k, key(&k, "´"), &f);
    CHECK(k.st.dead[0], "morta pendente");
    lock_osk_clear(&k);
    CHECK(!k.st.dead[0] && !bk_state_mods(&k.st), "clear apaga a morta e os modificadores");
    /* acerto do toque: o centro da primeira tecla */
    const BkRect *r0 = &k.geom.rect[0];
    CHECK(lock_osk_hit(&k, k.pad + r0->x + r0->w / 2, k.title + k.pad + r0->y + r0->h / 2) == 0, "toque na 1.ª tecla");
    CHECK(lock_osk_hit(&k, 1, 1) == -1, "toque na barra de título");

    /* controle físico: fechado, o primeiro botão abre com o cursor na 1.ª */
    f.buf[0] = 0;
    k.cursor = -1;
    CHECK(lock_osk_pad(&k, BWG_BACK, &sink, &f, 0) == 0 && !k.shown, "B não abre");
    CHECK(lock_osk_pad(&k, BWG_PRESS, &sink, &f, 0) & LOCK_OSK_SHOW && k.shown && k.cursor == 0, "A abre");
    lock_osk_pad(&k, BWG_PRESS, &sink, &f, 0);
    CHECK(!strcmp(f.buf, "'"), "A aperta a tecla do cursor: %s", f.buf);
    CHECK(lock_osk_flash_ms(&k, 10) == LOCK_OSK_FLASH_MS - 10, "tecla do controle afundada");
    CHECK(lock_osk_flash_ms(&k, 1000) == 0 && lock_osk_flash_ms(&k, 1001) == -1, "e volta");
    lock_osk_pad(&k, BWG_DOWN, &sink, &f, 0);
    CHECK(k.cursor == key(&k, "q"), "direcional desce para a fileira de baixo (q): %d", k.cursor);
    lock_osk_pad(&k, BWG_RIGHT, &sink, &f, 0);
    CHECK(k.cursor == key(&k, "w"), "direita: w");
    lock_osk_pad(&k, BWG_UP, &sink, &f, 0);
    CHECK(k.geom.rect[k.cursor].y == k.geom.rect[0].y, "sobe para a primeira fileira");
    lock_osk_pad(&k, BWG_BACKSPACE, &sink, &f, 0);
    lock_osk_pad(&k, BWG_SPACE, &sink, &f, 0);
    lock_osk_pad(&k, BWG_SHIFT, &sink, &f, 0);
    CHECK(!strcmp(f.buf, " ") && (k.st.latched & BK_MOD_SHIFT), "X apaga, Y espaço, LB Shift: %s", f.buf);
    lock_osk_pad(&k, BWG_ENTER, &sink, &f, 0);
    CHECK(f.enters == 2, "Start desbloqueia");
    /* modo apontar: os analógicos escolhem uma tecla de cada lado, os
     * gatilhos apertam */
    bwg_pad p;
    memset(&p, 0, sizeof p);
    p.sx = -0.9;
    p.sy = -0.9; /* esquerdo no canto de cima à esquerda */
    p.rx = 0.9;
    p.ry = 0.9; /* direito no de baixo à direita */
    CHECK(lock_osk_sticks(&k, &p) & LOCK_OSK_REDRAW, "apontar");
    CHECK(k.point[0] >= 0 && k.point[1] >= 0 && k.geom.rect[k.point[0]].x < k.geom.rect[k.point[1]].x,
          "dois cursores, um de cada lado");
    f.buf[0] = 0;
    int left = k.point[0];
    lock_osk_pad(&k, BWG_LT, &sink, &f, 0);
    CHECK(k.flash == left, "LT aperta a tecla do cursor esquerdo");
    p.sx = p.sy = p.rx = p.ry = 0.1;
    CHECK(lock_osk_sticks(&k, &p) == 0, "solto: os cursores ficam");
    lock_osk_pad(&k, BWG_R3, &sink, &f, 0);
    CHECK(k.st.panel == BK_PANEL_SYMBOLS, "R3: símbolos");
    lock_osk_pad(&k, BWG_TOGGLE, &sink, &f, 0);
    CHECK(!k.shown, "Select fecha");

    if (!fails)
        printf("lock-osk: todos os casos passaram\n");
    return fails ? 1 : 0;
}
