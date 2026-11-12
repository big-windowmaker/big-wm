#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Menus padrão traduzidos (.pt_BR) contra os em inglês.

Uso: menus.py [--wings] MENU_EM_INGLES MENU_PT_BR

Os modelos de menu (data/GNUstep/Defaults/WMRootMenu[.pt_BR],
data/wlmaker/RootMenu.plist[.pt_BR]) não passam pelo gettext: cada idioma é
um arquivo (o big-wm-firstrun escolhe pelo LANG e guarda o hash de cada
versão distribuída). Aqui se confere que o pt_BR tem a mesma estrutura do
inglês (submenus e itens na mesma ordem), os mesmos comandos, atalhos e
ações, e nenhum rótulo esquecido em inglês. Sai com 1 e lista as
divergências.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from plistcheck import tokenize  # noqa: E402

# Rótulos iguais nos dois idiomas: nomes próprios e termos que o pt_BR usa
# como no inglês (glossário: Dock, Clip, dockapp, Internet, Terminal...).
SAME = {
    "Big WindowMaker", "Terminal", "Dock", "Clip", "Dockapps", "WPrefs", "Internet", "BigLinux",
    "Window Maker", "wlmaker", "OPENSTEP", "NeXTSTEP", "Wayland", "X11", "Bluetooth", "Wi-Fi",
    "Layout", "Spectacle", "WindowMaker (WPrefs)", "Thunar", "Firefox", "AirPlay", "scrcpy",
    "Kvantum", "Qt", "GTK",
}


class Parser:
    def __init__(self, tokens):
        self.tokens, self.i = tokens, 0

    def value(self):
        kind, text, _line = self.tokens[self.i]
        self.i += 1
        if text == "(":
            out = []
            while self.tokens[self.i][1] != ")":
                out.append(self.value())
                if self.tokens[self.i][1] == ",":
                    self.i += 1
            self.i += 1
            return out
        if text == "{":
            out = {}
            while self.tokens[self.i][1] != "}":
                key = self.value()
                self.i += 1  # =
                out[key] = self.value()
                self.i += 1  # ;
            self.i += 1
            return out
        if kind == "string":
            return re.sub(r"\\(.)", r"\1", text[1:-1])
        return text


def load(path, wings):
    with open(path, encoding="utf-8") as f:
        tokens = [t for t in tokenize(f.read(), wings) if t[0] not in ("space", "lcomment", "bcomment")]
    return Parser(tokens).value()


def prompt(value):
    """O texto da pergunta do %a(...) do Window Maker é traduzido; o resto
    do comando, não."""
    return re.sub(r"%a\([^)]*\)", "%a()", value) if isinstance(value, str) else value


def is_submenu(node):
    return isinstance(node, list) and len(node) >= 2 and isinstance(node[1], list)


def compare(en, pt, where, out):
    if is_submenu(en) != is_submenu(pt):
        out.append(f"{where}: um é submenu e o outro não ({en[0]!r} / {pt[0]!r})")
        return
    label_en, label_pt = en[0], pt[0]
    here = f"{where} > {label_pt}" if where else label_pt
    if label_en == label_pt and label_en not in SAME and any(c.isalpha() for c in label_en):
        out.append(f"{here}: rótulo em inglês no menu pt_BR")
    if is_submenu(en):
        if len(en) != len(pt):
            out.append(f"{here}: {len(pt) - 1} itens no pt_BR, {len(en) - 1} no inglês "
                       f"(inglês: {[e[0] for e in en[1:]]}; pt_BR: {[p[0] for p in pt[1:]]})")
        for e, p in zip(en[1:], pt[1:]):
            compare(e, p, here, out)
    elif [prompt(c) for c in en[1:]] != [prompt(c) for c in pt[1:]]:
        out.append(f"{here}: comando diferente: inglês {en[1:]!r}, pt_BR {pt[1:]!r}")


def main(argv):
    wings = "--wings" in argv
    paths = [a for a in argv if a != "--wings"]
    if len(paths) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    en, pt = (load(p, wings) for p in paths)
    out = []
    compare(en, pt, "", out)
    for line in out:
        print(f"{paths[1]}: {line}")
    return 1 if out else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
