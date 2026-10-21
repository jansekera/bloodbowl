# Prázdné blitzy — rozpad příčin. „nic“ = jako diag.py P220: cíl bez KNOCKED_DOWN a nepohnul se.
# Kostka bloku: BLOCK.roll = zvolená stěna (enums.h BlockDiceFace): 0 útočník padá, 1 oba padají, 2 odtlačen,
# 3 obránce klopýtne, 4 obránce padá. die2 = počet kostek, die1 = všechny stěny po 3 bitech (face+1).
import sys, pickle, collections
Z = pickle.load(open(sys.argv[1], "rb"))
STENA = {0: "útočník padá", 1: "oba padají", 2: "odtlačen", 3: "klopýtne", 4: "obránce padá"}


def agilni(z):
    return z.get("blitzer_ag", 0) >= 4 or (z.get("blitzer_ag") == 3 and "Dodge" in z.get("blitzer_skilly", []))


def pricina(z):
    if z.get("bez_bloku"): return "bez bloku (Rat Ogre pohyb)"
    bl = [e for e in z["udalosti"] if e["type"] == "BLOCK"]
    if not bl: return "turnover před blokem" if z["turnover"] else "blok se nehodil (bez turnoveru)"
    f = bl[0]["roll"]; cs = z.get("cil_skilly", []); bs = z.get("blitzer_skilly", [])
    if f == 0: return "útočník padá (kostka)"
    if f == 1:
        if "Block" in cs and "Block" in bs: return "oba padají, oba mají Block"
        if "Block" in cs: return "oba padají, Block jen obránce ⇒ padá blitzující"
        if "Wrestle" in cs or "Wrestle" in bs: return "oba padají ⇒ Wrestle (oba leží, bez KNOCKED_DOWN)"
        return "oba padají — jiné"
    if f in (2, 3):
        if "StandFirm" in cs: return "odtlačen/klopýtne, cíl má Stand Firm"
        if "PRONE" in z.get("cil_stav_po", ""): return "odtlačen ⇒ leží bez KNOCKED_DOWN (jiné)"
        return "odtlačen/klopýtne, nepohnul se — jiné"
    if f == 4: return "obránce padá, ale bez KNOCKED_DOWN / nepohnul — jiné"
    return "jiné"


def tab(radky, nazev):
    vse = len(radky)
    nic = [z for z in radky if not z.get("bez_bloku") and not z.get("cil_srazen") and not z.get("cil_pohnul")]
    print(f"\n== {nazev}: blitzů {vse}, „nic“ {len(nic)} ({len(nic)/max(vse,1):.0%})")
    c = collections.Counter(pricina(z) for z in nic)
    for k, v in c.most_common(): print(f"   {v:4d}  {v/max(len(nic),1):5.0%}  {k}")
    return nic


print("blitzů celkem", len(Z))
tab(Z, "všechny naše blitzy")
nic = tab([z for z in Z if agilni(z)], "blitzující agilní (AG4+ / AG3+Dodge)")
for sou in ("dwarf", "orc", "human"):
    tab([z for z in Z if agilni(z) and z["sou"] == sou], f"agilní proti {sou}")
# kostky u „nic“ se Stand Firm: kolik bylo kostek a kdo volil
sf = [z for z in nic if pricina(z).startswith("odtlačen/klopýtne, cíl má Stand Firm")]
print("\nStand Firm „nic“: počet kostek", collections.Counter(next(e["die2"] for e in z["udalosti"] if e["type"] == "BLOCK") for z in sf))
print("Stand Firm „nic“: blitz pokračoval dalším pohybem blitzujícího",
      sum(any(p == z["blitzer"] for _, p in z["dal_kroky"]) for z in sf), "z", len(sf))
# pozitivní kontrola: příčiny i u blitzů, které cíl srazily (měřidlo umí rozlišit)
print("kontrola: sraženo", sum(z.get("cil_srazen", 0) for z in Z), "· pohnul", sum(z.get("cil_pohnul", 0) and not z.get("cil_srazen") for z in Z))
