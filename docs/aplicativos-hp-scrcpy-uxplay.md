# HP Device Manager, HP Scan, scrcpy e UxPlay nas sessões Big WindowMaker

Missão de novembro de 2026, rodada 2 (agente "apps"). Ramo
`feat/apps-hp-scrcpy-uxplay-mic` (base `feat/missao-integracao`).

Na sessão real (Wayland, wlmaker) estes quatro itens do menu falhavam:
clicar não fazia nada (HPLIP, scrcpy) ou abria um terminal com erros
(UxPlay). Nada aqui altera arquivos dos pacotes de terceiros: cada um ganha
um wrapper do big-wm e uma entrada de menu própria, só das sessões Big
WindowMaker (`OnlyShowIn=WindowMaker;wlmaker;`); as entradas originais devem
ser ocultadas nessas sessões pelos menus (agente "menus", ver
[Entradas de menu](#entradas-de-menu-para-o-integrador)).

Capturas: `docs/screenshots/aplicativos/` (as completas dos testes em
`~/.cache/bwm-pkg/apps/shots/`).

## 1. HP Device Manager e HP Scan (HPLIP)

### Problema
`hplip.desktop` (`Exec=hp-toolbox`) e `hp-uiscan.desktop`
(`Exec=/usr/bin/hp-uiscan`): clicar não abria nada, sem impressora HP
conectada ou com ela.

### Causa
As interfaces do HPLIP 3.26.6 são PyQt5, e no pacote `hplip` o
`python-pyqt5` é **dependência opcional** ("for running GUI and
hp-toolbox"). Na máquina de desenvolvimento ele não está instalado
(`pacman -Q python-pyqt5`: não encontrado). Rodando à mão:

```
$ hp-toolbox
HP Linux Imaging and Printing System (ver. 3.26.6)
HP Device Manager ver. 15.0
...
error: No module named 'PyQt5'          (código de saída 0)
```

O mesmo no `hp-uiscan`. Sem terminal, o erro some. Não é Wayland, nem
`QT_QPA_PLATFORM`, nem CUPS: com o PyQt5 no caminho os dois abrem na
sessão Wayland (Qt 5 sem `qt5-wayland` cai no `xcb`, pelo XWayland, com a
moldura do Window Maker).

A ISO do Big WindowMaker (`iso-profiles` `bigcommunity/wmaker/Packages-Desktop`)
já instala `python-pyqt5`: lá as entradas abrem. A falha é de instalações
sem o pacote (como a máquina do usuário).

Notas do teste: o `hp-toolbox` precisa do barramento do sistema (sem ele:
"Unable to connect to dbus system bus. Exiting.") e inicia o `hp-systray`,
que continua rodando depois (comportamento do HPLIP). O `hp-uiscan` leva de
10 a 30 s sondando o SANE antes de dizer "No device connected".

### Solução
`session/bin/big-wm-hp toolbox|scan`:
- sem `hp-toolbox`/`hp-uiscan`: janela "não está instalado" (pacote `hplip`);
- pergunta ao **mesmo python que o HPLIP usa** (`#!/usr/bin/env python`,
  com o site-packages do usuário) se `import PyQt5.QtWidgets` funciona;
  se não, uma janela GTK diz que falta o `python-pyqt5` e como instalar;
- com o PyQt5, `exec` do programa do HPLIP (que explica sozinho a falta de
  impressora/scanner).

Entradas: `data/applications/org.bigwindowmaker.HPDeviceManager.desktop`
(`Exec=big-wm-hp toolbox`, `TryExec=hp-toolbox`) e
`org.bigwindowmaker.HPScan.desktop` (`Exec=big-wm-hp scan`,
`TryExec=hp-uiscan`).

Decisão: o `python-pyqt5` ficou em `optdepends` do `big-wm-session` (a ISO
já o instala). Se o integrador preferir que o HP Device Manager sempre
abra, basta movê-lo para `depends` (≈ 15 MiB com o `qt5-base`, que já está
instalado).

### Testes
- Sem PyQt5 (python do sistema), laboratório Wayland: janela clara —
  `docs/screenshots/aplicativos/hp-sem-pyqt5.png`.
- Com o PyQt5 do pacote oficial extraído num diretório próprio
  (`PYTHONPATH`, sem instalar nada no sistema) e um barramento de sistema
  falso (`dbus-daemon` próprio): `hp-toolbox` abre com "No Installed HP
  Devices Found" (`hp-toolbox-wayland.png`); `big-wm-hp scan` executa o
  `hp-uiscan`, que mostra "No device connected" (`hp-scan-sem-scanner.png`).
- `tests/apps.py`: PyQt5 ausente/presente por um `python` falso e o `exec`
  do `hp-toolbox` falso.

## 2. scrcpy

### Problema
`scrcpy.desktop`: nada visível. `scrcpy-console.desktop`: terminal com
`/bin/sh -c "$SHELL -i -c 'scrcpy --pause-on-exit=if-error'"`.

### Causa
O `Exec` do pacote (`/bin/sh -c "\$SHELL -i -c scrcpy"`, sem terminal)
roda o scrcpy 4.1; sem aparelho ele escreve

```
ERROR: Could not find any ADB device
ERROR: Server connection failed
```

e sai com código 1 em ~0,1 s. Ninguém vê. A entrada "console" existe só para
mostrar esse erro num terminal.

As permissões USB não são o problema: o systemd desta versão já marca as
interfaces ADB/Fastboot com `ID_DEBUG_APPLIANCE="android"` e dá acesso ao
usuário da sessão (`70-uaccess.rules`); não há `android-udev` nem grupo
`adbusers` a configurar.

### Solução
`session/bin/big-wm-scrcpy` (Python; o GTK só carrega quando há o que
mostrar):
1. `adb devices -l` (respeita a variável `ADB`, como o scrcpy);
2. nenhum aparelho → janela com o passo a passo (Opções do desenvolvedor,
   Depuração USB, aceitar a chave; sem fio: `adb pair`/`adb connect`);
   `unauthorized` → desbloquear e tocar em Permitir; `no permissions` →
   uaccess/modo USB; `offline` → reconectar. Todas com **Tentar de novo**;
3. um aparelho pronto → `scrcpy -s <serial>`; vários → lista para escolher;
4. o scrcpy sai com erro (código diferente de 0 e 2) → janela com as últimas
   linhas; registro completo em `~/.local/state/big-wm/scrcpy.log`.
   (0 = janela fechada, 2 = aparelho desconectado: nada a dizer.)

Entrada: `org.bigwindowmaker.Scrcpy.desktop` (`Exec=big-wm-scrcpy`,
`TryExec=scrcpy`).

### Testes
Laboratório Wayland (wlmaker instalado aninhado) e X11 (Xvfb + startbigwm),
`adb` falso para cada estado e o `adb` real sem aparelho:
`scrcpy-sem-aparelho.png`, `scrcpy-nao-autorizado.png`,
`scrcpy-escolher.png`, `scrcpy-erro.png` (scrcpy falso que falha).
`tests/apps.py` lê uma saída real do `adb devices -l` com os quatro estados.
Com aparelho real: não testado (nenhum Android conectado à máquina).

## 3. UxPlay

### Problema
`uxplay.desktop` (`Terminal=true`) abre um terminal com dezenas de

```
GStreamer-CRITICAL **: gst_value_collect_int_range: assertion 'collect_values[0].v_int < collect_values[1].v_int' failed
GStreamer-CRITICAL **: range start is not smaller than end for `GstIntRange'
```

e termina com `*** ERROR: No DNS-SD Server found (DNSServiceRegister call
returned kDNSServiceErr_Unknown)`.

### Causas (reproduzidas na máquina, uxplay 1.73.7, GStreamer 1.28.7)

1. **DNS-SD** — fatal. O uxplay usa a `libdns_sd.so.1` de compatibilidade
   do Avahi, que fala com o `avahi-daemon` pelo barramento do sistema. Na
   máquina: `avahi-daemon.service` e `.socket` **desabilitados e parados**
   (`avahi-daemon --check` → 1). Sem o daemon o registro falha e o uxplay
   sai. No perfil da ISO wmaker, `enable_systemd` liga o `avahi-daemon` no
   sistema instalado, mas `disable_systemd_live` o desliga **no live**.
   O `nss-mdns` (na ISO, `hosts: ... mdns_minimal ...`) só resolve nomes
   `.local`; não anuncia nada e não muda este erro.
2. **Firewall** — o iPhone veria o receptor, mas não conectaria. O ufw
   (ligado: `ENABLED=yes`) da ISO só aceita CIFS e `5353/udp` (mDNS). O
   AirPlay usa TCP 7000, 7001, 7100 e UDP 6000, 6001, 7011 (com `-p`; sem
   `-p` o uxplay usa portas dinâmicas, impossíveis de liberar).
3. **GStreamer** — só ruído, mas com vídeo ruim. `gdb` com
   `G_DEBUG=fatal-criticals`: o aviso nasce em `gst_caps_new_simple` dentro
   do `libgstxvimagesink.so`, na mudança de estado do `autovideosink`.
   Na sessão Wayland o `DISPLAY` é o XWayland, o autovideosink escolhe o
   `xvimagesink`, e o adaptador Xv do XWayland ("glamor textured video",
   16 portas) anuncia a codificação `XV_IMAGE` com **0x0** (consultado com
   `XvQueryEncodings`); o xvimagesink monta `width=[1,0]`. Com
   `-vs waylandsink` o mesmo uxplay dá **0** avisos (24 com o padrão). Não é
   bug do uxplay nem versão de plugin; no X11 de verdade o Xv do servidor
   informa o tamanho máximo e o aviso não aparece.

### Solução
`session/bin/big-wm-airplay`: janela pequena GTK 3 (≈ 450×200), uma por
sessão (`org.bigwindowmaker.AirPlay`):
- **Nome na rede** (gravado em `~/.config/big-wm/airplay.conf`, `Name=`;
  padrão: o nome da máquina);
- **Estado**: Parado / Iniciando / Visível na rede como "X"; aguardando
  conexão (com a dica "Central de Controle › Espelhar Tela") / Conectado:
  *aparelho* está espelhando / erro (DNS-SD ou as últimas linhas do uxplay);
- **Iniciar/Parar**; **Configurar rede...** só aparece quando há o que
  configurar (Avahi parado e/ou ufw sem as portas do AirPlay).

Detalhes:
- `uxplay -n <nome> -nh -p` e, na sessão Wayland, `-vs waylandsink` (se o
  elemento existe); o waylandsink abre uma janela xdg-shell com a moldura
  do wlmaker (teste com `videotestsrc`: `waylandsink-wlmaker.png`).
- "Conectado" vem das conexões TCP **estabelecidas do próprio uxplay**
  (`/proc/<pid>/fd` + `/proc/net/tcp*`, a cada 2 s só com o receptor
  ligado); o nome do aparelho, da linha `connection request from <nome>
  (<modelo>)` do uxplay.
- O uxplay morre com a janela (fechar = parar), com SIGTERM/SIGINT/SIGHUP no
  controle e até com `kill -9` dele (`PR_SET_PDEATHSIG`).
- Diagnóstico sem root e sem inventar: Avahi por `avahi-daemon --check`;
  firewall só quando `/etc/ufw/ufw.conf` diz `ENABLED=yes` **e** o
  `user.rules` é legível sem regra para 7000/tcp.
- **Configurar rede...** roda `pkexec /usr/lib/big-wm/big-wm-airplay-setup
  [avahi] [firewall]` (senha de administrador, só por clique):
  `systemctl enable --now avahi-daemon.service` e `ufw allow UxPlay`
  (perfil `/etc/ufw/applications.d/big-wm-uxplay`, com as portas do `-p`).
  **Nada roda sozinho**: nem no login, nem na instalação. (Não executado
  nos testes: o laboratório bloqueia systemctl e o barramento do sistema.)

Entrada: `org.bigwindowmaker.AirPlay.desktop` ("Receptor AirPlay",
`Exec=big-wm-airplay`, `TryExec=uxplay`).

### Testes
- Laboratório Wayland com o estado real da máquina (Avahi parado, ufw sem
  as portas): parado com os dois avisos e, ao Iniciar, o uxplay real sai
  com "não conseguiu se anunciar (DNS-SD)" — `airplay-parado-dnssd.png`.
- uxplay falso (anuncia, abre uma conexão TCP real, imprime a linha de
  conexão, fecha): "Visível... aguardando" → "Conectado: iPhone de Teste"
  → aguardando de novo — `airplay-aguardando-conectado.png`; Parar e
  fechar a janela encerram o uxplay; segunda chamada `--start` vai para a
  mesma janela; SIGTERM e SIGKILL no controle não deixam uxplay órfão.
- X11 (Xvfb + startbigwm): mesma janela, sem `-vs` (autovideosink).
- Host (sem janela): `uxplay -vs waylandsink` 0 avisos × 24 com o padrão.
- `tests/apps.py`: ufw da ISO bloqueia; desligado, perfil UxPlay ou faixa
  7000:7001 liberam; regras ilegíveis não viram aviso; conexão TCP
  estabelecida × só LISTEN.
- **Não testado**: espelhamento com iPhone/iPad real (sem aparelho Apple) e
  o `big-wm-airplay-setup` com root.

## Entradas de menu para o integrador

Novas (instaladas pelo `big-wm-session` em `/usr/share/applications`, só
nas sessões Big WindowMaker):

| Nova entrada | Exec | Substitui (ocultar nas sessões Big WindowMaker) |
|---|---|---|
| `org.bigwindowmaker.HPDeviceManager.desktop` | `big-wm-hp toolbox` | `hplip.desktop` |
| `org.bigwindowmaker.HPScan.desktop` | `big-wm-hp scan` | `hp-uiscan.desktop` |
| `org.bigwindowmaker.Scrcpy.desktop` | `big-wm-scrcpy` | `scrcpy.desktop` **e** `scrcpy-console.desktop` |
| `org.bigwindowmaker.AirPlay.desktop` ("Receptor AirPlay") | `big-wm-airplay` | `uxplay.desktop` |

Mecanismo escolhido: IDs novos com `OnlyShowIn`, em vez de sombrear o mesmo
ID por `XDG_DATA_DIRS` (que exigiria exportar `XDG_DATA_DIRS` pelo
`big-wm-session-ctl` para o systemd/D-Bus e vazaria para outras sessões).
Os outros ambientes não mudam. Ícones no Dock/Clip que apontem para os IDs
antigos continuam abrindo o comando antigo: se o integrador quiser, uma
migração do firstrun pode trocar `hplip`/`scrcpy`/`uxplay` pelos novos (não
feita aqui: listas do firstrun são do agente "menus").

## Dependências e ISO

- `big-wm-session` `optdepends`: `python-pyqt5`, `scrcpy`, `uxplay`,
  `avahi` (já descritos no PKGBUILD).
- ISO live: o `avahi-daemon` está em `disable_systemd_live`; no live o
  Receptor AirPlay mostra "Descoberta desligada" e o Configurar rede... o
  liga (pkexec sem senha no live). Decisão do integrador: tirá-lo do
  `disable_systemd_live` do perfil wmaker para o AirPlay funcionar direto no
  live (custo: ~2 MiB e anúncios mDNS na rede).
- ufw: liberar as portas por padrão na ISO
  (`desktop-overlay/etc/ufw/user.rules`) é possível, mas abre portas sem o
  usuário usar o AirPlay; preferi o perfil + liberação sob pedido.

## Limitações e próximos passos
- O `hp-uiscan` demora a abrir (sondagem do SANE); o big-wm não mostra
  "abrindo...".
- O balão de "Conectado" usa o nome que o iPhone manda; com `-pin` ou
  `-restrict` (não usados) a lógica de estado seria a mesma.
- Na sessão X11 o vídeo continua no autovideosink (Xv do próprio Xorg,
  sem o problema do XWayland).
