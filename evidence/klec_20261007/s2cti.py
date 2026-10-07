import sys, pickle, collections, re
R=pickle.load(open(sys.argv[1],"rb")); UK=int(sys.argv[2]) if len(sys.argv)>2 else 0; FILTR=sys.argv[3] if len(sys.argv)>3 else ""
def sig(r):
    d="\n".join(r["dbg"])
    if not r["drzel"]: return "zvedl v tahu"
    nh=[m for m in r["makra"] if m[2] and m[1] in ("ADVANCE","REPOSITION")]
    hl=[m[0] for m in nh if m[0]!="cage"]
    proc=re.findall(r"hledání smí pohnout nosičem: ([^\n]+)", d)
    if "VÝBĚH" in d: p="výběh (fáze 3)"
    elif "nosič ke kleci" in d: p="nosič ke kleci"
    elif "klec napřed: plán" in d: p="klec napřed (plán 4 rohy)"
    elif "klec napřed: stojí" in d: p="klec stojí, plán není"
    elif "bez plánu" in d: p="bez plánu"
    else:
        m=re.search(r"plán postupu: verdikt (\d), platný (\d), krok (\d+), rohy stojí (\d), po tahu (\d)",d)
        p="řadič neplánoval" if not m else ("plán nevyšel" if m.group(2)=="0" else f"plán krok {'0' if m.group(3)=='0' else '≥1'} / {m.group(5)} rohů")
    return p+" · "+(("hledání pohnulo nosičem ("+(proc[-1] if proc else "?")+")") if hl else "nosič s řadičem" if nh else "nosič stál")
ma=[r for r in R if r["ma"] and r["ztrata"] is not None]
print(f"tahů končících s míčem (a soupeř pak hrál): {len(ma)} · ztráta {sum(r['ztrata'] for r in ma)}")
for nazev,f in (("0–1 roh",lambda r:r["rohy"]<=1),("2–3 rohy",lambda r:2<=r["rohy"]<=3),("nosič vedle soupeře",lambda r:r["zona"]>0),("4 rohy",lambda r:r["rohy"]==4)):
    q=[r for r in ma if f(r)]; C=collections.Counter(); Z=collections.Counter()
    for r in q: C[sig(r)]+=1; Z[sig(r)]+=r["ztrata"]
    print(f"--- {nazev}: {len(q)} tahů, ztráta {sum(r['ztrata'] for r in q)}")
    for k,v in C.most_common(9): print(f"   {v:3d} (ztráta {Z[k]:2d})  {k}")
n=0
for r in ma:
    if UK and FILTR and FILTR in sig(r) and r["ztrata"] and n<UK:
        n+=1; print(f"\n# hra {r['g']} proti {r['kop']} kolo {r['kolo']} rohy {r['rohy']} zóna {r['zona']} · {sig(r)}"); print(" ".join(m[0][0]+m[1][:4]+("N" if m[2] else "") for m in r["makra"]))
        for l in r["dbg"][:8]: print("   ",l[:150])
        for a,b in zip(r.get("pred",[""]*7), r["po"]): print("   ",a,"  →  ",b)
