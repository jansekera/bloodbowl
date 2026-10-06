#!/usr/local/bin/python3.8
"""P146 krok 2 (06.10.2026): zkouška režimu „AI plánuje tah“ na dohrané partii 02.–03.10.

Log partie se přehraje po začátek každého tahu; v té pozici AI naplánuje celý tah nanečisto
(vlastní kostky) a plán se položí vedle tahu, který se skutečně hrál. Srovnává se PLÁN tahu,
ne výsledek — jakmile AI zahraje jinak, kostky se rozejdou.

⚠️ Log se přehrává jen na enginu `4e142640` (dnešní main má jiný výkop). Kořen enginu se bere
z proměnné BB_ROOT (worktree starého stavu s přenesenou vazbou ai_plan_turn):

  BB_ROOT=/home/jenda/claude/bb-replay-4e142640 /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261006.py

Výstup: evidence/zkouska_ai_plan_20261006/ (tah_NN_*.md, souhrn.md, souhrn.json).
"""
import json
import os
import re
import sys
import time

TADY = os.path.dirname(os.path.abspath(__file__))
ROOT = os.environ.get("BB_ROOT", TADY)
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "engine", "build"))
import bb_engine as bb  # noqa: E402
import bb_kontrola_tahu as kt  # noqa: E402
import bb_play_session_20261002 as ps  # noqa: E402

LOG = os.path.join(TADY, "evidence", "play_20261002", "prikazy.log")
VYPIS = os.path.join(TADY, "evidence", "play_20261002", "partie_vypis.md")
OUT = os.path.join(TADY, "evidence", "zkouska_ai_plan_20261006")
SEMINKA = list(range(1, 21))
ITERACE = 50


def prehraj():
    """Přehraje log stejně jako server partie. Vrací (tahy, konečný stav, řetězce všech akcí).
    tah = dict(start, konec, strana, kroky, polocas, kolo)."""
    state, dice = ps.new_game()
    radky = [l.strip() for l in open(LOG) if l.strip()]
    tahy, akce_vse = [], []
    tah_klic, tah_start, tah_kroky = None, None, []
    last_actions = []

    def uzavri():
        if tah_start is not None and tah_start.phase == bb.GamePhase.PLAY and tah_kroky:
            ts = tah_start.home_team if tah_start.active_team == bb.TeamSide.HOME else tah_start.away_team
            tahy.append({"start": tah_start, "konec": state.clone(), "strana": tah_start.active_team,
                         "kroky": tah_kroky, "polocas": tah_start.half, "kolo": ts.turn_number})

    for line in radky + [None]:
        klic = (state.active_team, state.home_team.turn_number, state.away_team.turn_number, state.half,
                state.home_team.score, state.away_team.score)
        if klic != tah_klic or line is None:
            uzavri()
            tah_klic, tah_kroky = klic, []
            tah_start = state.clone() if state.phase == bb.GamePhase.PLAY else None
        if line is None:
            break
        p = line.split()
        c = p[0]
        if c == "actions":
            last_actions = list(bb.get_available_actions(state))
        elif c == "vykop":
            kick = bb.TeamSide.HOME if p[1].upper() == "HOME" else bb.TeamSide.AWAY
            bb.setup_drive(state, ps.ROSTERS[0], ps.ROSTERS[1], kick, dice)
            bb.simple_kickoff(state, dice)
        elif c == "polocas":
            kick = bb.TeamSide.HOME if p[1].upper() == "HOME" else bb.TeamSide.AWAY
            bb.setup_second_half(state, ps.ROSTERS[0], ps.ROSTERS[1], kick, dice)
            bb.simple_kickoff(state, dice)
        elif c == "kostka":
            bb.set_manual_block_face(int(p[1]))
        elif c == "beztackle":
            bb.set_manual_no_tackle(True)
        elif c == "push":
            bb.set_manual_push(int(p[1]), int(p[2]))
        elif c == "blitzfrom":
            bb.set_manual_blitz_square(int(p[1]), int(p[2]))
        elif c == "follow":
            bb.set_manual_follow_up(p[1] == "1")
        elif c == "do":
            if int(p[1]) >= len(last_actions):
                raise SystemExit(f"⛔ přehrání se rozešlo (akce {p[1]} není v nabídce) — jiný engine než 4e142640? BB_ROOT={ROOT}")
            a = last_actions[int(p[1])]
            r, events = bb.execute_action_logged(state, a, dice)
            bb.clear_manual_block_choices()
            tah_kroky.append({"action": a, "turnover": r.turnover, "events": events})
            # výpis partie má jméno typu bez předpony výčtu („BLOCK“, ne „ActionType.BLOCK“)
            akce_vse.append(ps.action_str(a).replace("ActionType.", "", 1))
        else:
            raise SystemExit(f"neznámý příkaz v logu: {line}")
    return tahy, state, akce_vse


