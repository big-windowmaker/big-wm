| Função (escuro, wayland, 300 s, 3 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: wlmaker | 108.6 | 108.5–108.9 | 1 | 1.67 |
| Outros (sistema: systemd, journald, udevd, logind...) | 37.9 | 37.7–38.1 | 20 | 0.00 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 36.8 | 36.8–37.0 | 4 | 0.00 |
| Bandeja Wayland (big-wm-tray) | 33.0 | 33.0–33.0 | 1 | 0.00 |
| Som: indicator-sound-switcher | 32.1 | 32.0–32.1 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 31.7 | 31.5–31.7 | 4 | 0.00 |
| Samba (smbd, nmbd) | 23.6 | 23.5–23.7 | 4 | 0.00 |
| Sincronia WPrefs→wlmaker (big-wm-wprefs-sync --watch) | 16.2 | 16.2–16.2 | 1 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 16.2 | 16.1–16.3 | 5 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.7 | 15.6–15.9 | 3 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 14.5 | 14.4–14.5 | 2 | 0.00 |
| Dockapps Wayland (wlm*) | 13.4 | 13.4–13.5 | 5 | 0.33 |
| Autenticação (polkit-gnome + polkitd) | 10.6 | 10.6–10.6 | 2 | 0.00 |
| Dock/Clip Wayland (wlmdock) | 8.6 | 8.5–8.6 | 2 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.7 | 7.7–7.7 | 4 | 0.00 |
| Notificações (dunst) | 7.3 | 7.2–7.3 | 1 | 0.00 |
| Discos (udisksd) | 6.1 | 6.0–6.1 | 1 | 0.00 |
| Tela de login (wdm) | 5.7 | 5.6–5.8 | 3 | 0.00 |
| GVfs | 5.1 | 5.1–5.1 | 3 | 0.00 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 4.1 | 4.1–4.1 | 1 | 0.00 |
| Outros (sessão) | 3.8 | 3.8–3.8 | 11 | 0.00 |
| Papel de parede Wayland (swaybg) | 3.0 | 3.0–3.0 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.7–2.8 | 1 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Acessibilidade (at-spi) | 2.4 | 2.4–2.4 | 3 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 1.0 | 1.0–1.1 | 2 | 0.00 |
| **Soma das medianas** | **451** | | | |
