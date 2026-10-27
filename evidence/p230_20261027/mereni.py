# P230 měření (27.10.2026): pomalejší klec u zdi bez povolené rány se přestaví po řadách (maska 4096).
# Ramena ze stejného stavu a semínka: "vse" (0), "bez229" (2048 = jen P228), "bez" (1024|2048 = jako main).
# Po každém rameni jen odpověď soupeře (jeden tah) — NIC DÁL; hra pokračuje ramenem "vse".
# Jen náš útok (soupeř kope). Měří se, co změna má dělat: prolomení zdi (blok předního rohu dopředu
# na soupeře dva sloupce před nosičem), posun míče vpřed, nosič sražen / míč ztracen v odpovědi,
# rohy (naši vedle nosiče) sraženi v odpovědi, turnover, TD.
import sys, os, pickle, time, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
DVOJICE = [("orc", "human"), ("orc", "wood-elf"), ("orc", "skaven"), ("human", "wood-elf"), ("human", "skaven"), ("dwarf", "human"), ("dwarf", "wood-elf"), ("dwarf", "skaven")]

RAMENA = {"vse": 0, "bez230": 4096}
sys.path.insert(0, KOREN)


def nosic_nas(po, H):
    if not po.ball.is_held or po.ball.carrier_id <= 0: return None
    c = po.get_player(po.ball.carrier_id)
    return c if c.team_side == H and c.is_on_pitch() else None


def rozbor(kt, bb, s0, kroky, po, k2, po2, H, A):
    c0 = nosic_nas(s0, H)
    dirx = 1 if H == bb.TeamSide.HOME else -1
    prolom = 0
    if c0 is not None:
        cx, cy = kt._xy(c0)
        for k in kroky:
            a = k["action"]
            if a.type.name not in ("BLOCK", "BLITZ"): continue
            p, t = s0.get_player(a.player_id), None
            try: t = s0.get_player(a.target_id)
            except IndexError: continue
            (px, py), (tx, ty) = kt._xy(p), kt._xy(t)
            if px == cx + dirx and abs(py - cy) == 1 and tx == cx + 2 * dirx and ty == py and a.type.name == "BLOCK":
                prolom += 1
    c1 = nosic_nas(po, H)
    hrozba = None
    if c1 is not None and po.phase == bb.GamePhase.PLAY and po.active_team == A:
        hrozba = bb.carrier_blitz_threat(po)        # předpověď řadiče: soupeř srazí nosiče / vezme míč
    posun = None
    radu = None
    if c0 is not None and c1 is not None:
        posun = (kt._xy(c1)[0] - kt._xy(c0)[0]) * dirx
        radu = int(kt._xy(c1)[0] == kt._xy(c0)[0] and kt._xy(c1)[1] != kt._xy(c0)[1])
    srazeni = {e["player"] for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN"} if k2 else set()
    rohy = []
    if c1 is not None:
        rohy = [q.id for q in kt.hraci(po) if q.team_side == H and q.id != c1.id and kt._ma_zony(q)
                and kt._vedle(kt._xy(q), kt._xy(c1))]
    mic_ztracen = None
    if c1 is not None:
        mic_ztracen = int(po2 is None or nosic_nas(po2, H) is None) if k2 is not None else 0
    return dict(
        mel_mic=int(c0 is not None), prolom=prolom, posun=posun, radu=radu, hrozba=hrozba,
        td=int(po.home_team.score + po.away_team.score != s0.home_team.score + s0.away_team.score),
        turnover=int(any(k["turnover"] for k in kroky)),
        nosic_srazen=int(c1 is not None and c1.id in srazeni), mic_ztracen=mic_ztracen,
        rohu=len(rohy), rohy_srazeny=len(set(rohy) & srazeni),
        nasi_srazeni=len(srazeni & {p.id for p in kt.hraci(po) if p.team_side == H}))


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
    bb.simple_kickoff(s, bb.DiceRoller(5000 + g))
    out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 90000 + 100 * g + tah
        if s.active_team == H:
            zaznam = dict(nasi=nasi, sou=sou, g=g, tah=tah, ramena={})
            for r, m in RAMENA.items():
                bb.set_extra_features_off(m)
                t0 = time.process_time()
                try:
                    kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
                finally:
                    bb.set_extra_features_off(0)
                cas = time.process_time() - t0
                k2 = po2 = None
                if po.phase == bb.GamePhase.PLAY and po.active_team == A:
                    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
                zaznam["ramena"][r] = rozbor(kt, bb, s, kroky, po, k2, po2, H, A)
                zaznam["ramena"][r]["cas"] = cas
                if r == "vse": dal = po
            out.append(zaznam)
            po = dal
        else:
            kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    print("hra", nasi, sou, g, len(out), flush=True)
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g) for n, o in DVOJICE for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo tahů", len(z))
