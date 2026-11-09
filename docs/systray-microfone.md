# Microfone (filtro de ruído do BigLinux) na bandeja

Missão de novembro de 2026, rodada 2 (agente "apps"). Ramo
`feat/apps-hp-scrcpy-uxplay-mic`. Capturas em
`docs/screenshots/aplicativos/microfone-*.png`.

## Problema
O "BigLinux Microfone" — pacote `biglinux-noise-reduction-pipewire`
26.11.01, aplicativo **"Filtrar ruídos"** (`br.com.biglinux.microphone.desktop`,
`Exec=biglinux-microphone`) — devia iniciar com a sessão Big WindowMaker e
ficar na bandeja com ícone de microfone, menu, uma instância, X11 e Wayland.

## O que o pacote oferece (investigação)
- `biglinux-microphone`: janela GTK 4/libadwaita; **sem modo bandeja**
  (nenhum StatusNotifierItem/AppIndicator, nenhuma opção de segundo plano;
  `main.py` roda `app.run(sys.argv[:1])`).
- No Plasma, a bandeja é o plasmoide `br.com.biglinux.micnoise` (QML) do
  mesmo pacote. Ele usa só:
  - `biglinux-microphone-cli status` → JSON com `mic_enabled`,
    `mic_running`, `output_enabled`, `output_running`, `audio_available`;
  - `biglinux-microphone-cli set mic|output on|off`;
  - releitura a cada 7 s e quando `~/.config/biglinux-microphone/settings.json`
    muda (inotifywait);
  - ícones `big-noise-reduction-on/off` (claros, `#dfdfdf`, para painéis
    escuros) e `dialog-warning-symbolic` quando algo não está pronto.
- Unidades `systemd --user` (`biglinux-microphone*.service/.path`) aplicam
  o filtro no login sozinhas: a bandeja só controla e mostra o estado.

Bandejas da sessão: X11 = `wmsystemtray` (XEmbed) + `snixembed` (SNI →
XEmbed); Wayland = `big-wm-tray` (hospedeiro SNI no Dock). As duas exibem
itens SNI com menu dbusmenu. Não foi criada outra bandeja.

## Solução: `big-wm-mic-tray`
`session/bin/big-wm-mic-tray`: item StatusNotifierItem **sem GTK** (Gio +
servidor dbusmenu da `libdbusmenu-glib`, ≈ 16 MiB de PSS medido no
laboratório).

- **Estado só de fontes confiáveis**: `biglinux-microphone-cli status`
  (mesmo esquema validado do plasmoide) e `wpctl get-volume
  @DEFAULT_AUDIO_SOURCE@` (`[MUTED]`). Sem `wpctl`, o silêncio não é
  mostrado nem oferecido.
- **Ícone**: `audio-input-microphone-symbolic` (microfone ativo) ou
  `microphone-sensitivity-muted-symbolic` (silenciado); `Status
  NeedsAttention` + `dialog-warning-symbolic` quando o status não pode ser
  lido, sem áudio, ou com um filtro ligado que não está rodando. Nomes
  simbólicos: as duas bandejas os pintam na cor do tema (claro/escuro). Os
  `big-noise-reduction-*` do pacote são claros e somem no ladrilho cinza.
- **Balão**: "Microfone ativo/silenciado", "Filtro do microfone:
  ligado/desligado", "Filtro do som do sistema: ...", e o aviso quando há.
- **Cliques** (iguais nas duas sessões): esquerdo abre as configurações
  (`biglinux-microphone`); direito, o menu; do meio, silenciar/reativar
  (só no Wayland: o snixembed não repassa o botão do meio, o
  `SecondaryActivate`).
- **Menu**: Filtro do microfone ✓, Filtro do som do sistema ✓ (pelo
  `biglinux-microphone-cli set`, que grava no `settings.json` do aplicativo:
  é a preferência persistida, a mesma da janela e do plasmoide), Silenciar
  microfone ✓, Abrir configurações..., Iniciar com a sessão ✓, Fechar.
  Enquanto um `set` roda, os filtros ficam insensíveis com "Aplicando...".
- **Atualização**: `Gio.FileMonitor` no diretório de configuração (sem
  processo), `pactl subscribe` (eventos de fonte/servidor do PipeWire, na
  hora) e leitura a cada 28 s; a cada 7 s quando não há `pactl` (ou ele
  saiu). Depois de uma ação, três leituras rápidas.
- **Uma instância**: nome `org.bigwindowmaker.MicTray` no barramento da
  sessão (`DO_NOT_QUEUE`); a segunda sai com 0.
- **Bandeja que chega depois / reinicia**: `bus_watch_name` no
  `org.kde.StatusNotifierWatcher` registra o item de novo.
- **Autostart**: `/etc/xdg/autostart/big-wm-mic-tray.desktop` com
  `OnlyShowIn=WindowMaker;wlmaker;` e `TryExec=biglinux-microphone-cli`
  (sem o pacote, nada inicia; Plasma/GNOME/XFCE não são afetados).
  "Iniciar com a sessão" desmarcado grava
  `~/.config/autostart/big-wm-mic-tray.desktop` com `Hidden=true` (forma
  padrão do XDG, respeitada pelo `systemd-xdg-autostart-generator`); marcar
  de novo apaga o arquivo.

## Testes
- Host, só leitura: `big-wm-mic-tray --state` →
  `status=Active icon=audio-input-microphone-symbolic` com "Filtro do
  microfone: ligado / Filtro do som do sistema: desligado" (igual ao
  `biglinux-microphone-cli status` real).
- Laboratório Wayland (wlmaker instalado aninhado, `big-wm-tray`
  instalado), `biglinux-microphone-cli`/`wpctl`/`biglinux-microphone`
  falsos (as ações não mexem no áudio real): item na bandeja
  (`big-wm-tray --state`: `big-wm-mic-tray shown
  name:audio-input-microphone-symbolic`); menu
  (`microfone-bandeja-wayland.png`); Silenciar troca o ícone para
  `microphone-sensitivity-muted-symbolic`; "Filtro do som do sistema"
  chama `set output on` e volta marcado; botão do meio silencia; esquerdo
  abre as configurações; mudança externa no `settings.json` atualiza o
  balão em ~1 s (`gdbus ... Get ToolTip`); "Iniciar com a sessão" cria e
  apaga o `Hidden=true`; Fechar tira o item da bandeja; segunda instância
  sai com 0; nenhum processo zumbi.
- Laboratório X11 (Xvfb + startbigwm, `wmsystemtray` + `snixembed`): ícone
  no wmsystemtray, menu pelo botão direito (`microfone-bandeja-x11.png`),
  ícone silenciado depois de Silenciar pelo menu, esquerdo abre as
  configurações.
- `tests/apps.py` (no `tests/run.sh`): leitura do status, esquema inválido,
  `wpctl`, ícone/estado (ativo, silenciado, sem wpctl, filtro parado,
  status ilegível), autostart Hidden.

## Limitações
- O botão do meio não silencia no X11 (snixembed não chama
  `SecondaryActivate`); o menu tem o mesmo comando.
- O `wpctl` mostra o silêncio da **fonte padrão** (com o filtro ligado, a
  fonte virtual do filtro): é o que os aplicativos ouvem.
- Não testado numa sessão de login real (autostart pelo systemd): o
  laboratório bloqueia o `systemctl`. O `.desktop` segue o padrão dos
  outros autostarts do big-wm.
