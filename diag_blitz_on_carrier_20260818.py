"""P33: když je soupeřův nosič V DOSAHU BLITZU, utratíme blitz na něj? (18.08.)

Uživatel 18.08.: „přijde mi zajímavější situace blitz na ballcarriera."
Z měření P10a vypadlo, že je to 4,5× častější situace než blok na nosiče:
u nosiče stojí naše tělo ve 3 733 kolech, nestojí nikdo v 16 832 -- a
doblitzovali jsme 6 143× (36,5 %).

⛔ Jenže 36,5 % je BEZ JMENOVATELE. Blitz je jeden za kolo a v části těch kol
   na nosiče vůbec NEDOSÁHNEME. Správný jmenovatel je „kol, kde je nosič
   v dosahu blitzu", a teprve v něm má smysl ptát se, kam blitz šel místo něj.

Dosah = Chebyshev k prstenci kolem nosiče <= MA hráče (horní odhad: nepočítá
dodge z TZ ani obsazená pole; označeno tak i ve výstupu).

⛔ K32: V LOGU ŽÁDNÁ UDÁLOST `BLITZ` NENÍ -- jsou jen `BLOCK` (ověřeno 18.08.
   na korpusu: MOVE, BLOCK, PUSH, ARMOR_BREAK, ... a nic mezi tím). První verze
   tohohle skriptu proto vrátila 0,0 % a vypadalo to jako nález; byla to vada
   nástroje. Fronta to má zapsané jako K32 „blitz se v logu nepozná od bloku,
   BLOKOVÁNO na X1".

⭐ REKONSTRUKCE, KTERÁ K32 OBCHÁZÍ: blok vyžaduje sousedství, a blitz je jeden
   za kolo. Když tedy ÚTOČNÍK na začátku kola se svým cílem NESOUSEDIL, musel
   se k němu přesunout -- a to je blitz. Tahle definice nepotřebuje nový log.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

def cheb(a, b): return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

st = Counter(); reach_hist = Counter(); away_dist = Counter()
for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    for S in r["turn_logs"]:
        if S["active_team"] != ours:
            continue
        us = players(S, ours); th = players(S, them)
        car = next((p for p in th if p["has_ball"]), None)
        if car is None:
            continue
        st["kol se soupeřovým míčem"] += 1
        c = (car["x"], car["y"])
        us_st = [p for p in us if p["state"] == STANDING]
        adj = [p for p in us_st if cheb((p["x"], p["y"]), c) == 1]
        # kdo na něj DOSÁHNE blitzem (dojít na prstenec a udeřit)
        reach = [p for p in us_st
                 if max(0, cheb((p["x"], p["y"]), c) - 1) <= p["ma"]]
        ourIds = {p["id"] for p in us}
        # K32 rekonstrukce: blok, u kterého útočník s cílem na začátku kola
        # NESOUSEDIL, je blitz (blok vyžaduje sousedství, blitz je 1/kolo).
        startPos = {p["id"]: (p["x"], p["y"]) for p in us + th}
        blitzes = []
        for e in S["events"]:
            if e["type"] != "BLOCK" or e["player_id"] not in ourIds:
                continue
            a = startPos.get(e["player_id"]); b = startPos.get(e.get("target_id"))
            if a is None or b is None:
                continue
            if cheb(a, b) > 1:
                blitzes.append(e)
        onCar = [e for e in blitzes if e.get("target_id") == car["id"]]

        if adj:
            st["  (u nosiče už někdo stojí — patří k P10a)"] += 1
            continue
        st["nosič BEZ souseda"] += 1
        if not reach:
            st["  na nosiče NEDOSÁHNEME (mimo jmenovatel)"] += 1
            continue
        st["JMENOVATEL: nosič v dosahu blitzu"] += 1
        reach_hist[min(len(reach), 4)] += 1
        if onCar:
            st["  ⇒ blitz šel NA NOSIČE"] += 1
        elif blitzes:
            st["  ⇒ blitz šel JINAM"] += 1
            tgt = next((p for p in th if p["id"] == blitzes[0].get("target_id")), None)
            if tgt:
                away_dist[min(cheb((tgt["x"], tgt["y"]), c), 5)] += 1
        else:
            st["  ⇒ blitz jsme NEUTRATILI VŮBEC"] += 1

games = len(glob.glob(DATA + '/*.json.gz'))
n = st["JMENOVATEL: nosič v dosahu blitzu"]
print(f"korpus: {DATA.rsplit('/',1)[-1]}   her: {games}")
print("dosah = dojde na pole vedle nosiče (horní odhad: bez dodge a bez obsazených polí)\n")
for k, v in st.items():
    print(f"  {k:48s} {v:8d}   {v/games:6.2f} / zápas")
print(f"\n  ⇒ z kol, kde nosič JE v dosahu blitzu, jsme na něj blitzovali v "
      f"{100*st['  ⇒ blitz šel NA NOSIČE']/max(1,n):.1f} %")
print("\nKOLIK NAŠICH TĚL NA NOSIČE DOSÁHNE (jen kola v jmenovateli)")
for k in sorted(reach_hist):
    print(f"  {k if k<4 else '4+':>3}   {reach_hist[k]:7d}   {100*reach_hist[k]/max(1,n):5.1f} %")
print("\nKDYŽ ŠEL BLITZ JINAM — jak daleko od nosiče ten cíl stál")
tot = sum(away_dist.values())
for k in sorted(away_dist):
    print(f"  {k if k<5 else '5+':>3} polí   {away_dist[k]:7d}   {100*away_dist[k]/max(1,tot):5.1f} %")
