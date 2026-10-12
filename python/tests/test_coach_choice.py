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
                      "USE_STAND_FIRM", "USE_SIDE_STEP"}, videno


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
