# 12 — Teclado preso

Causa: o slurp esperava 120 s; o Spectacle desiste da `CaptureInteractive`
aos 60 s. Entre 60 e 120 s, o slurp (camada com o teclado exclusivo) seguia
na tela, invisível para o Spectacle, segurando teclado e ponteiro. Um
segundo pedido abria outro slurp.

Correção: prazo de 50 s; um pedido novo encerra o slurp anterior; Esc,
conclusão e erro terminam o processo (Popen com `communicate`).

Medido: Esc três vezes seguidas → nenhum slurp nem Spectacle sobrando, e o
texto digitado depois chegou à janela de texto; sem ação, aos 53 s não havia
mais slurp (o Spectacle encerrou junto).