def over_prehrani(akce_vse, konec):
    """Pozitivní kontrola přehrání: pořadí akcí = pořadí ve výpisu partie, skóre 2:0."""
    ve_vypisu = re.findall(r"^- `([^`]+)`", open(VYPIS).read(), flags=re.M)
    if akce_vse != ve_vypisu:
        prvni = next((i for i, (a, b) in enumerate(zip(akce_vse, ve_vypisu)) if a != b), min(len(akce_vse), len(ve_vypisu)))
        raise SystemExit(f"⛔ přehrání se rozešlo s výpisem partie: akcí {len(akce_vse)} vs {len(ve_vypisu)}, "
                         f"první rozdíl na pozici {prvni}")
    skore = (konec.home_team.score, konec.away_team.score)
    if skore != (2, 0):
        raise SystemExit(f"⛔ přehrání skončilo {skore}, partie skončila 2:0")
    return len(akce_vse)


def smer(strana):
    return 1 if strana == bb.TeamSide.HOME else -1   # HOME skóruje na x=25, AWAY na x=0


def do_td(strana, x):
    return 25 - x if strana == bb.TeamSide.HOME else x


def mira(start, konec, strana, kroky):
    """Čísla jednoho tahu (skutečného nebo plánovaného)."""
    m = {}
    typy = [k["action"].type.name for k in kroky]
    m["blok"] = typy.count("BLOCK")
    m["blitz"] = typy.count("BLITZ")
    m["faul"] = typy.count("FOUL")
    m["prihravka"] = typy.count("PASS") + typy.count("HAND_OFF")
    m["hybali_se"] = len({k["action"].player_id for k in kroky if k["action"].type.name == "MOVE"})
    m["turnover"] = any(k["turnover"] for k in kroky)
    m["turnover_pricina"] = m["hralo_pred_turnoverem"] = None
    if m["turnover"]:
        # na čem tah spadl + kolik hráčů stihlo hrát dřív (pořadí: jisté napřed, riskantní nakonec)
        i = next(j for j, k in enumerate(kroky) if k["turnover"])
        a = kroky[i]["action"]
        hody = [e["type"] for e in kroky[i]["events"]
                if e["type"] in ("DODGE", "GFI", "PICKUP", "CATCH", "PASS", "LEAP", "STAND_UP") and not e["success"]]
        m["turnover_pricina"] = a.type.name + ("/" + hody[-1] if hody else "")
        m["hralo_pred_turnoverem"] = len({k["action"].player_id for k in kroky[:i]} - {a.player_id})
    ts0 = start.home_team if strana == bb.TeamSide.HOME else start.away_team
    ts1 = konec.home_team if strana == bb.TeamSide.HOME else konec.away_team
    m["td"] = ts1.score > ts0.score
    m["mic_start"] = (start.ball.position.x, start.ball.position.y)
    m["mic_konec"] = (konec.ball.position.x, konec.ball.position.y)
    m["postup_mice"] = smer(strana) * (m["mic_konec"][0] - m["mic_start"][0])
    drzi = konec.ball.is_held and konec.get_player(konec.ball.carrier_id).team_side == strana
    m["mame_mic"] = bool(drzi) or m["td"]
    m["do_td"] = None
    m["rohy"] = m["sousede"] = m["dosah_na_nosice"] = None
    if drzi and not m["td"]:
        n = konec.get_player(konec.ball.carrier_id)
        nx, ny = n.position.x, n.position.y
        m["do_td"] = do_td(strana, nx)
        nasi = {(p.position.x, p.position.y) for p in kt.hraci(konec)
                if p.team_side == strana and p.state == bb.PlayerState.STANDING and p.id != n.id}
        m["rohy"] = sum(1 for dx in (-1, 1) for dy in (-1, 1) if (nx + dx, ny + dy) in nasi)
        m["sousede"] = sum(1 for (x, y) in nasi if max(abs(x - nx), abs(y - ny)) == 1)
    radky = kt.kontrola_tahu(start, konec, strana, kroky)
    for r in radky:
        mm = re.search(r"dosáhne (\d+) soupeřů", r)
        if mm:
            m["dosah_na_nosice"] = int(mm.group(1))
        if "žádný soupeř na něj" in r:
            m["dosah_na_nosice"] = 0
    m["kraj"] = sum(1 for r in radky if "KRAJ" in r)
    m["kontakt"] = sum(1 for r in radky if "KONTAKT" in r)
    m["lezi"] = sum(1 for r in radky if "LEŽÍ" in r)
    m["blitz_nevyuzit"] = any("BLITZ nevyužit" in r for r in radky)
    return m, radky


