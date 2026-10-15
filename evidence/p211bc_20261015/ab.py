# P211b+c (15.10.2026): párové A/B — Wild Animal v kleci až před koncem tahu (s volným blitzem ohlášeným)
# a výběr blitzujícího „doběhne × srazí“. Báze b83afcf5 (P211 útěky) × e3a9644d.
# Naši: skaveni a lesní elfové × trpaslíci, orkové, lidé; přijímáme i kopeme; macro_mcts, semínka jako P196.
# Předregistrace (kniha P211): mechanismus napřed — kdo blitzuje (Wild Animal / Block a síla 3+ / síla 2 /
# ostatní), kolik blitzů s cílem srazí cíl nebo mu vyrazí míč, hody Wild Animal podle zdroje (klec / hledání)
# a kolik propadlo, kolikrát klec Ogra přesunula a kolikrát s ohlášeným blitzem; turnovery; TD až nakonec.
# ab.py KOREN vystup.pkl [semínek]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 12
NASI = ["skaven", "wood-elf"]; SOUPERI = ["dwarf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, nasi, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    WA = kt.SKILL["WildAnimal"]
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H if prij == nasi else A
    def role(p):
        if kt.ma_skill(p, "WildAnimal"): return "wa"
        if kt.ma_skill(p, "Block") and p.stats.strength >= 3: return "block_s3"
        if p.stats.strength <= 2: return "s2"
        return "jiny"
    r = dict(prij=prij, kop=kop, nasi=nasi, g=g, tahu=0, turnover=0, td=0, td_soupere=0,
             blitz={}, blitz_srazil={}, wa_hodu={}, wa_propadlo={}, ogre_klec=0, ogre_klec_ohlasen=0)
    def inc(d, k, v=1): d[k] = d.get(k, 0) + v
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        if s.active_team == my:
            role_of = {p.id: role(p) for p in kt.hraci(s) if p.team_side == my}
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if s.active_team == my:
            r["tahu"] += 1; r["turnover"] += any(k["turnover"] for k in kroky)
            makro = None; otevreny = None   # (cíl, nosič míče před blitzem?) až do dalšího makra
            for k in kroky:
                if "macro" in k:
                    makro = k["macro"]; otevreny = None
                    if makro["source"] == "cage" and role_of.get(makro["player"]) == "wa" and makro["type"] == "REPOSITION":
                        r["ogre_klec"] += 1; r["ogre_klec_ohlasen"] += makro["declare_blitz"]
                a_ = k["action"]
                if a_.type == bb.ActionType.BLITZ and a_.target_id > 0:
                    rl = role_of.get(a_.player_id, "?"); inc(r["blitz"], rl)
                    otevreny = [a_.target_id, rl, False]
                for e in k["events"]:
                    if e["type"] == "SKILL" and e["roll"] == WA and role_of.get(e["player"]) == "wa":
                        src = makro["source"] if makro else "?"
                        inc(r["wa_hodu"], src); inc(r["wa_propadlo"], src, not e["success"])
                    if otevreny and not otevreny[2]:
                        if (e["type"] == "KNOCKED_DOWN" and e["player"] == otevreny[0]) or \
                           (e["type"] == "BALL_BOUNCE" and s.ball.is_held and s.ball.carrier_id == otevreny[0]):
                            otevreny[2] = True; inc(r["blitz_srazil"], otevreny[1])
        my_d = (po.home_team.score - s.home_team.score) if my == H else (po.away_team.score - s.away_team.score)
        th_d = (po.away_team.score - s.away_team.score) if my == H else (po.home_team.score - s.home_team.score)
        if my_d > 0: r["td"] = 1; break
        if th_d > 0: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, n, g) for n in NASI for o in SOUPERI for p, k in ((n, o), (o, n)) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
