#!/usr/bin/env python3
"""
P42 — NECHÁVÁME NOSIČE KONČIT KOLO V KONTAKTU? A BYLO KAM JÍT? (10.09.2026)

⚑ PROČ TENHLE SKRIPT VŮBEC VZNIKL — OPRAVA MĚŘIDLA, NE NOVÝ NÁLEZ.
  `diag_cage_built_20260910.py` téhož dne napočítal „nosič sám v soupeřově TZ
  53,1 %" a to číslo přehodilo pořadí okruhu KLEC. ⛔ Je čtené ze ŠPATNÉHO
  SNÍMKU: `captureTurnSnapshot` (`game_simulator.cpp:748`, volané na hranici
  kola v `:851`/`:905`) razítkuje ZAČÁTEK kola — tedy stav PO soupeřově kole.
  ⇒ 53,1 % měří „jak často NÁM soupeř nosiče označí", což je většinou JEHO
  akce, ne naše volba. Predikát `P42`/`K38` je opačný konec kola.

⭐ A KONEC KOLA SE ČÍST DÁ, korpus na to stačí — `turn_logs[i+1]` je začátek
  soupeřova kola, tedy KONEC našeho. Není to rekonstrukce z událostí; je to
  přesně metoda, kterou `diag_rules_checks_20260812.py:382` používá pro `K38`
  už od 12.08. *(„Konec kola se bere z `turn_logs[i+1]`, ne rekonstrukcí
  z událostí — kdo se nehnul, nemá událost.")*
  ⛔ Páruje se JEN když mezi snímky nic hřiště nepřestaví: vyřazují se kola
  s TD a hranice poločasu, a ověřuje se, že `i+1` je opravdu SOUPEŘ.

OTÁZKA (zadání): z kol, kde nosič KONČÍ označený, v kolika bylo v jeho
zbývajícím pohybu volné NEOZNAČENÉ pole? To dělí „engine vybral špatně" od
„nebylo kam" — a opravitelná je jen první část.

⭐ ROZPAD PODLE MECHANISMU, ne jedno číslo. Označení na konci kola má čtyři
  různé příčiny a každá patří jinému kusu kódu:
    A  označen už na začátku ∧ nosič se NEHNUL   → chybí ÚTĚK (nosič je
       vyřazený z obecné REPOSITION `macro_actions.cpp:2054` i z ústupu
       po blitzu `:1383`)
    B  na začátku volný ∧ hnul se ∧ končí označen → vešel do kontaktu
       (ADVANCE to zakazuje `:2817`/`:2849`/`:2878`, takže tady je únik
       jinou cestou: SCORE, vlastní BLITZ, nebo chůze zastavená v půli)
    C  označen na začátku ∧ hnul se ∧ pořád označen → útěk nedotažen
    D  na začátku volný ∧ NEHNUL se ∧ končí označen → soupeře k němu
       přistrčil náš vlastní push/blok

⭐ POZITIVNÍ KONTROLA JE VEVNITŘ A BĚŽÍ PRVNÍ (pravidlo: k nule vždy důkaz,
  že čítač umí najít jedničku). Testuje se `marked()` i hledač volného pole
  na ručně postavených deskách, kde odpověď znám předem. Když spadne,
  skript se ani nepustí na korpus.

⚠️ ROZPOČET POHYBU JE HORNÍ MEZ, ne přesná hodnota. Log neveze
  `movementRemaining`, takže se počítá `MA − chebyshev(začátek, konec)`;
  skutečná cesta je vždy ≥ chebyshev (obcházení) a GFI ubírá dál ⇒ číslo
  „bylo kam jít" je NADHODNOCENÉ, tedy horní mez opravitelné části.
  Proto se tiskne i varianta s rozpočtem 1 (nejmenší možná žádost).

⚠️ A „bylo kam" ≠ „mělo se jít": nosič v TZ platí za odchod DODGE. Doktrína
  Q3/B2 (03.09.) říká, kdy se to smí: P(dodge selže) × zbývající aktivace < 1.
  Tenhle skript měří PŘÍLEŽITOST, ne správné rozhodnutí.

⛔⛔⛔ TŘI KOŠE, NE JEDNO ČÍSLO — DOKTRÍNA UŽIVATELE 10.09.:
  *„ono není vhodné ani postavit nosiče vedle ležícího soupeře — ale to chce
  jeho blitz — ale když mu dáme nosiče, proč by na to blitz nevyužil?"*
  ⇒ Vyřazení LEŽÍCÍCH z expozice NOSIČE bylo špatně: ležící soused není
  zdarma, jen se platí BLITZEM — a nosič je nejcennější blitz cíl na desce,
  takže předpoklad „soupeř blitz na míč neutratí" je nesmysl
  ([[feedback_worst_case_is_readable_not_searched]]: nejhorší případ se ČTE,
  soupeřovy zdroje jsou konečné, tedy vyjmenovatelné).
  ⭐ Zůstává to ale OMEZENÉ, a v tom je celá jemnost:
      N STOJÍCÍCH sousedů  = N bloků ZDARMA (neomezeně za kolo)
      N LEŽÍCÍCH sousedů  ≈ JEDEN zásah (blitz je 1× za kolo)
  ⇒ dvě různé TVRDOSTI, ne dvě velikosti téhož — týž argument, jakým P42
  samo odlišilo blok od blitzu, jen o patro níž (a táž doktrína jako P45).
  ⇒ Proto se tiskne rozpad na TŘI koše:
      ZDARMA    ≥1 stojící soused          (N bloků zdarma)
      ZA BLITZ  0 stojících, ≥1 ležící     (nejvýš jeden zásah)
      BEZPEČNO  ani jeden
  a u koše ZDARMA i POČET stojících sousedů — to je počet bloků zdarma.

Použití: python3 diag_carrier_marked_end_20260910.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "corpus_m11_20260909_data"
DIRS = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]
STANDING = 0
W, H = 26, 15          # Position::PITCH_WIDTH/HEIGHT (position.h:13-14)


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def tz_squares(them):
    """Pole, na která dosahuje tacklezóna STOJÍCÍHO soupeře.
    ⛔ Ležící se nepočítá: nemá TZ a blokovat nemůže (oprava z 20.08., kvůli
    které 39,3 % spadlo na 12,3 %)."""
    return {(e["x"] + dx, e["y"] + dy)
            for e in them if e["state"] == STANDING
            for dx, dy in DIRS}


def neighbours(car, them):
    """(stojící sousedé, ležící sousedé) nosiče. ⭐ Dva různé druhy nebezpečí:
    stojící = blok ZDARMA a kolikrát chce; ležící = jeho JEDINÝ blitz."""
    here = (car["x"], car["y"])
    st = pr = 0
    for e in them:
        if cheb(here, (e["x"], e["y"])) <= 1:
            if e["state"] == STANDING:
                st += 1
            else:
                pr += 1
    return st, pr


def marked(car, them):
    """Stojí nosič v tacklezóně STOJÍCÍHO soupeře? = predikát K38/P42
    v jeho „zdarma" podobě (blok, ne blitz)."""
    return neighbours(car, them)[0] > 0


