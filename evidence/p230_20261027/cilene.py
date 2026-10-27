# P230 cílené měření (27.10.2026): náhodné běžné hry situaci „pomalejší klec před zdí“ skoro nemají
# (souhrn.md: P230 0× z 296 tahů). Pozice podle partie zed-orkove: klec kolem nosiče, zeď 5 soupeřů o dva
# sloupce před ním, zbytek našich vzadu, zbytek soupeřů za zdí. Ramena ze stejného stavu a semínka:
# "vse" (0), "bez230" (4096), "bez_zalohy" (8192 = P230 záloha vzadu vypnutá). Po rameni jen odpověď soupeře.
import sys, os, pickle, random, time, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3])
DVOJICE = [("orc", "human"), ("orc", "skaven"), ("orc", "wood-elf"), ("dwarf", "human"), ("dwarf", "skaven"), ("dwarf", "wood-elf")]
RAMENA = {"vse": 0, "bez230": 4096, "bez_zalohy": 8192}
sys.path.insert(0, KOREN)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def pozice(bb, kt, nasi, sou, g):
    rng = random.Random(1000 * g + sum(map(ord, nasi + sou)))
    s = bb.GameState()
    bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), bb.TeamSide.AWAY)
    H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    my, op = [], []
    for pid in range(0, 40):
        try: p = s.get_player(pid)
        except IndexError: continue
        if p.team_side == H: my.append(p)
        else: op.append(p)
        p.position = bb.Position(-1, -1); p.state = bb.PlayerState.OFF_PITCH
    my, op = my[:11], op[:11]
    cx, cy = rng.randint(8, 13), rng.randint(4, 10)
    blk = lambda p: kt.ma_skill(p, "Block")
    nosic = next((p for p in my if kt.ma_skill(p, "SureHands")), min(my, key=lambda p: (blk(p), p.stats.movement)))
    zbyt = [p for p in my if p.id != nosic.id]
    zbyt.sort(key=lambda p: (blk(p), p.stats.movement))          # přední rohy bez Block, zadní pomalí
    predni, zbyt = zbyt[:2], zbyt[2:]
    zbyt.sort(key=lambda p: p.stats.movement)
    zadni, volni = zbyt[:2], zbyt[2:]
    obsaz = {}
    def dej(p, x, y):
        p.position = bb.Position(x, y); p.state = bb.PlayerState.STANDING
        p.movement_remaining = p.stats.movement; p.has_moved = False; p.has_acted = False
        obsaz[(x, y)] = p
    dej(nosic, cx, cy)
    dej(predni[0], cx + 1, cy - 1); dej(predni[1], cx + 1, cy + 1)
    dej(zadni[0], cx - 1, cy - 1); dej(zadni[1], cx - 1, cy + 1)
    op.sort(key=lambda p: -p.stats.movement)
    for i, p in enumerate(op[:5]): dej(p, cx + 2, cy - 2 + i)
    for p in op[5:]:
        while True:
            x, y = rng.randint(cx + 4, min(24, cx + 10)), rng.randint(1, 13)
            if (x, y) not in obsaz: dej(p, x, y); break
    for p in volni:
        while True:
            x, y = rng.randint(max(1, cx - 8), cx - 2), rng.randint(1, 13)
            if (x, y) not in obsaz: dej(p, x, y); break
    s.ball.position = nosic.position; s.ball.is_held = True; s.ball.carrier_id = nosic.id
    s.kicking_team = A; s.active_team = H; s.phase = bb.GamePhase.PLAY; s.half = 1
    kolo = rng.randint(2, 6)
    s.home_team.turn_number = kolo; s.away_team.turn_number = kolo - 1
    return s


def zaloha(kt, po, H, c1):
    """Kolik našich stojí 2+ sloupce za nosičem mimo zóny soupeře a kolik řad 1–13 jejich blitz nepokryje."""
    if c1 is None: return None, None
    cx, cy = kt._xy(c1)
    sou = [o for o in kt.hraci(po) if o.team_side != H and kt._ma_zony(o)]
    z = [q for q in kt.hraci(po) if q.team_side == H and q.id != c1.id and kt._ma_zony(q) and cx - kt._xy(q)[0] >= 2
         and not any(kt._vedle(kt._xy(q), kt._xy(o)) for o in sou)]
    nekryto = sum(1 for y in range(1, 14)
                  if not any(max(abs(kt._xy(q)[0] - cx), abs(kt._xy(q)[1] - y)) <= q.stats.movement + 1 for q in z))
    return len(z), nekryto


def ukol(a):
    try:
        return _ukol(a)
    except Exception as e:
        print("CHYBA", a, repr(e), flush=True)
        return None


def _ukol(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    import mereni
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = pozice(bb, kt, nasi, sou, g)
    seed = 50000 + 100 * g
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
        x = mereni.rozbor(kt, bb, s, kroky, po, k2, po2, H, A)
        x["cas"] = cas
        x["zaloha"], x["nekryto"] = zaloha(kt, po, H, mereni.nosic_nas(po, H))
        z["ramena"][r] = x
    print("pozice", nasi, sou, g, flush=True)
    return z


if __name__ == "__main__":
    ukoly = [(n, o, g) for n, o in DVOJICE for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6"))) as pool: res = pool.map(ukol, ukoly, chunksize=1)
    z = [x for x in res if x]
    pickle.dump(z, open(VYST, "wb")); print("hotovo pozic", len(z))
