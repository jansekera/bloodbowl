#!/usr/bin/env python3
"""Exposure scan — co deska na KONCI našeho kola dává soupeři, a předpovídá to něco?

Deska na konci našeho kola i = turn_logs[i+1] (pozice v logu jsou začátek tahu).
Výsledek soupeřova kola = srovnání turn_logs[i+1] -> turn_logs[i+2].

Kandidátní veličiny (počítané na desce turn_logs[i+1]):
  FB       bezplatné bloky: kolik JEJICH stojících hráčů sousedí s aspoň jedním
           naším stojícím (mohou hodit Block bez blitzu)
  FB2      z toho kolik jich má na nejlepší sousední cíl >= 2 kostky
           (ST + asistence; Guard asistuje i markovaný — na obou stranách)
  MARKED   kolik NAŠICH stojících stojí vedle aspoň jednoho jejich stojícího
           (počet našich terčů; duál FB)
  SURF     naši stojící do 1 pole od postranní lajny (y<=1 / y>=13) se stojícím
           soupeřem vedle
  -- jen když náš nosič drží míč:
  ESC      čistá úniková pole nosiče (sousední volné pole s 0 jejich TZ)
  REACH    kolik jejich stojících dosáhne na pole vedle nosiče (BFS 8směr přes
           volná pole, dosah MA+2 GFI; sousedství = dosah 0)
  REACH0   z toho bez jediného dodge (cesta nikdy neopouští pole v naší TZ)
  BLZ      nejlepší kostky, které si na nosiče postaví jejich blitz
           (asistence útoku = jejich hráči už stojící u nosiče; obranné asistence
           u neznámé pozice útočníka ~ 0 -> mírně nadsazuje jejich kostky)
  CCBAD    "zlé rohy klece": 4 diagonály nosiče, roh je DOBRÝ jen když na něm
           stojí náš stojící hráč mimo jejich TZ (pravidlo: roh v TZ = prázdný)

Výsledky soupeřova kola (turn_logs[i+1] -> [i+2]):
  down     kolik našich stojících šlo k zemi (stav 1/2/3 na začátku našeho
           dalšího kola)
  lost     kolik našich zmizelo ze hřiště (stav 3: KO/CAS/surf)
  ball_lost  drželi jsme na konci kola míč a na začátku dalšího už ne
  appr     posun soupeře k nosiči: min Čebyšev vzdálenost jejich stojících
           k nosiči, delta (i+2 minus i+1); záporná = přiblížili se

Vyloučení: kolo s TD, konec poločasu/hry mezi i..i+2 (deska se přestaví).
"""
import gzip, json, glob, math, collections, sys

DATA = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260811b_data"
STANDING, PRONE, STUNNED, OUT = 0, 1, 2, 3
W, H = 26, 15


def adj(x, y):
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if (dx or dy) and 0 <= x + dx < W and 0 <= y + dy < H:
                yield x + dx, y + dy


def guard(p):
    return "+Guard" in p["name"] or p["name"] == "Black Orc +Guard+Block" \
        or p["name"] == "Treeman +Guard"


class Board:
    def __init__(self, log, us_side):
        self.us, self.them = [], []
        for side in ("home", "away"):
            for p in log[side + "_players"]:
                if p["state"] == OUT:
                    continue
                q = dict(p)
                (self.us if (side == us_side) else self.them).append(q)
        self.occ = {(p["x"], p["y"]) for p in self.us + self.them}
        self.us_st = [p for p in self.us if p["state"] == STANDING]
        self.th_st = [p for p in self.them if p["state"] == STANDING]
        self.us_tz = collections.Counter()
        for p in self.us_st:
            for s in adj(p["x"], p["y"]):
                self.us_tz[s] += 1
        self.th_tz = collections.Counter()
        for p in self.th_st:
            for s in adj(p["x"], p["y"]):
                self.th_tz[s] += 1
        c = [p for p in self.us if p.get("has_ball")]
        self.carrier = c[0] if c else None

    def neighbors_of(self, p, group):
        return [q for q in group
                if max(abs(q["x"] - p["x"]), abs(q["y"] - p["y"])) == 1]

    def assists(self, att, deff, att_team_st, def_team_st):
        """(útočné, obranné) asistence pro blok att->deff, obě strany s Guard."""
        a = 0
        for r in self.neighbors_of(deff, att_team_st):
            if r is att:
                continue
            if guard(r) or not any(e is not deff for e in self.neighbors_of(r, def_team_st)):
                a += 1
        d = 0
        for s in self.neighbors_of(att, def_team_st):
            if s is deff:
                continue
            if guard(s) or not any(e is not att for e in self.neighbors_of(s, att_team_st)):
                d += 1
        return a, d

    def dice(self, att, deff, att_team_st, def_team_st):
        """>0 = kostky útočníka; -2/-3 = kostky proti němu (vybírá obránce)."""
        a, d = self.assists(att, deff, att_team_st, def_team_st)
        A, D = att["st"] + a, deff["st"] + d
        if A > 2 * D:
            return 3
        if A > D:
            return 2
        if A == D:
            return 1
        return -3 if D > 2 * A else -2

    def bfs(self, start, targets, limit, dodge_free=False):
        """Min. počet polí ze start do některého CÍLOVÉHO pole (volného).
        dodge_free: žádný krok nesmí opouštět pole v naší (us) TZ."""
        if start in targets:
            return 0
        seen = {start}
        frontier = [start]
        d = 0
        while frontier and d < limit:
            d += 1
            nxt = []
            for (x, y) in frontier:
                if dodge_free and self.us_tz[(x, y)] > 0:
                    continue  # odchod z TZ = dodge
                for s in adj(x, y):
                    if s in seen or s in self.occ:
                        continue
                    seen.add(s)
                    if s in targets:
                        return d
                    nxt.append(s)
            frontier = nxt
        return None


