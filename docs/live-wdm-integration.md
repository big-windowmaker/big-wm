# Sessão live pelo wdm, usuário `bigwindowmaker` (2026-11-13)

**O que mudou.** Na ISO do Big WindowMaker, a tela de login da sessão live
agora é a do **wdm** (WINGs Display Manager), a mesma do sistema instalado. O
LightDM não inicia mais. O usuário live passou a ser **`bigwindowmaker`**, com
a senha **`big`**, que só vale na sessão live.

## Como funciona

| Peça | Onde | O que faz |
|---|---|---|
| `displaymanager="wdm"` | `iso-profiles/bigcommunity/wmaker/profile.conf` | O manjaro-tools (`util.sh`, `enable_systemd+=("${displaymanager}")`) habilita o `wdm.service` na imagem. O LightDM continua instalado, porque o módulo `displaymanager` do Calamares precisa de um que ele conheça, mas não é habilitado. |
| `username="bigwindowmaker"`, `password="big"` | mesmo `profile.conf` | O `manjaro-live-setup` cria o usuário na camada **live** (livefs). O Calamares não copia essa camada (unpackfs: root e desktop), então o sistema instalado não herda o usuário nem a senha. |
| `live-overlay/etc/wdm/wdm-config` | iso-profiles | É a configuração do pacote wdm com duas linhas a mais: sessão padrão `x11/big-wm-live` e ajuda (F1) em `live-help.txt` com o usuário e a senha. Vale só no live; o sistema instalado usa a do pacote. |
| `live-overlay/usr/share/xsessions/big-wm-live.desktop` | iso-profiles | Roda o `startbiglive` (assistente de idioma, teclado e tema, depois o Window Maker). O `livecd.desktop` da BigLinux tem `TryExec=startplasma-x11`, e o wdm, seguindo a especificação, esconde a sessão: sem este arquivo, o assistente não aparecia na lista. |
| `startbiglive` | biglinux-livecd | Reconhece o `wdm` (antes as mensagens diziam "systemctl restart unknown"). |
| Teste de fumaça e openQA | build-iso | O teste entra pela tela como o usuário faria (Enter, senha, Enter): `smoke-test.sh` e `bigwm_live_login.pm`. |

O wdm não tem login automático. Para a sessão live, a tela já vem com o
usuário escolhido na lista e com a sessão "Big WindowMaker Live". Basta
digitar `big` e apertar Enter.

## Conferido (VM descartável, ISO de 2026-11-12, LightDM trocado pelo wdm em RAM)

| Cenário | Resultado |
|---|---|
| Tela do wdm com `bigwindowmaker` e a sessão "Big WindowMaker Live" escolhidos | PASSOU |
| Senha `big` → sessão X11 no logind (`seat0`, VT7, ativa), `startbiglive` e `wmaker` | PASSOU |
| `sudo` sem senha do usuário live (`%wheel`), home do usuário 0700 | PASSOU |
| Sair → volta à tela; F2 → "Big WindowMaker (Wayland)" → sessão Wayland (wlmaker, `seat0`) | PASSOU |
| Sair do Wayland → tela do wdm de novo (servidor X novo) | PASSOU |
| F3 → Halt → Enter: a VM desliga | PASSOU |
| ISO construída com estas mudanças | NÃO EXECUTADO (o build roda no CI do build-iso) |
| Mouse na tela do wdm no virt-manager | ver `virt-manager-input-debugging.md` |
