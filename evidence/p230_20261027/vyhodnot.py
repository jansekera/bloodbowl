import pickle, sys, math
z = pickle.load(open(sys.argv[1], 'rb'))
M = ["radu", "prolom", "posun", "td", "turnover", "nosic_srazen", "mic_ztracen", "rohu", "rohy_srazeny", "nasi_srazeni", "cas"]
import math
def tab(rows, nazev):
    rows = [r for r in rows if r["ramena"]["vse"]["mel_mic"]]
    n = len(rows)
    if not n: return
    lisi = sum(1 for r in rows if any(r["ramena"]["vse"][k] != r["ramena"]["bez230"][k] for k in M if k != "cas"))
    print(f"## {nazev} — {n} tahů s míčem, ramena se liší v {lisi}\n")
    print("| na 100 tahů | vše | bez P230 | rozdíl ± SE |\n|---|---:|---:|---:|")
    for m in M:
        val = lambda r, a: (r["ramena"][a][m] or 0)
        d = [val(r, "vse") - val(r, "bez230") for r in rows]
        mu = sum(d) / n; se = math.sqrt(sum((x - mu) ** 2 for x in d) / max(1, n - 1) / n)
        print(f"| {m} | {100*sum(val(r,'vse') for r in rows)/n:.1f} | {100*sum(val(r,'bez230') for r in rows)/n:.1f} | {100*mu:+.1f} ± {100*se:.1f} |")
    print()
tab(z, "všichni")
for t in sorted({r["nasi"] for r in z}): tab([r for r in z if r["nasi"] == t], t)

# Kalibrace hrozby: předpověď po našem tahu × co soupeř v odpovědi opravdu udělal (obě ramena dohromady)
print("## kalibrace hrozby (obě ramena)\n\n| hrozba po našem tahu | tahů | nosič sražen | míč ztracen |\n|---|---:|---:|---:|")
P = [(0, .05), (.05, .10), (.10, .15), (.15, .20), (.20, .25), (.25, .29), (.29, .35), (.35, 1.01)]  # hranice 0,15 (dnes) × 0,20 (návrh uživatele) × 0,29 (zeď, slabší tým)
for lo, hi in P:
    xs = [r["ramena"][a] for r in z for a in ("vse", "bez230") if r["ramena"][a].get("hrozba") is not None and lo <= r["ramena"][a]["hrozba"] < hi]
    if not xs: continue
    n = len(xs)
    print(f"| {lo:.2f}–{hi:.2f} | {n} | {100*sum(x['nosic_srazen'] for x in xs)/n:.0f} % | {100*sum((x['mic_ztracen'] or 0) for x in xs)/n:.0f} % |")
