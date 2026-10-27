# Kalibrace hrozby (27.10.2026, uživatel: „to číslo je odhad — zkus k tomu doplnit delší měření, co to upřesní —
# hodnoty, se kterými to porovnáváme, jsou dány hody kostek“).
# Hraje se náš útok (soupeř kope). Po každém našem tahu s míčem: předpověď řadiče `carrier_blitz_threat`,
# pak K odpovědí soupeře ZE STEJNÉHO STAVU s různým semínkem (kostky) ⇒ četnost „nosič sražen“ / „míč ztracen“
# pro ten jeden stav. Jen náš tah + odpověď soupeře, nic dál; hra pokračuje první odpovědí.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]); K = int(sys.argv[4])
DVOJICE = [("orc", "human"), ("orc", "wood-elf"), ("orc", "skaven"), ("human", "wood-elf"), ("human", "skaven"),
           ("dwarf", "human"), ("dwarf", "wood-elf"), ("dwarf", "skaven"), ("chaos", "human"), ("lizardmen", "skaven")]
sys.path.insert(0, KOREN)


def nosic_nas(po, H):
    if not po.ball.is_held or po.ball.carrier_id <= 0: return None
    c = po.get_player(po.ball.carrier_id)
    return c if c.team_side == H and c.is_on_pitch() else None


def hra(a):
    try:
        return _hra(a)
    except Exception as e:
        print("CHYBA", a, repr(e), flush=True)
        return []


def _hra(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(7000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 70000 + 100 * g + tah
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        if s.active_team == H and po.phase == bb.GamePhase.PLAY and po.active_team == A:
            c1 = nosic_nas(po, H)
            if c1 is not None:
                z = dict(nasi=nasi, sou=sou, g=g, tah=tah, hrozba=bb.carrier_blitz_threat(po), odpovedi=[])
                dal = None
                for j in range(K):
                    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1 + 1000 * j)
                    srazeni = {e["player"] for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN"}
                    z["odpovedi"].append(dict(nosic_srazen=int(c1.id in srazeni),
                                              mic_ztracen=int(nosic_nas(po2, H) is None)))
                    if j == 0: dal = po2
                out.append(z)
                if dal.home_team.score != po.home_team.score or dal.away_team.score != po.away_team.score: break
                s = dal
                continue
        s = po
    print("hra", nasi, sou, g, len(out), flush=True)
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g) for n, o in DVOJICE for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo stavů", len(z))
