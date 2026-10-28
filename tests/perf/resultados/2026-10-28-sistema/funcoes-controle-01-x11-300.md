| Função (controle-01, x11, 300 s, 3 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: Xorg | 68.1 | 68.0–68.2 | 1 | 2.67 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 49.3 | 48.4–49.3 | 4 | 0.00 |
| Outros (sistema: systemd, journald, udevd, logind...) | 38.4 | 38.2–38.4 | 20 | 0.00 |
| Som: indicator-sound-switcher | 34.7 | 34.5–43.7 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 30.1 | 30.0–30.2 | 4 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 24.8 | 24.2–24.9 | 2 | 0.00 |
| Samba (smbd, nmbd) | 23.6 | 23.5–23.6 | 4 | 0.00 |
| Bandeja X11 de apps modernos (snixembed) | 21.8 | 21.1–21.8 | 2 | 0.00 |
| Autenticação (polkit-gnome + polkitd) | 19.2 | 18.5–19.2 | 2 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.7 | 15.6–15.7 | 3 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 13.9 | 13.8–13.9 | 4 | 0.00 |
| Servidor gráfico e WM: wmaker | 13.6 | 13.5–13.6 | 2 | 0.00 |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | 10.5–10.6 | 2 | 2.67 |
| Notificações (dunst) | 8.1 | 8.0–8.3 | 1 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.8 | 7.7–7.8 | 4 | 0.00 |
| Discos (udisksd) | 6.1 | 6.1–6.2 | 1 | 0.00 |
| GVfs | 5.1 | 5.1–5.1 | 3 | 0.00 |
| Tela de login (wdm) | 4.9 | 4.9–5.0 | 3 | 0.00 |
| Dockapps X11 (wmclock, wmix, wmcpuload, wmmemload, wmnet) | 4.3 | 4.3–4.3 | 6 | 2.33 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 4.1 | 4.1–4.1 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.8–2.8 | 1 | 0.00 |
| Acessibilidade (at-spi) | 2.7 | 2.7–2.7 | 3 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Outros (sessão) | 2.3 | 2.3–2.3 | 5 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 0.8 | 0.8–0.8 | 1 | 0.00 |
| **Soma das medianas** | **415** | | | |
