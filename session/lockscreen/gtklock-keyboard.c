// SPDX-License-Identifier: GPL-3.0-or-later
//
// Teclado virtual da tela de bloqueio do Big WindowMaker no Wayland: um
// módulo do gtklock (gtklock -m .../big-wm-keyboard.so).
//
// Segurança: as teclas são botões dentro da própria superfície de bloqueio
// (ext-session-lock), no processo do gtklock. O texto vai direto para o
// campo de senha do gtklock (gtk_editable_insert_text), como uma tecla
// física: nada de injeção de eventos, de outro processo, de linha de comando
// ou de log. Os botões não pegam o foco, que fica no campo de senha. O PAM
// continua com o gtklock.
//
// O teclado é criado só quando o botão ⌨ é usado e fica oculto até lá
// (ou já aberto, com uma tela de toque e KeyboardTouch). Layouts: ABNT2 e
// US (o padrão vem do lock.conf, senão do layout XKB do sistema: "br" na
// configuração do teclado, senão do idioma pt). Teclas mortas do ABNT2
// (´ ` ~ ^ ¨) compõem com a vogal seguinte.
//
// Preferências (~/.config/big-wm/lock.conf, as mesmas do X11):
// VirtualKeyboardButton, KeyboardLayout (auto/br/us), KeyboardSize
// (small/medium/large), KeyboardTheme (light/dark; sem ele, o modo do Big
// WindowMaker), KeyboardTouch.
//
// A estrutura Window é a do gtklock 4.0 (include/window.h, GPL-3.0-or-later,
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik, Bhaskar Khoraja): o módulo
// declara a versão 4.0 e o gtklock recusa outra versão maior.

#include <gmodule.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <string.h>

// Textos: catálogo "big-wm" (po/ na raiz do big-wm), msgids em inglês; o
// domínio do gtklock (textdomain) não muda.
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif
#define _(s) dgettext("big-wm", s)

struct GtkLock;

struct Window {
	GdkMonitor *monitor;
	GtkWidget *window;
	GtkWidget *overlay;
	GtkWidget *window_box;
	GtkWidget *body_revealer;
	GtkWidget *body_grid;
	GtkWidget *input_label;
	GtkWidget *input_field;
	GtkWidget *message_revealer;
	GtkWidget *message_scrolled_window;
	GtkWidget *message_box;
	GtkWidget *unlock_button;
	GtkWidget *error_label;
	GtkWidget *warning_label;
	GtkWidget *info_box;
	GtkWidget *time_box;
	GtkWidget *clock_label;
	GtkWidget *date_label;
	void *module_data[];
};

G_MODULE_EXPORT const gchar module_name[] = "big-wm-keyboard";
G_MODULE_EXPORT const guint module_major_version = 4;
G_MODULE_EXPORT const guint module_minor_version = 0;

enum { L_ABNT2, L_US, L_N };

// Cada tecla: rótulo normal e com Shift. "\b" apaga, "\n" desbloqueia; as
// teclas especiais têm nome próprio.
typedef struct {
	const char *normal, *shift;
	double width;   // em unidades de tecla
} key_t_;

#define K(a, b) { a, b, 1 }
#define KW(a, b, w) { a, b, w }
#define ROW_END { NULL, NULL, 0 }

