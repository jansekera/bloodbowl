# TD v poločasech v útoku, všech 5 ras TV1500, každá přijímá proti každé. td_vse.py KOŘEN vystup.pkl [semínek] [rasy,čárkou]
import sys, os, pickle, itertools, multiprocessing as mp
KOREN=sys.argv[1]; VYST=sys.argv[2]; N=int(sys.argv[3]) if len(sys.argv)>3 else 30
RASY=sys.argv[4].split(",") if len(sys.argv)>4 else ["dwarf","wood-elf","skaven","orc","human"]
sys.path.insert(0,KOREN)
def hra(a):
    prij,kop,g=a
    import bb_kontrola_tahu as kt
    bb=kt.bb; H,A=bb.TeamSide.HOME,bb.TeamSide.AWAY; ST=bb.PlayerState
    assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__
    s=bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij,1500), bb.get_developed_roster(kop,1500), A); s.kicking_team=A
    bb.simple_kickoff(s, bb.DiceRoller(3000+g))
    r=dict(prij=prij,kop=kop,g=g,td=0,td_soupere=0,tahu=0,to=0,s_micem=0,ciste=0,kolo_td=0)
    for tah in range(16):
        if s.phase!=bb.GamePhase.PLAY: break
        strana=s.active_team
        kroky,po,_=kt.plan_tahu(s, ai="macro_mcts", seed=50000+100*g+tah)
        if strana==H:
            r["tahu"]+=1; r["to"]+=any(k["turnover"] for k in kroky)
            if po.phase==bb.GamePhase.PLAY and po.ball.is_held and po.get_player(po.ball.carrier_id).team_side==H:
                n=po.get_player(po.ball.carrier_id); x,y=n.position.x,n.position.y
                rohy=sum(1 for p in kt.hraci(po) if p.team_side==H and p.state==ST.STANDING and p.id!=n.id and abs(p.position.x-x)==1 and abs(p.position.y-y)==1)
                r["s_micem"]+=1; r["ciste"]+=rohy==4
        if po.home_team.score>s.home_team.score: r["td"]=1; r["kolo_td"]=s.home_team.turn_number; break
        if po.away_team.score>s.away_team.score: r["td_soupere"]=1; break
        s=po
    return r
if __name__=="__main__":
    ukoly=[(p,k,g) for p,k in itertools.permutations(RASY,2) for g in range(N)]
    with mp.Pool(8) as pool: res=pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST,"wb")); print("hotovo", len(res))