def predictors(b):
    P = {}
    fb = fb2 = 0
    for p in b.th_st:
        tg = b.neighbors_of(p, b.us_st)
        if not tg:
            continue
        fb += 1
        if max(b.dice(p, q, b.th_st, b.us_st) for q in tg) >= 2:
            fb2 += 1
    P["FB"], P["FB2"] = fb, fb2
    P["MARKED"] = sum(1 for p in b.us_st if b.neighbors_of(p, b.th_st))
    P["SURF"] = sum(1 for p in b.us_st
                    if (p["y"] <= 1 or p["y"] >= 13) and b.neighbors_of(p, b.th_st))
    c = b.carrier
    if c is None or c["state"] != STANDING:
        return P
    cx, cy = c["x"], c["y"]
    P["ESC"] = sum(1 for s in adj(cx, cy)
                   if s not in b.occ and b.th_tz[s] == 0)
    ring = {s for s in adj(cx, cy) if s not in b.occ}
    reach = reach0 = 0
    best = -9
    already_adj = b.neighbors_of(c, b.th_st)
    for p in b.th_st:
        start = (p["x"], p["y"])
        lim = p["ma"] + 2
        if p in already_adj:
            d0, dn = 0, 0
        else:
            dn = b.bfs(start, ring, lim)
            d0 = b.bfs(start, ring, lim, dodge_free=True) if dn is not None else None
        if dn is None:
            continue
        reach += 1
        if d0 is not None:
            reach0 += 1
        # kostky blitzu: útočné asistence = jejich hráči UŽ stojící u nosiče
        a = sum(1 for r in already_adj if r is not p and
                (guard(r) or not any(e is not c for e in b.neighbors_of(r, b.us_st))))
        A, D = p["st"] + a, c["st"]
        dd = 3 if A > 2 * D else 2 if A > D else 1 if A == D else -2
        best = max(best, dd)
    P["REACH"], P["REACH0"] = reach, reach0
    P["BLZ"] = best if best > -9 else None  # None = nikdo nedosáhne
    good = 0
    for dx in (-1, 1):
        for dy in (-1, 1):
            s = (cx + dx, cy + dy)
            occ_by = [q for q in b.us_st if (q["x"], q["y"]) == s]
            if occ_by and b.th_tz[s] == 0:
                good += 1
    P["CCBAD"] = 4 - good
    return P


def outcomes(bn, bm):
    """bn = konec našeho kola, bm = začátek našeho dalšího kola."""
    O = {}
    m_by_id = {p["id"]: p for p in bm.us + bm.them}
    down = lost = 0
    for p in bn.us:
        q = m_by_id.get(p["id"])
        st_m = q["state"] if q else OUT
        if p["state"] == STANDING and st_m != STANDING:
            down += 1
        if st_m == OUT:
            lost += 1
    O["down"], O["lost"] = down, lost
    held_n = bn.carrier is not None
    held_m = bm.carrier is not None
    O["ball_lost"] = (1 if held_n and not held_m else 0) if held_n else None
    if held_n and held_m:
        def mind(b):
            c = b.carrier
            return min((max(abs(p["x"] - c["x"]), abs(p["y"] - c["y"]))
                        for p in b.th_st), default=99)
        O["appr"] = mind(bm) - mind(bn)
    else:
        O["appr"] = None
    return O


