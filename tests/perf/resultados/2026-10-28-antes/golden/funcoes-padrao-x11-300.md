| Função (padrao, x11, 300 s, 5 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: Xorg | 68.8 | 68.3–71.7 | 1 | 2.33 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 49.2 | 39.0–49.3 | 4 | 0.00 |
| Outros (sistema: systemd, journald, udevd, logind...) | 38.3 | 38.3–38.7 | 20 | 0.00 |
| Som: indicator-sound-switcher | 34.5 | 34.5–34.6 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 30.2 | 30.0–30.3 | 4 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 25.0 | 15.0–26.1 | 2 | 0.00 |
| Samba (smbd, nmbd) | 23.6 | 23.5–23.7 | 4 | 0.00 |
| Bandeja X11 de apps modernos (snixembed) | 21.8 | 21.8–24.9 | 2 | 0.00 |
| Autenticação (polkit-gnome + polkitd) | 19.2 | 19.2–22.2 | 2 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.8 | 15.6–15.8 | 3 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 13.8 | 13.8–15.0 | 4 | 0.00 |
| Servidor gráfico e WM: wmaker | 13.5 | 13.5–13.5 | 2 | 0.00 |
| Compositor X11 (picom + big-wm-x11-repaint) | 10.5 | 10.5–10.6 | 2 | 2.33 |
| Notificações (dunst) | 8.2 | 8.1–8.3 | 1 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.7 | 7.6–7.7 | 4 | 0.00 |
| Discos (udisksd) | 6.1 | 6.0–6.2 | 1 | 0.00 |
| GVfs | 5.1 | 5.1–5.1 | 3 | 0.00 |
| Tela de login (wdm) | 4.9 | 4.9–5.1 | 3 | 0.00 |
| Dockapps X11 (wmclock, wmix, wmcpuload, wmmemload, wmnet) | 4.3 | 4.3–4.3 | 6 | 1.67 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 4.1 | 4.1–4.1 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.8–2.8 | 1 | 0.00 |
| Acessibilidade (at-spi) | 2.7 | 2.7–2.7 | 3 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Outros (sessão) | 2.3 | 2.3–2.3 | 5 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 0.8 | 0.8–0.8 | 1 | 0.00 |
| **Soma das medianas** | **416** | | | |
