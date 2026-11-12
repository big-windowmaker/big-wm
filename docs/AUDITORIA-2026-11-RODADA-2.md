# Auditoria de correções — novembro de 2026, segunda rodada

Continuação de [BUGFIX-AUDIT.md](BUGFIX-AUDIT.md). Cada item diz o que foi
encontrado, o que mudou, onde, e como foi verificado. "Verificado" significa
executado e observado (saída, captura de tela, medição); o que não foi
executado está marcado como pendente, sem estimativa no lugar do resultado.

Ambiente de teste: a máquina do mantenedor (Manjaro, X11, Window Maker,
monitores 3440×1440 + outro) e, para o que não pode rodar na sessão real,
Xvfb/Xephyr aninhados com o Window Maker compilado do ramo.

## Ramos

| Repositório | Ramo | O que tem |
|---|---|---|
| big-windowmaker/wmaker | `big-wm/fixes-2026-11` | Dock visível ao trocar de monitor e "Restaurar posição do Dock"; Lupa rápida; Salvar sessão sem duplicar |
| big-windowmaker/packages | `fix/gaming-desktop-2026-11` | windowmaker 8 (patches 0009–0011), big-dockapps 6 (wmcliphist), gtk-nocsd 4.8-2 |
| big-windowmaker/big-wm | `fix/gaming-desktop-2026-11` | o resto deste documento |
| ruscher/bigamemode | `fix/window-maker-desktop` | Window Maker e wlmaker reconhecidos como o desktop |
| big-comm/ashyterm | [PR #60](https://github.com/big-comm/ashyterm/pull/60) (patch em [patches/](patches/)) | esconder os botões do headerbar quando o gerenciador desenha a barra |

## 1. WMDock sumia ao trocar de monitor

**Causa.** Ao escolher outro monitor, o Dock ia para a borda interna, na
emenda entre dois monitores, abaixo das janelas; a posição salva o mantinha
lá. Na máquina real o ladrilho principal estava em x=3376, na emenda.

**Correção** (wmaker, patch 0009): o Dock vai para a borda externa livre do
Clip, é conferido dentro de um monitor e trazido à frente; "Restaurar posição
do Dock" no menu do Dock e no menu Área de trabalho (comando `RESET_DOCK`);
`big-wm-dock-reset` para quando o menu não está ao alcance.

**Verificado** no Xephyr com três monitores: posição clássica, emenda salva,
monitor inválido, comando, escolha de cada monitor pelo menu, dockapps
acompanhando. **Pendente:** recuperar o Dock na sessão real (instalar o
windowmaker 8, entrar de novo e, se preciso, `big-wm-dock-reset`).

## 2. Steam: jogos não abriam no X11

**Causa** (reproduzida 5 de 5 vezes): o gtk-nocsd no `LD_PRELOAD`, junto com
o `gameoverlayrenderer.so` da Steam, travava o `_v2-entry-point` do Steam
Linux Runtime (um lançamento do Streets of Rage 4 ficou parado 1h40).

**Correção** (gtk-nocsd 4.8-2): a biblioteca se desliga nos processos da
Steam e dos jogos (SteamAppId, SteamGameId, STEAM_COMPAT_APP_ID,
STEAM_RUNTIME, SteamClientLaunch, `steam`/`steam.sh`) e, ao se retirar do
`LD_PRELOAD`, tira só a si mesma.

**Verificado** com a biblioteca corrigida na Steam real: Streets of Rage 4
(Proton) abre em 13 s, duas vezes seguidas, menu, áudio, teclado,
minimizar/restaurar, saída com código 0; Shadow of the Tomb Raider (nativo)
abre pelo lançador da Feral e renderiza; R.E.P.O. (Proton) em tela cheia
3440×1440 com Alt+Tab. **Pendente:** jogar uma partida do SOR4 (manual),
DOOM Eternal e os jogos que pediam atualização; reiniciar a Steam
normalmente depois de instalar o gtk-nocsd 4.8-2.

## 3. Big Game Mode no Window Maker

O Big Game Mode pausava ou rebaixava processos do próprio desktop: `wmaker`,
`wmsetbg`, `wlmaker` e os componentes `big-wm-*` agora estão na lista do
desktop (ruscher/bigamemode; testes do cargo: 975 + 29). **Não feito:** o
comparativo de FPS com e sem o Game Mode. Não houve medição; o documento não
traz número.

## 4. Área de transferência e wmcliphist

"Copiar URL" no Firefox: copiar link, Ctrl+L/Ctrl+C, `writeText` e
`execCommand` funcionaram em todos os casos testados, com e sem gtk-nocsd e
wmcliphist; a falha **não foi reproduzida**. No wmcliphist (big-dockapps 6)
foram corrigidos: a PRIMARY no lugar da CLIPBOARD (o item escolhido não
colava com Ctrl+V), texto só em STRING (acentos viravam "aÃ§Ã£o"), o
histórico que não assumia a seleção quando o dono fechava e a consulta
periódica no lugar do aviso do XFixes. Itens marcados como senha
(`x-kde-passwordManagerHint`) não entram; histórico só em memória por padrão
(`persist no`).

## 5. ARandR: layout não voltava no login

Nada executava o script salvo em `~/.screenlayout`. `big-wm-monitors
--restore` (antes do Window Maker) aplica o layout escolhido ou o mais
recente, só quando todas as saídas dele estão conectadas, e volta ao
automático se o xrandr falhar. Aplicado na sessão real. **Pendente:**
confirmar depois de um reinício de verdade.

## 6. Área de trabalho > Salvar sessão

O que cada coisa guarda:

| | Onde | Quando |
|---|---|---|
| Sessão (aplicativos abertos, área de trabalho ativa) | `~/GNUstep/Defaults/WMState`, chave `Applications` | Salvar sessão; ao sair só com SaveSessionOnExit |
| Dock, Clip, gavetas, áreas de trabalho | `WMState` | ao mudar e ao sair |
| Aparência (tema claro/escuro, ícones, papel de parede) | `~/.config/big-wm/theme`, settings.ini, gsettings, `WindowMaker` | na hora, pelos menus |
| Monitores | script do ARandR + `big-wm-monitors` | ao salvar no ARandR |
| Dock com abas | `~/.config/big-tabbed-dock/dock.conf` (escrita atômica, com `.bak`) | na hora |

**Problemas encontrados** no `WMState` real: o big-wm-tabbed-dock na sessão
salva (a sessão já o inicia: um segundo dock ao entrar) e o Firefox duas
vezes (uma por janela: o Firefox restaura as próprias janelas e cada
lançamento extra abria outra).

**Correção:** patch 0011 do windowmaker (um programa com várias janelas num
processo, pelo `_NET_WM_PID`, é salvo uma vez; uma entrada antiga de algo
marcado DontSaveSession não é relançada) e DontSaveSession para o Dock com
abas, Buscar aplicativos e a Lupa (padrão e migração). Programas sem comando
de reinício confiável (o Window Maker guarda o que a janela informa) podem
não voltar: a sessão salva não promete mais do que isso.

**Verificado** no Xvfb, Window Maker antes e depois, mesmo teste: dois xterm
e um processo GTK com duas janelas; antes 4 entradas (o GTK duas vezes),
depois 3; uma entrada antiga DontSaveSession relançada antes, ignorada
depois. **Ação do usuário:** depois de instalar, Salvar sessão de novo (ou
Limpar sessão) para trocar a lista antiga.

## 7. Tema claro/escuro

O GTK recebia o tema pelo `GTK_THEME` do login: janelas abertas não mudavam
e programas abertos por processos com o ambiente antigo saíam no tema antigo
(na máquina real: modo escuro gravado, `GTK_THEME=OneStepBack` herdado). O
GTK 4/libadwaita recebia um tema GTK 3 à força.

Agora, na sessão X11, o `xsettingsd` publica `Net/ThemeName` e
`Net/IconThemeName`; Tema claro/escuro e o menu de ícones o atualizam e as
janelas GTK 3 abertas mudam na hora. Sem `GTK_THEME` global: libadwaita
segue o `color-scheme`. Qt pelo Kvantum (QTStepOpenStep/Dark), Window
Maker, wlmaker, dunst, Dock com abas e bandeja como antes. Dockapps X11
antigos têm as cores no próprio código e não mudam. Sem o xsettingsd, no
Wayland ou com `BIGWM_GTK_THEME=1`, o comportamento anterior.

**Verificado:** uma janela GTK 3 aberta passou de OneStepBack a
OneStepBack-Dark sem reabrir (Xvfb, Window Maker, xsettingsd 1.0.4).

## 8. Lixeira no Dock com abas

Ícone no fim da prateleira (vazia/cheia pelo GIO, sem consulta periódica),
abrir, esvaziar com confirmação (Cancelar é o padrão). Verificado: vazia,
cheia, Enter cancela, esvaziar deixa 0 itens.

## 9. Conjuntos de ícones

Menu Aparência > Tema de ícones com todos os temas XDG instalados (cache pela
data dos diretórios), o atual marcado; `big-wm-theme icons NOME`. Na
máquina real: 8 temas listados.

## 10. Bandeja (big-wm-systemtray) e BigLinux Microfone

Paginação conferida no modo em que o Big WindowMaker usa a bandeja (no
Dock): 1/3, 2/3, 3/3, 2/3 limpos; ao fechar os ícones, 1/1 e o ladrilho
limpo. O "fantasma" de ícones da página anterior só acontece com
`--non-wmaker` sem cor de fundo, que a sessão não usa; nada mudado. O
BigLinux Microfone não tem código de bandeja (nem no upstream): não é falha
da bandeja.

## 11. Dock com abas em todos os monitores

Preferências > Monitor: principal, um monitor, "Todos os monitores,
espelhado" (`all`) ou "cada um com as suas janelas" (`all-independent`).
Detalhes e capturas em [tabbed-dock/uso-e-configuracao.md](https://github.com/big-windowmaker/tabbed-dock/blob/main/docs/uso-e-configuracao.md).
**Verificado** com três monitores RandR (Xvfb): 3 docks; independente 1/0/2
janelas e, depois de mover uma janela, 0/1/2; largura total 1000/1100/1000
px. No Wayland a posição das janelas não é conhecida: cada dock mostra
todas.

## 12. Terminal: xterm no lugar do AshyTerm

O `xdg-terminal-exec` não tinha lista da sessão e escolhia o xterm. Listas
`windowmaker-`/`wlmaker-xdg-terminals.list` com o AshyTerm; a escolha do
usuário continua valendo. XTerm de reserva com fundo escuro, DejaVu Sans
Mono 11 e margem (xrdb).

## 13–14. Buscar aplicativos e Executar

Janela GTK com a barra do Window Maker (arrasta, minimiza, fecha), no lugar
do rofi. Categorias numa lista fixa (sem popover), busca em nome, nome
genérico, descrição, palavras-chave e categoria em qualquer categoria, roda
só rola, setas/Enter/Esc, contagem. Executar sem shell (shlex), modo shell
explícito, no terminal opcional, Tab completa, histórico opcional, erro na
janela. **Verificado** no Xvfb: abre em 519 ms; 6 cliques da roda sem mudar
a seleção; busca + setas + Enter; categoria + busca; Esc.

## 15. AshyTerm com a barra do Window Maker

GTK4/libadwaita: o headerbar é a decoração e o gtk-nocsd não alcança o GTK
4. O Window Maker passa a desenhar a barra (`IgnoreDecorationChanges`) e o
AshyTerm (patch em [patches/](patches/)) esconde os próprios botões de
janela quando o gerenciador desenha a barra (`_NET_FRAME_EXTENTS`); abas,
busca e menu ficam. O atributo só é gravado quando o AshyTerm instalado tem
o patch, para não deixar dois conjuntos de botões. **Pendente:** o patch
entrar no AshyTerm: [big-comm/ashyterm#60](https://github.com/big-comm/ashyterm/pull/60)
(2026-11-03), que também confere de novo quando a janela ganha o foco e não
muda nada no xfwm4, Mutter, Muffin e KWin (medido no X11).

## 16. Lupa (Magnify)

Captura por MIT-SHM em buffers reaproveitados, uma imagem por quadro, 30
quadros/s sem redesenhar o que não mudou; roda do mouse (1x a 16x, frações),
botão do meio e "0" voltam a 2x, pixel perfeito ou suavizado, tamanho livre
(mínimo 120×80), acima das outras janelas.

Medido no Xvfb (1920×1080, janela 600×400 a 2x, ponteiro em movimento,
média de 10 s, CPU em % de um núcleo):

| | atualizações/s | CPU wmagnify | CPU servidor X |
|---|---|---|---|
| antes | 4 | 2,39 % | 4,88 % |
| depois, pixel perfeito | 29 | 1,20 % | 1,38 % |
| depois, suavizado | 29 | 3,64 % | 1,39 % |

## 17. Sacudir o mouse para localizar o ponteiro

`big-wm-locate-pointer` (X11): detector de inversões de direção em pixels
proporcionais ao monitor, sem polling, desligado com janela em tela cheia e
com ponteiro invisível; menu Configurações > Localizar o ponteiro ao
sacudir. Medido: 0 µs de CPU em 10 s parado; 28,8 ms em 10 s de movimento a
480 eventos/s. **Não feito:** Wayland (wlmaker).

## Memória (Xvfb, Window Maker, mesma rodada, cache quente)

| Processo | PSS | RSS |
|---|---|---|
| Dock com abas, um monitor | 11,8 MiB | 42,8 MiB |
| Dock com abas, três monitores (um processo) | 13,5 MiB | 45,1 MiB |
| big-wm-locate-pointer | 0,4 MiB | 3,6 MiB |
| xsettingsd | 0,5 MiB | 5,0 MiB |
| Buscar aplicativos aberto | 34,1 MiB | 78,4 MiB |

A primeira medição do dock, a frio, deu PSS 20 MiB; a tabela usa a
comparação na mesma condição. O Buscar aplicativos só existe enquanto está
aberto.

## Não feito nesta rodada

- Comparativo de FPS com e sem o Big Game Mode.
- Revisão de paridade Wayland além do que está anotado em cada item.
- Conferência da ISO e do workflow de build da ISO (os pacotes novos entram
  como dependências do big-wm-session: big-wm-locate-pointer e xsettingsd).
- Recuperar o WMDock e validar o ARandR depois de reiniciar, na máquina
  real (dependem de instalar os pacotes).
