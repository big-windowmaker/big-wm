/* SPDX-License-Identifier: GPL-2.0-or-later
 * Big WM Virtual Keyboard: layouts e estado das teclas (veja bk-keys.h). */
#include "bk-keys.h"

#include <stdio.h>
#include <string.h>

#define C(n, s, a) { BK_KEY_CHAR, { n, s, a }, 0, NULL, 0, 0, 1 }
#define CW(n, s, a, w) { BK_KEY_CHAR, { n, s, a }, 0, NULL, 0, 0, w }
#define CD(n, s, a, d) { BK_KEY_CHAR, { n, s, a }, d, NULL, 0, 0, 1 }
#define SP(label, sym, w) { BK_KEY_SPECIAL, { label, NULL, NULL }, 0, sym, 0, 0, w }
#define MOD(label, m, w) { BK_KEY_MOD, { label, NULL, NULL }, 0, NULL, m, 0, w }
#define CAPS(w) { BK_KEY_CAPS, { "Caps", NULL, NULL }, 0, NULL, 0, 0, w }
#define PANEL(label, p, w) { BK_KEY_PANEL, { label, NULL, NULL }, 0, NULL, 0, p, w }
#define LAYOUTKEY(w) { BK_KEY_LAYOUT, { "", NULL, NULL }, 0, NULL, 0, 0, w }
#define HIDE(w) { BK_KEY_HIDE, { "⌄", NULL, NULL }, 0, NULL, 0, 0, w }
#define END { BK_KEY_CHAR, { NULL, NULL, NULL }, 0, NULL, 0, 0, 0 }

/* A fileira de baixo é a mesma nos três layouts. */
#define BOTTOM_ROW                                                                                                     \
    { MOD("Ctrl", BK_MOD_CTRL, 1.3), MOD("❖", BK_MOD_SUPER, 1.1), MOD("Alt", BK_MOD_ALT, 1.1),                        \
      CW(" ", NULL, NULL, 5.1), MOD("AltGr", BK_MOD_ALTGR, 1.1), PANEL("Fn", BK_PANEL_FN, 1.1),                       \
      PANEL("☺", BK_PANEL_SYMBOLS, 1.1), LAYOUTKEY(1.2), HIDE(1.2), END }

