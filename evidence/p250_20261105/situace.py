# P250 (5.11.2026): vyražený míč — zvedač podle celé akce (a), obestoupit (b), odtlačit na míč (c). Situace jako P249e/f.
# (původní hlavička P249e/f:) blitz na soupeřova nosiče až poslední, se zálohou (zvedač + 2 na zeď) — e; bez stropu úhybu,
# i s rizikem nad třetinu — f. Uživatel: „zvlášť ověř se skaveny — ti to asi reprezentují nejlépe“.
# Jen náš tah + odpověď soupeře, párově po situacích (stejné semínko). Situace = náš tah v obraně, míč drží soupeř
# a na nosiče někdo z našich dosáhne (≤ MA + 2). Ramena: vse · bez_e · bez_f · bez_ef. K opakování.
# U každé situace: nejlepší blitz na nosiče (riziko neúspěchu, šance srazit / vzít míč) — tentýž výběr jako hra —
# ⇒ jak často AI pokus zahraje podle šance a podle rizika (nad třetinu?).
# Měří se: blitz / blok na nosiče; soupeř ztratil míč v našem tahu; míč u nás po našem tahu a po tahu soupeře (A18:
# „sebrat míč = shodit + zvednout + odnést + zeď“); TD soupeře; turnover; čítače P249 (pozitivní kontrola).
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 3
K = int(sys.argv[4]) if len(sys.argv) > 4 else 2
NASI = ["skaven", "woodelf", "human"]; SOUPERI = ["orc", "human", "dwarf"]
A_, B_, C_ = 1 << 32, 1 << 33, 1 << 34
RAMENA = {"vse": 0, "bez_a": A_, "bez_b": B_, "bez_c": C_, "bez": A_ | B_ | C_}
sys.path.insert(0, KOREN)


def drzi(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return False
    p = s.get_player(s.ball.carrier_id)
    return p.team_side == side and p.is_on_pitch()


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    cid = s0.ball.carrier_id
    bb.set_extra_features_off(RAMENA[rameno]); bb.take_p250()
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    cx = bb.take_p250()
    bb.set_extra_features_off(0)
    makra = [k.get("macro", {}) for k in kroky]
    poradi = [i for i, m in enumerate(makra) if m.get("type") == "BLITZ" and m.get("target") == cid]
    o = dict(cx=cx, turnover=int(any(k["turnover"] for k in kroky)),
             blitz_nosic=int(bool(poradi)), blok_nosic=int(any(m.get("type") == "BLOCK" and m.get("target") == cid for m in makra)),
             akci_pred=(poradi[0] if poradi else -1),                      # kolik maker zahráno před blitzem na nosiče
             akci_po=(len([m for m in makra[poradi[0] + 1:] if m.get("type") not in (None, "END_TURN")]) if poradi else -1),
             mic_nas_tah=int(not drzi(bb, po, A)), mic_u_nas_po_nas=int(drzi(bb, po, H)), mic_u_nas_po_soup=0, td_soupere=0)
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        o["mic_u_nas_po_soup"] = int(drzi(bb, po2, H))
    else:
        o["mic_u_nas_po_soup"] = o["mic_u_nas_po_nas"]
    return o


def dosah(kt, bb, s, H, c):
    for p in kt.hraci(s):
        if p.team_side == H and p.state == bb.PlayerState.STANDING:
            if max(abs(p.position.x - c.position.x), abs(p.position.y - c.position.y)) <= p.stats.movement + 2: return True
    return False


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
        c = s.get_player(s.ball.carrier_id) if s.active_team == H and drzi(bb, s, A) else None
        if c is not None and dosah(kt, bb, s, H, c):
            sance_cap = None
            sance = bb.carrier_blitz_odds(s)           # bez stropu (P249f)
            sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, sance=sance, sance_cap=sance_cap, ramena={})
            for r in RAMENA:
                sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A) for j in range(K)]
            out.append(sit)
            print("situace", nasi, sou, g, tah, flush=True)
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI if o != n for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
