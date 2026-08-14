#!/usr/bin/env python3
"""Vyhodnocení A/B „Dauntless v nabídce bloku" (mode 4), 14.08.2026.

⚠️ PROČ SAMOSTATNÝ SKRIPT: `analyze_gate_20260813.py` čte
`diag_f1_cage_advance_rows.jsonl` a hlásí `cand_plans` jako „adoptované plány
brány". Mode 4 zapisuje do `diag_dauntless_rows.jsonl` a s bránou nemá nic
společného — spustit na něj gate skript by buď nenašlo soubory, nebo (hůř)
vypsalo verdikt proti špatnému prahu s hláškou o bráně. Přesně ta past, na
kterou jsme 14.08. narazili dvakrát.

Párová delta pro domácí rasu matchupu (týž vzorec jako harness):

    delta = chess(kandidát jako domácí rasa) + chess(kandidát jako hostující) - 1

odečte sílu strany a nechá jen rozdíl mezi rameny.

⭐ `orc-sk` JE SKUTEČNÝ A/A NULL TEST. Dauntless má v TV1200 rosterech **jen
trpaslík** (Troll Slayeři) — ork ani skaven ho nemají. V tom matchupu jsou tedy
obě ramena fakticky totožná a delta MUSÍ být nula. Když není, je rozbitý
harness, ne doktrína. Tohle je silnější kontrola než u brány, kde ork bránu
taky dostal.

Práh z předregistrace `evidence/weekend_prereg_20260814.md`:
  PROŠLO   obě trpasličí ramena >= 0 a aspoň jedno >= +0,02
  ZAMÍTNUTO kterékoli trpasličí rameno <= -0,02
  jinak    NEROZHODNUTO (zapisuje se jako neúspěch, ne jako naděje)
"""
import glob, json, math, os, sys
from collections import defaultdict

OUT = sys.argv[1] if len(sys.argv) > 1 else "dauntless_ab_20260814"
MATCHUPS = {0: "dw-sk", 1: "dw-we", 3: "orc-sk", 4: "dw-orc", 5: "dw-hu"}
# Trpasličí matchupy, kde Dauntless VŮBEC může vyskočit (potřebuje defST > 3):
#   dw-orc  4x Black Orc ST4  -> 83 %   (ta otázka)
#   dw-hu   Ogre ST5          -> 67 %
#   dw-we   Treeman ST6       -> 50 %
DWARF = {4}      # jediný, kde je efekt nad rozlišením
# dw-sk je DRUHÁ null kontrola: skaven má max ST3, takže Dauntless tam nevyskočí
# ani jednou a delta musí být nula stejně jako u orc-sk.
NULL_CONTROL = {0, 3}
PREREG = 0.02


def score(cand, base):
    return 1.0 if cand > base else 0.5 if cand == base else 0.0