static const BkLayout layouts[] = {
    { "br", "BR",
      { { C("'", "\"", NULL), C("1", "!", "¹"), C("2", "@", "²"), C("3", "#", "³"), C("4", "$", "£"),
          C("5", "%", "¢"), CD("6", "¨", "¬", 2), C("7", "&", NULL), C("8", "*", NULL), C("9", "(", NULL),
          C("0", ")", NULL), C("-", "_", NULL), C("=", "+", "§"), SP("⌫", "BackSpace", 1.8), END },
        { SP("Tab", "Tab", 1.5), C("q", "Q", "/"), C("w", "W", "?"), C("e", "E", "€"), C("r", "R", "®"),
          C("t", "T", NULL), C("y", "Y", NULL), C("u", "U", NULL), C("i", "I", NULL), C("o", "O", NULL),
          C("p", "P", NULL), CD("´", "`", NULL, 3), C("[", "{", "ª"), SP("Del", "Delete", 1.3), END },
        { CAPS(1.6), C("a", "A", NULL), C("s", "S", NULL), C("d", "D", NULL), C("f", "F", NULL),
          C("g", "G", NULL), C("h", "H", NULL), C("j", "J", NULL), C("k", "K", NULL), C("l", "L", NULL),
          C("ç", "Ç", NULL), CD("~", "^", NULL, 3), C("]", "}", "º"), SP("⏎", "Return", 1.2), END },
        { MOD("⇧", BK_MOD_SHIFT, 1.3), C("\\", "|", NULL), C("z", "Z", NULL), C("x", "X", NULL),
          C("c", "C", NULL), C("v", "V", NULL), C("b", "B", NULL), C("n", "N", NULL), C("m", "M", NULL),
          C(",", "<", NULL), C(".", ">", NULL), C(";", ":", NULL), C("/", "?", "°"), MOD("⇧", BK_MOD_SHIFT, 1.5),
          END },
        BOTTOM_ROW,
        { END } } },
    { "us", "US",
      { { C("`", "~", NULL), C("1", "!", NULL), C("2", "@", NULL), C("3", "#", NULL), C("4", "$", NULL),
          C("5", "%", NULL), C("6", "^", NULL), C("7", "&", NULL), C("8", "*", NULL), C("9", "(", NULL),
          C("0", ")", NULL), C("-", "_", NULL), C("=", "+", NULL), SP("⌫", "BackSpace", 1.8), END },
        { SP("Tab", "Tab", 1.5), C("q", "Q", NULL), C("w", "W", NULL), C("e", "E", NULL), C("r", "R", NULL),
          C("t", "T", NULL), C("y", "Y", NULL), C("u", "U", NULL), C("i", "I", NULL), C("o", "O", NULL),
          C("p", "P", NULL), C("[", "{", NULL), C("]", "}", NULL), C("\\", "|", NULL), END },
        { CAPS(1.8), C("a", "A", NULL), C("s", "S", NULL), C("d", "D", NULL), C("f", "F", NULL),
          C("g", "G", NULL), C("h", "H", NULL), C("j", "J", NULL), C("k", "K", NULL), C("l", "L", NULL),
          C(";", ":", NULL), C("'", "\"", NULL), SP("⏎", "Return", 2.0), END },
        { MOD("⇧", BK_MOD_SHIFT, 2.2), C("z", "Z", NULL), C("x", "X", NULL), C("c", "C", NULL),
          C("v", "V", NULL), C("b", "B", NULL), C("n", "N", NULL), C("m", "M", NULL), C(",", "<", NULL),
          C(".", ">", NULL), C("/", "?", NULL), MOD("⇧", BK_MOD_SHIFT, 2.6), END },
        BOTTOM_ROW,
        { END } } },
    /* US internacional: ' " ` ~ ^ mortas, AltGr com as letras acentuadas. */
    { "us-intl", "INTL",
      { { CD("`", "~", NULL, 3), C("1", "!", "¡"), C("2", "@", "²"), C("3", "#", "³"), C("4", "$", "¤"),
          C("5", "%", "€"), CD("6", "^", "¼", 2), C("7", "&", "½"), C("8", "*", "¾"), C("9", "(", "‘"),
          C("0", ")", "’"), C("-", "_", "¥"), C("=", "+", "×"), SP("⌫", "BackSpace", 1.8), END },
        { SP("Tab", "Tab", 1.5), C("q", "Q", "ä"), C("w", "W", "å"), C("e", "E", "é"), C("r", "R", "®"),
          C("t", "T", "þ"), C("y", "Y", "ü"), C("u", "U", "ú"), C("i", "I", "í"), C("o", "O", "ó"),
          C("p", "P", "ö"), C("[", "{", "«"), C("]", "}", "»"), C("\\", "|", "¬"), END },
        { CAPS(1.8), C("a", "A", "á"), C("s", "S", "ß"), C("d", "D", "ð"), C("f", "F", NULL),
          C("g", "G", NULL), C("h", "H", NULL), C("j", "J", NULL), C("k", "K", NULL), C("l", "L", "ø"),
          C(";", ":", "¶"), CD("'", "\"", "´", 3), SP("⏎", "Return", 2.0), END },
        { MOD("⇧", BK_MOD_SHIFT, 2.2), C("z", "Z", "æ"), C("x", "X", NULL), C("c", "C", "©"),
          C("v", "V", NULL), C("b", "B", NULL), C("n", "N", "ñ"), C("m", "M", "µ"), C(",", "<", "ç"),
          C(".", ">", NULL), C("/", "?", "¿"), MOD("⇧", BK_MOD_SHIFT, 2.6), END },
        BOTTOM_ROW,
        { END } } },
};

