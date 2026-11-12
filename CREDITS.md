# Créditos e licenças

## Código e configuração

Scripts, Makefile, testes e arquivos de configuração deste repositório:
GPL-2.0-or-later (mesma licença do Window Maker). Veja `LICENSE`.

A configuração padrão do Window Maker (`data/GNUstep/`) e os temas foram
reconciliados a partir dos repositórios anteriores do projeto, preservados
como referência histórica:

- [ruscher/comm-wmaker](https://github.com/ruscher/comm-wmaker)
- [ruscher/comm-wmaker-config](https://github.com/ruscher/comm-wmaker-config)
- [ruscher/biglinux-skel-wmaker](https://github.com/ruscher/biglinux-skel-wmaker)
- [ruscher/biglinux-calamares-wmaker](https://github.com/ruscher/biglinux-calamares-wmaker)

O formato dos arquivos de configuração segue o [Window Maker](https://www.windowmaker.org/)
(GPL-2.0-or-later) e o [wlmaker](https://github.com/phkaeser/wlmaker) (Apache-2.0).
O tema `data/wlmaker/Theme.plist` segue a estrutura do tema padrão do wlmaker.

Os cabeçalhos da API de plugins do [picom](https://github.com/yshui/picom) 13
em `session/zoom/picom/` (`api.h`, `backend.h`, `types.h`, de Yuxuan Shui)
são MPL-2.0 e ficam como estão; o plugin de ampliação que os usa
(`session/zoom/big-wm-picom-zoom.c`) é GPL-2.0-or-later.

## Arte

| Arquivo | Origem | Licença |
|---|---|---|
| `themes/WindowMaker/Themes/*.themed`, `Styles/BigLinux.style` | Rafael Ruscher (2022–2025), dos repositórios anteriores do Big WindowMaker | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/backgrounds/androide-synthwave.jpg` (`default.jpg`) | "Androide cromada em pose synthwave", de Rafael Ruscher, gerada com inteligência artificial; papel de parede padrão desde 09/11/2026 | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/backgrounds/big-wm-classic.jpg` | `wallpaper-big-wm.jpg`, de Rafael Ruscher (2022) | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/backgrounds/big-retro.jpg` | `Big-retro.jpg`, de Rafael Ruscher (2022) | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/backgrounds/owl.jpg` | `owl-ruscher.jpg`, de Rafael Ruscher (2025) | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/logo/big-wm.svg` | logo oficial do Big WindowMaker, fornecido pelo projeto (Ruscher, 2026); dele são gerados o ícone do Dock, o tema hicolor e os emblemas de sessão | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/icons/svg/*.svg` | recriações em SVG dos ícones do Window Maker (`WindowMaker/Icons`; vários de Marco van Hylckama Vlieg, ver `themes/icons/README.md`); deles são gerados o tema BigWindowMaker e os PNG do Dock e do Clip | GPL-2.0-or-later |
| `themes/icons/fornecidos/clipe.svg` | clipe escolhido pelo projeto (Rafael Ruscher, 2026) | GPL-2.0-or-later (declarada pelo autor, 11/2026) |
| `themes/icons/fornecidos/Netscape_icon_2007.svg` | o "N" do Netscape, mantido como homenagem ao navegador da época do NeXTSTEP | marca registrada de seus titulares; **não** é coberto pela licença livre deste repositório nem é oferecido para outros usos |
| rótulos dos ícones `pdf`, `ps`, `xv`, `xdvi`, `notepad`, `write` | contornos da Liberation Serif | SIL OFL 1.1 |
| `themes/boot/` (temas do GRUB e do Plymouth) | desenhados para o Big WindowMaker a partir da tela de boot "Window Maker Live" e das cores do estilo padrão; o logo vem de `themes/logo/big-wm.svg` | GPL-2.0-or-later |
| fontes `.pf2` do tema do GRUB (geradas no build) | Nimbus Sans Bold e Nimbus Mono PS Bold, fontes URW base35 do pacote `gsfonts` (ver `themes/boot/grub/FONTES-URW.txt`) | AGPL-3.0-only WITH PS-or-PDF-font-exception-20170817 |

Todas as autorias e licenças da arte foram declaradas pelo autor em 11/2026.
O "N" do Netscape é a única exceção à licença livre: quem redistribuir o tema
fora do Big WindowMaker deve trocá-lo por outro ícone.
