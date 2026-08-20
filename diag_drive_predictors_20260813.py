"""Předpovídá některá z kontrol výsledek DRIVU? (13.08.)

Brána zlepšila K9a/K29/K34/K31 a chess se nehnul -- takže se ptáme, jestli
ty kontroly vůbec souvisí s tím, jak drive dopadne. Drive se dělí stejně
jako v diag_drive_failure: hranice = start hry, změna půle, log po TD.
"""
import sys, glob, math, gzip, json
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from collections import defaultdict
from diag_rules_checks_20260812 import (load, players, threatens, adj, STANDING,
                                        key, TACKLE, dodge_cost, DODGE_COST_THRESHOLD,
                                        phase_floor)
from diag_exposure_scan_20260812 import Board, predictors
# P25 (17.08.): atribuce TD podle STŘELCE. Oprava `14c7d035` ze 14.08. šla jen
# do `diag_drive_failure` a tenhle sourozenec ji nedostal -- přitom právě on
# vyrobil σ-tabulku, podle které je seřazená celá fronta úkolů.
from diag_drive_failure_20260811 import td_scorer_side, build_id_map

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/diag_replay_mine_20260811b_data'
rows = []
for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    # P25: strana se ČETLA z jmen prvních tří domácích hráčů, ačkoli je race
    # přímo v datech. Kdyby první tři nebyli Longbeard/Slayer, skript by tiše
    # měřil SOUPEŘE jako nás -- a nic by to neřeklo.
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    fwd = 1 if ours == "home" else -1
    endzone = 25 if fwd == 1 else 0
    logs = r["turn_logs"]
    id_map = build_id_map(r)
    # hranice drivů
    starts = [0]
    for i in range(1, len(logs)):
        if logs[i]["half"] != logs[i-1]["half"] or logs[i-1].get("touchdown"):
            starts.append(i)
    starts.append(len(logs))
    for si in range(len(starts) - 1):
        a, b = starts[si], starts[si+1]
        seg = logs[a:b]
        ours_turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
        if len(ours_turns) < 7:   # jen PLNÉ drivy (uživatel 13.08.)
            continue
        # ⛔ BYLO: `logs[i]["active_team"] == ours`, tedy TD se přisuzoval tomu,
        # kdo byl na tahu. CRP ale umožňuje TD z odsunu do endzony v kole
        # SOUPEŘE -- 15 z 2183 TD (0,7 %). Tady se to navíc nesčítá jen jako
        # šum: `scored` je CÍLOVÁ PROMĚNNÁ celé regrese.
        scored = any(td_scorer_side(logs[i], id_map) == ours for i in range(a, b))
        # sesbírej kontroly přes naše kola v drivu
        acc = defaultdict(list)
        for i in ours_turns:
            if i + 1 >= len(logs) or logs[i+1]["half"] != logs[i]["half"]:
                continue
            E = logs[i+1]
            us = players(E, ours); them = players(E, "away" if ours=="home" else "home")
            car = next((p for p in us if p["has_ball"]), None)
            blocks = sum(1 for e in logs[i]["events"] if e["type"] == "BLOCK"
                         and any(p["id"] == e["player_id"] for p in us))
            acc["K33_blok"].append(1.0 if blocks else 0.0)
            acc["blokůdo"].append(float(blocks))
            if car is None:
                continue
            diag = [(car["x"]+dx, car["y"]+dy) for dx in (-1,1) for dy in (-1,1)]
            occ = {(p["x"],p["y"]): p for p in us}
            filled = [d for d in diag if d in occ]
            threat = [(p["x"],p["y"]) for p in them if threatens(p)]
            dirty = [d for d in filled if any(adj(d,t) for t in threat)]
            acc["rohů_všech"].append(float(len(filled)))
            # Uživatelovo pravidlo od 04.08.: roh sousedící s nepřítelem =
            # klec bez rohu. Do dneška se nikdy neměřilo. "Počet rohů" tedy
            # míchá dvě různé věci; správná veličina je počet ČISTÝCH rohů.
            acc["rohů_ČISTÝCH"].append(float(len(filled) - len(dirty)))
            # Špinavý roh není jen "chybějící roh": stojí tam NAŠE tělo, které
            # nebije a nekryje. Proto se měří zvlášť od počtu čistých.
            acc["rohů_ŠPINAVÝCH"].append(float(len(dirty)))
            if filled:
                acc["K29_čisté"].append(0.0 if dirty else 1.0)
            # ⭐ K29⭐⭐ — PRAVIDLO KLECE jako JEDNA veličina (19.08.).
            # Tabulka dosud obsahovala jen jeho ROZLOŽENÉ kusy (počet rohů,
            # počet čistých, počet špinavých) a ty se chovaly každý jinak.
            # Cíl je ale KONJUNKCE: tři rohy ze čtyř nejsou 75 % klece,
            # je to otevřená klec.
            orth = [(car["x"] + dx, car["y"] + dy)
                    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))]
            them_at = {(p["x"], p["y"]) for p in them}
            extra = ([o for o in orth if o in occ and not occ[o]["has_ball"]]
                     + [o for o in orth if o in them_at]
                     + [d for d in diag if d in them_at])
            acc["PRAVIDLO_klece"].append(
                1.0 if (len(filled) == 4 and not dirty and not extra) else 0.0)

            # ⭐ ODPOR KORIDORU (K9b, 18.08.) — poprvé v σ-tabulce. Do dneška
            # žádný korpus tohle pole nevezl, takže veličina existovala v enginu
            # a nikdy se neptalo, jestli něco předpovídá. -1 = nepočítalo se
            # (nosič mimo hru), takový záznam se NEPŘIČÍTÁ, aby se sentinel
            # nevydával za nulový odpor.
            _cr = logs[i].get("corridor_resistance", -1)
            if _cr is not None and _cr >= 0:
                acc["odpor_koridoru"].append(float(_cr))

            P = predictors(Board(E, ours))
            if "REACH0" in P:
                acc["K34_reach0"].append(1.0 if P["REACH0"] == 0 else 0.0)
                acc["REACH0_počet"].append(float(P["REACH0"]))
            acc["K35_fb2"].append(1.0 if P["FB2"] <= 1 else 0.0)
            turns_left = 9 - logs[i]["turn"]
            carS = next((p for p in players(logs[i], ours) if p["has_ball"]), None)
            if turns_left > 0 and carS and carS["id"] == car["id"]:
                need = math.ceil(abs(endzone - carS["x"]) / turns_left)
                got = (car["x"] - carS["x"]) * fwd
                acc["K9a_splněno"].append(1.0 if got >= need else 0.0)
                acc["Δx"].append(float(got))
                # --- T0.1 (20.08.): K9 po FÁZÍCH, přes TUTÉŽ funkci, kterou
                # volá kontrola K9c. Kdyby se logika zkopírovala, měřila by
                # σ-tabulka jinou veličinu než kontrola a nikdo by se to
                # nedozvěděl (táž lekce jako corridorResistance, K9b).
                _ph, _cap, _needph = phase_floor(
                    logs[i], carS, got, need, players(logs[i], ours), endzone)
                acc["K9c_splněno"].append(1.0 if got >= _needph else 0.0)
                # ⭐ Kolikrát rovnoměrná podlaha žádala mechanicky nemožné --
                # přesně to, kvůli čemu se K9 přepisuje.
                acc["K9a_žádala_nemožné"].append(1.0 if _cap < need else 0.0)
                acc["K9c_rezerva"].append(float(got - _needph))
        if acc:
            rows.append((scored, {k: sum(v)/len(v) for k, v in acc.items() if v}))

