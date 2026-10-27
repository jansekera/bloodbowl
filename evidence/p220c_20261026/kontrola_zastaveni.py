# Pozitivní kontrola P220 (27.10.2026): kolikrát P220 ve skutečně hraném tahu zastavil přesun agilního hráče.
# Táž sada a semínka jako mereni.py, jen rameno "vse" (maska 0), bez odpovědi soupeře navíc.
import sys, os, multiprocessing as mp
KOREN = sys.argv[1]; N = int(sys.argv[2]) if len(sys.argv) > 2 else 6
sys.path.insert(0, KOREN)
NASI = ["wood-elf", "skaven"]; SOUPERI = ["dwarf", "orc", "human"]

def hra(a):
    nasi, sou, g, utok = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    kick = A if utok else H
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), kick)
    s.kicking_team = kick
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    tahu = zast = tahu_se_zast = 0
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 80000 + 100 * g + tah + (50 if utok else 0)
        bb.take_p220_stops()
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        z = bb.take_p220_stops()
        if s.active_team == H:
            tahu += 1; zast += z; tahu_se_zast += int(z > 0)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return (nasi, tahu, zast, tahu_se_zast)

if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    for n in NASI:
        r = [x for x in res if x[0] == n]
        print(n, "tahů", sum(x[1] for x in r), "zastavení", sum(x[2] for x in r), "tahů se zastavením", sum(x[3] for x in r))
