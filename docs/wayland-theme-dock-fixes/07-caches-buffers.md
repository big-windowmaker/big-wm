# 07 — Caches e buffers

Não havia buffer velho preso: o wlmdock nunca recebeu o pedido de trocar.
O desenho do tema no wlmdock é feito do zero quando ele recomeça (estilo
lido do Theme.plist; ladrilhos, menus e balões recriados), então não sobra
`wlr_buffer`, cache Cairo nem textura do tema anterior. No wlmaker, o
`theme_changed` do Clip troca ladrilhos, sobreposição e imagem antes de
avisar os wlmdock.

Medidas (laboratório, 10 trocas disparadas a cada 0,3 s): os mesmos dois
PIDs do wlmdock (sem duplicata), RSS 15,2/15,5 MB → 14,9/15,3 MB; wlmaker
54.280 → 54.512 KB; estado final coerente (claro no tema, no Dock e no
Clip).
