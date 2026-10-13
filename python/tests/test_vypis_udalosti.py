"""P145: výpis událostí v partii — co padlo na kostce, odraz × vhazování, lékárník, dav, zranění.

Pozitivní kontrola: v odehraných akcích se musí objevit úhyb, GFI, zvednutí, odraz a zranění,
jinak by test prošel i s výpisem, který tyhle události vůbec nepotká.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
import bb_play_session_20261012 as ps  # noqa: E402

bb = ps.bb
H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY


def test_vypis_ze_skutecnych_udalosti_enginu():
    videno = {}
    for seed in range(1, 61):
        s = bb.GameState()
        bb.setup_half(s, bb.get_developed_roster("human", 1500), bb.get_developed_roster("orc", 1500), A)
        d = bb.DiceRoller(seed)
        bb.simple_kickoff(s, d)
        for krok in range(150):
            akce = list(bb.get_available_actions(s))
            if not akce or s.phase != bb.GamePhase.PLAY:
                break
            a = akce[(seed * 7 + krok * 13) % len(akce)]
            _, ev = bb.execute_action_logged(s, a, d)
            for e in ev:
                radek = ps.event_str(e)
                t = e["type"]
                if t in ("DODGE", "GFI", "PICKUP", "STAND_UP", "LEAP") and not e["roll"]:
                    assert "bez hodu" in radek and not e["die1"], radek
                    t += " bez hodu"
                elif t in ("DODGE", "GFI", "PICKUP", "STAND_UP", "LEAP"):
                    assert f"padlo {e['die1']}" in radek and 1 <= e["die1"] <= 6, radek
                    assert (e["die1"] >= e["roll"]) == e["success"], radek
                elif t == "BALL_BOUNCE":
                    t = "VHAZOVÁNÍ" if e["die1"] else "ODRAZ"
                    assert radek.startswith(t), radek
                    fx, fy = e["from"]
                    assert 0 <= fx < 26 and 0 <= fy < 15, f"míč se pouští z pole mimo hřiště: {radek}"
                    if t == "VHAZOVÁNÍ" and "DO AUTU" not in radek:
                        tx, ty = e["to"]
                        assert max(abs(tx - fx), abs(ty - fy)) == e["roll"] - 1, radek   # FAQ ř. 9307-9314
                elif t == "INJURY":
                    assert e["roll"] > 0 and ("omráčen" in radek or "KO" in radek), radek
                assert " cíl -1 hod " not in radek or t in ("PUSH", "KNOCKED_DOWN", "TOUCHDOWN", "TURNOVER",
                                                             "KICKOFF", "WEATHER", "FOUL", "HAND_OFF",
                                                             "REGENERATION", "EJECTED", "BLOODLUST_FEED"), radek
                videno[t] = videno.get(t, 0) + 1
    # Vhazování v náhodné hře prakticky nenastane (0 ve 200 poločasech, 13.10.) -- jeho událost
    # hlídá C++ test P145ThrowInEventCarriesTheTemplateDie… a tvar řádku test níž.
    assert set(videno) >= {"DODGE", "GFI", "PICKUP", "ODRAZ", "INJURY"}, videno


def udalost(typ, **kw):
    e = {"type": typ, "player": 7, "target": -1, "roll": 0, "success": True, "die1": 0, "die2": 0,
         "from": (10, 0), "to": (0, 0)}
    e.update(kw)
    return e


def test_vypis_zraneni_lekarnika_a_davu():
    # (c) `target` u CASUALTY je výsledek tabulky, hráč je `player` -- tisklo se „CASUALTY hráč 1“
    r = ps.event_str(udalost("CASUALTY", player=25, target=7, roll=11, die1=6, die2=5))
    assert "hráč 25" in r and "MRTEV" in r and "(6+5)" in r
    # (b) lékárník: první výsledek MRTEV (7), druhý Badly Hurt (0), platí Badly Hurt
    r = ps.event_str(udalost("APOTHECARY", player=4, roll=0, die1=8, die2=1))
    assert r.startswith("LÉKÁRNÍK hráč 4") and "MRTEV" in r and "platí Badly Hurt" in r
    # (d) dav má vlastní řádek, ne „INJURY hráč N: 0“
    assert ps.event_str(udalost("CROWD_SURF", player=25, **{"from": (16, 14)})) == "DO DAVU hráč 25 z pole (16, 14)"
    # vhazování (die1 = kostka šablony) × odraz (die1 = 0); `to` mimo hřiště = do autu
    r = ps.event_str(udalost("BALL_BOUNCE", player=-1, roll=9, die1=5, **{"from": (16, 14), "to": (24, 6)}))
    assert r == "VHAZOVÁNÍ z (16,14): šablona 5, 2D6 = 9 -> (24,6)"
    r = ps.event_str(udalost("BALL_BOUNCE", player=-1, roll=8, **{"from": (19, 0), "to": (18, -1)}))
    assert r == "ODRAZ míče (19,0) -> (18,-1), D8 = 8 — DO AUTU"
    # přehoz skillem: původní i nový hod
    r = ps.event_str(udalost("SKILL", roll=ps.SKILL_NAMES.index("Dodge"), die1=4, die2=2))
    assert "Dodge" in r and "2 -> 4" in r
