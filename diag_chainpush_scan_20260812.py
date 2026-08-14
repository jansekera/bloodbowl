#!/usr/bin/env python3
"""Chain-push scan (12.08.2026) — kolik DOSTAVITELNÝCH vzorů leží v korpusu.

Otázka: kolik dvojic "dostav vzor bez hodu -> chain push" je v trpasličím
korpusu nevyužitých, a co by přinesly.  Hledá se vzor O 1-2 TĚLA NEÚPLNÝ:
náš B stojí vedle stojícího soupeře E; odsunová pole E (getPushbackSquares,
helpers.cpp:194) se DOSTAVÍ volnými hráči (cesta BEZ DODGE — žádné opuštění
pole v soupeřově TZ — a BEZ GFI, tj. kroky <= MA), pak B blokne a řetěz
(block_handler.cpp: pushOne po fea042c) posune NAŠE tělo do prázdného pole
ZADARMO — bez hodu.

Dvě výplaty, měřené zvlášť, normalizované na NAŠE kolo (active_team=home):
  TEMPO — součet dx NAŠICH odsunutých těl (dx>0 = k endzone x=25)
  ÚNIK  — náš hráč stojící v >=1 TZ skončí odsunem mimo TZ BEZ HODU;
          první článek řetězu zůstává VŽDY vedle E (E je odsunut do jeho
          starého pole), plný únik prvního článku tedy chce E na zemi
          (POW / DS s Tackle) — váženo pravděpodobností; články 2+ unikají
          čistě (končí >=2 pole od E).

Pravidla zrcadlí engine (čteno jako specifikace):
  * odsun musí do PRÁZDNÉHO pole, je-li jaké (choosePushSquare) — řetěz jen
    když jsou všechna kandidátní pole obsazená; proto se dostavuje
  * směry odsunu = přímo + 45° CW/CCW od směru pusher->pushed
  * sekundární odsuny volí tahající kouč (my); Side Step/Stand Firm/Grab
    v matchupu dwarf-vs-skaven nikdo relevantní nemá
  * asistence pro kostky jako countAssists (Guard, nebo mimo cizí TZ)
  * ležící/omráčený TZ nemá, ale POLE OBSAZUJE a řetězem se sune

Korpus: diag_replay_mine_20260811{,b,c}_data (home=dwarf, away=skaven,
snapshot = stav NA ZAČÁTKU kola => všichni naši jsou "volní").
"""
import gzip, json, glob, os, sys
from collections import deque, Counter

BASE = os.path.dirname(os.path.abspath(__file__))
CORPORA = ["diag_replay_mine_20260811_data",
           "diag_replay_mine_20260811b_data",
           "diag_replay_mine_20260811c_data"]
W, H = 26, 15
DIRS8 = [(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1)]
STANDING = 0

# Dovednosti nejsou v replayi — mapa podle roster.cpp (všechny 1200 rostery:
# dwarf, skaven, orc, human, wood-elf; názvy anglicky).  Jména se přes rasy
# nekolidují v žádné dovednosti relevantní pro tenhle scan
# (Guard/Tackle/Dodge/Block/Frenzy/SideStep/StandFirm).
SKILLS = {
    "Longbeard":                  {"Block","Tackle","ThickSkull"},
    "Longbeard +Guard":           {"Block","Tackle","ThickSkull","Guard"},
    "Blitzer +Guard+Tackle":      {"Block","ThickSkull","Guard","Tackle"},
    "Troll Slayer +Guard+Tackle": {"Block","Frenzy","ThickSkull","Dauntless","Guard","Tackle"},
    "Runner +Block":              {"SureHands","ThickSkull","Block"},
    "Lineman":                    set(),
    "Gutter Runner +Sure Feet":   {"Dodge","SureFeet"},
    "Blitzer +Guard":             {"Block","Guard"},
    "Blitzer +Mighty Blow":       {"Block","MightyBlow"},
    "Blitzer ball-hunter":        {"Block","StripBall","Tackle"},
    "Thrower +Block":             {"SureHands","Pass","Block"},
    "Lineman +Wrestle":           {"Wrestle"},
    "Black Orc +Guard+Block":     {"Guard","Block"},
    "Ogre +Block":                {"Loner","BoneHead","MightyBlow","ThickSkull","Block"},
    "Catcher +Block":             {"Catch","Dodge","Block"},
    "Wardancer ball-hunter":      {"Block","Dodge","Leap","StripBall"},
    "Wardancer +Side Step":       {"Block","Dodge","Leap","SideStep"},
    "Treeman +Guard":             {"Loner","TakeRoot","StandFirm","MightyBlow","ThickSkull","Guard"},
}

