# Relatório final — volume (wmpulsemixer) e música (wmusic) no Dock

Trabalho de 09/11/2026, instalado na máquina de desenvolvimento e **não publicado** (sem push, PR nem ISO). Detalhes nos documentos 01 a 06 desta pasta.

## Resultado

Ordem padrão do Dock nas duas sessões: **bandeja → wmpulsemixer → wmusic** → os demais.

- **Bandeja:** `big-wm-tray` no Wayland, `wmsystemtray` no X11.
- **wmpulsemixer:** controla a saída e o microfone pelo PipeWire ou PulseAudio.
- **wmusic:** controla os players MPRIS.
- **Big Media Player:** mostra o volume e a reprodução reais, porque todos leem as mesmas fontes por eventos.

## Mudanças por repositório (commits locais)

| Repositório | Branch | O quê |
|---|---|---|
| `packages` | `feat/dockapps-audio-2026-11` | `wmpulsemixer` 0.2.2-1 (novo; patch 0001: espera o servidor de áudio, roda do mouse com o volume real, menu sem efeito nos controles, sem avisos do GDK) e `big-dockapps` 20260412-10 (`wmusic` leve) |
| `big-wm` | `feat/dockapps-audio-2026-11` | `WMState` e `State.plist` padrão; grupo de volume e música no `big-wm-dockapps`; nenhum dockapp em dobro; migração `big-wm-dock-audio`; itens do menu Dockapps; dependências; traduções pt_BR/es; testes; esta documentação |
| `wlmaker` | `fix/dockapp-menu-overlay` (sobre `fix/xwl-toplevel-geometry`) | Menu de dockapp do XWayland visível; forma do ladrilho aplicada de novo quando ele muda de lugar |
| `iso-profiles` | `feat/dockapps-audio-2026-11` | `wmpulsemixer` no `Packages-Desktop` |
| `build-iso` | `feat/dockapps-audio-2026-11` | Artefato `packages-wmpulsemixer` e pacote esperado na ISO |
| `big-screen-monitor-display` | — | Nada: já usava PipeWire (eventos do `pactl subscribe`) e MPRIS (sinais do D-Bus) |

## Pacotes instalados nesta máquina

- `wmpulsemixer` 0.2.2-1
- `wmusic` 20260412-10
- `wlmaker` 1:0.8.r80.g7481afe-30
- `big-wm-*` 26.11.09-2239

A migração e o wlmaker novo valem a partir do próximo login.

## Critérios de aceitação

| # | Critério | Situação |
|---|---|---|
| 1 | wmpulsemixer inicia automaticamente | Implementado e testado (X11 e Wayland, laboratório) |
| 2 | wmusic inicia automaticamente | Implementado e testado |
| 3 | Os dois visíveis | Implementado e testado; no Wayland, depois da correção da forma no wlmaker 30 |
| 4 | Acoplados ao Dock | Implementado e testado |
| 5 | Ordem pedida | Implementado e testado (contas novas; contas existentes mantêm a posição escolhida para o volume, ver abaixo) |
| 6 | Posição persiste | Implementado e testado (reinício do wmaker e da sessão Wayland) |
| 7 | Volume funciona | Implementado e testado |
| 8 | Microfone | Implementado e testado (barra, mudo, troca de entrada) |
| 9 | Controles MPRIS | Implementado e testado com players de laboratório; reprodutores reais (Spotify, VLC...) não instalados aqui |
| 10 | Integração com o Big Media Player | Implementado e testado (agente de mídia real do BSMD, 11/11) |
| 11 | X11 funcional | Implementado e testado no laboratório; aguardando o login real |
| 12 | Wayland funcional | Implementado e testado no laboratório; teclado no menu de dispositivos não funciona (mouse sim) |
| 13 | Sem instâncias duplicadas | Implementado e testado |
| 14 | Sem conflito com outros dockapps | Implementado e testado (um volume só; wmix e wlmmixer não sobem juntos) |
| 15 | Tema claro e escuro | Testado: os ladrilhos seguem o tema; o visor LCD dos dockapps é o mesmo nos dois, como o dos outros dockapps do Dock |
| 16 | Reorganizar ou remover | Implementado e testado (a migração respeita ordem e remoções; item "Restaurar volume e música no Dock") |
| 17 | Instalação e atualização | Implementado e testado (pacotes instalados; migração em cópias da conta real e no laboratório) |
| 18 | Documentação | Feita (esta pasta) |
| 19 | Testes relevantes | Feitos; o que faltou está abaixo |

## A conta desta máquina

O Dock do X11 daqui foi reorganizado à mão: wmix na posição 10, bandeja na 12.

- **Regra:** a migração respeita a organização pessoal, então o wmpulsemixer fica na 10, o wmusic na 11 e a bandeja desce para a 13.
- **No Wayland:** o wmix já estava logo depois da bandeja, então fica bandeja → wmpulsemixer → wmusic.
- **Para ter exatamente bandeja → volume → música também no X11:** menu Dockapps > **Restaurar volume e música no Dock**, e entrar de novo.

## Pendências e limitações

- **Teclado no menu de dispositivos no Wayland:** as setas não navegam (o GTK no XWayland não recebe as teclas). O mouse, o Esc e o clique fora funcionam.
- **Não testado:**
  - reprodutores reais (Spotify, VLC, Rhythmbox, Big Video Player não instalados; o Firefox da sessão real não foi tocado);
  - vários monitores;
  - o login real, que depende de sair da sessão;
  - a reconexão de áudio dentro do wlmaker aninhado (o mesmo binário passou no Xvfb).
- **Telas de 768 linhas.**
  - X11: com o wmusic a mais, o último dockapp que a sessão acrescenta sozinha (os olhos) não cabe. Fica no menu e entra numa tela maior (regra do `dock_add_once`).
  - Wayland: o Dock padrão já passava da tela antes; continua assim.
- **`wlmaker_test` em pt_BR:** o caso `generated_menu` falha com `LANG=pt_BR.UTF-8` (o menu gerado pelo `wlmtool` traz um caractere que o leitor de plist recusa). Passa com `LANG=C`. Não tem relação com este trabalho e não afeta o menu do Big WindowMaker. Vale investigar à parte.
- **Upstream:** os patches do wmpulsemixer e do wmusic e as correções do wlmaker estão escritos para envio, mas não foram enviados.
