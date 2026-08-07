# Design: odepření volného míče + dvojrozměrné rozhodnutí „jít / zavazet"

Připraveno 07.08. odpoledne pro implementaci v pondělí 10.08. Vstupy uživatele
ze 07.08. (doktrína odepření, kritérium „šance × existuje klec", pořadí fází,
cena turnoveru v okamžiku selhání). Doktrinální zápis:
`memory/project_bloodbowl_loose_ball_denial_doctrine_20260807.md`.
Zdrojová situace: g0003 (dwarf vs wood-elf, H2T2).

---

## Krok 1 (první, samostatně měřený): DENIAL ČLEN V HODNOTÍCÍ FUNKCI

### Proč
`simulate()` v `macro_mcts.cpp` má dnes u míče dva bloky:

* **volný míč** (~ř. 760-772): `-0.1` za to, že leží, plus bonus **jen za
  blízkost** („quick pickup potential"): nejbližší náš ≤2 pole `+0.08`,
  ≤4 pole `+0.04`. Neřeší, jestli u míče někdo STOJÍ, ani kolik nás tam je.
* **držený míč soupeře** (~ř. 774-784): denial UMÍ — `+0.08` za každou naši
  tackle zónu na nosiči, strop `+0.24`.

⇒ Markování se cení u nosiče, ale ne u volného míče. AI proto nemá důvod
postavit se k volnému míči a zavazet — přesně chování, které doktrína chce.

### Změna (zrcadlo existujícího členu)
Do větve volného míče přidat:

```cpp
// Denial: our tackle zones on the loose ball's square. Mirrors the
// carrier-marking term below -- the opponent's pickup takes +1 per zone,
// and whoever does come up with it starts the turn next to us.
int myTZ = countTacklezones(state, state.ball.position, opponent(perspective));
heuristic += DENIAL_PER_TZ * std::min(myTZ, DENIAL_TZ_CAP);
```

Pozor na sémantiku: `countTacklezones(state, pos, friendlySide)` počítá zóny
strany OPAČNÉ k `friendlySide` — proto se předává `opponent(perspective)`
(stejný vzor jako u nosiče, kde se předává `ballHolder.teamSide`).

**Kalibrace:** začít zrcadlově `DENIAL_PER_TZ = 0.08`, ale **strop 2**
(max `+0.16`), ne 3. Důvod: celá větev volného míče dnes dává nejvýš `+0.08`;
plný strop `+0.24` by sekci přebil a mohl by vyrábět „stání u míče" i tam,
kde je správné ho sebrat. A/B: 0.08/cap2 vs 0.04/cap2 vs 0.08/cap3.

**Vědomě NEDĚLÁME v tomtéž kroku** (jedna změna najednou): symetrický postih
za soupeřovy zóny na míči (on odepírá nám). Zaznamenat jako navazující
kandidát a měřit zvlášť.

### Testy
1. Volný míč, dva naši stojící vedle → hodnota vyšší než tentýž stav bez nich
   (a vyšší než při pouhé blízkosti na 2 pole = odlišení od proximity bonusu).
2. Strop: tři a čtyři sousedé nedají víc než dva.
3. Perspektiva: tentýž stav z pohledu soupeře nesmí dostat náš denial bonus.

### Měření
`diag_item13_staged_planner` (STAGE D už tiskne oba členy) + krátký A/B
v `diag_f1_cage_advance` harnessu na dw-we a dw-sk. **Dotýká se value funkce
⇒ všech ras a celého hledání** — párový A/B, ne jen sonda.

---

## Krok 2: DVOJROZMĚRNÉ ROZHODNUTÍ V STAGED PLÁNOVAČI

Dnešní plochý práh `MIN_PICKUP_SUCCESS = 0.25` (turn_planner.h) je aproximace
jen prvního členu. Nahradit:

```
plán je platný, pokud:
   chance >= MIN_PICKUP_SUCCESS                       (hod má smysl)
   A NE ( projectedCorners == 0 A carrierBlitzable )  (nekončit sám v obklíčení)
```

* `chance` — `calculatePickupTargetAt` + Sure Hands (už hotové, 2576e26).
* `projectedCorners` — `CageAdvancePlanner::tryAssign` na projekci
  (už veřejné, 575d4f1); `filled` po tahu.
* `carrierBlitzable` — **`carrierIsBlitzable()` v macro_actions.cpp UŽ
  EXISTUJE** (statická, ř. ~972); exportovat stejným způsobem jako dnes
  `carrierStallAwareSteps`.

Podmínka „a zároveň blitzovatelný" je nutná: volný míč na naší polovině bez
soupeře poblíž se má prostě sebrat, i když klec nevznikne.

### ⚠️ Známá mezera měření (z dnešního STAGE D)
`tryAssign` odmítá `step < 1`, takže pro sběrače, který dorazí k míči
s NULOVÝM zbytkem pohybu (g0003!), se rohy vůbec nespočítají (`-1`). Přitom
právě tam je odpověď nejdůležitější. Oprava: povolit `step == 0` (klec kolem
aktuálního pole nositele) — `build()` iteruje od 1, takže se ho to nedotkne;
strážní podmínku změnit na `step < 0`. Doplnit test na step=0.

---

## Krok 3 (větší, po krocích 1-2): OTEVÍRACÍ BLITZ

Pořadí fází: `bez kostek → BLITZ → PICKUP → dostavba klece`; vyhodnocení
zůstává dvouvětvové (celý řetěz = jeden úspěch / neúspěch — námitka uživatele
07.08., potvrzeno `sampleBranch`). Nutné doplňky:
* `stagedMacroStillValid` musí přijmout BLITZ (dnes vše kromě REPOSITION
  a PICKUP zamítá);
* **kontrola premisy**: plán si zapamatuje počet TZ na míči, se kterým
  počítal; po blitzu porovná se skutečností — když marker zůstal u míče,
  premisa padá → fallback na search (vzor `requireHeldBall`, 575d4f1);
* výběr cíle blitzu: soupeř sousedící s míčem, push směrem OD míče;
* pořadí není dogma: plánovač má umět ocenit obě varianty hodnotou stavu
  v bodě selhání (bezkostkový tah, který zhorší formaci — např. rozebrání
  klece — se nemá bankovat před kostkou). Podklad: item10 S7 (path-clearing
  blitz, naivní odklad −25,9 SE) a shipnutý Q-guard `config_.riskDeferral`
  (default OFF).

## Pořadí a rizika
1. Denial člen (malý, hned měřitelný, nezávislý na plánovači).
2. Dvojrozměrné rozhodnutí + oprava `step == 0`.
3. Otevírací blitz (největší, chce vlastní validaci).

Rizika: kroky 1 a 2 se dotýkají VŠECH ras (value funkce / generace) — každý
samostatně a párově měřený. Krok 1 navíc mění chování i mimo trpasličí
scénáře (kdokoli u volného míče), takže ho neposuzovat jen na dw-we.