# P25 (17.08.): PŮVOD ČÍSLA MUSÍ CESTOVAT S ČÍSLEM.
# σ-tabulka z tohohle skriptu řadí celou frontu úkolů, ale v knize stojí bez
# korpusu, bez data a bez commitu enginu -- jako by to byla vlastnost hry.
# Není: na `20260811b` (195 drivů) vyjde K33 0,6σ a REACH0 −1,8σ, na
# `20260813_gate` (194 drivů, jiný engine) 4,0σ a −4,0σ. Táž velikost vzorku,
# jiná odpověď. Bez téhle hlavičky se to nedá ani zpětně poznat.
import os, subprocess
_head = ""
# ⛔ 19.08.: obě dosavadní cesty minuly `corpus_baseline_*_data`, protože otisk
# leží v SOUROZENCI bez přípony `_data` (`corpus_baseline_YYYYMMDD/ENGINE_HEAD`),
# ne nad datovým adresářem. Hlavička pak tiskla „korpus bez otisku" u korpusu,
# který otisk MÁ -- tedy přesně ta vada, kterou si tenhle skript vynucuje (P22).
_root = DATA.rstrip("/")
_sib = _root[:-5] if _root.endswith("_data") else _root
for _cand in (os.path.join(DATA, "..", "ENGINE_HEAD"),
              os.path.join(os.path.dirname(_root) or ".", "ENGINE_HEAD"),
              os.path.join(_sib, "ENGINE_HEAD")):
    try:
        _head = open(_cand).read().strip()[:8]; break
    except OSError:
        pass
print(f"korpus: {DATA}")
print(f"her: {len(glob.glob(DATA + '/*.json.gz'))}"
      f"   engine korpusu: {_head or 'NEZNÁMÝ (korpus bez otisku — viz P22)'}")
print(f"filtr: jen PLNÉ drivy (>=7 našich kol) — je to VÝBĚR, ne vzorek:")
print(f"       drive, který skončí dřív, se sem nedostane, a to koreluje s výsledkem")
print(f"drivů: {len(rows)}, z toho se skórováním: {sum(1 for s,_ in rows if s)}")
print(f"jednotka n u každého řádku = DRIVY, ne kola ani zápasy\n")
keys = sorted({k for _, d in rows for k in d})
print(f"{'veličina':<16}{'TD drivy':>11}{'bez TD':>11}{'rozdíl':>10}{'σ':>8}{'n':>7}")
for k in keys:
    yes = [d[k] for s, d in rows if s and k in d]
    no  = [d[k] for s, d in rows if not s and k in d]
    if len(yes) < 5 or len(no) < 5:
        continue
    my, mn = sum(yes)/len(yes), sum(no)/len(no)
    vy = sum((x-my)**2 for x in yes)/max(1,len(yes)-1)
    vn = sum((x-mn)**2 for x in no)/max(1,len(no)-1)
    se = math.sqrt(vy/len(yes) + vn/len(no))
    sig = (my-mn)/se if se else 0
    print(f"{k:<16}{my:>11.3f}{mn:>11.3f}{my-mn:>+10.3f}{sig:>7.1f}σ{len(yes)+len(no):>7}")
