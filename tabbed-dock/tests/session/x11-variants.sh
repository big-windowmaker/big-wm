#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Variantes X11: arrastar de fora (XDND), dois monitores, borda superior,
# alinhamento, auto-ocultar, estilo moderno, HiDPI, preferências, coexistência.
. /t/lib.sh
export RESULTS=/work/x11v-results.tsv; : >"$RESULTS"
S=/shots
cfg=$XDG_CONFIG_HOME/big-tabbed-dock/dock.conf
big-wm-tabbed-dock >/work/dock-x11v.log 2>&1 & wait_for 10 state
# 1. arrastar arquivos de um "gerenciador de arquivos" (XDND) para a aba Temporary
mkdir -p "$HOME/Documents"; echo x >"$HOME/Documents/relatório final.txt"; mkdir -p "$HOME/Projetos"
python3 /src/tabbed-dock/tests/fixtures/btd-drag-source.py "$HOME/Documents/relatório final.txt" "$HOME/Projetos" &
sleep 2
big-wm-tabbed-dock --tab temporary; sleep 0.5
src=$(xdotool search --name '^btd-drag-source$' | head -1)
eval "$(xdotool getwindowgeometry --shell "$src")"
dst=$(slot_xy tab 3); read -r dx dy <<<"$dst"
xdotool mousemove $((X + 80)) $((Y + 60)) sleep 0.3 mousedown 1 sleep 0.2 mousemove_relative 20 0 sleep 0.2 mousemove_relative 20 0 sleep 0.3 mousemove $dx $((dy + 50)) sleep 0.5 mousemove_relative 3 0 sleep 0.5 mouseup 1
sleep 1.5
check "x11: soltar arquivos vindos de fora (XDND)" "arquivo + pasta na Temporary" bash -c 'big-wm-tabbed-dock --print-state | grep -Eq "^tab \\*?temporary \"Temporary\" items=2"'
check "x11: ação escolhida pelo dock nunca é MOVE" "$(cat /work/drag-action.txt 2>/dev/null)" bash -c '! grep -q MOVE /work/drag-action.txt'
check "x11: arquivo de origem intacto" "relatório final.txt" test -f "$HOME/Documents/relatório final.txt"
import -window root $S/x11-11-external-drop.png
pkill -f btd-drag-source.py
# remover atalho nunca apaga o arquivo
big-wm-tabbed-dock --quit; sleep 1
python3 - "$cfg" <<'PY'
import sys, re
p = sys.argv[1]; s = open(p).read()
s = re.sub(r'\[Item temporary 000\][^\[]*', '', s)   # remove o atalho como o menu faria
open(p, 'w').write(s)
PY
big-wm-tabbed-dock >>/work/dock-x11v.log 2>&1 & wait_for 10 state
check "x11: remover atalho não apaga o arquivo" "arquivo continua" test -f "$HOME/Documents/relatório final.txt"

# 3. borda superior, alinhamento à direita, estilo moderno
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^Position=.*/Position=top/; s/^Alignment=.*/Alignment=right/; s/^Style=.*/Style=modern/' "$cfg"
big-wm-tabbed-dock >>/work/dock-x11v.log 2>&1 & wait_for 10 state; sleep 1
st=$(state); oy=$(awk -F'[=,]' '/^origin=/{print $3}' <<<"$st"); ox=$(awk -F'[=,]' '/^origin=/{print $2}' <<<"$st"); w=$(awk -F'[=x ]' '/^geometry=/{print $2}' <<<"$st")
check "x11: borda superior + alinhamento à direita" "x=$ox y=$oy w=$w" test "$oy" -eq 0 -a $((ox + w)) -eq 1600
check "x11: reserva no topo" "_NET_WM_STRUT_PARTIAL top" bash -c 'xprop -name "Big Tabbed Dock" _NET_WM_STRUT_PARTIAL | grep -q "= 0, 0, [1-9]"'
import -window root $S/x11-13-top-right-modern.png
# 4. largura total + auto-ocultar
big-wm-tabbed-dock --quit; sleep 0.5
sed -i 's/^Position=.*/Position=bottom/; s/^Alignment=.*/Alignment=fill/; s/^Style=.*/Style=classic/; s/^AutoHide=.*/AutoHide=true/' "$cfg"
big-wm-tabbed-dock >>/work/dock-x11v.log 2>&1 & wait_for 10 state
xdotool mousemove 800 400; sleep 2
st=$(state)
check "x11: auto-ocultar recolhe para uma faixa" "hidden=1 (faixa de 4 px pela forma da janela)" bash -c "grep -q hidden=1 <<<\"$st\""
check "x11: auto-ocultar não reserva espaço" "sem strut" bash -c '! xprop -name "Big Tabbed Dock" _NET_WM_STRUT_PARTIAL | grep -q "= 0, 0, 0, [1-9]"'
import -window root $S/x11-14-autohide-hidden.png
xdotool mousemove 800 899; sleep 1
check "x11: auto-ocultar mostra ao tocar a borda" "hidden=0" bash -c 'big-wm-tabbed-dock --print-state | grep -q "hidden=0"'
import -window root $S/x11-15-autohide-shown-fill.png
sed -i 's/^AutoHide=.*/AutoHide=false/; s/^Alignment=.*/Alignment=center/' "$cfg"
big-wm-tabbed-dock --reload; sleep 1

# 5. preferências
big-wm-tabbed-dock --preferences; sleep 2
import -window root $S/x11-16-preferences.png
check "x11: janela de preferências" "Tabbed Dock Preferences" bash -c 'xdotool search --name "Preferences|Preferências" | grep -q .'
pref=$(xdotool search --name "Preferences|Preferências" | head -1); xdotool windowkill "$pref" 2>/dev/null
# 6. HiDPI (GDK_SCALE=2)
big-wm-tabbed-dock --quit; sleep 0.5
GDK_SCALE=2 big-wm-tabbed-dock >>/work/dock-x11v.log 2>&1 & wait_for 10 state; sleep 1
oy=$(state | awk -F'[=,]' '/^origin=/{print $3}'); h=$(state | awk -F'[=x ]' '/^geometry=/{print $3}')
check "x11: HiDPI GDK_SCALE=2 continua na borda" "y=$oy h=$h (px lógicos; tela 900/2)" test $((oy + h)) -eq 450
import -window root $S/x11-17-hidpi-scale2.png
big-wm-tabbed-dock --quit; sleep 0.5

# 7. coexistência: só o dock com abas (Dock clássico desligado pelo wdwrite)
big-wm-tabbed-dock --dock-mode=status
big-wm-tabbed-dock --dock-mode=tabbed
check "x11: modo 'tabbed' grava DisableDock no WindowMaker" "wdwrite" grep -q 'DisableDock = YES' "$HOME/GNUstep/Defaults/WindowMaker"
check "x11: WMState (conteúdo do Dock) intacto" "Applications" grep -q 'Dock' "$HOME/GNUstep/Defaults/WMState"
pkill -x wmaker; sleep 1; wmaker >>/work/wmaker.log 2>&1 & sleep 4
big-wm-tabbed-dock >>/work/dock-x11v.log 2>&1 & wait_for 10 state; sleep 1
import -window root $S/x11-18-mode-tabbed-only.png
big-wm-tabbed-dock --dock-mode=both
check "x11: voltar para 'both' reverte DisableDock" "NO" grep -q 'DisableDock = NO' "$HOME/GNUstep/Defaults/WindowMaker"
big-wm-tabbed-dock --dock-mode=status
grep -v '^$' /work/dock-x11v.log | grep -v 'window tracking' | tail -5
