# P233 v běžných hrách (29.10.2026): jak často řadič dojde k zdi před klecí (P229) a k čištění zdi od kraje (P233),
# a jestli P233 (s řetězem jen od klece, commit 43e08d71) změní tah. Týmy bez Block (jako cílené měření
# ../p233_20261028) × rychlejší soupeři, soupeř kope, 16 tahů.
# Ramena ze stejného stavu a semínka: "vse" (0), "bez_p233" (131072). Po každém rameni jen odpověď soupeře
# (jeden tah) — NIC DÁL; hra pokračuje ramenem "vse".
# Dosah se čte ze stopy řadiče (BB_CAGE_DEBUG, stderr přesměrovaný do souboru procesu): řádky "P229 zeď před klecí",
# "P233 zeď od kraje: blok/blitz" (rána) a "P233 ... nejde".
import sys, os, pickle, time, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
DVOJICE = [(n, o) for n in ("undead", "nurgle", "chaos") for o in ("human", "skaven", "wood-elf")]
RAMENA = {"vse": 0, "bez_p233": 131072}
STOPA = os.path.join(os.path.dirname(os.path.abspath(VYST)), "stopa")
sys.path.insert(0, KOREN)
os.environ["BB_CAGE_DEBUG"] = "1"


def nosic_nas(po, H):
    if not po.ball.is_held or po.ball.carrier_id <= 0: return None
    c = po.get_player(po.ball.carrier_id)
    return c if c.team_side == H and c.is_on_pitch() else None


def stopa_od(poz):
    """Řádky stopy řadiče zapsané od pozice poz (fd 2 je soubor procesu)."""
    sys.stderr.flush()
    with open(f"{STOPA}/{os.getpid()}.log", errors="replace") as f:
        f.seek(poz); return f.read().splitlines()


def pozice():
    sys.stderr.flush(); return os.lseek(2, 0, os.SEEK_CUR)


def pocty(radky):
    return dict(p229=sum("P229 zeď před klecí" in r for r in radky),
                p233_rana=sum("P233 zeď od kraje:" in r and ("blok hráčem" in r or "blitz hráčem" in r) for r in radky),
                p233_nejde=sum("P233 zeď od kraje: rána" in r and "nejde" in r for r in radky),
                p233=sum("P233 zeď od kraje" in r for r in radky))   # řádek P229 chybí, když P229 předá tah P233


def rozbor(kt, bb, s0, kroky, po, k2, po2, H, A):
    c0, c1 = nosic_nas(s0, H), nosic_nas(po, H)
    dirx = 1 if H == bb.TeamSide.HOME else -1
    posun = (kt._xy(c1)[0] - kt._xy(c0)[0]) * dirx if c0 is not None and c1 is not None else None
    srazeni = {e["player"] for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN"} if k2 else set()
    rohy = []
    if c1 is not None:
        rohy = [q.id for q in kt.hraci(po) if q.team_side == H and q.id != c1.id and kt._ma_zony(q)
                and kt._vedle(kt._xy(q), kt._xy(c1))]
    return dict(
        mel_mic=int(c0 is not None), posun=posun,
        akce=tuple((k["action"].type.name, k["action"].player_id, getattr(k["action"], "target_id", -1)) for k in kroky),
        td=int(po.home_team.score + po.away_team.score != s0.home_team.score + s0.away_team.score),
        turnover=int(any(k["turnover"] for k in kroky)),
        nosic_srazen=int(c1 is not None and c1.id in srazeni),
        mic_ztracen=(int(po2 is None or nosic_nas(po2, H) is None) if k2 is not None else 0) if c1 is not None else None,
        rohu=len(rohy), rohy_srazeny=len(set(rohy) & srazeni),
        nasi_srazeni=len(srazeni & {p.id for p in kt.hraci(po) if p.team_side == H}))


def init():
    fd = os.open(f"{STOPA}/{os.getpid()}.log", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    os.dup2(fd, 2); os.close(fd)


def hra(a):
    try:
        return _hra(a)
    except Exception as e:
        print("CHYBA", a, repr(e), flush=True)
        raise


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
                t0 = time.process_time(); p0 = pozice()
                try:
                    kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
                finally:
                    bb.set_extra_features_off(0)
                cas = time.process_time() - t0
                st = pocty(stopa_od(p0))
                k2 = po2 = None
                if po.phase == bb.GamePhase.PLAY and po.active_team == A:
                    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
                zaznam["ramena"][r] = rozbor(kt, bb, s, kroky, po, k2, po2, H, A)
                zaznam["ramena"][r].update(st, cas=cas)
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
    os.makedirs(STOPA, exist_ok=True)
    ukoly = [(n, o, g) for n, o in DVOJICE for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "6")), initializer=init) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo tahů", len(z))
    chyb = sum(1 for r in res if not r)
    if chyb: print("PRÁZDNÝCH HER (chyba nebo hned konec):", chyb)
