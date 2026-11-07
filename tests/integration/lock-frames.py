#!/usr/bin/env python3
"""Quadros do bloqueio X11 (tests/integration/lock-compositor.sh).

capture <dir> <largura> <altura>
    Lê a raiz do servidor (DISPLAY) em sequência, reduzida a 1/4, até existir
    <dir>/stop. Cada quadro é marcado "l" (bloqueado) ou "u" (depois que
    <dir>/unlock apareceu: a senha certa foi enviada).

classify <dir> <largura>
    S = a sessão (a janela vermelha 600x400 em 100,100 sobre o fundo preto);
    W = mais de 30% branco (o obscurer do xsecurelock, "INCOMPATIBLE
    COMPOSITOR, PLEASE FIX!"); G = a janela verde do ataque à mostra (nem a
    tela de bloqueio nem o obscurer por cima); L = o resto (a tela de
    bloqueio).
    Imprime exposicao (S enquanto bloqueado), aviso (W depois da senha certa),
    obscurer (W enquanto bloqueado), ataque (G enquanto bloqueado) e o último
    quadro.

find icon|keys <texto>
    Lê a tela inteira e acha, no auth_big_wm, o botão ⌨ do campo de senha
    (pixels 0x404040 dentro da faixa branca do campo) ou as teclas do
    teclado na tela (rostos 0xaeaeae, em fileiras; layout US). Imprime
    "x,y" do ícone, ou os "x,y" das teclas do texto seguidos dos de Enter
    e Cancelar ("none" quando não acha).

attack <segundos>
    Janela verde de tela cheia, override-redirect, levantada sem parar e
    pedindo _NET_WM_BYPASS_COMPOSITOR: o picom deixa de compor e esconde a
    janela de sobreposição, onde está a tela de bloqueio.
"""
import ctypes
import ctypes.util
import os
import sys
import time

X = ctypes.cdll.LoadLibrary(ctypes.util.find_library("X11"))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XGetImage.restype = ctypes.c_void_p
X.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                        ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
X.XCreateWindow.restype = ctypes.c_ulong
X.XInternAtom.restype = ctypes.c_ulong


class XImage(ctypes.Structure):
    _fields_ = [("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int),
                ("format", ctypes.c_int), ("data", ctypes.c_void_p), ("byte_order", ctypes.c_int),
                ("bitmap_unit", ctypes.c_int), ("bitmap_bit_order", ctypes.c_int),
                ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int),
                ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int)]


class SetAttr(ctypes.Structure):
    _fields_ = [("background_pixmap", ctypes.c_ulong), ("background_pixel", ctypes.c_ulong),
                ("border_pixmap", ctypes.c_ulong), ("border_pixel", ctypes.c_ulong),
                ("bit_gravity", ctypes.c_int), ("win_gravity", ctypes.c_int),
                ("backing_store", ctypes.c_int), ("backing_planes", ctypes.c_ulong),
                ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int),
                ("event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("colormap", ctypes.c_ulong),
                ("cursor", ctypes.c_ulong)]


