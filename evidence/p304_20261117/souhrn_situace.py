# P304 bod 5: párový souhrn situace.py (rameno A − rameno B po situacích) + pozitivní kontrola: v kolika bězích se
# rozhodnutí lišilo (čítače P250/P302 `cx` nebo jiný ukazatel).
import sys, pickle, math
sit = pickle.load(open(sys.argv[1], "rb")); A = sys.argv[2] if len(sys.argv) > 2 else "vse"
B = sys.argv[3] if len(sys.argv) > 3 else "bez_304b"
klice = ["souper_mic", "volny_po_nas", "mic_u_nas_po_nas", "mic_u_nas_po_soup", "mic_u_soup_po_nas", "turnover", "td_soupere"]
print(f"situací {len(sit)} · {A} − {B}")
for k in klice:
    a = [sum(o[k] for o in x["ramena"][A]) / len(x["ramena"][A]) for x in sit]
    b = [sum(o[k] for o in x["ramena"][B]) / len(x["ramena"][B]) for x in sit]
    d = [p - q for p, q in zip(a, b)]; n = len(d); m = sum(d) / n
    se = math.sqrt(sum((v - m) ** 2 for v in d) / (n - 1) / n) if n > 1 else 0.0
    print(f"{k:20s} {sum(a)/n:.3f}  {sum(b)/n:.3f}  {m:+.4f} ± {se:.4f}" + (f" ({m/se:+.1f}σ)" if se else ""))
jine = sum(1 for x in sit for o, p in zip(x["ramena"][A], x["ramena"][B]) if o != p)
beh = sum(len(x["ramena"][A]) for x in sit)
print(f"běhů s jiným výsledkem (pozitivní kontrola): {jine} z {beh}")
for t in sorted({x["sou"] for x in sit}):
    s2 = [x for x in sit if x["sou"] == t]
    f = lambda r: sum(o["souper_mic"] for x in s2 for o in x["ramena"][r]) / sum(len(x["ramena"][r]) for x in s2)
    print(f"  soupeř {t:8s} situací {len(s2):3d}  souper_mic {f(A):.3f} / {f(B):.3f}")
