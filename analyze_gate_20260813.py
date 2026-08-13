#!/usr/bin/env python3
"""Vyhodnocení A/B brány klece (`run_gate_20260813.sh`), 13.08.2026.

Sesbírá shardy jednoho matchupu do jednoho vzorku a spočítá párovou deltu
proti pre-registraci ve spouštěcím skriptu.

Párová delta pro domácí rasu matchupu (týž vzorec jako harness,
`PairResult::deltaHomeRace`): kandidát hraje jednou doma a jednou venku na
TÝŽ seed, takže

    delta = chess(kandidát jako domácí rasa) + chess(kandidát jako hostující)
            - 1

odečte sílu strany a nechá jen rozdíl mezi rameny.

⚑ Práh čte ze změřeného šumového dna, ne z dojmu: null-testy z 12.08. daly
na 400 párech ±5,3 pp, tedy SE ≈ 0,026. Tenhle běh má 1500 párů, takže
SE ≈ 0,0134 — ale nespoléhá se na přepočet, SE se počítá ze vzorku.
"""
import glob, json, math, os, sys
from collections import defaultdict

OUT = sys.argv[1] if len(sys.argv) > 1 else "gate_measure_20260813"
MATCHUPS = {0: "dw-sk", 1: "dw-we", 3: "orc-sk"}
DWARF = {0, 1}          # matchupy, kde je domácí rasou trpaslík
PREREG = 0.03           # pre-registrovaná hranice na trpasličích matchupech


def score(cand, base):
    return 1.0 if cand > base else 0.5 if cand == base else 0.0


def main():
    # seed_idx je díky shard offsetu globálně jednoznačný uvnitř matchupu
    rows = defaultdict(dict)     # matchup -> (seed_idx, cand_home) -> řádek
    plans = defaultdict(lambda: [0, 0])
    files = sorted(glob.glob(os.path.join(OUT, "*_s*", "diag_f1_cage_advance_rows.jsonl")))
    if not files:
        sys.exit(f"žádná data v {OUT}/*_s*/ — doběhl běh?")
    for f in files:
        for line in open(f):
            r = json.loads(line)
            rows[r["matchup"]][(r["seed_idx"], r["cand_home"])] = r
            plans[r["matchup"]][0] += r["cand_plans"]
            plans[r["matchup"]][1] += r["base_plans"]

    print(f"soubory: {len(files)} shardů\n")
    print(f"{'matchup':10}{'párů':>7}{'delta':>10}{'SE':>9}{'σ':>7}"
          f"{'chess cand':>12}{'plánů/hru':>12}   verdikt")
    for mi in sorted(rows):
        d = rows[mi]
        seeds = sorted({s for s, _ in d})
        deltas, chess_c = [], []
        skipped = 0
        for s in seeds:
            a, b = d.get((s, True)), d.get((s, False))
            if a is None or b is None:
                skipped += 1          # rozpůlený pár na hranici shardu
                continue
            ch_home = score(a["cand"], a["base"])   # kandidát hraje domácí rasu
            ch_away = score(b["cand"], b["base"])   # kandidát hraje hostující
            deltas.append(ch_home + ch_away - 1.0)
            chess_c += [ch_home, ch_away]
        n = len(deltas)
        if n == 0:
            print(f"{MATCHUPS.get(mi, mi):10}{0:>7}   žádný úplný pár")
            continue
        mu = sum(deltas) / n
        var = sum((x - mu) ** 2 for x in deltas) / (n - 1) if n > 1 else 0.0
        se = math.sqrt(var / n)
        sig = mu / se if se else 0.0
        cp, bp = plans[mi]
        # verdikt se vyslovuje jen tam, kde ho pre-registrace slíbila
        if mi in DWARF:
            v = ("✅ PROŠLO" if mu >= PREREG and sig >= 2 else
                 "❌ NEPROŠLO" if mu < 0 and sig <= -2 else
                 "— neprůkazné")
        else:
            v = "kontrola (ork bránu má taky, čeká se MENŠÍ efekt)"
        print(f"{MATCHUPS.get(mi, mi):10}{n:>7}{mu:>+10.4f}{se:>9.4f}{sig:>6.1f}σ"
              f"{sum(chess_c) / len(chess_c):>12.4f}{cp / (2.0 * n):>12.2f}   {v}")
        if skipped:
            print(f"{'':10}⚠️  {skipped} neúplných párů vynecháno")
        if cp == 0:
            print(f"{'':10}⚠️  BRÁNA NEADOPTOVALA ANI JEDEN PLÁN — měření neměří bránu")
        if bp:
            print(f"{'':10}⚠️  base rameno adoptovalo {bp} plánů, mělo mít 0")

    print("\nPřipomínka k čtení: null-testy z 12.08. daly na 400 párech rozptyl")
    print("±5,3 pp u delty, která měla být nulová. Cokoli pod 2σ je šum.")


if __name__ == "__main__":
    main()
