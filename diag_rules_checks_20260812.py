#!/usr/bin/env python3
"""Kontroly K29–K33 + K9a nad korpusem — první kus rozhodčího (T2.2).

Přenáší pravidla R1–R4 z ručního nástroje `diag_play_session_20260812.py`
do měření nad korpusem.  Konec kola se bere z `turn_logs[i+1]`, ne rekonstrukcí
z událostí (ČÁST 4.1 specifikace): kdo se nehnul, nemá událost.

Kola, po kterých následuje TD nebo poločas, se vyřazují — mezi snímky se
přestaví hřiště, takže `i+1` už není konec téhož kola.

──────────────────────────────────────────────────────────────────────────
PŘEPSÁNO 13.08. — audit měřicího aparátu našel šest míst, kde kontrola
měřila něco jiného, než čím se hlásila.  Společná příčina nebyla šest
překlepů, ale dvě systémové mezery:

  (1) Kontrola vydávala jediné procento bez opory.  Jmenovatel nebyl vidět,
      takže se nedalo poznat, že K31 dělí čitatel z kol S MÍČEM počtem VŠECH
      kol (0,86 místo 1,80 na kolo).

  (2) Predikát nad prázdnou množinou se počítal jako SPLNĚNÝ.  „Žádný roh
      klece není markovaný" je pravda i tehdy, když klec vůbec nestojí —
      a to byla třetina kol (33,4 %), které si K29 připisovala k dobru.

Proto se každá kontrola teď vydává jako povinná trojice
`Check(ok, n, deg)` — splněno / posuzováno / degenerovaných — a predikát
nad prázdnou množinou hlásí N/A, ne úspěch.  Nová kontrola, která trojici
nevyplní, se nedá vytisknout; to je záměr.

Opraveno navíc věcně:
  * K9a  `need` se počítal ze vzdálenosti PO pohybu, tedy z toho, kam jsme
         došli — shovívavé.  Bere se ze začátku kola.  A kola, ve kterých
         se během kola vyměnil nosič, jsou degenerovaná: `got` by byl
         rozdíl pozic dvou různých hráčů.
  * K29  kolo bez jediného obsazeného rohu je degenerované, ne čisté.
  * K30  R3 byl 12.08. přepsán z „AG3" na „kde dodge NĚCO STOJÍ" (elfové
         jsou celí AG4, na AG3 se proti nim nedá řadit).  Měří se cena
         dodge jako pravděpodobnost selhání, a ta závisí na dvojici:
         náš Tackle soupeři ruší Dodge reroll.  Držení, které obstarává
         výhradně roh klece nebo nosič, se nepočítá — R1 je nad R3.
  * K31  čitatel se počítal jen v kolech s míčem (byl za `continue`),
         jmenovatel bral všechna kola.
"""
import glob, gzip, json, math, sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field

# E1/E2 (ČÁST 13) se počítají netriviálně — REACH0 je BFS s dodge-free
# variantou, FB2 potřebuje kostky bloku i s Guard asistencemi na obou
# stranách.  Přepisovat to sem znamená vyrobit druhou definici téhož, a to
# je přesně ta vada, kvůli které se tenhle soubor 13.08. přepisoval.
# Importuje se tedy Fableho implementace; jediná definice, dvě použití.
from diag_exposure_scan_20260812 import Board, predictors

STANDING, PRONE, STUNNED = 0, 1, 2

# Jump Up se v replayi nevozí, je dané pozicí v rosteru. Žádný roster TV1200,
# který hrajeme, ho nemá (roster.cpp ř. 168/198/395 nejsou dwarf/skaven/
# wood-elf/human/orc), ale pravidlo se implementuje, ne jeho dnešní výsledek.
JUMP_UP_NAMES = set()

