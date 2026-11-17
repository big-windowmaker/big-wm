/* SPDX-License-Identifier: GPL-2.0-or-later
 * Unit tests of session/notifications/util.c (markup, text, time, search). */
#include "../../session/notifications/bwn.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_STR(got, want)                                                                  \
    do {                                                                                      \
        char *g_ = (got);                                                                     \
        if (strcmp(g_, (want)) != 0) {                                                        \
            fprintf(stderr, "%s:%d: got [%s] want [%s]\n", __FILE__, __LINE__, g_, (want));   \
            failures++;                                                                       \
        }                                                                                     \
        g_free(g_);                                                                           \
    } while (0)
#define CHECK(cond)                                                                           \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                        \
            failures++;                                                                       \
        }                                                                                     \
    } while (0)

static gboolean
valid_markup(const char *s)
{
    return pango_parse_markup(s, -1, 0, NULL, NULL, NULL, NULL);
}

int
main(void)
{
    setlocale(LC_ALL, "C.UTF-8");

    /* Markup: b, i, u kept; others dropped with their text kept; entities. */
    CHECK_STR(bwn_markup_sanitize("<b>bold</b> and <i>it</i>", 100), "<b>bold</b> and <i>it</i>");
    CHECK_STR(bwn_markup_sanitize("<script>alert(1)</script>", 100), "alert(1)");
    CHECK_STR(bwn_markup_sanitize("<a href='http://x'>link</a>", 100), "link");
    CHECK_STR(bwn_markup_sanitize("<img src='x' alt='y'/>z", 100), "z");
    CHECK_STR(bwn_markup_sanitize("a<br>b", 100), "a\nb");
    CHECK_STR(bwn_markup_sanitize("1 < 2 & 3 > 2", 100), "1 &lt; 2 &amp; 3 &gt; 2");
    CHECK_STR(bwn_markup_sanitize("&bogus; &amp; &#65; &#x42; &#0;", 100), "&amp;bogus; &amp; A B &amp;#0;");
    CHECK_STR(bwn_markup_sanitize("<b>open", 100), "<b>open</b>");
    CHECK_STR(bwn_markup_sanitize("<b><i>x</b></i>", 100), "<b><i>x</i></b>");
    CHECK_STR(bwn_markup_sanitize("</b>stray", 100), "stray");
    CHECK_STR(bwn_markup_sanitize("tab\tnl\nbell\a\x1b[31mred", 100), "tab\tnl\nbell[31mred");
    CHECK_STR(bwn_markup_sanitize("\xe2\x80\xae" "evil", 100), "evil"); /* bidi override dropped */
    CHECK_STR(bwn_markup_sanitize("ação 🎉 coração", 100), "ação 🎉 coração");
    CHECK_STR(bwn_markup_sanitize("abcdef", 3), "abc…");
    CHECK_STR(bwn_markup_sanitize("\xff\xfeok", 100), "\xef\xbf\xbd\xef\xbf\xbdok"); /* invalid UTF-8 */
    CHECK_STR(bwn_markup_sanitize(NULL, 10), "");
    const char *nasty[] = { "<b", "<<<>>>", "<b x='\">'>t</b>", "&#x110000;", "&#55296;",
                            "<u><u><u><u>deep", "\"quotes\" 'single'", "<!-- c -->x", "<![CDATA[x]]>" };
    for (gsize i = 0; i < G_N_ELEMENTS(nasty); i++) {
        char *m = bwn_markup_sanitize(nasty[i], 100);
        CHECK(valid_markup(m));
        g_free(m);
    }
    /* Deep nesting beyond the tag stack stays valid. */
    GString *deep = g_string_new(NULL);
    for (int i = 0; i < 200; i++)
        g_string_append(deep, "<b>");
    g_string_append(deep, "x");
    char *m = bwn_markup_sanitize(deep->str, 1000);
    CHECK(valid_markup(m));
    g_free(m);
    g_string_free(deep, TRUE);

    /* Empty tag pairs add no characters: the bytes are bounded too. */
    GString *empty = g_string_new(NULL);
    for (int i = 0; i < 100000; i++)
        g_string_append(empty, "<b></b>");
    m = bwn_markup_sanitize(empty->str, 100);
    CHECK(strlen(m) <= 100 * 8 + 64);
    CHECK(valid_markup(m));
    g_free(m);
    g_string_free(empty, TRUE);

    CHECK_STR(bwn_markup_to_plain("<b>a</b> &amp; b"), "a & b");

    /* Summary: plain, newlines as spaces, controls dropped, limit. */
    CHECK_STR(bwn_text_clean("line1\nline2\x01", 100), "line1 line2");
    CHECK_STR(bwn_text_clean("  padded  ", 100), "padded");
    CHECK_STR(bwn_text_clean("abcdef", 4), "abcd…");
    CHECK_STR(bwn_text_clean("<b>not markup</b>", 100), "<b>not markup</b>");

    /* Relative time (C locale: the English source strings). */
    gint64 now = g_get_real_time();
    gint64 s = G_USEC_PER_SEC;
    CHECK_STR(bwn_reltime(now - 5 * s, now), "Now");
    CHECK_STR(bwn_reltime(now - 60 * s, now), "1 minute ago");
    CHECK_STR(bwn_reltime(now - 15 * 60 * s, now), "15 minutes ago");
    GDateTime *noon = g_date_time_new_local(2026, 11, 17, 12, 0, 0);
    gint64 n12 = g_date_time_to_unix(noon) * s;
    g_date_time_unref(noon);
    CHECK_STR(bwn_reltime(n12 - 3600 * s, n12), "1 hour ago");
    CHECK_STR(bwn_reltime(n12 - 3 * 3600 * s, n12), "3 hours ago");
    CHECK_STR(bwn_reltime(n12 - 24 * 3600 * s, n12), "Yesterday");
    CHECK_STR(bwn_reltime(n12 - 2 * 24 * 3600 * s, n12), "2 days ago");
    CHECK_STR(bwn_reltime(n12 - 7 * 24 * 3600 * s, n12), "1 week ago");
    CHECK_STR(bwn_reltime(n12 - 15 * 24 * 3600 * s, n12), "2 weeks ago");
    char *old = bwn_reltime(n12 - 90LL * 24 * 3600 * s, n12);
    CHECK(strchr(old, '/') != NULL); /* a date */
    g_free(old);

    /* Search folding. */
    CHECK_STR(bwn_fold("Ação CORAÇÃO Pão"), "acao coracao pao");
    CHECK_STR(bwn_fold("Ünïcödé"), "unicode");

    /* Action keys. */
    CHECK(bwn_action_key_valid("default"));
    CHECK(bwn_action_key_valid("open-folder:1"));
    CHECK(!bwn_action_key_valid(""));
    CHECK(!bwn_action_key_valid("a\nb"));
    CHECK(!bwn_action_key_valid(NULL));

    if (failures)
        fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
