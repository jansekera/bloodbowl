# Rozbor data.pkl z proc.py: (A) hody Wild Animal podle makra a zdroje; (B) nabídka blitzu bez cíle
# a jak ji hledání ocenilo; (C) tahy skavenů bez blitzu — proč; (D) gutter runneři po tahu v TZ / bití.
import pickle, sys, collections as C
D = pickle.load(open(sys.argv[1] if len(sys.argv) > 1 else "data.pkl", "rb"))
BL = ("BLITZ", "BLITZ_AND_SCORE")
wa = C.Counter(); wa_f = C.Counter(); nab = C.Counter(); tahy = C.Counter(); grs = C.Counter()
vybor = []
for h in D:
    T = h["tahy"]
    for i, t in enumerate(T):
        if not t["nasi"]:
            continue
        tahy["tahu"] += 1
        blitz = any(a[0] == "BLITZ" for a in t["akce"])
        tahy["s_blitzem" if blitz else "bez_blitzu"] += 1
        ogre = [pid for pid, x in t["hraci"].items() if x["ogre"] and x["stoji"]]
        # (A) hody WA
        for w in t["wa"]:
            m = w["makro"] or {}
            k = (m.get("type"), m.get("source"), m.get("declare_blitz"))
            wa[k] += 1; wa_f[k] += not w["success"]
        # (B) rozhodnutí: byla v nabídce varianta s ohlášeným blitzem?
        nabizeno_blitz = False
        for r in t["rozhodnuti"]:
            ch = r["children"]
            db = [c for c in ch if c["declare_blitz"]]
            if db:
                nabizeno_blitz = True
                nab["rozhodnuti_s_variantou"] += 1
                bez = [c for c in ch if c["type"] == "REPOSITION" and not c["declare_blitz"]
                       and any(c["player"] == d["player"] for d in db)]
                for d in db:
                    tw = [c for c in bez if c["player"] == d["player"]]
                    vybor.append((d["visits"], d["q"], d["prior"], tw[0]["visits"] if tw else None,
                                  tw[0]["q"] if tw else None, tw[0]["prior"] if tw else None,
                                  r["declare_blitz"] and r["player"] == d["player"],
                                  sum(c["visits"] for c in ch), len(ch)))
            if r["player"] in ogre:
                nab[("ogre_rozhodnuti", r["type"], r["source"], r["declare_blitz"])] += 1
            if any(c["type"] in BL for c in ch):
                nab["rozhodnuti_s_blitz_makrem"] += 1
        # (C) bez blitzu: byl vůbec nabídnut blitz s cílem? jak daleko byli naši od soupeřů?
        if not blitz:
            mel = any(c["type"] in BL for r in t["rozhodnuti"] for c in r["children"])
            vsechna_cage = all(r["source"] == "cage" for r in t["rozhodnuti"])
            stoji = [x for x in t["hraci"].values() if x["stoji"]]
            dmin = min([x["d_soupere"] for x in stoji] or [99])
            volni = [x for x in stoji if not x["v_tz"]]
            tahy[("bez_blitzu", "blitz_v_nabidce" if mel else "blitz_nenabidnut")] += 1
            if vsechna_cage: tahy[("bez_blitzu", "jen_klec")] += 1
            tahy[("bez_blitzu", "nejblizsi_dist", min(dmin, 6))] += 1
            tahy[("bez_blitzu", "turnover_pred_koncem")] += any(a[3] for a in t["akce"])
        # (D) gutter runneři po tahu
        dalsi = T[i + 1] if i + 1 < len(T) and not T[i + 1]["nasi"] else None
        for pid, g in t["gr_po"].items():
            if not g["stoji"]: continue
            grs["gr_stoji_po_tahu"] += 1; grs["gr_v_tz"] += g["v_tz"]
            if dalsi:
                trefen = any(a[0] in ("BLOCK", "BLITZ") and a[2] == pid for a in dalsi["akce"])
                grs["gr_bit" if trefen else "gr_nebit"] += 1
                grs[("v_tz", g["v_tz"], "bit", trefen)] += 1
print("== (A) hody Wild Animal: (makro, zdroj, ohlášen blitz) -> propadlo/hodů")
for k, v in sorted(wa.items(), key=lambda kv: -kv[1]): print("  ", k, f"{wa_f[k]}/{v}")
print("== (B) nabídka")
for k, v in sorted(nab.items(), key=lambda kv: str(kv[0])): print("  ", k, v)
if vybor:
    n = len(vybor)
    print(f"  variant s blitzem: {n}; zvoleno {sum(v[6] for v in vybor)}")
    print("  průměr visits blitz/bez: %.1f / %.1f; q blitz/bez: %.3f / %.3f; prior blitz/bez: %.3f / %.3f; dětí %.1f; visits celkem %.1f" % (
        sum(v[0] for v in vybor) / n, sum(v[3] or 0 for v in vybor) / n,
        sum(v[1] for v in vybor) / n, sum(v[4] or 0 for v in vybor) / n,
        sum(v[2] for v in vybor) / n, sum(v[5] or 0 for v in vybor) / n,
        sum(v[8] for v in vybor) / n, sum(v[7] for v in vybor) / n))
print("== (C) tahy skavenů")
for k, v in sorted(tahy.items(), key=lambda kv: str(kv[0])): print("  ", k, v)
print("== (D) gutter runneři (stojí po našem tahu)")
for k, v in sorted(grs.items(), key=lambda kv: str(kv[0])): print("  ", k, v)
