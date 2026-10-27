# SPDX-License-Identifier: GPL-2.0-or-later
"""Testes do big-wm-wprefs-sync (WPrefs -> wlmaker).

Uso (da raiz do repositório): python3 -I -m unittest discover -s tests/wprefs

Fixtures:
  fixtures/global/       /etc/WindowMaker do pacote windowmaker do Big
                         WindowMaker (0.96.0.r130), os padrões globais.
  fixtures/wprefs-real/  gravados pelo próprio WPrefs 0.96.0 (Xvfb + wmaker,
                         clicando nos painéis), partindo dos padrões do Big
                         WindowMaker: foco Auto (sloppy), posicionamento
                         Smart, Fechar janela em Super+Q, aceleração do mouse
                         7,2 e duplo clique de 395 ms, barra de título da
                         janela ativa com a textura da barra de redimensionar
                         (sólida), título centralizado (valor global),
                         menu "flat", cantos ativos ligados com "foot" no
                         canto superior direito e fonte do título 15 px.
"""

import importlib.machinery
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import unittest

# Sem __pycache__ em session/bin (o run.sh trata tudo ali como script).
sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FIXTURES = os.path.join(HERE, "fixtures")
SCRIPT = os.path.join(ROOT, "session", "bin", "big-wm-wprefs-sync")


