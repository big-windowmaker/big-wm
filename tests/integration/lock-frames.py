#!/usr/bin/env python3
"""Quadros do bloqueio X11 (tests/integration/lock-compositor.sh).

capture <dir> <largura> <altura>
    Lê a raiz do servidor (DISPLAY) em sequência, reduzida a 1/4, até existir
    <dir>/stop. Cada quadro é marcado "l" (bloqueado) ou "u" (depois que
    <dir>/unlock apareceu: a senha certa foi enviada).

classify <dir> <largura>
    S = a sessão (a janela vermelha 600x400 em 100,100 sobre o fundo preto);
    W = mais de 30% branco (o obscurer do xsecurelock, "INCOMPATIBLE
    COMPOSITOR, PLEASE FIX!"); L = o resto (a tela de bloqueio).
    Imprime exposicao (S enquanto bloqueado), aviso (W depois da senha certa),
    obscurer (W enquanto bloqueado) e o último quadro.

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
        white = sum(1 for i in range(0, len(p), 3 * 37) if p[i:i + 3] == b"\xff\xff\xff")
        return "W" if white * 37 * 3 > len(p) * 0.3 else "L"

    c = [(kind(f), f[-5]) for f in frames]
    print(f"quadros={len(c)}"
          f" exposicao={sum(1 for k, t in c if t == 'l' and k == 'S')}"
          f" aviso={sum(1 for k, t in c if t == 'u' and k == 'W')}"
          f" obscurer={sum(1 for k, t in c if t == 'l' and k == 'W')}"
          f" final={c[-1][0] if c else '?'}")


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
    elif cmd == "attack":
        attack(float(sys.argv[2]))
    else:
        sys.exit(f"uso: {sys.argv[0]} capture|classify|attack ...")
