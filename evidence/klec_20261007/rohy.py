import sys, pickle, collections, re
for f in sys.argv[1:]:
    R=pickle.load(open(f,"rb")); ma=[r for r in R if r["ma"] and r["ztrata"] is not None]; n=len(ma)
    S=collections.Counter(); SZ=collections.Counter(); M=collections.Counter(); JAK=collections.Counter(); JAKN=collections.Counter()
    for r in ma:
        b=r["po"]; g=lambda dx,dy: b[3+dy][3+dx]
        X=lambda dx,dy: any(g(dx+ax,dy+ay)=="X" for ax in(-1,0,1) for ay in(-1,0,1) if (ax or ay) and -3<=dx+ax<=3 and -3<=dy+ay<=3)
        cor=[(-1,-1),(1,-1),(-1,1),(1,1)]
        stoji=[c for c in cor if g(*c)=="o"]; spin=[c for c in stoji if X(*c)]
        nos_zona=X(0,0)
        if len(stoji)==4 and not spin and not nos_zona: k="4 rohy, všechny čisté"
        elif len(stoji)==4 and not nos_zona: k=f"4 rohy, {len(spin)} v kontaktu se soupeřem"
        elif len(stoji)==4: k="4 rohy, soupeř u nosiče"
        elif len(stoji)==3: k="3 rohy"
        elif len(stoji)==2: k="2 rohy"
        else: k="0–1 roh"
        S[k]+=1; SZ[k]+=r["ztrata"]
        d="\n".join(r["dbg"])
        jak=("zvedl" if not r["drzel"] else "výběh" if ("VÝBĚH" in d or "výběh" in d) else "hledání pohnulo nosičem" if any(m[2] and m[0]!="cage" for m in r["makra"]) else "nosič s řadičem" if any(m[2] for m in r["makra"]) else "nosič stál")
        JAKN[jak]+=1
        for c in cor:
            if g(*c)=="o": continue
            ch=g(*c)
            if ch=="#": why="pole mimo hřiště"
            elif ch=="X": why="na poli stojí soupeř"
            elif ch=="x": why="na poli leží soupeř"
            elif ch=="_": why="na poli leží náš"
            else:
                # volné pole: je do 2 polí stojící náš hráč, který není rohem?
                near=[(c[0]+ax,c[1]+ay) for ax in range(-2,3) for ay in range(-2,3) if -3<=c[0]+ax<=3 and -3<=c[1]+ay<=3 and (c[0]+ax,c[1]+ay) not in cor and (c[0]+ax,c[1]+ay)!=(0,0) and g(c[0]+ax,c[1]+ay)=="o"]
                why="pole volné, náš stojící do 2 polí je" if near else "pole volné, nikdo náš do 2 polí"
                if X(*c): why+=" (pole v zóně soupeře)"
            M[why]+=1; JAK[(jak,why)]+=1
    print(f"== {f}: tahů končících s míčem {n}")
    for k in ["4 rohy, všechny čisté","4 rohy, 1 v kontaktu se soupeřem","4 rohy, 2 v kontaktu se soupeřem","4 rohy, 3 v kontaktu se soupeřem","4 rohy, 4 v kontaktu se soupeřem","4 rohy, soupeř u nosiče","3 rohy","2 rohy","0–1 roh"]:
        if S[k]: print(f"   {k:36s} {100*S[k]/n:4.0f} % ({S[k]:3d}) · ztráta v dalším tahu {SZ[k]:2d} = {100*SZ[k]/S[k]:3.0f} %")
    print("   CHYBĚJÍCÍ ROHY podle příčiny (počet polí):")
    for k,v in M.most_common(): print(f"     {v:4d}  {k}")
    print("   chybějící rohy podle toho, jak se nosič v tahu choval (na tah):", {k:f"{sum(v for (j,w),v in JAK.items() if j==k)/JAKN[k]:.2f} ({JAKN[k]} tahů)" for k in JAKN})
