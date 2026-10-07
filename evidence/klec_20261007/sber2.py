# Tahy s míčem + stopa řadiče + ztráta v dalším tahu soupeře. sber2.py KOŘEN vystup.pkl rasa [semínek=20]
import sys, os, pickle, tempfile, multiprocessing as mp
KOREN=sys.argv[1]; VYST=sys.argv[2]; RASA=sys.argv[3]; N=int(sys.argv[4]) if len(sys.argv)>4 else 20
RASY=["dwarf","wood-elf","skaven","orc","human"]
os.environ["BB_CAGE_DEBUG"]="1"
sys.path.insert(0,KOREN)
def hra(a):
    prij,kop,g=a
    import bb_kontrola_tahu as kt
    bb=kt.bb; H,A=bb.TeamSide.HOME,bb.TeamSide.AWAY; ST=bb.PlayerState
    def okoli(s):
        n=s.get_player(s.ball.carrier_id); x,y=n.position.x,n.position.y; out=[]
        for dy in range(-3,4):
            row=""
            for dx in range(-3,4):
                q=[p for p in kt.hraci(s) if p.position.x==x+dx and p.position.y==y+dy]
                if not (0<=x+dx<=25 and 0<=y+dy<=14): row+="#"
                elif not q: row+="."
                else:
                    p=q[0]; c=("N" if p.id==n.id else "o") if p.team_side==H else "X"
                    row+= c if p.state==ST.STANDING else c.lower() if c!="o" else "_"
            out.append(row)
        return out
    tmp=tempfile.TemporaryFile(mode="w+"); old=os.dup(2); os.dup2(tmp.fileno(),2)
    s=bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij,1500), bb.get_developed_roster(kop,1500), A); s.kicking_team=A
    bb.simple_kickoff(s, bb.DiceRoller(3000+g))
    out=[]; posl=None
    for tah in range(16):
        if s.phase!=bb.GamePhase.PLAY: break
        strana=s.active_team
        tmp.seek(0); tmp.truncate()
        kroky,po,_=kt.plan_tahu(s, ai="macro_mcts", seed=50000+100*g+tah); kroky=list(kroky)
        sys.stderr.flush(); tmp.seek(0); dbg=[l.strip() for l in tmp.read().split("\n") if "[cage" in l]
        td = po.home_team.score+po.away_team.score > s.home_team.score+s.away_team.score
        nasmic = (not td) and po.phase==bb.GamePhase.PLAY and po.ball.is_held and po.get_player(po.ball.carrier_id).team_side==H
        if strana==A and posl is not None: posl["ztrata"]= not nasmic; posl=None
        if strana==H:
            drzel=s.ball.is_held and s.get_player(s.ball.carrier_id).team_side==H
            nos=s.ball.carrier_id if drzel else -1
            mk=[]
            for k in kroky:
                if "macro" in k: m=k["macro"]; mk.append((m["source"],m["type"],m["player"]==nos, k["turnover"]))
            r=dict(g=g,kop=kop,kolo=s.home_team.turn_number,drzel=drzel,ma=nasmic,to=any(k["turnover"] for k in kroky),dbg=dbg,makra=mk,ztrata=None,td=td)
            if drzel: r["pred"]=okoli(s)
            if nasmic:
                n=po.get_player(po.ball.carrier_id); x,y=n.position.x,n.position.y
                r["rohy"]=sum(1 for p in kt.hraci(po) if p.team_side==H and p.state==ST.STANDING and p.id!=n.id and abs(p.position.x-x)==1 and abs(p.position.y-y)==1)
                r["zona"]=sum(1 for p in kt.hraci(po) if p.team_side==A and p.state==ST.STANDING and max(abs(p.position.x-x),abs(p.position.y-y))==1)
                r["xy"]=(x,y); r["po"]=okoli(po); r["nasich_stoji"]=sum(1 for p in kt.hraci(po) if p.team_side==H and p.state==ST.STANDING)
                posl=r
            out.append(r)
        if td: break
        s=po
    os.dup2(old,2)
    return out
if __name__=="__main__":
    ukoly=[(RASA,k,g) for k in RASY if k!=RASA for g in range(N)]
    with mp.Pool(8) as pool: res=pool.map(hra, ukoly, chunksize=1)
    pickle.dump([t for h in res for t in h], open(VYST,"wb")); print("hotovo", len(res))
