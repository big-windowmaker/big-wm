# Relatório final — tema e dockapps no Wayland

## Causas-raiz

1. **Lançadores do Clip e todo o WMDock presos no tema antigo.** O
   wlmdock, ao recomeçar com um tema (`execv("/proc/self/exe")`), passava a
   se chamar `exe`; o wlmaker procura os docks pelo nome `wlmdock` e não os
   achava mais (nem SIGHUP do tema, nem SIGUSR1 dos lançadores). Correção:
   wlmaker patch 0145 (o wlmdock fixa o nome; o Clip também o reconhece pelo
   executável).
2. **Dock com Abas "travado" no Wayland.** O controlador nunca era iniciado:
   o `big-wm-dockapps` comparava só 15 caracteres do nome e tomava a barra
   `big-wm-tabbed-dock` pelo controlador. Correção: big-wm,
   `big-wm-dockapps` compara o nome completo.

## Critérios

| # | Item | Estado |
|---|---|---|
| 1 | Clip acompanha claro/escuro | Corrigido e testado (laboratório; sessão real pelos logs), aguardando captura real |
| 2–7 | firefox, thunar, Welcome, Display, codium-insiders, onboard | Corrigido e testado (laboratório), aguardando captura real |
| 8–11 | ladrilho do WMDock, AshyTerm, rofi, Steam | Corrigido e testado (sessão real, captura depois do SIGHUP; laboratório) |
| 12 | Outros ícones (preferências do Dock com Abas no Clip, dockapps X11) | Corrigido e testado (laboratório) |
| 13 | Claro sem logout | Corrigido e testado (unidades do Dock reiniciadas, sem logout) |
| 14 | Escuro reaplicado | Corrigido e testado |
| 15 | Trocas consecutivas | Corrigido e testado (4 na sessão real, 10 rápidas no laboratório) |
| 16 | Dockapps não perdem eventos | Testado (ponteiro real no laboratório) |
| 17–19 | Dock com Abas, controlador, menu | Corrigido e testado (laboratório); na sessão real o controlador está rodando, clique real pendente |
| 20 | Preferências sincronizadas | Testado (estado lido pela barra) |
| 21 | Sem processos duplicados | Testado |
| 22 | Posições persistem | Testado (State.plist e Dockapps.plist sem mudança) |
| 23 | X11 | Não afetado (código só do Wayland); suíte passou |
| 24 | Desempenho | Testado (sem polling; memória estável) |
| 25 | Testes visuais | Laboratório e uma captura real; capturas reais depois da correção pendentes (tela bloqueada) |

## Instalado

wlmaker 1:0.8.r80.g7481afe-32 (vale no próximo login), big-wm
26.11.10-2222. Unidades `big-wm-wayland-dock` e `big-wm-wayland-clip`
reiniciadas; controlador iniciado pelo `big-wm-dockapps` corrigido.

## Não publicado

Nada foi enviado ao GitHub.