static const key_t_ rows_abnt2[][15] = {
	{ K("'", "\""), K("1", "!"), K("2", "@"), K("3", "#"), K("4", "$"), K("5", "%"), K("6", "¨"), K("7", "&"),
	  K("8", "*"), K("9", "("), K("0", ")"), K("-", "_"), K("=", "+"), KW("⌫", "⌫", 1.6), ROW_END },
	{ K("q", "Q"), K("w", "W"), K("e", "E"), K("r", "R"), K("t", "T"), K("y", "Y"), K("u", "U"), K("i", "I"),
	  K("o", "O"), K("p", "P"), K("´", "`"), K("[", "{"), ROW_END },
	{ K("a", "A"), K("s", "S"), K("d", "D"), K("f", "F"), K("g", "G"), K("h", "H"), K("j", "J"), K("k", "K"),
	  K("l", "L"), K("ç", "Ç"), K("~", "^"), K("]", "}"), ROW_END },
	{ KW("⇧", "⇧", 1.4), K("\\", "|"), K("z", "Z"), K("x", "X"), K("c", "C"), K("v", "V"), K("b", "B"),
	  K("n", "N"), K("m", "M"), K(",", "<"), K(".", ">"), K(";", ":"), K("/", "?"), ROW_END },
};
static const key_t_ rows_us[][15] = {
	{ K("`", "~"), K("1", "!"), K("2", "@"), K("3", "#"), K("4", "$"), K("5", "%"), K("6", "^"), K("7", "&"),
	  K("8", "*"), K("9", "("), K("0", ")"), K("-", "_"), K("=", "+"), KW("⌫", "⌫", 1.6), ROW_END },
	{ K("q", "Q"), K("w", "W"), K("e", "E"), K("r", "R"), K("t", "T"), K("y", "Y"), K("u", "U"), K("i", "I"),
	  K("o", "O"), K("p", "P"), K("[", "{"), K("]", "}"), K("\\", "|"), ROW_END },
	{ K("a", "A"), K("s", "S"), K("d", "D"), K("f", "F"), K("g", "G"), K("h", "H"), K("j", "J"), K("k", "K"),
	  K("l", "L"), K(";", ":"), K("'", "\""), ROW_END },
	{ KW("⇧", "⇧", 1.4), K("z", "Z"), K("x", "X"), K("c", "C"), K("v", "V"), K("b", "B"), K("n", "N"),
	  K("m", "M"), K(",", "<"), K(".", ">"), K("/", "?"), ROW_END },
};

// Preferências do lock.conf.
static struct {
	gboolean button, touch, dark;
	int layout; // -1: o do sistema
	double size;
} prefs = { TRUE, TRUE, FALSE, -1, 1 };

static void
load_prefs(void)
{
	// Sem KeyboardTheme, o modo do Big WindowMaker (~/.config/big-wm/theme).
	gchar *mode = NULL, *theme = g_build_filename(g_get_user_config_dir(), "big-wm", "theme", NULL);
	if (g_file_get_contents(theme, &mode, NULL, NULL))
		prefs.dark = g_str_has_prefix(mode, "dark");
	g_free(mode);
	g_free(theme);
	gchar *path = g_build_filename(g_get_user_config_dir(), "big-wm", "lock.conf", NULL);
	GKeyFile *kf = g_key_file_new();
	if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
		gchar *v;
		if ((v = g_key_file_get_string(kf, "Lock", "VirtualKeyboardButton", NULL))) {
			prefs.button = g_strcmp0(v, "false") != 0;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardTouch", NULL))) {
			prefs.touch = g_strcmp0(v, "false") != 0;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardTheme", NULL))) {
			if (!g_strcmp0(v, "dark") || !g_strcmp0(v, "light"))
				prefs.dark = g_strcmp0(v, "dark") == 0;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardLayout", NULL))) {
			prefs.layout = !g_strcmp0(v, "br") || !g_strcmp0(v, "abnt2") ? 0 : !g_strcmp0(v, "us") ? 1 : -1;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardSize", NULL))) {
			prefs.size = !g_strcmp0(v, "small") ? 0.85 : !g_strcmp0(v, "large") ? 1.3 : 1;
			g_free(v);
		}
	}
	g_key_file_free(kf);
	g_free(path);
}

typedef struct {
	struct Window *win;
	GtkWidget *toggle;        // botão ⌨ ao lado do campo
	GtkWidget *revealer;      // o teclado (criado no primeiro uso)
	GtkWidget *grid;
	GtkWidget *layout_btn;
	GtkWidget *caps_btn;
	GPtrArray *keys;          // botões das teclas de caractere
	int layout;
	gboolean shift, caps;
	char dead[8];             // tecla morta pendente (ABNT2)
} kbd;

static guint self_id;

static int
default_layout(void)
{
	if (prefs.layout >= 0)
		return prefs.layout;
	// O layout XKB do sistema (o mesmo do X11 e do wlmaker), senão o idioma.
	gchar *conf = NULL;
	if (g_file_get_contents("/etc/X11/xorg.conf.d/00-keyboard.conf", &conf, NULL, NULL)) {
		gboolean br = strstr(conf, "\"br\"") != NULL;
		g_free(conf);
		return br ? L_ABNT2 : L_US;
	}
	const char *lang = g_getenv("LANG");
	return lang && g_str_has_prefix(lang, "pt") ? L_ABNT2 : L_US;
}

