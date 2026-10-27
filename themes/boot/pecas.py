#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Gera as imagens simples dos temas de boot do Big WindowMaker, no relevo do
# Window Maker (luz de cima e da esquerda, sombra embaixo e à direita). São só
# cores chapadas e degradês, então o PNG é escrito aqui mesmo: nada de PNG no
# repositório nem de editor de imagem no build.
#
#   pecas.py grub DIRETÓRIO       peças de "9 fatias" do gfxmenu do GRUB
#   pecas.py plymouth DIRETÓRIO   ladrilho do Dock, barra de título e cores
#                                 do diálogo de senha e da barra de progresso

import struct
import sys
import zlib
from pathlib import Path

# Cores do estilo padrão da edição (data/GNUstep/Defaults/WindowMaker).
AZUL = "#8ba7c4"  # barra de seleção / progresso da tela "Window Maker Live"
AZUL_LUZ = "#a9bfd6"
AZUL_SOMBRA = "#4c5966"
TILE_CLARO = "#a6a6b6"  # IconBack = (dgradient, "rgb:a6/a6/b6", "rgb:51/55/61")
TILE_ESCURO = "#515561"
TITULO_ESQ = "#505a5e"  # FTitleBack = (hgradient, "rgb:50/5a/5e", "rgb:20/2a/2e")
TITULO_DIR = "#202a2e"
PAINEL = "#aeaaae"  # painéis do Window Maker (WPrefs, diálogos)


def rgb(cor):
    return bytes.fromhex(cor[1:])


def png(path, rows):
    """rows: lista de linhas; cada pixel é "#rrggbb" ou bytes RGB."""
    height, width = len(rows), len(rows[0])

    def pixel(p):
        return (rgb(p) if isinstance(p, str) else p) + b"\xff"

    raw = b"".join(b"\0" + b"".join(pixel(p) for p in row) for row in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    )


def mistura(a, b, t):
    return bytes(round(x + (y - x) * t) for x, y in zip(rgb(a), rgb(b)))


def caixa(dest, nome, fundo, cima, esquerda, direita, baixo):
    """Peças de 9 fatias; baixo é uma lista de cores, uma por linha."""
    pecas = {
        "nw": [[cima]],
        "n": [[cima]],
        "ne": [[direita]],
        "w": [[esquerda]],
        "c": [[fundo]],
        "e": [[direita]],
        "sw": [[c] for c in baixo],
        "s": [[c] for c in baixo],
        "se": [[c] for c in baixo],
    }
    for lado, rows in pecas.items():
        png(dest / f"{nome}_{lado}.png", rows)


def grub(dest):
    # Barra de seleção azul-aço com a sombra escura de 2 px embaixo.
    caixa(dest, "selecao", AZUL, AZUL_LUZ, AZUL_LUZ, AZUL_SOMBRA, [AZUL_SOMBRA, "#30353a"])
    # Barra de rolagem: trilho afundado e polegar em relevo, como no NeXTSTEP.
    caixa(dest, "trilho", "#5f5f5f", "#3a3a3a", "#3a3a3a", "#8c8c8c", ["#8c8c8c"])
    caixa(dest, "polegar", PAINEL, "#e0e0e0", "#e0e0e0", "#4a4a4a", ["#4a4a4a"])


def ladrilho(lado, borda):
    """Ladrilho de ícone do Dock: degradê diagonal e relevo de `borda` px."""
    rows = []
    for y in range(lado):
        row = []
        for x in range(lado):
            if x >= lado - borda or y >= lado - borda:
                row.append("#1e1e22")
            elif x < borda or y < borda:
                row.append("#e8e8ee")
            else:
                row.append(mistura(TILE_CLARO, TILE_ESCURO, (x + y) / (2 * (lado - 1))))
        rows.append(row)
    return rows


def plymouth(dest):
    # Ladrilho do Dock; o logo vai por cima, desenhado pelo script.
    png(dest / "ladrilho.png", ladrilho(176, 3))
    # Uma cor por arquivo, 1x1: o script estica com Image.Scale.
    for nome, cor in {
        "preto": "#000000",
        "branco": "#ffffff",
        "luz": "#e8e8e8",
        "sombra": "#555555",
        "painel": PAINEL,
        "trilho": "#5f5f5f",
        "trilho-sombra": "#3a3a3a",
        "trilho-luz": "#9a9a9a",
        "azul": AZUL,
        "azul-luz": AZUL_LUZ,
        "azul-sombra": AZUL_SOMBRA,
    }.items():
        png(dest / f"cor-{nome}.png", [[cor]])
    # Barra de título do Window Maker (hgradient), esticada na largura.
    png(dest / "titulo.png", [[mistura(TITULO_ESQ, TITULO_DIR, x / 255) for x in range(256)]])
    # Marcador de caractere da senha: quadrado preto, como no NeXTSTEP.
    png(dest / "marcador.png", [["#000000"] * 7 for _ in range(7)])


def main():
    alvo, dest = sys.argv[1], Path(sys.argv[2])
    dest.mkdir(parents=True, exist_ok=True)
    {"grub": grub, "plymouth": plymouth}[alvo](dest)


if __name__ == "__main__":
    main()