def on_pitch(p): return 0 <= p[0] < W and 0 <= p[1] < H
def add(p, d):   return (p[0]+d[0], p[1]+d[1])
def cheb(a, b):  return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

def push_squares(pusher, pushed):
    """Zrcadlí getPushbackSquares: přímo, +45°, -45°; jen pole na hřišti."""
    dx = (pushed[0] > pusher[0]) - (pushed[0] < pusher[0])
    dy = (pushed[1] > pusher[1]) - (pushed[1] < pusher[1])
    i = DIRS8.index((dx, dy))
    out = []
    for j in (i, (i+1) % 8, (i+7) % 8):
        q = add(pushed, DIRS8[j])
        if on_pitch(q):
            out.append(q)
    return out


class P:
    __slots__ = ("key","side","pos","state","name","sk","ma","st","ball")
    def __init__(self, side, d):
        self.key = (side, d["id"]); self.side = side
        self.pos = (d["x"], d["y"]); self.state = d["state"]
        self.name = d["name"]; self.sk = SKILLS[d["name"]]
        self.ma = d["ma"]; self.st = d["st"]; self.ball = d["has_ball"]


def build(t, us_home):
    """Strana "H" = MY (trpaslík), ať hostí nebo hostuje."""
    pl = {}
    pairs = (("H", t["home_players"]), ("A", t["away_players"])) if us_home \
        else (("H", t["away_players"]), ("A", t["home_players"]))
    for side, arr in pairs:
        for d in arr:
            if d["state"] == 3:
                continue
            p = P(side, d); pl[p.key] = p
    occ = {p.pos: p.key for p in pl.values()}
    tz = Counter()
    for p in pl.values():
        if p.side == "A" and p.state == STANDING:
            for d in DIRS8:
                tz[add(p.pos, d)] += 1
    carrier = next((p for p in pl.values() if p.side == "H" and p.ball), None)
    return pl, occ, tz, carrier


def reach_targets(p, occ, tz):
    """Pole dosažitelná BEZ DODGE a BEZ GFI: opustit lze jen pole s tz==0;
    vstup do TZ je zdarma, ale jen jako POSLEDNÍ krok; kroky <= MA."""
    if tz[p.pos] > 0:
        return {}
    dist = {p.pos: 0}; dq = deque([p.pos])
    while dq:
        c = dq.popleft()
        if dist[c] == p.ma:
            continue
        for d in DIRS8:
            n = add(c, d)
            if not on_pitch(n) or n in occ or n in dist or tz[n] > 0:
                continue
            dist[n] = dist[c] + 1; dq.append(n)
    targ = dict(dist)
    for c, dc in dist.items():
        if dc < p.ma:
            for d in DIRS8:
                n = add(c, d)
                if on_pitch(n) and n not in occ and n not in targ:
                    targ[n] = dc + 1        # vstup do TZ = poslední krok
    targ.pop(p.pos, None)
    return targ


def assists(pl, occ, target_key, side, exclude, tz_exclude):
    """countAssists: sousedé target hráče na straně side, mimo exclude,
    stojící, s Guard nebo mimo TZ soupeřů (kromě tz_exclude)."""
    tpos = pl[target_key].pos; n = 0
    for d in DIRS8:
        k = occ.get(add(tpos, d))
        if not k or k in exclude:
            continue
        a = pl[k]
        if a.side != side or a.state != STANDING:
            continue
        if "Guard" in a.sk:
            n += 1; continue
        marked = False
        for dd in DIRS8:
            ek = occ.get(add(a.pos, dd))
            if ek and ek != tz_exclude and pl[ek].side != side and pl[ek].state == STANDING:
                marked = True; break
        if not marked:
            n += 1
    return n


