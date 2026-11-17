# 2. Arquitetura

## Correção na origem (wlmaker 44, repositório packages)

- **0158** `toolkit: A press keeps the pointer until all buttons are up`: a
  superfície que recebeu o clique toma o grab do seu contêiner até
  `button_count == 0`; fora dela o movimento segue relativo à (sub)superfície
  do clique; `leave` é ignorado enquanto apertado. Uma superfície **do mesmo
  cliente** mapeada nesse meio-tempo (o menu que o GTK abre no clique) herda
  o grab; de outro cliente (um balão), nunca.
- **0159** `server: Ignores activation requests without user input`: no sinal
  `new_token` do wlroots (emitido só depois de validar serial e foco no
  commit), o token com seat **e** superfície recebe uma marca; só tokens
  marcados ativam janelas. A marca é tomada na criação porque o wlroots zera
  a superfície do token quando ela some.

## Big WM Notification Center (`big-wm-notifications`, big-wm)

Um processo em C (GTK 3; gtk-layer-shell no Wayland), único dono de
`org.freedesktop.Notifications` na sessão.

```
apps ──Notify──▶ server.c ──▶ store.c (memória + histórico em disco)
                    │               │
                    ├─▶ ui-toast.c (balões)       ◀─ hover pausa, ×, ações
                    ├─▶ ui-center.c (central) ◀── sni.c (sininho, SNI)
                    └─ org.bigwindowmaker.Notifications (controle/CLI)
```

| Arquivo | Papel |
|---|---|
| `server.c` | Especificação 1.2: `Notify`, `CloseNotification`, `GetCapabilities`, `GetServerInformation`, sinais `NotificationClosed`, `ActionInvoked`, `ActivationToken`; validação; limite por remetente; logind `LockedHint`; "Não perturbe". |
| `store.c` | Notificações, histórico (GVariant, 0600), retenção, configurações (GKeyFile). |
| `ui-toast.c` | Balões: X11 `GTK_WINDOW_POPUP` (override-redirect), `_NET_WM_WINDOW_TYPE_NOTIFICATION`, `accept-focus` falso; Wayland layer-shell `OVERLAY`, teclado `NONE`. Fila (3 visíveis). |
| `ui-center.c` | Central aberta só a pedido: X11 popup com grab enquanto aberta; Wayland layer-shell com teclado exclusivo enquanto aberta + "captador" transparente que fecha no clique fora. |
| `sni.c` | `org.kde.StatusNotifierItem`: ícone desenhado (contador, "não perturbe"); clique abre a central, botão do meio liga/desliga "não perturbe". |
| `wl-activation.c` | Token xdg-activation com o serial do clique (o 0159 aceita); wlr-foreign-toplevel ligado só no clique, para trazer a janela do aplicativo. |
| `activate.c` | O clique: ação padrão, janela à frente (Wayland e X11) ou o aplicativo aberto. |
| `gtknotif.c` | `org.gtk.Notifications` (GNotification): o aplicativo e a ação certos, por `org.freedesktop.Application`. |
| `util.c` | Markup seguro, texto, tempo relativo, busca sem acentos. |

## Integração

- `big-wm-notifyd` escolhe: dunst se o usuário tem `~/.config/dunst/dunstrc`;
  senão `big-wm-notifications`; sem ele, o dunst com o tema (como antes).
- `dunst.service.d/big-wm.conf` (`ExecCondition … notifications`): o dunst
  ativado pelo D-Bus não sobe nas sessões Big WindowMaker; Plasma, XFCE e
  GNOME ficam iguais. Se ainda assim um dunst pegar o nome antes, o servidor
  o encerra e assume (só esse caso).
- Bandeja: `big-wm-tray` (Wayland) e snixembed + wmsystemtray (X11), ambos
  hospedeiros SNI já existentes.
- Pacote próprio `big-wm-notifications` (x86_64); o `big-wm-session`
  (arch any) depende dele.

## Por que C + GTK 3

PSS ocioso medido no lab (sem outros processos GTK dividindo páginas):
Python + PyGObject 23,4 MiB; C + GTK 3 + layer-shell 6,8 MiB; dunst 1,9 MiB.
GTK 3 e gtk-layer-shell já são dependências da sessão (bandeja, boas-vindas).
