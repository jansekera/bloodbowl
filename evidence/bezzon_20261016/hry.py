# 16.10.2026 (uživatel: „zkus na lidech, co mají ogra — bone head — a na upírech“): hráč bez tackle zón
# (Bone-head, Hypnotic Gaze) nesmí chytat (ř. 7983-7986, 8185-8188). Lidé TV1200 (Ogre s Bone-head) × upíři
# (Vampire: Hypnotic Gaze, Bloodlust), oba směry, macro_mcts. Měří: kolikrát hráč přišel o zóny (pozitivní
# kontrola, že stav nastává), kolik pokusů o chytání (CATCH) udělal hráč bez zón (musí být 0), a pro srovnání
# chytání celkem. C++ engine.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 20
sys.path.insert(0, KOREN)

def hra(a):
    prij, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; A = bb.TeamSide.AWAY
    lide = bb.get_developed_roster("human", 1200); upiri = bb.get_roster("vampire")
    s = bb.GameState()
    if prij == "human": bb.setup_half(s, lide, upiri, A)
    else: bb.setup_half(s, upiri, lide, A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(7000 + g))
    r = dict(prij=prij, g=g, tahu=0, bez_zon_hracu_tahu=0, chytani=0, chytani_bez_zon=0, odraz_na_bez_zon=0)
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        bez = {p.id for p in kt.hraci(s) if p.lost_tacklezones and kt._stoji(p)}
        r["bez_zon_hracu_tahu"] += len(bez)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=80000 + 100 * g + tah)
        r["tahu"] += 1
        for k in kroky:
            for e in k["events"]:
                if e["type"] == "CATCH":
                    r["chytani"] += 1
                    if e["player"] in bez: r["chytani_bez_zon"] += 1
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return r

if __name__ == "__main__":
    ukoly = [(p, g) for p in ("human", "vampire") for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb"))
    for k in ["tahu", "bez_zon_hracu_tahu", "chytani", "chytani_bez_zon"]:
        print(k, sum(r[k] for r in res))
