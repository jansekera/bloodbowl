"""P130: rozhodnutí kouče uprostřed akce (vazba execute_action_coach) a výpis otázek v partii.

Pozitivní kontrola: test projde jen tehdy, když se v odehraných akcích objeví KAŽDÝ druh otázky,
který soupisky lidí a orků TV1500 umožňují — měřidlo, které se nikdy nezeptá, by jinak prošlo.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
import bb_play_session_20261012 as ps  # noqa: E402

bb = ps.bb
H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY


def otisk(s):
    hraci = tuple((i, s.get_player(i).position.x, s.get_player(i).position.y, str(s.get_player(i).state))
                  for i in range(1, 23))
    return (hraci, s.home_team.rerolls, s.away_team.rerolls, s.ball.position.x, s.ball.position.y,
            s.ball.is_held, str(s.active_team))


def nova_hra(seed):
    s = bb.GameState()
    bb.setup_half(s, bb.get_developed_roster("human", 1500), bb.get_developed_roster("orc", 1500), A)
    d = bb.DiceRoller(seed)
    bb.simple_kickoff(s, d)
    return s, d


def test_otazka_vrati_stav_i_kostky_a_potka_vsechny_druhy():
    videno = set()
    for seed in range(1, 31):
        s, d = nova_hra(seed)
        for krok in range(100):
            akce = list(bb.get_available_actions(s))
            if not akce or s.phase != bb.GamePhase.PLAY:
                break
            ostre = [a for a in akce if any(k in str(a.type) for k in ("BLOCK", "BLITZ", "PASS"))] or akce
            a = ostre[(seed * 7 + krok * 13) % len(ostre)]
            odpovedi = []
            for pokus in range(12):
                pred = otisk(s)
                r, ev, q = bb.execute_action_coach(s, a, d, H, odpovedi)
                if q is None:
                    assert r is not None
                    break
                assert r is None
                videno.add(q["question"])
                assert otisk(s) == pred, "otázka nesmí změnit stav"
                _, _, q2 = bb.execute_action_coach(s, a, d, H, odpovedi)
                assert q2 == q, "stejné kostky ⇒ stejná otázka"
                assert ps.otazka_str(q).startswith("❓")
                volby = [i for i in range(8) if q["options"] & (1 << i)]
                assert volby and 0 in volby or q["question"] == "BLOCK_FACE"
                odpovedi.append(volby[(seed + krok + pokus) % len(volby)])
            else:
                raise AssertionError("akce se ptá donekonečna")
    assert videno >= {"REROLL", "BLOCK_REROLL", "BLOCK_FACE", "USE_DODGE", "USE_WRESTLE",
                      "USE_STAND_FIRM", "USE_SIDE_STEP", "USE_FOLLOW_UP", "USE_APOTHECARY"}, videno


def test_neplatna_odpoved_nic_nezmeni():
    for seed in range(1, 31):
        s, d = nova_hra(seed)
        for a in bb.get_available_actions(s):
            _, _, q = bb.execute_action_coach(s, a, d, H, [])
            if q is None:
                break                      # akce proběhla, stav se změnil -- další semínko
            pred = otisk(s)
            spatna = next(i for i in range(8) if not q["options"] & (1 << i))
            try:
                bb.execute_action_coach(s, a, d, H, [spatna])
            except Exception:
                assert otisk(s) == pred
                return
            raise AssertionError("neplatná odpověď měla skončit chybou")
    raise AssertionError("žádná otázka se nenašla — test nic neověřil")


def test_bez_kouce_se_druha_strana_nepta():
    # kouč je AWAY, na tahu HOME: přehozy HOME zůstávají automatické; ptát se smí jen na skilly AWAY
    s, d = nova_hra(3)
    for krok in range(60):
        akce = list(bb.get_available_actions(s))
        if not akce or s.phase != bb.GamePhase.PLAY or s.active_team != H:
            break
        _, _, q = bb.execute_action_coach(s, akce[(krok * 5) % len(akce)], d, A, [])
        if q is not None:
            assert q["question"].startswith("USE_"), q
            assert s.get_player(q["player"]).team_side == A
            break


def test_preklad_odpovedi():
    prehoz = {"question": "REROLL", "player": 1, "other": -1, "roll": 2, "target": 4, "options": 0b011}
    assert ps.preloz_odpoved(prehoz, "ne") == 0
    assert ps.preloz_odpoved(prehoz, "tym") == 1
    assert ps.preloz_odpoved(prehoz, "pro") is None          # Pro není mezi možnostmi
    skill = {"question": "USE_DODGE", "player": 5, "other": 20, "roll": 0, "target": 0, "options": 0b11}
    assert ps.preloz_odpoved(skill, "ano") == 1 and ps.preloz_odpoved(skill, "ne") == 0
    assert ps.preloz_odpoved(skill, "tym") is None
    kostka = {"question": "BLOCK_FACE", "player": 1, "other": 16, "roll": (3 + 1) | ((0 + 1) << 3), "target": 2,
              "options": (1 << 3) | (1 << 0)}
    assert ps.preloz_odpoved(kostka, "3") == 3
    assert ps.preloz_odpoved(kostka, "4") is None
    assert "Defender Stumbles" in ps.otazka_str(kostka) and "lebka" in ps.otazka_str(kostka)


def test_preklad_odpovedi_follow_up_a_lekarnik():
    # P201 (1) a (12): ano / ne; u výběru výsledku lékárníka „1“ = první hod, „2“ = druhý
    f = {"question": "USE_FOLLOW_UP", "player": 1, "other": 16, "roll": 0, "target": 0, "options": 0b11}
    assert ps.preloz_odpoved(f, "ano") == 1 and ps.preloz_odpoved(f, "ne") == 0
    assert "následovat" in ps.otazka_str(f)
    ko = {"question": "USE_APOTHECARY", "player": 11, "other": -1, "roll": -1, "target": 0, "options": 0b11}
    assert ps.preloz_odpoved(ko, "ano") == 1 and "KO" in ps.otazka_str(ko)
    cas = dict(ko, roll=7)
    assert "MRTEV" in ps.otazka_str(cas)
    vyber = {"question": "APOTHECARY_PICK", "player": 11, "other": -1, "roll": 7, "target": 0, "options": 0b11}
    assert ps.preloz_odpoved(vyber, "1") == 0 and ps.preloz_odpoved(vyber, "2") == 1
    assert ps.preloz_odpoved(vyber, "ano") is None
    assert "MRTEV" in ps.otazka_str(vyber) and "Badly Hurt" in ps.otazka_str(vyber)
    ev = {"type": "APOTHECARY", "player": 11, "target": -1, "roll": -1, "success": True, "die1": 0, "die2": 0,
          "to": (0, 0)}
    assert "omráčený" in ps.event_str(ev)


def test_vypis_prehozu():
    e = {"type": "REROLL", "player": 7, "target": -1, "roll": 0, "success": True, "die1": 2, "die2": 5, "to": (0, 0)}
    assert "původní hod 2" in ps.event_str(e) and "nový hod 5" in ps.event_str(e)
    e.update(roll=3, die1=2, die2=0, success=False)
    assert "PROPADL" in ps.event_str(e)


def test_prvni_vykop_respektuje_kopajici_stranu():
    # P195: setup_half nenastavoval state.kicking_team ⇒ výkop vždy proběhl, jako by kopali AWAY.
    # Po výkopu je (až na události výkopu) na tahu ten, kdo PŘIJÍMÁ.
    for kope, prijima in ((H, A), (A, H)):
        na_tahu = []
        for seed in range(1, 41):
            s = bb.GameState()
            d = bb.DiceRoller(seed)
            bb.setup_half(s, bb.get_developed_roster("human", 1500), bb.get_developed_roster("orc", 1500), kope)
            assert s.kicking_team == kope
            bb.simple_kickoff(s, d)
            na_tahu.append(s.active_team)
        assert na_tahu.count(prijima) > 30, (kope, na_tahu.count(prijima))


def test_vypis_vykopu():
    # P200 (3) a P201 (13): výkop má ve výpisu rozptyl, tabulku a návrat z KO
    e = {"type": "KICKOFF", "player": -1, "target": -1, "roll": 1, "success": True, "die1": 3, "die2": 8,
         "from": (3, 7), "to": (2, 6)}
    r = ps.event_str(e)
    assert "D6 = 3" in r and "Kick půlí 3 -> 1" in r and "D8 = 8" in r and "(2, 6)" in r
    e.update(type="KICKOFF_TABLE", roll=6, target=6)
    assert "2D6 = 6" in ps.event_str(e) and "Cheering Fans" in ps.event_str(e)
    e.update(type="KO_RECOVERY", player=4, roll=3, success=False)
    assert "zůstává v KO" in ps.event_str(e)
    # celý výkop: ve 40 výkopech se objeví tabulka v každém a aspoň jednou přesun i reroll navíc
    tabulka = presun = rr = 0
    for seed in range(1, 41):
        s, d = ps.new_game(seed)
        out = ps.zahaj(s, d, A)
        tabulka += sum("VÝKOPOVÁ TABULKA" in l for l in out)
        presun += any("přesun při výkopu" in l for l in out)
        rr += any("týmové rerolly" in l for l in out)
    assert tabulka == 40 and presun > 0 and rr > 0, (tabulka, presun, rr)


def test_vypis_p201_body_2_3_4_9_11():
    u = lambda **kw: dict({"player": 7, "target": -1, "roll": 0, "success": True, "die1": 0, "die2": 0,
                           "from": (0, 0), "to": (0, 0)}, **kw)
    assert "potřeba 3+, padlo 4" in ps.event_str(u(type="PASS", roll=3, die1=4))                   # (9)
    assert "ZRANĚNÍ" in ps.event_str(u(type="INJURY", roll=10, die1=5, die2=5))                    # (2)
    lek = ps.event_str(u(type="APOTHECARY", roll=0, die1=8, die2=1))
    assert "REZERV" in lek and "vrátí se příští zápas" not in lek                                   # (3)
    assert "STAND FIRM" in ps.event_str(u(type="SKILL", roll=ps.SKILL_NAMES.index("StandFirm")))    # (4)
    assert "engine" in ps.event_str(u(type="REROLL", roll=4))                                      # (11)


def test_vypis_mighty_blow():
    # P201 (8): kam se započetl +1, a modifikátor u hodu zvlášť
    u = lambda **kw: dict({"player": 12, "target": -1, "roll": 0, "success": True, "die1": 0, "die2": 0,
                           "from": (0, 0), "to": (0, 0)}, **kw)
    mb = ps.SKILL_NAMES.index("MightyBlow")
    assert "+1 na brnění" in ps.event_str(u(type="SKILL", roll=mb, die1=1))
    assert "+1 na zranění" in ps.event_str(u(type="SKILL", roll=mb, die1=2))
    assert "9 (4+4 +1)" in ps.event_str(u(type="ARMOR_BREAK", roll=9, die1=4, die2=4))
    assert "8 (4+4)" in ps.event_str(u(type="ARMOR_BREAK", roll=8, die1=4, die2=4))


def test_kouc_za_obe_strany():
    # P201 (6): s both_sides se ptá i strana, za kterou by jinak rozhodl engine
    s, d = nova_hra(3)
    ptala = set()
    for seed in range(1, 31):
        s, d = nova_hra(seed)
        for krok in range(80):
            akce = list(bb.get_available_actions(s))
            if not akce or s.phase != bb.GamePhase.PLAY:
                break
            a = akce[(seed * 7 + krok * 13) % len(akce)]
            odp = []
            for _ in range(12):
                r, ev, q = bb.execute_action_coach(s, a, d, H, odp, True)
                if q is None:
                    break
                ptala.add(s.get_player(q["player"]).team_side)
                odp.append(next(i for i in range(8) if q["options"] & (1 << i)))
    assert ptala == {H, A}, ptala
