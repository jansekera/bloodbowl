# P159 (16.10.2026): odklad riskantních akcí ZAPNUTÝ (riskDeferral) s opravou „riziko jen odsunout, ne zrušit“ × vypnutý.
# Mechanismus: propadlé aktivace = stojící naši hráči, kteří v tahu s turnoverem nehráli; turnover na tah; čas.
# Za náš tah: rozpad turnoveru podle (a) neúspěšné události v kroku s turnoverem, (b) makra, ve kterém nastal;
# a počty POKUSŮ o riskantní hody (každá událost se "success") podle typu — rozliší "méně pokusů" od "menší neúspěšnosti".
# Navíc počet aktivací (maker) za tah: kdyby AI jen méně hrála, klesne i tohle.
# ab.py KOREN vystup.pkl [semínek]
import sys, os, pickle, time, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 12
RISK = {"DODGE", "GFI", "PICKUP", "CATCH", "PASS", "HAND_OFF", "LEAP", "INTERCEPTION", "FOUL", "THROW_TEAMMATE", "STAND_UP"}
NASI = ["skaven", "wood-elf"]; SOUPERI = ["dwarf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, nasi, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H if prij == nasi else A
    r = dict(prij=prij, kop=kop, nasi=nasi, g=g, tahu=0, turnover=0, makra=0, propadle=0, cas=0.0, td=0, td_soupere=0, pokus={}, neuspech={}, to_udalost={}, to_makro={})
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nas_tah = s.active_team == my
        stojici = {p.id for p in kt.hraci(s) if p.team_side == my and kt._stoji(p)}
        t0 = time.time()
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if nas_tah: r["cas"] += time.time() - t0
        if nas_tah:
            r["tahu"] += 1
            makro = None
            nasi_ids = {p.id for p in kt.hraci(s) if p.team_side == my}
            for k in kroky:
                if "macro" in k: makro = k["macro"]; r["makra"] += 1
                for e in k["events"]:
                    if e["type"] in RISK or e["type"] == "BLOCK":
                        r["pokus"][e["type"]] = r["pokus"].get(e["type"], 0) + 1
                        if not e["success"]: r["neuspech"][e["type"]] = r["neuspech"].get(e["type"], 0) + 1
                if k["turnover"]:
                    r["turnover"] += 1
                    fail = [e["type"] for e in k["events"] if e["type"] in RISK and not e.get("success", True)]
                    nas_dole = any(e["type"] == "KNOCKED_DOWN" and e["player"] in nasi_ids for e in k["events"])
                    akce = str(k["action"].type).split(".")[-1]
                    u = fail[-1] if fail else ("nas_sražen:" + akce if nas_dole else "jine:" + akce)
                    r["to_udalost"][u] = r["to_udalost"].get(u, 0) + 1
                    m = (makro["type"] + "/" + makro["source"]) if makro else "bez_makra"
                    r["to_makro"][m] = r["to_makro"].get(m, 0) + 1
                    hrali = {k2["macro"]["player"] for k2 in kroky if "macro" in k2}
                    r["propadle"] += len(stojici - hrali)
                    break
        my_score = (po.home_team.score if my == H else po.away_team.score) - (s.home_team.score if my == H else s.away_team.score)
        their = (po.away_team.score if my == H else po.home_team.score) - (s.away_team.score if my == H else s.home_team.score)
        if my_score > 0: r["td"] = 1; break
        if their > 0: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, n, g) for n in NASI for o in SOUPERI for p, k in ((n, o), (o, n)) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