/* F1-F12, navegação e edição. */
static const BkKey panel_fn[BK_MAX_ROWS][BK_MAX_KEYS] = {
    { SP("Esc", "Escape", 1.3), SP("F1", "F1", 1), SP("F2", "F2", 1), SP("F3", "F3", 1), SP("F4", "F4", 1),
      SP("F5", "F5", 1), SP("F6", "F6", 1), SP("F7", "F7", 1), SP("F8", "F8", 1), SP("F9", "F9", 1),
      SP("F10", "F10", 1), SP("F11", "F11", 1), SP("F12", "F12", 1), SP("⌫", "BackSpace", 1.5), END },
    { SP("Tab", "Tab", 1.5), SP("Ins", "Insert", 1.3), SP("Home", "Home", 1.3), SP("PgUp", "Prior", 1.3),
      SP("PrtSc", "Print", 1.3), SP("Del", "Delete", 1.3), SP("End", "End", 1.3), SP("PgDn", "Next", 1.3),
      SP("↑", "Up", 1.3), SP("Menu", "Menu", 1.3), END },
    { CAPS(1.6), SP("⇤", "Home", 1.3), SP("⇥", "End", 1.3), SP("←", "Left", 1.3), SP("↓", "Down", 1.3),
      SP("→", "Right", 1.3), SP("Pause", "Pause", 1.3), SP("⏎", "Return", 2.0), END },
    { MOD("⇧", BK_MOD_SHIFT, 1.5), SP("☀−", "XF86MonBrightnessDown", 1.3), SP("☀+", "XF86MonBrightnessUp", 1.3),
      SP("🔇", "XF86AudioMute", 1.3), SP("🔉", "XF86AudioLowerVolume", 1.3), SP("🔊", "XF86AudioRaiseVolume", 1.3),
      SP("⏯", "XF86AudioPlay", 1.3), MOD("⇧", BK_MOD_SHIFT, 1.5), END },
    { MOD("Ctrl", BK_MOD_CTRL, 1.3), MOD("❖", BK_MOD_SUPER, 1.1), MOD("Alt", BK_MOD_ALT, 1.1),
      CW(" ", NULL, NULL, 5.1), MOD("AltGr", BK_MOD_ALTGR, 1.1), PANEL("ABC", BK_PANEL_MAIN, 1.1),
      PANEL("☺", BK_PANEL_SYMBOLS, 1.1), LAYOUTKEY(1.2), HIDE(1.2), END },
    { END },
};

/* Símbolos e emojis: texto pronto, sem níveis. */
static const BkKey panel_symbols[BK_MAX_ROWS][BK_MAX_KEYS] = {
    { C("€", NULL, NULL), C("£", NULL, NULL), C("¥", NULL, NULL), C("¢", NULL, NULL), C("©", NULL, NULL),
      C("®", NULL, NULL), C("™", NULL, NULL), C("°", NULL, NULL), C("±", NULL, NULL), C("×", NULL, NULL),
      C("÷", NULL, NULL), C("§", NULL, NULL), C("¶", NULL, NULL), SP("⌫", "BackSpace", 1.8), END },
    { C("«", NULL, NULL), C("»", NULL, NULL), C("¿", NULL, NULL), C("¡", NULL, NULL), C("•", NULL, NULL),
      C("…", NULL, NULL), C("—", NULL, NULL), C("“", NULL, NULL), C("”", NULL, NULL), C("‘", NULL, NULL),
      C("’", NULL, NULL), C("½", NULL, NULL), C("¼", NULL, NULL), C("µ", NULL, NULL), END },
    { C("😀", NULL, NULL), C("😂", NULL, NULL), C("😊", NULL, NULL), C("😍", NULL, NULL), C("😎", NULL, NULL),
      C("🤔", NULL, NULL), C("😢", NULL, NULL), C("😡", NULL, NULL), C("👍", NULL, NULL), C("👎", NULL, NULL),
      C("👏", NULL, NULL), C("🙏", NULL, NULL), SP("⏎", "Return", 1.8), END },
    { C("❤️", NULL, NULL), C("🔥", NULL, NULL), C("✨", NULL, NULL), C("🎉", NULL, NULL), C("✅", NULL, NULL),
      C("❌", NULL, NULL), C("⚠️", NULL, NULL), C("💡", NULL, NULL), C("🎮", NULL, NULL), C("⌨️", NULL, NULL),
      C("🐧", NULL, NULL), C("☕", NULL, NULL), C("🚀", NULL, NULL), END },
    { MOD("Ctrl", BK_MOD_CTRL, 1.3), MOD("❖", BK_MOD_SUPER, 1.1), MOD("Alt", BK_MOD_ALT, 1.1),
      CW(" ", NULL, NULL, 5.1), MOD("AltGr", BK_MOD_ALTGR, 1.1), PANEL("Fn", BK_PANEL_FN, 1.1),
      PANEL("ABC", BK_PANEL_MAIN, 1.1), LAYOUTKEY(1.2), HIDE(1.2), END },
    { END },
};

