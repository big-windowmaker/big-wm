#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Monta o tema do GRUB do Big WindowMaker num diretório:
#
#   gerar.sh DESTINO instalado   sistema instalado (textos em português)
#   gerar.sh DESTINO live        ISO live (título "Big WindowMaker Live" e
#                                textos traduzidos pelo próprio GRUB, que
#                                seguem o idioma escolhido no menu da ISO)
#
# Nada binário fica no repositório: as fontes .pf2 saem do grub-mkfont (URW
# Nimbus Sans e Nimbus Mono PS, do pacote gsfonts), o logo do rsvg-convert e
# as peças da barra de seleção do ../pecas.py.

set -euo pipefail

dest=${1:?uso: gerar.sh DESTINO instalado|live}
variante=${2:?uso: gerar.sh DESTINO instalado|live}
aqui=$(cd "$(dirname "$0")" && pwd)
raiz=$(cd "$aqui/../../.." && pwd)

: "${GRUB_MKFONT:=grub-mkfont}"
: "${RSVG:=rsvg-convert}"
: "${FONTE_SANS_BOLD:=/usr/share/fonts/gsfonts/NimbusSans-Bold.otf}"
: "${FONTE_MONO_BOLD:=/usr/share/fonts/gsfonts/NimbusMonoPS-Bold.otf}"

case $variante in
instalado)
	titulo="Big WindowMaker"
	dica="Pressione E para editar as opções de boot"
	teclas="ENTER inicia, E edita a entrada"
	contagem="Inicia automaticamente em %d s"
	contagem_topo="100%-70"
	tamanho_titulo=46
	;;
live)
	# @KEYMAP_SHORT@ e @TIMEOUT_NOTIFICATION_MIDDLE@ são traduzidos pelo GRUB
	# (catálogos em /boot/grub/locales da ISO). A contagem ocupa o rodapé.
	# O título, mais comprido, usa 36 px para caber ao lado do logo a 640x480;
	# a contagem curta ("%d s restantes.") também cabe nessa largura.
	titulo="Big WindowMaker Live"
	dica="@KEYMAP_SHORT@"
	teclas=""
	contagem="@TIMEOUT_NOTIFICATION_MIDDLE@"
	contagem_topo="100%-40"
	tamanho_titulo=36
	;;
*)
	echo "gerar.sh: variante desconhecida: $variante" >&2
	exit 1
	;;
esac

mkdir -p "$dest"

sed -e "s|@TITULO@|$titulo|" \
	-e "s|@DICA@|$dica|" \
	-e "s|@TECLAS@|$teclas|" \
	-e "s|@CONTAGEM@|$contagem|" \
	-e "s|@CONTAGEM_TOPO@|$contagem_topo|" \
	-e "s|@TAMANHO_TITULO@|$tamanho_titulo|" \
	-e "s|@TITULO_TOPO@|38%-$((tamanho_titulo * 92 / 100))|" \
	"$aqui/theme.txt.in" >"$dest/theme.txt"
if grep -q '@[A-Z_]*@' <(grep -v -e '@KEYMAP_SHORT@' -e '@TIMEOUT_NOTIFICATION_MIDDLE@' "$dest/theme.txt"); then
	echo "gerar.sh: marcador não substituído em theme.txt" >&2
	exit 1
fi

# Fontes: o GRUB as chama de "Família Estilo Tamanho" (-n dá a família; o
# estilo e o tamanho o grub-mkfont acrescenta), como no theme.txt.
# Só as faixas usadas (latim, pontuação, setas e desenho de caixas do GRUB);
# o resto cai no unicode.pf2 que o GRUB também carrega.
faixas=0x20-0x7e,0xa0-0x17e,0x2010-0x2027,0x2190-0x2195,0x2500-0x257f,0x25b2-0x25bc
mkfont() { # arquivo, família, tamanho, saída
	"$GRUB_MKFONT" -r "$faixas" -n "$2" -s "$3" -o "$dest/$4" "$1"
}
mkfont "$FONTE_SANS_BOLD" "Nimbus Sans" "$tamanho_titulo" "nimbus-sans-bold-$tamanho_titulo.pf2"
mkfont "$FONTE_MONO_BOLD" "Nimbus Mono" 22 nimbus-mono-bold-22.pf2
mkfont "$FONTE_MONO_BOLD" "Nimbus Mono" 13 nimbus-mono-bold-13.pf2

"$RSVG" -w 152 -h 152 "$raiz/themes/logo/big-wm.svg" -o "$dest/logo.png"
python3 -I "$aqui/../pecas.py" grub "$dest"
