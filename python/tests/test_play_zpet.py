"""P201 (14): příkaz `zpet` vrátí poslední akci — log bez ní a stav jako před ní."""
import os
import subprocess
import sys
import time

KOREN = os.path.join(os.path.dirname(__file__), "..", "..")
sys.path.insert(0, KOREN)
import bb_play_session_20261012 as ps  # noqa: E402


def test_odstrihni_posledni_akci():
    log = ["kouc HOME", "los", "volba prijimat", "actions HOME", "do 3", "actions HOME", "push 5 5",
           "follow 1", "do 7", "odp tym", "odp 2", "actions HOME"]
    nove, pryc = ps.odstrihni_posledni_akci(log)
    assert nove == log[:6]                 # `actions` před vracenou akcí zůstává
    assert pryc == log[6:]
    assert ps.odstrihni_posledni_akci(["kouc HOME", "los", "volba prijimat"]) is None
    assert ps.odstrihni_posledni_akci(["kouc HOME", "los", "do 1", "vykop AWAY"]) is None   # přes výkop ne


def _cmd(d, *prikaz):
    env = dict(os.environ, BB_PLAY_DIR=d, BB_PLAY_LIMIT="60")
    return subprocess.run([sys.executable, os.path.join(KOREN, "bb_play_cmd.py"), *prikaz], env=env,
                          capture_output=True, text=True, timeout=90).stdout


def _po_vykopu(d):
    # Od P200 (14.10.2026) se výkop ptá kouče přijímajících na Kick-Off Return (kdo / kam). Tyhle testy
    # zkoušejí `zpet` a odmítnutý příkaz, ne výkop ⇒ odpovědět „0“ (nikdo / zůstat). Dřív tu chyběla
    # a testy padaly od 14.10. (`check-py` není v `make check`).
    for _ in range(3):
        if "KICK-OFF RETURN" not in _cmd(d, "board"):
            return
        _cmd(d, "odp", "0")


def _deska(text):
    i = text.find("--- hráči")
    return text[i:]


def test_zpet_vrati_stav(tmp_path):
    d = str(tmp_path)
    server = subprocess.Popen([sys.executable, os.path.join(KOREN, "bb_play_session_20261012.py")],
                              env=dict(os.environ, BB_PLAY_DIR=d), stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.5)
        _cmd(d, "kouc", "HOME")
        out = _cmd(d, "los")
        if "volba" in out:
            _cmd(d, "volba", "prijimat")
        _po_vykopu(d)
        pred = _deska(_cmd(d, "board"))
        akce = _cmd(d, "actions", "HOME")
        assert "[0]" in akce
        # pohyb prvního hráče, který jde (index s MOVE)
        idx = next(l.split("]")[0].strip(" [") for l in akce.splitlines() if "MOVE" in l)
        po = _cmd(d, "do", idx)
        while "ČEKÁ" in po:               # otázka kouče (přehoz) — odpovědět „ne“
            po = _cmd(d, "odp", "ne")
        assert _deska(_cmd(d, "board")) != pred, "pozitivní kontrola: akce musí desku změnit"
        vraceno = _cmd(d, "zpet")
        assert "vráceno" in vraceno, vraceno[:300]
        assert _deska(_cmd(d, "board")) == pred
        log = open(os.path.join(d, "prikazy.log")).read().split()
        assert "do" not in log
        assert any(f.startswith("prikazy_pred_vracenim_") for f in os.listdir(d))
    finally:
        _cmd(d, "quit")
        server.wait(timeout=30)


def test_odstrihni_necha_nabidku_pred_akci():
    # review 14.10.: `actions` před vracenou akcí zůstává (stejná nabídka jako na obrazovce)
    log = ["kouc HOME", "los", "actions HOME", "do 3", "actions HOME", "do 7", "actions HOME"]
    nove, pryc = ps.odstrihni_posledni_akci(log)
    assert nove == ["kouc HOME", "los", "actions HOME", "do 3", "actions HOME"]
    assert pryc == ["do 7", "actions HOME"]


def test_odmitnuty_do_neni_v_logu(tmp_path):
    # review 14.10.: odmítnutý příkaz (tady index mimo nabídku ⇒ CHYBA) se z logu odstraní
    d = str(tmp_path)
    server = subprocess.Popen([sys.executable, os.path.join(KOREN, "bb_play_session_20261012.py")],
                              env=dict(os.environ, BB_PLAY_DIR=d), stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.5)
        _cmd(d, "kouc", "HOME")
        if "volba" in _cmd(d, "los"):
            _cmd(d, "volba", "prijimat")
        _po_vykopu(d)
        _cmd(d, "actions", "HOME")
        assert "CHYBA" in _cmd(d, "do", "9999")
        assert "do 9999" not in open(os.path.join(d, "prikazy.log")).read()
    finally:
        _cmd(d, "quit")
        server.wait(timeout=30)


def _server(d, partie=None):
    env = dict(os.environ, BB_PLAY_DIR=d)
    if partie:
        env["BB_PARTIE"] = partie
    return subprocess.Popen([sys.executable, os.path.join(KOREN, "bb_play_session_20261012.py")],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def test_prehrani_logu_neodmitne_akci_kterou_ted_varovani_odmita(tmp_path):
    # P243 (3.11.2026): log je, co se hrálo. Blok zahraný v partii přes `do` dřív, než přišlo varování
    # u bloku, který by AI nedala, se po restartu musí přehrát — jinak se partie rozejde (partie 2
    # zed-orkove po zavedení varování skončila při přehrání 0:0 místo 1:0).
    d = str(tmp_path)
    server = _server(d, "zed-orkove")
    try:
        time.sleep(1.5)
        _cmd(d, "kouc", "OBE")
        _cmd(d, "pozice")
        varovany = None
        for _ in range(12):
            bloky = [l.split("]")[0].strip(" [") for l in _cmd(d, "actions", "HOME").splitlines()
                     if "ActionType.BLOCK " in l]
            for idx in bloky:
                out = _cmd(d, "do", idx)
                if "⚠️ blok" in out:
                    varovany = idx
                    break
                if "ČEKÁ" in out or "❓" in out:   # neodmítnutý blok proběhl ⇒ nabídka je jiná
                    break
            if varovany is not None:
                break
        assert varovany is not None, "pozitivní kontrola: v pozici se musí najít blok s varováním"
        po = _cmd(d, "do!", varovany)
        for _ in range(8):                         # otázky kouče: kostka 0, ostatní „ne“
            if "❓" not in po:
                break
            po = _cmd(d, "odp", "0" if "kterou kostku" in po else "ne")
        deska = _deska(_cmd(d, "board"))
        log = os.path.join(d, "prikazy.log")
        radky = open(log).read().splitlines()
        assert f"do! {varovany}" in radky
        # jako log ze dřívějška: týž blok přes `do`
        open(log, "w").write("\n".join("do " + r[4:] if r == f"do! {varovany}" else r for r in radky) + "\n")
    finally:
        _cmd(d, "quit")
        server.wait(timeout=30)
    os.remove(os.path.join(d, "out.txt"))
    server = _server(d, "zed-orkove")
    try:
        for _ in range(120):
            if os.path.exists(os.path.join(d, "out.txt")):
                break
            time.sleep(0.5)
        assert _deska(_cmd(d, "board")) == deska
    finally:
        _cmd(d, "quit")
        server.wait(timeout=30)
