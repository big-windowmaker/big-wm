| Função (dock-classico, x11, 300 s, 5 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: Xorg | 66.6 | 66.5–68.4 | 1 | 2.00 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 51.4 | 40.2–51.5 | 4 | 0.00 |
| Outros (sistema: systemd, journald, udevd, logind...) | 38.5 | 38.3–39.1 | 20 | 0.33 |
| Som: indicator-sound-switcher | 35.0 | 34.9–35.1 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 30.2 | 30.0–30.4 | 4 | 0.00 |
| Bandeja X11 de apps modernos (snixembed) | 23.8 | 23.8–25.8 | 2 | 0.00 |
| Samba (smbd, nmbd) | 23.6 | 23.5–23.7 | 4 | 0.00 |
| Autenticação (polkit-gnome + polkitd) | 20.5 | 20.5–22.4 | 2 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.7 | 15.4–15.8 | 3 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 14.1 | 14.1–14.1 | 4 | 0.00 |
| Servidor gráfico e WM: wmaker | 13.7 | 13.7–13.8 | 2 | 0.00 |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | 10.5–10.6 | 2 | 2.33 |
| Notificações (dunst) | 8.6 | 8.3–8.6 | 1 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.7 | 7.7–7.8 | 4 | 0.00 |
| Discos (udisksd) | 6.1 | 6.1–6.2 | 1 | 0.00 |
| GVfs | 5.1 | 5.1–5.2 | 3 | 0.00 |
| Tela de login (wdm) | 4.9 | 4.9–5.0 | 3 | 0.00 |
| Dockapps X11 (wmclock, wmix, wmcpuload, wmmemload, wmnet) | 4.4 | 4.3–4.4 | 6 | 1.33 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 4.1 | 4.1–4.1 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.8–2.8 | 1 | 0.00 |
| Acessibilidade (at-spi) | 2.7 | 2.7–2.7 | 3 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Outros (sessão) | 2.3 | 2.2–2.3 | 5 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 0.8 | 0.8–0.8 | 1 | 0.00 |
| **Soma das medianas** | **396** | | | |
