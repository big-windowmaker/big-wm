#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Itens StatusNotifierItem falsos para testar o big-wm-tray:
#
#   fake-sni.py <n> <diretório>
#
# Registra n itens (n >= 5) no org.kde.StatusNotifierWatcher, todos numa
# conexão, em /Item0 ... /Item<n-1>, cada um com um jeito de dar o ícone:
#   0  IconName do tema, ToolTip com título e descrição
#   1  só IconPixmap (sem nome de ícone), só Title
#   2  IconName que só existe no IconThemePath (PNG solto em <diretório>)
#   3  Status NeedsAttention com AttentionIconName
#   4  Status Passive (fica fora da bandeja); SIGUSR1 o torna Active
#   5+ IconName do tema, só Id
# Cada chamada (Activate, SecondaryActivate, ContextMenu, Scroll) vira uma
# linha em <diretório>/calls.log: "Método ItemN argumentos".

import os
import signal
import sys

import gi

gi.require_version("GdkPixbuf", "2.0")
from gi.repository import GdkPixbuf, Gio, GLib  # noqa: E402

ITEM_XML = """
<node>
  <interface name="org.kde.StatusNotifierItem">
    <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
    <property name="Id" type="s" access="read"/>
    <property name="Title" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="Category" type="s" access="read"/>
    <property name="IconName" type="s" access="read"/>
    <property name="IconThemePath" type="s" access="read"/>
    <property name="IconPixmap" type="a(iiay)" access="read"/>
    <property name="AttentionIconName" type="s" access="read"/>
    <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
    <property name="ItemIsMenu" type="b" access="read"/>
    <signal name="NewStatus"><arg type="s"/></signal>
    <signal name="NewIcon"/>
  </interface>
</node>
"""


def argb_square(size, rgb):
    """IconPixmap: ARGB32 em ordem de rede."""
    return [(size, size, bytes([0xff, *rgb]) * (size * size))]


def main():
    count, outdir = int(sys.argv[1]), sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    log = open(os.path.join(outdir, "calls.log"), "a", buffering=1, encoding="utf-8")
    pixbuf = GdkPixbuf.Pixbuf.new(GdkPixbuf.Colorspace.RGB, True, 8, 24, 24)
    pixbuf.fill(0x2080ffff)
    pixbuf.savev(os.path.join(outdir, "fake-sni-custom.png"), "png", [], [])

    items = []
    for i in range(count):
        props = {"Id": "fake-%d" % i, "Title": "", "Status": "Active", "Category": "ApplicationStatus",
                 "IconName": "image-x-generic", "IconThemePath": "", "IconPixmap": [],
                 "AttentionIconName": "", "ToolTip": ("", [], "", ""), "ItemIsMenu": False}
        if i == 0:
            props.update(IconName="network-wired", Title="Rede falsa",
                         ToolTip=("", [], "Rede falsa", "Conectado"))
        elif i == 1:
            props.update(IconName="", IconPixmap=argb_square(22, (0xd0, 0x20, 0x20)), Title="Só pixmap")
        elif i == 2:
            props.update(IconName="fake-sni-custom", IconThemePath=outdir, Title="Tema próprio")
        elif i == 3:
            props.update(IconName="dialog-information", AttentionIconName="dialog-warning",
                         Status="NeedsAttention", Title="Atenção")
        elif i == 4:
            props.update(Status="Passive", Title="Passivo")
        items.append(props)

    bus = Gio.bus_get_sync(Gio.BusType.SESSION)
    node = Gio.DBusNodeInfo.new_for_xml(ITEM_XML)

    def on_call(_bus, _sender, path, _iface, method, params, invocation):
        log.write("%s %s %s\n" % (method, path.lstrip("/"), " ".join(str(v) for v in params.unpack())))
        invocation.return_value(None)

    def on_get(_bus, _sender, path, _iface, prop):
        props = items[int(path[len("/Item"):])]
        sig = {"IconPixmap": "a(iiay)", "ToolTip": "(sa(iiay)ss)", "ItemIsMenu": "b"}.get(prop, "s")
        return GLib.Variant(sig, props[prop])

    for i in range(count):
        bus.register_object_with_closures2("/Item%d" % i, node.interfaces[0], on_call, on_get, None)
    for i in range(count):
        bus.call_sync("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                      "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
                      GLib.Variant("(s)", ("/Item%d" % i,)), None, Gio.DBusCallFlags.NONE, 3000, None)

    def activate_passive():
        items[4]["Status"] = "Active"
        bus.emit_signal(None, "/Item4", "org.kde.StatusNotifierItem", "NewStatus",
                        GLib.Variant("(s)", ("Active",)))
        return True

    loop = GLib.MainLoop()
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, activate_passive)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM, loop.quit)
    print("fake-sni: %d itens" % count, flush=True)
    loop.run()


if __name__ == "__main__":
    main()
