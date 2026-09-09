# PŘEDREGISTRACE — M14b (`blitzPathArm`), OPRAVENÁ KALIBRACE, mode 15, dw-dw
**Zapsáno PŘED spuštěním sondy. Prahy platí tak, jak stojí tady.**

---

## Proč tenhle běh existuje

Plochá cena BFS cesty blitzujícího (K=2 za tacklezónu, K=1 za GFI) byla
08.09. změřena s rezervou a **zamítnuta** (−2,72σ, škodí) —
`evidence/night_prereg_20260907_m14b.preds`, `ab_m14b_20260907/`.

Konkrétní situace ze zkušební partie 04.09. ("blitz rovně přes dodge do
tří tacklezón místo bezpečné cesty") ale potvrdila, že **koncept** (BFS
cesta vyhýbající se riziku místo hladového `pickApproachStep`) je
správný — problém byl v **kalibraci** ceny, ne v myšlence. 09.09. opraveny
tři věci (commit `548696d3`):

1. Multiplikátor rizika 6,0 → **4,0**, ukotveno na skutečný běžný případ
   (cíl 4+, P_fail=1/2 → cena 2 pole) — starý multiplikátor vycházel z
   matematicky špatného výpočtu ("1/3" odpovídá cíli 3+, ne 4+).
2. Reroll natvrdo `false` pro ocenění cesty (konzervativní — dřív dělalo
   GFI pole skoro zadarmo bez ohledu na to, jestli ho bude potřebovat
   někdo jiný později v kole).
3. `gfiSequenceFailProb` zobecněna z hardcoded N≤2 na libovolné N
   (vzorec `1 - s^N*(1+N*p)` s rerollem, `1-s^N` bez — numericky ověřeno
   proti starým hodnotám, přesná shoda).

Tenhle běh měří, jestli oprava kalibrace **otočí znaménko** (−2,72σ →
neutrální/pomáhá), ne jen jestli je menší v absolutní hodnotě.

MECHANISMUS: BFS cesta blitzujícího mění bezpečnost PŘIBLÍŽENÍ k cíli (méně
turnoverů při doběhu do tacklezóny) -- měří se přímo přes `takeBlitzOutcome`
(`engine/src/action_resolver.cpp`, commit `98db65bf`): kolik blitzů dojde
k bloku a kolik z nich srazí cíl, podle politiky chůze (BFS vs hladová).
SANITY-TEST: 2 páry, mode 15, dw-dw -- BFS provedeno 6655, cíl shozen 3239
(48,7 %); hladová provedeno 6649, cíl shozen 3181 (47,8 %). Obě strany
nenulové, číslo dává smysl -- čítač funguje.

## Co se měří

Mode 15, `blitzPathArm` zapnuté pro kandidátní stranu, `nextStepTowardAdjacent`
(BFS, opravená pravděpodobnostní cena) místo hladového `pickApproachStep`
pro každý krok blitzového přiblížení. Matchup **dw-dw** (index 2) — stejný
matchup jako zamítnutá 08.09. sonda, aby šlo srovnávat přímo.

## Dimenzování — SONDA, ne rozhodná noc

Podle vzoru P9c/M12(B) sond z 08.09. (uživatel: krátký běh, ne plýtvat
noc na první čtení směru): **PAIRS = 80, WORKERS = 4, CHUNKS = 8.**

Tempo z 07.09. (**stará kalibrace**): ~80,7 s/pár/worker. **Přeměřeno
09.09. čerstvě** (4 páry, 4 workery, 1 pár/worker): nejpomalejší kus
1,6 min = **96 s/pár/worker**, cca o 19 % pomaleji. Buď zobecněný vzorec
`gfiSequenceFailProb` (libovolné N místo hardcoded N≤2) stojí o něco víc
výpočtu, nebo je to šum stroje — nerozlišeno, bráno konzervativně.
⇒ **80 párů / 4 workery ≈ 20 párů/worker × 96 s ≈ 32 minut.**

## Nulová kontrola

`MOVED WITHOUT THE ARM ACTING` == 0, stejně jako u každé jiné noci s
touhle kontrolou zapnutou.

## Předregistrované předpovědi (`.preds`, strojově kontrolované)

| | čekám | proč |
|---|---|---|
| leak | 0 | jinak se nic nečte |
| arm_acted | ≥ 0,85 | stejná řídicí podmínka jako u zamítnuté verze, jen jiná cena — frekvence aktivace by se neměla měnit |
| n_nonzero | 0,10–0,65 | podíl přesměrovaných kroků se změnou konstant může mírně posunout oproti staré verzi, ale ne dramaticky — širší okno než u 07.09. sondy |
| delta | −0,05 až +0,10 | koncept potvrzen situací ze hry 04.09., takže čekám neutrálně-pozitivní, ne symetricky kolem nuly jako 07.09.; dolní okraj úzký, protože oprava cílí přesně na příčinu zamítnutí, ne na náhodnou jinou konstantu |
| delta_1s | −0,03 až +0,05 | totéž jednostranně, poloviční šířka |

## Pořadí čtení výsledku

1. `MOVED WITHOUT THE ARM ACTING` = 0?
2. `arm acted in N/M`.
3. `n_nonzero`.
4. Sloučená delta + `night_summarize.py` (sdružená SE, empirická SE mezi
   shardy, počet záporných shardů).
5. Srovnání se 07.09. sondou (stejný matchup, jiná kalibrace) — otočilo
   se znaménko?

## Co dál podle výsledku

- **Pomáhá / neutrální v pásmu ±0,015:** kandidát na nasazení do
  produkce, ale až po větší (rozhodné) noci — tohle je jen sonda.
- **Pořád škodí:** koncept (vyhýbat se riziku na cestě) možná chybný sám
  o sobě, ne jen kalibrace — vrátit do fronty na revizi zamítnutých
  ramen, ne opravovat čtvrtou konstantu narychlo.
