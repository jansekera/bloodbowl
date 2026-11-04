# P245 (4.11.2026): obrana zavazí nosiči. Jen tah + odpověď soupeře, párově po situacích (stejné semínko).
# Situace = náš tah v obraně (míč drží soupeř), kde se plán s P245 a bez něj liší. Tah K-krát v obou ramenech.
# Měří se (mechanismus napřed): výhled soupeřova nosiče po našem tahu — šance na TD v jeho tahu (i s úhyby)
#   a nejbližší doběh k zóně s rizikem do 1/3; pak tah soupeře: TD, míč ztracen, o kolik sloupců nosič postoupil;
#   turnover v našem tahu; blitz na nosiče zahrán.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
K = int(sys.argv[4]) if len(sys.argv) > 4 else 6
NASI = ["human", "orc", "dwarf"]; SOUPERI = ["orc", "human", "skaven"]
P245 = 8388608 | 16777216
RAMENA = {"vse": 0, "bez": P245}
sys.path.insert(0, KOREN)


def nosic(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return None
    p = s.get_player(s.ball.carrier_id)
    return p if p.team_side == side and p.is_on_pitch() else None


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    bb.set_extra_features_off(RAMENA[rameno])
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    bb.set_extra_features_off(0)
    o = dict(turnover=int(any(k["turnover"] for k in kroky)), td_sance=0.0, dobeh=26, nosic_mic=0,
             blitz_nosic=int(any(k.get("macro", {}).get("type") == "BLITZ" and k["macro"]["target"] == s0.ball.carrier_id for k in kroky)),
             td_soupere=0, mic_ztracen=0, postup=0)
    c = nosic(bb, po, A)
    if c is None:
        o["mic_ztracen"] = 1   # už v našem tahu (sražen / vypadl)
        return o
    o["nosic_mic"] = 1
    o["td_sance"], o["dobeh"] = bb.carrier_outlook(po, c.id, 1.0 / 3.0)
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        ez = 25 if A == bb.TeamSide.HOME else 0
        d0 = abs(c.position.x - ez)
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        c2 = nosic(bb, po2, A)
        if not o["td_soupere"]:
            if c2 is None: o["mic_ztracen"] = 1
            else: o["postup"] = d0 - abs(c2.position.x - ez)
    return o


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
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if s.active_team == H and nosic(bb, s, A) is not None:
            bb.set_extra_features_off(P245)
            kroky0, po0, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
            bb.set_extra_features_off(0)
            mk = lambda ks: [(k["macro"]["type"], k["macro"]["player"], tuple(k["macro"]["target_pos"])) for k in ks if "macro" in k]
            if mk(kroky) != mk(kroky0):
                sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, kolo_soupere=s.away_team.turn_number + 1, ramena={})
                for r in RAMENA:
                    sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A) for j in range(K)]
                out.append(sit)
                print("situace", nasi, sou, g, tah, flush=True)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI[:int(os.environ.get("RAS", "3"))] for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