# ── skilly, které replay nevozí ───────────────────────────────────────────
# Replay veze jen jméno a statistiky, ne skilly.  Klíč musí být čtveřice:
# „Lineman" je u wood-elfa AG4 a u orka AG3, a Dodge nemá ani jeden.
# Zdroj: engine/src/roster.cpp, sekce *Roster1200 (jediné, které hrajeme).
DODGE = {  # (jméno, ag, ma, st) -> má skill Dodge
    ("Gutter Runner +Sure Feet", 4, 9, 2),   # skaven
    ("Catcher +Block", 3, 8, 2),             # human
    ("Catcher +Block", 4, 8, 2),             # wood-elf
    ("Wardancer +Side Step", 4, 8, 3),       # wood-elf
    ("Wardancer ball-hunter", 4, 8, 3),      # wood-elf
}
TACKLE = {  # náš Tackle ruší soupeři Dodge reroll (CRP)
    ("Longbeard", 2, 4, 3),
    ("Longbeard +Guard", 2, 4, 3),
    ("Blitzer +Guard+Tackle", 3, 5, 3),
    ("Troll Slayer +Guard+Tackle", 2, 5, 3),
    ("Blitzer ball-hunter", 3, 6, 3),        # human/orc: Strip Ball + Tackle
    ("Blitzer ball-hunter", 3, 7, 3),        # skaven
}
# Každé jméno v korpusu musí být v ROSTER_KNOWN, jinak skript hlásí chybu.
# Tiché přeskočení neznámého hráče je přesně ta třída vady, kvůli které se
# tenhle soubor přepisoval.
ROSTER_KNOWN = DODGE | TACKLE | {
    ("Black Orc +Guard+Block", 2, 4, 4), ("Blitzer +Guard", 3, 6, 3),
    ("Blitzer +Guard", 3, 7, 3), ("Blitzer +Mighty Blow", 3, 6, 3),
    ("Blitzer +Mighty Blow", 3, 7, 3), ("Lineman", 3, 5, 3),
    ("Lineman", 3, 6, 3), ("Lineman", 3, 7, 3), ("Lineman", 4, 7, 3),
    ("Lineman +Wrestle", 3, 7, 3), ("Ogre +Block", 2, 5, 5),
    ("Runner +Block", 3, 6, 3), ("Thrower +Block", 3, 5, 3),
    ("Thrower +Block", 3, 6, 3), ("Thrower +Block", 3, 7, 3),
    ("Thrower +Block", 4, 7, 3), ("Treeman +Guard", 1, 2, 6),
}

# Práh „dodge něco stojí".  AG3 bez Dodge = 33 % selhání, AG4 s Dodge a bez
# našeho Tackle = 2,8 %.  Hranice 20 % ty dva případy dělí a leží v mezeře
# mezi nimi, ne na hraně žádného z nich.
DODGE_COST_THRESHOLD = 0.20


def key(p):
    return (p["name"], p["ag"], p["ma"], p["st"])


def dodge_cost(target, marker):
    """P(selhání dodge) pro `target`, který odchází od `marker`.

    CRP: hod d6 na 7 − AG, modifikátor +1 za odchod na pole bez cizí TZ;
    1 padá vždy.  Skill Dodge dovolí přehodit — pokud ho marker neruší
    skillem Tackle.  Vrací horní mez ceny: cizí tackle zóny na cílovém poli
    by ji jen zvýšily.
    """
    need = max(2, min(6, 7 - target["ag"] - 1))
    p_fail = (need - 1) / 6.0
    if key(target) in DODGE and key(marker) not in TACKLE:
        p_fail *= p_fail  # reroll
    return p_fail


# ── povinná trojice ───────────────────────────────────────────────────────
@dataclass
class Check:
    """Výsledek kontroly: splněno / posuzováno / degenerovaných.

    `deg` jsou případy, ve kterých predikát nedává smysl (prázdná množina,
    chybějící nosič).  Nepočítají se do `n` a nesmějí se počítat jako
    splněné — proto tenhle typ existuje.
    """
    label: str
    unit: str = "podíl kol"
    ok: int = 0
    n: int = 0
    deg: int = 0
    vals: list = field(default_factory=list)

    def hit(self, passed):
        self.n += 1
        self.ok += 1 if passed else 0

    def skip(self):
        self.deg += 1

    def line(self):
        if self.n == 0:
            return f"  {self.label:<44}{'N/A':>8}   (0 posuzováno, {self.deg} degenerovaných)"
        s = f"  {self.label:<44}{self.ok / self.n:>7.1%}   n={self.n}"
        if self.deg:
            s += f", {self.deg} N/A ({self.deg / (self.n + self.deg):.0%})"
        if self.vals:
            s += f", průměr {sum(self.vals) / len(self.vals):+.2f}"
        return s