def dice_probs(pl, occ, bk, ek):
    """(P_push, P_downpush, ndice, ours). Dauntless ignorován (vzácný uphill)."""
    B, E = pl[bk], pl[ek]
    sa = B.st + assists(pl, occ, ek, "H", {bk, ek}, ek)
    sd = E.st + assists(pl, occ, bk, "A", {bk, ek}, bk)
    if sa > 2*sd:   n, ours = 3, True
    elif sa > sd:   n, ours = 2, True
    elif sa == sd:  n, ours = 1, True
    elif sd > 2*sa: n, ours = 3, False
    else:           n, ours = 2, False
    p_push_die = 4/6                              # push, DS, POW
    p_dp_die = 2/6 if ("Tackle" in B.sk or "Dodge" not in E.sk) else 1/6
    if ours:
        return 1-(1-p_push_die)**n, 1-(1-p_dp_die)**n, n, ours
    return p_push_die**n, p_dp_die**n, n, ours


MAXDEPTH = 5

def chain_dfs(pl, occ, pusher_pos, pushed_key, pushed_pos, budget, fillers, reach,
              cand_hist, depth):
    """Vrací outcomes: {'moves':[(key,frm,to)...], 'fills':[(fkey,frm,to)...],
    'surf':key|None}.  moves[0] je vždy E.  Náš hráč NESMÍ ven z hřiště.
    pushed_pos se předává zvlášť — filler v řetězu už nestojí na pl[key].pos."""
    p = pl[pushed_key]
    cands = push_squares(pusher_pos, pushed_pos)
    if not cands:                                   # vytlačen z hřiště
        if p.side == "H":
            return []
        return [{"moves": [], "fills": [], "surf": pushed_key}]
    empties = [c for c in cands if c not in occ]
    out = []
    if depth > 0 and empties:                       # leaf: odsun do prázdna
        for e in empties:
            out.append({"moves": [(pushed_key, pushed_pos, e)], "fills": [], "surf": None})
    if depth >= MAXDEPTH:
        return out
    # extend: dostavět všechna prázdná kandidátní pole a řetězit dál
    if len(empties) <= budget:
        blocked = set(cand_hist) | set(cands)
        cand_fillers = []                           # na každé need pole seznam
        for sq in empties:
            fs = [fk for fk in fillers
                  if pl[fk].pos not in blocked and sq in reach.get(fk, {})]
            fs.sort(key=lambda fk: reach[fk][sq])
            cand_fillers.append(fs[:4])
        # všechny kombinace navzájem různých fillerů
        def gen(i, used):
            if i == len(empties):
                yield []
                return
            for fk in cand_fillers[i]:
                if fk in used:
                    continue
                for rest in gen(i+1, used | {fk}):
                    yield [(fk, pl[fk].pos, empties[i])] + rest
        for fills in gen(0, set()):
            occ2 = occ
            if fills:
                occ2 = dict(occ)
                for fk, frm, to in fills:
                    del occ2[frm]; occ2[to] = fk
            f2 = fillers - {f[0] for f in fills}
            for c in cands:
                k2 = occ2.get(c)
                if k2 is None:
                    continue
                # Soupeř se SideStep/StandFirm v řetězu: SideStep si vybere
                # jiné pole, StandFirm dle CRP odmítne odsun — konzervativně
                # přes ně neřetězíme (engine po fea042c StandFirm v řetězu
                # nekontroluje, ale nechceme na to stavět).
                if pl[k2].side == "A" and ({"SideStep","StandFirm"} & pl[k2].sk):
                    continue
                subs = chain_dfs(pl, occ2, pushed_pos, k2, c, budget - len(fills),
                                 f2, reach, blocked, depth + 1)
                for s in subs:
                    out.append({"moves": [(pushed_key, pushed_pos, c)] + s["moves"],
                                "fills": fills + s["fills"], "surf": s["surf"]})
    return out


