#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
# Gera cursors/pirate (formato Xcursor) a partir de skull.svg, em vários
# tamanhos, com rsvg-convert. O resultado fica no repositório; rode de novo
# só se o desenho mudar:  python3 mkcursor.py
import os
import struct
import subprocess
import sys
import tempfile

import cairo

SIZES = (24, 32, 36, 48, 64, 72, 96, 128)
HOT = 16 / 32  # o centro do desenho de 32x32

here = os.path.dirname(os.path.abspath(__file__))
svg = os.path.join(here, "skull.svg")
images = []
with tempfile.TemporaryDirectory() as tmp:
    for size in SIZES:
        png = os.path.join(tmp, f"{size}.png")
        subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), svg, "-o", png], check=True)
        surf = cairo.ImageSurface.create_from_png(png)
        if surf.get_format() != cairo.FORMAT_ARGB32:
            sys.exit(f"{png}: formato inesperado")
        stride, data = surf.get_stride(), bytes(surf.get_data())
        # cairo: ARGB pré-multiplicado, na ordem do processador (LE aqui),
        # o mesmo que o Xcursor espera.
        pixels = b"".join(data[y * stride:y * stride + size * 4] for y in range(size))
        hot = int(round(size * HOT))
        images.append((size, hot, pixels))

ntoc = len(images)
header = struct.pack("<4sIII", b"Xcur", 16, 0x10000, ntoc)
pos = 16 + ntoc * 12
toc, chunks = b"", b""
for size, hot, pixels in images:
    toc += struct.pack("<III", 0xFFFD0002, size, pos)
    chunk = struct.pack("<IIIIIIIII", 36, 0xFFFD0002, size, 1, size, size, hot, hot, 0) + pixels
    chunks += chunk
    pos += len(chunk)
os.makedirs(os.path.join(here, "cursors"), exist_ok=True)
with open(os.path.join(here, "cursors", "pirate"), "wb") as f:
    f.write(header + toc + chunks)
print("cursors/pirate:", ", ".join(str(s) for s, _, _ in images))
