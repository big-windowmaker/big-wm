| Função (sistema, wayland, 300 s, 5 boots) | PSS mediana (MiB) | mín–máx | processos | CPU (ms/s) |
|---|---:|---:|---:|---:|
| Servidor gráfico e WM: wlmaker | 108.9 | 108.8–109.0 | 1 | 2.00 |
| Outros (sistema: systemd, journald, udevd, logind...) | 35.2 | 34.9–35.5 | 20 | 0.33 |
| Bandeja Wayland (big-wm-tray) | 32.8 | 32.8–32.9 | 1 | 0.00 |
| Som: indicator-sound-switcher | 32.0 | 32.0–32.1 | 2 | 0.00 |
| Som: PipeWire + WirePlumber + pipewire-pulse | 31.4 | 31.4–31.6 | 4 | 0.00 |
| Rede: NetworkManager + nm-applet + ModemManager + dnsmasq | 30.9 | 30.7–31.0 | 3 | 0.00 |
| Samba (smbd, nmbd) | 23.7 | 23.6–23.7 | 4 | 0.00 |
| Sincronia WPrefs→wlmaker (big-wm-wprefs-sync --watch) | 16.2 | 15.8–16.2 | 1 | 0.00 |
| Portais (xdg-desktop-portal*, documentos, permissões) | 16.2 | 16.0–17.0 | 5 | 0.00 |
| Som: redução de ruído do microfone (biglinux-microphone) | 15.6 | 15.5–15.9 | 3 | 0.00 |
| Dock com abas (big-wm-tabbed-dock) | 14.7 | 14.6–14.7 | 2 | 0.00 |
| Dockapps Wayland (wlm*) | 13.7 | 13.7–13.8 | 5 | 0.67 |
| Autenticação (polkit-gnome + polkitd) | 10.6 | 10.6–10.6 | 2 | 0.00 |
| Dock/Clip Wayland (wlmdock) | 8.6 | 8.5–8.6 | 2 | 0.00 |
| systemd --user + D-Bus da sessão + sd-pam | 7.7 | 7.6–7.8 | 4 | 0.00 |
| Discos (udisksd) | 6.1 | 6.1–6.2 | 1 | 0.00 |
| Tela de login (wdm) | 5.7 | 5.6–5.7 | 3 | 0.00 |
| GVfs | 5.1 | 5.1–5.1 | 3 | 0.00 |
| Outros (sessão) | 4.0 | 4.0–4.1 | 12 | 0.00 |
| Papel de parede Wayland (swaybg) | 3.3 | 3.3–3.4 | 1 | 0.00 |
| Impressão (cupsd) | 2.8 | 2.7–2.8 | 1 | 0.00 |
| Recursos (uresourced) | 2.6 | 2.6–2.6 | 2 | 0.00 |
| Acessibilidade (at-spi) | 2.4 | 2.4–2.4 | 3 | 0.00 |
| Notificações (dunst) | 2.2 | 2.2–2.3 | 1 | 0.00 |
| Bloqueio de tela (xss-lock/swayidle + big-wm-lock + i3lock/swaylock) | 1.0 | 1.0–1.0 | 2 | 0.00 |
| Atualizações (pamac-tray, pamac-session, pamac-broker) | 0.5 | 0.5–0.6 | 1 | 0.00 |
| **Soma das medianas** | **434** | | | |