def strucne(kroky):
    """Akce tahu po hráčích: pohyb jako cesta, u ostatních akcí události s hody."""
    out, i = [], 0
    while i < len(kroky):
        a = kroky[i]["action"]
        if a.type.name == "MOVE":
            j, cesta, ev = i, [], []
            while j < len(kroky) and kroky[j]["action"].type.name == "MOVE" and kroky[j]["action"].player_id == a.player_id:
                cesta.append(f"({kroky[j]['action'].target.x},{kroky[j]['action'].target.y})")
                ev += [e for e in kroky[j]["events"] if e["type"] != "MOVE"]
                if kroky[j]["turnover"]:
                    ev.append({"type": "TURNOVER!"})
                j += 1
            out.append(f"- MOVE hráč {a.player_id}: " + "→".join(cesta))
            out += ["    - " + (ps.event_str(e) if e["type"] != "TURNOVER!" else "⛔ TURNOVER") for e in ev]
            i = j
        else:
            out.append("- " + kt.popis_kroku(kroky[i]))
            out += ["    - " + ps.event_str(e) for e in kroky[i]["events"] if e["type"] != "MOVE"]
            i += 1
    return out


def bunka(m):
    if m["td"]:
        kde = "TD"
    elif m["do_td"] is not None:
        kde = f"{m['do_td']} do TD, rohy {m['rohy']}, sousedé {m['sousede']}, dosah {m['dosah_na_nosice']}"
    else:
        kde = "bez míče"
    return (f"{kde} · blok {m['blok']} blitz {m['blitz']} faul {m['faul']} přihr. {m['prihravka']}"
            + (" · ⛔ turnover" if m["turnover"] else ""))


def agregat(miry):
    """Souhrn plánů jedné AI v jedné pozici přes semínka."""
    n = len(miry)
    s_micem = [m for m in miry if m["do_td"] is not None]
    prum = lambda xs: round(sum(xs) / len(xs), 2) if xs else None
    priciny = {}
    for m in miry:
        if m["turnover"]:
            priciny[m["turnover_pricina"]] = priciny.get(m["turnover_pricina"], 0) + 1
    return {"n": n, "turnover": sum(m["turnover"] for m in miry), "td": sum(m["td"] for m in miry),
            "mame_mic": sum(m["mame_mic"] for m in miry),
            "do_td": prum([m["do_td"] for m in s_micem]), "rohy": prum([m["rohy"] for m in s_micem]),
            "dosah": prum([m["dosah_na_nosice"] for m in s_micem if m["dosah_na_nosice"] is not None]),
            "blok": prum([m["blok"] for m in miry]), "blitz": sum(m["blitz"] > 0 for m in miry),
            "faul": sum(m["faul"] > 0 for m in miry), "prihravka": sum(m["prihravka"] > 0 for m in miry),
            "kraj": prum([m["kraj"] for m in miry]), "kontakt": prum([m["kontakt"] for m in miry]),
            "hralo_pred_turnoverem": prum([m["hralo_pred_turnoverem"] for m in miry if m["turnover"]]),
            "priciny": priciny}


