# AUDIT PARITY POHYBU PROTI rules_bb2016.txt — Fable, 24.08.2026

Zadání: `evidence/fable_brief_movement_parity_20260824.md`. Audit čtením, žádné změny kódu.
Každé tvrzení o pravidlech je doloženo číslem řádku z `rules_bb2016.txt` (BB2016).

## Pracovní deník

- [x] 0. Zmapovat pohybové soubory v engine/ (kde je resolver, plánovač, pathfinder)
- [x] 1. (a) Základ pohybu: MA a spotřeba, GFI (2+, blizzard 3+), Sprint, Sure Feet, TZ, pathfinder (strop GFI, zakořenění)
- [x] 2. (b) Vstávání: cena 3 MA, MA ≤ 2 ⇒ 4+, Jump Up, Take Root interakce, „stand-and-go"
- [x] 3. (c) Dodge: tabulka AG, modifikátory, Tackle, Break Tackle, Two Heads, Prehensile Tail, Stunty, Diving Tackle, rerolly
- [x] 4. (d) Nevlastní pohyb: odsun/chain push, Stand Firm, Side Step, Grab, Frenzy, Shadowing, Tentacles (2D6), Leap (F12), Ball & Chain, Throw Team-Mate
- [x] 5. (e) Brány: Bone-head, Really Stupid, Wild Animal, Take Root, Blood Lust — spotřeba aktivace, P55 blitz
- [x] 6. (f) Nabídka akcí: grep všech resolve* v pohybových souborech + dohledání volajících
- [x] 7. Tabulka nálezů + oba speciální seznamy
- [x] 8. Pořadí oprav podle dopadu + „hraje v korpusu?" (dwarf, skaven, wood-elf, human, orc)
- [x] 9. Co nejde rozhodnout čtením (zadání na měření)

## Nálezy — tabulka

