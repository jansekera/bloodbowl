# P249 (4.11.2026): soupeřův nosič — B4 (blitz jen na něj, když ho nikdo nezablokuje zdarma), b (blok na 1 kostku
# i 2 proti), c (přehoz kvůli nosiči), d (odsun k dalšímu blokujícímu + follow-up). Jen náš tah + odpověď soupeře,
# párově po situacích (stejné semínko).
# Situace = náš tah v obraně (míč drží soupeř) a na nosiče někdo z našich dosáhne (vzdálenost ≤ MA + 2). Výběr
# NEPODLE rozdílu plánů: c a d působí až při vyhodnocení bloku, plán (makra) se kvůli nim měnit nemusí.
# Ramena: vše · bez_b4 · bez_b · bez_c · bez_d · bez (nic z P249). K opakování na rameno.
# Měří se (mechanismus napřed): soupeř ztratil míč v našem tahu; blitz / blok na nosiče; čítače P249 (pozitivní
# kontrola, že část v rameni zabrala); pak tah soupeře: TD, o kolik sloupců nosič postoupil, naši sražení; turnover.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 4
K = int(sys.argv[4]) if len(sys.argv) > 4 else 3
NASI = ["human", "orc", "dwarf"]; SOUPERI = ["orc", "human", "skaven"]
B4, B, C, D = 67108864, 134217728, 268435456, 536870912
RAMENA = {"vse": 0, "bez_b4": B4, "bez_b": B, "bez_c": C, "bez_d": D, "bez": B4 | B | C | D}
sys.path.insert(0, KOREN)


def nosic(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return None
    p = s.get_player(s.ball.carrier_id)
    return p if p.team_side == side and p.is_on_pitch() else None


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    cid = s0.ball.carrier_id
    bb.set_extra_features_off(RAMENA[rameno]); bb.take_p249_carrier_blitz_first(); bb.take_p249_extra()
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    c249, cx = bb.take_p249_carrier_blitz_first(), bb.take_p249_extra()
    bb.set_extra_features_off(0)
    makra = [k.get("macro", {}) for k in kroky]
    o = dict(c249=c249, cx=cx, turnover=int(any(k["turnover"] for k in kroky)),
             blitz_nosic=int(any(m.get("type") == "BLITZ" and m.get("target") == cid for m in makra)),
             blok_nosic=int(any(m.get("type") == "BLOCK" and m.get("target") == cid for m in makra)),
             mic_nas_tah=0, mic_ztracen=0, td_soupere=0, postup=0, nasi_srazeni=0)
    c = nosic(bb, po, A)
    if c is None:
        o["mic_nas_tah"] = o["mic_ztracen"] = 1
        return o
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        ez = 25 if A == bb.TeamSide.HOME else 0
        d0 = abs(c.position.x - ez)
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        nasi = {p.id for p in kt.hraci(po) if p.team_side == H}
        o["nasi_srazeni"] = sum(1 for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN" and e["player"] in nasi)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        c2 = nosic(bb, po2, A)
        if not o["td_soupere"]:
            if c2 is None: o["mic_ztracen"] = 1
            else: o["postup"] = d0 - abs(c2.position.x - ez)
    return o


def dosah(kt, bb, s, H, c):
    for p in kt.hraci(s):
        if p.team_side == H and p.state == bb.PlayerState.STANDING:
            d = max(abs(p.position.x - c.position.x), abs(p.position.y - c.position.y))
            if d <= p.stats.movement + 2: return True
    return False


def hra(a):
    nasi, sou, g, utok = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    kick = A if utok else H
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), kick)
    s.kicking_team = kick
    bb.simple_kickoff(s, bb.DiceRoller(5000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 70000 + 100 * g + tah + (50 if utok else 0)
        c = nosic(bb, s, A) if s.active_team == H else None
        if c is not None and dosah(kt, bb, s, H, c):
            sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, kolo_soupere=s.away_team.turn_number + 1, ramena={})
            for r in RAMENA:
                sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A) for j in range(K)]
            out.append(sit)
            print("situace", nasi, sou, g, tah, flush=True)
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
