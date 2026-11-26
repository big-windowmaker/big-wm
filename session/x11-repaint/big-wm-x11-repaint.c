/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * big-wm-x11-repaint: faz o compositor (picom) mostrar as janelas que o
 * Window Maker desenha uma vez só. Roda como ExecStartPost do picom na
 * sessão X11:
 *
 *   big-wm-x11-repaint [--picom-pid PID]   vigia e redesenha (segundo plano)
 *   big-wm-x11-repaint --once              só redesenha tudo algumas vezes e sai
 *
 * Causa: o picom só pinta uma janela depois de receber um "damage" dela
 * depois de ter criado o objeto Damage (ever_damaged, picom 10+), e ele cria
 * esse objeto de forma assíncrona, um pouco depois de a janela aparecer. Os
 * ladrilhos do Dock e do Clip, os ícones e as barras de título do Window
 * Maker são desenhados uma vez só, pelo servidor X (pixmap de fundo), no
 * momento em que são mapeados: esse desenho chega antes do Damage e se
 * perde. Sem nenhum desenho depois, o picom nunca pinta a janela: os
 * ladrilhos ficam invisíveis (só o papel de parede) ou pretos, ao entrar na
 * sessão (picom e Window Maker sobem juntos), ao reiniciar o Window Maker e
 * sempre que um ladrilho novo aparece.
 *
 * Correção: XClearArea com exposures (o servidor redesenha o fundo e manda
 * Expose, sem nada mudar na tela) gera o damage que faltou. Vigiando a raiz
 * desde que o picom assume a tela: em cada janela logo depois de ser mapeada
 * (as override-redirect do Window Maker: ladrilhos, ícones, molduras, menus;
 * e também as que ele não emoldura, como o dock com abas, tipo DOCK, que
 * subia depois das repinturas gerais e ficava só com os rótulos na tela), e
 * em todas as janelas pouco depois de o picom e de o Window Maker subirem.
 * Fica no cgroup do picom: sai junto com ele.
 *
 * Mudança das telas (--picom-pid): o picom 13 com vsync = true entra num
 * laço de repintura quando a configuração das telas muda (xrandr, Display
 * Resolution, monitor ligado): com a tela parada, ele acorda a cada vblank
 * (60 vezes por segundo) e o Xorg junto. Não é só o tamanho: trocar a saída
 * primária (xrandr --primary, o que o big-wm-display-resolution faz ao
 * entrar na sessão) também (OptiPlex 760, Intel G45: picom 15 -> 61 ticks a
 * cada 10 s). O SIGUSR1 (o picom refaz a sessão dele, sem trocar de
 * processo) sai do laço: mandado 1 s depois do último RRScreenChangeNotify
 * (ou da última mudança de tamanho da raiz), seguido das repinturas de
 * sempre, porque o picom refaz o Damage de todas as janelas.
 *
 * Em C (antes Python + ctypes, ~8 MiB residentes a sessão inteira); só a
 * libX11 e a libXrandr.
 */
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CM_WAIT_SECONDS 5.0
/* Depois da última mudança das telas (o xrandr pode mudar mais de uma vez
 * seguida), o SIGUSR1 ao picom. */
#define RESET_DELAY 1.0
/* Todas as janelas, depois de o picom assumir a tela e de novo depois de o
 * Window Maker anunciar que subiu (ele ainda termina o Dock e o Clip). */
static const double REPAINT_DELAYS[] = { 0.5, 2.0, 5.0 };
/* Uma janela, depois de mapeada: o picom já criou o Damage dela (as outras
 * vezes cobrem um picom ocupado, ex.: ao entrar na sessão). */
static const double MAP_DELAYS[] = { 0.1, 0.5, 2.0 };
#define N_DELAYS 3

typedef struct {
    double when;
    Window win; /* a raiz = todas as janelas abaixo dela */
} Pending;

static Pending *pending;
static size_t n_pending, cap_pending;

static double
now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void
add_pending(double when, Window win)
{
    if (n_pending == cap_pending) {
        size_t cap = cap_pending ? cap_pending * 2 : 32;
        Pending *p = realloc(pending, cap * sizeof *p);
        if (!p)
            return; /* sem memória: perde uma repintura, não o processo */
        pending = p;
        cap_pending = cap;
    }
    pending[n_pending++] = (Pending){ when, win };
}

