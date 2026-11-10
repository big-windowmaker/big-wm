# 03 — wmpulsemixer (volume e microfone)

## Problema

O volume do Dock era o `wmix` (ALSA, pelo `big-wm-mixer`): sem microfone, sem
troca de dispositivo, e preso à espera de um mixer ALSA no login. O
`wmpulsemixer` (Fabien Pollet, 2026, fork do wmsmixer/wmmixer, GPL-2.0) fala
com o servidor de áudio pelo `libpulse`: PulseAudio ou PipeWire
(`pipewire-pulse`). Não estava empacotado.

## Investigação

Fonte: tag `0.2.2` de <https://github.com/polletfa/wmpulsemixer>.

- **Opções e janelas.**
  - `-w`: `WithdrawnState`, com janela de ícone. É o modo dockapp; sem `-w` ele abre uma janela comum.
  - O programa força `GDK_BACKEND=x11` (o menu é GTK) e `GDK_CORE_DEVICE_EVENTS=1`.
- **Controles.**
  - Setas: canal (`vol` = saída padrão, `mic` = entrada padrão).
  - Ícone: clique esquerdo põe ou tira o mudo; clique direito abre o menu de dispositivos daquele canal (saídas ou entradas).
  - Barra: clique e arrasto definem o volume; a roda do mouse muda de 4 em 4%.
- **Eventos.** `pa_context_subscribe` de sink, source e servidor: atualiza quando outro programa muda o volume ou o dispositivo padrão. Não faz polling.
- **Monitores de saída** (`.monitor`) não aparecem como entrada.
- **Ganho acima de 100%** só com `--boost`. O Big WindowMaker não o usa, então não há amplificação automática.

## Causas e correções (patch 0001 do pacote, a enviar ao autor)

| Defeito | Causa | Correção |
|---|---|---|
| Ladrilho vazio até o próximo login quando o PipeWire reinicia ou ainda não subiu | `gtk_main_quit()` no estado FAILED/TERMINATED do contexto | Descarta o contexto (fora do callback dele), mostra `--` com a barra vazia (nunca um volume antigo), reconecta com espera crescente 1, 2, 4… 30 s (um timer, sem polling), relê servidor, padrões e volumes. Uma linha no stderr por mudança de estado ("unavailable", "connection lost", "connected") |
| Rolar para cima num canal mudo em 80% punha 4% | A roda partia do volume mostrado (0 quando mudo) | Parte do volume real; o mudo continua mudo |
| Escolher um dispositivo no menu podia pôr no mudo ou mudar o volume | Os cliques no menu também chegavam a `uiPressEvent`, que olhava só as coordenadas | Cliques, arrasto e roda na janela do dockapp são tratados no filtro X dela, antes do GDK; o GTK só recebe os eventos do menu |
| `Gdk-WARNING: gdk_window_set_user_time called on non-toplevel` a cada clique | O GDK traduzia o clique da janela estrangeira | Idem: o filtro remove o evento (`GDK_FILTER_REMOVE`) |
| Mensagens em alemão | — | Em inglês |

## Arquivos

- `packages/wmpulsemixer/PKGBUILD`:
  - fonte: git na tag `0.2.2`;
  - licença `GPL-2.0-only` (COPYING v2, sem "or later");
  - dependências: `gtk3`, `libpulse`, `libx11`, `libxext`, `libxpm`;
  - `check()` confere `--version` e o `-w` no `--help`.
- `packages/wmpulsemixer/0001-reconnect-to-the-audio-server-and-real-volume-scroll.patch`.

## Testes

Feitos num PipeWire isolado (`pipewire` + `wireplumber` sem monitores de hardware + `pipewire-pulse`, com `XDG_RUNTIME_DIR` próprio), com dispositivos virtuais: "Lab Speakers", "Lab Headphones & Co", "Lab Microphone" e "Lab Webcam Mic". O áudio real da máquina não foi tocado.

- **Xvfb, 16 verificações, todas ok:**
  - barra, roda para cima e para baixo;
  - mudo e roda com mudo;
  - microfone pela barra sem mexer na saída, mudo do microfone sem mexer na saída;
  - troca de entrada e de saída pelo menu, sem efeito colateral nos controles;
  - volume mudado por fora aparece;
  - 0 avisos no log.
- **Servidor de áudio derrubado e religado:** o processo seguiu vivo, mostrou `--` e reconectou sozinho; CPU sem servidor: 0 ticks em 10 s.
- **Início sem servidor de áudio:** `--` na tela; conectou cerca de 3 s depois de o servidor subir.
- **Acoplado no Window Maker e no wlmaker:** ver [06](06-testes-desempenho.md).

## Limitações

- O menu de dispositivos é um menu GTK do tema da sessão. No Wayland, a navegação dele pelas setas do teclado não funciona (mouse, Esc e clique fora funcionam); no X11 funciona. Ver [05](05-sessoes.md).
- O dockapp mostra o volume médio dos canais e, ao mudar, iguala todos. É assim no upstream.
