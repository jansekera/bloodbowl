# P220 měření (21.10.2026): pravidlo „agilní hráč nekončí přesun v kontaktu“ (maska 128) zapnuto × vypnuto.
# Vychází z evidence/p220_20261020/diag.py (táž sada: 72 poločasů, elfové + skaveni × trpaslík/ork/člověk,
# útok i obrana, táž semínka). Každý náš tah dvakrát ze stejného stavu a semínka: „vse“ (maska 0, P220 zapnut)
# a „bez220“ (maska 128). Po každém rameni jen odpověď soupeře (jeho jeden tah) — NIC DÁL; hra pokračuje
# ramenem „vse“.
# Měří se, co má pravidlo dělat: agilní v kontaktu na konci tahu podle kategorie (blok · stál · nosič ·
# asistence · jen došel · jiné), z toho „jen došel“ vedle našeho nosiče (roh klece) a „jen došel“ bez pohybu
# (hrál předávku / přihrávku, ale stál na místě — diag.py ho počítá jako „jen došel“); naši v kontaktu celkem;
# turnover našeho tahu; soupeři sraženi naším tahem; blitz a jeho výsledek; v odpovědi soupeře sraženi naši
# (všichni i agilní z kategorií).
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
NASI = ["wood-elf", "skaven"]; SOUPERI = ["dwarf", "orc", "human"]
RAMENA = {"vse": 0, "bez220": 128}
sys.path.insert(0, KOREN)
BLOKY = ("BLOCK", "BLITZ", "MULTIPLE_BLOCK")


def agilni(kt, p):
    ag = p.stats.agility
    return ag >= 4 or (ag == 3 and kt.ma_skill(p, "Dodge"))


def rozbor(kt, bb, s0, kroky, po, H, A):
    """Táž kategorizace jako diag.py; navíc u „jen_dosel“ zda stojí vedle našeho nosiče (roh klece)."""
    sou_po = [q for q in kt.hraci(po) if q.team_side == A and kt._ma_zony(q)]
    sou_0 = [q for q in kt.hraci(s0) if q.team_side == A and kt._ma_zony(q)]
    nosic = po.ball.carrier_id
    nosic_sou = any(q.id == nosic for q in sou_po)
    nas_nosic = [q for q in kt.hraci(po) if q.team_side == H and q.id == nosic]
    akce = [(i, k["action"].type.name, k["action"].player_id, k["action"].target_id) for i, k in enumerate(kroky)]
    out, u_nosice, bez_pohybu = {}, 0, 0
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
        if kat == "jen_dosel" and nas_nosic and p.id != nosic and kt._vedle(kt._xy(p), kt._xy(nas_nosic[0])):
            u_nosice += 1
        if kat == "jen_dosel" and kt._xy(p0) == kt._xy(p): bez_pohybu += 1   # hrál (předávka, přihrávka), nepohnul se
        out[p.id] = kat
    return out, u_nosice, bez_pohybu


def v_kontaktu(kt, po, H, A):
    sou = [q for q in kt.hraci(po) if q.team_side == A and kt._ma_zony(q)]
    return sum(1 for p in kt.hraci(po) if p.team_side == H and kt._ma_zony(p)
               and any(kt._vedle(kt._xy(p), kt._xy(q)) for q in sou))


def srazeno_souperu(kt, s0, po, A):
    n = 0
    for q in kt.hraci(s0):
        if q.team_side != A or not kt._stoji(q): continue
        q1 = po.get_player(q.id)
        if not (q1.is_on_pitch() and kt._stoji(q1)): n += 1
    return n


def blitz_rozbor(kt, bb, s0, kroky, po, H, A):
    for i, k in enumerate(kroky):
        a = k["action"]
        if a.type.name != "BLITZ": continue
        b, t = a.player_id, a.target_id
        try:
            t0, t1 = s0.get_player(t), po.get_player(t)
        except IndexError:   # blitz ohlášen bez cíle (Wild Animal) — viz diag.py
            return dict(blitzer=b, agilni=int(agilni(kt, s0.get_player(b))), vysledek="bez_bloku", kontakt=0,
                        dal_tahl=0, turnover_v_blitzu=int(k["turnover"]))
        srazen = any(e["type"] == "KNOCKED_DOWN" and e["player"] == t for e in k["events"])
        posun = (not srazen) and (not t1.is_on_pitch() or kt._xy(t0) != kt._xy(t1))
        bp1 = po.get_player(b)
        lezi = lambda q: q.is_on_pitch() and q.state != bb.PlayerState.STANDING
        nic = None
        if not srazen and not posun:
            if lezi(t1) and lezi(bp1): nic = "oba_lezi"
            elif k["turnover"] or lezi(bp1) or not bp1.is_on_pitch(): nic = "padl_blitzujici"
            else: nic = "bez_ucinku"
        dal = any(kk["action"].player_id == b for kk in kroky[i + 1:])
        kontakt = kt._ma_zony(bp1) and any(q.team_side == A and kt._ma_zony(q) and kt._vedle(kt._xy(bp1), kt._xy(q))
                                          for q in kt.hraci(po))
        return dict(blitzer=b, agilni=int(agilni(kt, s0.get_player(b))),
                    vysledek="srazen" if srazen else ("posun" if posun else nic),
                    dal_tahl=int(dal), kontakt=int(bool(kontakt)), turnover_v_blitzu=int(k["turnover"]))
    return None


def hra(a):
    try:
        return _hra(a)
    except Exception as e:   # jedna vadná hra nesmí zahodit ostatní
        print("CHYBA", a, repr(e), flush=True)
        return []


def _hra(a):
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
                try:
                    kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
                finally:
                    bb.set_extra_features_off(0)
                kat, u_nosice, bez_pohybu = rozbor(kt, bb, s, kroky, po, H, A)
                bl = blitz_rozbor(kt, bb, s, kroky, po, H, A)
                srazeni = set()
                if po.phase == bb.GamePhase.PLAY and po.active_team == A:
                    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)   # odpověď soupeře, maska 0
                    srazeni = {e["player"] for k in k2 for e in k["events"] if e["type"] == "KNOCKED_DOWN"}
                    if bl:
                        q = po2.get_player(bl["blitzer"])
                        bl["volny_po_odpovedi"] = int(q.is_on_pitch() and kt._ma_zony(q) and not any(
                            o.team_side == A and kt._ma_zony(o) and kt._vedle(kt._xy(q), kt._xy(o)) for o in kt.hraci(po2)))
                if bl: bl["blitzer_srazen"] = int(bl["blitzer"] in srazeni)
                nasi_ids = {p.id for p in kt.hraci(po) if p.team_side == H}
                zaznam["ramena"][r] = dict(
                    kat=kat, srazen={pid: int(pid in srazeni) for pid in kat}, blitz=bl, u_nosice=u_nosice, bez_pohybu=bez_pohybu,
                    turnover=int(any(k["turnover"] for k in kroky)), v_kontaktu=v_kontaktu(kt, po, H, A),
                    srazeno_souperu=srazeno_souperu(kt, s, po, A),
                    nasi_srazeni=len(srazeni & nasi_ids))
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
    with mp.Pool(int(os.environ.get("JADER", "4"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo tahů", len(z))
