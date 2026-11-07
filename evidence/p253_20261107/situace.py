# P253 (7.11.2026, uživatel: „spusť měření“): AI cení přihrávku s přehozy (Pass/Catch/týmový) a dává přednost
# nabídkám bez týmového přehozu (kP253PassPricing, bit 35). Situace = náš tah, míč drží náš hráč a stojící spoluhráč
# je 2–10 polí od něj blíž k endzóně (kandidát na přihrávku nebo předání). Jen náš tah + odpověď soupeře, párově
# (stejné semínko). Ramena: vse · bez (P253 vypnuto). P247 (přehoz nepřesné přihrávky v enginu) vypínač nemá — platí v obou.
# Měří se: TD náš v našem tahu, míč u nás po našem tahu / po tahu soupeře, turnover, TD soupeře, přihrávka zahrána,
# čítače P253 (pozitivní kontrola: nabídka jen díky přehozům, nabídka vyřazena kvůli přednosti).
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 3
K = int(sys.argv[4]) if len(sys.argv) > 4 else 2
NASI = os.environ.get("NASI", "skaven,woodelf,human").split(","); SOUPERI = ["orc", "human", "dwarf"]
RAMENA = {"vse": 0, "bez": 1 << 35}
sys.path.insert(0, KOREN)


def drzi(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return False
    p = s.get_player(s.ball.carrier_id)
    return p.team_side == side and p.is_on_pitch()


def volny(s):
    return not s.ball.is_held and s.ball.is_on_pitch()


def jeden_tah(kt, bb, s0, rameno, seed, H, A):
    bb.set_extra_features_off(RAMENA[rameno]); bb.take_p253()
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    cx = bb.take_p253()
    bb.set_extra_features_off(0)
    skH = lambda st: st.home_team.score if H == bb.TeamSide.HOME else st.away_team.score
    makra = [k.get("macro", {}).get("type") for k in kroky]
    o = dict(cx=cx, turnover=int(any(k["turnover"] for k in kroky)),
             td_nase=int(skH(po) > skH(s0)), prihravka=int("PASS_ACTION" in makra),
             mic_u_nas_po_nas=int(drzi(bb, po, H)), mic_u_soup_po_nas=int(drzi(bb, po, A)),
             mic_u_nas_po_soup=0, td_soupere=0)
    if po.phase == bb.GamePhase.PLAY and po.active_team == A:
        k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
        o["td_soupere"] = int(skore(po2) > skore(po))
        o["mic_u_nas_po_soup"] = int(drzi(bb, po2, H))
    else:
        o["mic_u_nas_po_soup"] = o["mic_u_nas_po_nas"]
    return o


def kandidat(kt, bb, s, H):
    """Náš hráč drží míč a stojící spoluhráč je 1–10 polí od něj a blíž k endzóně (HOME útočí k x = 25,
    AWAY k x = 0 — one_cage.cpp distToEndzone)."""
    ez = lambda pos: abs(pos.x - (25 if H == bb.TeamSide.HOME else 0))
    c = s.get_player(s.ball.carrier_id)
    for p in kt.hraci(s):
        if p.team_side == H and p.id != c.id and p.state == bb.PlayerState.STANDING:
            d = max(abs(p.position.x - c.position.x), abs(p.position.y - c.position.y))
            if 1 <= d <= 10 and ez(p.position) < ez(c.position): return True
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
        if s.active_team == H and drzi(bb, s, H) and kandidat(kt, bb, s, H):
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
