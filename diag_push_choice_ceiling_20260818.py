"""STROP P9/P9c: kolik odsunů má vůbec na výběr — a kolikrát vybereme hůř? (18.08.)

Pravidlo: strop se počítá PŘED nocí. Dnes to zabilo P10a (81,5 % úderů na
nosiče už teď) a P8 před tím (0,056 faulu na zápas).

`choosePushSquare` (block_handler.cpp:156) volí `score = count - i`, tedy
„rovně dozadu první". Cílové pole se nehodnotí NIJAK, kromě (a) prázdné vs
obsazené, (b) odmítnutí darovaného TD, (c) Side Step / Grab.

Tady se ptáme na jmenovatel i na čitatel:
  jmenovatel = naše odsuny, kde byla SKUTEČNÁ volba (>=2 prázdná kandidátní pole)
  čitatel A  = vybrané pole nechalo odsunutého SOUSEDIT s rohem naší klece,
               ačkoli jiné prázdné pole by ho odtud odklidilo   (P9c)
  čitatel B  = vybrané pole přiblížilo odsunutého k NAŠEMU NOSIČI,
               ačkoli jiné prázdné pole ho oddálilo             (P9)
"""
import sys, glob
from collections import Counter
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

def cheb(a, b): return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

def pushback_squares(pusher, pushed):
    """Tři pole 'pryč od blokujícího' -- táž geometrie jako getPushbackSquares."""
    dx = pushed[0] - pusher[0]; dy = pushed[1] - pusher[1]
    dx = (dx > 0) - (dx < 0); dy = (dy > 0) - (dy < 0)
    straight = (pushed[0] + dx, pushed[1] + dy)
    if dx and dy:      # úhlopříčka: sousedi jsou ortogonální kroky
        sides = [(pushed[0] + dx, pushed[1]), (pushed[0], pushed[1] + dy)]
    elif dx:           # vodorovně
        sides = [(pushed[0] + dx, pushed[1] - 1), (pushed[0] + dx, pushed[1] + 1)]
    else:              # svisle
        sides = [(pushed[0] - 1, pushed[1] + dy), (pushed[0] + 1, pushed[1] + dy)]
    return [straight] + sides

ON = lambda p: 0 <= p[0] <= 25 and 0 <= p[1] <= 14

st = Counter()
for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    for S in r["turn_logs"]:
        if S["active_team"] != ours:
            continue
        us = players(S, ours); th = players(S, them)
        ourIds = {p["id"] for p in us}
        pos = {p["id"]: (p["x"], p["y"]) for p in us + th}
        occupied = set(pos.values())
        car = next((p for p in us if p["has_ball"]), None)
        carPos = (car["x"], car["y"]) if car else None
        corners = set()
        if carPos:
            corners = {(carPos[0]+dx, carPos[1]+dy) for dx in (-1,1) for dy in (-1,1)}
        # blokující = poslední BLOCK/BLITZ našeho hráče před PUSH
        pusher = None
        for e in S["events"]:
            if e["type"] in ("BLOCK", "BLITZ") and e["player_id"] in ourIds:
                pusher = pos.get(e["player_id"])
            if e["type"] != "PUSH" or pusher is None:
                continue
            src = (e.get("from_x", -1), e.get("from_y", -1))
            dst = (e.get("to_x", -1), e.get("to_y", -1))
            if src == (-1, -1) or dst == (-1, -1) or e["player_id"] in ourIds:
                continue
            st["našich odsunů"] += 1
            cands = [c for c in pushback_squares(pusher, src) if ON(c)]
            empty = [c for c in cands if c not in occupied or c == dst]
            if len(empty) < 2:
                st["bez volby (0-1 prázdné pole)"] += 1
                continue
            st["SE SKUTEČNOU VOLBOU (>=2 prázdná)"] += 1
            # A) roh klece
            if corners:
                stays = any(cheb(dst, k) <= 1 for k in corners)
                betters = [c for c in empty if not any(cheb(c, k) <= 1 for k in corners)]
                if stays and betters:
                    st["A) nechali ho u rohu, ač šlo jinam"] += 1
            # B) blízkost k našemu nosiči
            if carPos:
                dNow = cheb(dst, carPos)
                dBest = max(cheb(c, carPos) for c in empty)
                if dBest > dNow:
                    st["B) přisunuli k nosiči, ač šlo dál"] += 1
                    if dNow <= 1 and dBest > 1:
                        st["   z toho: přilepili ho PŘÍMO k nosiči"] += 1

games = len(glob.glob(DATA + '/*.json.gz'))
n = st["SE SKUTEČNOU VOLBOU (>=2 prázdná)"]
print(f"korpus: {DATA.rsplit('/',1)[-1]}   her: {games}\n")
for k, v in st.items():
    print(f"  {k:44s} {v:8d}   {v/games:6.2f} / zápas")
print(f"\n  ⇒ z odsunů se skutečnou volbou: A {100*st['A) nechali ho u rohu, ač šlo jinam']/max(1,n):.1f} % · "
      f"B {100*st['B) přisunuli k nosiči, ač šlo dál']/max(1,n):.1f} %")
