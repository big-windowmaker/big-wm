# 06 — Histórico (wmcliphist e big-wm-clipboard)

**Causa-raiz.** No `process_item` do wmcliphist, com `auto_take_up`, toda
cópia nova chamava `gtk_selection_owner_set`: o wmcliphist tomava a
seleção do aplicativo e a servia ele mesmo, só como texto (lido por
`gtk_clipboard_request_text`, que normaliza as quebras de linha) — o
aplicativo perdia os formatos ricos (arquivos, HTML, imagens) e o que se
colava não era o que fora copiado. O comentário do patch dizia o contrário
("só quando o dono vai embora").

**Correção** (`big-dockapps/wmcliphist-keep-the-owner.patch`, big-dockapps
11):
- uma cópia nova nunca é tomada; o último item é republicado só quando o
  dono some (já no `owner_changed`) e, no X11, no início da sessão sem dono;
- o texto é lido de `UTF8_STRING` como veio (sem normalização); sem UTF-8
  válido, a conversão de antes.

`big-wm-clipboard`: sem mudança. Ele só lê, respeita `CLIPBOARD_STATE=sensitive`
e `x-kde-passwordManagerHint`, e o limite `max_kib` vale só para o histórico
(a seleção não é tocada). Nenhum texto vai para logs.
