// SPDX-License-Identifier: GPL-3.0-or-later
//
// Módulo do gtklock do Big WindowMaker no Wayland (gtklock -m
// .../big-wm-keyboard.so): a senha em qualquer monitor, o sinal da senha
// (PasswordMask) e o teclado na tela do big-keyboard.
//
// Segurança: tudo acontece dentro da própria superfície de bloqueio
// (ext-session-lock), no processo do gtklock. As teclas da tela escrevem
// direto no campo de senha do gtklock (gtk_editable_insert_text), como uma
// tecla física: nada de injeção de eventos, de outro processo, de linha de
// comando ou de log. O teclado não pega o foco, que fica no campo de senha.
// O PAM continua com o gtklock.
//
// Vários monitores: o gtklock cria uma janela por monitor e mostra o campo
// de senha só na janela "em foco" (a primeira). Aqui o campo aparece em
// todas, e a senha continua uma só: quando uma tecla chega a outra janela,
// ou ela é clicada ou tocada, o foco do gtklock passa para ela
// (gtklock_focus_window), que leva o texto digitado e deixa o campo antigo
// vazio. Enquanto o PAM confere a senha (campo desativado), o foco não muda
// e as outras janelas não recebem teclas.
//
// Teclado na tela: o mesmo do big-keyboard (lock-osk.c e bk/), desenhado
// numa área no pé de cada monitor, com um estado só (layout, Shift, Caps,
// painel). Botão ⌨ no fim do campo; com tela de toque e KeyboardTouch, já
// abre. Controle físico (big-wm-gamepad.c, lido aqui mesmo, só joysticks do
// udev): direcional move o cursor, analógicos apontam (gatilhos apertam), A
// aperta, X apaga, Y espaço, LB Shift, RB layout, L3 Caps, R3 símbolos,
// Start desbloqueia, B ou Select fecham.
//
// Controle na tela do big-keyboard: com a sessão bloqueada, só a superfície
// do bloqueio aparece, e o controle na tela do big-keyboard (layer-shell)
// fica atrás dela. Jogando pelo Moonlight, os toques do celular continuam
// indo para o big-keyboard e o controle virtual dele chega aqui como um
// controle físico, mas não se via o controle. Enquanto o big-keyboard o tem
// na tela, ele grava o layout em $XDG_RUNTIME_DIR/big-keyboard/
// lock-gamepad.ini (com o monitor e o PID), e este módulo o desenha por
// cima do bloqueio, no mesmo lugar (bk/bk-ctlpaint.c, copiado do
// big-keyboard), entre o fundo e a caixa de senha. Só o desenho: não é um
// widget, não recebe toques, nada do arquivo vira tecla ou senha. Um PID que não é mais o do big-keyboard
// anula o arquivo.
//
// Preferências (~/.config/big-wm/lock.conf, as mesmas do X11):
// VirtualKeyboard, VirtualKeyboardButton, KeyboardLayout (auto/br/us/
// us-intl), KeyboardSize (small/medium/large), KeyboardTheme (light/dark;
// sem ele, o modo do Big WindowMaker), KeyboardTouch, PasswordMask,
// OnScreenGamepad (mostrar o controle na tela do big-keyboard; padrão
// true).
//
// A estrutura Window e o gtklock_focus_window são os do gtklock 4.0 e 5.0
// (include/window.h e src/gtklock.c, GPL-3.0-or-later, Copyright (c) 2022
// Kenny Levinsen, Jovan Lanik, Bhaskar Khoraja; a Window não mudou no 5.0,
// que só acrescentou campos à GtkLock, aqui opaca). O gtklock encerra com
// um módulo de outra versão maior (o 5.0.1 sai sem bloquear), então o
// módulo declara a 5 e o pacote exige gtklock>=5.

#include "big-wm-gamepad.h"
#include "bk/bk-ctlpaint.h"
#include "lock-osk.h"

#include <glib-unix.h>
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
G_MODULE_EXPORT const guint module_major_version = 5;
G_MODULE_EXPORT const guint module_minor_version = 0;

