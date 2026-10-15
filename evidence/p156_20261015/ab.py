# P156 (15.10.2026): párové A/B — Wild Animal smí ohlásit blitz bez cíle. Báze 5ece146c × 18a9ebdc.
# Skaveni (Rat Ogre = Wild Animal) přijímají i kopou proti 4 rasám, macro_mcts, semínka jako P196.
# Předregistrace (kniha P156): mechanismus napřed — hody Wild Animal (kolik propadlo), blitzy bez cíle,
# blitzy s cílem, pole ušlá Rat Ogrem; TD až nakonec.
# ab.py KOREN vystup.pkl [semínek]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 25
SOUPERI = ["dwarf", "wood-elf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    WA = kt.SKILL["WildAnimal"]
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    sk = H if prij == "skaven" else A
    r = dict(prij=prij, kop=kop, g=g, wa_hodu=0, wa_propadlo=0, blitz_bez_cile=0, blitz_s_cilem=0,
             ogre_pole=0, ogre_tahu=0, td=0, td_soupere=0)
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        ogre = [p.id for p in kt.hraci(s) if p.team_side == sk and kt.ma_skill(p, "WildAnimal")]
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if strana == sk:
            for k in kroky:
                a_ = k["action"]
                if a_.type == bb.ActionType.BLITZ:
                    r["blitz_bez_cile" if a_.target_id < 0 else "blitz_s_cilem"] += 1
                for e in k["events"]:
                    if e["type"] == "SKILL" and e["roll"] == WA and e["player"] in ogre:
                        r["wa_hodu"] += 1; r["wa_propadlo"] += (not e["success"])
                if a_.type == bb.ActionType.MOVE and a_.player_id in ogre: r["ogre_pole"] += 1   # 1 akce = 1 pole
            r["ogre_tahu"] += 1 if ogre else 0
        if po.home_team.score > s.home_team.score: r["td"] = 1; break
        if po.away_team.score > s.away_team.score: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, g) for o in SOUPERI for p, k in (("skaven", o), (o, "skaven")) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
