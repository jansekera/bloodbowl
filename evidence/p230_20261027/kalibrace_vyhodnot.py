# Vyhodnocení kalibrace: pásma předpovědi × skutečná četnost ze K odpovědí na stav (Wilsonův 95% interval).
import sys, pickle, math
z = pickle.load(open(sys.argv[1], "rb"))
def wilson(k, n):
    if n == 0: return (0, 0)
    p = k / n; zz = 1.96; d = 1 + zz * zz / n
    c = (p + zz * zz / (2 * n)) / d; h = zz * math.sqrt(p * (1 - p) / n + zz * zz / (4 * n * n)) / d
    return (c - h, c + h)
print(f"stavů {len(z)}, odpovědí {sum(len(x['odpovedi']) for x in z)}\n")
print("| předpověď | stavů | odpovědí | průměr předpovědi | míč ztracen (95% int.) | nosič sražen |\n|---|---:|---:|---:|---:|---:|")
P = [(0, .05), (.05, .10), (.10, .15), (.15, .20), (.20, .25), (.25, .29), (.29, .35), (.35, .50), (.50, 1.01)]
for lo, hi in P:
    xs = [x for x in z if lo <= x["hrozba"] < hi]
    if not xs: continue
    o = [r for x in xs for r in x["odpovedi"]]; n = len(o)
    k = sum(r["mic_ztracen"] for r in o); a, b = wilson(k, n)
    print(f"| {lo:.2f}–{hi:.2f} | {len(xs)} | {n} | {sum(x['hrozba'] for x in xs)/len(xs):.2f} | "
          f"{100*k/n:.0f} % ({100*a:.0f}–{100*b:.0f}) | {100*sum(r['nosic_srazen'] for r in o)/n:.0f} % |")
print("\n## hranice")
for lo, hi in [(0, .15), (.15, .20), (.20, .29), (.29, 1.01)]:
    o = [r for x in z if lo <= x["hrozba"] < hi for r in x["odpovedi"]]
    if not o: continue
    k = sum(r["mic_ztracen"] for r in o); a, b = wilson(k, len(o))
    print(f"- {lo:.2f}–{hi:.2f}: míč ztracen {100*k/len(o):.1f} % ({100*a:.0f}–{100*b:.0f} %), odpovědí {len(o)}")
