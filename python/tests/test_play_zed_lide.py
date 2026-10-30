"""Preset `zed-lide` (30.10.2026): pozice `zed-orkove` zrcadlená vodorovně (x' = 25 - x, y beze změny).
Uživatel (HOME) má lidskou zeď, engine (AWAY) orčí klec s míčem u téhož hráče; na tahu orkové, jejich 4. kolo.

Hráči se porovnávají podle soupisky, statů a skillů — id závisí na pořadí soupisek, takže shoda id nic neříká.
"""
import json
import os
import subprocess
import sys
import time

KOREN = os.path.join(os.path.dirname(__file__), "..", "..")

SONDA = r"""
import json, os, sys
sys.path.insert(0, sys.argv[1])
import bb_play_session_20261012 as ps
st, _ = ps.new_game()
ps.zaloz_pozici(st)
side = lambda p: "HOME" if p.team_side == ps.bb.TeamSide.HOME else "AWAY"
hraci = [{"pole": [p.position.x, p.position.y], "strana": side(p),
          "soupiska": ps.P["soupisky"][0 if side(p) == "HOME" else 1],
          "otisk": [p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour, sorted(ps.skilly(p))],
          "id": p.id}
         for p in ps.enum_players(st) if p.position.x >= 0]   # enum_players vrací i náhradníky mimo hřiště
nosic = st.get_player(st.ball.carrier_id)
print(json.dumps({"hraci": hraci, "mic": [st.ball.position.x, st.ball.position.y], "drzen": st.ball.is_held,
                  "nosic_id": nosic.id, "na_tahu": str(st.active_team).split(".")[-1],
                  "kope": str(st.kicking_team).split(".")[-1], "polocas": st.half,
                  "faze": str(st.phase).split(".")[-1],
                  "kolo": {"HOME": st.home_team.turn_number, "AWAY": st.away_team.turn_number},
                  "soupisky": list(ps.P["soupisky"])}))
"""


def _pozice(partie, tmp_path):
    env = dict(os.environ, BB_PARTIE=partie, BB_PLAY_DIR=str(tmp_path / partie))
    r = subprocess.run([sys.executable, "-c", SONDA, KOREN], env=env, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    return json.loads(r.stdout)


def _klic(h):
    return (h["soupiska"], json.dumps(h["otisk"]))


def test_zed_lide_je_zrcadlo_zed_orkove(tmp_path):
    vzor, zrc = _pozice("zed-orkove", tmp_path), _pozice("zed-lide", tmp_path)
    # uživatel (HOME) má lidi, engine (AWAY) orky
    assert vzor["soupisky"] == ["orc", "human"] and zrc["soupisky"] == ["human", "orc"]
    for h in zrc["hraci"]:
        assert h["strana"] == ("HOME" if h["soupiska"] == "human" else "AWAY")
    # na každém zrcadleném poli stojí hráč téže soupisky se stejnými staty a skilly
    ocekavano = sorted((_klic(h), (25 - h["pole"][0], h["pole"][1])) for h in vzor["hraci"])
    skutecne = sorted((_klic(h), tuple(h["pole"])) for h in zrc["hraci"])
    assert skutecne == ocekavano
    # míč drží týž hráč (orčí thrower) na zrcadleném poli
    nosic_v = next(h for h in vzor["hraci"] if h["id"] == vzor["nosic_id"])
    nosic_z = next(h for h in zrc["hraci"] if h["id"] == zrc["nosic_id"])
    assert _klic(nosic_z) == _klic(nosic_v)
    assert zrc["drzen"] and zrc["mic"] == [25 - vzor["mic"][0], vzor["mic"][1]] == nosic_z["pole"]


def test_zed_lide_na_tahu_orkove_ve_4_kole(tmp_path):
    z = _pozice("zed-lide", tmp_path)
    assert (z["na_tahu"], z["faze"], z["polocas"]) == ("AWAY", "PLAY", 1)
    # orkové (AWAY) hrají 4. kolo, lidé (HOME) mají za sebou 3.; táhnou první ⇒ přijímali, kopali lidé
    assert z["kolo"] == {"AWAY": 4, "HOME": 3}
    assert z["kope"] == "HOME"


def test_zed_orkove_beze_zmeny(tmp_path):
    # výchozí chování ostatních presetů: na tahu HOME
    v = _pozice("zed-orkove", tmp_path)
    assert (v["na_tahu"], v["kope"], v["kolo"]) == ("HOME", "AWAY", {"HOME": 4, "AWAY": 3})


def _cmd(d, *prikaz):
    env = dict(os.environ, BB_PLAY_DIR=d, BB_PLAY_LIMIT="60")
    return subprocess.run([sys.executable, os.path.join(KOREN, "bb_play_cmd.py"), *prikaz], env=env,
                          capture_output=True, text=True, timeout=90).stdout


def test_zed_lide_pres_server_prikaz_pozice(tmp_path):
    # recenze 30.10. (nález 1): partie začíná příkazem `pozice` přes server, ne voláním zaloz_pozici()
    d = str(tmp_path)
    server = subprocess.Popen([sys.executable, os.path.join(KOREN, "bb_play_session_20261012.py")],
                              env=dict(os.environ, BB_PLAY_DIR=d, BB_PARTIE="zed-lide"),
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.5)
        _cmd(d, "kouc", "HOME")
        out = _cmd(d, "pozice")
        assert "nezačal" not in out, out[:300]
        deska = _cmd(d, "board")
        assert "aktivni tym: TeamSide.AWAY" in deska and "kolo H:3 A:4" in deska, deska[-400:]
    finally:
        _cmd(d, "quit")
        server.wait(timeout=30)
