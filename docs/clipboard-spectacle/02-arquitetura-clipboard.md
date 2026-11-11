# 02 — Arquitetura da área de transferência

- **A — aplicativo de origem**: oferece os formatos (no Wayland por
  `wl_data_source`; no X11 é o dono da seleção CLIPBOARD).
- **B — compositor/servidor X**: guarda a seleção e transfere os bytes por
  descritores (Wayland) ou pela conversão da seleção (X11); a ponte do
  wlroots liga os dois lados.
- **C — histórico**: `big-wm-clipboard --store` (só lê, grava no SQLite,
  nunca escreve na seleção); wmcliphist (lê; republica o último item só
  quando o dono some, e o item que o usuário escolhe).

O defeito estava em **C** agindo como **A**: o wmcliphist tomava a seleção
de toda cópia feita num aplicativo X11 (ver 06).
