# Kde končí útočný poločas bez TD. diag.py KOŘEN vystup.pkl [rasa=dwarf] [semínek=40]
import sys, os, pickle, multiprocessing as mp
KOREN=sys.argv[1]; VYST=sys.argv[2]; RASA=sys.argv[3] if len(sys.argv)>3 else "dwarf"; N=int(sys.argv[4]) if len(sys.argv)>4 else 40
RASY=["dwarf","wood-elf","skaven","orc","human"]
sys.path.insert(0,KOREN)
def hra(a):
    prij,kop,g=a
    import bb_kontrola_tahu as kt
    bb=kt.bb; H,A=bb.TeamSide.HOME,bb.TeamSide.AWAY; ST=bb.PlayerState
    def stav(s):
        if not s.ball.is_held: return ("zem", s.ball.position.x, 0, 0)
        n=s.get_player(s.ball.carrier_id); x,y=n.position.x,n.position.y
        if n.team_side!=H: return ("soupeř", x, 0, 0)
        rohy=sum(1 for p in kt.hraci(s) if p.team_side==H and p.state==ST.STANDING and p.id!=n.id and abs(p.position.x-x)==1 and abs(p.position.y-y)==1)
        zona=sum(1 for p in kt.hraci(s) if p.team_side==A and p.state==ST.STANDING and max(abs(p.position.x-x),abs(p.position.y-y))==1)
        return ("náš", x, rohy, zona)
    s=bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij,1500), bb.get_developed_roster(kop,1500), A); s.kicking_team=A
    bb.simple_kickoff(s, bb.DiceRoller(3000+g))
    r=dict(prij=prij,kop=kop,g=g,vysl="bez TD",tahy=[])
    for tah in range(16):
        if s.phase!=bb.GamePhase.PLAY: break
        strana=s.active_team; pred=stav(s)
        kroky,po,_=kt.plan_tahu(s, ai="macro_mcts", seed=50000+100*g+tah); kroky=list(kroky)
        mk=[]; 
        for k in kroky:
            if "macro" in k: m=k["macro"]; mk.append([m["source"],m["type"],m["player"],False])
            if k["turnover"] and mk: mk[-1][3]=True
        td_h=po.home_team.score>s.home_team.score; td_a=po.away_team.score>s.away_team.score
        nos=s.ball.carrier_id if s.ball.is_held else -1
        r["tahy"].append(dict(strana="my" if strana==H else "on", kolo=(s.home_team if strana==H else s.away_team).turn_number,
                              pred=pred, po=("TD",0,0,0) if (td_h or td_a) else stav(po), to=[m[:3] for m in mk if m[3]],
                              nosic_hybal=[m[0] for m in mk if m[2]==nos and m[1] in ("REPOSITION","ADVANCE")] if strana==H else [],
                              makra=[(m[0],m[1]) for m in mk]))
        if td_h: r["vysl"]="TD"; break
        if td_a: r["vysl"]="TD soupeře"; break
        s=po
    return r
if __name__=="__main__":
    ukoly=[(RASA,k,g) for k in RASY if k!=RASA for g in range(N)]
    with mp.Pool(8) as pool: res=pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST,"wb")); print("hotovo", len(res))
