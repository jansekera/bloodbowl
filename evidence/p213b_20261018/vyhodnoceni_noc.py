# P213: párové vyhodnocení (spouštět z evidence/p213_20261016).
import pickle, math
V = {(r["nasi"], r["sou"], r["g"]): r for r in pickle.load(open("noc_vse.pkl", "rb"))}
B = {(r["nasi"], r["sou"], r["g"]): r for r in pickle.load(open("noc_bez.pkl", "rb"))}
ks = sorted(V)
def par(a, kk=ks):
    d = [V[k][a] - B[k][a] for k in kk]; m = sum(d) / len(d)
    sd = math.sqrt(sum((x - m) ** 2 for x in d) / (len(d) - 1)); return f"{m:+.3f}±{sd/math.sqrt(len(d)):.3f}"
print("párů", len(ks))
for a in ["tahu", "td", "td_posledni", "posledni_tah_s_mic", "td_predavkou", "td_soupere", "nosic_srazen", "mic_ztracen", "turnover"]:
    print(f"  {a:18s} vypnuto {sum(B[k][a] for k in ks):5d} → zapnuto {sum(V[k][a] for k in ks):5d}   párově {par(a)}")
print("  HAND_OFF_SCORE makra", sum(B[k]["makra"].get("HAND_OFF_SCORE", 0) for k in ks), "→", sum(V[k]["makra"].get("HAND_OFF_SCORE", 0) for k in ks))
for rasa in ["skaven", "wood-elf", "dwarf", "orc", "human"]:
    k2 = [k for k in ks if k[0] == rasa]
    print(f"  {rasa:9s} TD {sum(B[k]['td'] for k in k2)}→{sum(V[k]['td'] for k in k2)} · v posledním kole {sum(B[k]['td_posledni'] for k in k2)}→{sum(V[k]['td_posledni'] for k in k2)} · předávkou {sum(B[k]['td_predavkou'] for k in k2)}→{sum(V[k]['td_predavkou'] for k in k2)}")
