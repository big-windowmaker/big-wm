# 03 — Wayland

- Cópias de aplicativos Wayland chegam íntegras ao compositor e aos
  consumidores (gedit, kate e Firefox reais: bytes iguais, HTML do Firefox
  preservado).
- O vigia do histórico usa data-control (não depende de foco) e `--type
  text` escolhe `text/plain;charset=utf-8`: o histórico guarda os bytes
  exatos.
- O GTK3 oferece também `text/plain` sem charset, servido em Latin-1 com
  escapes (`\u201c`, `\U0001f642`): é o formato antigo, correto para quem o
  pede; o `wl-paste` sem `--type` o escolhe. Os aplicativos pedem UTF-8.
- PRIMARY e CLIPBOARD seguem separados: o histórico e o wmcliphist só usam
  CLIPBOARD.
- Na sessão real, o histórico mostra pares de entradas no mesmo instante com
  o mesmo texto em CRLF e em LF, vindos do vigia Wayland: o aplicativo
  publicou a seleção duas vezes. O que fica na área de transferência é a
  última versão (não houve troca de conteúdo).
