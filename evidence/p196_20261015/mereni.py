# P196 (14.10.2026): jak často AI ukončí tah, protože nic z nabídky nešlo provést („no_actions“),
# a kolik hráčů v tu chvíli ještě nehrálo. Stejné poločasy jako mech.py / td_vse.py (5 ras TV1500,
# každá přijímá proti každé, macro_mcts). Pozitivní kontrola: báze 1ff3a39e, test_kontrola_tahu
# semínko 5 ⇒ 1× no_actions (ověřeno 14.10.).
# mereni.py KOREN vystup.pkl [semínek]
import sys, os, pickle, itertools, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 10
RASY = ["dwarf", "wood-elf", "skaven", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    r = dict(prij=prij, kop=kop, g=g, tahu=0, rozhodnuti=0, no_actions=0, nehrali=[], kola=[])
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        r["tahu"] += 1
        hrali = set()
        for k in kroky:
            m = k.get("macro")
            if m is not None:
                r["rozhodnuti"] += 1
                if m["source"] == "no_actions":
                    r["no_actions"] += 1
                    mohli = [p.id for p in kt.hraci(s) if p.team_side == strana and p.state in (bb.PlayerState.STANDING, bb.PlayerState.PRONE)]
                    r["nehrali"].append(len([i for i in mohli if i not in hrali]))
                    r["kola"].append(tah)
            a = k["action"]
            if a.player_id is not None and a.player_id > 0: hrali.add(a.player_id)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, g) for p, k in itertools.permutations(RASY, 2) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
