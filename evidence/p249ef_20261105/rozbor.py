# Rozbor měření P249e/f: python3.8 rozbor.py situace.pkl
import sys, pickle, math, collections
S = pickle.load(open(sys.argv[1], "rb"))
print("situací", len(S), dict(collections.Counter(s["nasi"] for s in S)))
M = ["blitz_nosic", "blok_nosic", "mic_nas_tah", "mic_u_nas_po_nas", "mic_u_nas_po_soup", "td_soupere", "turnover"]
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
    print(f"{'':20}{'vse':>7}" + "".join(f"{h:>22}" for h in ["vse-bez_e", "vse-bez_f", "vse-bez_ef"]))
    for m in M:
        row = f"{m:20}{sum(avg(s, 'vse', m) for s in ss) / len(ss):7.3f}"
        for b in ["bez_e", "bez_f", "bez_ef"]:
            mu, se = rozdil(ss, "vse", b, m); row += f"{mu:+9.3f}±{se:.3f}({mu / se if se else 0:+.1f})"
        print(row)

# Pozitivní kontrola: čítače [3] odklad za mimo-zálohu (e), [4] strop úhybu nepoužit (f)
for r in ["vse", "bez_e", "bez_f", "bez_ef"]:
    os_ = [o for s in S for o in s["ramena"][r]]
    print(r, "čítače P249 b..f", [sum(o["cx"][i] for o in os_) for i in range(5)])

# Pořadí: kolik maker před blitzem a po něm (rameno vse × bez_e)
for r in ["vse", "bez_e"]:
    pr = [o["akci_pred"] for s in S for o in s["ramena"][r] if o["akci_pred"] >= 0]
    po = [o["akci_po"] for s in S for o in s["ramena"][r] if o["akci_po"] >= 0]
    if pr: print(r, "blitzů na nosiče", len(pr), "maker před ø", round(sum(pr) / len(pr), 2), "po ø", round(sum(po) / len(po), 2))

# Kdy AI pokus zahraje: koše podle šance a rizika nejlepšího blitzu na nosiče (bez stropu = P249f)
def kos(x, hranice):
    for i, h in enumerate(hranice):
        if x < h: return i
    return len(hranice)
SH = [0.1, 0.3, 0.5]; RH = [1 / 6, 1 / 3, 0.5]
print("\nPokus (blitz nebo blok na nosiče) v rameni vse podle ŠANCE úspěchu:")
for i, lab in enumerate(["<0,1", "0,1–0,3", "0,3–0,5", "≥0,5"]):
    xs = [max(o["blitz_nosic"], o["blok_nosic"]) for s in S if s["sance"] for o in s["ramena"]["vse"] if kos(s["sance"][1], SH) == i]
    if xs: print(f"  {lab:8} {len(xs):4} pokusů {sum(xs) / len(xs):.2f}")
print("podle RIZIKA neúspěchu (cesta + blok):")
for i, lab in enumerate(["<1/6", "1/6–1/3", "1/3–1/2", "≥1/2"]):
    for r in ["vse", "bez_f"]:
        xs = [o["blitz_nosic"] for s in S if s["sance"] for o in s["ramena"][r] if kos(s["sance"][0], RH) == i]
        if xs: print(f"  {lab:8} {r:6} {len(xs):4} blitz na nosiče {sum(xs) / len(xs):.2f}")
bez = [(s["nasi"], s["sou"], s["g"], s["tah"], round(s["sance"][0], 2), round(s["sance"][1], 2))
       for s in S if s["sance"] and s["sance"][1] >= 0.3 and all(not o["blitz_nosic"] and not o["blok_nosic"] for o in s["ramena"]["vse"])]
print("\nSituace se šancí ≥ 0,3, kde AI v rameni vse nezkusila nic:", len(bez))
for b in bez[:15]: print("  ", b)
