# Technická code review přestavby cage advance (04.08.) — dopady mimo klec

Datum review: 06.08.2026 · Reviewer: Fable (read-only, žádné změny kódu)
Stav: **FINÁLNÍ**

## 1) Přehled commitů přestavby

| Commit | Datum | Obsah |
|---|---|---|
| f615e65 | 03.08. 11:21 | F1 CAGE_ADVANCE staged plán (config-gated, default off) |
| 3e04189 | 04.08. 08:47 | MA strop kroku, carrier GFI emergency, **Manhattan tiebreak v scoreMoveAction (GLOBÁLNÍ)**, gfiAllowance v expandReposition |
| a5634c7 | 04.08. 08:53 | Klec from-scratch na cílovém poli carriera |
| 0dfd11b | 04.08. 09:33 | Bank-while-clear, MA-aware výběr rohů, **TZ/sideline výjimka na cílovém poli chůze (GLOBÁLNÍ)** |
| 0fc28e7 | 04.08. 10:23 | Substituce rohu (volné tělo > marked kandidát) |
| 47bbc18 | 04.08. 12:15 | Jen diagnostika (DICEY dumpy) — bez vlivu na chování |

Klíčový kontext gatingu (ověřeno v engine/include/bb/mcts.h:30-31):
- **Cage advance samotný je za `config.cageAdvance` = default OFF** (grind `cageGrind` dtto) — cage-lokální nálezy se v produkci projeví až při zapnutí.
- **Obě globální změny v `scoreMoveAction` (engine/src/macro_actions.cpp:35-86) NEJSOU za žádným gatem** — běží teď v každé hře pro každou chůzi každého makra (SCORE, ADVANCE, PICKUP, CAGE, REPOSITION, HAND_OFF/PASS/CHAIN přiblížení, BLITZ_AND_SCORE).

## 2) Nálezy (dle závažnosti)

### NÁLEZ 1 — CONFIRMED, STŘEDNÍ-VYSOKÁ (běží v produkci): ADVANCE dovede carriera do tackle zóny bez ocenění rizika

**Kde:** engine/src/macro_actions.cpp:58-61 (výjimka cílového pole) × macro_actions.cpp:1020-1028 (expandAdvance volí cíl čistě aritmeticky).

**Mechanismus:** Výjimka z 0dfd11b: cílové pole chůze se nepenalizuje za TZ/sideline — „makro, které cíl vybralo, vlastní risk a proby ho ocení". To platí pro cage advance (má MC proby). **Neplatí pro ADVANCE**: cíl = `x + směr·kroky` (stall-aware banka) + Y ke středu, **bez pohledu na soupeře**. Padne-li cíl do tackle zóny, nikdo to neocení.

**Selhávací scénář:** HOME dwarf Runner (carrier) na (10,7), kroky=3 → cíl (13,7); soupeřův lineman na (14,7).
- Před 04.08.: skóre (13,7) = 0·10+20 (vstup do TZ z bezpečí) = 20 > (12,7) = 10 → carrier zastavil na (12,7) MIMO dosah.
- Po 04.08.: (13,7) = 0 → carrier dojde na (13,7) a **skončí tah nalepený na obránce, navíc s úmyslně ušetřeným pohybem (stall-aware banka)**. Soupeř má příští tah blok na nosiče zdarma (bez blitze), s asistencemi → knockdown, volný míč.

Všechny rasy, každé kolo s aktivním ADVANCE; pro dwarf carriera bez Dodge (trvalý cíl zlepšení) dvojnásob citlivé. Mitigace: MCTS mezi makry arbitruje a value síť výsledný stav vidí — ale expanze je deterministická a ADVANCE bývá jediné dopředné makro; search „poslední pole nedoladí".

**Doporučení:** v expandAdvance posunout cíl o 1 pole zpět, je-li v enemy TZ; nebo výjimku podmínit flagem v Macro „cíl vybral risk-aware plánovač" (analogie gfiAllowance).

### NÁLEZ 2 — CONFIRMED, STŘEDNÍ (gated, důležité pro doktrínu příští týden): feasibility klece neváží přední vs. zadní rohy — plán pošle carriera do kontaktu s „klecí" jen za zády