def load_module():
    loader = importlib.machinery.SourceFileLoader("wprefs_sync", SCRIPT)
    spec = importlib.util.spec_from_loader("wprefs_sync", loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    return mod


ws = load_module()
ALL_CAPS = set(ws.CAPABILITIES)


class ParserTest(unittest.TestCase):
    def test_wings_document(self):
        text = '''
        /* comentário de bloco */
        {
          // comentário de linha
          Chave = valor.sem/aspas+;
          "Com espaço" = "texto \\"citado\\"\\n\\101";
          Lista = (a, "b c", (d, e), {X = 1;});
          Vazio = ();
          Dados = <0a0B ff>;
          Repetida = 1;
          Repetida = 2;
        }'''
        v = ws.loads(text)
        self.assertEqual(v["Chave"], "valor.sem/aspas+")
        self.assertEqual(v["Com espaço"], 'texto "citado"\nA')
        self.assertEqual(v["Lista"], ["a", "b c", ["d", "e"], {"X": "1"}])
        self.assertEqual(v["Vazio"], [])
        self.assertEqual(v["Dados"], bytes([0x0a, 0x0b, 0xff]))
        self.assertEqual(v["Repetida"], "2")

    def test_wlmaker_dialect(self):
        # No wlmaker "\x" vale x; vírgula final e ";" opcional no fim.
        v = ws.loads('{ A = "x\\ny"; B = (1, 2,); C = { D = E } }', "wlmaker")
        self.assertEqual(v, {"A": "xny", "B": ["1", "2"], "C": {"D": "E"}})

    def test_errors(self):
        for bad in ('{ A = 1 B = 2; }', '(a, b', '{ A = "x; }', '/* x', '{ = 1; }',
                    '(a) b', '<zz>'):
            with self.assertRaises(ws.PlistError, msg=bad):
                ws.loads(bad)
        self.assertIsNone(ws.loads("  // nada\n"))

    def test_shipped_files_parse(self):
        for d, dialect in (("data/GNUstep/Defaults", "wings"), ("data/wlmaker", "wlmaker")):
            for name in os.listdir(os.path.join(ROOT, d)):
                with self.subTest(name=name):
                    with open(os.path.join(ROOT, d, name), encoding="utf-8") as f:
                        text = f.read()
                    if name.endswith(".plist.in"):
                        text = text.replace("@", "")
                    self.assertIsNotNone(ws.loads(text, dialect))

    def test_dumps_round_trip_and_quoting(self):
        value = {"Neg": "-0.5", "Key": "Ctrl+Alt+T", "Bare": "argb32", "Empty": "",
                 "Q": 'a "b" \\ c', "L": ["x", ["y", "z"]], "D": {}}
        text = ws.dumps(value)
        self.assertIn('Neg = "-0.5";', text)
        self.assertIn('Key = "Ctrl+Alt+T";', text)
        self.assertIn("Bare = argb32;", text)
        ws.check_wlmaker(text)
        self.assertEqual(ws.loads(text, "wlmaker"), value)
        with self.assertRaises(ws.PlistError):
            ws.dumps({"B": b"\x00"})

    def test_check_wlmaker(self):
        ws.check_wlmaker('{ A = "/* não é comentário */"; }')
        for bad in ("{ A = Ctrl+T; }", "{ /* x */ A = 1; }", "{ A = <00>; }"):
            with self.assertRaises(ws.PlistError, msg=bad):
                ws.check_wlmaker(bad)


class PlistDocTest(unittest.TestCase):
    TEXT = """// Cabeçalho
{
    // Teclado
    Keyboard = {
        Repeat = {
            Delay = 300;
            Rate = 25;
        };
    };

    KeyBindings = {
        "Alt+F4" = WindowClose;  // fechar
        "F12" = RootMenu;
    };
    Ultimo = 1
}
"""

    def test_set_existing_keeps_comments(self):
        doc = ws.PlistDoc(self.TEXT)
        doc.set(("Keyboard", "Repeat", "Delay"), "250")
        self.assertIn("Delay = 250;", doc.text)
        self.assertIn("// Teclado", doc.text)
        self.assertIn("// fechar", doc.text)
        self.assertEqual(doc.text.replace("250", "300"), self.TEXT)

    def test_add_delete_and_nested(self):
        doc = ws.PlistDoc(self.TEXT)
        doc.set(("KeyBindings", "Logo+Q"), "WindowClose")
        doc.delete(("KeyBindings", "Alt+F4"))
        doc.set(("Mouse", "AccelSpeed"), "-0.25")
        doc.set(("Theme", "Window", "TitleBar", "Justify"), "Center")
        v = ws.loads(doc.text, "wlmaker")
        self.assertEqual(v["KeyBindings"], {"F12": "RootMenu", "Logo+Q": "WindowClose"})
        self.assertEqual(v["Mouse"], {"AccelSpeed": "-0.25"})
        self.assertEqual(v["Theme"]["Window"]["TitleBar"]["Justify"], "Center")
        self.assertEqual(v["Ultimo"], "1")
        self.assertNotIn("fechar", doc.text)
        ws.check_wlmaker(doc.text)

    def test_empty_dict_and_document(self):
        doc = ws.PlistDoc("{\n  A = {};\n}\n")
        doc.set(("A", "B"), "C")
        self.assertEqual(ws.loads(doc.text, "wlmaker"), {"A": {"B": "C"}})
        doc = ws.PlistDoc("")
        doc.set(("X",), ["1", "2"])
        self.assertEqual(ws.loads(doc.text, "wlmaker"), {"X": ["1", "2"]})


class ConversionTest(unittest.TestCase):
    def test_colors(self):
        self.assertEqual(ws.color_value("#fff"), "argb32:ffffffff")
        self.assertEqual(ws.color_value("#20262a"), "argb32:ff20262a")
        self.assertEqual(ws.color_value("rgb:50/5a/5e"), "argb32:ff505a5e")
        self.assertEqual(ws.color_value("rgb:f/0/80"), "argb32:ffff0080")
        self.assertEqual(ws.color_value("#ffff00000000"), "argb32:ffff0000")
        self.assertEqual(ws.color_value("black"), "argb32:ff000000")
        self.assertEqual(ws.color_value("Dark Slate Gray"), "argb32:ff2f4f4f")
        self.assertEqual(ws.color_value("gray50"), "argb32:ff808080")
        self.assertIsNone(ws.color_value("not-a-color"))
        self.assertIsNone(ws.color_value(None))

    def test_textures(self):
        f = ws.texture_fill
        self.assertEqual(f(["solid", "#c2c0c5"]), {"Color": "argb32:ffc2c0c5", "Type": "SOLID"})
        self.assertEqual(f(["hgradient", "rgb:50/5a/5e", "rgb:20/2a/2e"]),
                         {"From": "argb32:ff505a5e", "To": "argb32:ff202a2e", "Type": "HGRADIENT"})
        self.assertEqual(f(["vgradient", "white", "black"])["Type"], "VGRADIENT")
        self.assertEqual(f(["dgradient", "white", "black"])["Type"], "DGRADIENT")
        self.assertEqual(f(["mvgradient", "red", "green", "blue"]),
                         {"From": "argb32:ffff0000", "To": "argb32:ff0000ff", "Type": "VGRADIENT"})
        self.assertEqual(f(["igradient", "#000000", "#000000", "1", "#ffffff", "#ffffff", "1"]),
                         {"Color": "argb32:ff808080", "Type": "SOLID"})
        self.assertEqual(f(["thgradient", "x.png", "128", "#ff0000", "#0000ff"])["Type"], "HGRADIENT")
        for kind in ("tpixmap", "spixmap", "cpixmap", "mpixmap", "fpixmap"):
            self.assertEqual(f([kind, "x.png", "#505075"]),
                             {"Color": "argb32:ff505075", "Type": "SOLID"})
        self.assertEqual(f("#123456"), {"Color": "argb32:ff123456", "Type": "SOLID"})
        self.assertIsNone(f(["none"]))
        self.assertIsNone(f(["hgradient", "bad", "worse"]))
        self.assertEqual(ws.texture_color(["hgradient", "#000000", "#ffffff"]), "argb32:ff808080")

    def test_fonts(self):
        p = ws.parse_font
        self.assertEqual(p("Noto Sans,sans serif:bold:pixelsize=12"),
                         {"Face": "Noto Sans", "Weight": "Bold", "Size": "12"})
        # Como o WPrefs 0.96 grava (painel de fontes).
        self.assertEqual(p("Noto Sans:slant=0:weight=200:width=100:pixelsize=15"),
                         {"Face": "Noto Sans", "Weight": "Bold", "Size": "15"})
        self.assertEqual(p("DejaVu Serif:weight=80:size=12"),
                         {"Face": "DejaVu Serif", "Weight": "Normal", "Size": "16"})
        self.assertEqual(p("Sans-10"), {"Face": "Sans", "Weight": "Normal", "Size": "13"})
        self.assertEqual(p("Sans", 9), {"Face": "Sans", "Weight": "Normal", "Size": "9"})
        self.assertEqual(p("-*-helvetica-bold-r-normal-*-14-*-*-*-*-*-*-*"),
                         {"Face": "Helvetica", "Weight": "Bold", "Size": "14"})
        self.assertIsNone(p(""))

    def test_keys(self):
        k = ws.parse_wm_key
        self.assertEqual(ws.combo_string(*k("Mod1+F10")), "Alt+F10")
        self.assertEqual(ws.combo_string(*k("Control+Mod1+Left")), "Ctrl+Alt+Left")
        self.assertEqual(ws.combo_string(*k("Mod1+Shift+Tab")), "Shift+Alt+ISO_Left_Tab")
        self.assertEqual(ws.combo_string(*k("Mod4+Q")), "Logo+Q")
        self.assertEqual(ws.combo_string(*k("Super+Hyper+space")), "Logo+space")
        self.assertIsNone(k("None"))
        self.assertIsNone(k(""))
        self.assertIsNone(k("Lock+a"))
        self.assertIsNone(k("Mod1+a+b"))
        self.assertIsNone(k("Mod1+"))
        self.assertIsNone(k("Mod1+NotAKeysym_xyz"))
        self.assertEqual(ws.canonical_combo("Shift+Alt+ISO_Left_Tab"),
                         ws.canonical_combo("Alt+Shift+iso_left_tab"))

    def test_accel(self):
        self.assertAlmostEqual(ws.accel_to_speed(2), 0)
        self.assertAlmostEqual(ws.accel_to_speed(10), 1)
        self.assertAlmostEqual(ws.accel_to_speed(0.4), -1)
        self.assertEqual(ws.accel_to_speed(50), 1)
        self.assertEqual(ws._xset("#!/bin/sh\nxset m 72/10 4\n# xset r rate 1 1\nxset r rate 250 40\n"),
                         ((250, 40), 7.2))

    def test_menu(self):
        ws.Wlmaker._caps = ALL_CAPS
        wm = ["Meu menu",
              ["Terminal", "SHORTCUT", "Control+Mod1+t", "EXEC", "xterm"],
              ["Pergunta", "EXEC", "%A(Executar)"],
              ["Lupa", "EXEC", "wmagnify"],
              ["Monitores", "EXEC", "arandr"],
              ["Bloquear", "SHORTCUT", "Mod4+l", "EXEC", "big-wm-lock"],
              ["Apps", "OPEN_PLMENU", "|| big-wm-appmenu --dockapps"],
              ["Outro", "OPEN_PLMENU", "|| meu-gerador"],
              ["Estilos", "OPEN_MENU", "-noext /usr/share/WindowMaker/Styles WITH setstyle"],
              ["Áreas", ["Áreas", "WORKSPACE_MENU"], ["Esconder", "HIDE_OTHERS"]],
              ["Só ocultas", ["X", "ARRANGE_ICONS"]],
              ["Reiniciar", "RESTART"],
              ["Outro WM", "RESTART", "twm"],
              ["Sair", "EXIT"],
              "lixo"]
        conv = ws.MenuConverter()
        out = conv.menu(wm)
        self.assertEqual(out[0], "Meu menu")
        labels = [x[0] for x in out[1:]]
        self.assertEqual(labels, ["Terminal", "Lupa", "Monitores", "Bloquear", "Apps", "Outro",
                                  "Estilos", "Áreas", "Reiniciar", "Sair"])
        d = {x[0]: x[1:] for x in out[1:]}
        self.assertEqual(d["Terminal"], ["ShellExecute", "xterm"])
        self.assertEqual(d["Lupa"], ["OutputMagnify"])
        self.assertEqual(d["Monitores"], ["ShellExecute", "wdisplays"])
        self.assertEqual(d["Bloquear"], ["LockScreen"])
        self.assertEqual(d["Apps"], ["GeneratePlistMenu", "big-wm-appmenu --wlmaker --dockapps"])
        self.assertEqual(d["Outro"][0], "GeneratePlistMenu")
        self.assertIn("--open-plmenu", d["Outro"][1])
        self.assertIn("--open-menu", d["Estilos"][1])
        self.assertEqual(d["Áreas"][0][0], "Áreas")
        self.assertEqual(d["Reiniciar"], ["ShellExecute", "big-wm-wprefs-sync"])
        self.assertEqual(d["Sair"], ["Quit"])
        self.assertEqual(conv.shortcuts, {"Ctrl+Alt+t": ["ShellExecute", "xterm"],
                                          "Logo+l": "LockScreen"})
        ws.check_wlmaker(ws.dumps(out))

    def test_old_menu_format(self):
        text = '''#include "nao-existe.menu"
"Raiz" MENU
  "XTerm" EXEC xterm -bg black
  "Sub" MENU
    "Info" SHORTCUT "Mod1+i" EXEC xmessage oi
  "Sub" END
  "Sair" EXIT
"Raiz" END
'''
        menu = ws.parse_old_menu(text)
        self.assertEqual(menu, ["Raiz", ["XTerm", "EXEC", "xterm -bg black"],
                                ["Sub", ["Info", "SHORTCUT", "Mod1+i", "EXEC", "xmessage oi"]],
                                ["Sair", "EXIT"]])

    def test_directory_menu(self):
        with tempfile.TemporaryDirectory() as d:
            os.mkdir(os.path.join(d, "Azul.themed"))
            open(os.path.join(d, "Verde.style"), "w").close()
            open(os.path.join(d, "praia.jpg"), "w").close()
            items = ws.directory_menu([d], "setstyle", True)
            self.assertEqual([i[0] for i in items], ["Azul", "praia", "Verde"])
            self.assertTrue(items[0][2].startswith("setstyle "))
            self.assertTrue(items[0][2].endswith("&& big-wm-wprefs-sync"))
            items = ws.directory_menu([d], "wmsetbg -u -s", True)
            self.assertTrue(all(i[2].startswith("big-wm-wallpaper ") for i in items))

    def test_capabilities(self):
        ws.Wlmaker._caps = set()
        self.assertFalse(ws.Wlmaker.has_action(["WorkspaceSwitchTo", "1"]))
        self.assertTrue(ws.Wlmaker.has_action("WindowClose"))
        self.assertTrue(ws.Wlmaker.has_action(["ShellExecute", "x"]))
        ws.Wlmaker._caps = ALL_CAPS
        self.assertTrue(ws.Wlmaker.has_action(["WorkspaceSwitchTo", "1"]))


class SessionTest(unittest.TestCase):
    def test_reload_targets(self):
        # O próprio python não é um wlmaker que recarrega.
        self.assertFalse(ws.wlmaker_can_reload(os.getpid()))
        self.assertFalse(ws.wlmaker_can_reload(2 ** 22 + 7))
        saved = {k: os.environ.pop(k, None) for k in ("WAYLAND_DISPLAY", "XDG_RUNTIME_DIR")}
        try:
            self.assertIsNone(ws.session_compositor())
            self.assertFalse(ws.reload_wlmaker())
            with tempfile.TemporaryDirectory() as d:
                os.environ.update(WAYLAND_DISPLAY="wayland-teste", XDG_RUNTIME_DIR=d)
                self.assertIsNone(ws.session_compositor())
                # Um lock (flock) segurado por este processo é achado em /proc/locks.
                import fcntl
                with open(os.path.join(d, "wayland-teste.lock"), "w") as lock:
                    fcntl.flock(lock, fcntl.LOCK_EX)
                    found = ws.session_compositor()
                    self.assertIn(found, (os.getpid(), None))  # None: /proc/locks em outro pid ns
                    self.assertFalse(ws.reload_wlmaker())
        finally:
            for k, v in saved.items():
                os.environ.pop(k, None)
                if v is not None:
                    os.environ[k] = v


class SyncTest(unittest.TestCase):
    """Sincronização completa numa conta temporária."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="wprefs-sync-")
        self.home = os.path.join(self.tmp, "home")
        self.defaults = os.path.join(self.home, "GNUstep", "Defaults")
        self.cfgdir = os.path.join(self.home, ".config", "wlmaker")
        os.makedirs(self.cfgdir)
        shutil.copytree(os.path.join(ROOT, "data", "GNUstep", "Defaults"), self.defaults)
        shutil.copy(os.path.join(self.defaults, "WMRootMenu.pt_BR"),
                    os.path.join(self.defaults, "WMRootMenu"))
        with open(os.path.join(ROOT, "data", "wlmaker", "Config.plist.in"), encoding="utf-8") as f:
            text = f.read()
        for k, v in (("LAYOUT", "br"), ("MODEL", "pc105"), ("VARIANT", ""), ("OPTIONS", "")):
            text = text.replace(f"@XKB_{k}@", v)
        self.write(".config/wlmaker/Config.plist", text)
        shutil.copy(os.path.join(ROOT, "data", "wlmaker", "RootMenu.plist.pt_BR"),
                    os.path.join(self.cfgdir, "RootMenu.plist"))
        shutil.copy(os.path.join(ROOT, "data", "wlmaker", "State.plist"),
                    os.path.join(self.cfgdir, "State.plist"))
        self.env = {"HOME": self.home, "XDG_CONFIG_HOME": os.path.join(self.home, ".config"),
                    "LANG": "pt_BR.UTF-8", "BIGWM_WM_GLOBAL": os.path.join(FIXTURES, "global"),
                    "BIGWM_DATADIR": os.path.join(ROOT, "data"),
                    "BIGWM_WMSTYLES": os.path.join(ROOT, "themes", "WindowMaker", "Styles")}
        ws.Wlmaker._caps = ALL_CAPS

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def path(self, rel):
        return os.path.join(self.home, rel)

    def write(self, rel, text):
        os.makedirs(os.path.dirname(self.path(rel)), exist_ok=True)
        with open(self.path(rel), "w", encoding="utf-8") as f:
            f.write(text)

    def read(self, rel):
        with open(self.path(rel), encoding="utf-8") as f:
            return f.read()

    def wl(self, name):
        text = self.read(f".config/wlmaker/{name}")
        ws.check_wlmaker(text)
        return ws.loads(text, "wlmaker")

    def edit_wm(self, **changes):
        p = os.path.join(self.defaults, "WindowMaker")
        doc = ws.PlistDoc(open(p, encoding="utf-8").read())
        for k, v in changes.items():
            if v is None:
                doc.delete((k,))
            else:
                doc.set((k,), v)
        with open(p, "w", encoding="utf-8") as f:
            f.write(doc.text)

    def sync(self, **kw):
        return ws.Syncer(ws.Paths(self.env), **kw).run()

    def test_first_run_with_defaults_changes_only_keys(self):
        config_before = self.read(".config/wlmaker/Config.plist")
        menu_before = self.read(".config/wlmaker/RootMenu.plist")
        changed = self.sync()
        self.assertEqual(changed, {"Config.plist"})
        self.assertFalse(os.path.exists(self.path(".config/wlmaker/Theme.plist")))
        self.assertEqual(self.read(".config/wlmaker/RootMenu.plist"), menu_before)
        kb = self.wl("Config.plist")["KeyBindings"]
        # Teclas do Window Maker que faltavam no wlmaker.
        self.assertEqual(kb["Alt+M"], "WindowMinimize")
        self.assertEqual(kb["Alt+1"], ["WorkspaceSwitchTo", "1"])
        self.assertEqual(kb["Alt+0"], ["WorkspaceSwitchTo", "10"])
        # As que já existiam ficam como estavam, com a mesma grafia.
        self.assertEqual(kb["Shift+Alt+ISO_Left_Tab"], "TaskPrevious")
        self.assertEqual(kb["Ctrl+Alt+T"], ["ShellExecute", "big-wm-open terminal"])
        self.assertNotIn("Alt+Shift+ISO_Left_Tab", kb)
        new = self.read(".config/wlmaker/Config.plist")
        self.assertIn("// Mesmos atalhos da sessão X11", new)
        self.assertEqual(len(new.splitlines()) - len(config_before.splitlines()), 11)
        # Nada mais a fazer.
        self.assertEqual(self.sync(), set())
        state = json.load(open(self.path(".config/wlmaker/.wprefs-sync.json")))
        self.assertEqual(state["version"], 1)

    def test_without_capabilities_no_unknown_actions(self):
        ws.Wlmaker._caps = set()
        self.sync()
        text = self.read(".config/wlmaker/Config.plist")
        self.assertNotIn("WorkspaceSwitchTo", text)
        self.assertIn('"Alt+M" = WindowMinimize;', text)

    def test_no_wayland_config_yet(self):
        os.unlink(os.path.join(self.cfgdir, "Config.plist"))
        self.assertEqual(self.sync(), set())
        self.assertFalse(os.path.exists(os.path.join(self.cfgdir, ".wprefs-sync.json")))

    def test_theme_change_and_restore(self):
        self.sync()
        self.edit_wm(FTitleBack=["vgradient", "#c02020", "#400000"], FTitleColor="#ffe060",
                     TitleJustify="center", WindowTitleFont="DejaVu Serif:bold:pixelsize=14",
                     MenuStyle="flat", IconBack=["solid", "#336699"])
        changed = self.sync()
        self.assertIn("Theme.plist", changed)
        theme = self.wl("Theme.plist")
        tb = theme["Window"]["TitleBar"]
        self.assertEqual(tb["FocussedFill"], {"From": "argb32:ffc02020", "To": "argb32:ff400000",
                                              "Type": "VGRADIENT"})
        self.assertEqual(tb["FocussedTextColor"], "argb32:ffffe060")
        self.assertEqual(tb["Justify"], "Center")
        self.assertEqual(tb["Font"], {"Face": "DejaVu Serif", "Weight": "Bold", "Size": "14"})
        self.assertEqual(theme["Menu"]["Item"]["BezelWidth"], "0")
        self.assertEqual(theme["Tile"]["Fill"], {"Color": "argb32:ff336699", "Type": "SOLID"})
        # Não mexidos: os valores do tema padrão do Big WindowMaker.
        self.assertEqual(tb["BlurredFill"]["From"], "argb32:ffc2c0c5")
        self.assertEqual(theme["TaskList"]["Font"]["Size"], "16")
        cfg = self.wl("Config.plist")
        self.assertEqual(cfg["ThemeFile"], os.path.join(self.cfgdir, "Theme.plist"))
        self.assertIn("Gerado pelo big-wm-wprefs-sync", self.read(".config/wlmaker/Theme.plist"))
        # Uma edição à mão fica enquanto a aparência não muda...
        doc = ws.PlistDoc(self.read(".config/wlmaker/Theme.plist"))
        doc.set(("Window", "TitleBar", "Height"), "30")
        self.write(".config/wlmaker/Theme.plist", doc.text)
        self.edit_wm(CloseKey="Mod4+Q")
        self.sync()
        self.assertEqual(self.wl("Theme.plist")["Window"]["TitleBar"]["Height"], "30")
        # ...e vai para um .bak quando o tema é refeito.
        self.edit_wm(FTitleColor="#ffffff")
        self.sync()
        tb = self.wl("Theme.plist")["Window"]["TitleBar"]
        self.assertEqual(tb["Height"], "22")
        self.assertEqual(tb["FocussedTextColor"], "argb32:ffffffff")
        self.assertEqual(len([n for n in os.listdir(self.cfgdir) if n.startswith("Theme.plist.bak-")]), 1)
        # Tudo de volta ao padrão: sem tema próprio, o ThemeFile volta ao padrão.
        self.edit_wm(FTitleBack=["hgradient", "rgb:50/5a/5e", "rgb:20/2a/2e"], TitleJustify="left",
                     WindowTitleFont="Noto Sans,sans serif:bold:pixelsize=12", MenuStyle="normal",
                     IconBack=["dgradient", "rgb:a6/a6/b6", "rgb:51/55/61"])
        self.assertIn("Theme.plist", self.sync())
        self.assertFalse(os.path.exists(self.path(".config/wlmaker/Theme.plist")))
        self.assertEqual(self.wl("Config.plist")["ThemeFile"],
                         os.path.join(ROOT, "data", "wlmaker", "Theme.plist"))

    def set_mode(self, mode):
        """Como o big-wm-theme: guarda o modo, troca o estilo e o ThemeFile."""
        self.write(".config/big-wm/theme", mode + "\n")
        name = "BigWindowMaker-Dark.style" if mode == "dark" else "BigWindowMaker.style"
        style = ws.load_file(os.path.join(ROOT, "themes", "WindowMaker", "Styles", name))
        self.edit_wm(**style)
        doc = ws.PlistDoc(self.read(".config/wlmaker/Config.plist"))
        theme = "Theme-Dark.plist" if mode == "dark" else "Theme.plist"
        if doc.get(("ThemeFile",)).startswith("/usr/share/big-wm/"):
            doc.set(("ThemeFile",), "/usr/share/big-wm/wlmaker/" + theme)
        self.write(".config/wlmaker/Config.plist", doc.text)

    def test_dark_mode(self):
        self.sync()
        # Modo escuro do big-wm-theme: o tema escuro curado, sem tema gerado.
        self.set_mode("dark")
        self.sync()
        self.assertFalse(os.path.exists(self.path(".config/wlmaker/Theme.plist")))
        self.assertEqual(self.wl("Config.plist")["ThemeFile"], "/usr/share/big-wm/wlmaker/Theme-Dark.plist")
        # Uma mudança no WPrefs no modo escuro: gerado a partir do tema escuro.
        self.edit_wm(TitleJustify="right")
        self.sync()
        dark = ws.load_file(os.path.join(ROOT, "data", "wlmaker", "Theme-Dark.plist"), "wlmaker")
        theme = self.wl("Theme.plist")
        self.assertEqual(theme["Window"]["TitleBar"]["Justify"], "Right")
        self.assertEqual(theme["Window"]["TitleBar"]["FocussedFill"],
                         dark["Window"]["TitleBar"]["FocussedFill"])
        self.assertEqual(theme["TaskList"], dark["TaskList"])
        self.assertEqual(self.wl("Config.plist")["ThemeFile"], os.path.join(self.cfgdir, "Theme.plist"))
        # De volta ao claro: o tema gerado passa a partir do tema claro.
        self.set_mode("light")
        self.sync()
        light = ws.load_file(os.path.join(ROOT, "data", "wlmaker", "Theme.plist"), "wlmaker")
        theme = self.wl("Theme.plist")
        self.assertEqual(theme["Window"]["TitleBar"]["Justify"], "Right")
        self.assertEqual(theme["Window"]["TitleBar"]["FocussedFill"],
                         light["Window"]["TitleBar"]["FocussedFill"])
        self.assertEqual(theme["TaskList"], light["TaskList"])
        # Sem nada do WPrefs: volta ao tema padrão do modo.
        self.edit_wm(TitleJustify="left")
        self.sync()
        self.assertFalse(os.path.exists(self.path(".config/wlmaker/Theme.plist")))
        self.assertEqual(self.wl("Config.plist")["ThemeFile"],
                         os.path.join(ROOT, "data", "wlmaker", "Theme.plist"))

    def test_user_theme_untouched(self):
        self.sync()
        self.write(".config/wlmaker/Theme.plist", "// meu tema\n{ Name = Meu; }\n")
        self.edit_wm(FTitleBack=["solid", "#ff0000"])
        self.sync()
        self.assertEqual(self.read(".config/wlmaker/Theme.plist"), "// meu tema\n{ Name = Meu; }\n")
        self.assertEqual(self.wl("Config.plist")["ThemeFile"], "/usr/share/big-wm/wlmaker/Theme.plist")

    def test_keybinding_change_and_user_bindings(self):
        self.sync()
        doc = ws.PlistDoc(self.read(".config/wlmaker/Config.plist"))
        doc.set(("KeyBindings", "Logo+F5"), ["ShellExecute", "meu-programa"])
        self.write(".config/wlmaker/Config.plist", doc.text)
        self.edit_wm(CloseKey="Mod4+Q", MaximizeKey="Mod1+F10", ShadeKey="Mod4+s",
                     MiniaturizeKey="None")
        self.sync()
        kb = self.wl("Config.plist")["KeyBindings"]
        self.assertNotIn("Alt+F4", kb)
        self.assertNotIn("Alt+M", kb)
        self.assertEqual(kb["Logo+Q"], "WindowClose")
        self.assertEqual(kb["Alt+F10"], "WindowToggleMaximized")
        self.assertEqual(kb["Logo+s"], "WindowToggleShaded")
        self.assertEqual(kb["Logo+F5"], ["ShellExecute", "meu-programa"])
        # O atalho volta ao padrão: Alt+F4 de novo, Super+Q sai.
        self.edit_wm(CloseKey="Mod1+F4")
        self.sync()
        kb = self.wl("Config.plist")["KeyBindings"]
        self.assertEqual(kb["Alt+F4"], "WindowClose")
        self.assertNotIn("Logo+Q", kb)

    def test_real_wprefs_output(self):
        self.sync()
        real = os.path.join(FIXTURES, "wprefs-real")
        shutil.copy(os.path.join(real, "WindowMaker"), os.path.join(self.defaults, "WindowMaker"))
        self.write("GNUstep/Library/WindowMaker/autostart",
                   open(os.path.join(real, "autostart"), encoding="utf-8").read())
        changed = self.sync()
        self.assertEqual(changed, {"Config.plist", "Theme.plist"})
        cfg = self.wl("Config.plist")
        self.assertEqual(cfg["FocusMode"], "Sloppy")
        self.assertEqual(cfg["WindowPlacement"], "Smart")
        self.assertEqual(cfg["KeyBindings"]["Logo+Q"], "WindowClose")
        self.assertNotIn("Alt+F4", cfg["KeyBindings"])
        self.assertEqual(cfg["Mouse"], {"AccelSpeed": "0.80", "AccelProfile": "Adaptive",
                                        "DoubleClickTime": "395"})
        hc = cfg["HotCorner"]
        self.assertEqual(hc["TopLeftEnter"], ["ShellExecute", "big-wm-open apps"])
        self.assertEqual(hc["TopRightEnter"], ["ShellExecute", "foot"])
        self.assertEqual(hc["BottomLeftEnter"], "LockScreen")
        self.assertEqual(hc["BottomRightEnter"], "None")
        self.assertEqual(hc["TriggerDelay"], "250")
        tb = self.wl("Theme.plist")["Window"]["TitleBar"]
        self.assertEqual(tb["FocussedFill"], {"Color": "argb32:ffc2c0c5", "Type": "SOLID"})
        self.assertEqual(tb["Justify"], "Center")
        self.assertEqual(tb["Font"], {"Face": "Noto Sans", "Weight": "Bold", "Size": "15"})
        self.assertEqual(self.wl("Theme.plist")["Menu"]["Item"]["BezelWidth"], "0")
        self.assertEqual(self.sync(), set())

    def test_hot_corners_without_command_support(self):
        self.sync()
        ws.Wlmaker._caps = ALL_CAPS - {"corner-commands"}
        self.edit_wm(HotCorners="YES", HotCornerActions=["xterm", "None", "big-wm-lock", "None"])
        self.sync()
        hc = self.wl("Config.plist")["HotCorner"]
        self.assertEqual(hc["TopLeftEnter"], "None")
        self.assertEqual(hc["BottomLeftEnter"], "LockScreen")

    def test_menu_edit_backup_and_restore(self):
        self.sync()
        menu = ws.loads(open(os.path.join(self.defaults, "WMRootMenu"), encoding="utf-8").read())
        menu.insert(2, ["Meu editor", "SHORTCUT", "Mod4+F2", "EXEC", "featherpad"])
        with open(os.path.join(self.defaults, "WMRootMenu"), "w", encoding="utf-8") as f:
            f.write(ws.dumps(menu))
        # O menu Wayland foi editado à mão: vai para um .bak antes de ser trocado.
        self.write(".config/wlmaker/RootMenu.plist",
                   self.read(".config/wlmaker/RootMenu.plist").replace("Navegador", "Firefox"))
        self.assertIn("RootMenu.plist", self.sync())
        rm = self.wl("RootMenu.plist")
        self.assertEqual(rm[2], ["Meu editor", "ShellExecute", "featherpad"])
        self.assertEqual(len([n for n in os.listdir(self.cfgdir) if n.startswith("RootMenu.plist.bak-")]), 1)
        self.assertEqual(self.wl("Config.plist")["KeyBindings"]["Logo+F2"],
                         ["ShellExecute", "featherpad"])
        # De volta ao menu padrão: o menu Wayland padrão (curado), não a conversão.
        shutil.copy(os.path.join(ROOT, "data", "GNUstep", "Defaults", "WMRootMenu.pt_BR"),
                    os.path.join(self.defaults, "WMRootMenu"))
        self.sync()
        with open(os.path.join(ROOT, "data", "wlmaker", "RootMenu.plist.pt_BR"), encoding="utf-8") as f:
            self.assertEqual(self.read(".config/wlmaker/RootMenu.plist"), f.read())
        self.assertNotIn("Logo+F2", self.wl("Config.plist")["KeyBindings"])

    def test_wallpaper_workspaces_dock(self):
        self.sync()
        self.edit_wm(WorkspaceBack=["tpixmap", "/usr/share/backgrounds/x.png", "black"],
                     DisableDock="YES")
        state = ws.loads(open(os.path.join(self.defaults, "WMState"), encoding="utf-8").read())
        state["Workspaces"][0]["Name"] = "Trabalho"
        with open(os.path.join(self.defaults, "WMState"), "w", encoding="utf-8") as f:
            f.write(ws.dumps(state))
        changed = self.sync()
        self.assertIn("wallpaper", changed)
        self.assertIn("State.plist", changed)
        self.assertEqual(self.read(".config/big-wm/wallpaper"), "/usr/share/backgrounds/x.png\ntile\n")
        self.assertEqual(self.wl("State.plist")["Workspaces"][0], {"Name": "Trabalho"})
        self.assertIn("Dock", self.wl("State.plist"))
        dock = self.read(".config/autostart/big-wm-wayland-dock.desktop")
        self.assertIn("Hidden=true", dock)
        self.assertEqual(self.wl("Theme.plist")["BackgroundColor"], "argb32:ff000000")
        # Cor sólida.
        self.edit_wm(WorkspaceBack=["solid", "#204060"], DisableDock="NO")
        self.sync()
        self.assertEqual(self.read(".config/big-wm/wallpaper"), "color:#204060\n")
        self.assertFalse(os.path.exists(self.path(".config/autostart/big-wm-wayland-dock.desktop")))
        # O padrão de novo: segue o papel de parede padrão do sistema.
        self.edit_wm(WorkspaceBack=["spixmap", "/usr/share/backgrounds/big-wm/default.jpg", "#505075"])
        self.sync()
        self.assertFalse(os.path.exists(self.path(".config/big-wm/wallpaper")))

    def test_autostart_of_others_untouched(self):
        self.sync()
        self.write(".config/autostart/big-wm-wayland-dock.desktop",
                   "[Desktop Entry]\nHidden=true\nX-Big-Tabbed-Dock-Managed=true\n")
        self.edit_wm(DisableDock="YES")
        self.sync()
        self.edit_wm(DisableDock="NO")
        self.sync()
        self.assertIn("X-Big-Tabbed-Dock-Managed", self.read(".config/autostart/big-wm-wayland-dock.desktop"))

    def test_keyboard_and_mouse_from_autostart(self):
        self.sync()
        self.write("GNUstep/Library/WindowMaker/autostart", "#!/bin/sh\nxset r rate 200 50\nxset m 1/1 4\n")
        self.sync()
        cfg = self.wl("Config.plist")
        self.assertEqual(cfg["Keyboard"]["Repeat"], {"Delay": "200", "Rate": "50"})
        self.assertEqual(cfg["Mouse"]["AccelProfile"], "Flat")
        self.assertEqual(cfg["Mouse"]["AccelSpeed"], "-0.43")
        # Sem as linhas, volta o padrão do Config.plist do Big WindowMaker.
        os.unlink(self.path("GNUstep/Library/WindowMaker/autostart"))
        self.sync()
        self.assertEqual(self.wl("Config.plist")["Keyboard"]["Repeat"], {"Delay": "300", "Rate": "25"})

    def test_invalid_user_file_is_not_fatal(self):
        self.sync()
        with open(os.path.join(self.defaults, "WindowMaker"), "w") as f:
            f.write("{ quebrado = ")
        before = self.read(".config/wlmaker/Config.plist")
        self.sync()
        # Sem o arquivo do usuário vale o global; nada fica inválido.
        self.wl("Config.plist")
        self.assertTrue(before)

    def test_dry_run_writes_nothing(self):
        before = self.read(".config/wlmaker/Config.plist")
        import io
        out = io.StringIO()
        ws.Syncer(ws.Paths(self.env), dry_run=True, out=out).run()
        self.assertIn('"KeyBindings", "alt+m"', out.getvalue())
        self.assertEqual(self.read(".config/wlmaker/Config.plist"), before)
        self.assertFalse(os.path.exists(os.path.join(self.cfgdir, ".wprefs-sync.json")))


if __name__ == "__main__":
    unittest.main()