int
bk_layout_count(void)
{
    return (int)(sizeof layouts / sizeof layouts[0]);
}

const BkLayout *
bk_layout_get(int i)
{
    return i >= 0 && i < bk_layout_count() ? &layouts[i] : &layouts[0];
}

const BkLayout *
bk_layout_by_id(const char *id)
{
    for (int i = 0; id && i < bk_layout_count(); i++)
        if (!strcmp(layouts[i].id, id))
            return &layouts[i];
    return NULL;
}

const BkKey (*bk_panel_rows(const BkLayout *l, BkPanel panel))[BK_MAX_KEYS]
{
    if (panel == BK_PANEL_FN)
        return panel_fn;
    if (panel == BK_PANEL_SYMBOLS)
        return panel_symbols;
    return l->rows;
}

/* ---- composição ---------------------------------------------------------- */

const char *
bk_compose(const char *dead, const char *c)
{
    static const struct {
        const char *dead, *base, *out;
    } t[] = {
        { "´", "a", "á" }, { "´", "e", "é" }, { "´", "i", "í" }, { "´", "o", "ó" }, { "´", "u", "ú" },
        { "´", "y", "ý" }, { "´", "c", "ç" }, { "´", "A", "Á" }, { "´", "E", "É" }, { "´", "I", "Í" },
        { "´", "O", "Ó" }, { "´", "U", "Ú" }, { "´", "Y", "Ý" }, { "´", "C", "Ç" },
        { "'", "a", "á" }, { "'", "e", "é" }, { "'", "i", "í" }, { "'", "o", "ó" }, { "'", "u", "ú" },
        { "'", "c", "ç" }, { "'", "A", "Á" }, { "'", "E", "É" }, { "'", "I", "Í" }, { "'", "O", "Ó" },
        { "'", "U", "Ú" }, { "'", "C", "Ç" },
        { "`", "a", "à" }, { "`", "e", "è" }, { "`", "i", "ì" }, { "`", "o", "ò" }, { "`", "u", "ù" },
        { "`", "A", "À" }, { "`", "E", "È" }, { "`", "I", "Ì" }, { "`", "O", "Ò" }, { "`", "U", "Ù" },
        { "~", "a", "ã" }, { "~", "o", "õ" }, { "~", "n", "ñ" }, { "~", "A", "Ã" }, { "~", "O", "Õ" },
        { "~", "N", "Ñ" },
        { "^", "a", "â" }, { "^", "e", "ê" }, { "^", "i", "î" }, { "^", "o", "ô" }, { "^", "u", "û" },
        { "^", "A", "Â" }, { "^", "E", "Ê" }, { "^", "I", "Î" }, { "^", "O", "Ô" }, { "^", "U", "Û" },
        { "¨", "a", "ä" }, { "¨", "e", "ë" }, { "¨", "i", "ï" }, { "¨", "o", "ö" }, { "¨", "u", "ü" },
        { "¨", "y", "ÿ" }, { "¨", "A", "Ä" }, { "¨", "E", "Ë" }, { "¨", "I", "Ï" }, { "¨", "O", "Ö" },
        { "¨", "U", "Ü" },
        { "\"", "a", "ä" }, { "\"", "e", "ë" }, { "\"", "i", "ï" }, { "\"", "o", "ö" }, { "\"", "u", "ü" },
        { "\"", "A", "Ä" }, { "\"", "E", "Ë" }, { "\"", "I", "Ï" }, { "\"", "O", "Ö" }, { "\"", "U", "Ü" },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].dead, dead) && !strcmp(t[i].base, c))
            return t[i].out;
    return NULL;
}

/* ---- estado -------------------------------------------------------------- */

void
bk_state_init(BkState *s, int layout)
{
    memset(s, 0, sizeof *s);
    s->layout = layout >= 0 && layout < bk_layout_count() ? layout : 0;
}

unsigned
bk_state_mods(const BkState *s)
{
    return s->latched | s->locked;
}

