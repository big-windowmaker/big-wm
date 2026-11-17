# 6. Desempenho

Medições no lab Wayland (wlmaker headless, pixman), PSS de
`/proc/PID/smaps_rollup`. Sem outros processos GTK no lab: na sessão real o
PSS é menor (as bibliotecas são divididas com bandeja, Thunar etc.).

| Situação | PSS |
|---|---|
| big-wm-notifications ocioso, logo após iniciar | 5,4 MiB |
| após 300 notificações (histórico limitado a 200) | 11,6 MiB |
| central aberta (200 no histórico, 50 linhas montadas) | 18,1 MiB |
| central fechada de novo | 14,6 MiB |
| dunst ocioso (referência) | 1,8 MiB |
| protótipo Python + PyGObject ocioso (descartado) | 23,4 MiB |

- CPU ociosa: 0 tiques em 20 s (sem polling).
- Antes da paginação e do `malloc_trim`: central aberta 27,4 MiB, fechada
  24,3 MiB.
- O crescimento após o primeiro balão é o carregamento único do tema de
  ícones, das fontes e do CSS do GTK.
- Inicialização: o nome `org.freedesktop.Notifications` responde em menos de
  1 s no lab (o teste de conformidade espera até 5 s e nunca precisou).
- Sob carga: 80 notificações seguidas de um remetente e 300 de remetentes
  diferentes sem perda de resposta; no máximo 3 balões; nenhum processo órfão
  (o servidor não cria processos, exceto o som opcional).
