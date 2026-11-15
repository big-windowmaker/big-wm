# Perfis de desempenho: viabilidade

**Não implementados nesta rodada.** Os ganhos medidos vieram de correções
que valem para todo mundo (picom, wmusic, boot), sem perfil e sem custo
visual. Um perfil só se justifica quando troca aparência ou função por
desempenho, e nesta máquina só um desses itens tem número que o sustente.

## O que um perfil Legacy ou Ultra Light mudaria, com o que foi medido

| Item | Medido no OptiPlex 760 | Custo | Veredito |
|---|---|---|---|
| Wayland com `WLR_RENDERER=pixman` | metade da CPU do wlmaker + XWayland em repouso e 12 MiB a menos que o GLES2; empate com vídeo ([04-wayland.md](04-wayland.md)) | nenhum visível | **verificado aqui**; candidato a Legacy |
| X11 sem compositor (picom desligado) | CPU da máquina em repouso: 89–99 → 25–30 ticks em 30 s ([03-x11.md](03-x11.md), seção 4) | sem sombras, transparência do teclado e do controle, ampliação de tela | escolha do usuário: menu Configurações > Efeitos visuais (`big-wm-compositor`) |
| Sombras e fades do picom | sem diferença em repouso (o custo é só ao abrir e mover janelas; não medido) | aparência | não medido |
| wmnet mais lento | wmnet + picom + Xorg: ~1,6 % de um núcleo com tráfego | o gráfico rola mais devagar (A15) | escolha do usuário |
| `processor.max_cstate=1` (TSC no lugar do HPET) | não medido (sem medidor de energia) | consumo e calor | não recomendado sem medição |

## Como seria, se for feito

- Detecção no primeiro login, sem processo residente: memória total
  (`/proc/meminfo`), CPU (núcleos, flags), disco rotacional
  (`/sys/block/*/queue/rotational`) e driver da GPU (`/sys/class/drm`).
- Recomendação gravada em `~/.config/big-wm/` e mostrada uma vez; nunca
  aplicada sozinha nem por cima de uma escolha anterior.
- Aplicação pela infraestrutura que já existe: o drop-in do picom (X11) e o
  ambiente do `startbigwm-wayland` (o `WLR_RENDERER` que ele já respeita).
  Reverter = escolher "Padrão".

Fica pendente até haver medição em mais de uma máquina antiga (um Gen4 com
GLES2 estável ou um Haswell podem dar resultado oposto ao do pixman).