def nearest_free_unmarked(car, mine, them, budget, strict=False):
    """Nejbližší volné pole do `budget` kroků, na kterém nosič není označený.
    `strict=False` → bez tacklezóny STOJÍCÍHO soupeře (koš ZDARMA).
    `strict=True`  → navíc ani soused LEŽÍCÍHO (koš BEZPEČNO, doktrína 10.09.).
    Vrací vzdálenost, nebo None. Obsazená pole (i naše) se nepočítají."""
    if budget < 1:
        return None
    occ = {(p["x"], p["y"]) for p in mine} | {(p["x"], p["y"]) for p in them}
    tz = tz_squares(them)
    if strict:
        tz = tz | {(e["x"] + dx, e["y"] + dy)
                   for e in them if e["state"] != STANDING
                   for dx, dy in DIRS}
    here = (car["x"], car["y"])
    best = None
    for dx in range(-budget, budget + 1):
        for dy in range(-budget, budget + 1):
            q = (here[0] + dx, here[1] + dy)
            if q == here:
                continue
            if not (0 <= q[0] < W and 0 <= q[1] < H):
                continue
            d = cheb(here, q)
            if d > budget:
                continue
            if q in occ or q in tz:
                continue
            if best is None or d < best:
                best = d
    return best


# ---------------------------------------------------------------- pozitivní
def positive_control():
    """⭐ Než se přečte jakákoli nula: umí měřidlo najít jedničku?
    Ruční desky, u kterých odpověď znám předem."""
    ok = True

    def chk(name, got, want):
        nonlocal ok
        flag = "OK " if got == want else "SPADLO"
        if got != want:
            ok = False
        print(f"    [{flag}] {name}: dostal {got!r}, čekal {want!r}")

    car = {"x": 10, "y": 7, "state": 0, "ma": 6, "id": 1}
    # (1) stojící soupeř vedle => označen
    chk("marked: stojící soused", marked(car, [{"x": 11, "y": 7, "state": 0}]), True)
    # (2) LEŽÍCÍ soupeř vedle => NEoznačen (jádro opravy z 20.08.)
    chk("marked: ležící soused nepočítá",
        marked(car, [{"x": 11, "y": 7, "state": 1}]), False)
    # (3) stojící o dvě pole dál => neoznačen
    chk("marked: soupeř na dva kroky", marked(car, [{"x": 12, "y": 7, "state": 0}]),
        False)
    # (4) hledač: jeden soupeř vedle => volno na 1 krok existuje na druhé straně
    chk("hledač najde pole na 1 krok",
        nearest_free_unmarked(car, [car], [{"x": 11, "y": 7, "state": 0}], 6), 1)
    # (4b) TŘI KOŠE: ležící soused se NESMÍ počítat jako stojící, ale ani
    #      jako bezpečno (doktrína 10.09. — stojí soupeře blitz, ne nic).
    chk("neighbours: 2 stojící + 1 ležící",
        neighbours(car, [{"x": 11, "y": 7, "state": 0},
                         {"x": 9, "y": 7, "state": 0},
                         {"x": 10, "y": 8, "state": 1}]), (2, 1))
    # (4c) strict vs nestrict se MUSÍ rozejít, jinak přepínač nic nedělá.
    #      Prstenec LEŽÍCÍCH ve vzdálenosti 2: stojící TZ žádná (nestrict najde
    #      pole na 1 krok), ale jejich sousedství pokrývá prstence 1..3
    #      => strict do rozpočtu 2 nenajde nic.
    prone_ring = [{"x": 10 + dx, "y": 7 + dy, "state": 1}
                  for dx in range(-2, 3) for dy in range(-2, 3)
                  if max(abs(dx), abs(dy)) == 2]
    chk("hledač NEstrict prstenec ležících ignoruje",
        nearest_free_unmarked(car, [car], prone_ring, 2, strict=False), 1)
    chk("hledač STRICT prstenec ležících respektuje",
        nearest_free_unmarked(car, [car], prone_ring, 2, strict=True), None)
    # (5) hledač: obklíčení stojícími tak, že do 2 kroků není nic bez TZ.
    #     Kruh soupeřů ve vzdálenosti 2 pokrývá TZ celý prstenec 1..3.
    ring = [{"x": 10 + dx, "y": 7 + dy, "state": 0}
            for dx in range(-2, 3) for dy in range(-2, 3)
            if max(abs(dx), abs(dy)) == 2]
    chk("hledač vrátí None při obklíčení (rozpočet 2)",
        nearest_free_unmarked(car, [car], ring, 2), None)
    # (6) týž kruh, ale rozpočet 4: PŘES prstenec se dostane => NEsmí být None
    got = nearest_free_unmarked(car, [car], ring, 4)
    chk("hledač přes prstenec při rozpočtu 4 najde pole", got is not None, True)
    # (7) rozpočet 0 => None (bez pohybu se nikam nejde)
    chk("hledač při rozpočtu 0", nearest_free_unmarked(car, [car], [], 0), None)
    return ok


