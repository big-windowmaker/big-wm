# 02 — Arquitetura

```
             PipeWire (pipewire-pulse)                 D-Bus de sessão (MPRIS)
             ▲         ▲         ▲                     ▲          ▲          ▲
   libpulse  │  pactl  │ pactl   │             playerctl         Gio        Gio
             │subscribe│         │                     │          │          │
      wmpulsemixer   Big Media Player          wmusic     Big Media Player  players
      (Dock)         (BSMD, volume.py)         (Dock)     (BSMD, mpris.py)  (Firefox,
                                                                             VLC...)
```

- **Uma só fonte de verdade.**
  - Volume: o servidor de áudio, ou seja, o PipeWire pelo `pipewire-pulse` ou o PulseAudio.
  - Reprodução: os próprios players, pelo MPRIS.
  - Nenhum protocolo novo entre os dockapps e o Big Screen Monitor Display, nenhum serviço extra.
- **O que cada um vê do outro.**
  - O `wmpulsemixer` assina eventos de sink, source e servidor (`pa_context_subscribe`).
  - O Big Media Player assina os mesmos eventos pelo `pactl subscribe`.
  - Uma mudança de um aparece no outro sem polling.
  - Os comandos do `wmusic` vão ao player; o Big Media Player vê o resultado pelo `PropertiesChanged`, e o inverso também.
- **Desacoplamento.**
  - O `big-screen-monitor-display` não ganhou dependência nem código: funciona igual sem os dockapps, no KDE, no GNOME ou sem mídia.
  - Os dockapps não sabem do Big Screen Monitor Display.
- **Repositórios.**

| Mudança | Repositório |
|---|---|
| Pacote `wmpulsemixer` (patch 0001) e `wmusic` mais leve (`big-dockapps` 10) | `packages` |
| Padrões do Dock, autostart, migração, catálogo do menu, dependências | `big-wm` |
| `wmpulsemixer` na ISO | `iso-profiles` (`Packages-Desktop`) e `build-iso` (artefato e pacote esperado) |
| Menu de dockapp visível e forma do ladrilho no login (achados no teste) | `wlmaker` (branch `fix/dockapp-menu-overlay`, pacote 30) |
| Nada | `big-screen-monitor-display`, `wmaker` |

- **Wayland.**
  - Os dois dockapps rodam pelo XWayland. O wlmaker já põe a janela de ícone de um dockapp X11 num ladrilho do Dock, com forma, cliques, roda e menus (patches 0122–0131 do pacote `wlmaker`).
  - O teste na tela ([06](06-testes-desempenho.md)) achou dois defeitos do wlmaker (menu do dockapp invisível; preto em volta do wmusic no login), corrigidos na origem ([05](05-sessoes.md)). Depois disso, compatibilidade completa com o mouse. Uma versão nativa não traria vantagem e não foi feita.
- **Volume único.**
  - Wayland: o `big-wm-dockapps` inicia um só do grupo `wmpulsemixer -w | big-wm-mixer | wlmmixer`.
  - X11: a migração troca o wmix pelo wmpulsemixer no mesmo lugar.
  - O `wmix` continua instalado e no catálogo para quem preferir; o `big-wm-mixer` fica para ele.
