# Rozbor měření P250: python3.8 rozbor.py p250_situace.pkl
# Ramena: vse · bez_a (zvedač podle celé akce) · bez_b (obestoupit) · bez_c (odtlačit na míč) · bez (nic z P250).
# Hlavní otázka (A18): míč u nás po našem tahu a po tahu soupeře. Párově po situacích (stejné semínko).
import sys, pickle, math, collections
S = pickle.load(open(sys.argv[1], "rb"))
print("situací", len(S), dict(collections.Counter(s["nasi"] for s in S)))
M = ["mic_u_nas_po_nas", "mic_u_nas_po_soup", "mic_nas_tah", "td_soupere", "turnover", "blitz_nosic", "blok_nosic"]
B = ["bez_a", "bez_b", "bez_c", "bez"]
avg = lambda s, r, m: sum(o[m] for o in s["ramena"][r]) / len(s["ramena"][r])


def rozdil(ss, a, b, m):
    d = [avg(s, a, m) - avg(s, b, m) for s in ss]; n = len(d)
    if n < 2: return 0.0, 0.0
    mu = sum(d) / n; se = math.sqrt(sum((x - mu) ** 2 for x in d) / (n - 1) / n)
    return mu, se


for rasa in [None, "skaven", "woodelf", "human"]:
    ss = [s for s in S if rasa is None or s["nasi"] == rasa]
    if not ss: continue
    print(f"\n== {rasa or 'vše'} ({len(ss)} situací)")
    print(f"{'':20}{'vse':>7}" + "".join(f"{'vse-' + b:>22}" for b in B))
    for m in M:
        row = f"{m:20}{sum(avg(s, 'vse', m) for s in ss) / len(ss):7.3f}"
        for b in B:
            mu, se = rozdil(ss, "vse", b, m); row += f"{mu:+9.3f}±{se:.3f}({mu / se if se else 0:+.1f})"
        print(row)

# Pozitivní kontrola: čítače P250 [0] zvedač změněn (a), [1] obestoupení nabídnuto (b), [2] odtlačen na míč (c).
# Očekávání: v rameni vypnutého prvku je jeho čítač 0, v ostatních > 0.
print("\nčítače P250 (a zvedač, b obestoupit, c odtlačit):")
for r in ["vse"] + B:
    os_ = [o for s in S for o in s["ramena"][r]]
    print(f"  {r:6}", [sum(o["cx"][i] for o in os_) for i in range(3)], "tahů", len(os_))

# Kde mechanismus vůbec mohl zasáhnout: situace, kde v rameni vse míč během našeho tahu soupeř ztratil.
# (Podmínka je po zásahu ⇒ jen orientačně; hlavní je párový rozdíl přes všechny situace výše.)
ss = [s for s in S if avg(s, "vse", "mic_nas_tah") > 0]
print(f"\nSituace, kde v rameni vse soupeř míč ztratil ({len(ss)}):")
for m in ["mic_u_nas_po_nas", "mic_u_nas_po_soup", "turnover"]:
    print(f"  {m:20}" + "".join(f"{r}={sum(avg(s, r, m) for s in ss) / len(ss):.3f}  " for r in ["vse"] + B) if ss else "")
