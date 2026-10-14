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
    assert nove == log[:5]
    assert pryc == log[5:]
    assert ps.odstrihni_posledni_akci(["kouc HOME", "los", "volba prijimat"]) is None
    assert ps.odstrihni_posledni_akci(["kouc HOME", "los", "do 1", "vykop AWAY"]) is None   # přes výkop ne


def _cmd(d, *prikaz):
    env = dict(os.environ, BB_PLAY_DIR=d, BB_PLAY_LIMIT="60")
    return subprocess.run([sys.executable, os.path.join(KOREN, "bb_play_cmd.py"), *prikaz], env=env,
                          capture_output=True, text=True, timeout=90).stdout


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
