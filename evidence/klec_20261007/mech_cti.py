import sys, pickle, collections
for f in sys.argv[1:]:
    H=pickle.load(open(f,"rb")); T=[t for h in H for t in h if t["my"]]; n=len(T)
    d=sum(t["dodge"] for t in T); df=sum(t["dodge_fail"] for t in T); g=sum(t["gfi"] for t in T)
    zv=[t for t in T if t["zvedl"]]; dr=[t for t in T if t["drzel"] and t["rohy"]>=0]
    C=collections.Counter(t["to_cim"] for t in T if t["to"])
    print(f"== {f}: poločasů {len(H)}, vlastních tahů {n}, TD {sum(t['td'] for t in T)}")
    print(f"   úhyby: {d/n:.2f} na tah · první hod nevyšel {df}/{d} = {100*df/max(d,1):.0f} %   (úhyb 2+ = 17 %, 3+ = 33 %, 4+ = 50 %) · GFI {g/n:.2f} na tah")
    print(f"   turnover {sum(t['to'] for t in T)}/{n} = {100*sum(t['to'] for t in T)/n:.0f} % tahů · po úhybu {sum(v for k,v in C.items() if k.startswith('DODGE'))} · čím: {dict(C.most_common(6))}")
    print(f"   tahy se zvednutím: čistá klec {sum(t['rohy']==4 for t in zv)}/{len(zv)} · tahy s míčem od začátku: čistá {sum(t['rohy']==4 for t in dr)}/{len(dr)} · nosičem pohnulo hledání {sum(t['nosic_hledani'] for t in T)}")
