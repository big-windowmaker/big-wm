# 09 — "Reiniciar o Window Maker"

## O que o item faz

`("Reiniciar o Window Maker", RESTART)` no WMRootMenu: o comando interno do
Window Maker, o mesmo do `SIGUSR1` — ele reexecuta o próprio binário (mesmo
PID) e relê toda a configuração, inclusive o `IconSize`. Atenção: `SIGHUP`
**encerra** o Window Maker (`handleExitSig`, `startup.c`), não reinicia.

Verificado na sessão real: reiniciado com `SIGUSR1`, o Window Maker passou
do binário apagado (`/usr/bin/wmaker (deleted)`) ao novo, no mesmo PID, e
readotou os dockapps (que não reiniciam).

## Por que os defeitos continuavam

| Defeito | Por que reiniciar o Window Maker não resolvia |
|---|---|
| dockapps cortados | o `IconSize = 48` estava gravado: o Window Maker relia 48 |
| Largura total | o Dock com Abas é outro processo e o erro era do código dele |
| modo escuro | o `big-wm-theme` achava o estilo "custom" e não o trocava: o Window Maker relia o estilo claro |
| Clip sobre o Dock | o reinício gravava a posição errada (dentro da tela) e a relia |

Nenhum era um estado em memória do Window Maker: o reinício funcionava, mas
recarregava a configuração errada ou não tocava no processo errado.
