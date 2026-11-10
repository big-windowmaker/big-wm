# 01 — Inventário da máquina

| | |
|---|---|
| Sistema | Big WindowMaker, baseado no BigLinux (Manjaro) |
| Kernel | 6.18.55-1-MANJARO, x86_64 |
| GPU | AMD Navi 44 (Radeon RX 9050/9060 XT) e Cezanne (Vega integrada), amdgpu |
| Mesa | 26.2.4 |
| X | xorg-server 21.1.24, xorg-xwayland 24.1.13 |
| Sessão em uso | X11, `XDG_CURRENT_DESKTOP=WindowMaker`, `DISPLAY=:0`, iniciada pelo wdm |
| Compositor X11 | picom 13 |
| GTK / Qt | gtk3 3.24.52, gtk4 4.22.5, qt6-base 6.11.2, qt5-base 5.15.19, Kvantum 1.1.8 |
| Áudio | PipeWire 1.6.9, WirePlumber 0.5.18 |
| systemd / D-Bus | 261.3 / 1.16.2 |
| Bloqueio | xsecurelock 1.9.0 (X11), gtklock 4.0.0 (Wayland), xss-lock |
| XSETTINGS | xsettingsd 1.0.4 |

## Monitores

- No começo da auditoria: três — HDMI-A-0 3440×1440 (+0), DisplayPort-0
  3440×1440 primário (+3440), DisplayPort-1 2560×1080 (+6880); 9440×1440.
- Depois do reinício das 06:35 (ver [10](10-logs-erros-alertas.md)): só o
  DisplayPort-0, em 1920×1080 a 120 Hz; ele anuncia só modos de 1920×1080,
  embora informe o tamanho físico do ultrawide (800×334 mm); HDMI-A-0 e
  DisplayPort-1 desconectados. Não é software do Big WindowMaker (EDID, cabo,
  KVM ou o modo do monitor); a configuração de vídeo não foi tocada.

## Configurações auditadas (backup)

`~/GNUstep/Defaults`, `~/.config/big-wm`, `~/.config/big-tabbed-dock`,
`~/.config/wlmaker`, `~/.config/gtk-3.0`, `~/.config/gtk-4.0`,
`~/.config/Kvantum`, copiados antes de qualquer mudança para
`~/.cache/bwm-audit/backup-20261110-063136/`.