def mover_class(pl, key, carrier):
    p = pl[key]
    if p.state != STANDING:
        return "prone"
    if carrier and key == carrier.key:
        return "carrier"
    if carrier:
        d = (abs(p.pos[0]-carrier.pos[0]), abs(p.pos[1]-carrier.pos[1]))
        if d == (1, 1):
            return "corner"
        if max(d) == 1:
            return "escort"
    return "other"

CLASS_RANK = {"carrier": 0, "corner": 1, "escort": 2, "other": 3, "prone": 4}


def eval_outcome(pl, occ, tz, carrier, bk, ek, o, dxdir):
    E = pl[ek]
    our = [(k, f, t) for (k, f, t) in o["moves"] if pl[k].side == "H"]
    tempo = sum(dxdir * (t[0]-f[0]) for _, f, t in our)
    # kostky: asistence PO level-0 dostavbě (fillery vedle E asistují)
    occ0 = occ
    fills0 = [f for f in o["fills"] if cheb(f[2], E.pos) == 1]
    if fills0:
        occ0 = dict(occ)
        for fk, frm, to in fills0:
            del occ0[frm]; occ0[to] = fk
    p_push, p_dp, nd, ours_dice = dice_probs(pl, occ0, bk, ek)
    # úniky: TZ bez E (E se hýbe); články 2+ končí >=2 od E_new => čistý únik
    tz_noE = tz
    if E.state == STANDING:
        tz_noE = Counter(tz)
        for d in DIRS8:
            tz_noE[add(E.pos, d)] -= 1
    escapes = []
    filler_keys = {f[0] for f in o["fills"]}
    for i, (k, f, t) in enumerate(o["moves"]):
        if pl[k].side != "H" or pl[k].state != STANDING or k in filler_keys:
            continue                      # filler do TZ vstoupil až tento tah
        if tz[f] >= 1 and tz_noE[t] == 0:
            first = (i == 1)                       # hned za E: E zůstane vedle
            escapes.append({"key": k, "cls": mover_class(pl, k, carrier),
                            "needs_E_down": first,
                            "p": p_dp if first else p_push,
                            "tz_before": tz[f]})
    cage_break = any(carrier and cheb(frm, carrier.pos) <= 1
                     for _, frm, _ in o["fills"])
    benef_is_filler = bool(our) and our[-1][0] in filler_keys
    movers = [mover_class(pl, k, carrier) for k, _, _ in our]
    best_cls = min(movers, key=lambda c: CLASS_RANK[c]) if movers else None
    net_dx = tempo + sum(dxdir * (t[0]-f[0]) for _, f, t in o["fills"])
    return {"tempo": tempo, "net_dx": net_dx, "benef_is_filler": benef_is_filler,
            "ev_tempo": tempo * p_push, "p_push": p_push,
            "p_dp": p_dp, "ndice": nd, "ours_dice": ours_dice,
            "links": len(our), "fillers": len(o["fills"]),
            "cage_break": cage_break, "movers": movers, "best_cls": best_cls,
            "escapes": escapes, "frenzy": "Frenzy" in pl[bk].sk,
            "push_dx": dxdir * (o["moves"][0][2][0] - o["moves"][0][1][0]) if o["moves"] else 0,
            "surf": o["surf"]}


