# P211 (15.10.2026): párové A/B — agilní hráč (Dodge nebo AG 4+) v kontaktu dostane přesun pryč z dosahu.
# Naši: skaveni a lesní elfové; soupeři trpaslíci, orkové, lidé; přijímáme i kopeme; macro_mcts, semínka jako P196.
# Předregistrace (kniha P211): mechanismus napřed — kolik agilních zůstane po našem tahu v TZ, kolik jich
# soupeř bije, kolik útěků se zahraje a kolik z nich skončí turnoverem (úhyb), zranění agilních. TD až nakonec.
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
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H if prij == nasi else A
    def dist(a, b): return max(abs(a.position.x - b.position.x), abs(a.position.y - b.position.y))
    def v_tz(st, p): return any(q.team_side != p.team_side and kt._stoji(q) and dist(p, q) == 1 for q in kt.hraci(st))
    agil = lambda p: p.team_side == my and (kt.ma_skill(p, "Dodge") or p.stats.agility >= 4)
    r = dict(prij=prij, kop=kop, nasi=nasi, g=g, tahu=0, agil_po=0, agil_po_tz=0, zacal_tz=0, zacal_tz_odesel=0,
             utek_tahu=0, utek_turnover=0, bit=0, bit_v_tz=0, srazen=0, zraneni=0, turnover=0, td=0, td_soupere=0)
    posledni_v_tz = set()
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nas_tah = s.active_team == my
        start_tz = {p.id for p in kt.hraci(s) if agil(p) and kt._stoji(p) and v_tz(s, p)} if nas_tah else set()
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if nas_tah:
            r["tahu"] += 1
            to = any(k["turnover"] for k in kroky); r["turnover"] += to
            makro = None
            for k in kroky:
                if "macro" in k: makro = k["macro"]
                a_ = k["action"]
                for e in k["events"]:
                    if e["type"] == "DODGE" and not e["success"] and makro and makro["type"] == "REPOSITION" \
                            and makro["player"] in start_tz and e["player"] == makro["player"] and k["turnover"]:
                        r["utek_turnover"] += 1
            utekli = {k["macro"]["player"] for k in kroky if "macro" in k and k["macro"]["type"] == "REPOSITION"
                      and k["macro"]["player"] in start_tz}
            r["utek_tahu"] += len(utekli)
            Pp = {p.id: p for p in kt.hraci(po)}
            r["zacal_tz"] += len(start_tz)
            r["zacal_tz_odesel"] += sum(1 for pid in start_tz if pid in Pp and kt._stoji(Pp[pid]) and not v_tz(po, Pp[pid]))
            posledni_v_tz = set()
            for p in Pp.values():
                if agil(p) and kt._stoji(p):
                    r["agil_po"] += 1
                    if v_tz(po, p): r["agil_po_tz"] += 1; posledni_v_tz.add(p.id)
        else:
            agil_ids = {p.id for p in kt.hraci(s) if agil(p)}
            for k in kroky:
                a_ = k["action"]
                if a_.type in (bb.ActionType.BLOCK, bb.ActionType.BLITZ) and a_.target_id in agil_ids:
                    r["bit"] += 1; r["bit_v_tz"] += a_.target_id in posledni_v_tz
                for e in k["events"]:
                    if e["type"] == "KNOCKED_DOWN" and e["player"] in agil_ids: r["srazen"] += 1
                    if e["type"] in ("CASUALTY",) and e["player"] in agil_ids: r["zraneni"] += 1
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