@dataclass
class Bucket:
    """Kontrola, která nemá podobu ano/ne — koše prediktoru proti výsledku.

    Platí pro ni totéž co pro `Check`: bez `n` u každého koše je průměr
    nečitelný, protože se nedá poznat koš o třech vzorcích od koše o třech
    stech.  Koš pod `MIN_N` se proto tiskne, ale označí.
    """
    label: str
    xname: str
    yname: str
    cuts: tuple
    data: dict = field(default_factory=lambda: defaultdict(list))
    deg: int = 0
    MIN_N: int = 30

    def add(self, x, y):
        k = next((i for i, c in enumerate(self.cuts) if x <= c), len(self.cuts))
        self.data[k].append(y)

    def skip(self):
        self.deg += 1

    def lines(self):
        out = [f"  {self.label}   ({self.xname} → {self.yname}, "
               f"{self.deg} degenerovaných)"]
        if not self.data:
            out.append("      N/A — žádný použitelný vzorek")
            return out
        names = []
        lo = None
        for c in self.cuts:
            names.append(f"≤{c}" if lo is None else f"{lo + 1}–{c}")
            lo = c
        names.append(f"{lo + 1}+")
        for k in sorted(self.data):
            v = self.data[k]
            mark = "  ⚠ málo vzorků" if len(v) < self.MIN_N else ""
            out.append(f"      {names[k]:<8} n={len(v):<6} "
                       f"{self.yname} = {sum(v) / len(v):+.2f}{mark}")
        return out


def threatens(p):
    """Ohrozí roh klece, aniž by to soupeře stálo blitz (pravidlo R1, 12.08.).

    Stojící může hodit Block; ležící s Jump Up smí Block vyhlásit vleže a vstát
    zadarmo, takže jim blitz zůstane. Nikdo další — klasický ležící i omráčený
    se k nám dostanou jedině za cenu blitzu.
    """
    return p["state"] == STANDING or (p["state"] == PRONE and p["name"] in JUMP_UP_NAMES)


def adj(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1])) == 1


def load(path):
    return json.load(gzip.open(path, "rt"))


def our_side(turn):
    return turn["active_team"]


def players(turn, side, on_pitch=True):
    out = []
    for p in turn[f"{side}_players"]:
        if on_pitch and p["state"] == 3:
            continue
        out.append(p)
    return out


