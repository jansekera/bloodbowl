# Blitz do kostek, kde volí obránce: pozná se jen tehdy, když zvolená stěna je pro útočníka HORŠÍ než jiná
# hozená (útočník by ji nevybral). Shodné kostky (např. 0,0) nerozliší ⇒ jde o dolní odhad.
import sys, pickle, collections
Z = pickle.load(open(sys.argv[1], "rb"))
def cena(f, z):   # jak dobrá je stěna pro útočníka
    a, d = z.get("blitzer_skilly", []), z.get("cil_skilly", [])
    if f == 4: return 5
    if f == 3: return 4 if not ("Dodge" in d and "Tackle" not in a) else 3
    if f == 2: return 3
    if f == 1: return 4.5 if ("Block" in a and "Block" not in d and "Wrestle" not in d) else (2.5 if "Block" in a else 1)
    return 0
def steny(e): return [((e["die1"] >> (3 * i)) & 7) - 1 for i in range(e["die2"])]
vys = collections.Counter(); kde = []
for z in Z:
    e = next((e for e in z["udalosti"] if e["type"] == "BLOCK"), None)
    if not e: continue
    s = steny(e); ch = e["roll"]
    proti = any(cena(f, z) > cena(ch, z) + 1e-9 for f in s)
    vys[(e["die2"], "volí obránce" if proti else ("1 kostka" if e["die2"] == 1 else "volí útočník / nerozlišeno"))] += 1
    if proti: kde.append((e["die2"], z["nasi"], z["sou"], z.get("cil_nosic", "?"), z.get("blitzer_st"), z.get("cil_st"), s, ch,
                          int(z.get("cil_srazen", 0)), z["turnover"]))
print("blitzů s blokem:", sum(vys.values()))
for k, v in sorted(vys.items()): print(f"  {v:4d}  {k[0]} kostky · {k[1]}")
print("\nprokazatelně volil obránce (kostek, my, soupeř, cíl nosič, ST nás, ST cíl, stěny, zvoleno, sražen, turnover):")
for r in sorted(kde, key=lambda r: -r[0]): print("  ", r)