static void
add_delays(double now, Window win, const double *delays)
{
    for (int i = 0; i < N_DELAYS; i++)
        add_pending(now + delays[i], win);
}

/* Erros de X (janela que sumiu no meio do caminho) não podem derrubar o
 * processo: o tratador padrão da libX11 chama exit(). */
static int
ignore_errors(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

/* XClearArea com exposures em win e em todas as filhas. */
static void
repaint(Display *dpy, Window win)
{
    Window root_ret, parent_ret, *kids = NULL;
    unsigned int n = 0;
    XClearArea(dpy, win, 0, 0, 0, 0, True);
    if (!XQueryTree(dpy, win, &root_ret, &parent_ret, &kids, &n))
        return;
    for (unsigned int i = 0; i < n; i++)
        repaint(dpy, kids[i]);
    if (kids)
        XFree(kids);
}

/* Todas as janelas abaixo da raiz (a raiz não: é o papel de parede). */
static void
repaint_children(Display *dpy, Window root)
{
    Window root_ret, parent_ret, *kids = NULL;
    unsigned int n = 0;
    if (!XQueryTree(dpy, root, &root_ret, &parent_ret, &kids, &n))
        return;
    for (unsigned int i = 0; i < n; i++)
        repaint(dpy, kids[i]);
    if (kids)
        XFree(kids);
}

/* SIGUSR1 ao picom, só se o processo ainda for o picom (o PID não foi
 * reaproveitado). */
static int
reset_picom(pid_t pid)
{
    char path[64], comm[32] = "";
    snprintf(path, sizeof path, "/proc/%ld/comm", (long)pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    int ok = fgets(comm, sizeof comm, f) != NULL;
    fclose(f);
    comm[strcspn(comm, "\n")] = '\0';
    if (!ok || strcmp(comm, "picom") != 0)
        return 0;
    return kill(pid, SIGUSR1) == 0;
}

/* Tela bloqueada pelo xsecurelock (big-wm-lock): a janela "obscurer", filha
 * da raiz, está visível. Ao refazer a sessão, o picom desmapeia a janela de
 * sobreposição do Composite (COW), onde o xsecurelock desenha; o
 * xsecurelock a mapeia de volta e os dois brigam (centenas de vezes em
 * milissegundos), e, enquanto isso, aparece o obscurer: a tela pisca com
 * "INCOMPATIBLE COMPOSITOR, PLEASE FIX!". Acontece ao acordar os monitores
 * com a tela bloqueada (cada monitor que volta muda as telas). O SIGUSR1
 * espera o desbloqueio. */
static int
screen_locked(Display *dpy, Window root)
{
    Window root_ret, parent_ret, *kids = NULL;
    unsigned int n = 0;
    int locked = 0;
    if (!XQueryTree(dpy, root, &root_ret, &parent_ret, &kids, &n))
        return 0;
    for (unsigned int i = 0; i < n && !locked; i++) {
        XClassHint hint = { NULL, NULL };
        XWindowAttributes wa;
        if (!XGetClassHint(dpy, kids[i], &hint))
            continue;
        locked = hint.res_class && strcmp(hint.res_class, "xsecurelock") == 0 &&
                 XGetWindowAttributes(dpy, kids[i], &wa) && wa.map_state == IsViewable;
        XFree(hint.res_name);
        XFree(hint.res_class);
    }
    if (kids)
        XFree(kids);
    return locked;
}

static void
watch(Display *dpy, Window root, int once, pid_t picom_pid)
{
    XSelectInput(dpy, root,
                 SubstructureNotifyMask | PropertyChangeMask | (picom_pid ? StructureNotifyMask : 0));
    int rr_change = -1, rr_error;
    if (picom_pid && XRRQueryExtension(dpy, &rr_change, &rr_error))
        XRRSelectInput(dpy, root, RRScreenChangeNotifyMask); /* event_base + 0 */
    else
        rr_change = -1;
    XSync(dpy, False);
    Atom check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    XWindowAttributes wa;
    int width = 0, height = 0;
    if (XGetWindowAttributes(dpy, root, &wa)) {
        width = wa.width;
        height = wa.height;
    }
    double reset_at = -1;
    int reset_deferred = 0; /* esperando o desbloqueio (screen_locked) */
    add_delays(now_s(), root, REPAINT_DELAYS);
    int fd = ConnectionNumber(dpy);

    while (n_pending || reset_at >= 0 || !once) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            double now = now_s();
            if (ev.type == MapNotify && ev.xmap.event == root) {
                add_delays(now, ev.xmap.window, MAP_DELAYS);
            } else if (ev.type == PropertyNotify && ev.xproperty.window == root && ev.xproperty.atom == check) {
                /* O Window Maker subiu (ou reiniciou): tudo de novo. */
                add_delays(now, root, REPAINT_DELAYS);
            } else if (picom_pid && ev.type == ConfigureNotify && ev.xconfigure.window == root) {
                if (ev.xconfigure.width != width || ev.xconfigure.height != height) {
                    width = ev.xconfigure.width;
                    height = ev.xconfigure.height;
                    reset_at = now + RESET_DELAY;
                }
            } else if (rr_change >= 0 && ev.type == rr_change + RRScreenChangeNotify) {
                reset_at = now + RESET_DELAY;
            } else if (reset_deferred && (ev.type == UnmapNotify || ev.type == DestroyNotify) &&
                       ev.xany.window == root) {
                /* Uma janela de cima sumiu: talvez o desbloqueio. */
                reset_deferred = 0;
                reset_at = now + RESET_DELAY;
            }
        }
        double now = now_s();
        if (reset_at >= 0 && reset_at <= now) {
            reset_at = -1;
            /* Sempre o resultado de agora: um desbloqueio entre a leitura dos
             * eventos e esta consulta não deixa a espera armada (o Unmap
             * dele, lido depois, mandaria um segundo SIGUSR1). */
            reset_deferred = screen_locked(dpy, root);
            if (!reset_deferred && reset_picom(picom_pid))
                add_delays(now, root, REPAINT_DELAYS);
            continue;
        }
        /* As repinturas vencidas, cada janela uma vez. */
        int did = 0;
        for (size_t i = 0; i < n_pending;) {
            if (pending[i].when > now) {
                i++;
                continue;
            }
            Window w = pending[i].win;
            for (size_t j = i; j < n_pending;) { /* tira esta janela das vencidas */
                if (pending[j].win == w && pending[j].when <= now)
                    pending[j] = pending[--n_pending];
                else
                    j++;
            }
            if (w == root)
                repaint_children(dpy, root);
            else
                repaint(dpy, w);
            did = 1;
        }
        if (did) {
            XFlush(dpy);
            continue;
        }
        double next = reset_at;
        for (size_t i = 0; i < n_pending; i++)
            if (next < 0 || pending[i].when < next)
                next = pending[i].when;
        int timeout = next < 0 ? -1 : (int)((next - now) * 1000) + 1;
        struct pollfd pfd = { fd, POLLIN, 0 };
        int r = poll(&pfd, 1, timeout);
        if (r < 0 && errno != EINTR)
            return;
        if (r > 0 && !XPending(dpy)) {
            /* Conexão com o X fechada (fim da sessão): a libX11 sai ao ler. */
            XEvent ev;
            XNextEvent(dpy, &ev);
        }
    }
}

int
main(int argc, char **argv)
{
    int once = argc == 2 && strcmp(argv[1], "--once") == 0;
    pid_t picom_pid = 0;
    if (argc == 3 && strcmp(argv[1], "--picom-pid") == 0) {
        char *end;
        long v = strtol(argv[2], &end, 10);
        if (*argv[2] && !*end && v > 0)
            picom_pid = (pid_t)v;
    }
    XSetErrorHandler(ignore_errors);
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 0;
    Window root = DefaultRootWindow(dpy);

    /* Espera o compositor assumir a tela (seleção _NET_WM_CM_S0). */
    Atom cm = XInternAtom(dpy, "_NET_WM_CM_S0", False);
    double deadline = now_s() + CM_WAIT_SECONDS;
    while (!XGetSelectionOwner(dpy, cm) && now_s() <= deadline)
        usleep(50000);

    /* ExecStartPost espera este processo: o filho vigia, no cgroup do picom
     * (sai quando o picom para). --once redesenha e sai. */
    if (!once) {
        pid_t pid = fork();
        if (pid < 0)
            return 0;
        if (pid > 0)
            _exit(0);
        setsid();
    }
    watch(dpy, root, once, picom_pid);
    XCloseDisplay(dpy);
    free(pending);
    return 0;
}
