# 06 — Lançadores

| Onde | AppID (State.plist) | Ícone | Tipo |
|---|---|---|---|
| Clip | firefox | big-wm-netscape-56x56.png | PNG estático |
| Clip | thunar | big-wm-folder-56x56.png | PNG estático |
| Clip | org.bigwindowmaker.Welcome | big-wm-app-…Welcome.desktop.png | PNG gerado do .desktop (`~/.local/share/wlmaker/icons`) |
| Clip | org.bigwindowmaker.DisplayResolution | big-wm-app-…DisplayResolution.desktop.png | idem |
| Clip | codium-insiders | big-wm-app-vscodium-insiders-bin.desktop.png | idem |
| Clip | onboard | big-wm-app-onboard.desktop.png | idem |
| Clip | big-wm-tabbed-dock | big-wm-app-big-wm-tabbed-dock-preferences.desktop.png | idem |
| Dock | org.communitybig.ashyterm | big-wm-terminal-56x56.png | PNG estático |
| Dock | rofi | big-wm-apps-56x56.png | PNG estático |
| Dock | steam | big-wm-app-steam.desktop.png | PNG gerado |

Todos são imagens estáticas: o ícone não muda com o tema (como no Window
Maker); o ladrilho, o chanfro e os três pontos de "em execução" vêm do
tema e são redesenhados quando o wlmdock recomeça. Nenhum símbolo
dependente do tema nem SVG recolorido está envolvido. Fixados pelo usuário
e padrões de instalação passam pelo mesmo caminho (lista `Launchers` do
State.plist).
