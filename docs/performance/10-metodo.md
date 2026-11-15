# Método das medições no OptiPlex 760

As medições usam o coletor do próprio projeto
(`tests/perf/bigwm-perf-collect.py`), rodando como root na máquina real,
fora da sessão medida (unidade `bigwm-perf-*` do systemd, contada à parte).

## Login e amostras

- Cada série é de 5 logins reais pela tela do wdm (xdotool no X do wdm),
  da conta `novato`, sem aplicativos do usuário.
- Amostras aos 30, 90 e 300 s depois da abertura da sessão no logind, cada
  uma com uma janela de CPU de 30 s. A tela só bloqueia aos 600 s.
- "Pronto" = compositor rodando, `big-wm-session.target` ativo e fila de
  jobs do `systemd --user` vazia.
- Tabelas: mediana [mín–máx] dos 5 logins.

## O que fica de fora dos totais

- O coletor (unidade `bigwm-perf-*`).
- Os processos da sessão SSH do medidor (`user-1000.slice`: o `systemd
  --user`, o pipewire e os filtros de microfone de quem entra por SSH). O
  coletor os põe em "sistema"; as tabelas os tiram pelo cgroup. A "RAM
  usada" (MemTotal − MemAvailable) os inclui, porque o kernel não separa.

## Uma armadilha encontrada (e corrigida)

Uma série "depois" saiu inválida: o laço de controle abria um SSH a cada
10 s e, sem nenhuma outra sessão do medidor aberta, cada SSH subia e
derrubava o `user@1000.service` inteiro. systemd, polkit, logind e journald
apareciam com ~60 ticks por janela (14 ciclos em 2 min no journal). A
série foi refeita com uma única conexão SSH persistente (ControlMaster),
que mantém o `user@1000` vivo do começo ao fim, como na série "antes".

## Ferramentas instaladas na máquina para a investigação

`strace`, `perf` e `sysstat` (repositórios oficiais). Nada fica rodando.

## Limites

- Uma máquina, um monitor (VGA, 60 Hz), sem medidor de energia.
- O disco é mecânico: as primeiras leituras depois do boot dominam o
  tempo de login a frio; as séries de login (sem reboot) são com cache.
- Medições de quadros não são FPS de tela: os números de compositor são
  CPU por janela de tempo com uma carga fixa.
