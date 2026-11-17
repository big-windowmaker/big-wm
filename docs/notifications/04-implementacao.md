# 4. Implementação e uso

## Construir e instalar

```sh
make notifications                       # session/notifications/big-wm-notifications
make DESTDIR=… install-notifications     # /usr/bin/big-wm-notifications
```

Dependências de build: gtk3, gtk-layer-shell, wayland, wayland-protocols
(wayland-scanner gera o cliente xdg-activation em `session/notifications/gen`).
No PKGBUILD: pacote `big-wm-notifications`; `build()` chama `make
notifications`.

## Linha de comando

```
big-wm-notifications                 servidor (um por sessão; o 2.º sai com 0)
big-wm-notifications --toggle        abre/fecha a central (atalho de teclado)
big-wm-notifications --dnd on|off|toggle
big-wm-notifications --clear         limpa o histórico
big-wm-notifications --status        dnd=… unread=… total=… toasts=…
big-wm-notifications --reload        relê a configuração
```

Atualização do pacote: o servidor vigia o próprio arquivo e, quando ele é
trocado, grava o histórico e executa a versão nova no mesmo processo (assim
que a central estiver fechada). Sem isso a sessão seguia com a versão antiga
até o próximo login.

## Configuração (`~/.config/big-wm/notifications.conf`)

Só as chaves diferentes do padrão são gravadas.

```ini
[Notifications]
Toasts=true                     # balões
Timeout=6                       # segundos (o remetente pode pedir outro)
Position=top-right              # top-left, bottom-right, bottom-left
Monitor=-1                      # -1: monitor do ponteiro (X11) / do compositor
MaxVisible=3
MarginX=76                      # distância da borda (o Dock fica à direita)
MarginY=12
DoNotDisturb=false
DoNotDisturbAllowsCritical=true
History=true
RetentionDays=14                # 0: sem limite de idade
MaxHistory=200
Sound=false                     # canberra-gtk-play, se instalado
Animations=true                 # surgir suave (Wayland)
HideOnLock=true                 # sem balões com a sessão bloqueada
MutedApps=App1;App2             # vão só para o histórico
```

## Decisões

- Expiração: ao fim do tempo o remetente recebe `NotificationClosed(1)` e a
  entrada fica no histórico (sem ações: o remetente já não as atende).
  `persistence` não é anunciada.
- `replaces_id` só substitui uma notificação do mesmo remetente (ou do mesmo
  aplicativo); senão, recebe um id novo.
- O balão nunca é focado; o clique numa ação não muda o foco (X11:
  override-redirect não é gerido pelo Window Maker; Wayland: teclado `NONE`).
- Sem polling: um relógio de 30 s só enquanto há balões ou a central aberta;
  `malloc_trim` ao fechar a central e ao sumir o último balão.
