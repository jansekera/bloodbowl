import sys, pickle, collections, math
A=pickle.load(open(sys.argv[1],"rb")); B=pickle.load(open(sys.argv[2],"rb"))   # A = báze, B = nová
ka={(r["prij"],r["kop"],r["g"]):r for r in A}; kb={(r["prij"],r["kop"],r["g"]):r for r in B}
klice=sorted(set(ka)&set(kb)); print("párů", len(klice), "(báze", len(A), ", nová", len(B), ")")
def radek(jm, ks):
    n=len(ks); ta=sum(ka[k]["td"] for k in ks); tb=sum(kb[k]["td"] for k in ks)
    plus=sum(kb[k]["td"]>ka[k]["td"] for k in ks); minus=sum(kb[k]["td"]<ka[k]["td"] for k in ks)
    z=(plus-minus)/math.sqrt(plus+minus) if plus+minus else 0
    ca=sum(ka[k]["ciste"] for k in ks)/max(1,sum(ka[k]["s_micem"] for k in ks)); cb=sum(kb[k]["ciste"] for k in ks)/max(1,sum(kb[k]["s_micem"] for k in ks))
    oa=sum(ka[k]["to"] for k in ks)/sum(ka[k]["tahu"] for k in ks); ob=sum(kb[k]["to"] for k in ks)/sum(kb[k]["tahu"] for k in ks)
    sa=sum(ka[k]["td_soupere"] for k in ks); sb=sum(kb[k]["td_soupere"] for k in ks)
    print(f"{jm:22s} n={n:4d} | TD {ta:3d} → {tb:3d} ({100*ta/n:4.1f} → {100*tb/n:4.1f} %)  jen nová {plus:3d}, jen báze {minus:3d}, z={z:+.2f} | čistá klec {100*ca:3.0f} → {100*cb:3.0f} % | turnover {100*oa:3.0f} → {100*ob:3.0f} % | TD soupeře {sa} → {sb}")
radek("VŠECHNY RASY", klice)
for r in sorted({k[0] for k in klice}): radek("přijímá "+r, [k for k in klice if k[0]==r])
print()
for r in sorted({k[1] for k in klice}): radek("proti "+r, [k for k in klice if k[1]==r])
