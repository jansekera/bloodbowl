#!/usr/bin/env python3
"""L vs SLOUPCE (spec 17.4 + 17.6) — co korpus doopravdy obsahuje. 20.08.2026.

Krok 1: klasifikace NAŠICH obranných kol (snímek na ZAČÁTKU soupeřova kola,
tj. výsledek našeho předchozího kola): KONTAKT / ODSTUP-SLOUPEC /
ODSTUP-SCREEN / 1 TĚLO / ANI JEDNO.  Absolutní počty + podíly, jmenovatele
vytištěné.

Krok 2 (spec 17.6): metrika "nemá kam" — počet volných sousedních polí
nosiče a kolik z nich stojí hod (logika dodge_cost, práh 0,20).

Krok 3: výsledky soupeřova kola (postup míče, ztráta míče, pády nosiče na
dodge, TD drivu) podle klasifikace, podle metriky 17.6, a stratifikace podle
převahy stojících těl (konfundér iniciativy).  Protitvrzení webu: po bloku
na naše kontaktní tělo — projde nosič?

Krok 4: díra pevných Y {3,5,7,9,11}: nosič na křídle (y 0-1 / 13-14).

⚠️ Vše je KORELACE na korpusu, kde engine ani L, ani sloupce cíleně nehraje
(audit macro_actions.cpp 20.08.) — kategorie vznikají náhodou.
"""
import sys, glob
from collections import Counter, defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
SCREEN_YS = (3, 5, 7, 9, 11)


def adjc(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by)) == 1


def p_fail_exit(car, markers, our_stand, tx, ty):
    """P(selhání dodge) nosiče `car` na pole (tx,ty); markers = naši stojící
    sousedé nosiče.  need = 7 - AG - 1 + (naše TZ na cílovém poli)."""
    tzT = sum(1 for q in our_stand if adjc(q["x"], q["y"], tx, ty))
    need = max(2, min(6, 7 - car["ag"] - 1 + tzT))
    p = (need - 1) / 6.0
    if R.key(car) in R.DODGE and not any(R.key(m) in R.TACKLE for m in markers):
        p *= p
    return p


