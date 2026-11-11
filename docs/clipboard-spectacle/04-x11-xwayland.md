# 04 — X11 e XWayland

- A ponte do wlroots só entrega a seleção Wayland a um cliente X11 **com o
  foco**: um cliente X sem foco (o `xsel` de um terminal, o wmcliphist) recebe
  vazio. O wmcliphist já tratava isso (não toma a seleção quando não há
  resposta).
- Linha do tempo de uma cópia num aplicativo X11 no XWayland, antes da
  correção: no instante da cópia o dono já era o wmcliphist (10 formatos,
  texto convertido); em menos de 50 ms, o `wl-copy` (texto puro). O
  aplicativo perdia a seleção.
- Sessão X11 pura (Window Maker), antes: o dono passava a ser o wmcliphist e o
  texto servido era outro (CRLF trocado por LF; hash diferente).
