# P213c (18.10.): jako P213 noc, navíc proměnná P213OFF = vypnuté části P213 (1 = útěk před TD, 2 = útěk s GFI/úhybem).
# P213 (16.10.2026): předávka kvůli bezpečí míče. Dvě ramena: 0 = vše zapnuté, 1073741824 = P213 vypnuté. Měřidlo: míč ztracen / nosič sražen v tahu soupeře, makra HAND_OFF_SCORE.
# vypnutých úprav klece (bb.set_cage_features_off): 0 = vše zapnuté (dnešní stav) · 786432 = P180 vypnuté
# (kFeatReadyMate 262144 + kFeatScoreViaMate 524288) · 3145728 = P187 vypnuté (kFeatSafeRetreat + kFeatReleaseNeedsNoSlack).
# Jen útočné poločasy (my přijímáme), pět ras proti trpaslíkům / orkům / lidem; macro_mcts; semínka jako P211.
# Mechanismus: P180 — TD předávkou (HAND_OFF_SCORE), TD celkem, tah TD; P187 — náš nosič sražen v tahu soupeře,
# míč ztracen (soupeř ho drží nebo leží), turnovery. TD až nakonec.
# ab.py KOREN MASKA vystup.pkl [semínek]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; MASKA = int(sys.argv[2]); VYST = sys.argv[3]; N = int(sys.argv[4]) if len(sys.argv) > 4 else 10
NASI = ["skaven", "wood-elf", "dwarf", "orc", "human"]; SOUPERI = ["dwarf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    bb.set_cage_features_off(MASKA)
    bb.set_p213_parts_off(int(os.environ.get("P213OFF", "0")))
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H
    r = dict(nasi=nasi, sou=sou, g=g, tahu=0, td=0, td_tah=-1, td_predavkou=0, td_soupere=0, nosic_srazen=0,
             mic_ztracen=0, turnover=0, makra={}, td_posledni=0, posledni_tah_s_mic=0)
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nas_tah = s.active_team == my
        nosic = s.ball.carrier_id if s.ball.is_held else -1
        nosic_nas = nosic > 0 and any(p.id == nosic and p.team_side == my for p in kt.hraci(s))
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        if nas_tah:
            r["tahu"] += 1
            if s.home_team.turn_number >= 8 and nosic_nas: r["posledni_tah_s_mic"] += 1
            r["turnover"] += any(k["turnover"] for k in kroky)
            makro = None
            for k in kroky:
                if "macro" in k:
                    makro = k["macro"]; t = makro["type"]; r["makra"][t] = r["makra"].get(t, 0) + 1
                if any(e["type"] == "TOUCHDOWN" for e in k["events"]) and makro and makro["type"] == "HAND_OFF_SCORE":
                    r["td_predavkou"] += 1
        elif nosic_nas:
            if any(e["type"] == "KNOCKED_DOWN" and e["player"] == nosic for k in kroky for e in k["events"]):
                r["nosic_srazen"] += 1
            drzi_nas = po.ball.is_held and any(p.id == po.ball.carrier_id and p.team_side == my for p in kt.hraci(po))
            if not drzi_nas: r["mic_ztracen"] += 1
        my_score = po.home_team.score - s.home_team.score
        their = po.away_team.score - s.away_team.score
        if my_score > 0:
            r["td"] = 1; r["td_tah"] = tah
            if s.home_team.turn_number >= 8: r["td_posledni"] = 1
            break
        if their > 0: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(n, o, g) for n in NASI for o in SOUPERI for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
