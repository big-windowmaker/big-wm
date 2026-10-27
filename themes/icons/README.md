# Ícones do Window Maker em SVG

Recriações vetoriais dos ícones clássicos do Window Maker, desenhadas à mão
em SVG (grade de 64 × 64, luz vinda de cima e da esquerda, relevo cinza do
NeXTSTEP), para ficarem nítidas em 22, 32, 48, 56, 64 e 128 px nas sessões
X11 (Window Maker) e Wayland (wlmaker).

| Arquivo (`svg/`) | Original do Window Maker | Uso no Big WindowMaker |
|---|---|---|
| `clip.svg` | `clip.tiff` (com o arame de `fornecidos/clipe.svg`) | Clip (X11 `Logo.WMClip`, Wayland `Clip.Icon`) |
| `netscape.svg` | — (`fornecidos/Netscape_icon_2007.svg`) | Firefox no Clip (`Navigator.firefox`, lançador Wayland) |
| `folder.svg`, `folder-home.svg` | — (pasta no estilo NeXTSTEP/GNUstep) | Thunar no Clip, pastas do tema |
| `terminal.svg` | `Terminal.tiff` (com o prompt na tela) | terminal do Dock Wayland, `XTerm` |
| `apps.svg` | — (ladrilhos do Window Maker em miniatura) | botão Aplicativos do Dock Wayland |
| `defaultapp.svg` | `defaultAppIcon.tiff` | ícone padrão (`"*"` nos atributos) |
| `gnustep.svg` | `GNUstep.tiff` | — |
| `drawer.svg` | `Drawer.tiff` | gavetas |
| `magnify.svg` | `Magnify.tiff` | busca (Catfish) |
| `mixer.svg` | `mixer.tiff` | controle de volume |
| `notepad.svg`, `draw.svg`, `write.svg` | `notepad.tiff`, `draw.tiff`, `write.tiff` | editor de texto, desenho, processador de texto |
| `pdf.svg`, `ps.svg`, `xdvi.svg`, `xv.svg` | `pdf.tiff`, `ps.tiff`, `xdvi.tiff`, `xv.tiff` | leitores de PDF, PostScript, DVI e imagens |
| `timer.svg` | `timer.tiff` | relógio |
| `wilber.svg` | `wilber.tiff` | GIMP |

O `make install-themes` instala cada SVG uma vez e gera o resto:

- tema XDG **BigWindowMaker** em `/usr/share/icons/BigWindowMaker`: os
  desenhos em `scalable/apps/big-wm-*.svg` e os nomes padrão
  (`utilities-terminal`, `org.xfce.thunar`, `folder`, `firefox`...) como
  links, conforme o arquivo `nomes`. O que não está lá vem dos temas
  herdados (`Inherits` do `index.theme`: bigicons-papient, breeze,
  Adwaita, hicolor);
- `/usr/share/WindowMaker/Icons/big-wm-<nome>.png`, 64 px, para o Window
  Maker (o ladrilho inteiro; o desenho já tem a margem de 4 px);
- `/usr/share/wlmaker/icons/big-wm-<nome>-56x56.png` (`make
  install-wayland`), 56 px sem a margem, o conteúdo do ladrilho do wlmaker.

Nenhum PNG fica no repositório: tudo sai do `rsvg-convert`.

## Usar o tema de ícones nos aplicativos

O tema não é ligado sozinho: Dock e Clip já usam os PNG acima, e os
aplicativos continuam com o tema de ícones do sistema. Para usar os ícones
do Window Maker também nos aplicativos GTK (Thunar, menus, diálogos):

    gsettings set org.gnome.desktop.interface icon-theme BigWindowMaker

ou, no `~/.config/gtk-3.0/settings.ini` (e `gtk-4.0`), na seção
`[Settings]`: `gtk-icon-theme-name=BigWindowMaker`. Para voltar, use o
nome anterior (por exemplo `bigicons-papient`). O
`gsettings set` vale para todas as sessões do usuário, não só as do Big
WindowMaker.

## Origem e licença

- Desenhos de `svg/`: recriações (obras derivadas) dos ícones do Window
  Maker, distribuídos com ele sob a GPL. Os de `defaultAppIcon`, `notepad`,
  `draw`, `write`, `pdf`, `ps`, `xv`, `xdvi`, `mixer`, `timer` e `wilber`
  foram feitos por Marco van Hylckama Vlieg (o `WindowMaker/Icons/README` do
  Window Maker pede que o autor seja mencionado). O Wilber é o mascote do
  GIMP, de Tuomas Kuosmanen. Licença destes SVG: GPL-2.0-or-later, como o
  Big WindowMaker.
- Os rótulos "pdf", "ps", "XV", "xdvi", "N" e "W" são contornos da Liberation
  Serif (SIL Open Font License 1.1), convertidos em caminhos para não depender
  de fontes instaladas.
- `fornecidos/clipe.svg` (o arame do clipe) e
  `fornecidos/Netscape_icon_2007.svg` (o "N" do Netscape) foram escolhidos e
  fornecidos pelo projeto (Ruscher, 2026). `svg/clip.svg` usa o mesmo traçado
  do arame, mais grosso e com brilho; `svg/netscape.svg` é o desenho original
  com margem. O "N" do Netscape é marca de seus titulares; aqui identifica o
  navegador, como o tema Netscape 4 do Firefox.
