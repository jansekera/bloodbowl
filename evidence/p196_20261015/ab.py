# P196 (15.10.2026): párové A/B opravy „nabídka = jen proveditelné“ — báze 4ff106e3 × oprava.
# Stejné poločasy jako mereni.py (5 ras TV1500, každá přijímá proti každé, macro_mcts, semínka
# 3000+g / 50000+100g+tah). Předregistrace (kniha P196): MECHANISMUS napřed — tahy „no_actions“
# (čekám pokles k ~0) a „rescue“ (záchrana K6), pak čas plánu tahu, TD až nakonec.
# ab.py KOREN vystup.pkl [semínek]
import sys, os, time, pickle, itertools, multiprocessing as mp
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
    r = dict(prij=prij, kop=kop, g=g, tahu=0, rozhodnuti=0, no_actions=0, rescue=0, nehrali=[], cas=0.0,
             td=0, td_soupere=0, akci=0)
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        t0 = time.process_time()
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        r["cas"] += time.process_time() - t0
        r["tahu"] += 1; r["akci"] += len(kroky)
        hrali = set()
        for k in kroky:
            m = k.get("macro")
            if m is not None:
                r["rozhodnuti"] += 1
                if m["source"] == "rescue": r["rescue"] += 1
                if m["source"] == "no_actions":
                    r["no_actions"] += 1
                    mohli = [p.id for p in kt.hraci(s) if p.team_side == strana and p.state in (bb.PlayerState.STANDING, bb.PlayerState.PRONE)]
                    r["nehrali"].append(len([i for i in mohli if i not in hrali]))
            a = k["action"]
            if a.player_id is not None and a.player_id > 0: hrali.add(a.player_id)
        if po.home_team.score > s.home_team.score: r["td"] = 1; break
        if po.away_team.score > s.away_team.score: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, g) for p, k in itertools.permutations(RASY, 2) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
