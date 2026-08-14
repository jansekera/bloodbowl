import gzip,json,os,sys
ST="/tmp/play_state.json"
def load_init():
    r=json.load(gzip.open("diag_replay_mine_20260811b_data/g0000.json.gz","rt"))
    t=next(x for x in r["turn_logs"] if x["half"]==1 and x["turn"]==2 and x["active_team"]=="home")
    P={}
    for side,us in (("home",True),("away",False)):
        for p in t[f"{side}_players"]:
            if p["state"]==3: continue
            P[str(p["id"])]={"x":p["x"],"y":p["y"],"st":p["state"],"us":us,
                             "name":p["name"],"ball":p["has_ball"],
                             "ma":p["ma"],"stt":p["st"],"ag":p["ag"],"av":p["av"],
                             "acted":False}
    return {"players":P,"log":[]}
def code(p):
    n=p["name"]
    if p["us"]:
        c=("DR" if n.startswith("Runner") else "DTS" if n.startswith("Troll")
           else "DBZ" if n.startswith("Blitzer") else "DLG" if "+Guard" in n else "DL")
    else:
        c=("SGR" if n.startswith("Gutter") else "SBZ" if n.startswith("Blitzer")
           else "STH" if n.startswith("Thrower") else "SL")
    if p["st"]!=0: c=c.lower()
    return c
def render(S,X0=8,X1=20,Y0=3,Y1=13,w=7):
    cells={}
    for i,p in S["players"].items():
        cells[(p["x"],p["y"])]=f"{code(p)}{i}"+("*" if p["ball"] else "")
    out=["       "+"".join(f"{x:^{w}}" for x in range(X0,X1+1))+"  x"]
    for y in range(Y0,Y1+1):
        out.append(f"  y={y:>2} "+"".join(f"{cells.get((x,y),'·'):^{w}}" for x in range(X0,X1+1)))
    return "\n".join(out)
S=json.load(open(ST)) if os.path.exists(ST) else load_init()
for arg in sys.argv[1:]:
    if arg=="reset": S=load_init(); continue
    pid,dest=arg.split("@"); x,y=map(int,dest.split(","))
    S["players"][pid]["x"]=x; S["players"][pid]["y"]=y
    S["players"][pid]["acted"]=True
    S["log"].append(f"{pid} -> ({x},{y})")
json.dump(S,open(ST,"w"))
print(render(S))
print("\nprovedeno: "+" · ".join(S["log"]) if S["log"] else "")
