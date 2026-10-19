# P213d (19.10.2026): útěk nosiče přímo v situacích, kde nastal — jen rychlé týmy.
# Uživatel 19.10.: „útěk má sloužit pro případ, kdy už klec nestihne dojít … nový stav došel s rizikem
#   turnover a pádu a míč je mimo dosah soupeře pro možnost TD v dalším kole“; „porovnej přímo konkrétní
#   situace samostatně“; „má pomoci jen rychlým týmům“.
# Postup: hra se vším zapnutým (skaven, wood-elf × dwarf/orc/human, útočný poločas). V našem tahu, kde
#   řadič klece zvolil útěk (HAND_OFF_SCORE na pole mimo zónu), se stav uloží a z něj se ten tah zahraje
#   K-krát v každém rameni se stejnými semínky: vse = útěk (dnešní), bez_hodu = útěk jen bez GFI/úhybu,
#   bez_uteku = P213 vypnuté (kFeatSafetyHandOff). Pak tah soupeře a náš další tah, vše zapnuté.
# Měří se (mechanismus): turnover v tom tahu; míč náš po tahu soupeře; nosič sražen; TD náš v témž
#   nebo dalším tahu; TD soupeře.
# situace.py KOREN vystup.pkl [semínek] [K]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 10
K = int(sys.argv[4]) if len(sys.argv) > 4 else 8
NASI = ["skaven", "wood-elf"]; SOUPERI = ["dwarf", "orc", "human"]
RAMENA = {"vse": (0, 0), "bez_hodu": (0, 2), "bez_uteku": (1073741824, 0)}
sys.path.insert(0, KOREN)


def nastav(bb, r): bb.set_cage_features_off(RAMENA[r][0]); bb.set_extra_features_off(RAMENA[r][1])


def utek(kroky):
    for k in kroky:
        m = k.get("macro")
        if m and m["type"] == "HAND_OFF_SCORE" and m["source"] == "cage" and m["target_pos"][0] != 25: return m
    return None


def drzi(kt, s, my):
    return s.ball.is_held and any(p.id == s.ball.carrier_id and p.team_side == my for p in kt.hraci(s))


def dohraj(kt, bb, s0, rameno, seed, my):
    """Náš tah v rameni, pak soupeř a náš další tah se vším zapnutým."""
    o = dict(turnover=0, utek=0, mic_nas_po_souperi=0, nosic_srazen=0, td=0, td_hned=0, td_soupere=0)
    s = s0
    for i in range(3):
        if s.phase != bb.GamePhase.PLAY: break
        nastav(bb, rameno if i == 0 else "vse")
        nosic = s.ball.carrier_id if s.ball.is_held else -1
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed + i)
        if i == 0:
            o["turnover"] = int(any(k["turnover"] for k in kroky)); o["utek"] = int(utek(kroky) is not None)
        if i == 1:
            o["nosic_srazen"] = int(any(e["type"] == "KNOCKED_DOWN" and e["player"] == nosic for k in kroky for e in k["events"]))
            o["mic_nas_po_souperi"] = int(drzi(kt, po, my))
        if po.home_team.score > s.home_team.score:
            o["td"] = 1; o["td_hned"] = int(i == 0); break
        if po.away_team.score > s.away_team.score: o["td_soupere"] = 1; break
        s = po
    if o["td"] and o["td_hned"] == 0: o["mic_nas_po_souperi"] = 1
    return o


def hra(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H; out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        nastav(bb, "vse")
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
        m = utek(kroky) if s.active_team == my else None
        if m:
            c = next(p for p in kt.hraci(s) if p.id == s.ball.carrier_id)
            sit = dict(nasi=nasi, sou=sou, g=g, tah=tah, kolo=s.home_team.turn_number, nosic=(c.position.x, c.position.y),
                       do_zony=25 - c.position.x, cil=m["target_pos"], ramena={})
            for r in RAMENA:
                sit["ramena"][r] = [dohraj(kt, bb, s, r, 70000 + 1000 * g + 10 * tah + 100000 * j, my) for j in range(K)]
            out.append(sit)
            print("situace", nasi, sou, g, tah, {r: sum(x["mic_nas_po_souperi"] for x in v) for r, v in sit["ramena"].items()}, flush=True)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g) for n in NASI for o in SOUPERI for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
