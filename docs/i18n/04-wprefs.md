# i18n pt_BR do WPrefs (Preferências do Window Maker)

Agente i18n-wmaker, 09/11/2026. Mesmo worktree e branch de
[03-windowmaker-x11.md](03-windowmaker-x11.md) (`feat/i18n-pt-br-wmaker`,
commit `92980df1`).

## Problema

O WPrefs só tinha `WPrefs.app/po/pt.po` (português europeu, 2001, com
"ficheiro", "rato", "Gravar"). Com `LANG=pt_BR.UTF-8` o glibc procura
`pt_BR` e depois `pt`: sem `pt_BR/LC_MESSAGES/WPrefs.mo`, a sessão
brasileira via o WPrefs em português europeu incompleto (o que faltava no
`pt.po` aparecia em inglês). O mesmo valia para o wmgenmenu, e para as
entradas sem tradução do `WindowMaker.mo` pt_BR, que caíam no `pt`.

## Solução

- `WPrefs.app/po/pt_BR.po` criado pelo mecanismo do projeto
  (`make -C WPrefs.app/po WPrefs.pot` e `make update-lang PO=pt_BR`), cabeçalho
  pt_BR (UTF-8, `Plural-Forms: nplurals=2; plural=(n > 1);`, fonte: catálogos
  pt_BR do GTK 3/GLib/coreutils instalados) e `pt_BR.po` no `EXTRA_DIST`.
- 592/592 mensagens (27 idênticas ao inglês de propósito: nomes de programas
  do menu de exemplo, "OK", "Mod1 (Alt)" etc.). O `pt.po` entrou só como
  sugestão (`export_po_work.py --suggest-from`, 289 sugestões), reescrito em
  pt_BR.
- Vocabulário igual ao do Window Maker X11 (ver 03): Minimizar, Sombrear,
  Manter sobre outros, canto ativo (Hot Corner), janela ativa/inativa,
  miniatura (miniwindow), trazer à frente/enviar para trás (raise/lower),
  monitor (head), diretório.
- Lotes: `~/.cache/bwm-wt/i18n/WPrefs/lote-00..09/{pending,output}.jsonl`
  (dois tradutores, mesmo glossário) e as correções do laboratório em
  `~/.cache/bwm-wt/i18n/WPrefs/revisao-laboratorio/`.

## Teste no laboratório (todas as seções)

WPrefs do build com os catálogos do DESTDIR (`NLSPATH`), `LANG=pt_BR.UTF-8`,
Xvfb próprio. Capturas em `~/.cache/bwm-wt/i18n/wmaker-work/shots/wprefs/`:
`ptbr-a0..a15.png` (primeira passada, as 16 seções: Foco, Manipulação de
janelas, Menu, Ícones, Ergonomia, Caminhos, Dock, Áreas de trabalho, Outras
configurações, Menu de aplicativos, Atalhos de teclado, Cantos ativos, Mouse,
Aparência, Fontes, Usuário experiente) e `fix-*.png` (depois das correções),
`fix-teclado-lista.png` e `../ptBR-final-kb*.png` (lista de ações rolada).

Cortes encontrados na primeira passada e corrigidos:

| msgid | antes | depois | onde |
|---|---|---|---|
| OFF | DESL. ("DESI" em 30 px) | Não | Janelas (Mod+roda), Ícones, Foco, Cantos |
| Left/Right | Esquerda/direita (quebrava "d ireita") | Laterais | Ergonomia |
| More colors for\nWindow Maker | Mais cores para o\nWindow Maker | Mais cores p/\nWindow Maker | Outras configurações |
| Window List Menu | Menu da lista de janelas | Lista de janelas | Mouse |
| Mouse Wheel Tilt | Inclinação da roda do mouse | Roda inclinada | Mouse |
| Sample Text | Texto de exemplo | Exemplo | Fontes |
| Size | Tamanho | Tam. | Fontes (coluna) |
| Disabled Item | Item desativado | Desativado | prévia da Aparência |
| Maximize active window top left corner (e os 3 outros cantos) | Maximizar janela ativa no canto ... | Maximizar no canto ... | Atalhos |
| Raise/Lower window under mouse pointer | Trazer à frente/enviar para trás a janela sob o ponteiro | Alternar frente/trás da janela sob o ponteiro | Atalhos |
| Move to right/bottom/left/top head (e o inverso) | Mover para monitor à direita/abaixo/... | Mover ao monitor (dir./abaixo/esq./acima) | Atalhos |
| Mark window: bring marked window here | ... trazer a janela marcada para cá | ... trazer a marcada para cá | Atalhos |
| Move window to last used workspace | Mover janela para a última ... | Mover janela à última área de trabalho usada | Atalhos |

Fica com corte leve aceito: "Título do menu" na prévia da Aparência (o mesmo
msgid "Menu Title" é o rótulo da lista de texturas e do seletor de fontes,
onde cabe; sem `msgctxt` no código não dá para separar). Os outros rótulos
estreitos que o tradutor encurtou: "Mover/redim. opaco", "Incremento:",
"[Barra de redim.]", "Rec." (Coll.), "Topo/base".

en_US.UTF-8 mostra o WPrefs em inglês e es_ES.UTF-8 em espanhol (há
`es.po`), com o painel de arquivo do WINGs em inglês (sem `es.po` do WINGs):
`../enUS-wprefs-janelas.png`, `../esES-wprefs-janelas.png`.

## Achado fora da i18n

Ao abrir a seção Menu de aplicativos, o WPrefs avisa "Comando de menu
inválido "RESET_DOCK" com o rótulo "Restaurar posição do Dock" foi removido"
(log: `parseCommand(Menu.c:1050): comando desconhecido 'RESET_DOCK'`): o
comando novo do patch 0009 do wmaker não foi ensinado ao editor de menu do
WPrefs (`WPrefs.app/Menu.c`). Salvar o menu pelo WPrefs tiraria o item do
menu raiz. Depois desse aviso o WPrefs deixou de trocar de seção nesse
laboratório e precisou ser reiniciado (não investigado). Correção sugerida
(fora do meu escopo): acrescentar `RESET_DOCK` à tabela de comandos
internos do `Menu.c`, com rótulo `N_("Restore Dock Position")`.

## Limitações

- Balões de ajuda (Ajuda em balões) não foram abertos um a um; foram
  conferidos pelo `check_ui_text.py` (texto longo, quebra de linha livre).
- Revisão independente pendente.
