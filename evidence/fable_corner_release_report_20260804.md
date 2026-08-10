# Corner-release groundwork — podklad k diskuzi (Fable, 04.08.2026)

Zadání: `evidence/fable_corner_release_groundwork_20260804.md`. Galerie všech
62 situací: `evidence/fable_corner_release_gallery_20260804.md`. PODKLAD pro
diskuzi kalibru blitzu — žádná implementace, žádné rozhodnutí.

## Závěry napřed

1. **DICEY je 62/225 ADVANCE tahů (27,6 %)**; PLAN_READY jen 23 (10,2 %),
   TEMPO_INSUFFICIENT 137 (60,9 %). N=24 her (4 matchupy × 6), seedy 36M+.
2. **Hlavní zbytek po substituci rohů NENÍ marked roh, ale TZ stín na
   koridoru (kategorie b: 40/62 = 65 %).** Marked roh bez dosažitelného
   substituta (a) je 15/62 (24 %), úplný nedostatek těl (c) jen 3/62,
   jiné (d, pile spoluhráčů na trase) 4/62. Uvolnění ROHU je tedy menší
   polovina problému; větší je uvolnění KORIDORU — přesně konflikt „jediný
   blitz tahu: roh vs. koridor" z uživatelovy osnovy.
3. **Block-only vrstva odemkne ~12/62 DICEY (19 %)** → adoption horní odhad
   10,2 % → 15,6 % ADVANCE tahů (+5,3 pp). U marked rohů (a+c) uvolní
   7/18 (39 %); u koridorů (b) jen 5/40 (13 %) — casteři koridoru zřídka
   sousedí s volným spoluhráčem.
4. **Block+blitz vrstva odemkne ~35/62 (56 %)** → horní odhad 25,8 % ADVANCE
   tahů (+15,6 pp proti dnešku). U marked rohů uvolní 16/18 (89 %, typicky
   2d blitz s fail ~0,03–0,19); u koridorů 19/40. **20/62 situací (32 %,
   všechny kat. b) potřebuje >1 blitz → jedinou blitz akcí neřešitelné** —
   tam zbývá objezd, jiná noha plánu, nebo přijmout fallback.
5. Vedlejší nález: velká část kategorie b padá na **exec-fail bez kostek** —
   REPOSITION greedy walk se stínu vyhýbá, dojde mu budget a zastaví
   1–6 polí před cílem (`miss` v galerii). Část DICEY by tedy odemkl i
   levnější ne-bojový mechanismus (route-around s delším rozpočtem, jiný
   výběr nohy), ne jen block/blitz. Stojí za samostatnou položku fronty.

## Uživatelova čtveřice čísel (agregát; per situace viz galerie)

| metrika | hodnota |
|---|---|
| rohů potřebujících dodge (celkem / Ø na situaci) | 35 / 0,56 |
| volných těl na substituci (Ø na situaci) | 2,4 — těla většinou JSOU, ale `reach=0`: nedosáhnou na selhavší slot v MA+1 (proto substituce nestačila) |
| marked rohů uvolnitelných BLOCKem (a+c situace) | 7/18 (39 %); kostky reálné: typicky 2d s asisty, ale soused-blocker často chybí |
| marked rohů uvolnitelných BLITZem | 16/18 (89 %); path-aware item14 odhad, fail ≤ 0,35 (většinou 0,03–0,19), kolize s jediným blitzem u marked rohů 0 |

Selhání carrierovy nohy: 10/62 (dodge/GFI carriera — release nepomůže,
správně padá na fallback). Prahy: block ≥1d útočník volí; blitz kombinovaný
fail ≤ 0,35. Vše horní odhady: po uvolnění může selhat další noha plánu a
push-only výsledek bloku může TZ jen posunout (viz Limity).

## Ukázkové situace (plné mapy v galerii)

**Kat. a — block-release by stačil** (dw-sk-g4 H1 T5): roh P7 marked od
P16 (ST3); volný P2 stojí vedle markera, s asisty 2d blok (`blockRel=1`).
Dnes: DICEY → fallback, klec stojí.

**Kat. c — 3d blok na stole** (orc-sk-g2 H1 T2): roh P1 marked, nula
volných těl na substituci, ale sousední P2 má **3d blok** na markera P16.
Nejlevnější možný release, sonda ho dnes nevidí.

**Kat. b — kolize blitzu** (dw-sk-g4 H2 T3): koridor carriera stíní
5 casterů, bloky pokryjí 3, zbývají 2 → jeden blitz nestačí
(`collision=1`). Typický zástupce 20 neřešitelných situací.

## Návrh design constraintů release vrstvy — OPCE K DISKUZI