def analyse(paths, race="dwarf"):
    st = Counter()
    unknown = Counter()
    C = {
        "K29": Check("K29 (R1) žádný roh klece není markovaný"),
        "K29full": Check("K29⭐ plná ČISTÁ klec (4/4)"),
        "K9a": Check("K9a splnil rozvrhovou podlahu", vals=[]),
        "K30": Check("K30 (R3) drahý dodge je držený", "podíl soupeřů"),
        "K30cheap": Check("K30b levný dodge je držený (nemá cenu)", "podíl soupeřů"),
        "K31": Check("K31 (R4) kolo BEZ těla bez úkolu"),
        "K33": Check("K33 kolo s aspoň jedním blokem"),
        # E1/E2 z ČÁSTI 13 (exposure scan, 12.08.) jako vynucované kontroly
        "K34": Check("K34 (E1) REACH0 = 0 — nikdo nedosáhne bez dodge"),
        "K35": Check("K35 (E2) FB2 ≤ 1 — bezplatné ≥2kostkové bloky"),
    }
    # K36: hypotéza z odehrané situace 12.08. — postup vykoupený kontaktem
    # zamyká vlastní těla, a zamčené tělo neumí být rohem klece příští kolo.
    B = {
        "K36": Bucket("K36 LOCKED → postup nosiče v NÁSLEDUJÍCÍM našem kole",
                      "našich stojících v jejich TZ", "Δx nosiče",
                      cuts=(2, 5, 8)),
    }
    corners_filled, corners_clean, idle_per_turn = [], [], []
    for path in paths:
        r = load(path)
        # která strana je náš rasa
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == race), None)
        if ours is None:
            continue
        # směr útoku: kam nese míč, se pozná z endzone — home útočí k x=25
        fwd = 1 if ours == "home" else -1
        endzone = 25 if fwd == 1 else 0

        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if S.get("touchdown") or E["half"] != S["half"]:
                st["vyřazeno (TD/poločas mezi snímky)"] += 1
                continue
            st["našich kol"] += 1

            us = players(E, ours)
            them = players(E, "away" if ours == "home" else "home")
            for p in us + them:
                if key(p) not in ROSTER_KNOWN:
                    unknown[key(p)] += 1

            # --- K33: kolo bez jediného bloku je varovný signál
            blocks = sum(1 for e in S["events"]
                         if e["type"] == "BLOCK" and any(p["id"] == e["player_id"] for p in us))
            C["K33"].hit(blocks > 0)

            # --- kolik kol vůbec mělo plán (X6 / brána klece)
            pl = S.get("plan") or {}
            st[f"plán: {pl.get('verdict', '?')}"] += 1

            car = next((p for p in us if p["has_ball"]), None)
            diag = [(car["x"] + dx, car["y"] + dy)
                    for dx in (-1, 1) for dy in (-1, 1)] if car else []
            occ = {(p["x"], p["y"]): p for p in us}
            filled = [d for d in diag if d in occ]
            threat = [(p["x"], p["y"]) for p in them if threatens(p)]
            dirty = [d for d in filled if any(adj(d, t) for t in threat)]

            # --- K31 (R4): tělo bez úkolu = stojí, nehnulo se, nesousedí s nikým.
            # Počítá se i v kolech bez míče: tam je těl bez úkolu nejvíc.
            moved = {e["player_id"] for e in S["events"]}
            idle = 0
            for p in us:
                if p["state"] != STANDING or p["id"] in moved:
                    continue
                if p["has_ball"] or (p["x"], p["y"]) in diag:
                    continue
                if any(adj((p["x"], p["y"]), (o["x"], o["y"])) for o in them):
                    continue
                idle += 1
            C["K31"].hit(idle == 0)
            idle_per_turn.append(idle)
            st["K31 těl bez úkolu celkem"] += idle

            # --- K30 (R3): značkovat tam, kde dodge NĚCO STOJÍ.
            # Držení obstarané výhradně rohem klece nebo nosičem se nepočítá:
            # R1 jim markovat zakazuje, takže jako splnění R3 platit nemůže.
            for e in them:
                if e["state"] != STANDING:
                    continue
                markers = [p for p in us
                           if p["state"] == STANDING and adj((p["x"], p["y"]), (e["x"], e["y"]))]
                legal = [p for p in markers
                         if not (p["has_ball"] or (p["x"], p["y"]) in filled)]
                # Cena úniku: kdo ho drží, ten určuje, jestli smí použít Dodge
                # reroll (CRP Tackle ruší Dodge při odchodu z JEHO zóny, takže
                # stačí jediný Tackle mezi markery).  Když ho nedrží nikdo,
                # ptáme se, co by markování stálo soupeře — a referencí je náš
                # typický marker, tedy hráč s Tackle: v naší jedenáctce ho má
                # deset z jedenácti (všichni krom Runnera).
                if markers:
                    ref = next((p for p in markers if key(p) in TACKLE), markers[0])
                else:
                    ref = next((p for p in us if key(p) in TACKLE), None)
                if ref is None:
                    continue
                cost = dodge_cost(e, ref)
                tgt = C["K30"] if cost >= DODGE_COST_THRESHOLD else C["K30cheap"]
                tgt.hit(bool(legal))
                if markers and not legal:
                    st["K30 drženo jen klecí/nosičem (R1 zakazuje)"] += 1

            # --- K34/K35 (E1/E2): co deska na konci našeho kola dává soupeři.
            # Board i predictors jsou Fableho, aby existovala jediná definice.
            board = Board(E, ours)
            P = predictors(board)
            C["K35"].hit(P["FB2"] <= 1)
            if "REACH0" in P:
                C["K34"].hit(P["REACH0"] == 0)
            else:
                C["K34"].skip()   # bez stojícího nosiče predikát nedává smysl

            # --- K36: LOCKED = naši stojící, kteří na konci kola stojí
            # v soupeřově tackle zóně. Nemohou se příště hnout bez hodu.
            locked = sum(1 for p in board.us_st if board.th_tz[(p["x"], p["y"])] > 0)
            st["K36 locked celkem"] += locked
            # Postup nosiče v našem PŘÍŠTÍM kole: logs[i+2] je jeho začátek,
            # logs[i+3] konec. Nosič musí být týž, jinak by to byl rozdíl
            # pozic dvou hráčů (táž past jako u K9a).
            if i + 3 >= len(logs) or logs[i + 3]["half"] != S["half"] \
                    or logs[i + 2].get("touchdown"):
                B["K36"].skip()
            else:
                n0 = next((p for p in players(logs[i + 2], ours) if p["has_ball"]), None)
                n1 = next((p for p in players(logs[i + 3], ours) if p["has_ball"]), None)
                if n0 is None or n1 is None or n0["id"] != n1["id"]:
                    B["K36"].skip()
                else:
                    B["K36"].add(locked, (n1["x"] - n0["x"]) * fwd)

            if car is None:
                st["bez míče na konci kola"] += 1
                C["K29"].skip()
                C["K29full"].skip()
                C["K9a"].skip()
                continue
            st["s míčem na konci kola"] += 1

            # --- K29 (R1) + rohy klece.  Bez jediného obsazeného rohu klec
            # neexistuje a „není markovaná" není splnění, ale prázdný predikát.
            if not filled:
                st["K29 kol bez jediného rohu (klec nestojí)"] += 1
                C["K29"].skip()
                C["K29full"].skip()
            else:
                corners_filled.append(len(filled))
                corners_clean.append(len(filled) - len(dirty))
                C["K29"].hit(not dirty)
                C["K29full"].hit(len(filled) == 4 and not dirty)

            # --- K9a: rozvrhová podlaha.  `need` ze ZAČÁTKU kola: požadavek
            # na tohle kolo se nesmí počítat z toho, kam jsme došli.
            turns_left = 9 - S["turn"]
            carS = next((p for p in players(S, ours) if p["has_ball"]), None)
            if turns_left <= 0 or carS is None:
                C["K9a"].skip()
            elif carS["id"] != car["id"]:
                # nosič se během kola vyměnil: `got` by byl rozdíl pozic
                # dvou různých hráčů, ne postup
                st["K9a nosič se během kola změnil"] += 1
                C["K9a"].skip()
            else:
                need = math.ceil(abs(endzone - carS["x"]) / turns_left)
                got = (car["x"] - carS["x"]) * fwd
                C["K9a"].vals.append(got - need)
                C["K9a"].hit(got >= need)

    return st, C, B, corners_filled, corners_clean, idle_per_turn, unknown


