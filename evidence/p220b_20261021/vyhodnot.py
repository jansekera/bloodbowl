# Vyhodnocení P220 měření: párově po tazích (vse = P220 zapnut, bez220 = maska 128), na 100 našich tahů.
# Rozdíl = vse − bez220; ± = střední chyba rozdílu (σ/√n), σ = směrodatná odchylka párových rozdílů na tah.
import sys, pickle, math, collections
z = pickle.load(open(sys.argv[1], "rb"))
KAT = ["blok", "stal", "nosic", "asist", "jen_dosel", "jine"]
VYS = ["srazen", "posun", "padl_blitzujici", "bez_ucinku", "oba_lezi", "bez_bloku"]


def metriky(r):
    m = {}
    kat = r["kat"]
    for k in KAT: m["agilní " + k] = sum(1 for v in kat.values() if v == k)
    m["  z toho jen_dosel u našeho nosiče"] = r["u_nosice"]
    m["  z toho jen_dosel bez pohybu"] = r["bez_pohybu"]
    m["agilní v kontaktu sraženi v odpovědi"] = sum(r["srazen"].values())
    m["  z toho jen_dosel sraženi"] = sum(s for pid, s in r["srazen"].items() if kat[pid] == "jen_dosel")
    m["naši v kontaktu celkem"] = r["v_kontaktu"]
    m["naši sraženi v odpovědi (všichni)"] = r["nasi_srazeni"]
    m["turnover našeho tahu"] = r["turnover"]
    m["soupeři sraženi naším tahem"] = r["srazeno_souperu"]
    bl = r["blitz"]
    m["blitzy"] = int(bl is not None)
    m["  agilní blitzy"] = int(bool(bl) and bl["agilni"] == 1)
    for v in VYS: m["  blitz → " + v] = int(bool(bl) and bl["vysledek"] == v)
    m["  blitzující v kontaktu na konci"] = int(bool(bl) and bl["kontakt"] == 1)
    m["  blitzující sražen v odpovědi"] = int(bool(bl) and bl.get("blitzer_srazen", 0) == 1)
    return m


def tabulka(nazev, zaz):
    n = len(zaz)
    if n == 0: return
    print(f"\n## {nazev} — {n} tahů\n")
    print("| na 100 tahů | P220 zap. | vyp. (128) | rozdíl ± SE | σ párově |")
    print("|---|---:|---:|---:|---:|")
    klice = list(metriky(zaz[0]["ramena"]["vse"]).keys())
    for k in klice:
        a = [metriky(x["ramena"]["vse"])[k] for x in zaz]
        b = [metriky(x["ramena"]["bez220"])[k] for x in zaz]
        d = [p - q for p, q in zip(a, b)]
        md = sum(d) / n
        sd = math.sqrt(sum((x - md) ** 2 for x in d) / (n - 1)) if n > 1 else 0.0
        print(f"| {k} | {100 * sum(a) / n:.1f} | {100 * sum(b) / n:.1f} | "
              f"{100 * md:+.1f} ± {100 * sd / math.sqrt(n):.1f} | {100 * sd:.1f} |")


print(f"tahů celkem {len(z)}")
tabulka("všichni", z)
for nasi in ["wood-elf", "skaven"]:
    tabulka(nasi, [x for x in z if x["nasi"] == nasi])
for nasi in ["wood-elf", "skaven"]:
    for sou in ["dwarf", "orc", "human"]:
        zz = [x for x in z if x["nasi"] == nasi and x["sou"] == sou]
        n = len(zz)
        if not n: continue
        a = sum(sum(1 for v in x["ramena"]["vse"]["kat"].values() if v == "jen_dosel") for x in zz)
        b = sum(sum(1 for v in x["ramena"]["bez220"]["kat"].values() if v == "jen_dosel") for x in zz)
        print(f"- {nasi} × {sou}: {n} tahů, jen_dosel na 100 tahů {100 * a / n:.1f} (zap.) × {100 * b / n:.1f} (vyp.)")