// ´`~^¨ + vogal (ABNT2): o caractere composto; NULL se não compõe.
static const char *
compose(const char *dead, const char *c)
{
	static const struct { const char *dead, *base, *out; } t[] = {
		{ "´", "a", "á" }, { "´", "e", "é" }, { "´", "i", "í" }, { "´", "o", "ó" }, { "´", "u", "ú" },
		{ "´", "A", "Á" }, { "´", "E", "É" }, { "´", "I", "Í" }, { "´", "O", "Ó" }, { "´", "U", "Ú" },
		{ "`", "a", "à" }, { "`", "A", "À" },
		{ "~", "a", "ã" }, { "~", "o", "õ" }, { "~", "A", "Ã" }, { "~", "O", "Õ" }, { "~", "n", "ñ" },
		{ "^", "a", "â" }, { "^", "e", "ê" }, { "^", "o", "ô" }, { "^", "A", "Â" }, { "^", "E", "Ê" },
		{ "^", "O", "Ô" },
		{ "¨", "u", "ü" }, { "¨", "U", "Ü" },
	};
	for (size_t i = 0; i < G_N_ELEMENTS(t); i++)
		if (!strcmp(t[i].dead, dead) && !strcmp(t[i].base, c))
			return t[i].out;
	return NULL;
}

static gboolean
is_dead(int layout, const char *s)
{
	return layout == L_ABNT2 &&
	       (!strcmp(s, "´") || !strcmp(s, "`") || !strcmp(s, "~") || !strcmp(s, "^") || !strcmp(s, "¨"));
}

static void
insert(kbd *k, const char *s)
{
	GtkEditable *e = GTK_EDITABLE(k->win->input_field);
	gint pos = gtk_editable_get_position(e);
	gtk_editable_insert_text(e, s, (gint)strlen(s), &pos);
	gtk_editable_set_position(e, pos);
}

static void
relabel(kbd *k)
{
	gboolean upper = k->shift != k->caps;
	for (guint i = 0; i < k->keys->len; i++) {
		GtkWidget *b = g_ptr_array_index(k->keys, i);
		const key_t_ *key = g_object_get_data(G_OBJECT(b), "key");
		gboolean letter = g_utf8_strlen(key->normal, -1) == 1 && g_unichar_isalpha(g_utf8_get_char(key->normal));
		// Caps vale para letras; Shift para tudo.
		gboolean up = letter ? upper : k->shift;
		gtk_button_set_label(GTK_BUTTON(b), up ? key->shift : key->normal);
	}
	gtk_button_set_label(GTK_BUTTON(k->layout_btn), k->layout == L_ABNT2 ? "BR" : "US");
	GtkStyleContext *sc = gtk_widget_get_style_context(k->caps_btn);
	if (k->caps)
		gtk_style_context_add_class(sc, "big-wm-key-on");
	else
		gtk_style_context_remove_class(sc, "big-wm-key-on");
}

static void build_rows(kbd *k);

static void
on_key(GtkButton *b, kbd *k)
{
	const char *label = gtk_button_get_label(b);
	const char *role = g_object_get_data(G_OBJECT(b), "role");
	if (role && !strcmp(role, "backspace")) {
		k->dead[0] = 0;
		GtkEditable *e = GTK_EDITABLE(k->win->input_field);
		gint pos = gtk_editable_get_position(e);
		if (pos > 0)
			gtk_editable_delete_text(e, pos - 1, pos);
	} else if (role && !strcmp(role, "enter")) {
		k->dead[0] = 0;
		gtk_widget_activate(k->win->input_field); // o mesmo que Enter no campo
	} else if (role && !strcmp(role, "shift")) {
		k->shift = !k->shift;
	} else if (role && !strcmp(role, "caps")) {
		k->caps = !k->caps;
	} else if (role && !strcmp(role, "layout")) {
		k->layout = (k->layout + 1) % L_N;
		k->dead[0] = 0;
		build_rows(k);
	} else if (role && !strcmp(role, "space")) {
		if (k->dead[0]) {
			insert(k, k->dead);
			k->dead[0] = 0;
		}
		insert(k, " ");
	} else if (label) {
		if (is_dead(k->layout, label) && !k->dead[0]) {
			g_strlcpy(k->dead, label, sizeof k->dead);
		} else {
			const char *c = k->dead[0] ? compose(k->dead, label) : NULL;
			if (k->dead[0] && !c)
				insert(k, k->dead);
			insert(k, c ? c : label);
			k->dead[0] = 0;
		}
		if (k->shift)
			k->shift = FALSE; // Shift vale para uma tecla
	}
	relabel(k);
	gtk_entry_grab_focus_without_selecting(GTK_ENTRY(k->win->input_field));
}

