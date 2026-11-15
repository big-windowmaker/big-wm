# Resultados: OptiPlex 760, 2026-11-15

O X11 em repouso gasta **58 % menos CPU** e o Wayland **23 % menos**, e o
gerenciador de login (wdm) inicia **6,2 s mais cedo** no disco rígido
(o Xorg da tela de login, 3,6 s). A memória da sessão
não mudou: nesta máquina, o desperdício era CPU acordando à toa, não RAM.
Aparência, dockapps e funções ficaram iguais.

## Antes e depois (mediana [mín–máx])

| Métrica | X11 antes | X11 depois | Wayland antes | Wayland depois |
|---|---:|---:|---:|---:|
| CPU em repouso (% dos 2 núcleos; janela de 30 s aos 300 s) | 7,2 [7,1–7,5] | 3,0 [2,9–3,2] | 2,6 [2,5–2,7] | 2,0 [2,0–2,1] |
| Trocas de contexto/s | 2892 | 1278 | 849 | 563 |
| PSS da sessão (MiB) | 282,3 | 282,6 | 337,4 | 337,2 |
| PSS do servidor X (MiB) | 40,0 | 32,8 | — | — |
| PSS de todos os processos (MiB) | 424,1 | 414,4 | 443,0 | 443,9 |
| Processos na sessão | 65 | 65 | 82 | 82 |
| Login → pronto, com cache (s) | 5,0 [4,8–5,1] | 5,1 [5,0–5,1] | 5,7 [5,6–6,1] | 5,7 [5,5–5,7] |

N = 5 logins por coluna. Boot (3 reboots cada): wdm aos 20,1 → 13,9 s;
Xorg da tela de login aos 20,7 → 17,1 s; login a frio 8,7 → 9,3 s (sem
diferença clara). Detalhes nos arquivos por tema.

## Alterações

| Repositório | Mudança | Ganho medido |
|---|---|---|
| big-wm (`big-wm-x11-repaint`, `tests/run.sh`) | tira o picom do laço de vblank também quando a saída primária muda (RandR) | picom 183 → 46 ticks/30 s; Xorg 66 → 28 |
| big-wm (`session/x11-repaint/big-wm-x11-repaint.c`) | o mesmo helper em C (antes Python + ctypes), no pacote `big-wm-locate-pointer` | 8,2 MiB → 0,3 MiB de PSS por sessão X11 |
| packages (`big-dockapps` 15, `wmusic-player-manager.patch`) | lista de players por sinais, sem consultar o D-Bus a cada segundo | wmusic 22 → 0 ticks/30 s; dbus-broker 31 + 29 → 2 + 0 trocas/s |
| iso-profiles (drop-ins da edição) | `big-driver-manager-gpu` só com NVIDIA ou no live; `auto-unlock-pacman` com CPU e disco ociosos | wdm 6,2 s e Xorg 3,6 s mais cedo |
| big-wm (`big-wm-compositor`, menu Configurações) | "Efeitos visuais (compositor)": desliga e religa o picom do X11, nesta sessão e nas próximas | CPU da máquina em repouso: 60–95 → 16–26 ticks/30 s (uma janela "ligado" deu 26), também depois de relogar ([11-cenarios.md](11-cenarios.md)) |
| big-wm (`session/zoom/big-wm-picom-zoom.c`) | a ampliação não perde mais um entalhe da roda (evento preso na fila do xcb) | falha intermitente do `make check`: 9/105 → 0/60 |
| big-wm (`docs/performance/`, `tests/perf/bigwm-perf-cenarios.sh`) | esta documentação e os cenários de uso | — |

Nada mudou na aparência: mesmo tema, mesmo picom (xrender, vsync,
sombras), mesmos dockapps e mesma taxa de atualização.

## O que foi verificado na máquina real

- X11 e Wayland, logins reais pelo wdm, 5 por série; boot com 3 reboots
  por série.
- O desktop desenhado depois da correção do picom (Dock, Clip, dockapps).
- wmusic seguindo um player MPRIS que aparece e sai.
- Nenhuma unidade com falha e nenhum travamento da GPU durante as medições.

## Não verificado ou fora desta rodada

- **Causa interna do laço no picom 13**: o mecanismo (cada
  `PresentCompleteNotify` pede outro quadro) está verificado; a origem do
  dano a cada quadro não. Reprodução e texto para o upstream em
  [13-propostas.md](13-propostas.md).
- **Cenários B a F**: medidos, uma série cada, em
  [11-cenarios.md](11-cenarios.md) (abrir programas, tudo aberto, pressão
  de memória, mover janelas, sair da sessão). Sem série "antes": servem
  para saber onde está o limite, não para comparar.
- **Multi-monitor**: um monitor só.
- **Energia**: sem medidor; o HPET (TSC instável no idle C3) fica como
  achado.

## Próximas oportunidades

1. Compositor no X11 com dockapps: ~2/3 da CPU que sobra em repouso (89–99
   → 25–30 ticks sem picom). Desligá-lo agora é uma escolha do usuário no
   menu (Configurações > Efeitos visuais); um picom que componha só o
   retângulo do dockapp seria melhor para todos.
2. `big-wm-mic-tray`: 17 MiB de Python e um `biglinux-microphone-cli
   status` (Python) a cada ~28 s (~0,2 % de um núcleo).
3. BigLinux: verificar o hardware antes do `pacman -Qq` no
   `big-driver-manager-cli gpu boot`; `pacman-fix-keys` a cada boot.
4. Wayland com GLES2 em GPUs mais novas que o G45: o pixman ganhou aqui,
   mas a escolha não deve virar padrão sem outras máquinas.
