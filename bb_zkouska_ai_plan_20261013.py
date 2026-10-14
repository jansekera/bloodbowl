#!/usr/local/bin/python3.8
"""P205 část A (14.10.2026): zkouška „AI plánuje tah“ na partii lidé × orkové 12.–13.10. (1:1).

Log partie (`evidence/zkouska_ai_plan_20261013/vstup_prikazy_partie_20261012.txt`) se přehraje
stejně jako na serveru `bb_play_session_20261012.py` (kouč, los, otázky kouče s `odp`); v pozici
na začátku každého tahu AI naplánuje celý tah nanečisto (vlastní kostky) a plán se položí vedle
tahu, který se skutečně hrál. Míry a výpis tahu jsou tytéž jako v `bb_zkouska_ai_plan_20261006.py`.

⚠️ Log se přehraje jen na enginu značky `zkouska-ai-plan-20261013-baze` (worktree): od 14.10. se
server ptá i na follow-up a lékárníka (P201) a vhazování je o pole kratší — log na ně odpovědi nemá.

  /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261013.py            # přehrání + plány
  /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261013.py --kontrola # jen kontroly přehrání

Výstup: evidence/zkouska_ai_plan_20261013/ (tah_NN_*.md, tabulka_tahu.md, souhrn.json).
"""
import json
import os
import sys
import time

TADY = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TADY)
sys.path.insert(0, os.path.join(TADY, "engine", "build"))
import bb_engine as bb  # noqa: E402
import bb_kontrola_tahu as kt  # noqa: E402
import bb_play_session_20261012 as ps  # noqa: E402
import bb_zkouska_ai_plan_20261006 as z6  # noqa: E402

z6.ps = ps   # míry a výpis tahu z 06.10., výpis událostí z dnešního serveru

OUT = os.path.join(TADY, "evidence", "zkouska_ai_plan_20261013")
LOG = os.path.join(OUT, "vstup_prikazy_partie_20261012.txt")
SEMINKA = list(range(1, 21))
ITERACE = 50
DRUHA = ps.druha


def strana(t):
    return bb.TeamSide.HOME if t.upper() == "HOME" else bb.TeamSide.AWAY


def prehraj(radky):
    """Přehraje log jako server partie. Vrací (tahy, konečný stav, odmítnuté řádky).
    Server zapisuje příkaz do logu dřív, než ho provede, takže v logu jsou i řádky, které
    odmítl (akce při čekající otázce, chyba) — přehrání je odmítne stejně a vrátí je."""
    state, dice = ps.new_game()
    tahy, odmitnute = [], []
    coach, cekajici, los_vitez, prvni_kope = None, None, None, None
    last_actions = []
    tah_klic, tah_start, tah_kroky = None, None, []

    def proved(a, answers):
        nonlocal cekajici
        r, events, q = bb.execute_action_coach(state, a, dice, coach, answers)
        if q is not None:
            cekajici = {"action": a, "answers": list(answers), "q": q}
            return
        cekajici = None
        bb.clear_manual_block_choices()
        tah_kroky.append({"action": a, "turnover": r.turnover, "events": events})

    for n, line in enumerate(radky + [None], 1):
        klic = (state.active_team, state.home_team.turn_number, state.away_team.turn_number, state.half,
                state.home_team.score, state.away_team.score)
        if klic != tah_klic or line is None:
            if tah_start is not None and tah_start.phase == bb.GamePhase.PLAY and tah_kroky:
                ts = tah_start.home_team if tah_start.active_team == bb.TeamSide.HOME else tah_start.away_team
                tahy.append({"start": tah_start, "konec": state.clone(), "strana": tah_start.active_team,
                             "kroky": tah_kroky, "polocas": tah_start.half, "kolo": ts.turn_number})
            tah_klic, tah_kroky = klic, []
            tah_start = state.clone() if state.phase == bb.GamePhase.PLAY else None
        if line is None:
            break
        p = line.split()
        c = p[0]
        try:
            if cekajici and c not in ("odp", "push", "sidestep", "follow"):
                odmitnute.append((n, line, "čeká otázka kouče"))
            elif c == "kouc":
                coach = strana(p[1])
            elif c == "los":
                bb.roll_match_weather(state, dice)
                vitez = bb.TeamSide.HOME if dice.roll_d6() <= 3 else bb.TeamSide.AWAY
                if vitez == coach:
                    los_vitez = vitez
                else:
                    kopou = bb.ai_toss_elects_kick(ps.ROSTERS[0] if vitez == bb.TeamSide.HOME else ps.ROSTERS[1])
                    prvni_kope = vitez if kopou else DRUHA(vitez)
                    ps.zahaj(state, dice, prvni_kope)
            elif c == "volba":
                prvni_kope = los_vitez if p[1].lower() == "kopat" else DRUHA(los_vitez)
                los_vitez = None
                ps.zahaj(state, dice, prvni_kope)
            elif c == "actions":
                last_actions = list(bb.get_available_actions(state))
            elif c == "do":
                i = int(p[-1])
                if i >= len(last_actions):
                    odmitnute.append((n, line, f"akce mimo nabídku ({len(last_actions)})"))
                else:
                    proved(last_actions[i], [])
            elif c == "odp":
                if not cekajici:
                    odmitnute.append((n, line, "žádná otázka nečeká"))
                else:
                    a = ps.preloz_odpoved(cekajici["q"], p[1])
                    if a is None:
                        odmitnute.append((n, line, "odpověď není mezi možnostmi"))
                    else:
                        proved(cekajici["action"], cekajici["answers"] + [a])
            elif c == "push":
                bb.set_manual_push(int(p[1]), int(p[2]))
            elif c == "sidestep":
                bb.set_manual_side_step(int(p[1]), int(p[2]))
            elif c == "follow":
                bb.set_manual_follow_up(p[1] == "1")
            elif c == "blitzfrom":
                bb.set_manual_blitz_square(int(p[1]), int(p[2]))
            elif c == "vykop":
                bb.setup_drive(state, ps.ROSTERS[0], ps.ROSTERS[1], strana(p[1]), dice)
                bb.simple_kickoff(state, dice)
            elif c == "polocas":
                kick = strana(p[1]) if len(p) == 2 else DRUHA(prvni_kope)
                bb.setup_second_half(state, ps.ROSTERS[0], ps.ROSTERS[1], kick, dice)
                bb.simple_kickoff(state, dice)
            else:
                raise SystemExit(f"neznámý příkaz v logu (ř. {n}): {line}")
        except SystemExit:
            raise
        except Exception as e:   # server chybu jen vypíše a pokračuje
            odmitnute.append((n, line, f"CHYBA: {e}"))
    return tahy, state, odmitnute


