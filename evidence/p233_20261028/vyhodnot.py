# Cílené P233: průměry na 100 pozic, párové rozdíly vůči "vse" (± SE) a v kolika pozicích se ramena liší.
import sys, pickle, math
z = pickle.load(open(sys.argv[1], "rb"))
M = ["zed", "ran", "prolom", "posun", "radu", "hrozba", "nosic_srazen", "mic_ztracen", "turnover", "rohy_srazeny", "nasi_srazeni", "td", "cas"]
def tab(zz, nadpis):
    print(f"## {nadpis} — {len(zz)} pozic\n")
    for b in [r for r in zz[0]["ramena"] if r != "vse"] if zz else []:
        lisi = sum(1 for x in zz if any(x["ramena"]["vse"][k] != x["ramena"][b][k] for k in M if k != "cas"))
        print(f"### vse × {b} (liší se v {lisi} pozicích)\n\n| na 100 pozic (zed, ran, posun, hrozba, cas: průměr na pozici) | vse | {b} | rozdíl ± SE |\n|---|---:|---:|---:|")
        for k in M:
            pr = [(x["ramena"]["vse"][k], x["ramena"][b][k]) for x in zz if x["ramena"]["vse"][k] is not None and x["ramena"][b][k] is not None]
            if not pr: continue
            n = len(pr); d = [a - c for a, c in pr]; md = sum(d) / n
            se = math.sqrt(sum((v - md) ** 2 for v in d) / (n - 1) / n) if n > 1 else 0
            f = 1 if k in ("hrozba", "cas", "zed", "ran", "posun") else 100
            print(f"| {k} | {f*sum(a for a,_ in pr)/n:.2f} | {f*sum(c for _,c in pr)/n:.2f} | {f*md:+.2f} ± {f*se:.2f} |")
        print()
tab(z, "všechny")
for n in sorted({x["nasi"] for x in z}): tab([x for x in z if x["nasi"] == n], n)