// PasswordMask: o caractere que aparece no lugar de cada um digitado (0:
// nenhum). Os mesmos nomes do big-wm-lockscreen (X11).
static const struct {
	const char *name;
	gunichar ch;
} masks[] = {
	{ "square", 0x25A0 },  // ■ (padrão)
	{ "dot", 0x25CF },     // ●
	{ "bullet", 0x2022 },  // •
	{ "asterisk", '*' },
	{ "question", '?' },
	{ "diamond", 0x25C6 }, // ◆
	{ "none", 0 },
};

// Preferências do lock.conf.
static struct {
	gboolean vk, button, touch, dark, gamepad;
	int layout; // -1: o do sistema
	double size;
	gunichar mask;
} prefs = { TRUE, TRUE, TRUE, FALSE, TRUE, -1, 1, 0x25A0 };

static gboolean
conf_bool(GKeyFile *kf, const char *key, gboolean def)
{
	gchar *v = g_key_file_get_string(kf, "Lock", key, NULL);
	gboolean r = v ? g_strcmp0(v, "false") != 0 : def;
	g_free(v);
	return r;
}

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
		prefs.vk = conf_bool(kf, "VirtualKeyboard", TRUE);
		prefs.button = conf_bool(kf, "VirtualKeyboardButton", TRUE);
		prefs.touch = conf_bool(kf, "KeyboardTouch", TRUE);
		prefs.gamepad = conf_bool(kf, "OnScreenGamepad", TRUE);
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardTheme", NULL))) {
			if (!g_strcmp0(v, "dark") || !g_strcmp0(v, "light"))
				prefs.dark = g_strcmp0(v, "dark") == 0;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardLayout", NULL))) {
			prefs.layout = lock_osk_layout_from_conf(v);
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "KeyboardSize", NULL))) {
			prefs.size = !g_strcmp0(v, "small") ? 0.85 : !g_strcmp0(v, "large") ? 1.3 : 1;
			g_free(v);
		}
		if ((v = g_key_file_get_string(kf, "Lock", "PasswordMask", NULL))) {
			for (size_t i = 0; i < G_N_ELEMENTS(masks); i++)
				if (!g_strcmp0(v, masks[i].name))
					prefs.mask = masks[i].ch; // desconhecido: fica o padrão
			g_free(v);
		}
	}
	g_key_file_free(kf);
	g_free(path);
}

// Uma janela (monitor) do gtklock.
typedef struct {
	struct Window *win;
	GtkWidget *revealer; // o teclado (criado no primeiro uso)
	GtkWidget *area;
	GtkGesture *press;   // clique ou toque em qualquer lugar da janela
	GtkWidget *pad_host; // o GtkOverlay onde o controle é desenhado (ponteiro fraco)
	gulong pad_handler;
	gboolean pad_on;     // o controle do big-keyboard aparece neste monitor
} kbd;

static guint self_id;
static struct GtkLock *gtklock;
static void (*focus_window)(struct GtkLock *, struct Window *); // gtklock_focus_window
static GPtrArray *keyboards;   // kbd de cada monitor
static struct Window *focused; // a janela com a senha (a "em foco" do gtklock)
static gboolean hidden;        // gtklock --idle-hide escondeu os diálogos
static LockOsk osk;            // o teclado: um estado só para todos os monitores
static gboolean osk_ready;
static guint repeat_id, flash_id;

// ---- a senha em qualquer monitor -------------------------------------------

// O PAM está conferindo a senha (o gtklock desativa o campo): o texto não
// pode mudar de campo nem receber teclas (a thread do PAM o está lendo).
static gboolean
busy(void)
{
	return focused && focused->input_field && !gtk_widget_get_sensitive(focused->input_field);
}

static void
reveal_all(void)
{
	if (hidden)
		return;
	for (guint i = 0; keyboards && i < keyboards->len; i++) {
		kbd *k = g_ptr_array_index(keyboards, i);
		if (k->win->body_revealer)
			gtk_revealer_set_reveal_child(GTK_REVEALER(k->win->body_revealer), TRUE);
	}
}

