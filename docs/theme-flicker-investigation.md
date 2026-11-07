# Piscadas entre claro e escuro: investigação

Rodada de 2026-11-07, na sessão Wayland do usuário (só leitura) e no
laboratório.

## O que se viu

- O `big-wm-theme` só rodou uma vez na sessão real (no login, 15:53). As
  demais execuções no journal do dia eram do laboratório (`HOME=/tmp/wlh`,
  `big-wm-firstrun` a cada reinício do wlmaker aninhado): o tema não é
  reaplicado em segundo plano nem a cada clique.
- A sincronização de tema do BigLinux (`sync-gnome-theme-to-qt`) está
  desligada nas sessões Big WindowMaker (`big-wm-autostart-condition
  qt-theme-sync`) e não rodou.
- O estado do tema estava inconsistente:

| Fonte | Valor |
|---|---|
| `~/.config/big-wm/theme` | `light` |
| gsettings `color-scheme` / `gtk-theme` | `default` / `OneStepBack` |
| `GTK_THEME` da sessão | `OneStepBack` |
| Kvantum | `QTStepOpenStep` (claro) |
| `~/.config/gtk-3.0` e `gtk-4.0/settings.ini` | **`OneStepBack-Dark` e `gtk-application-prefer-dark-theme=true`** |

- O journal tem, de Pamac, biglinux-snapshot-restore e bigame-ui: "Using
  GtkSettings:gtk-application-prefer-dark-theme with libadwaita is
  unsupported". O libadwaita segue o `color-scheme` (claro) e o GtkSettings
  diz escuro: os dois brigam, e o GTK 3 escolhe a variante escura do tema.

## Causa

O `settings.ini` ficou do modo escuro (gravado em 2026-11-06 23:52) e a
volta ao claro (`big-wm-theme light`, 00:25) não o corrigiu: o
`big-wm-theme` só cuidava do tema de ícones ali.

## Correção (`session/bin/big-wm-theme`)

- A cada aplicação (login e troca de modo) o `settings.ini` do GTK 3 e 4
  segue o modo: tema `OneStepBack` ou `OneStepBack-Dark` e nenhum
  `gtk-application-prefer-dark-theme` (o modo vem do tema e, no libadwaita,
  do `color-scheme`). Tema de outra família é escolha do usuário e fica,
  com o prefer-dark dele.
- Os sinais do tema (xsettingsd, dunst, wmsystemtray) vão só para os
  programas desta sessão (mesmo `DISPLAY`; sem X, o mesmo `WAYLAND_DISPLAY`
  e `XDG_RUNTIME_DIR`), nunca para os de outro login ou de uma sessão de
  testes.

Teste: um `settings.ini` com `OneStepBack-Dark` e `prefer-dark=true` em modo
claro sai com `OneStepBack` e sem prefer-dark; um com `Adwaita` e
prefer-dark (escolha do usuário) fica igual.

## A conferir

A piscada em si não foi reproduzida no laboratório. Depois do novo
`big-wm-theme` (o próximo login já corrige o `settings.ini`), conferir na
sessão real se ela some nos aplicativos libadwaita e GTK 3. Se continuar,
registrar qual aplicativo e qual ação: o próximo suspeito é o reload
inteiro do wlmaker (SIGHUP) quando o vigia do WPrefs grava algo.
