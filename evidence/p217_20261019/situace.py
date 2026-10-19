# P217 (19.10.2026): nejvyšší šance na TD (klec / útěk / výběh / TD teď) přímo v situacích, kde rozhoduje.
# Uživatel 19.10.: „porovnej přímo konkrétní situace samostatně“, „má pomoci jen rychlým týmům“; „při kontrole
#   testuj jen tu jednu úpravu — celá hra je pro kontrolu pak obecnou — ne pro vyhodnocení té jedné úpravy“;
#   „počítej jen aktuální kolo a pak maximální odpověď oponenta v příštím kole“.
# Postup: hra se vším zapnutým (skaven, wood-elf × dwarf/orc/human, útočný poločas). V našem tahu s míčem se
#   tah naplánuje s P217 i bez něj (stejné semínko); liší-li se makra řadiče, je to situace. Z ní se tah
#   zahraje K-krát v obou ramenech (vse / bez_p217) a po něm JEN tah soupeře (vše zapnuté).
# Měří se jen to, co úprava slibuje: úspěch = TD v tom tahu NEBO míč náš po tahu soupeře; zvlášť TD v tom
#   tahu, míč přežil, turnover a sražení nosiče v tom tahu, nosič sražen v tahu soupeře.
# situace.py KOREN vystup.pkl [semínek] [K]
import sys, os, pickle, multiprocessing as mp
KOREN = sys.argv[1]; VYST = sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 10
K = int(sys.argv[4]) if len(sys.argv) > 4 else 6
NASI = ["skaven", "wood-elf"]; SOUPERI = ["dwarf", "orc", "human"]
RAMENA = {"vse": 0, "bez_p217": 16}
sys.path.insert(0, KOREN)


def drzi(kt, s, my):
    return s.ball.is_held and any(p.id == s.ball.carrier_id and p.team_side == my for p in kt.hraci(s))


def makra(kroky):
    return [(k["macro"]["type"], k["macro"]["player"], tuple(k["macro"]["target_pos"])) for k in kroky
            if "macro" in k and k["macro"]["source"] == "cage"]


def dohraj(kt, bb, s0, rameno, seed, my):
    """Náš tah v rameni, pak jen tah soupeře."""
    o = dict(turnover=0, nosic_srazen=0, td=0, mic_prezil=0, srazen_souperem=0, uspech=0)
    bb.set_extra_features_off(RAMENA[rameno])
    nosic = s0.ball.carrier_id if s0.ball.is_held else -1
    kroky, po, _ = kt.plan_tahu(s0, ai="macro_mcts", seed=seed)
    bb.set_extra_features_off(0)
    o["turnover"] = int(any(k["turnover"] for k in kroky))
    o["nosic_srazen"] = int(any(e["type"] == "KNOCKED_DOWN" and e["player"] == nosic for k in kroky for e in k["events"]))
    if po.home_team.score > s0.home_team.score:
        o["td"] = o["uspech"] = 1
        return o
    if po.phase == bb.GamePhase.PLAY and drzi(kt, po, my):
        nos2 = po.ball.carrier_id
        kroky2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=seed + 1)
        o["srazen_souperem"] = int(any(e["type"] == "KNOCKED_DOWN" and e["player"] == nos2 for k in kroky2 for e in k["events"]))
        o["mic_prezil"] = int(drzi(kt, po2, my) and po2.away_team.score == po.away_team.score)
    o["uspech"] = o["mic_prezil"]
    return o


def hra(a):
    nasi, sou, g = a
    import bb_kontrola_tahu as kt
    bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), A)
    s.kicking_team = A
    bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
    my = H; out = []
    for tah in range(16):
        if s.phase != bb.GamePhase.PLAY: break
        seed = 50000 + 100 * g + tah
        bb.set_extra_features_off(0)
        kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
        if s.active_team == my and drzi(kt, s, my) and s.home_team.turn_number < 8:
            bb.set_extra_features_off(16)
            kroky0, _, _ = kt.plan_tahu(s, ai="macro_mcts", seed=seed)
            bb.set_extra_features_off(0)
            if makra(kroky) != makra(kroky0):
                c = next(p for p in kt.hraci(s) if p.id == s.ball.carrier_id)
                sit = dict(nasi=nasi, sou=sou, g=g, tah=tah, kolo=s.home_team.turn_number, do_zony=25 - c.position.x,
                           s_p217=makra(kroky)[:2], bez=makra(kroky0)[:2], ramena={})
                for r in RAMENA:
                    sit["ramena"][r] = [dohraj(kt, bb, s, r, 70000 + 1000 * g + 10 * tah + 100000 * j, my) for j in range(K)]
                out.append(sit)
                print("situace", nasi, sou, g, tah, {r: sum(x["uspech"] for x in v) for r, v in sit["ramena"].items()}, flush=True)
        if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
        s = po
    return out


if __name__ == "__main__":
    ukoly = [(n, o, g) for n in NASI for o in SOUPERI for g in range(N)]
    with mp.Pool(int(os.environ.get("JADER", "7"))) as pool: res = pool.map(hra, ukoly, chunksize=1)
    sit = [x for r in res for x in r]
    pickle.dump(sit, open(VYST, "wb")); print("hotovo situací", len(sit))