// O ícone no fim do campo: o ⌨ (o gtklock põe o olho de mostrar a senha
// ao trocar de janela; aqui não há botão de mostrar a senha).
static void
set_icon(struct Window *w)
{
	GtkEntry *e = GTK_ENTRY(w->input_field);
	if (osk_ready && prefs.button) {
		gtk_entry_set_icon_from_icon_name(e, GTK_ENTRY_ICON_SECONDARY, "input-keyboard-symbolic");
		gtk_entry_set_icon_tooltip_text(e, GTK_ENTRY_ICON_SECONDARY, _("On-screen keyboard"));
		gtk_entry_set_icon_activatable(e, GTK_ENTRY_ICON_SECONDARY, TRUE);
	} else {
		gtk_entry_set_icon_from_icon_name(e, GTK_ENTRY_ICON_SECONDARY, NULL);
	}
	gtk_entry_set_visibility(e, FALSE);
}

// Uma senha só: o que tiver ficado num campo que não é o da janela em foco
// (um monitor desligado no meio, por exemplo) vai para ela, ou é apagado se
// ela já tem texto.
static void
consolidate(void)
{
	if (!focused || busy())
		return;
	GtkEntry *dst = GTK_ENTRY(focused->input_field);
	for (guint i = 0; keyboards && i < keyboards->len; i++) {
		kbd *k = g_ptr_array_index(keyboards, i);
		if (k->win == focused)
			continue;
		GtkEntry *src = GTK_ENTRY(k->win->input_field);
		if (gtk_entry_get_text_length(src) == 0)
			continue;
		if (gtk_entry_get_text_length(dst) == 0) {
			gtk_entry_set_text(dst, gtk_entry_get_text(src));
			gtk_editable_set_position(GTK_EDITABLE(dst), -1);
		}
		gtk_entry_set_text(src, "");
	}
}

// Leva o foco do gtklock (e o texto da senha) para a janela w.
static void
focus_to(struct Window *w)
{
	if (!w || w == focused || busy())
		return;
	if (focus_window && gtklock)
		focus_window(gtklock, w); // chama on_focus_change
}

// Uma tecla física chegou a esta janela (o compositor deu o teclado a este
// monitor): a senha vem para cá antes da tecla. Com o PAM conferindo, as
// outras janelas não recebem teclas.
static gboolean
on_window_key(GtkWidget *w, GdkEventKey *ev, kbd *k)
{
	(void)w;
	(void)ev;
	if (k->win != focused && busy())
		return TRUE;
	focus_to(k->win);
	return FALSE;
}

// Clique ou toque em qualquer lugar da janela (fase de captura: antes do
// campo, do botão Desbloquear ou do teclado).
static void
on_window_press(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, kbd *k)
{
	(void)g;
	(void)n;
	(void)x;
	(void)y;
	focus_to(k->win);
}

// ---- teclado na tela --------------------------------------------------------

static void
queue_draw_all(void)
{
	for (guint i = 0; keyboards && i < keyboards->len; i++) {
		kbd *k = g_ptr_array_index(keyboards, i);
		if (k->area)
			gtk_widget_queue_draw(k->area);
	}
}

// O que as teclas escrevem vai para o campo da janela em foco.
static GtkEditable *
field(void)
{
	if (!focused || busy() || !focused->input_field)
		return NULL;
	return GTK_EDITABLE(focused->input_field);
}

static void
sink_text(void *data, const char *s)
{
	(void)data;
	GtkEditable *e = field();
	if (!e)
		return;
	gint pos = gtk_editable_get_position(e);
	gtk_editable_insert_text(e, s, (gint)strlen(s), &pos);
	gtk_editable_set_position(e, pos);
}

static void
sink_backspace(void *data)
{
	(void)data;
	GtkEditable *e = field();
	if (!e)
		return;
	gint pos = gtk_editable_get_position(e);
	if (pos > 0)
		gtk_editable_delete_text(e, pos - 1, pos);
}

static void
sink_enter(void *data)
{
	(void)data;
	GtkEditable *e = field();
	if (e)
		gtk_widget_activate(GTK_WIDGET(e)); // o mesmo que Enter no campo
}

static const LockOskSink sink = { sink_text, sink_backspace, sink_enter };

static gboolean
on_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
	(void)w;
	(void)data;
	lock_osk_draw(&osk, cr, 0, 0, _("On-screen keyboard"), bwg_now_ms());
	return TRUE;
}

static void build(kbd *k);

