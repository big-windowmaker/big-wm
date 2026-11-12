# Relatório final — auditoria da instalação real (10/11/2026)

Máquina do desenvolvedor, sessão X11 em uso; Wayland em laboratório
aninhado (wlmaker 31). Detalhes e evidências nos documentos 01–14.

Estados: **Corrigido e validado visualmente**, **Corrigido e validado
funcionalmente**, **Corrigido no código, aguardando teste real**,
**Parcialmente corrigido**, **Não resolvido**, **Bloqueado por limitação
comprovada**.

## Problemas relatados

### 1. Dockapps com aparência defeituosa

- **Sintoma:** todos os dockapps do Dock cortados (8 px de cada lado), sem o
  ladrilho do tema; o controlador do Dock com Abas com as setas pela metade.
- **Causa comprovada:** `IconSize = 48` no `~/GNUstep/Defaults/WindowMaker`
  (WPrefs, 07/11); dockapps são janelas fixas de 64×64 e o Window Maker não as
  redimensiona. Prova: mesma sessão no Xvfb com 48 e com 64.
- **Solução:** com a escolha do usuário, `IconSize = 64` (backup antes); o
  `big-wm-doctor` avisa abaixo de 64.
- **X11:** **Corrigido e validado visualmente** (sessão real, depois do
  reinício: todos inteiros e nítidos). **Wayland:** o defeito não ocorre
  (ladrilhos de 64), visto no laboratório.

### 2. Dock com Abas não respeita a Largura Total com aplicativos abertos

- **Sintoma:** com um aplicativo aberto, faixa vazia de 48 px à esquerda.
- **Causa comprovada:** a detecção das colunas do Dock/Clip (0.2.0, uma
  regressão introduzida na missão anterior) usava o retângulo da janela
  inteira (165 px) em vez do corpo (112 px); o appicon do aplicativo, logo
  acima da prateleira (y 1280–1328), virava uma "coluna do Dock":
  `wm_left=48`.
- **Arquivos:** tabbed-dock `src/core/btd-geometry.[ch]` (novo),
  `src/backend/btd-x11.c`, `src/ui/btd-dock.c`.
- **Testes:** `test-geometry` (6) e `test-fullwidth.sh` (6, Xvfb; a 0.2.0
  falha 4 de 6, a 0.2.1 passa todos).
- **X11:** **Corrigido e validado visualmente** (sessão real: appicon sobre a
  prateleira, `wm_left=0`). **Wayland:** **Corrigido e validado
  funcionalmente** no laboratório (1600 de 1600 com 0, 1 e 2 aplicativos).

### 3. Modo escuro incompleto

- **Window Maker claro no modo escuro.** Causa comprovada: o WPrefs tira do
  arquivo do usuário as chaves iguais às do global; sem o `IconBack`, o
  `big-wm-wmstyle` via "custom" e o `big-wm-theme` nunca trocava o estilo.
  Correção: o valor efetivo (global + usuário). **Corrigido e validado
  funcionalmente** (o arquivo real foi de "custom" a "dark", com as chaves do
  `BigWindowMaker-Dark`); a captura na sessão real ficou pendente porque a
  tela estava bloqueada (o usuário ausente).
- **Tela de bloqueio sempre clara.** Paleta escura no `big-wm-lockscreen`
  (X11; a clara ficou idêntica, 0 pixels de diferença) e `gtklock-dark.css`
  (Wayland). **Corrigido e validado visualmente** em prévia (X11) e no
  laboratório (Wayland); **aguardando teste real** na próxima vez que a tela
  bloquear.
- **Kate (Qt/KDE): áreas de conteúdo brancas no laboratório.** Hipótese do
  `kdeglobals` testada e descartada. **Não resolvido.**
- **gedit: área de texto clara.** Hipótese do `prefer-dark` testada e
  descartada. **Não resolvido.**
- GTK, Kvantum, ícones, XSETTINGS, Dock com Abas, controlador e bandeja já
  seguiam o modo (X11 real e Wayland no laboratório).

### 4. "Reiniciar o Window Maker" não resolvia

- **Achado:** o item funciona (é o `RESTART` interno, o mesmo do `SIGUSR1`;
  relê a configuração). Os defeitos não eram estado em memória: o
  `IconSize` 48 e o estilo claro estavam gravados, o Clip errado era
  regravado pelo próprio reinício e a Largura total é de outro processo.
- **Situação:** **Corrigido e validado funcionalmente** pelas correções
  acima; nenhuma mudança no reinício era necessária.

## Achados na varredura