def kdo_str(s):
    return "lide" if s == bb.TeamSide.HOME else "orkove"


def popis_prehrani(tahy, konec):
    td = [(t["polocas"], t["kolo"], kdo_str(t["strana"])) for t in tahy if z6.mira(t["start"], t["konec"], t["strana"], t["kroky"])[0]["td"]]
    return {"skore": (konec.home_team.score, konec.away_team.score), "tahu": len(tahy), "td": td}


def over_prehrani(tahy, konec, odmitnute):
    """Kontrola měřidla: partie skončila 1:1, 32 tahů, TD lidí 1. pol. kolo 5, orků 2. pol. kolo 7."""
    d = popis_prehrani(tahy, konec)
    ocek = {"skore": (1, 1), "tahu": 32, "td": [(1, 5, "lide"), (2, 7, "orkove")]}
    if d != ocek:
        raise SystemExit(f"⛔ přehrání nesedí s partií: {d}, čekáno {ocek}; odmítnuto {odmitnute}")
    return d


def kontroly():
    radky = [l.strip() for l in open(LOG) if l.strip()]
    tahy, konec, odm = prehraj(radky)
    d = over_prehrani(tahy, konec, odm)
    print(f"přehrání OK: {d}; odmítnutých řádků {len(odm)}")
    for o in odm:
        print("   odmítnuto:", o)
    # pozitivní kontrola: bez prvního `odp` se přehrání musí rozejít
    i = next(j for j, l in enumerate(radky) if l.startswith("odp"))
    tahy2, konec2, odm2 = prehraj(radky[:i] + radky[i + 1:])
    d2 = popis_prehrani(tahy2, konec2)
    rozeslo = d2 != d or len(odm2) != len(odm)
    print(f"pozitivní kontrola (vynechán ř. {i + 1} `{radky[i]}`): {d2}, odmítnuto {len(odm2)} ⇒ "
          + ("rozešlo se ✅" if rozeslo else "⛔ NEROZEŠLO SE — měřidlo nic nekontroluje"))
    if not rozeslo:
        raise SystemExit(1)
    return tahy, d


