# P180 + P187: párové vyhodnocení tří ramen (spouštět z evidence/p180_p187_20261016).
import pickle, math
V = {(r["nasi"], r["sou"], r["g"]): r for r in pickle.load(open("vse.pkl", "rb"))}
A = {(r["nasi"], r["sou"], r["g"]): r for r in pickle.load(open("bez180.pkl", "rb"))}
B = {(r["nasi"], r["sou"], r["g"]): r for r in pickle.load(open("bez187.pkl", "rb"))}
ks = sorted(V)
def sec(D, kl): return sum(D[k][kl] for k in ks)
def mak(D, t): return sum(D[k]["makra"].get(t, 0) for k in ks)
def par(D, a):
    d = [V[k][a] - D[k][a] for k in ks]; m = sum(d) / len(d)
    sd = math.sqrt(sum((x - m) ** 2 for x in d) / (len(d) - 1)); return f"{m:+.3f}±{sd/math.sqrt(len(d)):.3f}"
print("párů", len(ks))
for nm, D in (("bez P180", A), ("bez P187", B)):
    print("==", nm, "→ vše zapnuté")
    for a in ["tahu", "td", "td_predavkou", "td_soupere", "nosic_srazen", "mic_ztracen", "turnover"]:
        print(f"  {a:13s} {sec(D,a):5d} → {sec(V,a):5d}   párově (zapnuto − vypnuto) {par(D,a)}")
    print("  HAND_OFF_SCORE makra", mak(D, "HAND_OFF_SCORE"), "→", mak(V, "HAND_OFF_SCORE"))
    tt = lambda D: [D[k]["td_tah"] for k in ks if D[k]["td"]]
    print("  průměrný tah TD %.2f → %.2f" % (sum(tt(D))/max(len(tt(D)),1), sum(tt(V))/max(len(tt(V)),1)))
for rasa in ["skaven", "wood-elf", "dwarf", "orc", "human"]:
    k2 = [k for k in ks if k[0] == rasa]
    print(rasa, "TD vše/bez180/bez187:", sum(V[k]["td"] for k in k2), sum(A[k]["td"] for k in k2), sum(B[k]["td"] for k in k2),
          "· předávkou:", sum(V[k]["td_predavkou"] for k in k2), sum(A[k]["td_predavkou"] for k in k2),
          "· nosič sražen:", sum(V[k]["nosic_srazen"] for k in k2), sum(B[k]["nosic_srazen"] for k in k2))
