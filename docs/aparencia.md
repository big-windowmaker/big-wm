# Aparência retrô dos aplicativos

As sessões Big WindowMaker (X11 e Wayland) dão aos aplicativos a mesma cara
do Window Maker: cinza, relevo e a barra de título do gerenciador de janelas.
Nada disso vale fora delas; Plasma, GNOME e XFCE continuam com os temas deles.

| Aplicativos | Tema | Pacote | Onde é ligado |
|---|---|---|---|
| Qt 5 e Qt 6 | QTStep OpenStep (Kvantum) | `qtstep-kvantum` | `QT_STYLE_OVERRIDE=kvantum` em `big-wm-env.sh`; o tema vai para `~/.config/Kvantum/kvantum.kvconfig` (`big-wm-firstrun`) |
| GTK 2, 3 e 4 (Thunar, Firefox...) | OneStepBack | `onestepback-gtk-theme` | `GTK_THEME=OneStepBack` em `big-wm-env.sh`, exportado ao D-Bus por `big-wm-session-ctl` |
| Firefox | Netscape 4 (RetroThemesFirefox) | `retrothemes-firefox` | `chrome/` copiado para o perfil por `big-wm-firstrun` |

## Qt: QTStep

[QTStep](https://github.com/andbgr/QTStep) traz o modelo e as cores de cada
variação; o pacote gera os 25 temas do Kvantum. O padrão é o
`QTStepOpenStep`, o da captura `screenshots/kvantum.png` do projeto: fundo e
campos `#aaaaaa`, seleção branca, texto preto. As outras variações aparecem no
Kvantum Manager (`kvantummanager`).

O Kvantum só lê o tema da configuração do usuário. O `big-wm-firstrun` cria o
arquivo apenas quando ele ainda não existe: quem escolheu outro tema fica com
ele.

## GTK: OneStepBack

[OneStepBack](http://www.vide.memoire.free.fr/pages/onestepback/onestepback.php)
imita o NeXTSTEP com três cinzas e uma cor. Para usar outro tema GTK nas
sessões Big WindowMaker, defina `GTK_THEME` em `~/.config/environment.d/` ou
`BIGWM_GTK_THEME=0`. Os aplicativos libadwaita ignoram temas GTK em boa parte.

Um aplicativo GTK que já estava aberto antes do login (por exemplo o Thunar
ativado pelo D-Bus) só muda depois de fechado: `thunar -q`.

## Firefox: Netscape 4 e a barra de título do Window Maker

O navegador padrão é o Firefox. O `big-wm-firstrun` copia o tema Netscape 4
de [RetroThemesFirefox](https://github.com/matthewmx86/RetroThemesFirefox)
para cada perfil que ainda não tem `chrome/`. Sem perfil nenhum, abre o Firefox
uma vez sem janela (`--headless`) para criar o perfil padrão, como na primeira
execução. Ficam de fora os scripts do fx-autoconfig do tema (`JS/`, `utils/`),
que rodariam JavaScript com privilégios a cada início.

No `prefs.js` do perfil, só com o Firefox fechado, entram três preferências
que o usuário pode mudar depois em `about:config`:

- `toolkit.legacyUserProfileCustomizations.stylesheets = true`: lê o
  `userChrome.css`;
- `browser.tabs.inTitlebar = 0`: barra de título e bordas do Window Maker
  (X11) ou do wlmaker (Wayland, decoração do servidor) em vez das abas no
  lugar dela;
- `browser.shell.checkDefaultBrowser = false`: o `.desktop` do Firefox chama
  `firefox`, que no BigLinux é o wrapper `/usr/local/bin/firefox`
  (auto-tweaks-browser); o Firefox não se reconhece nele e diria que não é o
  padrão mesmo sendo.

Para voltar ao visual normal do Firefox: apague a pasta `chrome/` do perfil
(`about:support` mostra onde ele fica).

## Navegadores Chromium (Chrome, Brave, Vivaldi)

O Brave saiu da edição. Num navegador Chromium instalado pelo usuário, a barra
de título do Window Maker se liga em Configurações > Aparência > "Usar barra
de título e bordas do sistema" (ou `custom_chrome_frame: false` no
`Preferences` do perfil, com o navegador fechado).
