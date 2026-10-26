#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Valida a sintaxe de property lists no formato OpenStep (Window Maker/wlmaker).

Uso: plistcheck.py [--wings] ARQUIVO...

--wings  modo estrito para arquivos lidos pela WINGs (Window Maker):
         sem comentários e sem vírgula sobrando no fim de arrays.
         Sem a opção, aceita a sintaxe do wlmaker (comentários // e /* */,
         vírgula final em arrays).
"""

import re
import sys

TOKEN = re.compile(r'''
    (?P<space>\s+)
  | (?P<lcomment>//[^\n]*)
  | (?P<bcomment>/\*.*?\*/)
  | (?P<string>"(?:[^"\\]|\\.)*")
  | (?P<data><[0-9a-fA-F\s]*>)
  | (?P<word>[^\s{}()=;,"<>]+)
  | (?P<punct>[{}()=;,])
''', re.VERBOSE | re.DOTALL)


class PlistError(Exception):
    pass


def tokenize(text, wings):
    pos, tokens = 0, []
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            line = text.count('\n', 0, pos) + 1
            raise PlistError(f'linha {line}: caractere inesperado {text[pos]!r}')
        kind = m.lastgroup
        if kind in ('lcomment', 'bcomment') and wings:
            line = text.count('\n', 0, pos) + 1
            raise PlistError(f'linha {line}: comentário não suportado pela WINGs')
        if kind not in ('space', 'lcomment', 'bcomment'):
            tokens.append((kind, m.group(), text.count('\n', 0, pos) + 1))
        pos = m.end()
    return tokens


class Parser:
    def __init__(self, tokens, wings):
        self.tokens, self.i, self.wings = tokens, 0, wings

    def peek(self):
        return self.tokens[self.i] if self.i < len(self.tokens) else ('eof', '', -1)

    def take(self, value=None):
        tok = self.peek()
        if tok[0] == 'eof' or (value is not None and tok[1] != value):
            raise PlistError(f'linha {tok[2]}: esperado {value or "valor"!r}, encontrado {tok[1]!r}')
        self.i += 1
        return tok

    def value(self):
        kind, text, line = self.peek()
        if text == '{':
            return self.dict()
        if text == '(':
            return self.array()
        if kind in ('string', 'word', 'data'):
            self.i += 1
            return text
        raise PlistError(f'linha {line}: valor inválido {text!r}')

    def dict(self):
        self.take('{')
        keys = set()
        while self.peek()[1] != '}':
            kind, key, line = self.take()
            if kind not in ('string', 'word'):
                raise PlistError(f'linha {line}: chave inválida {key!r}')
            if key in keys:
                raise PlistError(f'linha {line}: chave repetida {key}')
            keys.add(key)
            self.take('=')
            self.value()
            self.take(';')
        self.take('}')

    def array(self):
        self.take('(')
        if self.peek()[1] == ')':
            self.take(')')
            return
        while True:
            self.value()
            if self.peek()[1] == ')':
                break
            self.take(',')
            if self.peek()[1] == ')':
                if self.wings:
                    raise PlistError(f'linha {self.peek()[2]}: vírgula final não suportada pela WINGs')
                break
        self.take(')')

    def document(self):
        self.value()
        if self.peek()[0] != 'eof':
            raise PlistError(f'linha {self.peek()[2]}: conteúdo após o fim do documento')


def main(argv):
    wings = '--wings' in argv
    files = [a for a in argv if a != '--wings']
    failed = False
    for path in files:
        try:
            with open(path, encoding='utf-8') as f:
                Parser(tokenize(f.read(), wings), wings).document()
        except (PlistError, UnicodeDecodeError) as e:
            print(f'{path}: {e}', file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