# ---------------------------------------------------------------- korpus
def run():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    races = {}
    skip = Counter()
    logs_seen = 0
    pair_moved = 0        # kontrola párování: E se od S opravdu liší

    for f in files:
        g = json.load(gzip.open(f))
        logs = g["turn_logs"]
        for i, S in enumerate(logs):
            us = S["active_team"]
            them_key = "away" if us == "home" else "home"
            race = g[f"{us}_race"]
            st = races.setdefault(race, dict(
                turns=0, m_start=0, m_end=0, cls=Counter(),
                avail=Counter(), mind=Counter(), avail1=Counter(),
                lost_ball=0, best_dist=0,
                basket=Counter(), nstand=Counter(), nstand_start=Counter(),
                safe_avail=Counter(), best_strict=0, sub=Counter()))

            logs_seen += 1
            if i + 1 >= len(logs):
                skip["poslední snímek hry (není konec kola)"] += 1
                continue
            E = logs[i + 1]
            # ⛔ mezi S a E se nesmí přestavět hřiště
            if S.get("touchdown"):
                skip["TD v kole (mezi snímky výkop)"] += 1
                continue
            if E["half"] != S["half"]:
                skip["hranice poločasu"] += 1
                continue
            if E["active_team"] == us:
                skip["i+1 není soupeř (výkop/přestavba)"] += 1
                continue

            carS = next((p for p in S[f"{us}_players"] if p["has_ball"]), None)
            if carS is None:
                skip["na začátku kola míč nedržíme (predikát N/A)"] += 1
                continue
            if carS["state"] != STANDING:
                skip["nosič na začátku LEŽÍ (K38 skip)"] += 1
                continue

            carE = next((p for p in E[f"{us}_players"] if p["has_ball"]), None)
            if carE is None or carE["id"] != carS["id"]:
                # ⛔ Ztráta míče / výměna nosiče NENÍ chybějící datum, je to
                #   jiný druh selhání — vede se zvlášť, ne jako skip.
                st["lost_ball"] += 1
                continue
            if carE["state"] != STANDING:
                skip["nosič na konci LEŽÍ (sražen — jiná vada)"] += 1
                continue

            themS = S[f"{them_key}_players"]
            themE = E[f"{them_key}_players"]
            mineE = E[f"{us}_players"]

            st["turns"] += 1
            nsS, nprS = neighbours(carS, themS)
            nsE, nprE = neighbours(carE, themE)
            ms = nsS > 0
            me = nsE > 0
            st["m_start"] += ms
            st["m_end"] += me
            st["nstand_start"][nsS] += 1

            # ⭐ TŘI KOŠE na KONCI kola (doktrína 10.09.)
            if nsE > 0:
                st["basket"]["ZDARMA (≥1 stojící soused)"] += 1
                st["nstand"][nsE] += 1
            elif nprE > 0:
                st["basket"]["ZA BLITZ (0 stojících, ≥1 ležící)"] += 1
            else:
                st["basket"]["BEZPEČNO (ani jeden soused)"] += 1

            moved = (carS["x"], carS["y"]) != (carE["x"], carE["y"])
            if moved:
                pair_moved += 1

            # dostupnost ÚPLNĚ ČISTÉHO pole se ptá i u koše ZA BLITZ — tam je
            # otázka „šlo se zbavit i toho jednoho zásahu?"
            if nsE > 0 or nprE > 0:
                spent0 = cheb((carS["x"], carS["y"]), (carE["x"], carE["y"]))
                bud0 = max(0, carS["ma"] - spent0)
                ds = nearest_free_unmarked(carE, mineE, themE, bud0, strict=True)
                key = "ZDARMA" if nsE > 0 else "ZA BLITZ"
                st["safe_avail"][(key, ds is not None)] += 1
                if ds is not None:
                    st["best_strict"] = max(st["best_strict"], ds)

            if not me:
                continue

            # ⭐ PODATRIBUCE Z UDÁLOSTÍ: „končí označený" má tři různé příčiny
            #   a každá patří jinému kusu kódu. Události kola jsou v S
            #   (`game_simulator.cpp:938` je připisuje do AKTUÁLNÍHO snímku).
            cid = carS["id"]
            ev = S["events"]
            car_blocked = any(e.get("type") == "BLOCK"
                              and e.get("player_id") == cid for e in ev)
            car_fouled = any(e.get("type") == "FOUL"
                             and e.get("player_id") == cid for e in ev)
            car_any = any(e.get("player_id") == cid for e in ev)
            endpos = (carE["x"], carE["y"])
            pushed_next = any(e.get("type") == "PUSH"
                              and cheb(endpos, (e.get("to_x", -9),
                                                e.get("to_y", -9))) <= 1
                              for e in ev)
            sub = ("c1 nosič sám BLOKOVAL (kontakt z konstrukce)" if car_blocked
                   else "c1b nosič FAULOVAL" if car_fouled
                   else "c2 soupeře k němu PŘISTRČIL náš push" if pushed_next
                   else "c3 chůze ho v kontaktu NECHALA" if car_any
                   else "c4 nosič se v kole VŮBEC NEPROJEVIL")
            st["sub"][sub] += 1

            cls = {(True, False): "A označen na začátku, NEHNUL se",
                   (False, True): "B na začátku volný, VEŠEL do kontaktu",
                   (True, True): "C označen, hnul se, pořád označen",
                   (False, False): "D volný, nehnul se, přistrčen k němu"}[(ms, moved)]
            st["cls"][cls] += 1

            # rozpočet: nehnul se => plné MA; hnul se => MA − ušlá vzdálenost
            # (⚠️ HORNÍ MEZ, viz docstring)
            spent = cheb((carS["x"], carS["y"]), (carE["x"], carE["y"]))
            budget = max(0, carS["ma"] - spent)
            d = nearest_free_unmarked(carE, mineE, themE, budget)
            st["avail"][(cls, d is not None)] += 1
            if d is not None:
                st["mind"][d] += 1
                st["best_dist"] = max(st["best_dist"], d)
            d1 = nearest_free_unmarked(carE, mineE, themE, 1)
            st["avail1"][(cls, d1 is not None)] += 1

    return races, skip, logs_seen, pair_moved


