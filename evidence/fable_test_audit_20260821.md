# FABLE AUDIT — TESTY: CO NÁM VŮBEC HLÍDAJÍ? (21.08.2026)

Zadání: `evidence/fable_brief_test_audit_20260821.md`. Vstup:
`evidence/fable_rules_parity_20260821.md` (13 nálezů F1-F13).
Edice **BB2016** (`rules_bb2016.txt`, čísla řádků odtud).

Stav: **HOTOVO** (viz konec souboru).

## JMENOVATELE (nahoře pro rychlé čtení, souhrn na konci)

* Testů celkem: **577** ve 31 souborech (`grep -c "^TEST" engine/tests/test_*.cpp`);
  brief uváděl 572 — od rána přibyly testy F1/F2 oprav.
* Řádků s odkazem na pravidla ve všech testech: **40**, z toho většina cituje
  CRP/LRB6, tj. po nálezu z 20.08. špatný zdroj; BB2016 s čísly řádků citují
  jen dnešní opravy (P45, F1, F2) — vzor, kterým se řídí doporučení níže.

## ČÁST 1 — „PROČ TO NECHYTIL TEST?" (F1-F13 z rules-parity)

Prošel jsem test k místu KAŽDÉHO z 13 nálezů. Vzor je jednoznačný — tři mechanismy selhání:

**Mechanismus T-I: test CERTIFIKUJE vadu** (aserce souhlasí s chybným kódem):

* **F1 stun** — `GameState.ResetPlayersForNewTurn` (původní znění, git `4ff6f6d6`):
  `p.state = STUNNED; reset; EXPECT_EQ(p.state, PRONE)  // stunned → prone`.
  Komentář „stunned → prone" je přepis KÓDU, ne pravidla (ř. 703-710 žádá flip
  na KONCI příštího kola). Test byl 21.08. opraven spolu s kódem.
* **F2b faul dublety** — původní `test_foul_handler.cpp` používal kostky
  `{5,4,3,3}` v ČTYŘECH testech: injury 3+3 JE dublet (ř. 1878-1882 → vyloučení
  + turnover), a testy asertovaly průchod bez vyloučení. Vada byla certifikovaná
  4×. Opraveno 21.08. (kostky změněny na `{...,3,4}` s citací l. 1878).
* **P45/StandUpNotEnoughMA** — doloženo v briefu, původní znění potvrzeno
  z gitu (`EXPECT_FALSE(result.success)` pro MA2, žádný hod 4+; ř. 691-693).

**Mechanismus T-II: test testuje jen HAPPY PATH, který s pravidly souhlasí —
chybí negativní případ, a právě v něm je vada:**

* **F3 Frenzy** — `BlockHandler.FrenzyDoubleBlock` testuje PUSHED → druhý blok
  (správně). Komentář ale zní *„Both still standing + adjacent → mandatory 2nd
  block"* — to je PODMÍNKA Z KÓDU, ne z pravidel (ř. 8138-8140: jen po
  Pushed/DS). Chybí test „po BOTH DOWN druhý blok NENÍ". Komentář vadné
  kritérium dokonce slovně fixuje.
* **F11 Wrestle** — `BlockHandler.WrestleBothProne` asertuje `no turnover` pro
  ne-nosiče (správně). Chybí případ „aktivní hráč držel míč → turnover"
  (ř. 8677-8678). Kód vrací ok() bezpodmínečně a žádný test to nezpochybní.
* **F7 fumble** — test se JMENUJE `FumbleOnNatural1`: název sám fixuje špatnou
  sémantiku (ř. 1742-1745: modifikovaný výsledek ≤ 1). Scénář (přirozená 1 bez
  modifikátorů) pravidlům neodporuje — chybí test „hod 2 s modifikátorem −1 =
  fumble", a ten by kód shodil.
* **F9 throw-in** — 4 testy (`ThrowInSideExit*`, `ThrowInCornerExit`,
  `ThrowInFinalBounceOntoPlayer`) pokrývají výhradně dráhy, kde se kód
  s pravidly shoduje (dopad na prázdné pole → bounce, ř. 872-874). Nepokrytý je
  přesně (a) dopad na STOJÍCÍHO hráče → má být catch (ř. 871-872), (b) míč
  letící znovu ven → má se vhodit ZNOVU (ř. 875-877), u nás klamp. Navíc
  komentáře citují **LRB6**, ne BB2016 (obsahově tu shodné, ale špatný zdroj —
  táž rodina jako zápis ze 07.08.).

