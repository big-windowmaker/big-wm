# 04 — wmusic (players MPRIS)

## Problema

O `wmusic` (dockapps do Window Maker, pacote `big-dockapps`) já falava MPRIS pelo `playerctl`. O patch `wmusic-segue-player` (anterior a este trabalho) fazia ele seguir o player que está tocando e não congelar ao abrir o player.

Medido no Xvfb, o `wmusic` acordava 10 vezes por segundo o tempo todo. A cada vez:

- listava os players no D-Bus;
- pedia a posição ao player (o MPRIS não a sinaliza);
- redesenhava o ladrilho inteiro, mesmo parado.

Pelo XWayland, cada redesenho é um quadro a mais para o compositor.

## Correção (patch `wmusic-light-idle`, `big-dockapps` 20260412-10)

- **Lista de players:** uma vez por segundo. O fechamento continua imediato: o sinal `exit` do `PlayerctlPlayer` solta o player na hora e força a próxima lista. Sem isso, a primeira versão do patch consultava por até 1 s um player que já tinha saído (34 avisos `ServiceUnknown` no log); com ele, 0.
- **Posição:** pedida uma vez por segundo e contada no intervalo enquanto toca. É pedida de novo logo depois de um botão (próxima, anterior, avançar, voltar).
- **Sem player, ou com ele parado:** o visor `--` é desenhado uma vez e o laço acorda uma vez por segundo.
- **Tocando ou em pausa:** seta, título e tempo seguem a 10 quadros por segundo, como antes.

## Seleção do player

Do patch anterior, mantida:

- segue o que está tocando; senão um em pausa; senão o primeiro;
- enquanto o atual não toca, passa para outro que começou a tocar;
- o botão de ejetar continua trocando de player à mão;
- os comandos vão só ao player atual: no teste com dois players, o Pause não chegou ao outro.

## Comando

- **Linha de comando no Dock:** `wmusic -c "big-wm-open music"`. O clique duplo no visor abre o player de música padrão (`big-wm-open music`), em segundo plano. Nada é aberto no login.
- **Sem `-w`:** nele `-w` é `--windowed`, janela comum. É o oposto do `wmpulsemixer -w`.

## Testes

Feitos no Xvfb, com um barramento de sessão próprio e players MPRIS de teste (objetos D-Bus reais com Play, Pause, PlayPause, Next, Previous, Metadata e PropertiesChanged).

- **14 verificações, todas ok:**
  - sem player continua vivo;
  - player aberto depois é encontrado;
  - Play, Pause, continuar, próxima, anterior;
  - com dois players, o Pause vai ao que tocava e o outro não recebe nada;
  - segue o player que passou a tocar;
  - player fechado: desconecta;
  - player reaberto: controla de novo;
  - 0 avisos.
- **CPU em 20 s** (ticks de 10 ms):

| | Antes | Depois |
|---|---|---|
| Sem player | 19 | 3 |
| Tocando | 16 | 5 |
| Pedidos ao player | ~10/s | ~1/s |

## Reprodutores reais

- O teste usou players MPRIS de laboratório: Spotify, VLC, Rhythmbox e o Big Video Player não estão instalados nesta máquina.
- O Firefox da sessão real publica MPRIS (`org.mpris.MediaPlayer2.firefox.instance_1_102`). Não foi controlado, para não mexer na sessão do usuário.
- Navegadores só publicam MPRIS enquanto há mídia na página. O `wmusic` não presume nada: segue o que estiver no barramento.
