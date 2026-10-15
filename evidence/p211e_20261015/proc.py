# P211e (15.10.2026): PROČ agilní (Dodge / AG 4+) po našem tahu stojí vedle soupeře. Uživatel: „proč tolik
# našich agilních končí u souseda, ať už začali u souseda nebo ne — můžeš rozlišit“. Jen měření, engine
# beze změny (main 2d76f8db+). Za každého takového hráče: začal v kontaktu? co v tahu dělal (makra),
# turnover v tahu? nese míč? Zápasy jako P211 (skaveni, lesní elfové × trpaslíci/orkové/lidé).
# proc.py KOREN vystup.pkl [semínek]
import sys, os, pickle, multiprocessing as mp, collections as C
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
NASI = ["skaven", "wood-elf"]; SOUPERI = ["dwarf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    prij, kop, nasi, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H if prij == nasi else A
    def dist(a, b): return max(abs(a.position.x - b.position.x), abs(a.position.y - b.position.y))
    def v_tz(st, p): return any(q.team_side != p.team_side and kt._stoji(q) and dist(p, q) == 1 for q in kt.hraci(st))
    agil = lambda p: p.team_side == my and (kt.ma_skill(p, "Dodge") or p.stats.agility >= 4)
    zaznamy = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nas = s.active_team == my
        if nas:
            start = {p.id: (kt._stoji(p) and v_tz(s, p)) for p in kt.hraci(s) if agil(p)}
            nosic0 = s.ball.carrier_id if s.ball.is_held else -1
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if nas:
            to = any(k["turnover"] for k in kroky)
            mic_nas = s.ball.is_held and s.get_player(s.ball.carrier_id).team_side == my
            makra = C.defaultdict(list); makro = None; cile = {}
            opp_nosic = s.get_player(s.ball.carrier_id) if (s.ball.is_held and s.get_player(s.ball.carrier_id).team_side != my) else None
            mic_volny = (not s.ball.is_held) and s.ball.position.x >= 0
            for k in kroky:
                if "macro" in k: makro = k["macro"]
                a_ = k["action"]
                if a_.player_id in start and makro:
                    t = (makro["type"], makro["source"])
                    if not makra[a_.player_id] or makra[a_.player_id][-1] != t: makra[a_.player_id].append(t)
                    cile.setdefault(a_.player_id, tuple(makro["target_pos"]))
            nosic1 = po.ball.carrier_id if po.ball.is_held else -1
            for p in kt.hraci(po):
                if not agil(p) or not kt._stoji(p) or not v_tz(po, p): continue
                cil = cile.get(p.id)
                vedle_nosice = bool(opp_nosic and cil and max(abs(cil[0] - opp_nosic.position.x), abs(cil[1] - opp_nosic.position.y)) == 1)
                vedle_mice = bool(mic_volny and cil and max(abs(cil[0] - s.ball.position.x), abs(cil[1] - s.ball.position.y)) <= 1)
                dosel = bool(cil and (p.position.x, p.position.y) == cil)
                zaznamy.append(dict(nasi=nasi, zacal_tz=start.get(p.id, False), makra=tuple(makra.get(p.id, ())),
                                    cil=cil, vedle_nosice=vedle_nosice, vedle_mice=vedle_mice, dosel=dosel,
                                    turnover=to, nosic=(p.id == nosic1), mic_nas=mic_nas))
        my_d = (po.home_team.score - s.home_team.score) if my == H else (po.away_team.score - s.away_team.score)
        th_d = (po.away_team.score - s.away_team.score) if my == H else (po.home_team.score - s.home_team.score)
        if my_d > 0 or th_d > 0: break
        s = po
    return zaznamy


if __name__ == "__main__":
    ukoly = [(p, k, n, g) for n in NASI for o in SOUPERI for p, k in ((n, o), (o, n)) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump([z for r in res for z in r], open(VYST, "wb")); print("hotovo", len(res))
