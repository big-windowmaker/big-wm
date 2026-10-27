#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Ponteiro virtual (zwlr_virtual_pointer_v1) para os testes de integração:
# move, clica e rola de verdade pelo compositor, sem dispositivo de entrada
# (WLR_LIBINPUT_NO_DEVICES). Fala o protocolo de fio do Wayland direto no
# socket, como o big-wm-kill.
#
#   wl-pointer.py <largura> <altura> move <x> <y>
#   wl-pointer.py <largura> <altura> click <x> <y> [left|middle|right]
#   wl-pointer.py <largura> <altura> wheel <x> <y> up|down
#   wl-pointer.py <largura> <altura> drag <x> <y> <x2> <y2> [pausa]
#       aperta o esquerdo em x,y, arrasta até x2,y2 e solta (depois da
#       pausa em segundos, para uma captura com o botão ainda apertado)
#
# <largura> × <altura>: a área de todas as saídas (o movimento absoluto é
# relativo a ela); x e y em coordenadas dessa área.

import os
import socket
import struct
import sys
import time

BUTTONS = {"left": 0x110, "right": 0x111, "middle": 0x112}


class Wayland:
    def __init__(self):
        name = os.environ.get("WAYLAND_DISPLAY") or "wayland-0"
        path = name if name.startswith("/") else os.path.join(os.environ["XDG_RUNTIME_DIR"], name)
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.buf = b""
        self.next_id = 2
        self.globals = {}
        self.done = set()
        self.registry = self.new_id()
        self.send(1, 1, struct.pack("<I", self.registry))  # wl_display.get_registry
        self.roundtrip()

    def new_id(self):
        i = self.next_id
        self.next_id += 1
        return i

    @staticmethod
    def string(s):
        data = s.encode() + b"\0"
        return struct.pack("<I", len(data)) + data + b"\0" * (-len(data) % 4)

    def send(self, obj, opcode, payload=b""):
        self.sock.sendall(struct.pack("<II", obj, ((8 + len(payload)) << 16) | opcode) + payload)

    def roundtrip(self):
        callback = self.new_id()
        self.send(1, 0, struct.pack("<I", callback))  # wl_display.sync
        while callback not in self.done:
            data = self.sock.recv(65536)
            if not data:
                raise ConnectionError("compositor fechou a conexão")
            self.buf += data
            while len(self.buf) >= 8:
                obj, word = struct.unpack_from("<II", self.buf)
                size, opcode = word >> 16, word & 0xFFFF
                if len(self.buf) < size:
                    break
                body, self.buf = self.buf[8:size], self.buf[size:]
                if obj == 1 and opcode == 0:
                    raise RuntimeError("erro do Wayland: %r" % body)
                if obj == self.registry and opcode == 0:
                    (gname,) = struct.unpack_from("<I", body)
                    (length,) = struct.unpack_from("<I", body, 4)
                    iface = body[8:8 + length].rstrip(b"\0").decode()
                    (version,) = struct.unpack_from("<I", body, 8 + length + (-length % 4))
                    self.globals[iface] = (gname, version)
                elif opcode == 0 and obj >= 2 and obj != self.registry:
                    self.done.add(obj)  # wl_callback.done


def main():
    width, height, action, x, y = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
    arg = sys.argv[6] if len(sys.argv) > 6 else "left"
    wl = Wayland()
    if "zwlr_virtual_pointer_manager_v1" not in wl.globals:
        print("wl-pointer: compositor sem zwlr_virtual_pointer_manager_v1", file=sys.stderr)
        return 1
    gname, _version = wl.globals["zwlr_virtual_pointer_manager_v1"]
    manager = wl.new_id()
    wl.send(wl.registry, 0, struct.pack("<I", gname) + wl.string("zwlr_virtual_pointer_manager_v1")
            + struct.pack("<II", 1, manager))
    pointer = wl.new_id()
    wl.send(manager, 0, struct.pack("<II", 0, pointer))  # create_virtual_pointer(seat=null)
    ms = lambda: int(time.monotonic() * 1000) & 0xFFFFFFFF  # noqa: E731
    wl.send(pointer, 1, struct.pack("<IIIII", ms(), x, y, width, height))  # motion_absolute
    wl.send(pointer, 4, struct.pack("<I", ms()))  # frame
    wl.roundtrip()
    time.sleep(0.2)
    if action == "click":
        for state in (1, 0):
            wl.send(pointer, 2, struct.pack("<III", ms(), BUTTONS[arg], state))  # button
            wl.send(pointer, 4, struct.pack("<I", ms()))
            wl.roundtrip()
            time.sleep(0.1)
    elif action == "drag":
        x2, y2 = int(sys.argv[6]), int(sys.argv[7])
        pause = float(sys.argv[8]) if len(sys.argv) > 8 else 0.3
        wl.send(pointer, 2, struct.pack("<III", ms(), BUTTONS["left"], 1))
        wl.send(pointer, 4, struct.pack("<I", ms()))
        wl.roundtrip()
        steps = 12
        for i in range(1, steps + 1):
            time.sleep(0.04)
            px, py = x + (x2 - x) * i // steps, y + (y2 - y) * i // steps
            wl.send(pointer, 1, struct.pack("<IIIII", ms(), px, py, width, height))
            wl.send(pointer, 4, struct.pack("<I", ms()))
            wl.roundtrip()
        time.sleep(pause)
        wl.send(pointer, 2, struct.pack("<III", ms(), BUTTONS["left"], 0))
        wl.send(pointer, 4, struct.pack("<I", ms()))
        wl.roundtrip()
    elif action == "wheel":
        step = -1 if arg == "up" else 1
        wl.send(pointer, 5, struct.pack("<I", 0))  # axis_source: wheel
        wl.send(pointer, 7, struct.pack("<IIii", ms(), 0, step * 10 * 256, step))  # axis_discrete
        wl.send(pointer, 4, struct.pack("<I", ms()))
        wl.roundtrip()
    time.sleep(0.2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