// Abre ou fecha o teclado em todos os monitores. O diálogo sobe a altura do
// teclado: os dois cabem sem um cobrir o outro.
static void
set_shown(gboolean show)
{
	osk.shown = show;
	for (guint i = 0; keyboards && i < keyboards->len; i++) {
		kbd *k = g_ptr_array_index(keyboards, i);
		if (show && !k->revealer)
			build(k);
		if (!k->revealer)
			continue;
		gtk_revealer_set_reveal_child(GTK_REVEALER(k->revealer), show);
		if (k->win->window_box)
			gtk_widget_set_margin_bottom(k->win->window_box, show ? (int)osk.height + 24 : 0);
	}
	if (!show) {
		osk.pressed = -1;
		if (repeat_id) {
			g_source_remove(repeat_id);
			repeat_id = 0;
		}
	}
	if (focused && !busy())
		gtk_entry_grab_focus_without_selecting(GTK_ENTRY(focused->input_field));
	queue_draw_all();
}

static void
apply(int r)
{
	if (r & LOCK_OSK_HIDE)
		set_shown(FALSE);
	else if (r & LOCK_OSK_SHOW)
		set_shown(TRUE);
	else if (r & LOCK_OSK_REDRAW)
		queue_draw_all();
}

static gboolean
on_repeat(gpointer data)
{
	(void)data;
	repeat_id = 0;
	if (osk.pressed < 0)
		return G_SOURCE_REMOVE;
	apply(lock_osk_repeat(&osk, &sink, NULL));
	if (osk.pressed >= 0 && lock_osk_repeatable(&osk))
		repeat_id = g_timeout_add(LOCK_OSK_REPEAT_RATE, on_repeat, NULL);
	return G_SOURCE_REMOVE;
}

static void
key_down(kbd *k, double x, double y)
{
	focus_to(k->win); // a senha vem para o monitor tocado
	if (repeat_id)
		g_source_remove(repeat_id);
	repeat_id = 0;
	apply(lock_osk_down(&osk, lock_osk_hit(&osk, x, y)));
	if (lock_osk_repeatable(&osk))
		repeat_id = g_timeout_add(LOCK_OSK_REPEAT_DELAY, on_repeat, NULL);
}

static void
key_up(double x, double y, gboolean cancel)
{
	if (repeat_id)
		g_source_remove(repeat_id);
	repeat_id = 0;
	apply(lock_osk_up(&osk, cancel ? -1 : lock_osk_hit(&osk, x, y), &sink, NULL));
	if (focused && !busy())
		gtk_entry_grab_focus_without_selecting(GTK_ENTRY(focused->input_field));
}

static gboolean
on_button(GtkWidget *w, GdkEventButton *ev, kbd *k)
{
	(void)w;
	// Um toque chega como toque (on_area_touch); o clique emulado dele não conta.
	if (ev->button != 1 || gdk_event_get_pointer_emulated((GdkEvent *)ev))
		return TRUE;
	if (ev->type == GDK_BUTTON_PRESS)
		key_down(k, ev->x, ev->y);
	else if (ev->type == GDK_BUTTON_RELEASE)
		key_up(ev->x, ev->y, FALSE);
	return TRUE;
}

static GdkEventSequence *touch_seq; // o dedo que está apertando uma tecla

static gboolean
on_area_touch(GtkWidget *w, GdkEventTouch *ev, kbd *k)
{
	(void)w;
	if (ev->type == GDK_TOUCH_BEGIN && !touch_seq) {
		touch_seq = ev->sequence;
		key_down(k, ev->x, ev->y);
	} else if (ev->sequence == touch_seq && (ev->type == GDK_TOUCH_END || ev->type == GDK_TOUCH_CANCEL)) {
		touch_seq = NULL;
		key_up(ev->x, ev->y, ev->type == GDK_TOUCH_CANCEL);
	}
	return TRUE;
}

