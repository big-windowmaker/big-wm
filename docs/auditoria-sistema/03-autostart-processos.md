# 03 — Autostart e processos

## O Dock com Abas morto não voltava

- **Sintoma:** depois das 04:03, nenhum processo principal do dock; o que se
  via era um `big-wm-tabbed-dock --preferences` iniciado à mão (PPID 1).
- **Evidência:** `wmaker[5926]: ... X_KillClient ... Resource ID: 0x1400003`
  às 04:03:06 — o Window Maker matou a janela do dock (0x1400003 estava na
  `_NET_CLIENT_LIST`); o dock saiu com status 1. O Window Maker só faz isso
  por ação do usuário (Matar, ou fechar uma janela sem `WM_DELETE_WINDOW`).
- **Por que não voltou:** a unidade gerada pelo autostart usa
  `ExitType=cgroup`; o cgroup do dock tinha 91 tarefas (os zygotes do
  VSCodium, aberto pelo dock, que nasceram antes de o processo ir para o seu
  escopo). A unidade ficou "active (running)" e o `Restart=on-failure` nunca
  agiu (`NRestarts=0`).
- **Correção:** `ExitType=main` no drop-in do Dock com Abas (repositório
  tabbed-dock). O `KillMode=process`, que já estava, garante que reiniciar o
  dock nunca fecha um aplicativo.
- **Verificação:** duas unidades de teste com um processo principal que
  morre deixando um filho: `cgroup` → 0 reinícios, `main` → reiniciada.
  Na sessão real, depois de instalar: `systemctl --user restart` trocou o
  dock (2726 → 329761) e os 15 processos do VSCodium continuaram.

## Duplicatas

Nenhum dockapp em dobro na sessão (cada um uma vez; o `wmeyes` tem PPID 1
porque se desliga do pai, normal).

## O autostart do Window Maker

`~/GNUstep/Library/WindowMaker/autostart` contém `xset s 1800`, mas não é
executável: o Window Maker registra "não foi possível executar o script de
inicialização" a cada início e o protetor de tela fica nos 600 s padrão (o
bloqueio das 06:47 foi por isso). Nenhum programa do Big WindowMaker grava
esse arquivo assim (o WPrefs grava com `#!/bin/sh` e `+x`); por ser um
arquivo do usuário, não foi mudado. O `big-wm-doctor` o aponta.
