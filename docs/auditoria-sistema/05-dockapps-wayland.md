# 05 — Dockapps no Wayland

Laboratório: wlmaker 31 instalado, aninhado num Xvfb (backend X11, pixman),
HOME e barramento próprios, autostarts da sessão iniciados à mão
(`wlmdock`, `wlmdock --clip`, `big-wm-dockapps` pelo XWayland, Dock com Abas,
bandeja).

- Dock e Clip do wlmaker com os ladrilhos do tema; os dockapps X11
  adotados pelo XWayland, o controlador do Dock com Abas logo acima da
  bandeja, inteiros.
- Modo escuro: ladrilhos, controlador e bandeja escuros.
- Os cortes do X11 não acontecem: o wlmaker usa ladrilhos de 64.

**Não testado numa sessão Wayland real** (iniciada pelo gerenciador de
login): a sessão em uso era a X11 e não foi encerrada.