def pearson(xs, ys):
    n = len(xs)
    if n < 3:
        return None, n
    mx, my = sum(xs) / n, sum(ys) / n
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx == 0 or sy == 0:
        return None, n
    r = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / (sx * sy)
    return r, n


def main():
    rows = []
    excl = collections.Counter()
    for f in sorted(glob.glob(f"{DATA}/g*.json.gz")):
        r = json.load(gzip.open(f, "rt"))
        us_side = "home" if r["home_race"] == "dwarf" else "away"
        race = r["away_race"] if us_side == "home" else r["home_race"]
        logs = r["turn_logs"]
        for i, t in enumerate(logs):
            if t["active_team"] != us_side:
                continue
            if t["touchdown"]:
                excl["nase_kolo_TD"] += 1
                continue
            if i + 1 >= len(logs) or logs[i + 1]["half"] != t["half"]:
                excl["konec_pulky_po_nas"] += 1
                continue
            n = logs[i + 1]
            if n["active_team"] == us_side:
                excl["nealternuje"] += 1
                continue
            if n["touchdown"]:
                excl["souperovo_kolo_TD"] += 1
                continue
            if i + 2 >= len(logs) or logs[i + 2]["half"] != n["half"]:
                excl["konec_pulky_po_nich"] += 1
                continue
            bn = Board(n, us_side)
            bm = Board(logs[i + 2], us_side)
            row = {"race": race, "game": f, "turn": t["turn"], "half": t["half"]}
            row.update(predictors(bn))
            row.update(outcomes(bn, bm))
            rows.append(row)
    print(f"korpus {DATA}: {len(rows)} vzorků (naše kolo s platným soupeřovým kolem po něm)")
    print("vyloučeno:", dict(excl))
    races = sorted({r['race'] for r in rows})

    preds = ["FB", "FB2", "MARKED", "SURF", "ESC", "REACH", "REACH0", "BLZ", "CCBAD"]
    outs = ["down", "lost", "ball_lost", "appr"]

    print("\n== PRŮMĚRY PREDIKTORŮ (vše | podle rasy soupeře) ==")
    print(f"{'':8}{'vše':>7}" + "".join(f"{rc:>10}" for rc in races))
    for p in preds:
        vals = [r[p] for r in rows if r.get(p) is not None]
        line = f"{p:<8}{sum(vals)/len(vals):>7.2f}"
        for rc in races:
            v = [r[p] for r in rows if r["race"] == rc and r.get(p) is not None]
            line += f"{sum(v)/len(v):>10.2f}" if v else f"{'—':>10}"
        print(line)
    print("\nprůměry výsledků:")
    for o in outs:
        vals = [r[o] for r in rows if r.get(o) is not None]
        line = f"{o:<10} n={len(vals):<5} {sum(vals)/len(vals):>7.3f}"
        for rc in races:
            v = [r[o] for r in rows if r["race"] == rc and r.get(o) is not None]
            line += f"  {rc}={sum(v)/len(v):.3f}" if v else ""
        print(line)

    print("\n== KORELACE prediktor x výsledek (Pearson r, hvězdička = |r| > 2/sqrt(n)) ==")
    print(f"{'':8}" + "".join(f"{o:>12}" for o in outs))
    for p in preds:
        line = f"{p:<8}"
        for o in outs:
            xy = [(r[p], r[o]) for r in rows
                  if r.get(p) is not None and r.get(o) is not None]
            r_, n = pearson([a for a, _ in xy], [b for _, b in xy])
            if r_ is None:
                line += f"{'—':>12}"
            else:
                sig = "*" if abs(r_) > 2 / math.sqrt(n) else " "
                line += f"{r_:>10.3f}{sig} "
        print(line)

    print("\n== KORELACE UVNITŘ RASY (down; ball_lost) — kontrola, že to není jen rozdíl ras ==")
    for p in preds:
        line = f"{p:<8}"
        for rc in races:
            xy = [(r[p], r["down"]) for r in rows
                  if r["race"] == rc and r.get(p) is not None]
            r_, n = pearson([a for a, _ in xy], [b for _, b in xy])
            xy2 = [(r[p], r["ball_lost"]) for r in rows
                   if r["race"] == rc and r.get(p) is not None and r.get("ball_lost") is not None]
            r2, n2 = pearson([a for a, _ in xy2], [b for _, b in xy2])
            f1 = f"{r_:.2f}" if r_ is not None else "—"
            f2 = f"{r2:.2f}" if r2 is not None else "—"
            line += f"  {rc}:{f1}/{f2}"
        print(line)

    def bucket_table(p, cuts, outs_sel=("down", "lost", "ball_lost")):
        print(f"\n-- {p} po koších --")
        labels = []
        lo = None
        edges = []
        for c in cuts:
            edges.append(c)
        def lab(k):
            a = 0 if k == 0 else cuts[k - 1] + 1
            b = cuts[k] if k < len(cuts) else None
            return f"{a}-{b}" if b is not None else f"{a}+"
        def kof(v):
            for k, c in enumerate(cuts):
                if v <= c:
                    return k
            return len(cuts)
        by = collections.defaultdict(list)
        for r in rows:
            if r.get(p) is None:
                continue
            by[kof(r[p])].append(r)
        print(f"{'koš':<7}{'n':>5}" + "".join(f"{o:>11}" for o in outs_sel))
        for k in sorted(by):
            grp = by[k]
            line = f"{lab(k):<7}{len(grp):>5}"
            for o in outs_sel:
                v = [g[o] for g in grp if g.get(o) is not None]
                line += f"{sum(v)/len(v):>11.3f}" if v else f"{'—':>11}"
            print(line)

    bucket_table("FB", [0, 2, 4, 6])
    bucket_table("FB2", [0, 1, 2, 3])
    bucket_table("MARKED", [1, 3, 5, 7])
    bucket_table("REACH", [0, 1, 3])
    bucket_table("REACH0", [0, 1, 3])
    bucket_table("ESC", [0, 2, 4])
    bucket_table("CCBAD", [1, 2, 3])
    bucket_table("SURF", [0, 1])
    # BLZ je kategorie
    print("\n-- BLZ (nejlepší kostky blitzu na nosiče; None = nedosáhne) --")
    by = collections.defaultdict(list)
    for r in rows:
        if "BLZ" in r and r.get("REACH") is not None:
            by[r["BLZ"]].append(r)
    print(f"{'kostky':<8}{'n':>5}{'down':>9}{'ball_lost':>11}")
    for k in sorted(by, key=lambda x: (-99 if x is None else x)):
        grp = by[k]
        bl = [g["ball_lost"] for g in grp if g.get("ball_lost") is not None]
        dn = [g["down"] for g in grp]
        print(f"{str(k):<8}{len(grp):>5}{sum(dn)/len(dn):>9.3f}"
              + (f"{sum(bl)/len(bl):>11.3f}" if bl else f"{'—':>11}"))

    # nezávislost REACH0 vs BLZ pro ball_lost
    print("\n-- ball_lost podle REACH0 x BLZ --")
    tab2 = collections.defaultdict(list)
    for r in rows:
        if r.get("REACH0") is None or r.get("ball_lost") is None:
            continue
        k0 = 0 if r["REACH0"] == 0 else 1 if r["REACH0"] <= 2 else 2
        kb = "≤1d/—" if (r["BLZ"] is None or r["BLZ"] <= 1) else f"{r['BLZ']}d"
        tab2[(k0, kb)].append(r["ball_lost"])
    print(f"{'':10}{'BLZ ≤1d/—':>12}{'BLZ 2d':>10}{'BLZ 3d':>10}")
    for k0, lab in ((0, "R0 = 0"), (1, "R0 1-2"), (2, "R0 3+")):
        line = f"{lab:<10}"
        for kb in ("≤1d/—", "2d", "3d"):
            v = tab2.get((k0, kb))
            line += f"{sum(v)/len(v):>7.2f}({len(v):>3})" if v else f"{'—':>10}"
        print(line)

    # kontrola nezávislého přínosu FB2 nad MARKED: parciální pohled
    print("\n-- down podle MARKED x FB2 (řádky MARKED koš, sloupce FB2 koš) --")
    def k_m(v): return 0 if v <= 2 else 1 if v <= 5 else 2
    def k_f(v): return 0 if v <= 1 else 1 if v <= 3 else 2
    tab = collections.defaultdict(list)
    for r in rows:
        tab[(k_m(r["MARKED"]), k_f(r["FB2"]))].append(r["down"])
    print(f"{'':10}{'FB2 0-1':>10}{'FB2 2-3':>10}{'FB2 4+':>10}")
    for km, lab in ((0, "MRK 0-2"), (1, "MRK 3-5"), (2, "MRK 6+")):
        line = f"{lab:<10}"
        for kf in (0, 1, 2):
            v = tab.get((km, kf))
            line += f"{sum(v)/len(v):>7.2f}({len(v):>3})" if v else f"{'—':>10}"
        print(line)


if __name__ == "__main__":
    main()
