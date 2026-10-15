# P196 (15.10.2026): rozdělení tahů „no_actions“ podle příčiny. Přehraje poločasy z main.pkl, které
# měly no_actions (stejná semínka jako mereni.py), a v okamžiku no_actions uloží:
#   - nabídku hledání (children: typ, hráč, návštěvy, q) — prázdná nabídka × odmítnuté rozbalení,
#   - stav těsně před ním (přehráním akcí tahu se stejnými kostkami jako ai_plan_turn; kontrola: události se shodují),
#   - pro každého hráče, který ještě nehrál: jaké primitivní akce má k dispozici.
# pricina.py vystup.pkl
import sys, os, pickle, multiprocessing as mp
KOREN = "/home/jenda/claude/blood-bowl"; sys.path.insert(0, KOREN)
VYST = sys.argv[1]


def hra(a):
    prij, kop, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        seed = 50000 + 100 * g + tah
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        mohli = [p.id for p in kt.hraci(s) if p.team_side == strana and p.state in (bb.PlayerState.STANDING, bb.PlayerState.PRONE)]
        rep = s.clone(); d = bb.DiceRoller((seed ^ 0x5bd1e995) & 0xffffffff)
        hrali = set(); shoda = True
        for i, k in enumerate(kroky):
            m = k.get("macro")
            if m is not None and m["source"] == "no_actions":
                av = list(bb.get_available_actions(rep))
                neh = {}
                for pid in mohli:
                    if pid in hrali: continue
                    p = rep.get_player(pid)
                    neh[pid] = dict(stav=str(p.state), pos=(p.position.x, p.position.y),
                                    ma_mic=(rep.ball.is_held and rep.ball.carrier_id == pid) if hasattr(rep.ball, "carrier_id") else None,
                                    akce=sorted({str(x.type).split(".")[-1] for x in av if x.player_id == pid}))
                out.append(dict(prij=prij, kop=kop, g=g, tah=tah, krok=i, strana=str(strana), shoda=shoda,
                                children=[(c["type"], c["player"], c["visits"], round(c["q"], 3)) for c in m["children"]],
                                nehrali=neh, typy_akci=sorted({str(x.type).split(".")[-1] for x in av})))
            act = k["action"]
            if act.player_id is not None and act.player_id > 0: hrali.add(act.player_id)
            _, ev = bb.execute_action_logged(rep, act, d)
            if repr(ev) != repr(k["events"]): shoda = False
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    R = pickle.load(open(os.path.join(KOREN, "evidence/p196_20261015/main.pkl"), "rb"))
    ukoly = [(r["prij"], r["kop"], r["g"]) for r in R if r["no_actions"]]
    with mp.Pool(8) as pool: res = pool.map(hra, ukoly, chunksize=1)
    res = [x for r in res for x in r]
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
