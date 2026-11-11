# 14 — Regressões e pendências

- Nenhuma regressão detectada (suítes do big-wm e do wlmaker; laboratório).
- Pendência visual: capturar o Dock e o Clip reais nos dois temas quando a
  tela for desbloqueada.
- O wlmaker 32 só passa a rodar no próximo login; até lá a sessão usa o 31
  com os wlmdock novos (suficiente, ver 04).
- Achado à parte, não corrigido: com `LANG=pt_BR`, o teste
  `root_menu.generated_menu` do wlmaker falha — o `wlmtool` gera o menu
  "Debian" traduzido com textos não ASCII sem aspas, e o analisador de plist
  os recusa. Afeta o RootMenuDebian.plist (não é o menu do Big
  WindowMaker); o pacote foi compilado com `LANG=C.UTF-8`, como antes.
- `big-wm-doctor` ganhou a verificação "Dock e Clip (Wayland)": wlmdock sem
  o nome e controlador acoplado sem rodar.
