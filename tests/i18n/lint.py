#!/usr/bin/python3 -I
# SPDX-License-Identifier: GPL-2.0-or-later
"""Texto visível sem marcação do gettext nas ferramentas da sessão.

Falha (código 1) quando acha:
  - o esquema antigo de idioma: def tr(pt, en), "if PT else", startswith("pt")
    para escolher texto, tabelas {"pt": {...}};
  - em Python, um literal com letras como texto de interface (label=, title=,
    text=, tooltip_text=, placeholder_text=, set_title/set_label/set_text/
    set_markup/set_tooltip_text/format_secondary_*/add_button, notify-send)
    fora de _(), N_(), ngettext();
  - em shell, notify-send/zenity/yad com texto literal fora de $(_t ...);
  - em C, notify-send com texto literal fora de _().
Uso: lint.py ARQUIVO...   (ver tests/run.sh)
"""
import ast
import re
import sys

KWARGS = {"label", "title", "text", "tooltip_text", "placeholder_text", "secondary_text"}
METHODS = {"set_title", "set_label", "set_text", "set_markup", "set_tooltip_text",
           "set_tooltip_markup", "format_secondary_text", "format_secondary_markup",
           "add_button", "set_placeholder_text", "new_with_label", "new_with_mnemonic", "set_name"}
MARKS = {"_", "N_", "ngettext", "pgettext"}
# Literais que não se traduzem: marcas, nomes de programas, símbolos.
ALLOWED = re.compile(r"^(Big WindowMaker|Dockapps|Terminal|XWayland|Wayland|X11|OPENSTEP|AirPlay|"
                     r"HP Device Manager|HP Scan|scrcpy|UxPlay|big-wm[-\w]*|[\W\d_]*)$")
OLD = re.compile(r'def tr\(pt, en\)|\bif PT else\b|startswith\(["\']pt["\']\)|^\s*["\']pt["\']: \{|'
                 r'\bTEXT\[["\']pt["\']\]|\$\{LANG%%_\*\} in$')


def has_letters(s):
    return bool(re.search(r"[A-Za-zÀ-ÿ]{2}", s)) and not ALLOWED.match(s.strip())


def python(path, src, out):
    tree = ast.parse(src, path)
    marked = set()
    for n in ast.walk(tree):
        if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id in MARKS:
            for a in ast.walk(n):
                marked.add(id(a))
        elif isinstance(n, ast.Subscript):
            # T["chave"]: a chave não é texto
            marked.add(id(n.slice))

    def check(node, where):
        for c in ast.walk(node):
            if (isinstance(c, ast.Constant) and isinstance(c.value, str) and id(c) not in marked
                    and has_letters(c.value)):
                out.append(f"{path}:{c.lineno}: texto sem _() em {where}: {c.value[:60]!r}")

    for n in ast.walk(tree):
        if not isinstance(n, ast.Call):
            continue
        name = n.func.attr if isinstance(n.func, ast.Attribute) else getattr(n.func, "id", "")
        for k in n.keywords:
            if k.arg in KWARGS and isinstance(k.value, (ast.Constant, ast.JoinedStr, ast.BinOp)):
                check(k.value, k.arg + "=")
        if name in METHODS and name != "set_name":
            for a in n.args:
                if isinstance(a, (ast.Constant, ast.JoinedStr, ast.BinOp)):
                    check(a, name + "()")
        # subprocess.run(["notify-send", ..., "Título", "Corpo"])
        for a in n.args:
            if (isinstance(a, ast.List) and a.elts and isinstance(a.elts[0], ast.Constant)
                    and a.elts[0].value == "notify-send"):
                args = a.elts[1:]
                skip = False
                for e in args:
                    if skip:
                        skip = False
                        continue
                    if isinstance(e, ast.Constant) and str(e.value).startswith("-"):
                        skip = e.value in ("-a", "-i", "-t", "-u", "-c", "-h", "--app-name", "--icon")
                        continue
                    check(e, "notify-send")


def shell(path, src, out):
    for i, line in enumerate(src.splitlines(), 1):
        if line.lstrip().startswith("#"):
            continue
        m = re.search(r"\b(notify-send|zenity|yad)\b(.*)", line)
        if not m:
            continue
        rest = re.sub(r"--?[\w-]+(=\S+)?|\$\(_t[^)]*\)|\"\$[\w{}#%/*.@-]+\"|\$[\w{}]+|2>/dev/null|\\$", " ", m.group(2))
        # sobram textos entre aspas com letras
        for lit in re.findall(r'"([^"$]*)"|\'([^\']*)\'', rest):
            text = lit[0] or lit[1]
            if has_letters(text):
                out.append(f"{path}:{i}: texto sem _t em {m.group(1)}: {text[:60]!r}")


def c(path, src, out):
    for m in re.finditer(r'execlp\("notify-send"(.*?)\(char \*\)NULL\)', src, re.S):
        # o valor de -a (nome do aplicativo) e de -i (ícone) não é texto
        body = re.sub(r'"-[ai]",\s*("[^"]*"|\w+(\s*\?[^,]*:[^,]*)?)|_\("[^"]*"\)', " ", m.group(1))
        for text in re.findall(r'"([^"]*)"', body):
            if has_letters(text) and text != "notify-send":
                line = src[:m.start()].count("\n") + 1
                out.append(f"{path}:{line}: texto sem _() em notify-send: {text[:60]!r}")


def main(paths):
    out = []
    for path in paths:
        with open(path, encoding="utf-8") as f:
            src = f.read()
        lines = src.splitlines()
        for i, line in enumerate(lines, 1):
            if OLD.search(line) and "i18n-lint: ignorar" not in line + (lines[i - 2] if i > 1 else ""):
                out.append(f"{path}:{i}: escolha de idioma antiga (use o gettext): {line.strip()[:70]}")
        first = src.split("\n", 1)[0]
        if path.endswith(".c"):
            c(path, src, out)
        elif "python" in first:
            python(path, src, out)
        elif "sh" in first:
            shell(path, src, out)
    for line in out:
        print(line)
    return 1 if out else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
