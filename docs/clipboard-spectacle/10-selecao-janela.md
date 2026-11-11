# 10 — Selecionar janela

`CaptureInteractive(kind=0)`: o slurp recebe as caixas das janelas do
compositor (`wmake-zones windows -l`). Correções: a janela ativa vai
primeiro, janelas fora das telas (minimizadas) ficam de fora, e as caixas
com coordenadas negativas (`1519x985+5231-2`) deixaram de ser descartadas
como inválidas.

Laboratório: duas janelas sobrepostas (Vermelha 500×300 embaixo, Azul
420×260 em cima); clique na Azul → imagem 422×293 (a Azul com a moldura),
azul no centro; o slurp terminou.
