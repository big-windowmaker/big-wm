# 14 — Testes do Spectacle

`tests/screenshot/test_kwin_screenshot.py` (no `tests/run.sh`): funções com
um grim e um wmake-zones falsos, e o serviço num dbus-daemon próprio
(`screen`, tamanho, stride, escala, bytes completos e fim do pipe; "Tela
atual" uma tela; área de trabalho inteira). 7 testes; com o serviço antigo, o
teste trava no pipe.

Matriz:

| Recurso | 1 monitor | Vários |
|---|---|---|
| Todas as telas | ok | ok (laboratório e real) |
| Tela atual | ok | ok (laboratório; real pela janela ativa até o próximo login) |
| Janela ativa | inalterada | inalterada |
| Selecionar janela | ok | ok (laboratório) |
| Região retangular | ok | ok (laboratório) |
| Esc | ok | ok |
| Digitar depois | ok | ok |
| Janela do Spectacle no monitor do ponteiro | ok | ok |
| Copiar a imagem | não testado |
