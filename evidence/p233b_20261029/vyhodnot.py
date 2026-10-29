# Tabulka k mereni.py (P233 v běžných hrách): dosah (stopa řadiče) a párové rozdíly vse × bez_p233.
import sys, pickle, math
z = pickle.load(open(sys.argv[1], "rb"))
mic = [x for x in z if x["ramena"]["vse"]["mel_mic"]]
print(f"tahů celkem {len(z)}, s míčem {len(mic)}")
v = lambda x, r, k: x["ramena"][r][k]
zed = [x for x in z if v(x, "vse", "p229") or v(x, "vse", "p233")]
print(f"řadič u zdi (P229 nebo P233) v rameni vse: {len(zed)} tahů")
print(f"P233 došel na řadu: {sum(1 for x in z if v(x,'vse','p233'))} tahů; rána P233: {sum(1 for x in z if v(x,'vse','p233_rana'))} tahů "
      f"({sum(v(x,'vse','p233_rana') for x in z)} ran)")
lisi = [x for x in z if v(x, "vse", "akce") != v(x, "bez_p233", "akce")]
print(f"ramena se liší (jiné akce): {len(lisi)} tahů"
      f"; z toho s P233 v rameni vse: {sum(1 for x in lisi if v(x,'vse','p233'))}")
for x in lisi[:20]:
    print("  ", x["nasi"], x["sou"], x["g"], "tah", x["tah"], "P233 rána", v(x, "vse", "p233_rana"), "P229", v(x, "vse", "p229"))
print()
print("na 100 tahů s míčem           vse   bez_p233   rozdíl ± SE")
for k in ("posun", "nosic_srazen", "mic_ztracen", "turnover", "td", "rohy_srazeny", "cas"):
    d = [(v(x, "vse", k) or 0) - (v(x, "bez_p233", k) or 0) for x in mic]
    a = sum(v(x, "vse", k) or 0 for x in mic) / max(1, len(mic)); b = sum(v(x, "bez_p233", k) or 0 for x in mic) / max(1, len(mic))
    n = len(d); m = sum(d) / max(1, n); se = math.sqrt(sum((q - m) ** 2 for q in d) / max(1, n - 1) / max(1, n))
    f = 1 if k in ("posun", "cas") else 100
    print(f"{k:28s} {a*f:6.2f} {b*f:8.2f}   {m*f:+.2f} ± {se*f:.2f}")
