"""Blok na soupeřova nosiče: KOLIK MÁME BLÍZKO VOLNÝCH TĚL NA SCRAMBLE? (18.08.)

Uživatel 18.08.: „ještě k tomu blocku na nosiče — zkusit dopočítat, kolik
dalších volných hráčů máme pak blízko."

⚑ PROČ TO ROZHODUJE P10a
  Sražení nosiče není samo o sobě zisk — míč se **rozptýlí z pole nosiče**
  a začne scramble. Cena té akce je proto podmíněná: kolik NAŠICH těl je
  u toho pole a kolik jich tam má soupeř. Když jsme tam sami proti třem,
  sražením nosiče jsme mu míč vlastně uvolnili.

⚑ CO SE POČÍTÁ
  Na snímku ZAČÁTKU našeho kola (`logs[i]`, ne po pohybu):
   * má míč soupeř? jinak N/A
   * je blok na nosiče k dispozici = stojí náš STOJÍCÍ hráč vedle nosiče?
   * kolik dalších našich stojících těl je do vzdálenosti 1/2/3 od POLE NOSIČE
     (odtud se míč rozptyluje), BEZ toho, kdo by udeřil
   * z nich VOLNÁ = nesousedí s žádným stojícím soupeřem (jinak je zamčené,
     K36) -- „volný" v tomhle projektu znamená, že tělo může jednat, ne že stojí
   * a totéž pro SOUPEŘE, protože scramble je závod, ne sólo
"""
import sys, glob, math
from collections import Counter, defaultdict
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

def cheb(a, b):
    return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

st = Counter()
ours_free = defaultdict(list)     # d -> [počty]
ours_all  = defaultdict(list)
theirs_free = defaultdict(list)
hist2 = Counter()
edge = Counter()

for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    logs = r["turn_logs"]
    for i, S in enumerate(logs):
        if S["active_team"] != ours:
            continue
        st["našich kol"] += 1
        us = players(S, ours); th = players(S, them)
        car = next((p for p in th if p["has_ball"]), None)
        if car is None:
            st["soupeř nemá míč (N/A)"] += 1
            continue
        st["soupeř má míč"] += 1
        c = (car["x"], car["y"])
        us_st = [p for p in us if p["state"] == STANDING]
        th_st = [p for p in th if p["state"] == STANDING and p["id"] != car["id"]]
        blockers = [p for p in us_st if cheb((p["x"], p["y"]), c) == 1]
        if not blockers:
            st["blok na nosiče NENÍ k dispozici"] += 1
            continue
        st["blok na nosiče JE k dispozici"] += 1
        st["blokujících k dispozici celkem"] += len(blockers)
        # ten, kdo udeří, u scramble nestojí volný -- odečítá se jeden
        bid = blockers[0]["id"]
        def free(p, opp):
            return not any(cheb((p["x"], p["y"]), (o["x"], o["y"])) == 1 for o in opp)
        for d in (1, 2, 3):
            near_us = [p for p in us_st if p["id"] != bid and cheb((p["x"], p["y"]), c) <= d]
            near_th = [p for p in th_st if cheb((p["x"], p["y"]), c) <= d]
            ours_all[d].append(len(near_us))
            ours_free[d].append(sum(1 for p in near_us if free(p, th_st)))
            theirs_free[d].append(sum(1 for p in near_th if free(p, us_st)))
        n2 = ours_free[2][-1]
        hist2[min(n2, 4)] += 1
        # převaha u pole nosiče do 2: vyhráváme scramble, nebo darujeme míč?
        edge["my >" if ours_free[2][-1] > theirs_free[2][-1] else
             "shoda" if ours_free[2][-1] == theirs_free[2][-1] else "soupeř >"] += 1

def m(v):
    return sum(v) / len(v) if v else float("nan")

print(f"korpus: {DATA.rsplit('/',1)[-1]}   her: {len(glob.glob(DATA+'/*.json.gz'))}")
print("snímek = ZAČÁTEK našeho kola; vzdálenost = Chebyshev k POLI NOSIČE\n")
for k, v in st.items():
    print(f"  {k:38s} {v}")
n = st["blok na nosiče JE k dispozici"]
print(f"\n  ⇒ blok na nosiče je k dispozici v {100*n/max(1,st['soupeř má míč']):.1f} % kol, "
      f"kdy soupeř drží míč (a v {100*n/max(1,st['našich kol']):.1f} % všech našich kol)")
print(f"  ⇒ průměrně {st['blokujících k dispozici celkem']/max(1,n):.2f} kandidáta na úder\n")

print("KOLIK DALŠÍCH TĚL MÁME U POLE NOSIČE (bez toho, kdo udeří)")
print(f"{'do':>4} {'našich stojících':>18} {'z toho VOLNÝCH':>16} {'soupeřových volných':>21}")
for d in (1, 2, 3):
    print(f"{d:>4} {m(ours_all[d]):>18.2f} {m(ours_free[d]):>16.2f} {m(theirs_free[d]):>21.2f}")

print("\nROZLOŽENÍ našich VOLNÝCH těl do 2 polí od nosiče")
for k in sorted(hist2):
    lbl = f"{k}" if k < 4 else "4+"
    print(f"  {lbl:>3} volných   {hist2[k]:6d}   {100*hist2[k]/max(1,n):5.1f} %")

print("\nKDO MÁ U POLE NOSIČE PŘEVAHU (volná těla do 2)")
for k in ("my >", "shoda", "soupeř >"):
    print(f"  {k:>9}   {edge[k]:6d}   {100*edge[k]/max(1,n):5.1f} %")
