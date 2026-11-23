# Desempenho no hardware antigo

Rodada seguinte (2026-11-23: memória, alocadores, estabilidade):
[2026-11-23-memoria/](2026-11-23-memoria/README.md).

Rodada de 2026-11-15 num Dell OptiPlex 760 (Core 2 Duo E8400, Intel G45,
3,66 GiB, disco rígido). Resultado em [12-resultados.md](12-resultados.md).

| Arquivo | Conteúdo |
|---|---|
| [01-baseline-optiplex760.md](01-baseline-optiplex760.md) | Máquina, linha de base das duas sessões e do boot |
| [03-x11.md](03-x11.md) | Picom preso a 60 quadros/s, wmusic, dockapps, backends do picom |
| [04-wayland.md](04-wayland.md) | Wayland antes/depois; pixman x GLES2 no crocus; HPET |
| [06-boot-e-servicos.md](06-boot-e-servicos.md) | wdm 6,2 s mais cedo no disco rígido |
| [08-perfis.md](08-perfis.md) | Perfis Legacy/Ultra Light: o que foi medido e por que não entraram |
| [10-metodo.md](10-metodo.md) | Como foi medido, o que fica fora dos totais, armadilhas |
| [11-cenarios.md](11-cenarios.md) | Uso cotidiano: abrir programas, tudo aberto, pressão de memória, mover janelas, sair da sessão |
| [12-resultados.md](12-resultados.md) | Antes e depois, alterações, pendências |
| [13-propostas.md](13-propostas.md) | Textos para o picom e a BigLinux (enviados: links no topo) |

Os números não substituem os da VM em
[../performance-benchmarks.md](../performance-benchmarks.md) e as decisões
anteriores em [../performance-decisions.md](../performance-decisions.md)
(A1–A15), que continuam valendo. A numeração segue o pedido da missão; os
temas sem conteúdo novo (memória, dockapps, regressões) estão dentro dos
arquivos acima.