def main():
    rows = defaultdict(dict)
    plans = defaultdict(lambda: [0, 0])
    daunt = defaultdict(int)
    rolls = defaultdict(int)
    modes = set()
    files = sorted(glob.glob(os.path.join(OUT, "*_s*", "diag_dauntless_rows.jsonl")))
    if not files:
        sys.exit(f"žádná data v {OUT}/*_s*/diag_dauntless_rows.jsonl — doběhl běh?")
    for f in files:
        for line in open(f):
            r = json.loads(line)
            rows[r["matchup"]][(r["seed_idx"], r["cand_home"])] = r
            daunt[r["matchup"]] += r.get("cand_daunt", 0)
            rolls[r["matchup"]] += r.get("cand_roll", 0)
            plans[r["matchup"]][0] += r.get("cand_plans", 0)
            plans[r["matchup"]][1] += r.get("base_plans", 0)
            modes.add(r.get("mode"))

    if modes != {4}:
        sys.exit(f"⛔ řádky nejsou z mode 4, ale z {sorted(modes)} — špatná data")

    print(f"soubory: {len(files)} shardů | mode 4 (Dauntless v nabídce bloku)\n")
    print(f"{'matchup':10}{'párů':>7}{'delta':>10}{'SE':>9}{'σ':>7}"
          f"{'chess cand':>12}{'KO+CAS cand':>13}{'base':>7}   verdikt")

    verdicts = {}
    for mi in sorted(rows):
        d = rows[mi]
        seeds = sorted({s for s, _ in d})
        deltas, chess_c = [], []
        cas_c = cas_b = 0
        skipped = 0
        for s in seeds:
            a, b = d.get((s, True)), d.get((s, False))
            if a is None or b is None:
                skipped += 1          # rozpůlený pár na hranici shardu
                continue
            ch_home = score(a["cand"], a["base"])
            ch_away = score(b["cand"], b["base"])
            deltas.append(ch_home + ch_away - 1.0)
            chess_c += [ch_home, ch_away]
            # attrition ZPŮSOBENÁ ramenem = ztráty PROTIVNÍKA daného ramene.
            # cand_home=True -> kandidát je doma, jeho oběti jsou away_attr.
            for row, cand_is_home in ((a, True), (b, False)):
                vic = row["away_attr"] if cand_is_home else row["home_attr"]
                opp = row["home_attr"] if cand_is_home else row["away_attr"]
                cas_c += vic[0] + vic[1] + vic[2]   # KO + zranění + mrtví
                cas_b += opp[0] + opp[1] + opp[2]
        n = len(deltas)
        if n == 0:
            print(f"{MATCHUPS.get(mi, mi):10}{0:>7}   žádný úplný pár")
            continue
        mu = sum(deltas) / n
        var = sum((x - mu) ** 2 for x in deltas) / (n - 1) if n > 1 else 0.0
        se = math.sqrt(var / n)
        sig = mu / se if se else 0.0
        verdicts[mi] = (mu, sig, n)

        if mi in DWARF:
            v = ("✅ PROŠLO" if mu >= PREREG and sig >= 2 else
                 "❌ ZAMÍTNUTO" if mu <= -PREREG and sig <= -2 else
                 "— NEROZHODNUTO")
        else:
            why = "skaven má max ST3" if mi == 0 else "ani jedna strana Dauntless nemá"
            v = ("⛔ NULL TEST SE HNUL — podezřelý harness" if abs(sig) >= 2 else
                 f"✅ null OK ({why})")
        print(f"{MATCHUPS.get(mi, mi):10}{n:>7}{mu:>+10.4f}{se:>9.4f}{sig:>6.1f}σ"
              f"{sum(chess_c) / len(chess_c):>12.4f}"
              f"{cas_c / (2.0 * n):>13.2f}{cas_b / (2.0 * n):>7.2f}   {v}")
        if skipped:
            print(f"{'':10}⚠️  {skipped} neúplných párů vynecháno")
        # POJISTKA MECHANISMU: rameno, které nic nezměnilo, se musí poznat od
        # změny, která nemá efekt. Gate analyzer tohle měl, my jsme na to málem
        # zapomněli.
        dz, rz = daunt[mi], rolls[mi]
        print(f"{'':10}Dauntless nabídek/hru: {dz / (2.0 * n):.2f}"
              f"   hodů/hru: {rz / (2.0 * n):.2f}"
              + (f"   ⚠️ NABÍDLI, ale SEARCH SI NEVZAL — prior BLOCK je plochý (P10a)"
                 if dz and not rz else "")
              + ("   ⛔ NULA — RAMENO NIC NEZMĚNILO, měření neměří Dauntless"
                 if dz == 0 and mi not in NULL_CONTROL else
                 "   ✅ (null: očekáváno 0)" if dz == 0 else ""))
        if dz and mi in NULL_CONTROL:
            print(f"{'':10}⛔ NULL matchup nabídek {dz} — měl mít 0, podezřelý harness")
        cp, bp = plans[mi]
        if cp or bp:
            print(f"{'':10}⛔ adoptované plány klece {cp}/{bp} — mode 4 má mít "
                  f"bránu VYPNUTOU na obou ramenech")

    # ---- celkový verdikt proti předregistraci, ne proti dojmu ----
    dw = [verdicts[m] for m in DWARF if m in verdicts]
    print()
    if len(dw) < 1:
        print("⚠️  chybí dw-orc — verdikt se nevyslovuje")
    else:
        mus = [m for m, _, _ in dw]
        sigs = [s for _, s, _ in dw]
        if any(m >= PREREG and s >= 2 for m, s in zip(mus, sigs)):
            print("VERDIKT: ✅ PROŠLO podle předregistrace")
        elif any(m <= -PREREG and s <= -2 for m, s in zip(mus, sigs)):
            print("VERDIKT: ❌ ZAMÍTNUTO podle předregistrace")
        else:
            print("VERDIKT: — NEROZHODNUTO. Zapisuje se jako NEÚSPĚCH, ne jako naděje.")
            print("         Doplnit: kolik párů by bylo potřeba na změřený efekt,")
            print("         a jestli se to vyplatí (SD páru ~0,55 ⇒ 1 pp ≈ 12k párů).")

    print("\n⚠️  Chess pod ~2 pp neuvidí ani 3000 párů. Předregistrace proto žádá")
    print("    POVINNĚ i rozklad drivů: dauntless_corpus_20260814/drives.txt")
    print("    proti night_big_20260813/drives.txt, celkem i PER MATCHUP.")
    print("    Převod: Δchess ≈ 0,42 × Δ(drivy/hru).")


if __name__ == "__main__":
    main()
