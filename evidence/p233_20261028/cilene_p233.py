# P233 cílené měření (28.10.2026): „pomalejší klec před zdí, která nesmí bít“ — týmy BEZ Block (nemrtví, Nurgle,
# Chaos) proti rychlejším. Cílené měření P230 (orkové, trpaslíci) tu situaci nemělo: Block hráči smějí bít i na
# 1 kostku (P229), takže P230/P233 nenastaly. Pozice z ../p230_20261027/cilene.py (pozice()).
# Ramena ze stejného stavu a semínka: "vse" (0), "bez_p233" (131072), "bez_p233_230" (131072|4096 = ani posun po řadách).
# Po rameni jen odpověď soupeře (jeden tah) — NIC DÁL.
import sys, os, pickle, time, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3])
D = os.path.dirname(os.path.abspath(__file__))
DVOJICE = [(n, o) for n in ("undead", "nurgle", "chaos") for o in ("human", "skaven", "wood-elf")]
RAMENA = {"vse": 0, "bez_p233": 131072, "bez_p233_230": 131072 | 4096}
sys.path.insert(0, KOREN)
sys.path.insert(0, os.path.join(D, "..", "p230_20261027"))   # cilene.pozice
sys.path.insert(0, os.path.join(D, "..", "p232_20261028"))   # mereni.rozbor (tamní)


def zed(kt, s, po, H):
    """Kolik soupeřů se zónami stojí po našem tahu ve sloupci zdi (2 před původním nosičem, řádky ±3)."""
    import mereni as m232
    c0 = m232.nosic_nas(s, H)
    if c0 is None: return None
    cx, cy = kt._xy(c0)
    wx = cx + 2
    return sum(1 for o in kt.hraci(po) if o.team_side != H and kt._ma_zony(o)
               and kt._xy(o)[0] == wx and abs(kt._xy(o)[1] - cy) <= 3)


def ukol(a):
    try:
        return _ukol(a)
    except Exception as e:
        print("CHYBA", a, repr(e), flush=True)
        return None


def _ukol(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    import cilene
    import mereni as m232
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = cilene.pozice(bb, kt, nasi, sou, g)
    seed = 70000 + 100 * g
    z = dict(nasi=nasi, sou=sou, g=g, ramena={})
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
        x = m232.rozbor(kt, bb, s, kroky, po, k2, po2, H, A)
        x["cas"] = cas
        x["zed"] = zed(kt, s, po, H)
        x["ran"] = sum(1 for k in kroky if k["action"].type.name in ("BLOCK", "BLITZ"))
        z["ramena"][r] = x
    print("pozice", nasi, sou, g, flush=True)
    return z


if __name__ == "__main__":
    ukoly = [(n, o, g) for n, o in DVOJICE for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(ukol, ukoly, chunksize=1)
    z = [x for x in res if x]
    pickle.dump(z, open(VYST, "wb")); print("hotovo pozic", len(z))
    if len(z) < len(ukoly):
        print("CHYBY:", len(ukoly) - len(z), "pozic"); sys.exit(1)
