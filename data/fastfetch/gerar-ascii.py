#!/usr/bin/python3 -I
# Gera o logo ASCII do fastfetch a partir do PNG do logo (rsvg-convert).
# Uso: gerar-ascii.py logo.png colunas > big-wm.txt
#
# Classes por célula: gradiente azul ($1) / ciano ($2), letras "big"
# (recortes transparentes dentro do logo: brancas, $3) e disco preto
# (espaço: o fundo escuro do terminal faz o papel do preto).
import sys
from collections import deque

from PIL import Image

src, cols = sys.argv[1], int(sys.argv[2])
img = Image.open(src).convert("RGBA")
w, h = img.size
px = img.load()

# Transparência ligada à borda = fora do logo; o resto transparente = letras.
outside = [[False] * w for _ in range(h)]
queue = deque()
for x in range(w):
    queue.extend([(x, 0), (x, h - 1)])
for y in range(h):
    queue.extend([(0, y), (w - 1, y)])
while queue:
    x, y = queue.popleft()
    if not (0 <= x < w and 0 <= y < h) or outside[y][x] or px[x, y][3] >= 128:
        continue
    outside[y][x] = True
    queue.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])


def classe(x, y):
    r, g, b, a = px[x, y]
    if a < 128:
        return None if outside[y][x] else "letra"
    if r + g + b < 120:
        return "preto"
    # Gradiente: do azul #2d85c6 (g baixo) ao ciano #04bacc (g alto).
    return "ciano" if g > 168 else "azul"


rows = round(cols * h / w / 2.1)  # células do terminal ~2,1x mais altas
cw, ch = w / cols, h / rows
cor = {"azul": "$1", "ciano": "$2", "letra": "$3"}
# Densidade pela cobertura da célula: bordas mais suaves.
rampa = " .:-=+*#%@"
out = []
for j in range(rows):
    line, atual = "", None
    for i in range(cols):
        conta = {}
        amostras = 0
        for sy in range(4):
            for sx in range(4):
                x = min(w - 1, int((i + (sx + 0.5) / 4) * cw))
                y = min(h - 1, int((j + (sy + 0.5) / 4) * ch))
                c = classe(x, y)
                amostras += 1
                if c and c != "preto":
                    conta[c] = conta.get(c, 0) + 1
        if not conta:
            line += " "
            continue
        c = max(conta, key=conta.get)
        cobertura = sum(conta.values()) / amostras
        if c != atual:
            line += cor[c]
            atual = c
        line += rampa[max(1, min(9, round(cobertura * 9)))]
    out.append(line.rstrip())
while out and not out[-1].strip():
    out.pop()
print("\n".join(out))
