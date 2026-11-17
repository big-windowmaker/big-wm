/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Pure helpers: body markup sanitizer, plain text, relative time, search
 * folding. No GTK widgets here (unit tested in tests/notifications).
 */
#include "bwn.h"
#include <string.h>

/* Control characters are dropped, except newline and tab. */
static gboolean
is_control(gunichar c)
{
    if (c == '\n' || c == '\t')
        return FALSE;
    return c < 0x20 || c == 0x7f || (c >= 0x80 && c < 0xa0) ||
           c == 0x200e || c == 0x200f || (c >= 0x202a && c <= 0x202e) ||
           (c >= 0x2066 && c <= 0x2069); /* bidi overrides: no spoofing */
}

static void
append_escaped_char(GString *out, gunichar c)
{
    switch (c) {
    case '<': g_string_append(out, "&lt;"); break;
    case '>': g_string_append(out, "&gt;"); break;
    case '&': g_string_append(out, "&amp;"); break;
    case '"': g_string_append(out, "&quot;"); break;
    case '\'': g_string_append(out, "&apos;"); break;
    default: g_string_append_unichar(out, c);
    }
}

/* Decodes an entity at p ('&'). Returns its length, or 0 if not valid. */
static gsize
parse_entity(const char *p, gunichar *out)
{
    static const struct { const char *name; gunichar c; } named[] = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
        { "apos", '\'' }, { "nbsp", 0xa0 },
    };
    const char *semi = strchr(p, ';');
    if (!semi || semi - p > 10 || semi - p < 3)
        return 0;
    gsize len = semi - p + 1;
    if (p[1] == '#') {
        char *end = NULL;
        guint64 v = (p[2] == 'x' || p[2] == 'X') ? g_ascii_strtoull(p + 3, &end, 16)
                                                 : g_ascii_strtoull(p + 2, &end, 10);
        if (end != semi || v == 0 || v > 0x10ffff || !g_unichar_validate((gunichar)v))
            return 0;
        *out = (gunichar)v;
        return len;
    }
    for (gsize i = 0; i < G_N_ELEMENTS(named); i++) {
        gsize n = strlen(named[i].name);
        if (n == len - 2 && strncmp(p + 1, named[i].name, n) == 0) {
            *out = named[i].c;
            return len;
        }
    }
    return 0;
}

/* The freedesktop body markup: <b> <i> <u> <a href> <img> and <br>.
 * Kept: b, i, u (balanced). <br> becomes a newline. Links and images keep
 * only their text (no hyperlinks capability: nothing is opened from a
 * notification's text). Everything else is escaped, so the result is always
 * valid Pango markup. At most max_chars characters of text. */
char *
bwn_markup_sanitize(const char *in, gsize max_chars)
{
    if (!in)
        return g_strdup("");
    char *valid = g_utf8_make_valid(in, -1);
    GString *out = g_string_new(NULL);
    char stack[64];
    int depth = 0;
    gsize chars = 0;
    const char *p = valid;

    while (*p && chars < max_chars && out->len < max_chars * 8) {
        /* A tag starts with a letter, '/' or '!' (comments); "1 < 2" is text. */
        gboolean tag_start = *p == '<' && (g_ascii_isalpha(p[1]) || p[1] == '!' ||
                                           (p[1] == '/' && g_ascii_isalpha(p[2])));
        if (tag_start) {
            const char *gt = strchr(p, '>');
            if (gt && gt - p <= 256) {
                gboolean closing = p[1] == '/';
                const char *name = p + 1 + (closing ? 1 : 0);
                gsize nlen = 0;
                while (g_ascii_isalpha(name[nlen]))
                    nlen++;
                char tag = 0;
                if (nlen == 1) {
                    char c = g_ascii_tolower(name[0]);
                    if (c == 'b' || c == 'i' || c == 'u')
                        tag = c;
                }
                gboolean br = nlen == 2 && g_ascii_strncasecmp(name, "br", 2) == 0;
                if (tag && !closing && depth < (int)sizeof stack) {
                    stack[depth++] = tag;
                    g_string_append_printf(out, "<%c>", tag);
                } else if (tag && closing) {
                    int at = depth - 1;
                    while (at >= 0 && stack[at] != tag)
                        at--;
                    if (at >= 0) {
                        while (depth > at)
                            g_string_append_printf(out, "</%c>", stack[--depth]);
                    }
                } else if (br) {
                    g_string_append_c(out, '\n');
                    chars++;
                }
                /* other tags (a, img, unknown): dropped, their text stays */
                p = gt + 1;
                continue;
            }
            g_string_append(out, "&lt;");
            chars++;
            p++;
            continue;
        }
        gunichar c = g_utf8_get_char(p);
        gsize step = g_utf8_next_char(p) - p;
        if (c == '&') {
            gunichar e;
            gsize elen = parse_entity(p, &e);
            if (elen) {
                if (!is_control(e))
                    append_escaped_char(out, e), chars++;
                p += elen;
                continue;
            }
        }
        if (!is_control(c)) {
            append_escaped_char(out, c);
            chars++;
        }
        p += step;
    }
    if (*p)
        g_string_append(out, "…");
    while (depth > 0)
        g_string_append_printf(out, "</%c>", stack[--depth]);
    g_free(valid);

    if (!pango_parse_markup(out->str, -1, 0, NULL, NULL, NULL, NULL)) {
        /* Never expected; still, never hand invalid markup to a label. */
        char *plain = g_markup_escape_text(out->str, -1);
        g_string_free(out, TRUE);
        return plain;
    }
    return g_string_free(out, FALSE);
}

