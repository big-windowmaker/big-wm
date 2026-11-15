# Wayland (wlmaker) no OptiPlex 760

O Wayland em repouso já era leve: o wlmaker gasta ~0,4 % de um núcleo. A
CPU total caiu de **2,6 % para 2,0 %** (dois núcleos) com a correção do
wmusic, que roda pelo XWayland. Neste hardware o renderizador pixman ganha
do GLES2 em repouso e em memória, e empata com vídeo.

## Resultado (sessão Wayland, 5 logins cada, mediana [mín–máx]; janela de CPU de 30 s aos 300 s)

| Métrica | Antes | Depois | Ganho |
|---|---:|---:|---:|
| CPU ocupada (% dos 2 núcleos, janela de 30 s) | 2,6 [2,5–2,7] | 2,0 [2,0–2,1] | −23 % |
| Trocas de contexto/s (sistema) | 849 [798–867] | 563 [541–585] | −34 % |
| wmusic (ticks de 10 ms em 30 s) | 22 [21–24] | 0 | −100 % |
| wlmaker (ticks em 30 s) | 12 [11–12] | 12 [11–12] | igual |
| PSS da sessão, com o wlmaker e o XWayland (MiB) | 337,4 [336,9–342,0] | 337,2 [336,7–337,4] | igual |
| PSS de todos os processos (MiB) | 443,0 [442,0–446,4] | 443,9 [442,9–445,2] | igual |

"Antes" = wlmaker 40, big-wm `eab850e`, big-dockapps 14; "depois" =
big-dockapps 15 ([03-x11.md](03-x11.md), seção 2).

## Renderizador: pixman x GLES2 (crocus)

Nesta máquina o `/etc/environment` tem `WLR_RENDERER=pixman` (ajuste local
da máquina, depois de travamentos da GPU com o glamor no X11). Sem ele, o
teste do wlmaker (patch 0100, que troca o llvmpipe pelo pixman) escolhe o
GLES2 no driver de hardware. Comparação na mesma sessão, 3 rodadas de cada,
20 s por medida; carga fixa = mpv com `--vo=wlshm` tocando um `testsrc`
1280×720 a 30 quadros/s numa janela:

| Métrica | pixman | GLES2 |
|---|---:|---:|
| Repouso: wlmaker + XWayland (ticks em 20 s) | 10–12 | 23 |
| Vídeo: wlmaker (ticks em 20 s) | 190–198 | 165–174 |
| Vídeo: XWayland (ticks em 20 s) | 5–6 | 19–20 |
| PSS do wlmaker em repouso (MiB) | 24,9–25,7 | 36,6–37,5 |
| Shmem do sistema em repouso (MiB) | 59 | 69–70 |
| Travamento da GPU | nenhum | nenhum |

**Decisão:** pixman nesta máquina (verificado). O repouso é o estado mais
comum, e nele o pixman gasta metade da CPU e 12 MiB a menos. Com vídeo, o
GLES2 economiza no wlmaker o que perde no XWayland. O padrão do projeto não
muda: uma máquina não prova o resultado para todo o crocus (Gen4 a Gen7;
um Haswell tem uma GPU bem mais forte). O perfil Legacy
([08-perfis.md](08-perfis.md)) é o lugar para essa escolha.

## Achados sem mudança

- **Relógio do sistema pelo HPET.** O kernel marca o TSC instável ("TSC
  halts in idle": o E8400 não tem `nonstop_tsc` e o estado C3 é o mais
  usado), e cada `clock_gettime` vira uma syscall com leitura do HPET
  (`read_hpet` aparece no perf de laços do GLib). Limitar o idle a C1
  (`processor.max_cstate=1`) traria o TSC de volta, a um custo de energia
  que esta medição não tem como pesar. Não aplicado.
- **Xorg com SNA ainda travou a GPU 3 vezes** neste boot (dmesg: "GPU HANG
  ... in Xorg", com reset do chip), todas entre 23 e 26 min depois do boot,
  fora das medições. O Wayland com pixman não usa a GPU para compor.