static GtkWidget *
key_button(kbd *k, const char *label, const char *role, double width)
{
	GtkWidget *b = gtk_button_new_with_label(label);
	gtk_widget_set_can_focus(b, FALSE); // o foco fica no campo de senha
	gtk_widget_set_size_request(b, (int)(38 * width * prefs.size), (int)(38 * prefs.size));
	gtk_style_context_add_class(gtk_widget_get_style_context(b), "big-wm-key");
	if (role)
		g_object_set_data(G_OBJECT(b), "role", (gpointer)role);
	g_signal_connect(b, "clicked", G_CALLBACK(on_key), k);
	return b;
}

static void
build_rows(kbd *k)
{
	GList *children = gtk_container_get_children(GTK_CONTAINER(k->grid));
	for (GList *l = children; l; l = l->next)
		gtk_widget_destroy(l->data);
	g_list_free(children);
	g_ptr_array_set_size(k->keys, 0);
	const key_t_(*rows)[15] = k->layout == L_ABNT2 ? rows_abnt2 : rows_us;
	for (int r = 0; r < 4; r++) {
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
		for (int c = 0; rows[r][c].normal; c++) {
			const key_t_ *key = &rows[r][c];
			GtkWidget *b;
			if (!strcmp(key->normal, "⌫"))
				b = key_button(k, "⌫", "backspace", key->width);
			else if (!strcmp(key->normal, "⇧"))
				b = key_button(k, "⇧", "shift", key->width);
			else {
				b = key_button(k, key->normal, NULL, key->width);
				g_object_set_data(G_OBJECT(b), "key", (gpointer)key);
				g_ptr_array_add(k->keys, b);
			}
			gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0);
		}
		gtk_grid_attach(GTK_GRID(k->grid), row, 0, r, 1, 1);
	}
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
	k->caps_btn = key_button(k, "Caps", "caps", 1.6);
	k->layout_btn = key_button(k, "BR", "layout", 1.2);
	gtk_widget_set_tooltip_text(k->layout_btn, "ABNT2 / US");
	gtk_box_pack_start(GTK_BOX(row), k->caps_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), k->layout_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), key_button(k, " ", "space", 6), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), key_button(k, "⏎", "enter", 1.8), FALSE, FALSE, 0);
	gtk_grid_attach(GTK_GRID(k->grid), row, 0, 4, 1, 1);
	gtk_widget_show_all(k->grid);
	relabel(k);
}

