import sys, pickle, collections
R=pickle.load(open(sys.argv[1],"rb"))
def rozbor(r):
    t=r["tahy"]; drzel=False; ztraty=[]
    for i,x in enumerate(t):
        if x["strana"]=="my":
            if x["pred"][0]=="náš" and x["po"][0] not in ("náš","TD"):
                ztraty.append(("vlastní tah", x["kolo"], x["to"][0][1] if x["to"] else "bez turnoveru", x["pred"][2], x["pred"][3], x["pred"][1]))
            if x["po"][0]=="náš": drzel=True
        else:
            if x["pred"][0]=="náš" and x["po"][0]!="náš":
                ztraty.append(("tah soupeře", x["kolo"], "", x["pred"][2], x["pred"][3], x["pred"][1]))
    return drzel, ztraty
V=collections.Counter(r["vysl"] for r in R); n=len(R)
print(f"poločasů {n}: TD {V['TD']}, TD soupeře {V['TD soupeře']}, bez TD {V['bez TD']}")
D=collections.Counter(); ROH=collections.Counter(); ZON=collections.Counter(); KOLO=collections.Counter(); VL=collections.Counter(); KDE=[]
for r in R:
    if r["vysl"]=="TD": continue
    drzel, z = rozbor(r)
    if not drzel and not z: D["míč nikdy neudržel do konce svého tahu"]+=1; continue
    if not z:
        posl=[x for x in r["tahy"] if x["strana"]=="my"][-1]
        D["míč držel až do konce, nedoběhl"]+=1; KDE.append(25-posl["po"][1]); continue
    kdo,kolo,cim,rohy,zona,x=z[0]
    D[f"první ztráta: {kdo}"]+=1; KOLO[(kdo, "kolo 1-3" if kolo<=3 else "kolo 4-6" if kolo<=6 else "kolo 7-8")]+=1
    if kdo=="tah soupeře": ROH[rohy]+=1; ZON["nosič měl soupeře vedle sebe" if zona else "nosič bez soupeře vedle"]+=1
    else: VL[cim]+=1
print("PROČ poločas bez našeho TD (první událost):")
for k,v in D.most_common(): print(f"  {v:3d}  {k}")
if KDE: print("  držel do konce: zbývalo polí do TD zóny:", sorted(KDE))
print("ztráta v TAHU SOUPEŘE — kolik rohů měl nosič před jejich tahem:", dict(sorted(ROH.items())), "·", dict(ZON))
print("ztráta ve VLASTNÍM tahu — čím:", dict(VL.most_common()))
print("kdy:", dict(sorted(KOLO.items())))
# tempo
T=collections.defaultdict(list); X=collections.defaultdict(list); HYB=collections.Counter()
for r in R:
    for x in r["tahy"]:
        if x["strana"]=="my" and x["pred"][0]=="náš" and x["po"][0]=="náš":
            T[x["kolo"]].append(x["po"][1]-x["pred"][1]); X[x["kolo"]].append(x["po"][1])
            HYB["nosič stál" if x["po"][1]==x["pred"][1] and not x["nosic_hybal"] else "s klecí" if set(x["nosic_hybal"])<={"cage"} and x["nosic_hybal"] else "hledání/záchrana"]+=1
print("TEMPO: kolo: průměrný posun nosiče (počet tahů) · průměrné x po tahu")
for k in sorted(T): print(f"   kolo {k}: {sum(T[k])/len(T[k]):+.1f} ({len(T[k])}) · x={sum(X[k])/len(X[k]):.1f}")
allv=[v for k in T for v in T[k]]; print(f"   celkem průměr {sum(allv)/len(allv):+.2f} pole na tah s míčem · tahů bez posunu {sum(1 for v in allv if v<=0)}/{len(allv)} ·", dict(HYB))
for sou in sorted({r["kop"] for r in R}):
    q=[r for r in R if r["kop"]==sou]; print(f"   proti {sou:9s}: TD {sum(r['vysl']=='TD' for r in q)}/{len(q)}, TD soupeře {sum(r['vysl']=='TD soupeře' for r in q)}")
