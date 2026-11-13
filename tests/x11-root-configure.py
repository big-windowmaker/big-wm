#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Manda à raiz um ConfigureNotify sintético com o tamanho dado, como o que o
servidor X manda quando a resolução muda (o Xvfb não muda de tamanho).

    x11-root-configure.py LARGURA ALTURA
"""
import ctypes
import ctypes.util
import sys

CONFIGURE_NOTIFY = 22
STRUCTURE_NOTIFY_MASK = 1 << 17


class XConfigureEvent(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int),
                ("display", ctypes.c_void_p), ("event", ctypes.c_ulong), ("window", ctypes.c_ulong),
                ("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int),
                ("height", ctypes.c_int), ("border_width", ctypes.c_int),
                ("above", ctypes.c_ulong), ("override_redirect", ctypes.c_int)]


class XEvent(ctypes.Union):
    _fields_ = [("xconfigure", XConfigureEvent), ("pad", ctypes.c_long * 24)]


def main():
    width, height = int(sys.argv[1]), int(sys.argv[2])
    x = ctypes.CDLL(ctypes.util.find_library("X11"))
    x.XOpenDisplay.restype = ctypes.c_void_p
    x.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x.XDefaultRootWindow.restype = ctypes.c_ulong
    x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long,
                             ctypes.POINTER(XEvent)]
    x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    x.XCloseDisplay.argtypes = [ctypes.c_void_p]
    dpy = x.XOpenDisplay(None)
    if not dpy:
        return 1
    root = x.XDefaultRootWindow(dpy)
    ev = XEvent()
    ev.xconfigure.type = CONFIGURE_NOTIFY
    ev.xconfigure.event = root
    ev.xconfigure.window = root
    ev.xconfigure.width = width
    ev.xconfigure.height = height
    x.XSendEvent(dpy, root, 0, STRUCTURE_NOTIFY_MASK, ctypes.byref(ev))
    x.XSync(dpy, 0)
    x.XCloseDisplay(dpy)
    return 0


if __name__ == "__main__":
    sys.exit(main())