static void
toggle(kbd *k)
{
	if (!k->revealer) {
		// Criado no primeiro uso: sem o botão, nada disso existe.
		k->keys = g_ptr_array_new();
		k->layout = default_layout();
		k->revealer = gtk_revealer_new();
		gtk_revealer_set_transition_type(GTK_REVEALER(k->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
		gtk_revealer_set_transition_duration(GTK_REVEALER(k->revealer), 120);
		gtk_widget_set_halign(k->revealer, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(k->revealer, GTK_ALIGN_END);
		gtk_widget_set_margin_bottom(k->revealer, 24);
		GtkWidget *frame = gtk_event_box_new();
		gtk_widget_set_name(frame, "big-wm-keyboard");
		if (prefs.dark)
			gtk_style_context_add_class(gtk_widget_get_style_context(frame), "big-wm-keyboard-dark");
		k->grid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(k->grid), 4);
		gtk_container_set_border_width(GTK_CONTAINER(k->grid), 8);
		gtk_container_add(GTK_CONTAINER(frame), k->grid);
		gtk_container_add(GTK_CONTAINER(k->revealer), frame);
		gtk_overlay_add_overlay(GTK_OVERLAY(k->win->overlay), k->revealer);
		build_rows(k);
		gtk_widget_show_all(k->revealer);
	}
	gboolean show = !gtk_revealer_get_reveal_child(GTK_REVEALER(k->revealer));
	gtk_revealer_set_reveal_child(GTK_REVEALER(k->revealer), show);
	// O diálogo sobe metade da altura do teclado: os dois cabem sem um
	// cobrir o outro (o diálogo fica centrado no espaço que sobra).
	if (k->win->window_box) {
		gint h = 0;
		gtk_widget_get_preferred_height(gtk_bin_get_child(GTK_BIN(k->revealer)), NULL, &h);
		gtk_widget_set_margin_bottom(k->win->window_box, show ? h + 24 : 0);
	}
	gtk_entry_grab_focus_without_selecting(GTK_ENTRY(k->win->input_field));
}

static void
on_icon(GtkEntry *e, GtkEntryIconPosition pos, GdkEvent *ev, kbd *k)
{
	(void)e;
	(void)ev;
	if (pos == GTK_ENTRY_ICON_SECONDARY)
		toggle(k);
}

static void
on_input_destroy(GtkWidget *w, kbd *k)
{
	(void)w;
	if (k->keys)
		g_ptr_array_free(k->keys, TRUE);
	g_free(k);
}

G_MODULE_EXPORT void
on_activation(struct GtkLock *gtklock, int id)
{
	(void)gtklock;
	self_id = (guint)id;
	const char *dir = g_getenv("BIGWM_LOCALEDIR");
	bindtextdomain("big-wm", dir && *dir ? dir : LOCALEDIR);
	bind_textdomain_codeset("big-wm", "UTF-8");
	load_prefs();
}

// Primeiro toque na tela (KeyboardTouch): abre o teclado.
static gboolean
on_touch(GtkWidget *w, GdkEvent *ev, kbd *k)
{
	(void)w;
	if (ev->type == GDK_TOUCH_BEGIN && (!k->revealer || !gtk_revealer_get_reveal_child(GTK_REVEALER(k->revealer))))
		toggle(k);
	return FALSE;
}

static gboolean
has_touchscreen(GtkWidget *w)
{
	GdkSeat *seat = gdk_display_get_default_seat(gtk_widget_get_display(w));
	return seat && (gdk_seat_get_capabilities(seat) & GDK_SEAT_CAPABILITY_TOUCH);
}

G_MODULE_EXPORT void
on_window_create(struct GtkLock *gtklock, struct Window *win)
{
	(void)gtklock;
	if (!win->input_field || !win->overlay || !GTK_IS_ENTRY(win->input_field))
		return;
	kbd *k = g_new0(kbd, 1);
	k->win = win;
	win->module_data[self_id] = k;
	if (prefs.button) {
		// ⌨ no fim do campo de senha: um ícone do próprio campo, que não
		// tira o foco dele.
		gtk_entry_set_icon_from_icon_name(GTK_ENTRY(win->input_field), GTK_ENTRY_ICON_SECONDARY,
						  "input-keyboard-symbolic");
		gtk_entry_set_icon_tooltip_text(GTK_ENTRY(win->input_field), GTK_ENTRY_ICON_SECONDARY,
						_("On-screen keyboard"));
		gtk_entry_set_icon_activatable(GTK_ENTRY(win->input_field), GTK_ENTRY_ICON_SECONDARY, TRUE);
		g_signal_connect(win->input_field, "icon-release", G_CALLBACK(on_icon), k);
	}
	g_signal_connect(win->input_field, "destroy", G_CALLBACK(on_input_destroy), k);
	if (prefs.touch && win->window) {
		gtk_widget_add_events(win->window, GDK_TOUCH_MASK);
		g_signal_connect(win->window, "touch-event", G_CALLBACK(on_touch), k);
		if (has_touchscreen(win->window))
			toggle(k);
	}
}
