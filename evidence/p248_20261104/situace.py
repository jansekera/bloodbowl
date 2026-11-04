# P248 (4.11.2026): ochrana lovce. Jen náš tah + odpověď soupeře, párově po situacích (stejné semínko).
# Situace = náš tah lesních elfů (mají lovce: AG 4 + Strip Ball + Wrestle), kde se plán s P248 a bez něj liší.
# Tah K-krát v obou ramenech. Měří se (mechanismus napřed):
#   lovec sražen v našem tahu / v tahu soupeře (nestojí na konci, ač na začátku stál);
#   hrozba nejlepší rány soupeře na lovce po našem tahu (pro kalibraci meze 0,15: hrozba × skutečně sražen);
#   čítač P248 (pozitivní kontrola, že pravidlo v rameni „zap“ zabralo);
#   cena: kolik soupeřů jsme v našem tahu srazili, turnover; míč: soupeř ztratil míč, TD soupeře.
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 6
K = int(sys.argv[4]) if len(sys.argv) > 4 else 4
NASI = ["woodelf"]; SOUPERI = ["orc", "human", "skaven", "dwarf"]
P248 = 33554432
RAMENA = {"zap": 0, "vyp": P248}
sys.path.insert(0, KOREN)


def stoji(bb, s, pid):
    p = s.get_player(pid)
    return p.is_on_pitch() and p.state == bb.PlayerState.STANDING


def lovec(kt, bb, s, H):
    for p in kt.hraci(s):
        if p.team_side == H and p.state == bb.PlayerState.STANDING and p.stats.agility >= 4 and \
           kt.ma_skill(p, "StripBall") and kt.ma_skill(p, "Wrestle"):
            return p.id
    return None


def drzi(bb, s, side):
    if not s.ball.is_held or s.ball.carrier_id <= 0: return False
    return s.get_player(s.ball.carrier_id).team_side == side


def jeden_tah(kt, bb, s0, rameno, seed, H, A, lid):
    bb.set_extra_features_off(RAMENA[rameno]); bb.take_p248_hunter()
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    citac = bb.take_p248_hunter()
    bb.set_extra_features_off(0)
    souperi = {p.id for p in kt.hraci(s0) if p.team_side == A}
    o = dict(citac=citac, turnover=int(any(k["turnover"] for k in kroky)),
             srazili_jsme=sum(1 for k in kroky for e in k["events"] if e["type"] == "KNOCKED_DOWN" and e["player"] in souperi),
             lovec_sraz_nas=int(not stoji(bb, po, lid)), hrozba=-1.0, lovec_sraz_soup=0, mic_soup_ztracen=0, td_soupere=0)
    soup_mel = drzi(bb, po, A)
    if po.phase != bb.GamePhase.PLAY or po.active_team != A: return o
    if not o["lovec_sraz_nas"]:
        o["hrozba"] = bb.player_blitz_threat(po, lid)
    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
    if not o["lovec_sraz_nas"]: o["lovec_sraz_soup"] = int(not stoji(bb, po2, lid))
    skore = lambda st: st.home_team.score if A == bb.TeamSide.HOME else st.away_team.score
    o["td_soupere"] = int(skore(po2) > skore(po))
    o["mic_soup_ztracen"] = int(soup_mel and not o["td_soupere"] and not drzi(bb, po2, A))
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
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        lid = lovec(kt, bb, s, H) if s.active_team == H else None
        if lid is not None:
            bb.set_extra_features_off(P248)
            kroky0, po0, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
            bb.set_extra_features_off(0)
            mk = lambda ks: [(k["macro"]["type"], k["macro"]["player"], tuple(k["macro"]["target_pos"])) for k in ks if "macro" in k]
            if mk(kroky) != mk(kroky0):
                sit = dict(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah, obrana=drzi(bb, s, A), ramena={})
                for r in RAMENA:
                    sit["ramena"][r] = [jeden_tah(kt, bb, s, r, 90000 + 1000 * g + 10 * tah + 100000 * j, H, A, lid) for j in range(K)]
                out.append(sit)
                print("situace", nasi, sou, g, tah, flush=True)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
