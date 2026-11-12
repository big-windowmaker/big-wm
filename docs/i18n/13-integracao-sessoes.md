# i18n do big-wm — 13. Idioma das sessões

## Caminho do idioma

1. O BigLinux grava o idioma do sistema em `/etc/locale.conf` (instalador
   Calamares; não há ferramenta de idioma no biglinux-settings). O
   gerenciador de login (lightdm/slick-greeter) pode trocar o `LANG` da
   sessão (idioma escolhido no greeter, AccountsService) e o
   `/etc/profile.d/locale.sh` lê `~/.config/locale.conf`.
2. `startbigwm` e `startbigwm-wayland` carregam o `big-wm-env.sh`, que
   define `LANGUAGE=$LANG` quando o usuário não definiu (o resto segue do
   ambiente da sessão; nada de `LC_ALL`).
3. `big-wm-session-ctl start` exporta o ambiente gráfico para o systemd
   --user e a ativação do D-Bus (autostart XDG, serviços, aplicativos
   `DBusActivatable`, portais).
4. Os aplicativos do menu do Window Maker e do wlmaker herdam o ambiente
   do gerenciador de janelas (o da sessão).

## O que faltava (corrigido em e9dd92a)

O `big-wm-session-ctl` já levava o `LANGUAGE`, mas não o `LANG` nem os
`LC_*`: o autostart e os aplicativos do D-Bus ficavam com o `LANG` do
`/etc/locale.conf` que o systemd --user leu ao iniciar. Com o idioma
escolhido no login diferente do sistema, as mensagens seguiam o
`LANGUAGE` da sessão mas datas, números e o `LANG` (de que o gettext do
Python e da glibc dependem para não cair no locale C) vinham do sistema.

Agora, quando a sessão tem `LANG`, o `start` exporta `LANG` e os `LC_*`
presentes (e tira os ausentes, sobras de uma sessão anterior); nunca o
`LC_ALL`. Sem `LANG` na sessão (gerenciador de login que não o define),
o idioma do systemd fica como está. O `stop` não apaga o idioma do
systemd --user (os serviços que continuam depois do logout seguem com
um).

## Verificado

- Máquina do usuário: `systemctl --user show-environment` tem `LANG` e os
  `LC_*` de `/etc/locale.conf` (pt_BR) e o `LANGUAGE=pt_BR.UTF-8` da sessão
  — coerente hoje porque sessão e sistema têm o mesmo idioma.
- `tests/run.sh`: o `big-wm-session-ctl start` com `systemctl` e
  `dbus-update-activation-environment` falsos (funções exportadas) leva
  `LANG`, `LANGUAGE`, `LC_TIME` e tira o `LC_NUMERIC` ausente; sem `LANG`
  não mexe no idioma; nunca no `LC_ALL`. O `stop` é conferido só pelo texto
  (ele chama o systemctl de verdade pelo `timeout`).
- Laboratório (login simulado `x-up.sh`/`wl-up.sh`, que usam o
  `startbigwm`/`big-wm-firstrun` instalados com `LANG=pt_BR.UTF-8`): as
  ferramentas abertas com `LANG`/`LANGUAGE` de cada idioma (ver
  `14-testes-validacao.md`). O systemctl é bloqueado no laboratório
  (safebin): a exportação para o systemd só foi testada com os falsos.

## Limitações

- O menu raiz (`WMRootMenu`/`RootMenu.plist`) é escolhido pelo idioma no
  primeiro login (`big-wm-firstrun`): trocar o idioma depois não troca os
  rótulos do menu raiz já copiado para a conta (o conteúdo gerado —
  Aplicativos, Dockapps, Papéis de parede — segue o idioma da sessão).
- `es` tem só o que já existia em espanhol no código (54 textos); o resto
  cai no inglês.
