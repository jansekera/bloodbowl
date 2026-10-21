# Prázdné blitzy (uživatel 20.10.: prověřit, proč 1/3 blitzů agilních „nic“ — diagnostika P220).
# „nic“ v diag.py = cíl nesražen (bez KNOCKED_DOWN) a nepohnul se. Tady se u každého našeho blitzu uloží
# celé události kroku blitzu, skilly cíle a blitzujícího a stav obou po kroku — aby šlo rozepsat příčinu
# (Stand Firm · oboustranný pád s Block · Wrestle · pád blitzujícího · turnover před blokem · jiné).
# Stejné nastavení her jako diag.py P220 (elfové + skaveni × trpaslík/ork/člověk, útok i obrana), jen jedno
# rameno: aktuální main (P219 odstraněn).
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 3
NASI = ["wood-elf", "skaven"]; SOUPERI = ["dwarf", "orc", "human"]
SKILLY = ["Block", "Dodge", "StandFirm", "Wrestle", "SideStep", "Tackle", "Frenzy", "Juggernaut",
          "Fend", "Grab", "MightyBlow", "Horns", "Dauntless", "Guard", "Claw"]
sys.path.insert(0, KOREN)


def skilly(kt, p):
    return [s for s in SKILLY if kt.ma_skill(p, s)]


def blitzy(kt, s0, kroky, po):
    out = []
    for i, k in enumerate(kroky):
        a = k["action"]
        if a.type.name != "BLITZ": continue
        b, t = a.player_id, a.target_id
        z = dict(blitzer=b, cil=t, turnover=int(k["turnover"]),
                 udalosti=[{kk: (str(v) if not isinstance(v, (int, float, str)) else v) for kk, v in e.items()}
                           for e in k["events"]],
                 dal_kroky=[(kk["action"].type.name, kk["action"].player_id) for kk in kroky[i + 1:]])
        try:
            bp0, bp1 = s0.get_player(b), po.get_player(b)
            z.update(blitzer_skilly=skilly(kt, bp0), blitzer_ag=bp0.stats.agility, blitzer_st=bp0.stats.strength,
                     blitzer_stav_po=str(bp1.state))
        except IndexError:
            pass
        try:
            t0, t1 = s0.get_player(t), po.get_player(t)
            z.update(cil_skilly=skilly(kt, t0), cil_st=t0.stats.strength, cil_stav_po=str(t1.state),
                     cil_pohnul=int((not t1.is_on_pitch()) or kt._xy(t0) != kt._xy(t1)),
                     cil_srazen=int(any(e.get("type") == "KNOCKED_DOWN" and e.get("player") == t for e in k["events"])))
        except IndexError:
            z["bez_bloku"] = 1
        out.append(z)
    return out


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
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if s.active_team == H:
            for z in blitzy(kt, s, kroky, po):
                z.update(nasi=nasi, sou=sou, g=g, utok=utok, tah=tah)
                out.append(z)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    print("hra", nasi, sou, g, utok, len(out), flush=True)
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g, u) for n in NASI for o in SOUPERI for g in range(N) for u in (True, False)]
    if os.environ.get("JEN_JEDNA"): ukoly = ukoly[:1]
    with mp.Pool(int(os.environ.get("JADER", "2"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    z = [x for r in res for x in r]
    pickle.dump(z, open(VYST, "wb")); print("hotovo blitzů", len(z))
