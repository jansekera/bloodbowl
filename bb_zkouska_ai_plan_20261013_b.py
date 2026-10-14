#!/usr/local/bin/python3.8
"""P205 část B (14.10.2026): partie trpaslíci × lesní elfové 02.–03.10. (2:0) na dnešním enginu.

Log té partie se přehraje jen na enginu `4e142640` (dnešní main má jiný výkop), proto dva kroky:

  1. výpis pozic — na starém enginu (worktree se sestaveným modulem):
       BB_ROOT=/home/jenda/claude/bb-replay-4e142640 /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261013_b.py vypis
     ⇒ evidence/zkouska_ai_plan_20261013_b/pozice.json (začátek a konec každého tahu + skutečné akce)
  2. plány — na dnešním main:
       /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261013_b.py plany [--kontrola]
     pozice se složí na stav z `setup_half` se stejnými soupiskami (dwarf × wood-elf TV1200) přepsáním
     zapisovatelných polí; `GameState` nemá mřížku obsazenosti, přímý zápis pozic stačí.

Co vazba nezpřístupňuje, a nepřenese se tedy: stunnedThisTurn, lostTacklezones, příznaky Big Guy,
rooted, proUsedThisTurn, stoodUpNextToEnemy, lékárník. Skilly se nezapisují — porovnají se (podle
jména, pořadí výčtu se mezi enginy může lišit) a neshoda se vypíše.
"""
import json
import os
import re
import sys
import time

TADY = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(TADY, "evidence", "zkouska_ai_plan_20261013_b")
POZICE = os.path.join(OUT, "pozice.json")
SEMINKA = list(range(1, 21))
ITERACE = 50
MAX_ID = 40


def skill_jmena(root):
    txt = open(os.path.join(root, "engine", "include", "bb", "enums.h")).read()
    body = txt[txt.index("enum class SkillName"):]
    body = re.sub(r"//[^\n]*", "", body[body.index("{") + 1: body.index("};")])
    return [n.split("=")[0].strip() for n in body.split(",") if n.strip() and n.strip() != "SKILL_COUNT"]


def jm(e):
    return str(e).split(".")[-1]


def stav_do_json(bb, s, jmena):
    hraci = []
    for pid in range(MAX_ID):
        try:
            p = s.get_player(pid)
        except IndexError:
            continue
        if p.id != pid:
            continue
        hraci.append({"id": p.id, "strana": jm(p.team_side), "stav": jm(p.state),
                      "pole": [p.position.x, p.position.y],
                      "staty": [p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour],
                      "zbyva_pohyb": p.movement_remaining, "hybal": p.has_moved, "blitzoval": p.used_blitz,
                      "jednal": p.has_acted,
                      "skilly": sorted(n for i, n in enumerate(jmena) if p.has_skill_index(i))})
    tym = lambda t: {"skore": t.score, "rerolly": t.rerolls, "reroll_pouzit": getattr(t, "reroll_used_this_turn", False),  # starý engine pole nemá  # noqa: E731
                     "kolo": t.turn_number, "blitz": t.blitz_used_this_turn, "prihravka": t.pass_used_this_turn,
                     "predani": t.hand_off_used_this_turn, "faul": t.foul_used_this_turn}
    return {"polocas": s.half, "faze": jm(s.phase), "na_tahu": jm(s.active_team), "pocasi": jm(s.weather),
            "kope": jm(s.kicking_team),
            "mic": {"pole": [s.ball.position.x, s.ball.position.y], "drzen": s.ball.is_held, "nosic": s.ball.carrier_id},
            "home": tym(s.home_team), "away": tym(s.away_team), "hraci": hraci}


def vypis():
    root = os.environ.get("BB_ROOT")
    if not root:
        raise SystemExit("vypis běží na starém enginu: BB_ROOT=/home/jenda/claude/bb-replay-4e142640")
    sys.path.insert(0, root)
    sys.path.insert(0, os.path.join(root, "engine", "build"))
    import bb_engine as bb
    sys.path.insert(0, TADY)
    import bb_zkouska_ai_plan_20261006 as z6     # přehrání s kontrolou proti výpisu partie (2:0)
    jmena = skill_jmena(root)
    tahy, konec, akce_vse = z6.prehraj()
    n = z6.over_prehrani(akce_vse, konec)
    out = []
    for t in tahy:
        m, _ = z6.mira(t["start"], t["konec"], t["strana"], t["kroky"])
        out.append({"start": stav_do_json(bb, t["start"], jmena), "konec": stav_do_json(bb, t["konec"], jmena),
                    "strana": jm(t["strana"]), "polocas": t["polocas"], "kolo": t["kolo"],
                    "akce": z6.strucne(t["kroky"]), "mira": m})
    os.makedirs(OUT, exist_ok=True)
    with open(POZICE, "w") as f:
        json.dump({"engine": root, "akci": n, "tahy": out}, f, ensure_ascii=False, indent=1)
    print(f"vypsáno {len(out)} tahů ({n} akcí, přehrání ověřeno proti výpisu partie, 2:0) ⇒ {POZICE}")


# ---------------------------------------------------------------- krok 2 (dnešní main)