def analyze_turn(t, us_home):
    dxdir = 1 if us_home else -1          # trpaslík doma útočí na x=25, jinak na x=0
    pl, occ, tz, carrier = build(t, us_home)
    ours = [p for p in pl.values() if p.side == "H" and p.state == STANDING]
    enemies = [p for p in pl.values() if p.side == "A" and p.state == STANDING
               and not ({"SideStep","StandFirm"} & p.sk)]
    filler_pool = {p.key for p in ours if not p.ball}
    reach = {p.key: reach_targets(p, occ, tz) for p in ours if not p.ball}

    pair_best_tempo, pair_best_carrier_esc, pair_best_any_esc = [], [], []
    for E in enemies:
        for B in ours:
            if cheb(B.pos, E.pos) != 1:
                continue
            outs = chain_dfs(pl, occ, B.pos, E.key, E.pos, 2,
                             filler_pool - {B.key}, reach, [], 0)
            bt = bc = be = None
            for o in outs:
                ev = eval_outcome(pl, occ, tz, carrier, B.key, E.key, o, dxdir)
                if ev["tempo"] > 0 and (bt is None or ev["ev_tempo"] > bt["ev_tempo"]):
                    bt = ev
                for e in ev["escapes"]:
                    if e["cls"] == "carrier" and (bc is None or e["p"] > bc["esc"]["p"]):
                        bc = {**ev, "esc": e}
                    if be is None or e["p"] > be.get("esc", {}).get("p", 0):
                        be = {**ev, "esc": e}
            if bt: pair_best_tempo.append(bt)
            if bc: pair_best_carrier_esc.append(bc)
            if be: pair_best_any_esc.append(be)
    return {"pl": pl, "carrier": carrier,
            "tempo_pairs": pair_best_tempo,
            "carrier_esc_pairs": pair_best_carrier_esc,
            "any_esc_pairs": pair_best_any_esc,
            "carrier_marked": bool(carrier and tz[carrier.pos] >= 1
                                   and carrier.state == STANDING)}


