# P219 (19.10.2026): blitz agilního hráče podle šance srazit × odskočit — jen v tahu, kde rozhodne.
# Uživatel: „při kontrole testuj jen tu jednu úpravu“; „počítej jen aktuální kolo a pak odpověď oponenta“.
# wood-elf, skaven × dwarf/orc/human, útok i obrana. V našem tahu se plán s P219 a bez něj (stejné semínko)
#   porovná: liší se blitz (kdo / zda) ⇒ situace; tah K-krát v obou ramenech, pak jen tah soupeře.
# Měří se: soupeřů sražených v našem tahu · našich sražených v tahu soupeře · blitzující sražen soupeřem ·
#   turnover · naši v kontaktu na konci tahu · míč (náš držitel) přežil.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 10
K = int(sys.argv[4]) if len(sys.argv) > 4 else 8
NASI = ["wood-elf", "skaven"]; SOUPERI = ["dwarf", "orc", "human"]
RAMENA = {"vse": 0, "bez_p219": 32}
sys.path.insert(0, KOREN)


def blitzer(kt, po, H):
    b = [p.id for p in kt.hraci(po) if p.team_side == H and p.used_blitz]
    return b[0] if b else -1


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    bb.set_extra_features_off(RAMENA[rameno])
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    bb.set_extra_features_off(0)
    b = blitzer(kt, po, H)
    down = lambda side, ks: sum(1 for k in ks for e in k["events"] if e["type"] == "KNOCKED_DOWN"
                                 and any(p.id == e["player"] and p.team_side == side for p in kt.hraci(s0)))
    kontakt = sum(1 for p in kt.hraci(po) if p.team_side == H and p.state == bb.PlayerState.STANDING and any(
        q.team_side == A and q.state == bb.PlayerState.STANDING and max(abs(q.position.x - p.position.x), abs(q.position.y - p.position.y)) == 1
        for q in kt.hraci(po)))
    o = dict(blitz=int(b > 0), souperi_srazeni=down(A, kroky), turnover=int(any(k["turnover"] for k in kroky)),
             kontakt=kontakt, nasi_srazeni_souperem=0, blitzer_srazen=0)
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        k2, _, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        o["nasi_srazeni_souperem"] = down(H, k2)
        o["blitzer_srazen"] = int(b > 0 and any(e["type"] == "KNOCKED_DOWN" and e["player"] == b for k in k2 for e in k["events"]))
    return o


def hra(a):
    nasi, sou, g, utok = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    kick = A if utok else H
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), kick)
    s.kicking_team = kick
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 80000 + 100 * g + tah + (50 if utok else 0)
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if s.active_team == H:
            bb.set_extra_features_off(32)
            _, po0, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
            bb.set_extra_features_off(0)
            if blitzer(kt, po, H) != blitzer(kt, po0, H):
                sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, s_p219=blitzer(kt, po, H), bez=blitzer(kt, po0, H), ramena={})
                for r in RAMENA:
                    sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A) for j in range(K)]
                out.append(sit)
                print("situace", nasi, sou, g, tah, sit["s_p219"], sit["bez"], flush=True)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
