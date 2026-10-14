# P207 (14.10.2026): sloty na linii podle role — párové A/B (báze 1ff3a39e × e9edbe35).
# Stejné poločasy jako td_vse.py (klec_20261007): 5 ras TV1500, každá přijímá proti každé,
# macro_mcts, semínka 3000+g / 50000+100g+tah. Navíc MECHANISMUS (předregistrace P207):
#   - kdo stojí na linii po rozestavení (role: lineman / specialista / ST4 / big guy),
#   - KO a zranění během poločasu podle role a strany (přijímající / kopající),
#   - totéž jen v prvních dvou tazích drivu (úvodní bloky proti linii).
# mech.py KOREN vystup.pkl [semínek]
import sys, os, pickle, itertools, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 40
RASY = ["dwarf", "wood-elf", "skaven", "orc", "human"]
LINEMAN = {"human": (6, 3, 3, 8), "orc": (5, 3, 3, 9), "dwarf": (4, 3, 2, 9), "skaven": (7, 3, 3, 7),
           "wood-elf": (7, 3, 4, 7)}
sys.path.insert(0, KOREN)


def role(p, rasa, kt):
    st = (p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour)
    if kt.ma_skill(p, "Loner"):
        return "bigguy"
    if p.stats.strength >= 4:
        return "st4"
    if st == LINEMAN[rasa] and not kt.ma_skill(p, "SureHands") and not kt.ma_skill(p, "Pass"):
        return "lineman"
    return "spec"


def hra(a):
    prij, kop, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij, 1500), bb.get_developed_roster(kop, 1500), A)
    s.kicking_team = A
    rasa = {H: prij, A: kop}
    r = dict(prij=prij, kop=kop, g=g, td=0, td_soupere=0, tahu=0, to=0,
             linie={"H": {}, "A": {}}, zraneni={}, zraneni_uvod={})
    for p in kt.hraci(s):
        x = 12 if p.team_side == H else 13
        if p.position.x == x:
            k = "H" if p.team_side == H else "A"
            ro = role(p, rasa[p.team_side], kt)
            r["linie"][k][ro] = r["linie"][k].get(ro, 0) + 1
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        strana = s.active_team
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        for k in kroky:
            for e in k["events"]:
                ko = e["type"] == "INJURY" and 8 <= e["roll"] <= 9
                if not (ko or e["type"] == "CASUALTY"): continue
                p = s.get_player(e["player"])
                klic = ("H" if p.team_side == H else "A") + ":" + role(p, rasa[p.team_side], kt)
                r["zraneni"][klic] = r["zraneni"].get(klic, 0) + 1
                if tah < 2: r["zraneni_uvod"][klic] = r["zraneni_uvod"].get(klic, 0) + 1
        if strana == H:
            r["tahu"] += 1; r["to"] += any(k["turnover"] for k in kroky)
        if po.home_team.score > s.home_team.score: r["td"] = 1; break
        if po.away_team.score > s.away_team.score: r["td_soupere"] = 1; break
        s = po
    return r


if __name__ == "__main__":
    ukoly = [(p, k, g) for p, k in itertools.permutations(RASY, 2) for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "8"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo", len(res))