def main():
    files = []
    for c in CORPORA:
        files += sorted(glob.glob(os.path.join(BASE, c, "*.json.gz")))
    if len(sys.argv) > 1:
        files = files[:int(sys.argv[1])]

    n_turns = n_games = 0
    n_turns_with_tempo = 0
    tot_pairs_tempo = tot_pairs_cesc = tot_pairs_aesc = 0
    sum_best_raw = sum_best_ev = sum_best_net = 0.0
    best_links = Counter(); best_fill = Counter(); best_cls = Counter()
    best_dx = Counter(); cage_break_n = 0; frenzy_n = 0
    n_turns_carrier_marked = 0
    n_turns_carrier_esc = 0
    sum_carrier_esc_p = 0.0
    carrier_esc_needsdown = Counter()
    n_turns_any_esc = 0
    sum_any_esc_p = 0.0
    fill_dist_pairs = Counter()
    benef_filler_n = 0
    by_race = {}                          # opp race -> [turns, sum_ev, sum_raw, cesc_geo, cesc_p]

    for f in files:
        r = json.load(gzip.open(f, "rt"))
        us_home = r["home_race"] == "dwarf"
        if not us_home and r["away_race"] != "dwarf":
            continue                       # hra bez trpaslíka (nemělo by nastat)
        n_games += 1
        opp = r["away_race"] if us_home else r["home_race"]
        rr = by_race.setdefault(opp, [0, 0.0, 0.0, 0, 0.0])
        for t in r["turn_logs"]:
            if t["active_team"] != ("home" if us_home else "away"):
                continue
            n_turns += 1
            rr[0] += 1
            a = analyze_turn(t, us_home)
            tot_pairs_tempo += len(a["tempo_pairs"])
            tot_pairs_cesc += len(a["carrier_esc_pairs"])
            tot_pairs_aesc += len(a["any_esc_pairs"])
            for ev in a["tempo_pairs"]:
                fill_dist_pairs[ev["fillers"]] += 1
            if a["tempo_pairs"]:
                n_turns_with_tempo += 1
                b = max(a["tempo_pairs"], key=lambda e: e["ev_tempo"])
                sum_best_raw += b["tempo"]; sum_best_ev += b["ev_tempo"]
                sum_best_net += b["net_dx"]
                rr[1] += b["ev_tempo"]; rr[2] += b["tempo"]
                benef_filler_n += b["benef_is_filler"]
                best_links[min(b["links"], 3)] += 1
                best_fill[b["fillers"]] += 1
                best_cls[b["best_cls"]] += 1
                best_dx[b["push_dx"]] += 1
                cage_break_n += b["cage_break"]
                frenzy_n += b["frenzy"]
            if a["carrier_marked"]:
                n_turns_carrier_marked += 1
            if a["carrier_esc_pairs"]:
                n_turns_carrier_esc += 1
                b = max(a["carrier_esc_pairs"], key=lambda e: e["esc"]["p"])
                sum_carrier_esc_p += b["esc"]["p"]
                carrier_esc_needsdown[b["esc"]["needs_E_down"]] += 1
                rr[3] += 1; rr[4] += b["esc"]["p"]
            if a["any_esc_pairs"]:
                n_turns_any_esc += 1
                b = max(a["any_esc_pairs"], key=lambda e: e["esc"]["p"])
                sum_any_esc_p += b["esc"]["p"]

    print(f"hry: {n_games}, naše kola: {n_turns}")
    print(f"\n== NEVYUŽITÉ DVOJICE (feasible vzor, <=2 dostavovaná těla) ==")
    print(f"  s kladným TEMPEM:   {tot_pairs_tempo}  ({tot_pairs_tempo/n_turns:.2f}/kolo)")
    print(f"    rozpad podle dostavovaných těl (všechny páry): {dict(fill_dist_pairs)}")
    print(f"  s únikem KOHOKOLI:  {tot_pairs_aesc}  ({tot_pairs_aesc/n_turns:.2f}/kolo)")
    print(f"  s únikem NOSIČE:    {tot_pairs_cesc}  ({tot_pairs_cesc/n_turns:.3f}/kolo)")
    print(f"\n== TEMPO (best-per-turn, jen 1 využitá příležitost za kolo) ==")
    print(f"  kol s >=1 příležitostí: {n_turns_with_tempo} ({100*n_turns_with_tempo/n_turns:.1f} %)")
    print(f"  RAW pole vpřed/kolo:    {sum_best_raw/n_turns:.3f}")
    print(f"  EV (x P_push)/kolo:     {sum_best_ev/n_turns:.3f}")
    print(f"  NET vč. chůze fillerů:  {sum_best_net/n_turns:.3f}/kolo "
          f"(záporné = stroj tahá vlastní těla dozadu)")
    print(f"  články (naše těla) 1/2/3+: {dict(best_links)}")
    print(f"  dostavovaná těla 0/1/2:    {dict(best_fill)}")
    print(f"  kdo se hýbe (nejcennější): {dict(best_cls)}")
    print(f"  dx prvního odsunu (E):     {dict(best_dx)}")
    print(f"  vyžaduje rozbít klec:      {cage_break_n} z {n_turns_with_tempo}")
    print(f"  bloker s Frenzy:           {frenzy_n} z {n_turns_with_tempo}")
    print(f"  beneficientem sám filler:  {benef_filler_n} z {n_turns_with_tempo}")
    print(f"\n== PODLE SOUPEŘE (best-per-turn) ==")
    print(f"  {'soupeř':<10}{'kol':>6}{'EV tempo/kolo':>15}{'RAW/kolo':>10}"
          f"{'únik nosiče/10 kol':>20}{'vážené':>8}")
    for race, (nt, sev, sraw, cg, cp) in sorted(by_race.items()):
        print(f"  {race:<10}{nt:>6}{sev/nt:>15.3f}{sraw/nt:>10.3f}"
              f"{10*cg/nt:>20.2f}{10*cp/nt:>8.2f}")
    print(f"\n== ÚNIK ==")
    print(f"  kol, kdy nosič stojí v >=1 TZ: {n_turns_carrier_marked} "
          f"({100*n_turns_carrier_marked/n_turns:.1f} % kol)")
    print(f"  kol s únikem nosiče (geometricky): {n_turns_carrier_esc} "
          f"= {10*n_turns_carrier_esc/n_turns:.2f}/10 kol")
    print(f"  vážené P (úspěšný únik bez hodu):  {10*sum_carrier_esc_p/n_turns:.2f}/10 kol")
    print(f"  z toho 1. článek (chce E na zem): {dict(carrier_esc_needsdown)}")
    print(f"  kol s únikem kohokoli: {n_turns_any_esc} "
          f"= {10*n_turns_any_esc/n_turns:.2f}/10 kol (vážené {10*sum_any_esc_p/n_turns:.2f})")


if __name__ == "__main__":
    main()
