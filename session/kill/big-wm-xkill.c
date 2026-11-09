/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * big-wm-xkill — forçar o fechamento de um aplicativo travado (X11), como o
 * xkill, com a caveira vermelha do Big WindowMaker.
 *
 *   big-wm-xkill       o ponteiro vira a caveira; clique (botão esquerdo)
 *                      na janela do aplicativo: o cliente X dela é
 *                      encerrado (XKillClient) e o processo dele recebe
 *                      SIGKILL. Esc, outro botão ou Ctrl+Alt+Esc de novo
 *                      cancelam.
 *
 * O xkill aceita -button, mas o cursor dele é fixo (a caveira do tema). Aqui
 * o cursor vem do tema "big-wm-skull" (imagem "pirate"), com a do tema
 * atual como reserva.
 *
 * Encerra o cliente daquela janela, não processos por nome: as outras
 * janelas de um navegador com vários processos só vão junto se forem do
 * mesmo cliente. O PID vem do servidor (XRes, a conexão do cliente), não de
 * _NET_WM_PID, que o próprio programa escreve. São recusados (com um
 * aviso): o gerenciador de janelas (o Dock, o Clip, os menus e os ícones são
 * janelas dele), o servidor X, os serviços do sistema e da sessão, os
 * componentes do Big WindowMaker e os dockapps.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XRes.h>

/* ---- processos protegidos ------------------------------------------------- */

/* Pelo nome exato (comm, executável ou script). */
static const char *protected_names[] = {
    "wmaker", "WindowMaker", "Xorg", "X", "Xwayland", "init", "dbus-daemon", "dbus-broker",
    "dbus-broker-lau", "pipewire", "pipewire-pulse", "wireplumber", "pulseaudio", "lightdm",
    "sddm", "gdm", "login", "polkitd", "gnome-keyring-d", "xsettingsd", "dunst", "picom",
    "xss-lock", "xsecurelock", "startbigwm", "wlmaker", NULL,
};

/* Pelo começo: componentes, serviços, dockapps. */
static const char *protected_prefixes[] = {
    "big-wm", "wlm", "wm", "systemd", "dbus-", "pipewire", "xdg-desktop-por", "xdg-document-po",
    "xdg-permission-", NULL,
};

static bool protected_name(const char *name)
{
    if (!name || !*name)
        return false;
    for (const char **n = protected_names; *n; n++)
        if (!strcmp(*n, name))
            return true;
    for (const char **p = protected_prefixes; *p; p++)
        if (!strncmp(*p, name, strlen(*p)))
            return true;
    return false;
}

static ssize_t read_proc(pid_t pid, const char *what, char *buf, size_t len)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/%s", (int)pid, what);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, len - 1, f);
    fclose(f);
    buf[n] = 0;
    return (ssize_t)n;
}

static pid_t parent_of(pid_t pid)
{
    char buf[512];
    if (read_proc(pid, "stat", buf, sizeof buf) <= 0)
        return 0;
    const char *p = strrchr(buf, ')');
    int ppid = 0;
    if (!p || sscanf(p + 1, " %*c %d", &ppid) != 1)
        return 0;
    return ppid;
}

/* O processo é da sessão (ou de outro usuário, ou um ancestral deste)? */
static bool protected_pid(pid_t pid)
{
    if (pid <= 1 || pid == getpid())
        return true;
    for (pid_t p = getppid(); p > 1; p = parent_of(p))
        if (p == pid)
            return true;
    char path[64], buf[PATH_MAX];
    struct stat st;
    snprintf(path, sizeof path, "/proc/%d", (int)pid);
    if (stat(path, &st) != 0 || st.st_uid != getuid())
        return true;
    if (read_proc(pid, "comm", buf, sizeof buf) > 0) {
        buf[strcspn(buf, "\n")] = 0;
        if (protected_name(buf))
            return true;
    }
    snprintf(path, sizeof path, "/proc/%d/exe", (int)pid);
    ssize_t n = readlink(path, buf, sizeof buf - 1);
    if (n > 0) {
        buf[n] = 0;
        const char *b = strrchr(buf, '/');
        if (protected_name(b ? b + 1 : buf))
            return true;
    }
    /* scripts: os primeiros argumentos do interpretador */
    n = read_proc(pid, "cmdline", buf, sizeof buf);
    for (ssize_t i = 0, arg = 0; n > 0 && i < n && arg < 3; arg++) {
        const char *a = &buf[i];
        const char *b = strrchr(a, '/');
        if (*a != '-' && protected_name(b ? b + 1 : a))
            return true;
        i += (ssize_t)strlen(a) + 1;
    }
    return false;
}