static void
build(kbd *k)
{
	if (k->revealer || !k->win->overlay)
		return;
	k->revealer = gtk_revealer_new();
	gtk_revealer_set_transition_type(GTK_REVEALER(k->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
	gtk_revealer_set_transition_duration(GTK_REVEALER(k->revealer), 120);
	gtk_widget_set_halign(k->revealer, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(k->revealer, GTK_ALIGN_END);
	gtk_widget_set_margin_bottom(k->revealer, 24);
	k->area = gtk_drawing_area_new();
	gtk_widget_set_name(k->area, "big-wm-keyboard");
	gtk_widget_set_can_focus(k->area, FALSE); // o foco fica no campo de senha
	// a moldura e a sombra (6 px, como a do X11)
	gtk_widget_set_size_request(k->area, (int)osk.width + 6, (int)osk.height + 6);
	gtk_widget_add_events(k->area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_TOUCH_MASK);
	g_signal_connect(k->area, "draw", G_CALLBACK(on_draw), NULL);
	g_signal_connect(k->area, "button-press-event", G_CALLBACK(on_button), k);
	g_signal_connect(k->area, "button-release-event", G_CALLBACK(on_button), k);
	g_signal_connect(k->area, "touch-event", G_CALLBACK(on_area_touch), k);
	gtk_container_add(GTK_CONTAINER(k->revealer), k->area);
	gtk_overlay_add_overlay(GTK_OVERLAY(k->win->overlay), k->revealer);
	gtk_widget_show_all(k->revealer);
}

static void
on_icon(GtkEntry *e, GtkEntryIconPosition pos, GdkEvent *ev, kbd *k)
{
	(void)e;
	(void)ev;
	if (pos != GTK_ENTRY_ICON_SECONDARY)
		return;
	focus_to(k->win);
	set_shown(!osk.shown);
}

// Primeiro toque na tela (KeyboardTouch): abre o teclado.
static gboolean
on_touch(GtkWidget *w, GdkEvent *ev, kbd *k)
{
	(void)w;
	(void)k;
	if (ev->type == GDK_TOUCH_BEGIN && osk_ready && !osk.shown)
		set_shown(TRUE);
	return FALSE;
}

static gboolean
has_touchscreen(GtkWidget *w)
{
	GdkSeat *seat = gdk_display_get_default_seat(gtk_widget_get_display(w));
	return seat && (gdk_seat_get_capabilities(seat) & GDK_SEAT_CAPABILITY_TOUCH);
}

// ---- controle físico --------------------------------------------------------

static bwg pad;
static GArray *pad_sources; // guint: uma fonte do laço por descritor
static guint pad_repeat_id;
static unsigned pad_generation = ~0u;

static gboolean
on_flash(gpointer data)
{
	(void)data;
	flash_id = 0;
	int t = lock_osk_flash_ms(&osk, bwg_now_ms());
	if (t > 0)
		flash_id = g_timeout_add((guint)t, on_flash, NULL);
	else
		queue_draw_all();
	return G_SOURCE_REMOVE;
}

static void
pad_action(enum bwg_action a)
{
	if (!osk_ready || !focused)
		return;
	apply(lock_osk_pad(&osk, a, &sink, NULL, bwg_now_ms()));
	if (osk.flash >= 0 && !flash_id)
		flash_id = g_timeout_add(LOCK_OSK_FLASH_MS, on_flash, NULL);
}

static void pad_watch(void);

static gboolean
on_pad_repeat(gpointer data)
{
	(void)data;
	pad_repeat_id = 0;
	enum bwg_action out[4];
	int n = bwg_tick(&pad, bwg_now_ms(), out, 4);
	for (int i = 0; i < n; i++)
		pad_action(out[i]);
	pad_watch();
	return G_SOURCE_REMOVE;
}

static gboolean
on_pad_fd(gint fd, GIOCondition cond, gpointer data)
{
	(void)cond;
	(void)data;
	enum bwg_action out[BWG_MAX_ACTIONS];
	int n = bwg_handle(&pad, fd, bwg_now_ms(), out, BWG_MAX_ACTIONS);
	for (int i = 0; i < n; i++)
		pad_action(out[i]);
	if (osk_ready)
		apply(lock_osk_sticks(&osk, bwg_active(&pad)));
	pad_watch(); // controles que entraram ou saíram, prazo da repetição
	return G_SOURCE_CONTINUE;
}

// (Re)cria as fontes do laço do GLib quando a lista de controles muda; o
// prazo da repetição de uma direção segurada, sem polling.
static void
pad_watch(void)
{
	if (pad.generation != pad_generation) {
		pad_generation = pad.generation;
		for (guint i = 0; pad_sources && i < pad_sources->len; i++)
			g_source_remove(g_array_index(pad_sources, guint, i));
		if (!pad_sources)
			pad_sources = g_array_new(FALSE, FALSE, sizeof(guint));
		g_array_set_size(pad_sources, 0);
		int fds[BWG_MAX_PADS + 1];
		int n = bwg_fds(&pad, fds, G_N_ELEMENTS(fds));
		for (int i = 0; i < n; i++) {
			guint id = g_unix_fd_add(fds[i], G_IO_IN | G_IO_HUP | G_IO_ERR, on_pad_fd, NULL);
			g_array_append_val(pad_sources, id);
		}
	}
	if (pad_repeat_id) {
		g_source_remove(pad_repeat_id);
		pad_repeat_id = 0;
	}
	int t = bwg_timeout(&pad, bwg_now_ms());
	if (t >= 0)
		pad_repeat_id = g_timeout_add((guint)t, on_pad_repeat, NULL);
}

// ---- controle na tela do big-keyboard ---------------------------------------

static struct {
	BkCtlLayout lay;
	BkCtlState st;   // sempre vazio: nenhum dedo aqui, o desenho parado
	gboolean on;     // o big-keyboard tem o controle na tela
	gboolean has_mon;
	GdkRectangle mon; // o monitor dele (geometria lógica); sem ele, todos
	char pid[16]; // só dígitos (o do big-keyboard), "" nenhum
	GFileMonitor *watch;
	guint reload_id, alive_id;
} mirror;

static char *
mirror_path(void)
{
	return g_build_filename(g_get_user_runtime_dir(), "big-keyboard", "lock-gamepad.ini", NULL);
}

// O processo que gravou o arquivo ainda é o big-keyboard?
static gboolean
mirror_alive(const char *pid)
{
	if (!*pid)
		return FALSE;
	char *path = g_build_filename("/proc", pid, "comm", NULL), *comm = NULL;
	gboolean ok = g_file_get_contents(path, &comm, NULL, NULL) && g_str_has_prefix(comm, "big-keyboard");
	g_free(comm);
	g_free(path);
	return ok;
}

// O Pid do arquivo, só se for um número (nada de caminhos em /proc).
static void
mirror_set_pid(const char *v)
{
	mirror.pid[0] = 0;
	size_t n = v ? strlen(v) : 0;
	if (n == 0 || n >= sizeof mirror.pid || v[0] == '0')
		return;
	for (size_t i = 0; i < n; i++)
		if (!g_ascii_isdigit(v[i]))
			return;
	memcpy(mirror.pid, v, n + 1);
}

static void
mirror_apply(void)
{
	for (guint i = 0; keyboards && i < keyboards->len; i++) {
		kbd *k = g_ptr_array_index(keyboards, i);
		if (!k->pad_host)
			continue;
		gboolean here = mirror.on;
		if (here && mirror.has_mon && k->win->monitor) {
			GdkRectangle g;
			gdk_monitor_get_geometry(k->win->monitor, &g);
			here = gdk_rectangle_equal(&g, &mirror.mon);
		}
		if (here != k->pad_on || here)
			gtk_widget_queue_draw(k->pad_host);
		k->pad_on = here;
	}
}

static gboolean on_mirror_alive(gpointer data);

static void
mirror_load(void)
{
	char *path = mirror_path();
	GKeyFile *kf = g_key_file_new();
	gboolean on = FALSE;
	if (prefs.gamepad && g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
		gchar *pid = g_key_file_get_string(kf, "Mirror", "Pid", NULL);
		mirror_set_pid(pid);
		g_free(pid);
		gsize n = 0;
		gint *geo = g_key_file_get_integer_list(kf, "Mirror", "Monitor", &n, NULL);
		mirror.has_mon = geo && n == 4 && geo[2] > 0 && geo[3] > 0;
		if (mirror.has_mon)
			mirror.mon = (GdkRectangle){ geo[0], geo[1], geo[2], geo[3] };
		g_free(geo);
		on = mirror_alive(mirror.pid) && bk_ctl_load(&mirror.lay, path, NULL);
	}
	g_key_file_free(kf);
	g_free(path);
	mirror.on = on;
	bk_ctl_state_init(&mirror.st);
	// o big-keyboard caiu sem apagar o arquivo: o controle some
	if (on && !mirror.alive_id)
		mirror.alive_id = g_timeout_add_seconds(2, on_mirror_alive, NULL);
	mirror_apply();
}

static gboolean
on_mirror_alive(gpointer data)
{
	(void)data;
	if (mirror.on && mirror_alive(mirror.pid))
		return G_SOURCE_CONTINUE;
	mirror.alive_id = 0;
	mirror.on = FALSE;
	mirror_apply();
	return G_SOURCE_REMOVE;
}

static gboolean
on_mirror_reload(gpointer data)
{
	(void)data;
	mirror.reload_id = 0;
	mirror_load();
	return G_SOURCE_REMOVE;
}

// O arquivo é gravado por troca atômica: espera a rajada de eventos acabar.
static void
on_mirror_changed(GFileMonitor *m, GFile *f, GFile *other, GFileMonitorEvent ev, gpointer data)
{
	(void)m;
	(void)f;
	(void)other;
	(void)ev;
	(void)data;
	if (mirror.reload_id)
		g_source_remove(mirror.reload_id);
	mirror.reload_id = g_timeout_add(100, on_mirror_reload, NULL);
}

static void
mirror_start(void)
{
	if (!prefs.gamepad || mirror.watch)
		return;
	char *path = mirror_path();
	GFile *f = g_file_new_for_path(path);
	mirror.watch = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, NULL);
	if (mirror.watch)
		g_signal_connect(mirror.watch, "changed", G_CALLBACK(on_mirror_changed), NULL);
	g_object_unref(f);
	g_free(path);
	mirror_load();
}

// Antes do desenho do GtkOverlay (que desenha os filhos): o controle fica
// sobre o fundo da janela e abaixo da caixa de senha, do teclado e de
// tudo o mais. Não é um widget, então não recebe toque nem clique.
static gboolean
on_pad_draw(GtkWidget *w, cairo_t *cr, kbd *k)
{
	if (!mirror.on || !k->pad_on)
		return FALSE;
	double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
	for (int i = 0; i < mirror.lay.n; i++)
		if (mirror.lay.c[i].enabled)
			bk_ctl_paint(cr, &mirror.lay, &mirror.st, W, H, i, 0, 0);
	return FALSE;
}

// O controle do big-keyboard no GtkOverlay da janela (a tela inteira).
static void
pad_build(kbd *k)
{
	if (!prefs.gamepad || k->pad_host || !k->win->overlay)
		return;
	k->pad_host = k->win->overlay;
	g_object_add_weak_pointer(G_OBJECT(k->pad_host), (gpointer *)&k->pad_host);
	k->pad_handler = g_signal_connect(k->pad_host, "draw", G_CALLBACK(on_pad_draw), k);
}

// ---- ganchos do gtklock -----------------------------------------------------

static int
default_layout(void)
{
	if (prefs.layout >= 0)
		return prefs.layout;
	// O layout XKB do sistema (o mesmo do X11 e do wlmaker), senão o idioma.
	gchar *conf = NULL;
	const char *xkb = NULL;
	if (g_file_get_contents("/etc/X11/xorg.conf.d/00-keyboard.conf", &conf, NULL, NULL))
		xkb = strstr(conf, "\"br") ? "br" : "us";
	int l = lock_osk_layout_auto(xkb, g_getenv("LANG"));
	g_free(conf);
	return l;
}

G_MODULE_EXPORT void
on_activation(struct GtkLock *g, int id)
{
	gtklock = g;
	self_id = (guint)id;
	const char *dir = g_getenv("BIGWM_LOCALEDIR");
	bindtextdomain("big-wm", dir && *dir ? dir : LOCALEDIR);
	bind_textdomain_codeset("big-wm", "UTF-8");
	load_prefs();
	keyboards = g_ptr_array_new();
	// gtklock_focus_window vem do próprio gtklock (exportado para os
	// módulos); sem ele, cada monitor mostra o campo, mas o texto não muda
	// de lugar.
	GModule *self = g_module_open(NULL, 0);
	if (self) {
		gpointer fn = NULL;
		if (g_module_symbol(self, "gtklock_focus_window", &fn))
			focus_window = (void (*)(struct GtkLock *, struct Window *))fn;
		g_module_close(self);
	}
	if (prefs.vk) {
		lock_osk_init(&osk, default_layout(), prefs.dark, prefs.size);
		osk_ready = TRUE;
		if (bwg_open(&pad) >= 0) {
			pad.no_stick_nav = 1; // os analógicos apontam (modo apontar)
			pad_watch();
		}
	}
	mirror_start();
}

G_MODULE_EXPORT void
on_focus_change(struct GtkLock *g, struct Window *win, struct Window *old)
{
	(void)g;
	focused = win;
	// O gtklock esconde o campo da janela que perdeu o foco: volta.
	reveal_all();
	set_icon(win);
	if (old && old != win) {
		set_icon(old);
		if (old->error_label && win->error_label) {
			// "Login failed" acompanha a senha
			gtk_label_set_text(GTK_LABEL(win->error_label), gtk_label_get_text(GTK_LABEL(old->error_label)));
			gtk_label_set_text(GTK_LABEL(old->error_label), "");
		}
	}
	consolidate();
}

G_MODULE_EXPORT void
on_output_change(struct GtkLock *g)
{
	(void)g;
	reveal_all();
	consolidate();
	mirror_apply(); // monitores que entraram ou mudaram de lugar
}

G_MODULE_EXPORT void
on_idle_hide(struct GtkLock *g)
{
	(void)g;
	hidden = TRUE;
}

G_MODULE_EXPORT void
on_idle_show(struct GtkLock *g)
{
	(void)g;
	hidden = FALSE;
	reveal_all();
}

G_MODULE_EXPORT void
on_window_destroy(struct GtkLock *g, struct Window *win)
{
	(void)g;
	if (win == focused)
		focused = NULL; // o gtklock passa o texto para outra janela
	if (osk_ready && keyboards && keyboards->len <= 1)
		lock_osk_clear(&osk); // a última janela: a tecla morta pendente é parte da senha
}

static void
on_input_destroy(GtkWidget *w, kbd *k)
{
	(void)w;
	if (keyboards)
		g_ptr_array_remove(keyboards, k);
	if (k->press)
		g_object_unref(k->press);
	if (k->pad_host) {
		g_signal_handler_disconnect(k->pad_host, k->pad_handler);
		g_object_remove_weak_pointer(G_OBJECT(k->pad_host), (gpointer *)&k->pad_host);
	}
	g_free(k);
}

G_MODULE_EXPORT void
on_window_create(struct GtkLock *g, struct Window *win)
{
	(void)g;
	if (!win->input_field || !GTK_IS_ENTRY(win->input_field))
		return;
	kbd *k = g_new0(kbd, 1);
	k->win = win;
	win->module_data[self_id] = k;
	g_ptr_array_add(keyboards, k);
	g_signal_connect(win->input_field, "destroy", G_CALLBACK(on_input_destroy), k);
	// o controle do big-keyboard, abaixo da caixa de senha e do teclado
	pad_build(k);
	mirror_apply();
	// O sinal da senha (PasswordMask); 0 = nada aparece.
	gtk_entry_set_invisible_char(GTK_ENTRY(win->input_field), prefs.mask);
	// O campo de senha em todos os monitores, com uma senha só.
	if (win->body_revealer && !hidden)
		gtk_revealer_set_reveal_child(GTK_REVEALER(win->body_revealer), TRUE);
	if (win->window) {
		g_signal_connect(win->window, "key-press-event", G_CALLBACK(on_window_key), k);
		k->press = gtk_gesture_multi_press_new(win->window);
		gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(k->press), GTK_PHASE_CAPTURE);
		g_signal_connect(k->press, "pressed", G_CALLBACK(on_window_press), k);
	}
	set_icon(win);
	if (!osk_ready)
		return;
	if (prefs.button)
		g_signal_connect(win->input_field, "icon-release", G_CALLBACK(on_icon), k);
	if (osk.shown) // um monitor ligado com o teclado aberto
		set_shown(TRUE);
	if (prefs.touch && win->window) {
		gtk_widget_add_events(win->window, GDK_TOUCH_MASK);
		g_signal_connect(win->window, "touch-event", G_CALLBACK(on_touch), k);
		if (has_touchscreen(win->window) && !osk.shown)
			set_shown(TRUE);
	}
}