| # | Problema | Gravidade | Estado |
|---|---|---|---|
| 5 | Dock com Abas morto não voltava (`ExitType=cgroup`, filhos do VSCodium no cgroup) | Alto | Corrigido e validado funcionalmente |
| 6 | Clip sobre o primeiro ícone do Dock depois de perder monitores (WM 0028) | Médio | Corrigido e validado visualmente (sessão real e laboratório) |
| 7 | Gaveta sobre o wmake-zones em outra altura de tela (WM 0029) | Médio | Corrigido e validado visualmente no laboratório com o WMState real; na sessão real, aguardando (tela bloqueada) |
| 8 | Window Maker 15–22 instalado sem estar no repositório `packages` | Alto | Corrigido (windowmaker 24 no `packages`, branch local) |
| 9 | autostart do Window Maker sem `+x` (o `xset s 1800` nunca roda) | Médio | Não resolvido: arquivo do usuário, não alterado; o `big-wm-doctor` aponta |
| 10 | 15 temporários órfãos do WINGs | Baixo | Parcialmente corrigido: `big-wm-doctor --repair` os move (não executado) |
| 11 | Reinício da máquina às 06:34:57 (`reboot`, `session-3.scope`; também 09/11 00:10 e 03:07) | Crítico | Não resolvido: não veio dos comandos da auditoria; causa não determinada |
| 12 | Só um monitor depois do reinício; dois LG ligados depois do boot não aparecem | Alto | Causa fora do software: sem HPD nem EDID pelo AUX no kernel (`detect`/`on` no sysfs), os mesmos que não acordaram às 06:31. Voltaram às 13:31 e o vigia aplicou sozinho o perfil dos três (validado na sessão real). Corrigidos junto: fundo preto nos monitores novos (WM 0030, `_XROOTPMAP_ID` 3440→9440 px), janela de Monitores parada, monitores que voltam um a um (Display Resolution 510cc53) |
| 13 | wmnet vazio por alguns segundos depois de reiniciar o Window Maker | Cosmético | Não resolvido (comportamento do wmnet) |
| 14 | Cubos genéricos para dockapps com `AutoLaunch = No` na lista de 1080 | Cosmético | Configuração do usuário, não alterada |

Ferramenta nova: `big-wm-doctor` (só leitura; `--json`; `--repair` com um
reparo só, reversível), com teste.

## Matriz de validação

| Recurso | X11 | Wayland |
|---|---|---|
| Dockapps renderizados / nítidos | validado na sessão real | validado no laboratório |
| Dock e Clip estáveis | validado (sessão real: Clip; laboratório: gaveta) | validado no laboratório |
| Largura Total com aplicativos | validado na sessão real | validado no laboratório |
| Modo escuro | Window Maker validado funcionalmente; GTK/Qt/ícones na sessão real; Kate e gedit não resolvidos | laboratório; gedit não resolvido |
| Modo claro | prévia da tela de bloqueio idêntica; resto não revisto visualmente | não testado |
| WPrefs | não testado visualmente | não testado |
| Menus e molduras | molduras escuras no laboratório; menu raiz não capturado | laboratório (menus não capturados) |
| Tela de bloqueio | prévia clara/escura | gtklock escuro no laboratório |
| wmake-zones | visível depois do WM 24 (laboratório) | adotado no laboratório |
| Autostart / sem duplicatas | sessão real | laboratório |
| Reinício de componentes | sessão real (WM com `SIGUSR1`; dock pela unidade) | não testado |
| Persistência | sessão real depois do reinício da máquina | não testado |
| Vários monitores | defeito medido com 3; correção verificada com 1 | não testado |
| Pacotes atualizados | instalados e conferidos (`/proc/<pid>/exe`) | os mesmos pacotes |
| Testes de regressão | tabbed-dock 12/12; big-wm tests/run.sh passa | — |

Sessão Wayland real: **não testada** (a sessão X11 em uso não foi encerrada).

## Resumo de saúde

- Problemas encontrados: **17** — 7 nos relatados (o modo escuro conta como 4: Window Maker, tela de bloqueio, Kate, gedit) e 10 na varredura (5–14).
- Corrigidos: **8** (1, 2, 3a, 3b, 5, 6, 7, 8); explicado e resolvido pelas
  correções: **1** (4); parcialmente: **1** (10).
- Validados visualmente na sessão real: **3** (1, 2, 6); em laboratório ou
  prévia: **2** (3b, 7); funcionalmente: **3** (3a, 4, 5); o 8 foi conferido
  nos pacotes e no git.
- Não resolvidos ou fora do alcance: **7** (3c Kate, 3d gedit, 9, 11, 12, 13,
  14); validações pendentes na sessão real: **2** (3a e 7, com a tela
  bloqueada).
- Crítico restante: o reinício não explicado (11).
- Regressão eliminada: a Largura total (introduzida na 0.2.0).
- Regressões introduzidas: nenhuma detectada (testes e prévia da tela de
  bloqueio, que ficou idêntica no modo claro).
- Desempenho: sem mudança medível (Dock com Abas 19/43/28 ms e 0 de CPU
  parado; componentes do Big WindowMaker 0,57% de CPU e 178 MB de PSS com a
  sessão parada).

## O que não foi enviado

Nenhum `git push`: wmaker, packages e big-wm em `fix/auditoria-2026-11-10`
(locais); o tabbed-dock tem três commits à frente do GitHub (0.2.1).
