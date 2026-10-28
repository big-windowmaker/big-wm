| Função (sistema, x11, 300 s, 5 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: Xorg | 68.8 | 67.9–70.0 | 1 | 2.00 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 42.9 | 33.7–43.1 | 3 | 0.00 |
| Outros (sistema: systemd, journald, udevd, logind...) | 35.6 | 35.5–35.7 | 20 | 0.00 |
| Som: indicator-sound-switcher | 34.6 | 34.5–34.6 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 30.3 | 30.1–30.4 | 4 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 24.9 | 24.8–26.3 | 2 | 0.00 |
| Samba (smbd, nmbd) | 23.5 | 23.5–23.6 | 4 | 0.00 |
| Bandeja X11 de apps modernos (snixembed) | 21.8 | 21.8–23.6 | 2 | 0.00 |
| Autenticação (polkit-gnome + polkitd) | 19.2 | 19.2–20.3 | 2 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.6 | 15.4–16.1 | 3 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 13.8 | 13.7–13.9 | 4 | 0.00 |
| Servidor gráfico e WM: wmaker | 13.6 | 13.5–14.0 | 2 | 0.00 |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | 10.4–10.5 | 2 | 2.33 |
| Notificações (dunst) | 8.2 | 2.5–8.2 | 1 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.7 | 7.7–7.8 | 4 | 0.00 |
| Discos (udisksd) | 6.2 | 6.1–6.3 | 1 | 0.00 |
| GVfs | 5.1 | 5.1–5.2 | 3 | 0.00 |
| Tela de login (wdm) | 4.9 | 4.9–5.1 | 3 | 0.00 |
| Dockapps X11 (wmclock, wmix, wmcpuload, wmmemload, wmnet) | 4.3 | 4.3–4.3 | 6 | 1.33 |
| Impressão (cupsd) | 2.8 | 2.8–2.8 | 1 | 0.00 |
| Acessibilidade (at-spi) | 2.7 | 2.7–2.7 | 3 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Outros (sessão) | 2.4 | 2.4–2.5 | 6 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 0.8 | 0.8–0.8 | 1 | 0.00 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 0.7 | 0.7–0.7 | 1 | 0.00 |
| **Soma das medianas** | **404** | | | |