def sloz(bb, ps, d):
    """Stav dnešního enginu s pozicí z JSON (soupisky = setup_half dwarf × wood-elf TV1200)."""
    s = bb.GameState()
    bb.setup_half(s, ps.ROSTERS[0], ps.ROSTERS[1], getattr(bb.TeamSide, d["kope"]))
    s.half = d["polocas"]
    s.phase = getattr(bb.GamePhase, d["faze"])
    s.active_team = getattr(bb.TeamSide, d["na_tahu"])
    s.weather = getattr(bb.Weather, d["pocasi"])
    s.kicking_team = getattr(bb.TeamSide, d["kope"])
    for klic, t in (("home", s.home_team), ("away", s.away_team)):
        j = d[klic]
        t.score, t.rerolls, t.reroll_used_this_turn, t.turn_number = j["skore"], j["rerolly"], j["reroll_pouzit"], j["kolo"]
        t.blitz_used_this_turn, t.pass_used_this_turn = j["blitz"], j["prihravka"]
        t.hand_off_used_this_turn, t.foul_used_this_turn = j["predani"], j["faul"]
    for h in d["hraci"]:
        p = s.get_player(h["id"])
        p.team_side = getattr(bb.TeamSide, h["strana"])
        p.state = getattr(bb.PlayerState, h["stav"])
        p.position.x, p.position.y = h["pole"]
        p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour = h["staty"]
        p.movement_remaining, p.has_moved, p.used_blitz, p.has_acted = h["zbyva_pohyb"], h["hybal"], h["blitzoval"], h["jednal"]
    s.ball.position.x, s.ball.position.y = d["mic"]["pole"]
    s.ball.is_held, s.ball.carrier_id = d["mic"]["drzen"], d["mic"]["nosic"]
    return s


def rozdily(a, b, cesta=""):
    """Rozdíly dvou výpisů pozice (seznam řetězců)."""
    if isinstance(a, dict) and isinstance(b, dict):
        return [r for k in sorted(set(a) | set(b)) for r in rozdily(a.get(k), b.get(k), f"{cesta}.{k}")]
    if isinstance(a, list) and isinstance(b, list) and a and isinstance(a[0], dict) and "id" in a[0]:
        ia, ib = {x["id"]: x for x in a}, {x["id"]: x for x in b}
        return [r for i in sorted(set(ia) | set(ib)) for r in rozdily(ia.get(i), ib.get(i), f"{cesta}[{i}]")]
    return [] if a == b else [f"{cesta}: {a} ≠ {b}"]


def plany():
    sys.path.insert(0, TADY)
    sys.path.insert(0, os.path.join(TADY, "engine", "build"))
    import bb_engine as bb
    import bb_kontrola_tahu as kt
    import bb_play_session_20261002 as ps
    ps.new_game()                                 # nastaví ps.ROSTERS (dwarf, wood-elf TV1200) na dnešním enginu
    jmena = skill_jmena(TADY)
    data = json.load(open(POZICE))
    tahy = data["tahy"]

    # kontroly přenosu: složená pozice vypsaná zpět = JSON ze starého enginu (jen skilly smí lišit — vypsat)
    nesouhlas, skilly = [], set()
    for i, t in enumerate(tahy, 1):
        for r in rozdily(t["start"], stav_do_json(bb, sloz(bb, ps, t["start"]), jmena)):
            (skilly if ".skilly" in r else nesouhlas).append(f"tah {i}: {r}")
    print(f"přenos {len(tahy)} pozic: neshod mimo skilly {len(nesouhlas)}, ve skillech {len(skilly)}")
    for r in nesouhlas[:20] + sorted(skilly)[:40]:
        print("   ", r)
    # pozitivní kontrola: posunutý hráč ⇒ neshoda
    zk = json.loads(json.dumps(tahy[0]["start"]))
    zk["hraci"][0]["pole"][0] += 1
    rp = rozdily(tahy[0]["start"], stav_do_json(bb, sloz(bb, ps, zk), jmena))
    print(f"pozitivní kontrola (posunutý hráč {zk['hraci'][0]['id']}): {rp} ⇒ " + ("✅" if rp else "⛔ NIC NENAŠLO"))
    if nesouhlas or not rp:
        raise SystemExit("⛔ přenos pozic nesedí")
    if "--kontrola" in sys.argv:
        return

    import bb_zkouska_ai_plan_20261006 as z6
    z6.ps = ps
    souhrn = []
    for i, t in enumerate(tahy, 1):
        st = sloz(bb, ps, t["start"])
        strana = st.active_team
        kdo = "trpaslici" if strana == bb.TeamSide.HOME else "elfove"
        plany_, t0 = [], time.time()
        for ai, sd in [("macro_mcts", s) for s in SEMINKA] + [("greedy", s) for s in SEMINKA]:
            kroky, po, hotovo = kt.plan_tahu(st, ai=ai, seed=1000 * i + sd, mcts_iterations=ITERACE)
            m, _ = z6.mira(st, po, strana, list(kroky))
            m["dokonceno"] = bool(hotovo)
            plany_.append({"ai": ai, "seminko": sd, **m})
        agr = {ai: z6.agregat([p for p in plany_ if p["ai"] == ai]) for ai in ("macro_mcts", "greedy")}
        souhrn.append({"tah": i, "polocas": t["polocas"], "kolo": t["kolo"], "kdo": kdo, "skutecnost": t["mira"],
                       **agr, "plany": plany_})
        print(f"tah {i:2d} {kdo:9s} p{t['polocas']} k{t['kolo']}: {time.time() - t0:5.1f} s\n"
              f"      macro: {z6.veta(agr['macro_mcts'])}\n      greedy: {z6.veta(agr['greedy'])}", flush=True)
    with open(os.path.join(OUT, "souhrn.json"), "w") as f:
        json.dump({"skilly_neshoda": sorted(skilly), "tahy": souhrn}, f, ensure_ascii=False, indent=1)


if __name__ == "__main__":
    {"vypis": vypis, "plany": plany}[sys.argv[1]]()
