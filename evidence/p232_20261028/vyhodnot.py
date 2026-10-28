# P232: průměry na 100 tahů s míčem, párové rozdíly vůči "vse" (± SE), v kolika tazích se ramena liší.
import pickle, sys, math
z = pickle.load(open(sys.argv[1], 'rb'))
M = ["posun", "hrozba", "td", "turnover", "nosic_srazen", "mic_ztracen", "rohu", "rohy_srazeny", "nasi_srazeni", "radu", "prolom", "cas"]
JEDNOTKY = {"hrozba": 1, "cas": 1, "posun": 1, "rohu": 1}
def tab(rows, nazev):
    rows = [r for r in rows if r["ramena"]["vse"]["mel_mic"]]
    n = len(rows)
    if not n: return
    for b in [a for a in rows[0]["ramena"] if a != "vse"]:
        lisi = sum(1 for r in rows if any(r["ramena"]["vse"][k] != r["ramena"][b][k] for k in M if k != "cas"))
        print(f"## {nazev} × {b} — {n} tahů s míčem, ramena se liší v {lisi}\n")
        print(f"| na 100 tahů (posun, hrozba, rohů, čas: průměr) | vše | {b} | rozdíl ± SE |\n|---|---:|---:|---:|")
        for m in M:
            pr = [(r["ramena"]["vse"].get(m), r["ramena"][b].get(m)) for r in rows]
            pr = [(a or 0, c or 0) for a, c in pr if not (a is None and c is None)]
            if not pr: continue
            k = len(pr); f = JEDNOTKY.get(m, 100)
            d = [a - c for a, c in pr]; mu = sum(d) / k
            se = math.sqrt(sum((x - mu) ** 2 for x in d) / max(1, k - 1) / k)
            print(f"| {m} | {f*sum(a for a,_ in pr)/k:.2f} | {f*sum(c for _,c in pr)/k:.2f} | {f*mu:+.2f} ± {f*se:.2f} |")
        print()
tab(z, "všichni")
for t in sorted({r["nasi"] for r in z}): tab([r for r in z if r["nasi"] == t], t)
