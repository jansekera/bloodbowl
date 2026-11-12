# P302 (11.11.2026, uživatel: „postupné změny a každou změřit — … spočítat zvednutí míče soupeřem“): měřidlo A18
#   `opponent_pickup_chance` na situacích P250b. Nový ukazatel `souper_mic` = s jakou šancí má soupeř míč po našem
#   tahu: drží ho ⇒ 1, leží volně ⇒ jeho nejlepší šance zvednout (doběh bez pádu × zvednutí), máme ho my ⇒ 0.
#   Ramena (env RAMENA): vse · bez_b (bez obestoupení) · bez_c (bez odtlačení na míč) — co dnešní P250 ubírá soupeři.
# Původní hlavička P250b (5.11.2026): měření P250 tam, kde působí — na začátku našeho tahu leží míč volně a náš stojící hráč
# na něj dosáhne (≤ MA + 2). P250 = zvedač podle celé akce (a), obestoupit (b), odtlačit na míč (c).
# První měření (evidence/p250_20261105) mělo volný míč jen ve 37 ze 160 situací ⇒ účel nešlo změřit.
# Jen náš tah + odpověď soupeře, párově po situacích (stejné semínko). Ramena: vse · bez_a · bez_b · bez_c · bez.
# Měří se: míč u nás po našem tahu a po tahu soupeře, míč u soupeře po našem tahu, turnover, TD soupeře,
# čítače P250 (pozitivní kontrola).
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 3
K = int(sys.argv[4]) if len(sys.argv) > 4 else 2
NASI = os.environ.get("NASI", "skaven,woodelf,human").split(","); SOUPERI = ["orc", "human", "dwarf"]
A_, B_, C_ = 1 << 32, 1 << 33, 1 << 34
RAMENA = {"vse": 0, "bez_a": A_, "bez_b": B_, "bez_c": C_, "bez": A_ | B_ | C_,
          "bez_302": 1 << 36}   # P302 krok 2 (12.11.): obestoupení na nejbližší pole (dřívější P250b)
# P250c (6.11.): ověření části a — jen vybraná ramena (env RAMENA=vse,bez_a)
RAMENA = {r: RAMENA[r] for r in os.environ.get("RAMENA", "vse,bez_b,bez_c").split(",")}
sys.path.insert(0, KOREN)


def drzi(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return False
    p = s.get_player(s.ball.carrier_id)
    return p.team_side == side and p.is_on_pitch()


def volny(s):
    return not s.ball.is_held and s.ball.is_on_pitch()


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    bb.set_extra_features_off(RAMENA[rameno]); bb.take_p250()
    bb.take_p302()
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    cx = tuple(bb.take_p250()) + tuple(bb.take_p302())
    bb.set_extra_features_off(0)
    o = dict(cx=cx, turnover=int(any(k["turnover"] for k in kroky)),
             mic_u_nas_po_nas=int(drzi(bb, po, H)), mic_u_soup_po_nas=int(drzi(bb, po, A)),
             mic_u_nas_po_soup=0, td_soupere=0,
             volny_po_nas=int(volny(po)),
             souper_mic=(1.0 if drzi(bb, po, A) else (bb.opponent_pickup_chance(po, A) if volny(po) else 0.0)))
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        o["mic_u_nas_po_soup"] = int(drzi(bb, po2, H))
    else:
        o["mic_u_nas_po_soup"] = o["mic_u_nas_po_nas"]
    return o


def dosah(kt, bb, s, H):
    b = s.ball.position
    for p in kt.hraci(s):
        if p.team_side == H and p.state == bb.PlayerState.STANDING:
            if max(abs(p.position.x - b.x), abs(p.position.y - b.y)) <= p.stats.movement + 2: return True
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
        # tah 0 = po výkopu: v kouři 5 z 5 zvednuto ve všech ramenech (strop) ⇒ jen volný míč uprostřed hry
        if tah > 0 and s.active_team == H and volny(s) and dosah(kt, bb, s, H):
            sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, ramena={})
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
    if len(sys.argv) > 5: ukoly = ukoly[:int(sys.argv[5])]          # kouř: jen prvních pár her
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
