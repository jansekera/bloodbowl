#!/usr/bin/env python3
"""P59 horizont, SPRÁVNÝ TVAR: když ztratíme míč TADY, skóruje soupeř ve svém
   PŘÍŠTÍM kole -- během, nebo přihrávkou?

⛔ Měření (4) z 25.08. mělo ŠPATNÝ TVAR: počítalo `vzdálenost / zbývající kola
   <= MA+2`, tedy TEMPO NA VÍC KOL -- a to je odhad. Číslo 64 % se necituje.
   Správná veličina je BINÁRNÍ A JEDNOKOLOVÁ (uživatel 25.08.: „vždy ti stačí
   tvoje aktuální kolo a co na to soupeř v jeho následujícím").

Počítá se z NAŠEHO kola, kdy MY držíme míč -- tedy „co by bylo, kdybychom ho
tady ztratili". Pro každého jejich stojícího hráče:
   BĚH:       dojde na míč a odtud do endzóny? (MA + 2 GFI, Chebyshev)
   PŘIHRÁVKA: dojde na míč NĚKDO, a je někdo JINÝ už teď v dosahu endzóny?
              (přihrávka je jen doručení -- příjemce musí dojít sám)

⚠️ HORNÍ MEZ, a volná: bez tackle zón, dodge, hodů a bez toho, že jim v cestě
   stojíme; dosah Chebyshevem. Když i tak vyjde nízko, hrozba je vzácná.
"""
import gzip, json, glob, sys
from collections import Counter

def cheb(ax,ay,bx,by): return max(abs(ax-bx),abs(ay-by))

def main(limit):
    T=Counter()
    for f in sorted(glob.glob('blitzlanding_replic_20260825_corpus_data/g*.json.gz'))[:limit]:
        d=json.load(gzip.open(f))
        dw='home' if d['home_race']=='dwarf' else 'away'
        opp='away' if dw=='home' else 'home'
        # jejich endzóna: kam ONI útočí
        their_ez = 25 if opp=='home' else 0
        for t in d['turn_logs']:
            if t['active_team']!=dw: continue
            cid=t.get('ball_carrier_id',-1)
            if not cid or cid<1: continue
            mine=[p for p in t[dw+'_players'] if p['id']==cid]
            if not mine: continue
            c=mine[0]
            bx,by=c['x'],c['y']
            their=[p for p in t[opp+'_players'] if p['state']==0]
            if not their: continue
            T['nasich_kol_s_micem']+=1
            reach=[]                       # kdo dojde na míč
            scorers=[]                     # kdo dojde na míč A odtud do endzóny
            near_ez=[]                     # kdo je UŽ TEĎ v dosahu endzóny (příjemce)
            for p in their:
                ma=p.get('ma',6)
                d_ball=cheb(p['x'],p['y'],bx,by)
                if d_ball<=ma+2:
                    reach.append(p)
                    # zbytek pohybu po sebrání
                    left=ma+2-d_ball
                    if abs(their_ez-bx)<=left: scorers.append(p)
                if abs(their_ez-p['x'])<=ma+2: near_ez.append(p)
            if reach: T['dosahne_na_mic']+=1
            if scorers: T['BEHEM_skoruje']+=1
            # přihrávka: někdo vezme míč, JINÝ je v dosahu endzóny
            pass_ok = bool(reach) and any(q['id']!=r['id'] for r in reach for q in near_ez)
            if pass_ok: T['PRIHRAVKOU_skoruje']+=1
            if scorers or pass_ok: T['SKORUJE_JAKKOLI']+=1
            if pass_ok and not scorers: T['JEN_prihravkou']+=1
    n=T['nasich_kol_s_micem'] or 1
    print(f"trpaslík drží míč, {limit} her: {T['nasich_kol_s_micem']} kol\n")
    print("  KDYBYCHOM MÍČ ZTRATILI TADY, skóruje soupeř ve SVÉM PŘÍŠTÍM kole?\n")
    for k in ('dosahne_na_mic','BEHEM_skoruje','PRIHRAVKOU_skoruje','SKORUJE_JAKKOLI','JEN_prihravkou'):
        print(f"    {k:24s} {T[k]:6d}  {100*T[k]/n:5.1f} %")
    print(f"\n  ⛔ číslo 64 % z 25.08. se NECITUJE -- mělo tvar 'tempo na víc kol', tedy odhad.")

if __name__=='__main__':
    main(int(sys.argv[1]) if len(sys.argv)>1 else 400)