### R-A: block-only release (nejmenší krok)
Po substituci: markery selhavší nohy (marker rohu či caster koridoru),
které blokuje volný spoluhráč UŽ STOJÍCÍ vedle, dostanou BLOCK jako
přípravnou nohu plánu (žádný pohyb navíc, kostky spočtené s asisty).
Odemkne ~19 % DICEY (+5,3 pp adoption, horní odhad). Konflikty: rozpočet
těl (blocker nesmí být jediný kandidát rohu → reservedPlayerIds); push-only
výsledek TZ nutně neodstraní → po projekci bloku znovu ověřit dice-free,
jinak fallback. Acceptance: A/B 4 matchupy, DICEY→PLAN_READY konverze,
adoption/hru, HtH neregrese (šumové dno ±8–11 pp), pTO nezvýšené.

### R-B: block + blitz release
Navíc jediný blitz tahu na marker/caster mimo adjacenci; blitzer path-aware
dle item14 (sdílená rutina pickApproachStep — odhad nesmí divergovat od
exekuce). Odemkne ~56 % DICEY (+15,6 pp, horní odhad); 32 % situací zůstane
(kolize >1 blitz). Konflikty: jediný blitz si konkuruje mezi (i) uvolněním
rohu, (ii) čištěním koridoru — data říkají, že (ii) je častější potřeba,
(iii) sack/škodicím blitzem ze search() a (iv) budoucí BLITZER rolí
(taxonomie 4 účelů blitzu, uživatel 03.08.). Release smí blitz čerpat, jen
není-li rezervován vyšší prioritou. Acceptance: jako R-A + kvalitativní
archiv „blitz utracen na release vs. ušlé sack příležitosti" (metodika
29.07).

### R-C: plná alokace rolí (BLITZER → asistenti → fauler → rohy)
Release jako součást celo-tahového alokátoru dle pořadí z 03.08.; účel
blitzu se volí z taxonomie, rohy dostávají zbytková těla. Koncepčně
nejčistší (řeší roh–koridor konflikt globálně), ale kalibr blitz-D1
diskuze — nezačínat před ní. Acceptance: nejdřív design review
s uživatelem; tento report je podklad.

Doporučené pořadí: R-A → R-B, R-C až po blitz D1 diskuzi („jedna změna
najednou"). Pozn.: data naznačují i R-A′ (route-around bez boje, závěr 5)
jako levný doplněk nezávislý na blitz diskuzi.

## Checklist závazných constraintů (poučení z MAX_STEP incidentu)

| Constraint | R-A | R-B | R-C |
|---|---|---|---|
| Tempo = výpočet, ne konstanta | OK; tryAssign PŘEPOČÍTAT po rezervaci blockera (tělo mimo pool může snížit achievable step) | totéž + blitzer | alokátor řeší globálně |
| Klec vždy na cílovém poli carriera | OK — release nemění geometrii slotů | OK | OK |
| Pořadí exekuce situační (dependency) | dependency sort rozšířit o hranu „block předchází move přes uvolněné pole" | totéž; blitz noha (pohyb) první | totéž |
| Bank-while-clear | po projekci bloku přepočíst resistance (koridor se čistí → bank povolen); release nikdy nekupuje GFI | totéž | totéž |
| Carrier GFI jen v nouzi | carrier se release neúčastní (není blocker) — uvést explicitně | totéž (ani jako blitzer) | totéž |
| „Postav správnou klec vždy" | release PODŘÍZENÁ stavbě: substituce zdarma > block > blitz | totéž | totéž |
| RESERVE_TURNS=1 | beze změny (vše v rámci tahu) | beze změny | beze změny |
| Žádná nová konstanta bez výpočtu | počet release bloků = pokrytí markerů distinct blockery (z dat), NE „max 1 block/turn" | jediný blitz = pravidlo hry, ne návrhová konstanta | priority = pořadí rolí, ne váhy |

## Metodika a limity

- Engine: main 51c1aa0 + merge `f1-cage-fix` (worktree
  agent-acc24c039cd798d6e), testy 493/493 i po přidání diag polí do
  `CageAdvancePlan` (jen diagnostika, žádná změna chování; DICEY nyní nese
  selhavší nohu + pto/ceil + plánované nohy).
- Sonda: rozšířený `diag_f1_adoption_probe.cpp` (v worktree; kompilace dle
  hlavičky souboru). Běhy `nice -n 19`, max 2 souběžně, ostrá iterace
  (PID 67125) nedotčena. Logy: scratchpad `probe_mu{0..3}.log`.
- Horní odhady: (1) po uvolnění markera může selhat DALŠÍ noha plánu —
  reálná konverze bude nižší; (2) block modelován jako odstranění TZ, push
  geometrie ignorována (push-only může marker jen posunout); (3) blitz práh
  fail ≤ 0,35 je volba — přísnější práh čísla sníží; (4) N=24 her, podíly
  ±6 pp (binom. SE na n=62).
- Kategorie d (4×) a carrier-leg selhání (10×) release vrstva neřeší —
  správně zůstávají na search() fallbacku.
