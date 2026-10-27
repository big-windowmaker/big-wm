# SPDX-License-Identifier: GPL-2.0-or-later
#
# Notificações no modo escuro (big-wm-theme dark): o dunstrc com os cinzas
# neutros do estilo BigWindowMaker-Dark no lugar do azul-acinzentado do modo
# claro. Gera /usr/share/big-wm/dunstrc-dark no make install.
1s/^.*$/# Notificações do Big WindowMaker no modo escuro (dunst), gerado do dunstrc./
s/^\(    frame_color = \)"#b6b6b3"/\1"#6a6a6a"/
/^\[urgency_low\]/,/^\[/s/^\(    background = \)"#20262a"/\1"#2e2e2e"/
/^\[urgency_low\]/,/^\[/s/^\(    foreground = \)"#c2c0c5"/\1"#b4b4b4"/
/^\[urgency_normal\]/,/^\[/s/^\(    background = \)"#2a3236"/\1"#3c3c3c"/