char *
bwn_markup_to_plain(const char *markup)
{
    char *text = NULL;
    if (markup && pango_parse_markup(markup, -1, 0, NULL, &text, NULL, NULL))
        return text;
    return g_strdup(markup ? markup : "");
}

/* Plain text (the summary): valid UTF-8, no control characters, newlines as
 * spaces, at most max_chars characters. */
char *
bwn_text_clean(const char *in, gsize max_chars)
{
    if (!in)
        return g_strdup("");
    char *valid = g_utf8_make_valid(in, -1);
    GString *out = g_string_new(NULL);
    gsize chars = 0;
    for (const char *p = valid; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (chars >= max_chars) {
            g_string_append(out, "…");
            break;
        }
        if (c == '\n' || c == '\t')
            c = ' ';
        if (is_control(c))
            continue;
        g_string_append_unichar(out, c);
        chars++;
    }
    g_free(valid);
    return g_strstrip(g_string_free(out, FALSE));
}

/* "Now", "2 minutes ago", "Yesterday", "3 days ago", "2 weeks ago", a date. */
char *
bwn_reltime(gint64 then_us, gint64 now_us)
{
    gint64 secs = (now_us - then_us) / G_USEC_PER_SEC;
    if (secs < 60)
        return g_strdup(_("Now"));
    if (secs < 3600) {
        int m = (int)(secs / 60);
        return g_strdup_printf(ngettext("%d minute ago", "%d minutes ago", m), m);
    }
    GDateTime *then = g_date_time_new_from_unix_local(then_us / G_USEC_PER_SEC);
    GDateTime *now = g_date_time_new_from_unix_local(now_us / G_USEC_PER_SEC);
    GDate d1, d2;
    g_date_set_dmy(&d1, g_date_time_get_day_of_month(then), g_date_time_get_month(then),
                   g_date_time_get_year(then));
    g_date_set_dmy(&d2, g_date_time_get_day_of_month(now), g_date_time_get_month(now),
                   g_date_time_get_year(now));
    int days = g_date_days_between(&d1, &d2);
    char *s;
    if (days <= 0) {
        int h = (int)(secs / 3600);
        s = g_strdup_printf(ngettext("%d hour ago", "%d hours ago", h), h);
    } else if (days == 1) {
        s = g_strdup(_("Yesterday"));
    } else if (days < 7) {
        s = g_strdup_printf(ngettext("%d day ago", "%d days ago", days), days);
    } else if (days < 30) {
        int w = days / 7;
        s = g_strdup_printf(ngettext("%d week ago", "%d weeks ago", w), w);
    } else {
        s = g_date_time_format(then, "%x");
    }
    g_date_time_unref(then);
    g_date_time_unref(now);
    return s;
}

/* Lower case, without accents: "Ação" and "acao" match. */
char *
bwn_fold(const char *s)
{
    if (!s)
        return g_strdup("");
    char *nfd = g_utf8_normalize(s, -1, G_NORMALIZE_NFD);
    if (!nfd)
        return g_strdup("");
    GString *out = g_string_new(NULL);
    for (const char *p = nfd; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_type(c) != G_UNICODE_NON_SPACING_MARK)
            g_string_append_unichar(out, c);
    }
    g_free(nfd);
    char *folded = g_utf8_casefold(out->str, -1);
    g_string_free(out, TRUE);
    return folded;
}

/* Action keys come from the sender and go back to it in ActionInvoked:
 * never executed, only checked to be sane. */
gboolean
bwn_action_key_valid(const char *key)
{
    if (!key || !*key || strlen(key) > 128 || !g_utf8_validate(key, -1, NULL))
        return FALSE;
    for (const char *p = key; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (is_control(c) || c == '\n' || c == '\t')
            return FALSE;
    }
    return TRUE;
}
