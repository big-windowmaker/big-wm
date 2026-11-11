# 13 — Desempenho

- Nenhum polling novo. A troca continua por sinal; o `prctl` custa uma
  chamada de sistema no início do wlmdock; a busca pelo executável só roda
  quando o nome não é `wlmdock`, numa troca de tema.
- Recomeço de um wlmdock: 50–100 ms (do "Starting" ao laço de eventos no log: 51 ms no login, 101 ms numa troca na sessão real).
- Memória: estável em 10 trocas rápidas (07).
