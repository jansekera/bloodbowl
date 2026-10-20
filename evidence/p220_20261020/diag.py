# P220 diagnostika (20.10.2026): kdo z NAŠICH AGILNÍCH hráčů končí tah v kontaktu a jak se tam dostal.
# Uživatel: „bez plánu asistence pro blitz [agilní] nemají [docházet k soupeři a označit ho] vůbec“;
#   výjimka: markování nosiče míče; platí pro všechny agilní (AG4+, nebo AG3 s Dodge).
# Každý náš tah dvakrát ze stejného stavu a semínka: „vse“ (vše zapnuto) a „bez219“ (maska 32) — hra pokračuje
#   ramenem „vse“; po každém rameni jen tah soupeře (kdo z kategorie padne).
# Kategorie hráče v kontaktu na konci tahu: blok (blokoval/blitzoval) · stal (v kontaktu už na začátku, nehýbal se)
#   · nosic (došel a markuje nosiče — výjimka) · asist (došel vedle cíle bloku/blitzu, který přišel PO jeho pohybu)
#   · jen_dosel (došel do kontaktu bez toho — porušení P220) · jine.
import sys, os, pickle, collections, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
NASI = ["wood-elf", "skaven"]; SOUPERI = ["dwarf", "orc", "human"]
RAMENA = {"vse": 0, "bez219": 32}
sys.path.insert(0, KOREN)
BLOKY = ("BLOCK", "BLITZ", "MULTIPLE_BLOCK")


def agilni(kt, p):
    ag = p.stats.agility
    return ag >= 4 or (ag == 3 and kt.ma_skill(p, "Dodge"))


def rozbor(kt, bb, s0, kroky, po, H, A):
    sou_po = [q for q in kt.hraci(po) if q.team_side == A and kt._ma_zony(q)]
    sou_0 = [q for q in kt.hraci(s0) if q.team_side == A and kt._ma_zony(q)]
    nosic = po.ball.carrier_id
    nosic_sou = any(q.id == nosic for q in sou_po)
    akce = [(i, k["action"].type.name, k["action"].player_id, k["action"].target_id) for i, k in enumerate(kroky)]
    out = {}
    for p in kt.hraci(po):
        if p.team_side != H or not kt._ma_zony(p) or not agilni(kt, p): continue
        sousede = [q for q in sou_po if kt._vedle(kt._xy(p), kt._xy(q))]
        if not sousede: continue
        p0 = s0.get_player(p.id)
        moje = [a for a in akce if a[2] == p.id]
        if any(a[1] in BLOKY for a in moje): kat = "blok"
        elif not moje and kt._xy(p0) == kt._xy(p) and any(kt._vedle(kt._xy(p0), kt._xy(q)) for q in sou_0): kat = "stal"
        elif nosic_sou and any(q.id == nosic for q in sousede): kat = "nosic"
        else:
            posl = max((a[0] for a in moje), default=-1)
            cile = {a[3] for a in akce if a[1] in BLOKY and a[2] != p.id and a[0] > posl}
            kat = "asist" if any(q.id in cile for q in sousede) else ("jen_dosel" if moje else "jine")
        out[p.id] = kat
    return out


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
        if s.active_team == H:
            zaznam = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, ramena={})
            for r, m in RAMENA.items():
                bb.set_extra_features_off(m)
                kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
                bb.set_extra_features_off(0)
                kat = rozbor(kt, bb, s, kroky, po, H, A)
                srazeni = set()
                if po.phase == bb.GamePhase.PLAY and po.active_team == A and kat:
                    k2, _, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
                    srazeni = {e["player"] for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN"}
                zaznam["ramena"][r] = dict(kat=kat, srazen={pid: int(pid in srazeni) for pid in kat},
                                           turnover=int(any(k["turnover"] for k in kroky)))
                if r == "vse": dal = po
            out.append(zaznam)
            po = dal
        else:
            kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    print("hra", nasi, sou, g, utok, len(out), flush=True)
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo tahů", len(z))
