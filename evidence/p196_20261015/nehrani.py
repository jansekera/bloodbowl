# P196 (15.10.2026), uživatel: „když je např. human lineman zavázaný vedle orka — je záměrné zůstat a nezkusit
# ani blok na 1 kostku ani dodge pryč — umíš to vyhodnotit?“ Pro každý tah „no_actions“ z pricina.pkl přehraje
# stav (stejné kostky) a u každého nehraného hráče určí: v kolika soupeřových zónách stojí, nejlepší blok
# (počet kostek PŘIBLIŽNĚ: síla + asistence bez Guardu a bez ztracených zón), Block, úhyb pryč (nejlepší
# sousední pole: 6 − AG + zóny v cíli, min 2+), zda leží, kolo týmu (8 = poslední kolo poločasu).
# nehrani.py vstup_pricina.pkl vystup.pkl
import sys, os, pickle, multiprocessing as mp
KOREN = "/home/jenda/claude/blood-bowl"; sys.path.insert(0, KOREN)


def hra(skupina):
    import bb_kontrola_tahu as kt
    bb = kt.bb; A = bb.TeamSide.AWAY; ST = bb.PlayerState.STANDING
    r0 = skupina[0]
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(r0["prij"], 1500), bb.get_developed_roster(r0["kop"], 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + r0["g"]))
    chci = {r["tah"]: r for r in skupina}; out = []
    for tah in range(max(chci) + 1):
        seed = 50000 + 100 * r0["g"] + tah
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if tah in chci:
            r = chci[tah]
            rep = s.clone(); d = bb.DiceRoller((seed ^ 0x5bd1e995) & 0xffffffff)
            for k in kroky[:r["krok"]]: bb.execute_action_logged(rep, k["action"], d)
            na_hristi = [p for p in kt.hraci(rep) if p.is_on_pitch()]
            u = {(p.position.x, p.position.y): p for p in na_hristi}
            def sousede(pos):
                return [(pos[0] + dx, pos[1] + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                        if (dx or dy) and 0 <= pos[0] + dx < 26 and 0 <= pos[1] + dy < 15]
            def zony(pos, strana):   # kolik stojících soupeřů strany `strana` sousedí s polem
                return sum(1 for q in sousede(pos) if q in u and u[q].team_side != strana and u[q].state == ST)
            def asist(pomocnik_strana, cil, kromě):
                n = 0
                for q in sousede((cil.position.x, cil.position.y)):
                    h = u.get(q)
                    if not h or h.team_side != pomocnik_strana or h.state != ST or h.id == kromě.id: continue
                    if zony(q, pomocnik_strana) - 1 == 0: n += 1   # mimo cíl nikdo jiný nezná
                return n
            for pid, info in r["nehrali"].items():
                p = rep.get_player(pid); pos = (p.position.x, p.position.y)
                z = zony(pos, p.team_side); bloky = []
                if p.state == ST:
                    for q in sousede(pos):
                        dfd = u.get(q)
                        if not dfd or dfd.team_side == p.team_side or dfd.state != ST: continue
                        a = p.stats.strength + asist(p.team_side, dfd, p); b = dfd.stats.strength + asist(dfd.team_side, p, dfd)
                        k = 3 if a > 2 * b else 2 if a > b else 1 if a == b else -3 if b > 2 * a else -2
                        bloky.append(k)
                volna = [q for q in sousede(pos) if q not in u]
                uhyb = min([max(2, 6 - p.stats.agility + zony(q, p.team_side)) for q in volna], default=None) if z else None
                out.append(dict(skupina=r["kat"], rasa=r["prij"] if r["strana"].endswith("HOME") else r["kop"],
                                kolo=tah // 2 + 1, lezi=p.state != ST, zony=z, blok=max(bloky, default=None),
                                block_skill=kt.ma_skill(p, "Block"), uhyb=uhyb, akce=info["akce"],
                                lineman=kt.ma_skill(p, "Loner") is False and p.stats.strength <= 3))
        s = po
    return out


if __name__ == "__main__":
    ex = pickle.load(open(sys.argv[1], "rb"))
    hry = {}
    for kat, rs in ex.items():
        for r in rs:
            r = dict(r, kat=kat[0]); hry.setdefault((r["prij"], r["kop"], r["g"]), []).append(r)
    with mp.Pool(8) as pool: res = pool.map(hra, list(hry.values()), chunksize=1)
    res = [x for r in res for x in r]
    pickle.dump(res, open(sys.argv[2], "wb")); print("hotovo", len(res))
