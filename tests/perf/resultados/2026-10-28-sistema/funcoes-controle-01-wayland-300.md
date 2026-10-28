| Função (controle-01, wayland, 300 s, 3 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: wlmaker | 108.6 | 108.6–108.8 | 1 | 2.33 |
| Outros (sistema: systemd, journald, udevd, logind...) | 37.8 | 37.7–37.8 | 20 | 0.00 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 36.6 | 36.6–36.8 | 4 | 0.00 |
| Bandeja Wayland (big-wm-tray) | 32.9 | 32.9–33.0 | 1 | 0.00 |
| Som: indicator-sound-switcher | 32.1 | 31.9–32.1 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 31.3 | 31.3–31.6 | 4 | 0.00 |
| Samba (smbd, nmbd) | 23.5 | 23.5–23.6 | 4 | 0.00 |
| Sincronia WPrefs→wlmaker (big-wm-wprefs-sync --watch) | 16.2 | 16.2–16.3 | 1 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 16.1 | 16.1–16.3 | 5 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.5 | 15.3–15.8 | 3 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 14.5 | 14.5–14.5 | 2 | 0.00 |
| Dockapps Wayland (wlm*) | 13.5 | 13.5–13.5 | 5 | 1.33 |
| Autenticação (polkit-gnome + polkitd) | 10.6 | 10.6–10.6 | 2 | 0.00 |
| Dock/Clip Wayland (wlmdock) | 8.6 | 8.5–8.6 | 2 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.8 | 7.7–7.8 | 4 | 0.00 |
| Notificações (dunst) | 7.3 | 7.2–7.3 | 1 | 0.00 |
| Discos (udisksd) | 6.1 | 6.1–6.2 | 1 | 0.00 |
| Tela de login (wdm) | 5.6 | 5.6–5.7 | 3 | 0.00 |
| GVfs | 5.1 | 5.1–5.1 | 3 | 0.00 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 4.1 | 4.1–14.0 | 1 | 0.00 |
| Outros (sessão) | 3.8 | 3.8–3.9 | 11 | 0.00 |
| Papel de parede Wayland (swaybg) | 3.1 | 3.0–3.1 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.8–2.8 | 1 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.5–2.6 | 2 | 0.00 |
| Acessibilidade (at-spi) | 2.4 | 2.4–2.4 | 3 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 1.0 | 1.0–1.0 | 2 | 0.00 |
| **Soma das medianas** | **450** | | | |
