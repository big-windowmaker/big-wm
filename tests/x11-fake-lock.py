#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Imita a tela bloqueada pelo xsecurelock: uma janela visível, filha da
raiz, com a classe do "obscurer" dele ("obscurer", "xsecurelock"). Fica até
o processo terminar (a conexão fecha e a janela some, como no desbloqueio).
Escreve "pronto" na saída quando a janela já está mapeada.

    x11-fake-lock.py
"""
import ctypes
import ctypes.util
import signal
import sys


class XClassHint(ctypes.Structure):
    _fields_ = [("res_name", ctypes.c_char_p), ("res_class", ctypes.c_char_p)]


def main():
    x = ctypes.CDLL(ctypes.util.find_library("X11"))
    x.XOpenDisplay.restype = ctypes.c_void_p
    x.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x.XDefaultRootWindow.restype = ctypes.c_ulong
    x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x.XCreateSimpleWindow.restype = ctypes.c_ulong
    x.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                                      ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong,
                                      ctypes.c_ulong]
    x.XSetClassHint.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XClassHint)]
    x.XMapWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    dpy = x.XOpenDisplay(None)
    if not dpy:
        return 1
    win = x.XCreateSimpleWindow(dpy, x.XDefaultRootWindow(dpy), 0, 0, 10, 10, 0, 0, 0)
    hint = XClassHint(b"obscurer", b"xsecurelock")
    x.XSetClassHint(dpy, win, ctypes.byref(hint))
    x.XMapWindow(dpy, win)
    x.XSync(dpy, 0)
    print("pronto", flush=True)  # a janela já está mapeada no servidor
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    signal.pause()
    return 0


if __name__ == "__main__":
    sys.exit(main())
