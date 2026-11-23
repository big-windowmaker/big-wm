# 09 — Hardware legado (OptiPlex 760)

Core 2 Duo E8400, 3,7 GiB, Intel G41 (Gen4: Xorg com SNA, wlroots com
pixman), disco rígido. r57 instalada do zero nesta rodada pelo Calamares;
sessões reais pelo wdm, entrada por uinput (o mesmo caminho do teclado e
do mouse). Rodada anterior na mesma máquina:
[../01-baseline-optiplex760.md](../01-baseline-optiplex760.md).

## Medido nesta rodada

| O quê | Resultado |
|---|---|
| `big-wm-mic-tray`, PSS na sessão Wayland | 17,2 → 1,1 MiB (pacote desta rodada) |
| `big-wm-mic-tray`, PSS na sessão X11 | 1,1 MiB (C) |
| `big-wm-kwin-screenshot`, PSS (Wayland) | 0,7 MiB (C; Python: ~16 MiB) |
| Partida do mic-tray até o item existir | 167 → 6 ms (mediana de 10) |
| CPU do mic-tray nos 3 s iniciais | 170 → ~0 ms |
| 21 verificações funcionais | 21/21 no X11 e 21/21 no Wayland |
| Pressão com 3,1 GiB incompressíveis (3×) | 0 OOM; 1ª abertura de app 14–23 s (HDD), depois 0,8–4,9 s ([08](08-zram-swap-memory-pressure.md)) |
| Bandeja e menu do microfone | conferidos na tela das duas sessões ([13](13-ux-responsividade.md)) |

## Alocadores no hardware real

Não repetido no OptiPlex. A diferença medida na VM é estrutural (reservas
de arena/segmento de cada alocador sobre um heap de 6–14 MiB) e não depende
da CPU; nenhum alocador reduziu a CPU do compositor fora da dispersão
([03](03-baseline-x11-wayland.md)). Não havia candidato para confirmar.

## O que mais pesa nesta máquina

1. **Disco rígido sob pressão**: o tempo de abrir programas depois de a
   memória encher vem do HDD, não do alocador nem do zram. Menos residentes
   = menos pressão; os portes em C tiram ~16 MiB no X11 e ~31 MiB no
   Wayland.
2. Partida da sessão: cada residente Python custa ~170 ms de CPU no
   login desta CPU; dois deles saíram.
3. GPU Gen4: sem mudança nesta rodada (SNA no X11, pixman no Wayland, já
   decididos antes).
