# 15 — Desempenho

- Nenhum serviço novo, nenhum polling. O wmcliphist faz uma leitura a
  menos por cópia (não toma mais a seleção, nem chama o wl-copy).
- O serviço de captura consulta o `wmake-zones status` uma vez por pedido
  (milissegundos). Captura: o mesmo grim.
- Testes: 7 em 0,4 s.