**Mechanismus T-III: test je VAKUÓZNÍ (nic neasertuje) nebo asertuje neurčitě:**

* **F8 rozptyl passu** — `PassHandler.InaccuratePassScatters` má v kostkách
  NAKÓDOVANOU vadnou šablonu (D8 směr × D6 vzdálenost, komentář „Scatter:
  D8=3(E) D6=1") a pak asertuje `EXPECT_GE(result.turnover + result.success, 0)`
  — **vždy pravda**. Komentář v testu: *„This test verifies the pass completes
  without crash."* Totéž `HailMaryPassScatters3Times` (*„Just verify
  completes"*). Dva testy klíčové mechaniky = smoke testy s nulovou asercí.
* **F10 výkop** — `KickoffHandler.BallScatterStaysOnPitch` asertuje „míč nikdy
  neskončí mimo hřiště" — to projde i po opravě (touchback → isHeld), ale
  NEUMÍ vadu chytit; test na touchback při rozptylu ven (ř. 279-281) neexistuje.
  `ThrowARockStunsPlayers` čeká jen KNOCKED_DOWN event (kód dělá jen stun; F13),
  `PitchInvasionCanStunPlayers` je výslovně „Just verify it doesn't crash".

**F12 Leap — zvláštní případ: testy certifikují MRTVÝ KÓD.** `resolveLeap` má
3 zelené testy (`test_big_guy_handler.cpp:218-260`, LeapSuccess/LeapFail/
LeapWithVeryLongLegs) — a funkce nemá žádného volajícího. Zelený test tu
vyrábí falešný signál „Leap funguje", zatímco se nikdy nezahraje. Unit test
resolver testuje, ale NIKDO netestuje, že akce je v NABÍDCE — přesně třída
„vstávání" (P45: resolveStandUp taky měl testy, a nikdo nevstal).

**F2a, F4, F5, F6:** F2a (Guard u faulu) — `Helpers.AssistsGuardInEnemyTZ`
testoval countAssists v blokovém kontextu; test „faul nesmí použít Guard"
neexistoval (dnes doplněn, `test_foul_handler.cpp:197`). F4/F5 (limit 1/kolo)
— žádný test neexistoval, dnes opraveno v kódu (`helpers.cpp:252-263`). F6
(Pass reroll na nepřesnou) — testy rerollový řetěz passu na nepřesné přihrávce
vůbec nespouštějí.

**Skóre: 13 nálezů parity → 0 chytil test.** 3× test vadu přímo certifikoval
(T-I), 6× chyběl negativní případ (T-II), 3× vakuózní aserce (T-III), 1× test
zeleně kryje mrtvý kód (F12).

## ČÁST 2 — ÚLOHA A: TESTY FIXUJÍCÍ CHOVÁNÍ BEZ OPORY V PRAVIDLECH

**Jmenovatel:** prošel jsem **~250 testů z 577** — všechny soubory dotýkající se
pravidlové mechaniky: block (50), move (20), helpers (41), pass (21), foul (12),
ball (20), kickoff (13), big_guy (23), ttm (8), gaze (4), bomb (6),
ball_and_chain (6), game_state (8), turn_handler (3), rules_engine (12),
injury (21, s oporou v parity auditu), + vzorky z game_simulator (46),
action_resolver a enums. **Vynecháno ~250 testů AI/aparátu** (macro_actions 81,
cage_advance 29, turn_planner 22, macro_mcts 20, feature_extractor 15,
policy_network 14, mcts 10, action_features 9, value_function 8, dice 7,
position 7, player 7, zbytek game_simulator) — ty netvrdí nic o pravidlech.

### 2a ⛔⛔ NOVÉ PRAVIDLOVÉ VADY CERTIFIKOVANÉ TESTEM (nejsou v F1-F13)

**TA1 ⛔⛔ ŘETĚZ REROLLŮ TÉHOŽ HODU — JEDINÁ NOVÁ VADA ŽIVÁ V KORPUSU**
* Pravidlo: ř. 925-928 *„VERY IMPORTANT: No matter how many re-rolls you have,
  or what type they are, **you may never re-roll a single dice roll more than
  once**"* + ř. 950-951 totéž; a ř. 8385-8388 (Pro): po neúspěšné Pro bráně
  (1-3) *„the original result stands and **may not be re-rolled with a skill or
  team re-roll**"*.
* Kód: `helpers.cpp:241-300` (attemptRoll) — kaskáda skill reroll → Pro → team
  reroll na TÉMŽE hodu, každý stupeň se spustí po selhání předchozího. Dnešní
  oprava F4/F5 (limit 1/kolo) kaskádu NEŘEŠÍ.
* Test, který to certifikuje: `Helpers.AttemptRollFullChain`
  (`test_helpers.cpp:367`) — kostky {2,1,4,2,5}: fail → Dodge reroll fail →
  Pro gate pass → Pro reroll fail → team reroll success, `EXPECT_TRUE(ok)`.
  **Tři rerolly jednoho hodu, pravidlo dovoluje jeden.**
* Dopad na korpus: Pro nemá v rosterech nikdo (ověřeno `roster.cpp`), takže
  živá větev je **skill reroll → team reroll** (2 rerolly téhož hodu) — každý
  neúspěšný dodge/GFI/pickup se skillem A týmovým rerollem dostává třetí šanci.
  Nadržuje dodge rasám (WE/skaven/human), tj. deformuje hlavní pár dw-we.

**TA2 ⛔ TAKE ROOT — hod se hází JEN na MOVE a zakořenění NEPERZISTUJE
(Treeman = KORPUS)**
* Pravidlo ř. 8572-8584: hod D6 *„Immediately after declaring **an Action**"*
  (= KAŽDOU akci vč. BLOCK); na 1 **MA=0 až do konce drivu** (nebo sražení),
  zakořeněný nesmí být odsunut, smí blokovat bez follow-upu, po failu při
  BLITZi nesmí ten tah blokovat.
* Test `BigGuyHandler.TakeRootOnlyMove` — komentář *„Block: no roll needed"*,
  prázdné kostky, `EXPECT_FALSE(actionBlocked)` ⇒ certifikuje, že Treeman
  bloky hází BEZ rizika. Perzistentní stav zakořenění v kódu neexistuje vůbec
  ⇒ Treeman „zakořeněný" v jednom kole příští kolo normálně chodí.
* Korpus: Treeman je v WE rosteru ⇒ ŽIVÉ (byť malé — Treeman se hýbe málo,
  ale blokuje často; riziko 1/6 na každou BLOCK akci se mu promíjí).

**TA3 ⛔ THROW TEAM-MATE — pět rozporů, celý handler stojí na míčové šabloně**
(mechanismus A z parity auditu: recyklace passu i tam, kde má TTM výjimky)
* ř. 8608-8613: *„**accurate passes are treated instead as inaccurate** …
  scattering the player **three times**"* — test `TTMHandler.AccurateLanding`
  certifikuje dopad PŘESNĚ na cílové pole; `InaccurateScatter` scatteruje JEDNOU.
* ř. 8614-8615: *„A fumbled team-mate will **land in the square he originally
  occupied**"* — test `TTMHandler.Fumble` scatteruje z pozice HÁZEJÍCÍHO a
  dokonce asertuje `EXPECT_NE(pozice, původní)` = **pravý opak pravidla**;
  druhá aserce je tautologie `success || !turnover || turnover`.
* ř. 8430-8431 (Right Stuff): *„A failed landing roll or landing in the crowd
  **does not cause a turnover, unless he was holding the ball**"* + katalog
  turnoverů ř. 378-381 (bod 6 jen s míčem) — testy `FailedLanding` a
  `OffPitchTurnover` asertují `EXPECT_TRUE(result.turnover)` bez míče.
* ř. 7782-7795 (Always Hungry): sežrání chce **DVĚ jedničky po sobě** (a je to
  smrt bez záchrany); jedna 1 + 2-6 = fumble. Test `AlwaysHungryEat` sežere
  po JEDNÉ jedničce a dá stav INJURED.
* ř. 8606-8607: házející **odečítá 1 od hodu** — v `ttm_handler.cpp:71-78`
  modifikátor chybí (testy to nevidí, hody zvoleny mimo hranici).
* Korpus: v pěti hlavních rosterech není žádný Right Stuff ⇒ latentní; goblin
  roster ale existuje (`roster.cpp:299+`).

**TA4 ⛔ BOMBARDIER — test se JMENUJE „NeverTurnover" a pravidlo říká opak**
* ř. 7956-7958: *„**Fumbles or any bomb explosions that lead to a player on
  the active team being knocked over ARE turnovers.**"* — test
  `BombHandler.NeverTurnover` (komentář *„Even if fumble with ball carrier,
  never turnover"*) certifikuje pravý opak.
* ř. 7967-7968: fumble → bomba vybuchne **v poli házejícího** — test
  `BombHandler.Fumble` certifikuje scatter D8 z pole házejícího.
* ř. 7969-7972: v poli výbuchu jde k zemi KAŽDÝ (házející nemá výjimku),
  sousedi na 4+ — test `BombHandler.ThrowerImmune` certifikuje vymyšlenou
  imunitu házejícího. Latentní (goblin).

**TA5 ⛔ HYPNOTIC GAZE — fail není turnover a hod nemá být 2+TZ, ale Agility**
* ř. 8178-8189: *„Make an **Agility roll** … If the roll fails, then the
  hypnotic gaze **has no effect**"* — žádný turnover; katalog turnoverů gaze
  nezná. Test `GazeHandler.FailureTurnover` asertuje `turnover == true`.
* Kód počítá cíl jako `min(6, 2+TZ)` bez ohledu na AG — pro AG4 upíra to
  náhodou vychází stejně, pro jiné AG ne; test `SuccessLosesTZ` to certifikuje
  na hráči s **AG3** (cíl 3 místo správných 4). Latentní (vampire roster
  existuje, `roster.cpp:229`, v korpusu nehraje).

**TA6 ⛔ BALL & CHAIN — čtyři rozpory, celý handler je vymyšlená mechanika**
* ř. 7810-7814: směr = **throw-in šablona (3 směry) + D6**, trenér volí
  orientaci — kód/testy: D8 do 8 směrů bez volby.
* ř. 7815-7817: mimo hřiště = **„beaten up by the crowd"** (hod na zranění
  davem) — test `OffPitchKO` certifikuje automatické KO bez hodu.
* ř. 7827-7830: sražen → **okamžitě injury BEZ armouru**, Stunned se čte jako
  KO — test `BCDownStops` certifikuje armour 3+3 a stav PRONE.
* Katalog turnoverů ř. 368-369 bod 1: sražení hráče aktivního týmu NA HŘIŠTI
  = turnover (B&C výjimku nemá) — testy `BCDownStops`/`NeverTurnover`
  certifikují „nikdy turnover". (Mimo hřiště bez míče je to správně — „injured
  by the crowd" turnover není a Fanatic má No Hands.)
* ř. 7822-7825: Prone hráči v cílovém poli se odsunou + armour — kód je
  přeskakuje (komentář v `AutoBlockOccupied`). Latentní (goblin Fanatic).

**TA7 ⛔ CHAINSAW — chybí +3 na armour a kickback sráží i při neproražení**
* ř. 8001-8005: hod na zbroj zasaženého (oběti I nositele při kickbacku)
  **+3**; neprorazí-li, útok **nemá žádný účinek**.
* Kód `block_handler.cpp:414-437`: +3 nikde; kickback sráží nositele a vrací
  turnover BEZ ohledu na výsledek zbroje. Test `BlockHandler.ChainsawKickback`
  certifikuje obojí: kostky {1,3,3} → 6 ≤ 8 „not broken", a přesto
  `EXPECT_TRUE(result.turnover)` + `PRONE`. (S +3 by 9 > 8 prorazilo a test by
  spadl na vyčerpaných kostkách.) Latentní (goblin Looney).

**TA8 ⛔ TENTACLES — jiná kostková mechanika:** ř. 8585-8591 žádá **2D6** +
ST utíkajícího − ST chapadel, ≤5 chycen; kód/testy (`MoveHandler.
TentaclesCaught/Escape`) hrají **D6 proti D6** (opposed roll). Jiné
pravděpodobnosti. Latentní (nikdo v rosterech).

**TA9 ⛔ SHADOWING — obrácená a zjednodušená formule:** ř. 8455-8464 žádá
**2D6** + MA utíkajícího − MA stínujícího, **≤7 stínující následuje**; kód/testy
(`MoveHandler.ShadowingFollow/Fail`) hází JEDNU D6 + MA stínujícího − MA
utíkajícího ≥ 6. Latentní (nikdo v rosterech).

**TA10 ⛔ BLOOD LUST — kousnutí je injury roll, ne auto-KO:** ř. 7929-7947:
kousnutý Thrall dostává **Injury roll** (casualty čti jako Badly Hurt), upír
bez Thralla jde do **RESERVES a je to TURNOVER**. Testy
`BigGuyHandler.BloodlustBiteThrall/NoThrall` certifikují auto-KO Thralla a KO
upíra bez turnoveru. Latentní (vampire).

### 2b ➖ VOLBY ENGINU FIXOVANÉ TESTEM JAKO „MECHANIKA" (mají být označené)

* `BlockHandler.PushbackBasic` — asertuje **povinný follow-up** („Attacker
  follows up", `EXPECT_EQ(pozice, {11,7})`). Pravidla follow-up dávají NA VÝBĚR
  — tohle je přesně **P44** (dvojrole blitzujícího se sama ruší). Test tu volbu
  fixuje jako fakt bez poznámky, že je to volba.
* `BlockHandler.SideStepStillHasToLandOnAnEmptySquare`,
  `DefenderChoosesWorstForAttacker`, `AutoChoosePrefersDDOverAD`, PushGeometry
  sada — fixují POLITIKU výběru (legitimně), ale jen PushGeometry to o sobě ví.
* `KickoffHandler.BallScatterStaysOnPitch` — asertuje „míč nikdy neskončí mimo
  hřiště" — po opravě F10 (touchback) by prošel taky, ale vadu chytit neumí;
  jediné, co fixuje, je klamp.

### 2c ⚠️ VAKUÓZNÍ TESTY (aserce, která nemůže selhat)

| test | aserce |
|---|---|
| `PassHandler.InaccuratePassScatters` | `EXPECT_GE(turnover+success, 0)` — vždy pravda |
| `PassHandler.HailMaryPassScatters3Times` | totéž („Just verify completes") |
| `TTMHandler.Fumble` | `success \|\| !turnover \|\| turnover` — tautologie |
| `KickoffHandler.PitchInvasionCanStunPlayers` | jen „doesn't crash" |
| `KickoffHandler.MultipleKickoffsInGame` | `EXPECT_GT(totalActions, 0)` |

Právě pod dvěma z nich (pass scatter) leží F8.

### 2d ✅ CO SEDÍ (namátkou ověřeno proti textu)

Dodge tabulka AG + modifikátory (vč. Stunty/Two Heads/Break Tackle/Prehensile
Tail/Tackle), GFI 2+/blizzard 3+/Sprint 3×, pickup/catch, Both Down/Block/
Wrestle(bez nosiče)/Dodge-DS/Tackle, Dauntless (2 regresní testy s citací a
zdůvodněním — vzor, jak to má vypadat), Stand Firm vč. řetězu, chain push bez
stohování, Multiple Block (+2 ST, bez follow-upu), Horns, Stab (unmodified),
Bone-head/Really Stupid/Wild Animal čísla, Frenzy+GFI ekonomika blitzu,
throw-in šablona pro prázdná pole, foul (po dnešní opravě), P45 sada
(`test_action_resolver.cpp:310+` — cituje řádky a vysvětluje, jediný soubor,
kde je to standard).

⚠️ Zdrojová hygiena: z ~40 citací pravidel v testech je většina **„CRP"**
(`test_injury`, `test_game_simulator`, `test_block_handler`, `test_ball_handler`
„LRB6") — po nálezu z 20.08. (hrajeme BB2016, ne CRP) jsou to všechno špatně
označené zdroje, i když text je většinou obsahově shodný.

## ČÁST 3 — ÚLOHA B: DOVEDNOSTI A AKCE BEZ TESTU

**Jmenovatel: 75 hodnot `SkillName`** (`engine/include/bb/enums.h`), prověřeno
grepem proti `engine/src/*.cpp` (mimo roster.cpp) a `engine/tests/test_*.cpp`.

**Mrtvé hodnoty enumu — v celém `src/` se nevyskytují (rodina T5.15):**
`PilingOn` (známé, T5.15) · `Leader` · `DumpOff` · `Animosity` · `PassBlock` —
**5 mrtvých dovedností**. Pozn.: orc/human throwery bez implementovaného
Leader/DumpOff = tichá absence, stejná třída jako Leap (F12): z logu nejde
poznat, že se „nikdy nepoužil", protože neexistuje ani event.

**Implementované, ale BEZ JEDINÉHO testu (11):**
| skill | kde v kódu | v korpusu? |
|---|---|---|
| **Catch** | `ball_handler.cpp:44`, `pass_handler.cpp:70-107` | **ANO** (human/WE/skaven catchers) |
| **Pass** | `pass_handler.cpp:279` (= místo vady F6!) | **ANO** (všechny throwery) |
| **Fend** | `block_handler.cpp:741` | **ANO** (roster :277) |
| Grab | `block_handler.cpp:356` | ne |
| DivingTackle | `helpers.cpp:114` — ⚠️ jen pasivní modifikátor; BB2016 žádá VOLBU a cenu (hráč jde k zemi) | ne |
| KickOffReturn | `kickoff_handler.cpp:284-290` | ne |
| DivingCatch | `helpers.cpp:153` | ne |
| Titchy | `helpers.cpp:94` | ne (goblin) |
| NurglesRot | `injury.cpp:170` | ne |
| Stakes | testován jen přes `ctx.hasStakes`, zapojení skillu z hráče netestováno | ne |
| SneakyGit | foul path | ne (?) |

⭐ Vzor se opakuje: **F6 (pass reroll jen na přirozenou 1) sedí přesně v místě,
kde skill Pass nemá žádný test.** Netestovaný skill v korpusové rase je
nejlevnější předpověď příští vady.

**Akce:** všech 11 herních `ActionType` má aspoň nějaký test resolveru; chybí
ActionType pro **LEAP** (F12 — resolver bez volajícího, testy zelené).

## ČÁST 4 — ÚLOHA C: N/A KONTROL (rodina T2.18, ne nová položka)

Stav v `diag_rules_checks_20260812.py` je po opravách z 13.-20.08. **lepší, než
brief předpokládá**: třída `Check` má `deg` (degenerované) a tiskne
`N/A (x %)` i „0 posuzováno, N degenerovaných"; vedle toho slovník `st[]`
u většiny skipů zapisuje DŮVOD (ř. 395-546). Artefakt K9c (TD-kola vyhozená
z jmenovatele) je opraven a zdokumentován přímo v kódu (ř. 373-384).

Co zbývá (patří pod T2.18):
1. **Důvod není součást typu** — `Check.skip()` je jeden čítač bez důvodu;
   důvody žijí v ad-hoc `st[]`, takže nová kontrola je může zase vynechat.
2. **Nejmíň jedna tichá cesta existuje dál**: K30, `if ref is None: continue`
   (ř. 464) — bez `skip()` i bez `st[]` ⇒ neviditelné smrštění jmenovatele.
3. **Engine-side pole (plan.\*)** — doložený případ NOT_CONSULTED ve 100 %
   (`cage_advance.h:146,172`): exportér žádné „tahle cesta vůbec žila?" pole
   nemá a noční sumarizace nulu nevaruje. To testová sada nechytí — je to
   kontrakt exportu, ne unit test.

## VERDIKT K HYPOTÉZE (brief §5)

Hypotéza *„kontrola ověřuje to, co nás napadlo ověřit; pravidlová vada je to,
co nás nenapadlo"* — **potvrzena, ale NENÍ to osud, jde z toho ven systematicky.**
Důkaz vlastním během: postup „čti PRAVIDLO → najdi jeho test → ptej se, kde je
test HRANICE" našel za jedno odpoledne **10 nových certifikovaných rozporů
(TA1-TA10)**, z toho **1 živý v korpusu (TA1 řetěz rerollů)** a 1 korpusový
s malým dopadem (TA2 Take Root/Treeman) — žádný z nich neležel v F1-F13 a
žádný by nenašlo pokrytí kódu (všechna ta místa POKRYTÁ JSOU, zeleně).

Systematika, která z toho plyne (seřazeno podle ceny):
1. **Povinná citace pravidla v testu mechaniky** — `test_action_resolver.cpp`
   sekce P45 a dnešní `test_foul_handler.cpp` jsou hotový vzor. Test bez citace
   = „naše volba" a MUSÍ to o sobě říct (jako PushGeometry sada).
2. **Test hranice, ne happy path**: každé „unless / only / instead / may not"
   v citovaném textu chce test na OBOU stranách hranice. Všech 6 případů T-II
   (F3, F7, F9, F11 + TTM/bomba) je porušení právě tohohle.
3. **Zákaz vakuózních asercí** — 5 kusů (část 2c) jde najít grepem
   (`EXPECT_GE(...+..., 0)`, tautologie, „doesn't crash").
4. **Detektor mrtvých cest**: každý resolver má mít volajícího, každý skill
   z rosteru výskyt v src (odhalí Leap, PilingOn, Leader, DumpOff…, za minutu
   grepem — dnes našel 5 mrtvých skillů).
5. **Kostky ve FixedDiceRoller jsou tvrzení**: {5,4,3,3} u faulu certifikovalo
   dublet čtyřikrát. Pravidlo: každá sekvence kostek v testu má v komentáři
   říct, KTERÝM pravidlovým hraničním případům se vyhýbá (dnešní opravy `3,4
   (ne 3,3)` to už dělají).
Strukturální zbytek: bod 2 je lidská práce nad textem pravidel — tam aparát
zůstane odkázaný na čtení. Ale 1, 3, 4, 5 jsou mechanické a levné.

## POŘADÍ PODLE DOPADU (co z tohohle auditu má cenu opravovat)

1. **TA1 řetěz rerollů** (`helpers.cpp` attemptRoll) — jediná nová vada ŽIVÁ
   v běžícím korpusu; táž měna jako F4/F5, ale větší (třetí šance na každý
   klíčový hod, nadržuje dodge rasám). Patří do pravidlového balíku k F1-F11.
2. **TA2 Take Root** — malé, ale korpusové (WE Treeman); navíc interaguje
   s čerstvým P45 (Treeman teď konečně vstává — a hned dostal i bloky bez rizika).
3. Testová hygiena: smazat/spravit 5 vakuózních testů + doplnit hraniční testy
   k F3/F7/F9/F11 (až se budou opravovat — test PŘED opravou, lekce 18.08.
   „oprava kontroly se povyšuje před měření").
4. TA3-TA10 (TTM, bomba, gaze, B&C, chainsaw, tentacles, shadowing, bloodlust)
   — latentní (goblin/vampire rosery mimo korpus); opravovat až s rozhodnutím
   ty rostery vůbec hrát; do té doby aspoň označit testy `// ⛔ ODPORUJE ř. X`.
5. Mrtvé skilly (PilingOn, Leader, DumpOff, Animosity, PassBlock) — rozhodnout
   implementovat/vyhodit; Leader je týmový reroll zadarmo — jeho absence
   zkresluje reroll ekonomiku ras, které ho mají mít.

## JMENOVATELE (souhrn)

* Testů celkem **577** (31 souborů). Plně prošlé mechanické soubory: **268
  testů** (block 50, helpers 41, move 20, pass 21, foul 12, ball 20, kickoff 13,
  big_guy 23, ttm 8, gaze 4, bomb 6, b&c 6, game_state 8, turn 3, rules 12,
  injury 21); vzorkované: game_simulator 46, action_resolver 21, enums 13
  (citované mechanické kusy). **Vynecháno ~229 testů AI/aparátu** (macro_actions
  81, cage_advance 29, turn_planner 22, macro_mcts 20, feature_extractor 15,
  policy_network 14, mcts 10, action_features 9, value_function 8, dice 7,
  position 7, player 7).
* Nálezy: **13/13 parity nálezů testy nechytily** (3 certifikace, 6 chybějících
  hranic, 3 vakuózní, 1 mrtvý kód) · **10 nových certifikovaných rozporů**
  (TA1-TA10, z toho 1 korpusový velký + 1 korpusový malý) · **5 vakuózních
  testů** · **5 mrtvých skillů** · **11 implementovaných skillů bez testu**
  (3 z nich v korpusu) · **~40 citací pravidel** v testech, většina na špatný
  zdroj (CRP/LRB6).

**HOTOVO** — audit doběhl celý (21.08.2026, Fable).