def show(race, st):
    n = st["turns"]
    if not n:
        return
    print(f"\n=== {race}: {n} spárovaných kol se stojícím nosičem na OBOU koncích ===")
    print(f"  (+ {st['lost_ball']} kol, kde jsme nosiče/míč během kola ztratili"
          f" — vedeno zvlášť, není to skip)")
    print(f"  nosič označen na ZAČÁTKU kola  {st['m_start']:6d}  "
          f"{100*st['m_start']/n:5.1f} %   <- tohle četl cage_built (soupeřova akce)")
    print(f"  nosič označen na KONCI kola    {st['m_end']:6d}  "
          f"{100*st['m_end']/n:5.1f} %   <- ⭐ predikát P42/K38 (naše volba)")

    print("\n  ⛔⛔ TŘI KOŠE NA KONCI KOLA (doktrína uživatele 10.09. —"
          " ležící NENÍ zdarma, stojí jeho blitz):")
    tot = 0
    for k in ("ZDARMA (≥1 stojící soused)", "ZA BLITZ (0 stojících, ≥1 ležící)",
              "BEZPEČNO (ani jeden soused)"):
        v = st["basket"][k]
        tot += v
        print(f"    {k:<38s} {v:6d}  {100*v/n:5.1f} %")
    print(f"    ZBYTEK (musí být 0)  {n - tot}")
    exp = st["basket"]["ZDARMA (≥1 stojící soused)"] \
        + st["basket"]["ZA BLITZ (0 stojících, ≥1 ležící)"]
    print(f"    ⇒ EXPOZICE CELKEM (zdarma + za blitz) {exp} = {100*exp/n:.1f} %")
    print("  kolik BLOKŮ ZDARMA to je (počet stojících sousedů nosiče):")
    for k in sorted(st["nstand"]):
        v = st["nstand"][k]
        print(f"    {k} stojících  {v:6d}  {100*v/n:5.1f} %")
    for key in ("ZDARMA", "ZA BLITZ"):
        y = st["safe_avail"][(key, True)]
        nn = st["safe_avail"][(key, False)]
        if y + nn:
            print(f"  koš {key}: ÚPLNĚ ČISTÉ pole (ani stojící, ani ležící) "
                  f"bylo k dispozici {y}/{y+nn} = {100*y/(y+nn):.1f} %")
    print(f"  pozitivní kontrola z DAT (strict hledač): největší nalezená "
          f"vzdálenost {st['best_strict']}")

    me = st["m_end"]
    if not me:
        print("  ⛔ nula kol s označeným nosičem na konci — viz pozitivní kontrola výš")
        return
    print(f"\n  ROZPAD {me} kol podle MECHANISMU (musí dát dohromady {me}):")
    tot = 0
    for k in sorted(st["cls"]):
        v = st["cls"][k]
        tot += v
        yes = st["avail"][(k, True)]
        no = st["avail"][(k, False)]
        y1 = st["avail1"][(k, True)]
        print(f"    {k:<45s} {v:6d}  {100*v/me:5.1f} %")
        print(f"        z toho BYLO KAM JÍT (horní mez rozpočtu) {yes:6d} "
              f"= {100*yes/v:5.1f} %   nebylo {no:6d}   (zbytek {v-yes-no})")
        print(f"        totéž s rozpočtem jen 1 krok             {y1:6d} "
              f"= {100*y1/v:5.1f} %")
    print(f"    ZBYTEK (musí být 0)  {me - tot}")
    print(f"\n  PODATRIBUCE týchž {me} kol Z UDÁLOSTÍ (musí dát {me}):")
    tot2 = 0
    for k in sorted(st["sub"]):
        v = st["sub"][k]
        tot2 += v
        print(f"    {k:<45s} {v:6d}  {100*v/me:5.1f} %")
    print(f"    ZBYTEK (musí být 0)  {me - tot2}")
    ay = sum(v for (c, a), v in st["avail"].items() if a)
    an = sum(v for (c, a), v in st["avail"].items() if not a)
    print(f"\n  ⭐ CELKEM: bylo kam jít {ay} = {100*ay/me:.1f} % "
          f"· nebylo kam {an} = {100*an/me:.1f} % · zbytek {me-ay-an}")
    print("  vzdálenost k nejbližšímu volnému NEoznačenému poli (histogram):")
    for d in sorted(st["mind"]):
        v = st["mind"][d]
        print(f"    {d} kroků  {v:6d}  {100*v/me:5.1f} %")
    print(f"  pozitivní kontrola z DAT: největší nalezená vzdálenost "
          f"{st['best_dist']} ⇒ hledač nevrací jen 1")


if __name__ == "__main__":
    print("POZITIVNÍ KONTROLA MĚŘIDLA (běží PŘED korpusem):")
    if not positive_control():
        sys.exit("⛔ pozitivní kontrola SPADLA — čísla z korpusu se nesmí čít")
    print("  ⇒ měřidlo umí najít jedničku i nulu, obojí na známé desce.\n")

    races, skip, logs_seen, pair_moved = run()
    print(f"snímků v korpusu celkem: {logs_seen}")
    print("VYŘAZENO (a proč) — jmenovatel se nesmí ztratit tiše:")
    ssum = 0
    for k, v in skip.most_common():
        ssum += v
        print(f"    {k:<50s} {v:7d}")
    used = sum(s["turns"] + s["lost_ball"] for s in races.values())
    print(f"    ---- vyřazeno {ssum} + použito {used} = {ssum+used} "
          f"(z {logs_seen}, zbytek {logs_seen-ssum-used})")
    print(f"kontrola párování: v {pair_moved} použitých kolech se nosič HNUL "
          f"⇒ E není kopie S (ochrana proti off-by-one)")

    for race in sorted(races, key=lambda r: -races[r]["turns"]):
        show(race, races[race])