void
bk_state_release_all(BkState *s)
{
    s->latched = s->locked = 0;
    s->dead[0] = 0;
}

static int
is_letter(const char *t)
{
    if (!t || !t[0])
        return 0;
    unsigned char c = (unsigned char)t[0];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return t[1] == 0;
    return !strcmp(t, "ç") || !strcmp(t, "Ç");
}

static int
level_of(const BkState *s, const BkKey *k)
{
    unsigned m = bk_state_mods(s);
    if ((m & BK_MOD_ALTGR) && k->level[2])
        return 2;
    int shift = (m & BK_MOD_SHIFT) != 0;
    if (s->caps && is_letter(k->level[0]))
        shift = !shift; /* Caps vale para letras; Shift para tudo */
    return shift && k->level[1] ? 1 : 0;
}

const char *
bk_state_label(const BkState *s, const BkKey *k)
{
    if (k->kind == BK_KEY_LAYOUT)
        return bk_layout_get(s->layout)->name;
    if (k->kind != BK_KEY_CHAR)
        return k->level[0];
    return k->level[level_of(s, k)];
}

static int
emit_text(BkOut *out, int n, int max, const char *t)
{
    if (n < max && t && t[0]) {
        out[n].kind = BK_OUT_TEXT;
        snprintf(out[n].text, sizeof out[n].text, "%s", t);
        out[n].keysym[0] = 0;
        out[n].mods = 0;
        n++;
    }
    return n;
}

static int
emit_key(BkOut *out, int n, int max, const char *sym, unsigned mods)
{
    if (n < max) {
        out[n].kind = BK_OUT_KEY;
        snprintf(out[n].keysym, sizeof out[n].keysym, "%s", sym);
        out[n].text[0] = 0;
        out[n].mods = mods;
        n++;
    }
    return n;
}

int
bk_state_press(BkState *s, const BkKey *k, BkOut *out, int max)
{
    int n = 0;
    unsigned mods = bk_state_mods(s);
    switch (k->kind) {
    case BK_KEY_MOD:
        /* um toque: próxima tecla; dois: travado; três: solto */
        if (s->locked & k->mod) {
            s->locked &= ~k->mod;
        } else if (s->latched & k->mod) {
            s->latched &= ~k->mod;
            s->locked |= k->mod;
        } else {
            s->latched |= k->mod;
        }
        return 0;
    case BK_KEY_CAPS:
        s->caps = !s->caps;
        return 0;
    case BK_KEY_PANEL:
        s->panel = (BkPanel)k->panel;
        return 0;
    case BK_KEY_LAYOUT:
        s->layout = (s->layout + 1) % bk_layout_count();
        s->dead[0] = 0;
        return 0;
    case BK_KEY_HIDE:
        return 0;
    case BK_KEY_SPECIAL:
        /* a tecla morta pendente sai sozinha antes de uma tecla especial */
        if (s->dead[0] && strcmp(k->keysym, "BackSpace") != 0)
            n = emit_text(out, n, max, s->dead);
        s->dead[0] = 0;
        n = emit_key(out, n, max, k->keysym, mods & ~BK_MOD_ALTGR);
        break;
    case BK_KEY_CHAR: {
        int lvl = level_of(s, k);
        const char *t = k->level[lvl] ? k->level[lvl] : k->level[0];
        if (mods & (BK_MOD_CTRL | BK_MOD_ALT | BK_MOD_SUPER)) {
            /* atalho (Ctrl+C, Alt+F4...): a tecla de base com os modificadores */
            s->dead[0] = 0;
            n = emit_key(out, n, max, k->level[0], mods & ~BK_MOD_ALTGR);
        } else if ((k->dead >> lvl) & 1 && !s->dead[0]) {
            snprintf(s->dead, sizeof s->dead, "%s", t);
        } else if (s->dead[0]) {
            const char *c = !strcmp(t, " ") ? s->dead : bk_compose(s->dead, t);
            if (c) {
                n = emit_text(out, n, max, c);
            } else {
                n = emit_text(out, n, max, s->dead);
                n = emit_text(out, n, max, t);
            }
            s->dead[0] = 0;
        } else {
            n = emit_text(out, n, max, t);
        }
        break;
    }
    }
    s->latched = 0; /* valiam para esta tecla */
    return n;
}
