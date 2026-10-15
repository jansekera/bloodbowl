# P156b (15.10.2026): proč AI blitz bez cíle volí zřídka; kola bez blitzu; gutter runneři v dosahu.
# Jen měření, engine se nemění. Hry jako P156 ab.py (skaveni přijímají i kopou proti 4 rasám, macro_mcts).
# proc.py KOREN vystup.pkl [semínek]
# Záznam: za každý tah skavenů stav na začátku tahu + každé rozhodnutí makra (nabídka s visits/q)
# + hody Wild Animal s makrem, ve kterém padly; za každý tah soupeře rány do gutter runnerů.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 12
SOUPERI = ["dwarf", "wood-elf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    WA = kt.SKILL["WildAnimal"]
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    sk = H if prij == "skaven" else A
    tahy = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        P = kt.hraci(s)
        nasi = [p for p in P if p.team_side == sk]; jejich = [p for p in P if p.team_side != sk]
        ogre = {p.id for p in nasi if kt.ma_skill(p, "WildAnimal")}
        gr = {p.id for p in nasi if p.stats.movement >= 9}
        def dist(a, b): return max(abs(a.position.x - b.position.x), abs(a.position.y - b.position.y))
        stoji_j = [q for q in jejich if kt._stoji(q)]
        hraci_info = {p.id: dict(stoji=kt._stoji(p), ma=p.stats.movement,
                                 d_soupere=min([dist(p, q) for q in stoji_j] or [99]),
                                 v_tz=any(dist(p, q) == 1 for q in stoji_j),
                                 dosah_soupere=sum(1 for q in stoji_j if dist(p, q) <= q.stats.movement + 3),
                                 ogre=p.id in ogre, gr=p.id in gr, mic=s.ball.is_held and s.ball.carrier_id == p.id)
                      for p in nasi}
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        t = dict(tah=tah, nasi=strana == sk, hraci=hraci_info, rozhodnuti=[], akce=[], wa=[])
        makro = None
        for k in kroky:
            if "macro" in k:
                m = k["macro"]; makro = dict(type=m["type"], player=m["player"], target=m["target"],
                                             declare_blitz=m["declare_blitz"], source=m["source"])
                t["rozhodnuti"].append(dict(makro, children=[dict(type=c["type"], player=c["player"], target=c["target"],
                                       declare_blitz=c["declare_blitz"], visits=c["visits"], q=c["q"], prior=c["prior"])
                                       for c in m["children"]]))
            a_ = k["action"]
            t["akce"].append((str(a_.type).split(".")[-1], a_.player_id, a_.target_id, k["turnover"]))
            for e in k["events"]:
                if e["type"] == "SKILL" and e["roll"] == WA:
                    t["wa"].append(dict(player=e["player"], success=e["success"], die=e["die1"], makro=makro))
        # po tahu: kde stojí gutter runneři (pro tah soupeře)
        Pp = kt.hraci(po); stoji_po = [q for q in Pp if q.team_side != sk and kt._stoji(q)]
        t["gr_po"] = {p.id: dict(v_tz=any(dist(p, q) == 1 for q in stoji_po),
                                 dosah=sum(1 for q in stoji_po if dist(p, q) <= q.stats.movement + 3),
                                 stoji=kt._stoji(p))
                      for p in Pp if p.team_side == sk and p.id in gr}
        tahy.append(t)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return dict(prij=prij, kop=kop, g=g, sk=str(sk), tahy=tahy)


if __name__ == "__main__":
    ukoly = [(p, k, g) for o in SOUPERI for p, k in (("skaven", o), (o, "skaven")) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