| # | co je špatně | pravidlo (řádek) | náš kód (soubor:řádek) | odhad dopadu | hraje to v korpusu? |
|---|---|---|---|---|---|
| N1 | Dodge skill dává −1 k cílovému číslu dodge **a k tomu** reroll. Pravidla dávají JEN reroll jednoho neúspěšného dodge za kolo — žádný modifikátor hodu. Každý dodge hráče s Dodge je o 1 pip lehčí, než má být (a Tackle pak „maže" modifikátor, který vůbec nemá existovat). | ř. 8086-8090 (Dodge = re-roll; jediné modifikátory dodge jsou ř. 578-584: +1 za hod, −1 za TZ na cíli) | `engine/src/helpers.cpp:77-91` (−1 target) + `move_handler.cpp:147-148` (tentýž hod dostává i reroll) | VELKÝ — zdvojená výhoda systematicky nadhodnocuje uhýbání (stejným směrem jako známá vada TA1 v korpusu) | ANO — skaven (4 Gutter Runneři), human (2 Catchers), wood-elf (2 Wardanceři + 2 Catchers); orc a dwarf TV1200 Dodge nemají |
| N2 | Stunty = paušální −1 k cílovému číslu. Pravidla: Stunty **ignoruje TZ na cílovém poli** (ekvivalent −1 za každou TZ na cíli, a nic navíc při 0 TZ). Kód tedy Stunty hráči při dodge do prázdna nelegálně ulehčuje o 1 a při dodge do 2+ TZ ho naopak trestá. | ř. 8530-8533 („may ignore any enemy tackle zones on the square he is moving to") | `engine/src/helpers.cpp:93` | STŘEDNÍ tam, kde Stunty hraje | NE — orc TV1200 gobliny nestaví (`roster.cpp:499-517`); latentní |
| N3 | Diving Tackle: −2 se přičítá **automaticky a vždy**, jakmile vedle stojí kdokoli s DT — bez volby po hodu, bez položení DT hráče do opuštěného pole (známá půlka z briefu) a bez toho, aby DT bylo jednorázové „use". Pravidla: volitelné, po hodu (i po rerollu), DT hráč je Placed Prone do uvolněného pole. | ř. 8072-8085 | `engine/src/helpers.cpp:108-118` | STŘEDNÍ — DT zdarma penalizuje každý dodge; cena (prone DT hráče) neexistuje | NE — žádná z 5 TV1200 sestav DT nemá (`roster.cpp:503-596`); latentní |
| N4 | Break Tackle bez limitu „once per turn": kód použije ST místo AG při každém dodge v kole. | ř. 7987-7991 („This skill may only be used once per turn.") | `engine/src/helpers.cpp:57-59` | malý | NE — v TV1200 sestavách ho nikdo nemá (Deathroller s Break Tackle je z dwarf TV1200 vyřazen, `roster.cpp:538-558`); latentní |
| N5 | Titchy protihráč má vyzařovat dodge-postih „bez −1" (nedává −1 při dodge DO jeho TZ); kód počítá jeho TZ na cílovém poli normálně. | ř. 8639-8642 | `engine/src/helpers.cpp:72-73` (countTacklezones bez výjimky pro Titchy) | malý | latentní (Titchy v 5 rasách korpusu není) |
| N6 | Leap neprovádí kontrolu Tentacles ani Shadowing, přestože pravidla Tentacles výslovně jmenují „dodge **or leap** out of any of his tackle zones" a Shadowing platí při opuštění TZ „for any reason". | ř. 8586-8587 (Tentacles), ř. 8456-8458 (Shadowing) | `engine/src/move_handler.cpp:219-312` (resolveLeap — žádné checkTentacles/checkShadowing) | latentní dvojnásob (Leap je mrtvý kód, F12) | NE |
| N7 | Leap s deficitem 2 MA hází jen JEDEN GFI hod; pravidla: hod po **každém** extra poli (dvě GFI pole ⇒ dva hody). | ř. 1701 („Roll a D6 for the player after they have moved each extra square.") | `engine/src/move_handler.cpp:237-245,276-291` | latentní (mrtvý kód F12) | NE |
| N8 | **BOTH DOWN odsouvá obránce a dovoluje follow-up.** Pravidla: Both Down kácí OBA NA MÍSTĚ — žádný push, žádný follow-up (odsun mají jen Pushed/DS/DD, ř. 521-533). Kód při BD u obránce bez Blocku nastaví `defPushed=true` („defender gets pushed then knocked down") a projde celou push+follow-up větví: obránce padá o pole dál, útočník obsadí jeho pole; navíc se tak nelegálně aktivují Stand Firm/Side Step/chain push a bounce míče do odsunu, který nemá existovat. | ř. 514-520 (BD) vs. ř. 521-533 (push jen u P/DS/DD) | `engine/src/block_handler.cpp:717-719` (defPushed=true) → `:768-877` (push+follow-up) | VELKÝ — BD proti obránci bez Blocku je častá volba (scoreFace ji cení 8/10); každý takový blok posune dva hráče o pole, kam nemají; deformuje klece, sacky i geometrii měřenou celým výzkumem | ANO — všech 5 ras, prakticky každý zápas |
| N9 | Tentacles „held firm ⇒ his action ends immediately" ukončí jen BLITZ smyčku (no-progress guard). U obyčejného pohybu nic aktivaci neukončí: `movePlayerToward` chyceného hráče zkouší tentýž krok znovu a KAŽDÁ iterace hází nový 2D6 únik — chycení se degraduje na „zkoušej, dokud neutečeš nebo nedojde maxSteps". | ř. 8590-8591 („the moving player is held firm, and his action ends immediately") | `engine/src/move_handler.cpp:38-42` (jen `hasMoved=true`, nic víc) + `macro_actions.cpp:1201-1226` (retry smyčka) | STŘEDNÍ tam, kde Tentacles hrají | NE (Tentacles v 5 TV1200 rasách nikdo nemá) — latentní |
| N10 | **Blitzer nesmí pokračovat v pohybu po bloku.** Pravidla: „The block may be made at any point during the move" (blok kdykoli během pohybu, pak lze dojít zbytek MA). `resolveBlock` nastaví `att.hasActed = true` na KAŽDÉ cestě, takže po bloku aktivace končí (jen follow-up). Vzor hit-and-run i „nosič si blitzem otevře cestu a doběhne" jsou nemožné — v `expandBlitzAndScore` s nosičem-blitzerem krok 2 tiše umře na `canAct()==false`. | ř. 347-350 („He may make one block during the move. The block may be made at any point during the move…") | `engine/src/block_handler.cpp:887` (a všechny ostatní `hasActed = true` cesty) + `macro_actions.cpp:1660-1663` | VELKÝ pro sílu hry, STŘEDNÍ pro pravidlovou paritu (engine hraje legální PODmnožinu, ale třída „akce se nikdy nestane") | ANO — každý blitz všech 5 ras |
| N11 | **`rooted` nezná pathfinder ani `resolveMoveStep`** ⇒ zakořeněnému hráči se nabídne BLITZ na nesousední cíl (`canReachAdjacentTo` počítá maxGfi 2/3 bez ohledu na `rooted`) a blitz smyčka ho pak reálně POSUNE přes GFI hody (`resolveMoveStep` má vlastní maxGfi bez `rooted`). Zákaz GFI pro zakořeněné žije jen v nabídce MOVE (`rules_engine.cpp:36`) a v blitz-bloku (`block_handler.cpp:423`). Pravidla: zakořeněný „may not Go For It… or use any skill that would allow him to move out of his current square". | ř. 8574-8580 | `engine/src/pathfinder.cpp:34` + `move_handler.cpp:121` vs. `rules_engine.cpp:34-37` | STŘEDNÍ — vzácná souhra (zakořeněný Treeman zvolen jako blitzer), ale je to pohyb, který pravidla výslovně zakazují | ANO potenciálně (wood-elf Treeman s Take Root); četnost nutno změřit |
| N12 | **Bone-head/Really Stupid: ztracené TZ a zákaz akcí se obnoví automaticky na začátku vlastního kola** (`lostTacklezones = false` v resetu), bez hodu. Pravidla: stav trvá, „until he manages to roll a 2 or better at the start of a future Action or the drive ends" — hráč, kterého trenér znovu neaktivuje (nebo znovu hodí 1), má TZ ztracené DÁL. Engine mu je vrací zadarmo každé kolo ještě PŘED novým hodem. | ř. 7983-7986 (Bone-head), ř. 8401-8405 (Really Stupid) | `engine/src/game_state.cpp:70` | malý-STŘEDNÍ — bone-headed Ogre je v soupeřově kole správně bez TZ, ale ve vlastním kole před aktivací už TZ zase vyzařuje | ANO (human Ogre) |
| N13 | **P55 trvá: propadlá akce po bráně nespotřebuje týmové limity.** `blitzUsedThisTurn`/`passUsedThisTurn`/… se nastavují až UVNITŘ `case BLITZ`/resolverů, ale big-guy brána vrací `ok()` PŘED switchem. Bone-head: „the player's team LOSES the declared Action for the turn. (So if a Bone-head player declares a Blitz Action and rolls a 1, then the team cannot declare another Blitz Action that turn.)" — totéž Really Stupid ř. 8397-8401. U Take Root pravidlo říká jen „he may not block that turn" (ř. 8582-8583) — jestli tým ztrácí i blitz, text neurčuje výslovně (deklarační pravidlo ř. 351-352 mluví pro ztrátu). U Wild Animal „the Action is wasted" (ř. 8668-8669). | ř. 7980-7983, 8397-8401, 8582-8583, 8668-8669 | `engine/src/action_resolver.cpp:47-60` (návrat před switch) vs. `:94-96` (flag uvnitř case BLITZ) | STŘEDNÍ — tým dostane druhý blitz/pass, který mít nemá | ANO (human Ogre Bone-head; WE Treeman jen v Take Root čtení) |
| N14 | Take Root roll se při vstávání zcela přeskočí (výjimka `standUpAttempt && onlyTakeRoot`). Pravidlová sekvence: hod se hází při KAŽDÉ deklaraci akce; na 1 hráč zakoření (MA 0, trvale do konce drivu) a vstávat smí hodem 4+ (MA<3). Pro Treemana MA 2 vyjde pravděpodobnost vstání stejně (50 %), ALE engine na hodu 1 nezaloží trvalý `rooted` stav — Treeman, který měl být zakořeněný do konce drivu, další kolo normálně chodí. | ř. 8573-8576 + 8583-8584 | `engine/src/action_resolver.cpp:33-48` | malý | ANO (WE Treeman) |
| N15 | Nabídky SCORE/ADVANCE/HAND_OFF_SCORE/PASS_SCORE/pickup-reach počítají dosah paušálně `movementRemaining + 2`, i pro hráče se **Sprint** (3 GFI, ř. 8488-8490). Hráč se Sprintem 3 GFI od endzóny SCORE nabídku nedostane (resolver by to uměl). Třída „akce se nenabídne". | ř. 8487-8490 | `engine/src/macro_actions.cpp:435,445,461,471,489,537,764` + `turn_planner.cpp:34` | malý | ANO (wood-elf Catchers +Sprint) |
| N16 | **Throw Team-Mate resolver se odchyluje od pravidel v ≥4 bodech:** (1) chybí −1 k hodu za házení hráče („must subtract 1 from the D6 roll when he passes the player"); (2) přesná přihrávka se NEpřehazuje na nepřesnou se **3 scattery** — engine přistane přímo na cíli, nepřesná scatteruje jen 1×; (3) fumble má hráče nechat v PŮVODNÍM poli — engine ho scatteruje od házejícího; (4) neúspěšné přistání je turnover VŽDY — pravidla: turnover jen když nesl míč. K tomu obsazené pole při dopadu: pravidla kácejí prvního hráče, na kterého dopadl (armour roll) — engine jen scatteruje dál. | ř. 8605-8624 + 8417-8431 (Right Stuff) + ř. 381-383 (turnover katalog) | `engine/src/ttm_handler.cpp:71-78,85-101,113-124,146-153` | latentní | NE (TTM v korpusu nemá kdo použít: Ogre nemá Right Stuff spoluhráče) |
| N17 | **Ball & Chain resolver: špatná šablona a špatné bloky.** Pravidla: throw-in šablona (trenér ji natočí, D6 ⇒ 1 ze 3 směrů), bloky proti obsazeným polím podle NORMÁLNÍCH blokových pravidel (počet kostek dle ST, povinný follow-up), ležící hráči v cíli se odsunou + armour roll, GFI dovoleno, KD ⇒ rovnou injury bez armouru a Stunned⇒KO. Engine: D8 náhodný směr bez volby, jednokostkový „auto-blok" bez ST/asistencí, bez push/follow-upu, ležícího v cíli PŘEKRYJE (dva hráči na jednom poli), GFI nemá, KD řeší s armour rollem. K tomu A5: akce se stejně nikdy nenabídne. | ř. 7809-7834 | `engine/src/ball_and_chain_handler.cpp:12-147` | latentní | NE |
| N18 | Blood Lust: krmení se vyhodnocuje HNED při deklaraci (soused-Thrall na začátku akce), pravidla: „at the end of the declared Action, but before actually passing, handing off, or scoring" — upír se smí k Thrallovi nejdřív DOJÍT. Engine tak hlásí turnover+reserves i tam, kde by pravidlový upír v klidu dokrmil po pohybu. | ř. 7936-7939 | `engine/src/big_guy_handler.cpp:124-188` | latentní | NE (vampire není v korpusu) |
| N19 | Timmm-ber! (asistence vstávání, +1 za volného souseda) v enginu vůbec neexistuje (žádný SkillName). | ř. 8625-8635 | enums.h (chybí) | latentní | NE |

## Seznam A: resolver bez volajícího / akce, která se nenabízí

* **A1 — Leap (F12): k 24.08. stále mrtvý kód.** `resolveLeap` (`move_handler.cpp:219`) nemá
  žádný ActionType ani makro; `rules_engine.cpp` LEAP negeneruje, `macro_actions.cpp` jakbysmet.
  Grep celého stromu: jediní volající jsou testy (`engine/tests/test_big_guy_handler.cpp:349-396`).
  Jediní nositelé Leapu v korpusu jsou oba wood-elf Wardanceři ⇒ „za celý rok neskočili" platí dál.
* **A2 — Jump Up blok z lehu se nikdy nenabídne.** Pravidla ř. 8200-8204: ležící hráč s Jump Up
  smí deklarovat Block Action s hodem AG+2. Engine nabízí ležícímu hráči JEN vstávací
  MOVE na vlastní pole (`rules_engine.cpp:222-238`); BLOCK pro ležícího neexistuje.
  Komentář v `move_handler.cpp:326-331` to sám přiznává. Latentní — Jump Up v 5 TV1200
  sestavách nikdo nemá (`roster.cpp:503-596`; má ho dark elf/norse/slann).
* **A3 — HYPNOTIC_GAZE: akce existuje (`rules_engine.cpp:186-195`), resolver existuje
  (`gaze_handler`), ale žádné MAKRO ji neemituje** (`getAvailableMacros` nemá větev pro gaze).
  Korpus hraje přes makro vrstvu ⇒ gaze se nikdy nehraje. Latentní pro 5 ras (nikdo nemá
  Hypnotic Gaze), ale TA5 (dnešní oprava gaze mechaniky) je tím pádem za mrtvou nabídkou.
* **A4 — THROW_TEAM_MATE: akce + resolver existují, makro neexistuje.** Tentýž vzor jako A3.
  V korpusu latentní: orc TV1200 Trolla ani gobliny nestaví a human Ogre (TTM) nemá žádného
  spoluhráče s Right Stuff (`roster.cpp:503-536`) — akce se nenabídne už na úrovni
  `rules_engine.cpp:140-147`. Při rozšíření ras (goblin/halfling/ogre) ŽIVÁ díra třídy P45/F12.
* **A5 — BALL_AND_CHAIN: akce + resolver existují, makro neexistuje** a všechna makra hráče
  s Ball & Chain výslovně vynechávají (`macro_actions.cpp:424,572,602,761,939,989`).
  Fanatik by se za celý zápas ani nepohnul. Latentní (žádný v 5 rasách).
* **A6 — MULTIPLE_BLOCK: akce + resolver existují, makro neexistuje.** Latentní analogicky.
* **A7 — Stand-and-go: vstávací makro stojí NA MÍSTĚ.** Vstávání se emituje jako REPOSITION
  s cílem = vlastní pole (`macro_actions.cpp:421-426`); `movePlayerToward` po vstání vrátí
  „arrived". Hráč má pak `hasMoved=true`, a všechna pohybová makra pro řadové hráče vyžadují
  `isFreeToAct` = `!hasMoved` ⇒ **řadový hráč po vstání už NIKDY ten tah nejde dál** — přestože
  ř. 670-671 dovolují po vstání utratit zbytek MA. VÝJIMKA: nosič míče — SCORE/ADVANCE/PASS
  makra testují jen `canAct()` (bez `hasMoved`), takže nosič může vstát a v dalším makru jít.
  Ležící hráč u volného míče se ale k PICKUPu ten tah nedostane nikdy (PICKUP vyžaduje
  `isFreeToAct`). ⇒ strop P45 trvá pro všechny mimo nosiče.

## Seznam B: plánovač oceňuje jiný pohyb, než resolver provede

* **B1 (živé, default-off oprava existuje) — P35: výběr blitzera cení blok z jeho VÝCHOZÍHO pole,
  resolver ho hází z pole PŘÍLETU.** `getBlockDiceCount` v nabídce BLITZ maker
  (`macro_actions.cpp:604`) a v `expandBlitz` bez zapnutého `blitzLandingArm` počítá obranné
  asistence kolem aktuální pozice blitzera; `resolveBlock` je počítá u pole, kam blitzer došel
  (`block_handler.cpp:550`). Vlastní komentář kódu: bracket kostek se liší v 16,2 % blitzů.
  Oprava je za A/B ramenem default OFF ⇒ v produkci mismatch žije.
* **B2 (živé) — `estimateBlockFailChance` nezná obranný Wrestle.** `blockDieBadFraction`
  (`macro_actions.cpp:294-296`) počítá BOTH_DOWN jako bezpečný pro útočníka s Blockem, ale
  resolver (`block_handler.cpp:670-703`) nechá obránce bez Blocku s Wrestle útočníka SLOŽIT
  (a u nosiče je to turnover, ř. 8677-8678). Skaven staví 2 linemany +Wrestle ⇒ blitz do nich
  je systematicky podceněné riziko. (Zrcadlo dnešní opravy scoreFace — tam šlo o útočný
  Wrestle, tady zbývá obranný.)
* **B3 (živé, malé) — `estimateApproachFailChance` ignoruje rerolly.** Cena dodge kroku
  = (target−1)/6 bez Dodge/týmového rerollu (`macro_actions.cpp:342-346`), resolver reroll dává
  (`move_handler.cpp:147-148`). Nadhodnocuje riziko cesty hráčů s Dodge ⇒ volba blitzera se
  systematicky kloní jinam, než jak resolver skutečně padá.
* **B4 (vyřešeno dnes, evidováno pro úplnost)** — scoreFace neuměl ocenit Wrestle
  (BOTH_DOWN=4 < odsun=5), resolverová větev byla nedosažitelná; od 24.08. opraveno
  (`block_handler.cpp:12-48`).
* **B5 (živé, malé) — nabídka HAND_OFF_SCORE chce `adjDist ≤ 2`, executor umí dojít
  `movementRemaining` polí** (`macro_actions.cpp:456-457` vs. `:1810-1820`) — nabídka přísnější
  než provedení; hand-off přes delší doběh se nikdy nenabídne (příbuzné třídě P45).
* Pozn.: `scoreMoveAction` (paušály 20/12 za TZ, 8 za GFI) je vědomá heuristika trasy — cena se
  liší od pravé pravděpodobnosti pádu (nezná AG/Tackle/DT), ale resolver pak jde přesně tu
  trasu, kterou heuristika vybrala; není to mismatch identity akce, jen kalibrace.

## Poznámky z průchodu

### Mapa souborů (podúloha 0)

Pohybový kód: `engine/src/move_handler.cpp` (379 ř.), `rules_engine.cpp` (241 ř., generuje akce),
`turn_planner.cpp` (431 ř.), `macro_actions.cpp` (2081 ř., makro-výběr), `pathfinder.cpp` (135 ř.),
`action_resolver.cpp` (254 ř.), `big_guy_handler.cpp` (193 ř., brány Bone-head atd.),
`ball_and_chain_handler.cpp` (149 ř.), `block_handler.cpp` (986 ř., odsuny), `cage_advance.cpp`,
`helpers.cpp`, `turn_handler.cpp`, `ttm_handler.cpp`. Hlavičky v `engine/include/bb`.

### Index pravidel (rules_bb2016.txt, ověřené řádky)

* Move = MA čtverců: ř. 341-342; Blitz = MA + 1 blok za 1 čtverec: ř. 347-350.
* Pohyb libovolným směrem, ne do obsazeného pole, nemusí vyčerpat MA: ř. 470-475.
* Dodge při opuštění TZ, jen jednou za opuštění pole: ř. 480-486; tabulka AG ř. 506-507.
* Modifikátory dodge: +1 za hod, −1 za každou TZ na CÍLOVÉM poli (příklad ř. 578-584 „subtract 2
  because there are two Orc tackle zones on the square he is moving to“; tabulka ř. 597-600 má
  OCR chybu „+1“, příklad je jednoznačný).
* Neúspěšný dodge: KD v poli, KAM skákal + turnover: ř. 496-500.
* Follow-up po bloku zdarma, bez dodge: ř. 608-617.
* Push back: do prázdného pole, jinak chain push; směr volí útočník; sekundární směry volí
  aktivní tým: ř. 635-649; vytlačení do davu ř. 650-663.
* Prone: ztrácí TZ, vstávání 3 MA, bez dodge při vstávání, po vstání nesmí Block Action: ř. 667-676.
* STANDING UP: jen na začátku akce, 3 MA; MA < 3 ⇒ 4+ a při úspěchu se smí hnout jen přes GFI;
  neúspěch NENÍ turnover: ř. 689-695. (Pozn.: brief říká „MA ≤ 2", text ř. 691-692 říká „less than
  three squares of movement" — totéž.)
* GFI: 1-2 pole navíc, hod po každém poli, KD jen na 1 (tj. 2+): ř. 1694-1706; Blizzard ⇒ pád na
  1-2 (tj. 3+): ř. 1489-1492 (a 1527-1531).
* Rerolly: „jeden hod nikdy nesmíš rerollnout víc než jednou": ř. 925-927; týmový reroll max 1 za
  kolo: ř. 934-935.
* Dovednosti (řádky): Ball & Chain 7809-7834 · Blood Lust 7929-7947 · Bone-head 7975-7986 ·
  Break Tackle 7987-7991 · Diving Tackle 8072-8085 · Dodge 8086-8092 · Frenzy 8134-8145 ·
  Grab 8146-8157 · Jump Up 8196-8204 · Leap 8270-8283 · Prehensile Tail 8373-8377 ·
  Really Stupid 8388-8405 · Shadowing 8455-8469 · Side Step 8470-8480 · Sprint 8487-8490 ·
  Stand Firm 8510-8516 · Stunty 8525-8538 · Sure Feet 8539-8542 · Tackle 8566-8571 ·
  Take Root 8572-8584 · Tentacles 8585-8594 · Throw Team-Mate 8599-8624 · Timmm-ber! 8625-8635 ·
  Titchy 8636-8642 · Two Heads 8643-8646 · Wild Animal 8662-8669 · Wrestle 8670-8678 ·
  Hypnotic Gaze 8178-8189 · Juggernaut 8190-8195.
* Blokové kostky: ATTACKER DOWN 512-513 · BOTH DOWN 514-520 (oba NA MÍSTĚ) · PUSHED 521-523 ·
  DEFENDER STUMBLES 524-529 · DEFENDER DOWN 530-533. Bone-head kácí blitz týmu: 7980-7983.

### Co proti pravidlům SEDÍ (pozitivní parita, ověřeno čtením)

* **MA a spotřeba**: krok = 1 pole, nemusí vyčerpat MA, nesmí do obsazeného pole (ř. 470-475;
  `move_handler.cpp:94-115`). ✓
* **GFI**: 2+ (pád jen na 1), Blizzard 3+ (pád na 1-2), max 2 pole, Sprint 3, hod po každém poli
  u krokového pohybu (ř. 1694-1706, 1489-1492; `move_handler.cpp:117-127,167-189`). ✓
* **Sure Feet**: reroll GFI, 1×/kolo (ř. 8539-8542; `helpers.cpp:262-268`). ✓
* **Dodge mechanika**: hod při opuštění TZ, jen 1× za opuštěné pole, cíl = tabulka AG + 1 za
  každou TZ na cílovém poli, pád na cílovém poli + turnover (ř. 480-500, 578-584;
  `move_handler.cpp:103-165`, `helpers.cpp:70-73`). ✓ (modifikátory dovedností viz N1-N5)
* **Tackle**: ruší Dodge reroll (ř. 8566-8571; `move_handler.cpp:133-148`). ✓
* **Rerolly**: jeden hod max jeden reroll (ř. 925-927), týmový reroll max 1/kolo (ř. 934-935),
  Dodge reroll 1×/kolo (ř. 8089-8090), Pro brána 4+ s pravidlem „Pro roll lze přehodit týmovým"
  (ř. 8381-8387), Loner 4+ (ř. 8287-8290) — vše v `helpers.cpp:241-325`. ✓ (opravy TA1 z 21.-24.08. drží)
* **Vstávání**: 3 MA na začátku akce; MA < 3 ⇒ 4+ a dál jen GFI; neúspěch není turnover, akce
  spotřebována; po vstání nelze Block Action (ř. 689-695, 674-676; `move_handler.cpp:314-377`,
  `rules_engine.cpp:42-55,221-238`). ✓
* **Jump Up**: vstává zdarma při ne-blokové akci (ř. 8197-8199; `move_handler.cpp:322-334`). ✓
  (chybí jen bloková větev z lehu — A2)
* **Take Root (TA2)**: hod při každé deklaraci 1×/aktivaci, zakořenění trvá do konce drivu /
  do sražení, MA 0, žádný push „for any reason" (i chain, i Juggernaut — `block_handler.cpp:285-291,375-380`),
  blok bez follow-upu (`block_handler.cpp:411-414`), vstávání dovoleno (ř. 8572-8584;
  `big_guy_handler.cpp:87-119`, `game_simulator.cpp:382-384`). ✓ až na N11/N14.
* **Bone-head / Really Stupid / Wild Animal hody**: 2+ / 4+ (2+ s ne-RS sousedem, ř. 8393-8395) /
  4+ (2+ na Block-Blitz, žádný auto-úspěch, ř. 8666-8669); ztráta TZ jen u BH/RS
  (`big_guy_handler.cpp:13-85`). ✓ až na N12/N13.
* **Push řetěz**: přednost prázdného pole, chain push „jako by ho blokoval první" včetně
  ležících, směry sekundárních pushů volí aktivní tým, Side Step se přenáší řetězem, míč
  v cílovém poli se odrazí (ř. 635-649; `block_handler.cpp:138-369`). ✓
* **Crowd surf**: bez armouru, injury od davu; nosič ⇒ throw-in z posledního pole (ř. 650-663;
  `block_handler.cpp:293-319,772-791`). ✓
* **Stand Firm**: volba nebýt odsunut, KD na místě, „pushed into Stand Firm ⇒ neither moves",
  Juggernaut na blitzu ruší (ř. 8510-8516, 8192-8194; `block_handler.cpp:285-291,326-343,384-390`). ✓
* **Side Step**: libovolné SOUSEDNÍ pole volbou bránícího, jen jsou-li volná, jinak standard
  (ř. 8470-8480; `block_handler.cpp:138-148,243`). ✓
* **Grab**: jen Block Action, prázdná pole, vzájemné vyrušení se Side Step (ř. 8146-8157;
  `block_handler.cpp:392-396`). ✓
* **Frenzy**: druhý blok jen po Pushed/DS, povinný follow-up, na blitzu platí 1 MA/GFI i za
  druhý blok (ř. 8134-8145; `block_handler.cpp:889-918`). ✓
* **Blitz blok stojí 1 MA** (ř. 349-350; `block_handler.cpp:416-446` včetně GFI-do-bloku). ✓
* **Shadowing (TA9)**: 2D6 + MA utíkajícího − MA stínujícího, ≤ 7 ⇒ následuje do uvolněného
  pole bez dodge, ≥ 8 ⇒ únik; bez limitu za kolo; jen jeden stínující (ř. 8458-8469;
  `move_handler.cpp:49-84`). ✓ (nevolá se u Leapu — N6; „may" je vždy-ano — legální volba)
* **Tentacles (TA8)**: 2D6 + ST utíkajícího − ST chapadel, ≤ 5 ⇒ držen; jen jeden chapadlový
  hráč (ř. 8588-8594; `move_handler.cpp:11-47`). ✓ mechanika; ukončení akce viz N9, Leap N6.
* **Leap resolver**: cíl do 2 polí, 2 MA, bez dodge z výchozího pole, čisté AG bez modifikátorů
  mimo VLL, pád = KD na cílovém poli + armour + turnover (ř. 8270-8283, 8655-8657;
  `move_handler.cpp:219-312`). ✓ resolver — ale bez volajícího (A1) a s N6/N7.
* **Stunned → face-up na KONCI příštího vlastního kola** (ř. 703-708; `player.h:34-42`,
  reset + resolveEndTurn). ✓
* **Wrestle (F11 + volba)**: oba prone bez armouru, přebíjí Block, turnover jen když aktivní
  hráč nesl míč (ř. 8670-8678, 368-372; `block_handler.cpp:639-704`). ✓

## Pořadí oprav podle dopadu

1. **N8 — Both Down odsouvá obránce + follow-up** (`block_handler.cpp:717-719`). Hraje v korpusu:
   ANO, všech 5 ras, prakticky každý blok s výsledkem BD proti obránci bez Blocku (scoreFace ji
   u Block-útočníka cení 8/10, tedy častá VOLBA). Posouvá dva hráče o pole, kam pravidla
   nedovolují — deformuje klece, screeny i všechna dosavadní poziční měření. Oprava je lokální
   (nekácet přes push větev, kácet na místě).
2. **N1 — Dodge skill dostává −1 modifikátor navíc k rerollu** (`helpers.cpp:77-91`). Hraje:
   ANO (skaven, human, wood-elf). Systematicky nadhodnocené uhýbání týchž týmů, které už
   nadhodnotil TA1 řetěz rerollů — korpus 20260821 je tím zkreslen DVAKRÁT stejným směrem.
3. **A7 — stand-and-go strop** (`macro_actions.cpp:421-426` + `isFreeToAct`). Hraje: ANO, každé
   vstávání ne-nosiče (a P45 měřil právě tohle). Vstávající hráč ztrácí celý zbytek MA; ležící
   hráč u volného míče se ten tah k míči nikdy nedostane.
4. **N13 — P55: propadlá akce nespotřebuje týmový blitz/pass** (`action_resolver.cpp:47-60`).
   Hraje: ANO (human Ogre Bone-head; Take Root čtení sporné — viz tabulka). Tým dostává druhý
   blitz, který podle ř. 7980-7983 nemá mít.
5. **N10 — žádný pohyb po blitz-bloku** (`block_handler.cpp:887`). Hraje: ANO, každý blitz.
   Legální podmnožina, ale škrtá celé taktické patro (hit-and-run, blitz-and-score nosičem);
   třída „akce se nikdy nestane".
6. **A1/F12 — Leap bez volajícího** (+ N6, N7 v resolveru). Hraje: ANO v tom smyslu, že oba
   wardanceři (jediní nositelé Leapu v korpusu) rok neskočili; samotné N6/N7 se projeví až
   s volajícím.
7. **B1 — P35 blitz se cení z výchozího pole** (`macro_actions.cpp:604`; oprava existuje za
   default-off ramenem). Hraje: ANO, 16,2 % blitzů má jiný bracket kostek. Rozhodnutí = zapnout
   rameno po A/B.
8. **B2 — plánovač nezná obranný Wrestle** (`macro_actions.cpp:294-296`). Hraje: ANO
   (skaven 2× lineman +Wrestle jako obránce).
9. **N12 — auto-obnova TZ po Bone-head/RS** (`game_state.cpp:70`). Hraje: ANO (human Ogre),
   dopad malý-střední.
10. **N15 — Sprint se nepočítá do dosahu nabídek** (`macro_actions.cpp` + `turn_planner.cpp:34`).
    Hraje: ANO (WE Catchers), malý.
11. **N11 — rooted projde blitzem přes GFI** (`pathfinder.cpp:34`, `move_handler.cpp:121`).
    Hraje: potenciálně (WE Treeman), vzácné — změřit (M1 níže).
12. **N14 — Take Root hod se při vstávání přeskočí** (chybí trvalé zakořenění z hozené 1).
    Hraje: ANO (WE Treeman), malý.
13. **B3 — cena přístupové cesty bez rerollů**. Hraje: ANO, malý (kalibrace výběru blitzera).
14. **B5 — HAND_OFF_SCORE jen z adjDist ≤ 2**. Hraje: ANO, malý.
15. Latentní balík (v korpusu 5 ras nehraje nic z toho): **N2 Stunty, N3 Diving Tackle,
    N4 Break Tackle, N5 Titchy, N9 Tentacles-retry, N16 TTM, N17 Ball & Chain, N18 Blood Lust
    timing, N19 Timmm-ber!, A2 Jump Up blok, A3 Gaze bez makra, A4 TTM bez makra, A5 B&C bez
    makra, A6 Multiple Block bez makra.** Opravovat až s rozšířením ras, ale A3-A6 jsou třída
    P45/F12 — při KAŽDÉM přidání rasy projít tenhle seznam.

## Co nejde rozhodnout čtením — zadání na měření (ne závěry)

* **M1 (k N11):** Jak často korpus/MCTS pošle na blitz zakořeněného hráče? Zadání: projít
  eventy her (SKILL_USED TakeRoot fail=1 → tentýž hráč později v témže drivu PLAYER_MOVE/GFI).
  Data: `crosses_20260821_data/` (pozor na známou vadu TA1 v tom korpusu).
* **M2 (k N8):** Kauzální dopad opravy Both Down na TD/hru, casualties a délku drivů — jen
  A/B běh po opravě; čtením lze říct jen směr (obránci končí o pole dál, útočník získává pole).
* **M3 (k N1):** Očekávaný posun úspěšnosti dodge po odebrání −1 lze spočítat offline
  (přepočítat targety dodge hodů z uložených her); skutečný dopad na výsledky jen A/B.
* **M4 (k A7):** Kolik % vstávání má nevyužitý MA ≥ 1 a smysluplný cíl (míč/EZ/klec do dosahu)?
  Offline na korpusu; řekne, jaký strop P45 pořád drží.
* **M5 (k A7-nosič):** Jak často MCTS vloží mezi nosičovo vstávací makro a jeho pokračování
  makro jiného hráče (actor-switch pak nosiče uzavře přes `hasMoved→hasActed`,
  `action_resolver.cpp:216-223`)? Rozhodne, jestli nosičův stand-and-go reálně funguje.
* **M6 (k B1):** Zapnout `blitzLandingArm` v párovém A/B — counter repicků už existuje
  (`takeBlitzLandingRepicksInSearch`).
* **M7 (k B2):** Četnost a výsledky blitzů/bloků do obránců s Wrestle v korpusu (kolik
  BOTH_DOWN tam padlo a s jakým výsledkem pro útočníka).
* **M8 (pořadí dodge vs. GFI v jednom kroku):** text pořadí nefixuje (ř. 496-500 vs. 1701-1706);
  outcome je čtením ekvivalentní (pád na cílovém poli + turnover v obou pořadích, pravděpodobnost
  součinová), liší se jen spotřeba rerollů. Pokud na tom někdy bude záležet, změřit spotřebu
  rerollů — není to dnes nález.

HOTOVO
