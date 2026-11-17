# P304 bod 2 (uživatel 17.11.2026 „ano“): makro-AI nabízí blok z lehu s Jump Up (hod AG +2, neúspěch není turnover).
# Měří se tam, kde mechanismus působí: na začátku našeho tahu leží náš hráč s Jump Up vedle stojícího soupeře.
# Párově po situacích (stejné semínko), ramena: vse (nové) · bez (bit kP304ProneBlock = dřívější nabídka jen stojícím).
# Jen náš tah + odpověď soupeře. Měří se: blok z lehu proběhl (pozitivní kontrola ⇒ v „bez“ musí být 0), hod Jump Up,
# turnover, soupeři na zemi po našem tahu (přírůstek), naši na zemi po tahu soupeře, TD soupeře.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 3
K = int(sys.argv[4]) if len(sys.argv) > 4 else 2
NASI = os.environ.get("NASI", "darkelf,norse").split(","); SOUPERI = ["orc", "human", "dwarf"]
RAMENA = {"vse": 0, "bez": 1 << 37}

sys.path.insert(0, KOREN)


def dole(bb, kt, s, side):
    return sum(1 for p in kt.hraci(s) if p.team_side == side and p.is_on_pitch() and p.state != bb.PlayerState.STANDING)


def lezici_ju(bb, kt, s, H):
    out = set()
    for p in kt.hraci(s):
        if p.team_side != H or p.state != bb.PlayerState.PRONE or not p.is_on_pitch(): continue
        if not kt.ma_skill(p, "JumpUp") or p.has_acted: continue
        for q in kt.hraci(s):
            if q.team_side != H and q.is_on_pitch() and q.state == bb.PlayerState.STANDING and \
                    max(abs(q.position.x - p.position.x), abs(q.position.y - p.position.y)) == 1:
                out.add(p.id); break
    return out


def jeden_tah(kt, bb, s0, rameno, seed, H, A, ids):
    bb.set_extra_features_off(RAMENA[rameno])
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    bb.set_extra_features_off(0)
    bloky = [k for k in kroky if k["action"].player_id in ids and
             k["action"].type in (bb.ActionType.BLOCK, bb.ActionType.MULTIPLE_BLOCK)]
    ju = [e for k in bloky for e in k["events"] if e["type"] == "STAND_UP" and e["player"] in ids]
    # pozitivní kontrola: makro BLOCK ležícího bylo v nabídce hledání (děti rozhodnutí)
    nab = sum(1 for k in kroky if "macro" in k for c in k["macro"]["children"]
              if c["type"] == "BLOCK" and c["player"] in ids)
    o = dict(nabidnuto=int(nab > 0), blok_z_lehu=len(bloky), ju_ok=sum(1 for e in ju if e["success"]), ju_hod=len(ju),
             turnover=int(any(k["turnover"] for k in kroky)),
             soup_dole=dole(bb, kt, po, A) - dole(bb, kt, s0, A), nasi_dole_po_soup=0, td_soupere=0)
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        o["nasi_dole_po_soup"] = dole(bb, kt, po2, H)
    else:
        o["nasi_dole_po_soup"] = dole(bb, kt, po, H)
    return o


def hra(a):
    nasi, sou, g, utok = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    kick = A if utok else H
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), kick)
    s.kicking_team = kick
    bb.simple_kickoff(s, bb.DiceRoller(5000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 70000 + 100 * g + tah + (50 if utok else 0)
        if s.active_team == H:
            ids = lezici_ju(bb, kt, s, H)
            if ids:
                sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, ramena={})
                for r in RAMENA:
                    sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A, ids)
                                        for j in range(K)]
                out.append(sit)
                print("situace", nasi, sou, g, tah, flush=True)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


def souhrn(sit):
    import math
    klice = ["nabidnuto", "blok_z_lehu", "ju_ok", "ju_hod", "turnover", "soup_dole", "nasi_dole_po_soup", "td_soupere"]
    print("situací", len(sit), "· běhů na rameno", sum(len(x["ramena"]["vse"]) for x in sit))
    for k in klice:
        a = [sum(o[k] for o in x["ramena"]["vse"]) / len(x["ramena"]["vse"]) for x in sit]
        b = [sum(o[k] for o in x["ramena"]["bez"]) / len(x["ramena"]["bez"]) for x in sit]
        d = [p - q for p, q in zip(a, b)]
        n = len(d); m = sum(d) / n if n else 0.0
        sd = math.sqrt(sum((v - m) ** 2 for v in d) / (n - 1)) if n > 1 else 0.0
        print(f"{k:18s} vse {sum(a)/max(n,1):.3f}  bez {sum(b)/max(n,1):.3f}  rozdíl {m:+.3f} ± {sd/math.sqrt(max(n,1)):.3f} (1σ)")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--souhrn":
        souhrn(pickle.load(open(sys.argv[2], "rb"))); sys.exit()
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    if len(sys.argv) > 5: ukoly = ukoly[:int(sys.argv[5])]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit)); souhrn(sit)
