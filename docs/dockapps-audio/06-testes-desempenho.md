# 06 — Testes e desempenho

## Laboratório

O laboratório não toca no áudio, no D-Bus nem na sessão do usuário:

- **Áudio:** PipeWire próprio, com WirePlumber sem monitores de hardware e `pipewire-pulse`, em `XDG_RUNTIME_DIR` próprio. Dispositivos virtuais: duas saídas e duas entradas.
- **D-Bus:** barramento de sessão próprio, com players MPRIS de teste (objetos D-Bus reais) e o barramento do sistema apontado para um socket inexistente.
- **Desligamento bloqueado:** `systemctl`, `loginctl`, `reboot`, `poweroff`, `halt` e `shutdown` são substituídos por stubs que recusam.
- **X11:** Xvfb com o Window Maker instalado e uma conta nova com o `WMState` padrão novo.
- **Wayland:** wlmaker aninhado num Xvfb (`mklab.sh audio`), XWayland, e o Dock, o Clip, a bandeja e o `big-wm-dockapps` como no autostart.

## Matriz

| Funcionalidade | X11 (Window Maker) | Wayland (wlmaker + XWayland) |
|---|---|---|
| Inicialização automática | ok (AutoLaunch, conta nova) | ok (`big-wm-dockapps`) |
| Posição abaixo da bandeja | ok (0,2 → 0,3 → 0,4) | ok (bandeja → wmpulsemixer → wmusic) |
| Persistência da posição | ok (WMState depois de reiniciar o wmaker; as duas listas) | ok (`Dockapps.plist` depois de reiniciar a sessão) |
| wmpulsemixer visível | ok | ok |
| wmusic visível | ok | ok (forma corrigida no wlmaker 30) |
| Controle de volume (barra, roda) | ok | ok |
| Controle do microfone (barra, mudo) | ok no Xvfb (janela comum); acoplado no Window Maker não repetido | ok (acoplado) |
| Mudo | ok | ok |
| Troca de dispositivos (menu) | ok (mouse e teclado) | ok pelo mouse (menu corrigido no wlmaker 30); teclado não |
| Play/pause | ok | ok |
| Próxima/anterior | ok | ok |
| Detecção MPRIS (player aberto depois) | ok | ok |
| Sem player ativo | ok (`--`, vivo, 1 despertar/s) | ok |
| Reconexão de áudio (servidor derrubado/religado) | ok (Xvfb) | mesmo binário e mesma conexão (libpulse); não repetido no aninhado |
| Menu contextual | ok | ok |
| Reorganização no Dock | migração preserva a ordem do usuário (testes 12/12) | idem |
| Tema claro | ok (captura) | ok (captura) |
| Tema escuro | ok (captura) | ok (captura) |
| Reinicialização da sessão | ok (1 instância de cada) | ok (1 instância de cada) |
| Integração Big Media Player | ok, 11/11 (agente de mídia real do BSMD no laboratório) | mesma fonte (PipeWire/MPRIS), independente da sessão |

## Testes adicionais

1. **Primeiro login de usuário novo:** os dois labs, com o `WMState` e o `State.plist` padrão.
2. **Atualização de uma instalação existente:**
   - cópia da conta real: X11 nas duas listas, Wayland no `Dockapps.plist`;
   - laboratório Wayland com o `Dockapps.plist` real: bandeja → wmpulsemixer → wmusic → relógio…, sem wmix nem wlmmixer rodando.
3. **Usuário que removeu um dockapp:** `test_volume_removed_by_user` (X11 e Wayland).
4. **Usuário que mudou a ordem:** `test_user_order_kept` e a conta real.
5. **Início sem servidor de áudio:** `--` e conexão cerca de 3 s depois de o servidor subir.
6. **Reinício do PipeWire:** reconectou sozinho.
7. **Player aberto depois do login:** encontrado.
8. **Player encerrado:** `--` na hora (sinal `exit`).
9. **Alternância entre dois players:** segue o que toca; o comando vai só ao atual.
10. **Mudança entre X11 e Wayland:** cada sessão tem a marca dela; o `Dockapps.plist` e o `WMState` são independentes.
11. **Vários monitores:** não testado (o Dock fica num monitor; nada mudou nisso).
12. **XWayland sob demanda:** a sessão real usa `--lazy_xwayland` e o wmix já subia por ele. O laboratório usou `--start_xwayland`.

## Testes automáticos

- **big-wm `tests/run.sh`:** 45 verificações, todas passando. Inclui:
  - `tests/dockapps/test_dock_audio.py` (12 casos: X11 e Wayland, ordem do usuário, removidos, Clip, tela baixa, listas por altura, `State.plist` regravado, restaurar);
  - o `big-wm-dockapps` com o grupo de volume e a música;
  - o `State.plist` e o `WMState` padrão;
  - o Dock padrão numa tela de 700 linhas.
- **wlmaker:** `ctest` 29/29. O `wlmaker_test` passa com `LANG=C` (30/30). Com `LANG=pt_BR.UTF-8` falha só o `generated_menu`, que gera o menu pelo `wlmtool` com os `.desktop` instalados: não tem relação com estas mudanças e o menu do Big WindowMaker não usa o `wlmtool`.
- **Pacotes:**
  - `wmpulsemixer`: `check()` (versão e `-w`);
  - `big-dockapps` 20260412-10: compilação completa.

## Desempenho

| Processo | CPU parado (20 s, ticks de 10 ms) | Memória privada | Observação |
|---|---|---|---|
| wmpulsemixer | 0 (sem servidor de áudio: 0) | ~6,8 MB (PSS 8,2 MB) | eventos do libpulse; reconexão por um timer só |
| wmusic antes | 19 sem player, 16 tocando | — | 10 listas de players e ~10 pedidos ao player por segundo |
| wmusic depois | 3 sem player, 5 tocando | ~1,1 MB (PSS 1,4 MB) | 1 lista/s, ~1 pedido/s |
| wmix (comparação) | — | ~1,5 MB (PSS 1,8 MB) | |

- O custo de memória do wmpulsemixer é o GTK 3, só para o menu de dispositivos.
- Nenhum daemon novo; nenhum serviço de sincronização.
- O Big Media Player e os dockapps leem as mesmas fontes por eventos.
