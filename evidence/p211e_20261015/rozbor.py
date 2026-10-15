# Rozbor data.pkl z proc.py: agilní po našem tahu vedle soupeře, podle příčiny.
import pickle, sys, collections as C
Z = pickle.load(open(sys.argv[1] if len(sys.argv) > 1 else "data.pkl", "rb"))
def pricina(z):
    if z["nosic"]: return "nese míč (neutíká)"
    m = z["makra"]
    if z["zacal_tz"]:
        if not m: return "začal v kontaktu, nejednal, tah skončil TURNOVEREM" if z["turnover"] else "začal v kontaktu, nejednal, tah skončil normálně"
        if any(t == "escape" for _, t in m): return "začal v kontaktu, útěk nedošel"
        if m[0][0] in ("BLOCK", "BLITZ"): return "začal v kontaktu, blokoval/blitzoval"
        return "začal v kontaktu, jednal: " + "+".join(f"{a}/{b}" for a, b in m)
    if not m: return "začal volně, nejednal (soupeř k němu odsunut)"
    if m[0][0] in ("BLITZ", "BLITZ_AND_SCORE"): return "začal volně, blitzoval"
    return "začal volně, přišel: " + "+".join(f"{a}/{b}" for a, b in m)
for nasi in ("skaven", "wood-elf", None):
    zz = [z for z in Z if nasi is None or z["nasi"] == nasi]
    c = C.Counter(pricina(z) for z in zz)
    print(f"== {nasi or 'vše'}: {len(zz)} hráčů-tahů v kontaktu")
    for k, v in c.most_common(25): print(f"  {v:5d}  {100*v/len(zz):5.1f} %  {k}")