**Kde:** engine/src/cage_advance.cpp:142-148 (soupeř na slotu → jen „open") × cage_advance.cpp:279-290 (feasible = filled ≥ 2, bez rozlišení front/back).

**Mechanismus + scénář:** Přední diagonální sloty nové pozice (newPos.x+dx, y±1) obsadí soupeři → oba „open"; dva spoluhráči dosáhnou zadních slotů → filled=2 → **feasible**. Celý plán je dice-free (vstup do TZ nestojí hod) → PLAN_READY. Výsledek: carrier naplánovaně dojde na pole, kde stojí diagonálně PŘED ním dva obránci (jeho pole je v jejich TZ), a „klec" ho kryje jen zezadu — přesný opak účelu („front pair first — the screen the advance is for", cage_advance.cpp:102-103). Příští tah blok na carriera se dvěma útočníky vepředu.

**Doporučení:** do feasibility přidat podmínku na přední sloty (např. aspoň 1 přední roh filled, nebo soupeř na předním slotu = plán infeasible na tomto kroku, ne jen „open"). Řešit před doktrinální implementací — je to přesně vrstva „kdy má klec postupovat do kontaktu", která se příští týden bude rozšiřovat.

### NÁLEZ 3 — CONFIRMED (pozitivní vedlejší efekt, jen zdokumentovat): výjimka cílového pole opravila „nedojití" u SCORE a PICKUP

Stejná výjimka (macro_actions.cpp:58-61) opravila dva staré nehlášené defekty mimo klec:
- **SCORE:** pole endzóny hlídané obráncem mělo dřív skóre 20 (resp. 12 už-v-TZ) > 10 za zastavení o pole dřív → carrier **odmítal vstoupit do bráněné endzóny** (loop guard movePlayerToward, macro_actions.cpp:897, walk abort). Teď dojde a skóruje.
- **PICKUP:** picker dřív zastavil o pole vedle hlídaného míče a spálil aktivaci; teď došlápne a hodí si pickup (TZ postih je v hodu, riziko arbitruje MCTS).

Bez akce; uvést v paměti — vysvětluje případné posuny ve skórovacích metrikách od 04.08.

### NÁLEZ 4 — PLAUSIBLE, NÍZKÁ-STŘEDNÍ (běží v produkci): REPOSITION cíle (screen/receiver/safety) nově končí v TZ

**Kde:** macro_actions.cpp:58-61 × cíle REPOSITION volené bez ohledu na soupeře: screen před carrierem (:744-746, `carrier.x + 2·směr` — přesně kde stojí obranná linie), receiver setup u endzóny (:735-742), obranné screeny/safety (:820-846).

**Scénář:** receiver setup — přijímač skončí v TZ soupeřovy safety → příští kolo catch s −1 na chycení; screen 2 pole před carrierem skončí nalepený na obránce → snadný blok s asistencí. Není jednoznačně špatně (markování je legitimní BB chování), proto jen PLAUSIBLE — ale je to neoceněná změna chování všech screen pohybů. Sledovat metrikou (vlastní hráči končící tah v TZ mimo klec), nefixovat naslepo.

### NÁLEZ 5 — BEZ NÁLEZU (ověřeno analýzou mezí): Manhattan tiebreak je bezpečně ohraničený

**Kde:** macro_actions.cpp:62-70 (`score += min(manhattan − chebyshev, 9)`), commit 3e04189.

Meze vůči všem složkám skóre: vzdálenost — cap 9 < 10/pole, nikdy nepřebije kratší pole ✓; TZ — rozdíl surplusu mezi sousedními kandidáty ≤ 2 < 12/20, nezatáhne chodce do TZ ✓; sideline — ≤ 2 < 6 ✓; GFI (8) — vlastnost hráče v kroku, konstantní přes kandidáty, bez interakce ✓. Selhávací scénář nelze zkonstruovat. Kosmetika: pro vzdálené diagonální cíle (min(|dx|,|dy|) ≥ 9) cap saturuje a tiebreak je inertní (staré pořadí adjacency) — není regrese.

Pozn.: pickApproachStep (blitz přiblížení, engine/src/helpers.cpp:33-51) je přestavbou **nedotčen** — nemá Manhattan ani výjimku cílového pole (blitz cíl je z definice vedle obránce). Komentované zrcadlení vah 20/12 stále platí, drift nevznikl. Druhý „Manhattan tiebreak" v draftu rohů (cage_advance.cpp:199-207) je cage-lokální a řadí jen kandidáty, ne pole — bez globálního dopadu.

## 3) Testovací mezery

1. **Obě globální změny jsou testovány výhradně přes cage testy.** Nové testy z 0dfd11b/3e04189 jsou jen v engine/tests/test_cage_advance.cpp (BankWhileClearRevertsToSchedule…, FasterCornerPreferred…). Žádný test nepinuje NE-cage chování:
   - chybí test „ADVANCE s cílem v enemy TZ" (zachytil by NÁLEZ 1 a pinoval budoucí fix),
   - chybí test „SCORE do bráněné endzóny projde" (pinoval by pozitivní NÁLEZ 3, dnes nechráněný proti regresi),
   - chybí přímý unit test Manhattan tiebreaku v scoreMoveAction (rovná trasa při stejné Chebyshev vzdálenosti; existující testy ho kryjí jen nepřímo přes cage).
2. Existující `ScoreAvoidsEnemyTZ` (test_macro_actions.cpp:1172-1209) asserted jen `!result.actions.empty()` — neověřuje ani trasu, ani cílové pole; změnu z 0dfd11b by nezachytil v žádné variantě.
3. Cage feasibility (NÁLEZ 2): žádný test nepokrývá „soupeři na obou předních slotech" — testy staví jen prázdné/spoluhráčské sloty.

## 4) Sekundární témata (prošlo bez nálezu + úklid)

- **GFI prahy SAFE_PTO_GFI1=0.25 / GFI2=0.40** (cage_advance.cpp:484-486, konstanty cage_advance.h:146-156): matematika sedí — teoretické fail raty 1 GFI ≈ 0,167 / 2 GFI ≈ 0,306, sd MC odhadu při K=48 ≈ 0,054-0,067, prahy ≈ +1,5 sd. Ceiling se aplikuje jen na makro s `gfiAllowance` > 0 a to nastavuje výhradně carrier leg plánovače (cage_advance.cpp:442) — únik do běžných REPOSITION makro nehrozí (brace-init bez gfiAllowance). Drobná známá tolerance: ~+9pp skrytého rizika navíc (např. jediný 6+/AG5 dodge) může pod GFI2 stropem prošumět — v mezích designové filozofie 1,5 sd, bez akce.
- **Dependency sort** (cage_advance.cpp:446-474): stabilní, řetězy A→B→C řadí správně (kontrola proti AKTUÁLNÍM pozicím neumístěných moverů; carrier je v seznamu, stayPut hráči na cizí cíle geometricky nemohou). Cyklus (vzájemný swap) → fallback na base order → chodec nedojde → probe/exec fáze plán shodí do DICEY (miss > allowed, :520-524) → fallback na search. Žádné tiché selhání.
- **Vacate-fix** (carrierBlocker, cage_advance.cpp:85-99 a :249-277): korektní; blocker bez slotu = plán infeasible (konzervativní). Latentní drobnost k úklidu: `dest->needsGfi = gfi` na řádku :269 mutuje needsGfi i na kandidátním slotu, který je pak nahrazen bližším — zbylý slot má playerId=-1, takže se nikdy neemituje ani nezapočítá (dnes neškodné), ale při budoucím čtení slots[] to může překvapit.
- **From-scratch feasibility** (cage_advance.cpp:313-317, a5634c7): „no minimum on already-built corners" je konzistentní s uživatelským standardem; pojistka nedegradace (minFilled = min(built,3), :289) funguje. Jediný reálný defekt je NÁLEZ 2 (front/back nevážení). Pozn.: `built` počítá i skill-neeligible rohy (Ogre) → jen konzervativnější, bez scénáře selhání.

**Úklid dokumentace (není nález):**
- cage_advance.h:161-163 — **potvrzený zastaralý komentář**: „valid=false unless … >= TRIGGER_MIN_CORNERS diagonal corners STAND around the carrier" — od a5634c7 se minimum už-postavených rohů nevyžaduje; TRIGGER_MIN_CORNERS platí pro sloty NAPLNĚNÉ po přesunu.
- cage_advance.h:93 — `int step … (1..2)` — zastaralé, strop je teď MA+2 (3e04189).
- cage_advance.h:112-113 — „carrier LAST" popisuje jen base order; dependency sort může pořadí změnit.
- cage_advance.h:60-63 — „REPOSITION … deliberately never GFIs (expandReposition caps at movementRemaining)" — od 3e04189 platí jen pro makra bez gfiAllowance; doplnit zmínku o výjimce.

## 5) Doporučení

**Opravit před doktrinální implementací příštího týdne:**
1. **NÁLEZ 1** (produkce, teď): TZ-check cíle v expandAdvance — malá lokální oprava, zachová výjimku tam, kde riziko někdo ocenil. + regresní test „ADVANCE s cílem v TZ".
2. **NÁLEZ 2** (před zapnutím cage advance): front-slot podmínka ve feasibility — přesně vrstva, na kterou doktrinální práce naváže (uvolňování rohů / postup do kontaktu).
3. Doplnit 3 testy ze sekce 3 bodu 1 (NE-cage cesty globálních změn) — obě globální změny dnes nemají žádnou pojistku proti regresi mimo klec.

**OK / bez akce:**
- Manhattan tiebreak (meze ověřeny, bez scénáře selhání), pickApproachStep bez driftu.
- GFI prahy, dependency sort, vacate-fix, from-scratch pojistka nedegradace.
- NÁLEZ 3 zdokumentovat (pozitivní změna — SCORE/PICKUP nedojití opraveno), NÁLEZ 4 jen sledovat metrikou.
- Úklid komentářů v cage_advance.h (4 místa výše) při příštím dotyku souboru.