def capture(out, w, h):
    d = ctypes.c_void_p(X.XOpenDisplay(None))
    root = X.XDefaultRootWindow(d)
    n = 0
    while not os.path.exists(os.path.join(out, "stop")):
        p = X.XGetImage(d, root, 0, 0, w, h, 0xFFFFFFFF, 2)
        im = XImage.from_address(p)
        bpl = im.bytes_per_line
        buf = ctypes.string_at(im.data, bpl * h)
        X.XFree(ctypes.c_void_p(im.data))
        X.XFree(ctypes.c_void_p(p))
        px = bytearray()
        for y in range(0, h, 4):
            row = buf[y * bpl:(y + 1) * bpl]
            for x in range(0, w * 4, 16):
                px += bytes((row[x + 2], row[x + 1], row[x]))
        tag = "u" if os.path.exists(os.path.join(out, "unlock")) else "l"
        with open(os.path.join(out, f"f{n:05d}{tag}.ppm"), "wb") as f:
            f.write(b"P6 %d %d 255\n" % (w // 4, h // 4))
            f.write(px)
        n += 1


def classify(out, w):
    w //= 4
    frames = sorted(f for f in os.listdir(out) if f.endswith(".ppm"))

    def kind(name):
        data = open(os.path.join(out, name), "rb").read()
        p = data[data.index(b"\n") + 1:]
        inside = p[(75 * w + 100) * 3:(75 * w + 100) * 3 + 3]
        outside = p[(175 * w + 12) * 3:(175 * w + 12) * 3 + 3]
        if inside == b"\xff\x00\x00" and outside == b"\x00\x00\x00":
            return "S"
        if inside == b"\x00\xff\x00" and outside == b"\x00\xff\x00":
            return "G"
        white = sum(1 for i in range(0, len(p), 3 * 37) if p[i:i + 3] == b"\xff\xff\xff")
        return "W" if white * 37 * 3 > len(p) * 0.3 else "L"

    c = [(kind(f), f[-5]) for f in frames]
    print(f"quadros={len(c)}"
          f" exposicao={sum(1 for k, t in c if t == 'l' and k == 'S')}"
          f" aviso={sum(1 for k, t in c if t == 'u' and k == 'W')}"
          f" obscurer={sum(1 for k, t in c if t == 'l' and k == 'W')}"
          f" ataque={sum(1 for k, t in c if t == 'l' and k == 'G')}"
          f" final={c[-1][0] if c else '?'}")


def screen():
    d = ctypes.c_void_p(X.XOpenDisplay(None))
    scr = X.XDefaultScreen(d)
    w, h = X.XDisplayWidth(d, scr), X.XDisplayHeight(d, scr)
    p = X.XGetImage(d, X.XDefaultRootWindow(d), 0, 0, w, h, 0xFFFFFFFF, 2)
    im = XImage.from_address(p)
    buf = ctypes.string_at(im.data, im.bytes_per_line * h)
    bpl = im.bytes_per_line
    X.XFree(ctypes.c_void_p(im.data))
    X.XFree(ctypes.c_void_p(p))

    def px(x, y):
        o = y * bpl + x * 4
        return (buf[o + 2] << 16) | (buf[o + 1] << 8) | buf[o]
    return w, h, px


def find(what, word=""):
    w, h, px = screen()
    if what == "icon":
        # campo de senha: faixa branca de 150+ px em 12+ linhas seguidas
        band = []
        for y in range(h):
            run = best = 0
            for x in range(w):
                run = run + 1 if px(x, y) == 0xFFFFFF else 0
                best = max(best, run)
            if best > 150:
                band.append(y)
                if len(band) >= 12 and band[-1] - band[0] == len(band) - 1:
                    continue
            elif len(band) >= 12:
                break
            else:
                band = []
        if len(band) < 12:
            print("none")
            return
        xs = [x for y in band for x in range(w) if px(x, y) == 0x404040]
        print(f"{(min(xs) + max(xs)) // 2},{(band[0] + band[-1]) // 2}" if xs else "none")
        return
    # teclas: componentes 0xaeaeae com 16+ px (passo 2 para ir rápido)
    taken = set()
    keys = []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if (x, y) in taken or px(x, y) != 0xAEAEAE:
                continue
            x2 = x
            while x2 + 1 < w and px(x2 + 1, y) == 0xAEAEAE:
                x2 += 1
            y2 = y
            while y2 + 1 < h and px(x, y2 + 1) == 0xAEAEAE:
                y2 += 1
            for yy in range(y, y2 + 1, 2):
                for xx in range(x - x % 2, x2 + 1, 2):
                    taken.add((xx, yy))
            if x2 - x > 15 and y2 - y > 15:
                keys.append(((x + x2) // 2, (y + y2) // 2))
    rows = {}
    for cx, cy in keys:
        r = next((k for k in rows if abs(k - cy) < 8), cy)
        rows.setdefault(r, []).append(cx)
    ys = sorted(rows)
    if len(ys) < 6:
        print("none")
        return
    # a fileira de botões do diálogo (Cancelar, Desbloquear) e as 5 do teclado
    buttons, kb = sorted(rows[ys[-6]]), ys[-5:]
    grid = [sorted(rows[y]) for y in kb]
    chars = ["`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "^zxcvbnm,./"]
    out = []
    for ch in word:
        for r, line in enumerate(chars):
            if ch in line and line.index(ch) < len(grid[r]):
                out.append(f"{grid[r][line.index(ch)]},{kb[r]}")
                break
    out.append(f"{grid[4][-1]},{kb[4]}")
    out.append(f"{buttons[0]},{ys[-6]}")
    print(" ".join(out))


def attack(seconds):
    d = ctypes.c_void_p(X.XOpenDisplay(None))
    scr = X.XDefaultScreen(d)
    a = SetAttr()
    a.background_pixel = 0x00FF00
    a.override_redirect = 1
    win = X.XCreateWindow(d, ctypes.c_ulong(X.XDefaultRootWindow(d)), 0, 0,
                          X.XDisplayWidth(d, scr), X.XDisplayHeight(d, scr), 0, 0, 1, None,
                          ctypes.c_ulong(2 | 512), ctypes.byref(a))
    one = ctypes.c_long(1)
    X.XChangeProperty(d, ctypes.c_ulong(win),
                      ctypes.c_ulong(X.XInternAtom(d, b"_NET_WM_BYPASS_COMPOSITOR", 0)),
                      ctypes.c_ulong(6), 32, 0, ctypes.byref(one), 1)
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        X.XMapRaised(d, ctypes.c_ulong(win))
        X.XFlush(d)
        time.sleep(0.05)


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "capture":
        capture(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
    elif cmd == "classify":
        classify(sys.argv[2], int(sys.argv[3]))
    elif cmd == "find":
        find(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "")
    elif cmd == "attack":
        attack(float(sys.argv[2]))
    else:
        sys.exit(f"uso: {sys.argv[0]} capture|classify|find|attack ...")