def main():
    pats = sys.argv[1:] or ["diag_replay_mine_20260811b_data/*.json.gz"]
    paths = sorted(p for pat in pats for p in glob.glob(pat))
    st, C, B, cf, cc, idle, unknown = analyse(paths)
    print(f"korpus: {len(paths)} her\n")
    for k in ("našich kol", "vyřazeno (TD/poločas mezi snímky)",
              "s míčem na konci kola", "bez míče na konci kola"):
        print(f"  {k:<44}{st[k]:>6}")
    kol = st["našich kol"] or 1

    if unknown:
        print("\n⚠️  NEZNÁMÍ HRÁČI (chybí v ROSTER_KNOWN — cena dodge je u nich"
              " odhad bez skillů):")
        for k, v in unknown.most_common():
            print(f"     {k}  ×{v}")

    print("\n=== KONTROLY (splněno / posuzováno / N/A) ===")
    print(C["K29"].line())
    print(C["K29full"].line())
    if cf:
        print(f"    obsazených rohů průměrně (jen kola, kdy klec stojí) "
              f"{sum(cf) / len(cf):.2f} / 4, z toho čistých {sum(cc) / len(cc):.2f}")
    print(f"    kol s míčem, kdy klec vůbec NESTOJÍ: "
          f"{st['K29 kol bez jediného rohu (klec nestojí)']}")
    print(C["K9a"].line())
    print(C["K30"].line())
    print(C["K30cheap"].line())
    print(f"    z toho drženo jen klecí/nosičem (R1 zakazuje): "
          f"{st['K30 drženo jen klecí/nosičem (R1 zakazuje)']}")
    print(C["K31"].line())
    print(f"    těl bez úkolu na kolo                      "
          f"{st['K31 těl bez úkolu celkem'] / kol:.2f} z 11")
    print(C["K33"].line())
    print("\n--- E1/E2: co deska dává soupeři (ČÁST 13) ---")
    print(C["K34"].line())
    print(C["K35"].line())
    print(f"\n--- zámky (hypotéza 12.08.) ---")
    print(f"  našich stojících v jejich TZ na kolo        "
          f"{st['K36 locked celkem'] / kol:.2f} z 11")
    for ln in B["K36"].lines():
        print(ln)

    print("\nplán enginu (X6 / brána klece)")
    for k, v in st.items():
        if k.startswith("plán:"):
            print(f"  {k:<44}{v / kol:>7.1%}")


if __name__ == "__main__":
    main()