/* ---- textos ---------------------------------------------------------------- */

static bool pt(void)
{
    const char *l = getenv("LC_ALL");
    if (!l || !*l)
        l = getenv("LC_MESSAGES");
    if (!l || !*l)
        l = getenv("LANG");
    return l && !strncmp(l, "pt", 2);
}

static void notice(const char *icon, const char *msg)
{
    const char *title = pt() ? "Forçar o fechamento" : "Force close";
    fprintf(stderr, "big-wm-xkill: %s\n", msg);
    pid_t p = fork();
    if (p == 0) {
        execlp("notify-send", "notify-send", "-a", title, "-i", icon ? icon : "process-stop", title, msg,
               (char *)NULL);
        _exit(0);
    }
}

/* ---- X11 ------------------------------------------------------------------- */

static Display *dpy;
static Window root;

static int ignore_errors(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

/* WM_STATE da janela: -1 sem ela. */
static long wm_state(Window w)
{
    Atom a = XInternAtom(dpy, "WM_STATE", False), type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    long st = -1;
    if (XGetWindowProperty(dpy, w, a, 0, 2, False, a, &type, &fmt, &n, &after, &data) == Success && data) {
        if (n >= 1 && fmt == 32)
            st = (long)((unsigned long *)data)[0];
        XFree(data);
    }
    return st;
}

/* A janela do cliente dentro do quadro (como XmuClientWindow): a primeira
 * com WM_STATE, em largura. None se não há (janela do próprio WM). */
static Window client_window(Window w, int depth)
{
    if (wm_state(w) >= 0)
        return w;
    if (depth > 6)
        return None;
    Window r, parent, *kids = NULL;
    unsigned int n = 0;
    Window found = None;
    if (!XQueryTree(dpy, w, &r, &parent, &kids, &n))
        return None;
    for (unsigned int i = n; i > 0 && found == None; i--)
        if (wm_state(kids[i - 1]) >= 0)
            found = kids[i - 1];
    for (unsigned int i = n; i > 0 && found == None; i--)
        found = client_window(kids[i - 1], depth + 1);
    if (kids)
        XFree(kids);
    return found;
}

/* PID do cliente dono da janela, pelo servidor (XRes); 0 se não sabe. */
static pid_t client_pid(Window w)
{
    int ev, er;
    if (!XResQueryExtension(dpy, &ev, &er))
        return 0;
    XResClientIdSpec spec = {.client = w, .mask = XRES_CLIENT_ID_PID_MASK};
    long n = 0;
    XResClientIdValue *vals = NULL;
    pid_t pid = 0;
    if (XResQueryClientIds(dpy, 1, &spec, &n, &vals) == Success) {
        for (long i = 0; i < n; i++)
            if (XResGetClientPid(&vals[i]) > 0)
                pid = XResGetClientPid(&vals[i]);
        XResClientIdsDestroy(n, vals);
    }
    return pid;
}

/* Nome legível da janela: título, senão a classe. */
static void window_name(Window w, char *out, size_t len)
{
    *out = 0;
    XTextProperty tp;
    if (XGetWMName(dpy, w, &tp) && tp.value) {
        snprintf(out, len, "%s", (char *)tp.value);
        XFree(tp.value);
    }
    if (!*out) {
        XClassHint ch;
        if (XGetClassHint(dpy, w, &ch)) {
            snprintf(out, len, "%s", ch.res_class ? ch.res_class : ch.res_name ? ch.res_name : "");
            if (ch.res_name)
                XFree(ch.res_name);
            if (ch.res_class)
                XFree(ch.res_class);
        }
    }
}

static void icon_name(Window w, char *out, size_t len)
{
    *out = 0;
    XClassHint ch;
    if (XGetClassHint(dpy, w, &ch)) {
        snprintf(out, len, "%s", ch.res_name ? ch.res_name : "");
        if (ch.res_name)
            XFree(ch.res_name);
        if (ch.res_class)
            XFree(ch.res_class);
    }
}

static Cursor skull_cursor(void)
{
    int size = XcursorGetDefaultSize(dpy);
    XcursorImages *imgs = XcursorLibraryLoadImages("pirate", "big-wm-skull", size);
    Cursor c = None;
    if (imgs) {
        c = XcursorImagesLoadCursor(dpy, imgs);
        XcursorImagesDestroy(imgs);
    }
    return c != None ? c : XCreateFontCursor(dpy, XC_pirate);
}

/* Encerra o cliente da janela clicada (sub: o filho da raiz sob o
 * ponteiro). 0 encerrou, 1 recusou. */
static int kill_at(Window sub)
{
    if (sub == None) {
        notice(NULL, pt() ? "Isto não é a janela de um aplicativo. Tecle Ctrl+Alt+Esc e clique na janela "
                            "do aplicativo que quer fechar."
                          : "This is not the window of an application. Press Ctrl+Alt+Escape and click "
                            "on the window of the application to close.");
        return 1;
    }
    Window client = client_window(sub, 0);
    long st = client != None ? wm_state(client) : -1;
    if (client == None || st == WithdrawnState) {
        /* o Dock, o Clip, um menu, um ícone: janelas do gerenciador; ou um
         * dockapp (retirado, no Dock) */
        notice(NULL, pt() ? "Isto faz parte da sessão e não é fechado à força."
                          : "This is part of the session, and is not closed by force.");
        return 1;
    }
    char name[256], icon[128], msg[512];
    window_name(client, name, sizeof name);
    icon_name(client, icon, sizeof icon);
    pid_t pid = client_pid(client);
    if (pid > 0 && protected_pid(pid)) {
        snprintf(msg, sizeof msg,
                 pt() ? "“%s” faz parte da sessão e não é fechado à força."
                      : "“%s” is part of the session, and is not closed by force.",
                 *name ? name : "?");
        notice(*icon ? icon : NULL, msg);
        return 1;
    }
    fprintf(stderr, "big-wm-xkill: encerrando \"%s\" (janela 0x%lx, PID %d)\n", name, client, (int)pid);
    XKillClient(dpy, client);
    XSync(dpy, False);
    if (pid > 0 && kill(pid, SIGKILL) != 0 && errno != ESRCH)
        perror("big-wm-xkill: kill");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf("uso: big-wm-xkill   (clique na janela do aplicativo travado; Esc cancela)\n");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--protected")) {
        /* para os testes: big-wm-xkill --protected NOME... */
        for (int i = 2; i < argc; i++)
            printf("%s %s\n", argv[i], protected_name(argv[i]) ? "protegido" : "livre");
        return 0;
    }
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "big-wm-xkill: sem display X11\n");
        return 1;
    }
    root = DefaultRootWindow(dpy);
    XSetErrorHandler(ignore_errors);
    Cursor cur = skull_cursor();

    /* como o xkill: o ponteiro pode estar preso por um instante (o atalho
     * acabou de ser apertado) */
    int r = GrabNotViewable;
    for (int i = 0; i < 20; i++) {
        r = XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None,
                         cur, CurrentTime);
        if (r == GrabSuccess)
            break;
        struct timespec ts = {0, 50 * 1000000L};
        nanosleep(&ts, NULL);
    }
    if (r != GrabSuccess) {
        fprintf(stderr, "big-wm-xkill: o ponteiro está preso por outro programa\n");
        return 1;
    }
    /* o teclado: Esc (e Ctrl+Alt+Esc de novo) cancela */
    bool kbd = false;
    for (int i = 0; i < 20 && !kbd; i++) {
        kbd = XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess;
        if (!kbd) {
            struct timespec ts = {0, 50 * 1000000L};
            nanosleep(&ts, NULL);
        }
    }

    int rc = 2; /* cancelado */
    unsigned int pressed = 0;
    Window target = None;
    bool done = false;
    while (!done || pressed) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == ButtonPress) {
            pressed |= 1u << (ev.xbutton.button & 31);
            if (!done) {
                done = true;
                if (ev.xbutton.button == Button1) {
                    target = ev.xbutton.subwindow;
                    rc = -1;
                }
            }
        } else if (ev.type == ButtonRelease) {
            /* espera soltar: o clique não chega ao aplicativo de baixo */
            pressed &= ~(1u << (ev.xbutton.button & 31));
        } else if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Escape)
                done = true;
        }
    }
    XUngrabPointer(dpy, CurrentTime);
    if (kbd)
        XUngrabKeyboard(dpy, CurrentTime);
    XFlush(dpy);
    if (rc == -1)
        rc = kill_at(target);
    XFreeCursor(dpy, cur);
    XCloseDisplay(dpy);
    return rc;
}