def veta(a):
    pr = ", ".join(f"{k} {v}×" for k, v in sorted(a["priciny"].items(), key=lambda kv: -kv[1]))
    return (f"turnover {a['turnover']}/{a['n']}" + (f" ({pr})" if pr else "") + f" · TD {a['td']}/{a['n']} · "
            f"s míčem na konci {a['mame_mic']}/{a['n']} · průměr do TD {a['do_td']}, rohy {a['rohy']}, "
            f"dosah soupeřů {a['dosah']} · bloků {a['blok']} · blitz {a['blitz']}/{a['n']} · faul {a['faul']}/{a['n']}")


def main():
    os.makedirs(OUT, exist_ok=True)
    tahy, konec, akce_vse = prehraj()
    n_akci = over_prehrani(akce_vse, konec)
    print(f"engine: {ROOT}")
    print(f"přehrání OK: {n_akci} akcí shodně s výpisem partie, skóre 2:0, tahů {len(tahy)}", flush=True)

    souhrn = []
    for i, t in enumerate(tahy, 1):
        st, strana = t["start"], t["strana"]
        kdo = "trpaslici" if strana == bb.TeamSide.HOME else "elfove"
        ms, radky_s = mira(st, t["konec"], strana, t["kroky"])
        plany, t0 = [], time.time()
        for ai, sd in [("macro_mcts", s) for s in SEMINKA] + [("greedy", s) for s in SEMINKA]:
            # semínko kostek jiné pro každý tah: se stejným by první hod plánu padl ve všech 32 pozicích stejně
            kroky, po, hotovo = kt.plan_tahu(st, ai=ai, seed=1000 * i + sd, mcts_iterations=ITERACE)
            kroky = list(kroky)
            m, radky = mira(st, po, strana, kroky)
            m["dokonceno"] = bool(hotovo)
            plany.append({"ai": ai, "seminko": sd, "kroky": kroky, "po": po, "mira": m, "kontrola": radky})
        cas = time.time() - t0

        md = [f"# Tah {i}: {t['polocas']}. poločas, kolo {t['kolo']}, na tahu {kdo}", "",
              "## Pozice na začátku tahu", "```", ps.board(st), "```", "",
              "## Skutečně zahráno", *strucne(t["kroky"]), "", "```", ps.board(t["konec"]), "```", "",
              "Kontrola tahu:", *["- " + r for r in radky_s], ""]
        p1 = plany[0]
        md += [f"## Plán AI (macro_mcts, semínko {p1['seminko']}, {ITERACE} iterací) — nanečisto",
               *strucne(p1["kroky"]), "", "```", ps.board(p1["po"]), "```", "",
               "Kontrola tahu:", *["- " + r for r in p1["kontrola"]], ""]
        md += ["## Všechny plány vedle skutečnosti", "", "| kdo | výsledek |", "|---|---|",
               f"| skutečnost | {bunka(ms)} |"]
        md += [f"| {p['ai']} s{p['seminko']} | {bunka(p['mira'])} |" for p in plany]
        md += ["", "Souhrn přes semínka:", ""] + [f"- {ai}: {veta(agregat([p['mira'] for p in plany if p['ai'] == ai]))}"
                                                 for ai in ("macro_mcts", "greedy")]
        g = plany[len(SEMINKA)]
        md += ["", "## Plán greedy (semínko 1)", *strucne(g["kroky"]), "",
               "Kontrola tahu:", *["- " + r for r in g["kontrola"]], ""]
        with open(os.path.join(OUT, f"tah_{i:02d}_p{t['polocas']}_k{t['kolo']}_{kdo}.md"), "w") as f:
            f.write("\n".join(md) + "\n")

        souhrn.append({"tah": i, "polocas": t["polocas"], "kolo": t["kolo"], "kdo": kdo, "skutecnost": ms,
                       "macro_mcts": agregat([p["mira"] for p in plany if p["ai"] == "macro_mcts"]),
                       "greedy": agregat([p["mira"] for p in plany if p["ai"] == "greedy"]),
                       "plany": [{"ai": p["ai"], "seminko": p["seminko"], **p["mira"]} for p in plany]})
        print(f"tah {i:2d} {kdo:9s} p{t['polocas']} k{t['kolo']}: {cas:5.1f} s | skut.: {bunka(ms)}\n"
              f"      macro: {veta(souhrn[-1]['macro_mcts'])}\n      greedy: {veta(souhrn[-1]['greedy'])}", flush=True)

    with open(os.path.join(OUT, "souhrn.json"), "w") as f:
        json.dump(souhrn, f, ensure_ascii=False, indent=1)

    # tabulka všech tahů: skutečnost vedle plánů AI
    md = ["# Zkouška režimu „AI plánuje tah“ na partii 02.–03.10. — tabulka tahů", "",
          f"Engine `4e142640` + vazba z `d07d7ed2`; v každé pozici {len(SEMINKA)} plánů macro_mcts ({ITERACE} iterací, "
          f"`weights_best.json`) a {len(SEMINKA)} plánů greedy, každý s vlastními kostkami. Přehrání ověřeno: "
          f"{n_akci} akcí shodně s `partie_vypis.md`, skóre 2:0.", "",
          "Sloupce plánu: turnover / TD / s míčem na konci (z 20) · průměr polí do TD · rohy klece · kolik soupeřů "
          "dosáhne na nosiče · blitz a faul (z 20). „—“ = tým na konci tahu míč nedrží.", "",
          "| tah | pol. | kolo | kdo | skutečnost | macro_mcts | na čem padá macro_mcts | greedy |", "|---|---|---|---|---|---|---|---|"]

    def kratce(a):
        kde = f"do TD {a['do_td']}, rohy {a['rohy']}, dosah {a['dosah']}" if a["do_td"] is not None else "—"
        return (f"TO {a['turnover']} · TD {a['td']} · míč {a['mame_mic']} · {kde} · "
                f"blitz {a['blitz']} · faul {a['faul']}")

    for t in souhrn:
        pr = ", ".join(f"{k} {v}×" for k, v in sorted(t["macro_mcts"]["priciny"].items(), key=lambda kv: -kv[1]))
        md.append(f"| {t['tah']} | {t['polocas']} | {t['kolo']} | {t['kdo']} | {bunka(t['skutecnost'])} | "
                  f"{kratce(t['macro_mcts'])} | {pr} | {kratce(t['greedy'])} |")
    md += ["", "## Součty", "", "| kdo | AI | turnover | TD | před turnoverem hrálo jiných hráčů (průměr) | turnover hned první akcí |",
           "|---|---|---|---|---|---|"]
    for kdo in ("trpaslici", "elfove"):
        sk = [t["skutecnost"] for t in souhrn if t["kdo"] == kdo]
        md.append(f"| {kdo} | skutečnost | {sum(m['turnover'] for m in sk)}/{len(sk)} | {sum(m['td'] for m in sk)} | | |")
        for ai in ("macro_mcts", "greedy"):
            pl = [p for t in souhrn if t["kdo"] == kdo for p in t["plany"] if p["ai"] == ai]
            to = [p for p in pl if p["turnover"]]
            md.append(f"| {kdo} | {ai} | {len(to)}/{len(pl)} = {len(to) / len(pl):.0%} | {sum(p['td'] for p in pl)} | "
                      f"{sum(p['hralo_pred_turnoverem'] for p in to) / max(1, len(to)):.1f} | "
                      f"{sum(p['hralo_pred_turnoverem'] == 0 for p in to)}/{len(to)} |")
    with open(os.path.join(OUT, "tabulka_tahu.md"), "w") as f:
        f.write("\n".join(md) + "\n")


if __name__ == "__main__":
    main()