def main():
    paths = []
    for a in sys.argv[1:]:
        paths += sorted(glob.glob(a))
    st = Counter()
    hist_based = Counter()      # kolik našich těl v kontaktu
    hist_screen = Counter()     # kolik screenerů bez kontaktu
    # výsledkové akumulátory: cat -> dict of lists
    OUT = defaultdict(lambda: {"dx": [], "lost": [], "td_turn": [], "td_drive": [],
                               "dodge_fail": [], "n": 0})
    # metrika 17.6: koše podle počtu LEVNÝCH únikových polí
    M = defaultdict(lambda: {"dx": [], "lost": [], "td_drive": []})
    OLD = defaultdict(lambda: {"dx": [], "lost": [], "td_drive": []})  # starý metr: stojí u nosiče někdo náš
    STRAT = defaultdict(lambda: {"dx": [], "lost": []})  # (převaha, kontakt?) -> outcomes
    RACE = defaultdict(lambda: defaultdict(lambda: {"dx": [], "lost": [], "td_drive": []}))
    GAP = defaultdict(lambda: {"dx": [], "lost": [], "thru": 0, "n": 0})  # protitvrzení
    WING = defaultdict(lambda: {"dx": [], "lost": [], "lane_cov": 0, "n": 0})
    FREE = {"free": [], "cheap": []}

    for path in paths:
        r = R.load(path)
        if r["home_race"] == "dwarf":
            ours, theirs = "home", "away"
        elif r["away_race"] == "dwarf":
            ours, theirs = "away", "home"
        else:
            st["her bez trpaslíka (přeskočeno)"] += 1
            continue
        opprace = r[f"{theirs}_race"]
        # home skóruje na x=25, away na x=0 (ověřeno g0001)
        oppfwd = -1 if ours == "home" else 1
        our_ez = 0 if ours == "home" else 25
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != theirs:
                continue
            their_all = R.players(S, theirs)
            car = next((p for p in their_all if p["has_ball"]), None)
            if car is None:
                continue
            st["obranná kola (soupeř aktivní a drží míč)"] += 1
            if car["state"] != STANDING:
                st["  nosič neležel = stojící"] += 0  # jen pro čitelnost
                st["  nosič LEŽÍ (přeskočeno z klasifikace)"] += 1
                continue
            st["  nosič stojí — KLASIFIKOVÁNO"] += 1

            our_all = R.players(S, ours)
            our_stand = [p for p in our_all if p["state"] == STANDING]
            their_stand = [p for p in their_all if p["state"] == STANDING]
            occupied = {(p["x"], p["y"]) for p in our_all + their_all}

            based = [q for q in our_stand
                     if any(adjc(q["x"], q["y"], t["x"], t["y"]) for t in their_stand)]
            based_ids = {q["id"] for q in based}
            markers = [q for q in our_stand if adjc(q["x"], q["y"], car["x"], car["y"])]
            hist_based[min(len(based), 5)] += 1

            screeners = [q for q in our_stand if q["id"] not in based_ids
                         and 1 <= (q["x"] - car["x"]) * oppfwd <= 6
                         and abs(q["y"] - car["y"]) <= 3]
            if not based:
                hist_screen[min(len(screeners), 4)] += 1
            depth2 = any(abs(a["y"] - b["y"]) <= 1
                         and 1 <= (b["x"] - a["x"]) * oppfwd <= 2
                         for a in screeners for b in screeners if a["id"] != b["id"])

            if len(based) >= 2:
                cat = "KONTAKT (≥2 těla v base)"
            elif len(based) == 1:
                cat = "1 TĚLO v kontaktu (marker)"
            elif len(screeners) >= 2 and depth2:
                cat = "ODSTUP-SLOUPEC (≥2 screeneři, hloubka 2)"
            elif len(screeners) >= 2:
                cat = "ODSTUP-SCREEN jednořadý (≥2 screeneři)"
            else:
                cat = "ANI JEDNO"
            st[f"kategorie: {cat}"] += 1

            # ---- metrika 17.6 ----
            exits = [(car["x"] + dx, car["y"] + dy)
                     for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]
            free = [(x, y) for x, y in exits
                    if 0 <= x <= 25 and 0 <= y <= 14 and (x, y) not in occupied]
            if markers:
                cheap = [t for t in free
                         if p_fail_exit(car, markers, our_stand, *t) < R.DODGE_COST_THRESHOLD]
            else:
                cheap = free
            FREE["free"].append(len(free))
            FREE["cheap"].append(len(cheap))

            # ---- výsledky soupeřova kola ----
            td_turn = bool(S.get("touchdown"))
            E = logs[i + 1] if i + 1 < len(logs) else None
            same_half = E is not None and E["half"] == S["half"] and not td_turn
            dx = lost = None
            if td_turn:
                lost = 0
                # postup = vzdálenost míče k naší endzoně (dolní mez; bez
                # toho by kategorie s více TD měly postup uměle NÍZKÝ)
                dx = (our_ez - S["ball_x"]) * oppfwd
            elif same_half:
                their_E = R.players(E, theirs)
                carE = next((p for p in their_E if p["has_ball"]), None)
                if carE is None:
                    lost = 1
                else:
                    lost = 0
                    dx = (E["ball_x"] - S["ball_x"]) * oppfwd
            # TD drivu (první TD ve zbytku půle)
            td_drive = None
            for j in range(i, len(logs)):
                if logs[j]["half"] != S["half"]:
                    td_drive = 0
                    break
                if logs[j].get("touchdown"):
                    td_drive = 1 if logs[j]["active_team"] == theirs else 0
                    break
            if td_drive is None:
                td_drive = 0  # půle dohrána bez TD
            dodge_fails = sum(1 for e in S["events"]
                              if e["type"] == "DODGE" and e["player_id"] == car["id"]
                              and not e["success"])

            o = OUT[cat]
            o["n"] += 1
            o["td_turn"].append(1 if td_turn else 0)
            o["td_drive"].append(td_drive)
            o["dodge_fail"].append(dodge_fails)
            if lost is not None:
                o["lost"].append(lost)
            if dx is not None:
                o["dx"].append(dx)

            # metrika 17.6 vs starý metr — prediktivita
            kb = min(len(cheap), 3)
            ob = "u nosiče NIKDO náš" if not markers else "u nosiče stojí náš (starý metr)"
            for T, kk in ((M, kb), (OLD, ob)):
                if dx is not None:
                    T[kk]["dx"].append(dx)
                if lost is not None:
                    T[kk]["lost"].append(lost)
                T[kk]["td_drive"].append(td_drive)

            # stratifikace: převaha stojících těl × kontakt
            diff = len(our_stand) - len(their_stand)
            band = "převaha ZÁPORNÁ" if diff < 0 else ("NULA" if diff == 0 else "KLADNÁ")
            kon = "kontakt≥2" if len(based) >= 2 else "kontakt≤1"
            sk = (band, kon)
            if dx is not None:
                STRAT[sk]["dx"].append(dx)
            if lost is not None:
                STRAT[sk]["lost"].append(lost)

            # rasa soupeře
            rr = RACE[opprace][kon]
            if dx is not None:
                rr["dx"].append(dx)
            if lost is not None:
                rr["lost"].append(lost)
            rr["td_drive"].append(td_drive)

            # protitvrzení webu: blok na naše kontaktní tělo -> průnik?
            if based:
                blk = any(e["type"] == "BLOCK" and e["target_id"] in based_ids
                          for e in S["events"])
                gk = "po bloku na kontaktní tělo" if blk else "bez bloku na kontaktní tělo"
                g = GAP[gk]
                g["n"] += 1
                if dx is not None:
                    g["dx"].append(dx)
                    if dx >= 3:
                        g["thru"] += 1
                if lost is not None:
                    g["lost"].append(lost)

            # díra pevných Y: nosič na křídle
            wing = "nosič na KŘÍDLE (y 0-1 / 13-14)" if car["y"] in (0, 1, 13, 14) \
                else "nosič ve STŘEDU (y 2-12)"
            w = WING[wing]
            w["n"] += 1
            lane = any(abs(q["y"] - car["y"]) <= 1
                       and 1 <= (q["x"] - car["x"]) * oppfwd <= 8
                       for q in our_stand)
            w["lane_cov"] += 1 if lane else 0
            if dx is not None:
                w["dx"].append(dx)
            if lost is not None:
                w["lost"].append(lost)

    # ================= výstup =================
    def mean(v):
        return sum(v) / len(v) if v else float("nan")

    def fr(v):
        return "%5.1f %% (n=%d)" % (100 * mean(v), len(v)) if v else "  N/A (n=0)"

    N = st["  nosič stojí — KLASIFIKOVÁNO"]
    print("=" * 78)
    print("KROK 1 — KLASIFIKACE OBRANNÝCH KOL (snímek na začátku soupeřova kola)")
    print("=" * 78)
    for k in sorted(st):
        print("  %-58s %6d" % (k, st[k]))
    print("\n  podíly kategorií (jmenovatel = %d klasifikovaných kol):" % N)
    for k in sorted(st):
        if k.startswith("kategorie:"):
            print("    %-56s %6d  %5.1f %%" % (k[11:], st[k], 100.0 * st[k] / N))
    print("\n  histogram: našich těl v base kontaktu (0..5+):",
          [hist_based[i] for i in range(6)])
    print("  histogram: screenerů když kontakt=0 (0..4+): ",
          [hist_screen[i] for i in range(5)])

    print("\n" + "=" * 78)
    print("KROK 2 — METRIKA 17.6: kolik má nosič volných polí a kolik z nich je LEVNÝCH")
    print("=" * 78)
    print("  ⌀ volných sousedních polí nosiče: %.2f (n=%d)" %
          (mean(FREE["free"]), len(FREE["free"])))
    print("  ⌀ z nich LEVNÝCH (p_fail<0,20):   %.2f" % mean(FREE["cheap"]))
    z0 = sum(1 for v in FREE["cheap"] if v == 0)
    print("  kol, kde nosič NEMÁ ŽÁDNÉ levné pole: %d = %.1f %% z %d"
          % (z0, 100.0 * z0 / N, N))

    print("\n  prediktivita: LEVNÁ POLE (0/1/2/3+) → výsledek soupeřova kola")
    for k in sorted(M):
        v = M[k]
        print("    levných=%s   ⌀ postup míče %+.2f (n=%d) · ztráta míče %s · TD drivu %s"
              % (k, mean(v["dx"]), len(v["dx"]), fr(v["lost"]), fr(v["td_drive"])))
    print("\n  starý metr rozhodčího (dvě hodnoty):")
    for k in sorted(OLD):
        v = OLD[k]
        print("    %-34s ⌀ postup %+.2f (n=%d) · ztráta %s · TD drivu %s"
              % (k, mean(v["dx"]), len(v["dx"]), fr(v["lost"]), fr(v["td_drive"])))

    print("\n" + "=" * 78)
    print("KROK 3 — VÝSLEDKY PODLE KATEGORIE (⚠️ korelace, kategorie vznikají náhodou)")
    print("=" * 78)
    for k in sorted(OUT, key=lambda k: -OUT[k]["n"]):
        v = OUT[k]
        print("  %-42s n=%d" % (k, v["n"]))
        print("      ⌀ postup míče %+.2f (n=%d) · ztráta %s · TD v kole %s · TD drivu %s · ⌀ pádů nosiče na dodge %.3f"
              % (mean(v["dx"]), len(v["dx"]), fr(v["lost"]), fr(v["td_turn"]),
                 fr(v["td_drive"]), mean(v["dodge_fail"])))

    print("\n  stratifikace INICIATIVOU (stojící my − oni) × kontakt≥2:")
    for k in sorted(STRAT):
        v = STRAT[k]
        print("    %-28s ⌀ postup %+.2f (n=%d) · ztráta %s"
              % (" ".join(k), mean(v["dx"]), len(v["dx"]), fr(v["lost"])))

    print("\n  protitvrzení webu (jen kola s ≥1 kontaktním tělem):")
    for k in sorted(GAP):
        v = GAP[k]
        thru = "%5.1f %% (n=%d)" % (100.0 * v["thru"] / len(v["dx"]), len(v["dx"])) if v["dx"] else "N/A"
        print("    %-34s n=%d · ⌀ postup %+.2f (n=%d) · průnik ≥3 pole %s · ztráta %s"
              % (k, v["n"], mean(v["dx"]), len(v["dx"]), thru, fr(v["lost"])))

    print("\n  po rasách soupeře (kontakt≥2 vs ≤1):")
    for race in sorted(RACE):
        for kon in sorted(RACE[race]):
            v = RACE[race][kon]
            print("    %-9s %-11s ⌀ postup %+.2f (n=%d) · ztráta %s · TD drivu %s"
                  % (race, kon, mean(v["dx"]), len(v["dx"]), fr(v["lost"]),
                     fr(v["td_drive"])))

    print("\n" + "=" * 78)
    print("KROK 4 — DÍRA PEVNÝCH Y {3,5,7,9,11}: nosič na křídle")
    print("=" * 78)
    for k in sorted(WING):
        v = WING[k]
        print("  %-34s n=%d (%.1f %% z %d) · v jeho pruhu před ním náš: %.1f %% · ⌀ postup %+.2f (n=%d) · ztráta %s"
              % (k, v["n"], 100.0 * v["n"] / N, N, 100.0 * v["lane_cov"] / v["n"],
                 mean(v["dx"]), len(v["dx"]), fr(v["lost"])))


if __name__ == "__main__":
    main()
