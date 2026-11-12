# 14 — Desempenho

Sessão real parada (tela bloqueada), 30 s, `/proc`:

| Componente | CPU | PSS |
|---|---|---|
| Dock com Abas + controlador | 0,00% | 47,8 MB |
| Window Maker (2 processos) | 0,00% | 30,7 MB |
| wmusic / wmnet / picom | 0,23% / 0,17% / 0,10% | 1,7 / 0,4 / 5,1 MB |
| demais dockapps e serviços | ≤ 0,03% cada | — |
| **componentes do Big WindowMaker** | **0,57%** | **178 MB** |

Dock com Abas, mesmas condições de ontem (Xvfb 1280×1024, binários
instalados), antes → depois da correção: início 19 → 19 ms, controlador
43 → 43 ms, `--set` aplicado ~30 → 28 ms, 0 de CPU em 60 s nos dois. A regra
nova (um `GArray` por recálculo da prateleira) não aparece nas medidas.
