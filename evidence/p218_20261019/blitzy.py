# P218 (19.10.2026): elfové a zbytečný blitz — jen popis, žádná úprava.
# Uživatel 19.10.: „za elfy není nutné blitzovat každé kolo“ … „ano — změř to po P217“.
# wood-elf proti dwarf/orc/human, útočné i obranné poločasy, vše zapnuté. U každého našeho blitzu:
#   cíl = nosič míče soupeře? · cíl sražen · blitzující sražen už v našem tahu · turnover v tahu ·
#   blitzující na konci tahu stojí vedle stojícího soupeře (kontakt) · blitzující sražen v příštím tahu soupeře.
# blitzy.py KOREN vystup.pkl [semínek]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 10
SOUPERI = ["dwarf", "orc", "human"]
sys.path.insert(0, KOREN)


def hra(a):
    sou, g, utok = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    kick = A if utok else H
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster("wood-elf", 1500), bb.get_developed_roster(sou, 1500), kick)
    s.kicking_team = kick
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    out = []; nase_tahy = 0
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nas = s.active_team == H
        nosic0 = s.ball.carrier_id if s.ball.is_held else -1
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=60000 + 100 * g + tah + (50 if utok else 0))
        if nas:
            nase_tahy += 1
            bl = [p.id for p in kt.hraci(po) if p.team_side == H and p.used_blitz]
            if bl:
                b = bl[0]
                cil = next((k["action"].target_id for k in kroky if k["action"].type == bb.ActionType.BLITZ and k["action"].player_id == b and k["action"].target_id > 0), -1)
                if cil < 0:
                    cil = next((k["action"].target_id for k in kroky if k["action"].type == bb.ActionType.BLOCK and k["action"].player_id == b), -1)
                down = lambda pid, ks: any(e["type"] == "KNOCKED_DOWN" and e["player"] == pid for k in ks for e in k["events"])
                pb = next(p for p in kt.hraci(po) if p.id == b)
                kontakt = pb.state == bb.PlayerState.STANDING and any(
                    p.team_side == A and p.state == bb.PlayerState.STANDING and max(abs(p.position.x - pb.position.x), abs(p.position.y - pb.position.y)) == 1
                    for p in kt.hraci(po))
                r = dict(sou=sou, g=g, utok=utok, tah=tah, kolo=s.home_team.turn_number, cil=cil,
                         cil_nosic=int(cil > 0 and cil == nosic0), cil_srazen=int(cil > 0 and down(cil, kroky)),
                         blitzer_srazen_hned=int(down(b, kroky)), turnover=int(any(k["turnover"] for k in kroky)),
                         kontakt=int(kontakt), srazen_souperem=0)
                if po.phase == bb.GamePhase.PLAY and po.active_team == A:
                    k2, _, _ = kt.plan_tahu(po, ai="macro_mcts", seed=61000 + 100 * g + tah)
                    r["srazen_souperem"] = int(down(b, k2))
                out.append(r)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score:
            break
        s = po
    return dict(blitzy=out, nase_tahy=nase_tahy, sou=sou, utok=utok)


if __name__ == "__main__":
    ukoly = [(o, g, u) for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST, "wb")); print("hotovo her", len(res))
