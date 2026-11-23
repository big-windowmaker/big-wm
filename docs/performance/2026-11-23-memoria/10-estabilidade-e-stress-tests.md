# 10 — Estabilidade e testes de estresse

## O que rodou

| Teste | Volume | Resultado |
|---|---|---|
| Lotes de alocadores ([03](03-baseline-x11-wayland.md)) | 68 boots, 7 perfis, X11 e Wayland; 40 janelas abertas e fechadas por boot (2720 ciclos; no Wayland, nativas e pelo XWayland alternadas) | nenhum compositor caiu (mesmo PID de 30 a 600 s em todos); todas as cargas terminaram |
| Sessões de 1 h | 4 boots (glibc e jemalloc × X11 e Wayland), carga de 40 janelas repetida a cada 10 min (6 cargas, 240 janelas por sessão) | nenhuma queda; nenhum uso de swap; ver abaixo |
| Matriz do Virt-Manager ([04](04-validacao-virt-manager.md)) | 14 boots, 2–16 GiB, 1024x768–2560x1440 | 21/21 verificações funcionais em todos |
| Pressão de memória no OptiPlex ([08](08-zram-swap-memory-pressure.md)) | 3 × 3,1 GiB incompressíveis por 120 s | sem OOM, sessão viva |
| Clip em grade (rodada anterior, mesma imagem) | 60 movimentos de ícones no OptiPlex | RSS do wlmaker em patamar |

## Sessões de 1 h (PSS, MiB)

| Instante | wlmaker glibc | wlmaker jemalloc | wmaker glibc | wmaker jemalloc |
|---|---:|---:|---:|---:|
| 30 s | 24,4 | 25,4 | 9,9 | 10,8 |
| 300 s (1ª carga) | 27,2 | 28,3 | 10,0 | 11,0 |
| 900 s | 29,3 | 30,6 | 9,9 | 10,9 |
| 1800 s | 30,5 | 31,9 | 10,0 | 11,1 |
| 3600 s | 30,5 | 31,8 | 10,1 | 11,1 |

- wmaker: plano a hora inteira.
- wlmaker: sobe com as primeiras cargas e para; de 1800 a 3600 s houve
  mais 3 cargas (120 janelas) sem crescimento, com os dois alocadores.
  Patamar de heap, não vazamento. (1 boot por perfil.)

O total da sessão sobe entre 300 e 900 s por dois motivos que não são
vazamento, e depois fica plano até 3600 s:

| Processo | Wayland | X11 | Causa |
|---|---:|---:|---|
| bloqueio de tela | gtklock +27,5 | xsecurelock +1,0, Xorg +17,3 | a VM ociosa bloqueia a tela em ~10 min (custo já registrado em A13) |
| pamac-tray | +4,2 | +4,2 | aviso de atualizações da BigLinux, sobe atrasado |
| big-wm-notifications | +3,0 | — | primeira notificação mostrada |

## Não feito

- Sessões de 1 h com os outros perfis (descartados pela memória antes).
- Estresse de entrada (teclado/mouse em rajada) e troca de monitor durante
  a carga: cobertos por rodadas anteriores (monitor principal, Clip), não
  repetidos aqui.
- Execução de vários dias.