def main():
    tahy, d = kontroly()
    if "--kontrola" in sys.argv:
        return
    souhrn = []
    for i, t in enumerate(tahy, 1):
        st, strana_t = t["start"], t["strana"]
        kdo = kdo_str(strana_t)
        ms, radky_s = z6.mira(st, t["konec"], strana_t, t["kroky"])
        plany, t0 = [], time.time()
        for ai, sd in [("macro_mcts", s) for s in SEMINKA] + [("greedy", s) for s in SEMINKA]:
            kroky, po, hotovo = kt.plan_tahu(st, ai=ai, seed=1000 * i + sd, mcts_iterations=ITERACE)
            kroky = list(kroky)
            m, radky = z6.mira(st, po, strana_t, kroky)
            m["dokonceno"] = bool(hotovo)
            plany.append({"ai": ai, "seminko": sd, "kroky": kroky, "po": po, "mira": m, "kontrola": radky})
        cas = time.time() - t0

        md = [f"# Tah {i}: {t['polocas']}. poločas, kolo {t['kolo']}, na tahu {kdo}", "",
              "## Pozice na začátku tahu", "```", ps.board(st), "```", "",
              "## Skutečně zahráno", *z6.strucne(t["kroky"]), "", "```", ps.board(t["konec"]), "```", "",
              "Kontrola tahu:", *["- " + r for r in radky_s], ""]
        p1 = plany[0]
        md += [f"## Plán AI (macro_mcts, semínko {p1['seminko']}, {ITERACE} iterací) — nanečisto",
               *z6.strucne(p1["kroky"]), "", "```", ps.board(p1["po"]), "```", "",
               "Kontrola tahu:", *["- " + r for r in p1["kontrola"]], ""]
        md += ["## Všechny plány vedle skutečnosti", "", "| kdo | výsledek |", "|---|---|",
               f"| skutečnost | {z6.bunka(ms)} |"]
        md += [f"| {p['ai']} s{p['seminko']} | {z6.bunka(p['mira'])} |" for p in plany]
        md += ["", "Souhrn přes semínka:", ""] + [f"- {ai}: {z6.veta(z6.agregat([p['mira'] for p in plany if p['ai'] == ai]))}"
                                                 for ai in ("macro_mcts", "greedy")]
        g = plany[len(SEMINKA)]
        md += ["", "## Plán greedy (semínko 1)", *z6.strucne(g["kroky"]), "",
               "Kontrola tahu:", *["- " + r for r in g["kontrola"]], ""]
        with open(os.path.join(OUT, f"tah_{i:02d}_p{t['polocas']}_k{t['kolo']}_{kdo}.md"), "w") as f:
            f.write("\n".join(md) + "\n")

        souhrn.append({"tah": i, "polocas": t["polocas"], "kolo": t["kolo"], "kdo": kdo, "skutecnost": ms,
                       "macro_mcts": z6.agregat([p["mira"] for p in plany if p["ai"] == "macro_mcts"]),
                       "greedy": z6.agregat([p["mira"] for p in plany if p["ai"] == "greedy"]),
                       "plany": [{"ai": p["ai"], "seminko": p["seminko"], **p["mira"]} for p in plany]})
        print(f"tah {i:2d} {kdo:6s} p{t['polocas']} k{t['kolo']}: {cas:5.1f} s | skut.: {z6.bunka(ms)}\n"
              f"      macro: {z6.veta(souhrn[-1]['macro_mcts'])}\n      greedy: {z6.veta(souhrn[-1]['greedy'])}", flush=True)

    with open(os.path.join(OUT, "souhrn.json"), "w") as f:
        json.dump(souhrn, f, ensure_ascii=False, indent=1)

    md = ["# Zkouška „AI plánuje tah“ na partii lidé × orkové 12.–13.10. — tabulka tahů", "",
          f"Engine = značka `zkouska-ai-plan-20261013-baze`; v každé pozici {len(SEMINKA)} plánů macro_mcts "
          f"({ITERACE} iterací, `weights_best.json`) a {len(SEMINKA)} plánů greedy, každý s vlastními kostkami. "
          f"Přehrání ověřeno: skóre {d['skore'][0]}:{d['skore'][1]}, {d['tahu']} tahů, TD {d['td']}.", "",
          "Sloupce plánu: turnover / TD / s míčem na konci (z 20) · průměr polí do TD · rohy klece · kolik soupeřů "
          "dosáhne na nosiče · blitz a faul (z 20). „—“ = tým na konci tahu míč nedrží.", "",
          "| tah | pol. | kolo | kdo | skutečnost | macro_mcts | na čem padá macro_mcts | greedy |", "|---|---|---|---|---|---|---|---|"]

    def kratce(a):
        kde = f"do TD {a['do_td']}, rohy {a['rohy']}, dosah {a['dosah']}" if a["do_td"] is not None else "—"
        return f"TO {a['turnover']} · TD {a['td']} · míč {a['mame_mic']} · {kde} · blitz {a['blitz']} · faul {a['faul']}"

    for t in souhrn:
        pr = ", ".join(f"{k} {v}×" for k, v in sorted(t["macro_mcts"]["priciny"].items(), key=lambda kv: -kv[1]))
        md.append(f"| {t['tah']} | {t['polocas']} | {t['kolo']} | {t['kdo']} | {z6.bunka(t['skutecnost'])} | "
                  f"{kratce(t['macro_mcts'])} | {pr} | {kratce(t['greedy'])} |")
    md += ["", "## Součty", "", "| kdo | AI | turnover | TD | před turnoverem hrálo jiných hráčů (průměr) | turnover hned první akcí |",
           "|---|---|---|---|---|---|"]
    for kdo in ("lide", "orkove"):
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
