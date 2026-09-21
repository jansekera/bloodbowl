# AUDIT PHP ENGINE PROTI `rules_bb2016.txt` — 15.09.2026

Dva read-only agenti *(skilly A–L · skilly M–Z + tabulky)*. **Nálezy agentů NEJSOU ověřené** —
před každou opravou přečíst řádek pravidel a kód znovu *(souhrn zahladí výhradu)*.
Řádky = `rules_bb2016.txt`; PHP pod `src/Engine/`; C++ pod `engine/src/`.

**Stav:** ✅ = opraveno · ⭐ = ověřeno ručně · bez značky = jen nález agenta

⭐ **Vzorec:** testy psané podle implementace kódují vadu *(12 souborů A–L, 15 souborů M–Z)*.
⇒ Pravidlový test má zdroj řádek pravidel, ne současné chování.

---

## Opraveno 15.09. před auditem *(nehledat znovu)*
počasí u přihrávky/chytání/zvedání · NoS a Big Hand u zvedání · intercepce (modifikátory,
podmínka zóny, volba nejlepšího) · Safe Throw · fumble přihrávky · DP ležící · vánice jen
quick/short · tabulka počasí · tabulka výkopu 7/8 · Thick Skull · Pass/Dodge/MB/Accurate/Catch/Sure Hands (14.09.)

---

## A. SKILLY A–L *(27 odchylek)*

| skill | pravidla | PHP | C++ | testy s vadou | jistota |
|---|---|---|---|---|---|
| Always Hungry | 7786-7794: druhá 1 = sněden (mrtev), 2-6 = fumble, míč od spoluhráče | `Action/ThrowTeamMateHandler.php:79-121` stačí jedna 1, jen INJURED, fumble chybí | dle pravidel | `AlwaysHungryTest.php:49,111,135` | jistá |
| Animosity | 7804-7806: možno změnit cíl na stejnou rasu | `PassResolver.php:47-61`, `Action/HandOffHandler.php:100-108` jen konec akce; hand-off neodečte akci | neimpl. | – | pravděpodobná |
| Ball & Chain | 7811-7830: šablona throw-in, dav, normální blok, ležící odstrčen + zbroj, bez zbroje → zranění, Stunned = KO, GFI | `Action/BallAndChainHandler.php` D8 (:53), mimo hřiště KO bez hodu (:58-74), blok 1 kostka (:143), vstup na obsazené pole (:85-86), GFI chybí | stejná vada | `BallAndChainTest.php:21,74` | jistá |
| Blood Lust | 7934-7938: krmí se NA KONCI akce | `BigGuyCheckResolver.php:304-316` hned při deklaraci | stejná | `BloodlustTest.php:48,93` | jistá |
| Bombardier | 7950-7972: nespotřebuje Pass akci; fumble exploduje u házeče; sousedé 4+ i ležící; turnover; chycení/intercepce | `Action/BombThrowHandler.php` spotřebuje Pass (:56-58), fumble odletí (:87-90), 3×3 automaticky jen stojící (:130-158), nikdy turnover (:111), fumble jen přirozená 1 (:79) | skoro dle pravidel | `BombThrowTest.php:22,86,118,140` | jistá |
| Bone-head *(+ Hypnotic Gaze, Really Stupid)* | 7983-7985: nechytá, nezachycuje, nepřihrává, neasistuje | `BallResolver.php:108-145` chytá; `StrengthCalculator.php:42-58` asistuje | asistence OK | – | jistá |
| ✅ **Brawler** — **ODSTRANĚN 15.09.** | **v BB2016 NEEXISTUJE** (BB2020) | `Action/BlockHandler.php:162,254,304-322,406-410` přehoz Both Down | neimpl. | `BlockRerollTest.php:120-171` | jistá |
| Break Tackle | 7988-7991: ST místo AG, 1× za kolo | `TacklezoneCalculator.php:128-130` vždy ST (i nižší), bez limitu | limit chybí | – | jistá |
| ✅ **Dodge — přehoz 1× za kolo, OPRAVENO 21.09.** | 8089-8090, 960-962: „may only re-roll one failed Dodge roll per turn“ | `Action/MoveHandler.php:251` hlídá `isDodgeUsedThisTurn()`; příznak v `MatchPlayerDTO`, reset v `GameState::resetPlayersForNewTurn` | OK *(`dodgeRerollUsedThisTurn`, `helpers.cpp:307`)* | `DodgeOncePerTurnTest.php` (4 testy) | jistá |
| ✅ **Pro — chytání po odskoku a throw-inu, OPRAVENO 21.09.** | 1263 + 8381: Pro na jakýkoli hod kromě brnění/zranění; r. 925-926 po přehozu z Catch už ne | `BallResolver::resolveCatchFromBounce` nabízí Pro a přehoz z Catch **vydá událost** (dřív se dělal potichu a v záznamu byl jen výsledek) | ⏰ nekontrolováno | `BallResolverTest.php` (4 testy) | jistá |
| ✅ **Týmový přehoz u chytání po odskoku — DOPLNĚNO 21.09.** | 929-933: „any dice roll … made by a player in their own team … **during their own turn**"; 8387 týmový přehoz hodu Pro; 1263 u výkopu ne | `resolveCatchFromBounce` si podmínky spočítá ze stavu (vlastní hráč, vlastní kolo, fáze PLAY, volný přehoz); Loner respektován; u výkopu zůstává `resolveCatch(teamRerollAvailable: false)` | – | `BallResolverTest.php` (+3 testy) | jistá |
| **C++: blok po neúspěšném Pro** *(nález agenta 18.09.; C++ engine)* | 8386-8387: týmový přehoz smí přehodit jen hod Pro | `engine/src/block_handler.cpp:733+` přehodí kostky bloku | ⛔ tady je vada | – | pravděpodobná |
| Chainsaw | 8002-8016: +3 ke zbroji (zásah, kickback, pád držitele, faul), 1× za kolo | `Action/BlockHandler.php:780-842` +3 chybí; v Multiple Block 2× (:599-632); kickback bez turnoveru | +3 OK | `ChainsawTest.php:105` | jistá |
| Claw | 8021-8022: zbroj 8+ **po modifikacích**, jen při bloku | `InjuryResolver.php:52,59` bez modifikátorů; předává se do Stab/Chainsaw | dle pravidel | – | jistá |
| Dauntless | 8029-8035: > (ne ≥), před asistencemi, Horns | `Action/BlockHandler.php:131-138,659-665` `>=`, se součty asistencí, bez Horns, ne na Frenzy | dle pravidel | `NewSkillsTest.php:326-357` | jistá |
| Decay | 8038-8039: 2× Casualty tabulka | `InjuryResolver.php:210-213` 2× hod na zranění | dle pravidel | – | jistá |
| Dirty Player | 8045-8049: +1 ke zbroji **nebo** zranění | `Action/FoulHandler.php:68-70` vždy zbroj | stejná | `Phase12SkillsTest.php:150` | jistá |
| Diving Catch | 8064-8071: +1 k chytání přesné přihrávky na své pole; chytání do sousedního pole bez přesunu; kick-off, throw-in; dva DC = nikdo | `BallResolver.php:336-338` −1 v zóně u všech; `PassResolver.php:275-298` přesun, jen nepřesná | jiná vada | (`PassingSkillsTest.php:356`) | jistá |
| Diving Tackle | 8074-8085: −2 volitelně PO hodu, DT hráč Prone na uvolněné pole | `TacklezoneCalculator.php:160-172` vždy −2; `Action/MoveHandler.php:352-365` lehne jen při úspěchu a na svém poli | horší | `CombatSkillsTest.php:600` | jistá |
| Dump-Off | 8097-8098: normální pravidla přihrávky | `Action/BlockHandler.php:78-95,1373-1400`; bez intercepce, bez Pass přehozu, automaticky | neimpl. | `Phase12SkillsTest.php:536` | pravděpodobná |
| Fend | 8115-8116: nenásleduje ani když Fend padl; Juggernaut ruší | `Action/BlockHandler.php:975` následuje po pádu; Juggernaut neruší | dle pravidel | `Phase12SkillsTest.php:74` | jistá |
| Frenzy | 8139-8145: 2. blok jen po Pushed/Defender Stumbles; na Blitzu stojí pole MA | `Action/BlockHandler.php:222-262` po čemkoli s oběma stojícími; MA ani GFI; bez Horns/Dauntless | dle pravidel | – | jistá |
| Grab | 8148-8154: jen Block Action; libovolné volné sousední pole; Grab × Side Step se ruší | `Action/BlockHandler.php:1118-1133` i Blitz/MB, jen 3 pole, Side Step přebije | dle pravidel | `ChainPushTest.php:278` | jistá |
| Hail Mary Pass | 8163-8164: jakékoli pole | `PassResolver.php:66-71` jen mimo dosah; `RulesEngine::validatePass` mimo dosah odmítne | – | – | nejasné |
| Horns | 8174-8175: všechny bloky v Blitzu | `Action/BlitzHandler.php:176-181` jen první | stejná | – | jistá |
| Hypnotic Gaze | 8181-8188: na konci Move akce, **AG hod**, oběť nechytá/neasistuje | `Action/HypnoticGazeHandler.php:77` `min(6, 2+tz)` bez AG, bez pohybu | AG OK | `HypnoticGazeTest.php:108,135` | jistá |
| Juggernaut | 8194: **může** zvolit; ruší Fend | `Action/BlockHandler.php:876` vždy | nucené taky | – | pravděpodobná / jistá (Fend) |
| Jump Up | 8200-8204: blok z lehu = AG hod +2 | `Action/BlockHandler.php:60-64,489-493`; `RulesEngine.php:147-150` zdarma | neimpl. | `SkillModifiersTest.php:354` | jistá |
| Kick-Off Return | 8250-8254: ne na lajně ani v zóně; před kick-off tabulkou | `KickoffResolver.php:70-88,526-547` po tabulce, bez podmínek | stejná | – | jistá |
| Leader | 8260-8266: na začátku hry i v poločase, jen s Leaderem na hřišti | `GameFlowResolver.php:173-174,226-244` jen poločas, bez podmínky | neimpl. | `Phase12SkillsTest.php:281` | jistá |
| Leap | 8271-8277: libovolné volné pole do 2, **bez modifikátorů** (kromě VLL), GFI, přehoz | `Pathfinder.php:158-177,260-274` jen 8 polí, přičítá zóny; `Action/MoveHandler.php:137-217` GFI se nehází | dle pravidel | `LeapTest.php:119` | jistá |

Chybí v enumu: Fan Favourite, Filthy Rich, Kick Team-Mate. Sedí: Block, Foul Appearance, Guard, Kick, Loner.

---

## B. SKILLY M–Z *(22 odchylek)*

| skill | pravidla | PHP | C++ | testy s vadou | jistota |
|---|---|---|---|---|---|
| Stunty – zranění | 8535-8536: 7 = KO, 9 = Badly Hurt (ne +1) | `InjuryResolver.php:215` +1 | stejná | `SkillModifiersTest.php:105` | pravděpodobná (liší se s Thick Skull a Stab) |
| Stunty – přihrávka | 8533: −1 k přihrávce | `PassResolver::getPassRollModifier` chybí | stejná | – | jistá |
| Stunty + Secret Weapon | 8537: SW nesmí ignorovat zóny | `TacklezoneCalculator.php:198` vždy | stejná | – | jistá |
| Titchy + Stunty | 8641 | `TacklezoneCalculator.php:213-225` dvojí odečet | jiná vada | – | jistá |
| Stakes | 8507: +1 zbroj při Stab proti Khemri/Necro/Undead/Vampire | `InjuryResolver.php:247-258` jen blok Regenerace, +1 chybí, i u bloku a faulu | stejná | `StakesTest.php:18,90` | jistá |
| Stab | 8494: nemodifikovaná zbroj | `Action/BlockHandler.php:740,636` předává Claw | OK | – | jistá |
| Regeneration | 8409-8413: po Casualty a po lékárníkovi, nepřehazuje se | `InjuryResolver.php:248` před lékárníkem, lékárník → 2. hod na regen (:156) | OK | – | jistá |
| ✅⭐ **Pro** — **OPRAVENO 18.09.** *(`84f7dc46` + `5fabdd12`; zápis použití u zvedání/chytání/přihrávky, ne po skill přehozu, po Pro týmový jen na hod Pro, blok = všechny kostky, reset i v soupeřově kole)* | 8381-8387, 919-926, 982: 1× za kolo, kostka nejvýš 1× přehozena, 1-3 = výsledek platí, týmový přehoz jen na hod Pro | `BallResolver.php:62,164`, `PassResolver.php:142` nenastaví used; Pro po skill přehozu; po Pro týmový přehoz (`MoveHandler.php:308,442`); blok jen nejhorší kostka (`BlockHandler.php:320-340,690`) | OK | `InteractiveRerollTest.php:318` přepsán | jistá |
| ✅⭐ **Sure Feet** — **OPRAVENO 18.09.** *(`84f7dc46`; `sureFeetUsedThisTurn`)* | 8540-8541: 1× za kolo | `Action/MoveHandler.php:396` bez limitu | OK | – | jistá |
| Tentacles | 8587-8590: 2D6 + vlastní ST − ST Tentacles ≤ 5; jen jeden | `MoveHandler.php:222-238` D6+ST vs D6+ST, každý marker, ne na Leap | OK | `CombatSkillsTest.php:305,344` | jistá |
| Shadowing | 8458-8461: 2D6 + MA − MA stínu ≤ 7 | `MoveHandler.php:373-376` D6 rozdělení; ne po Leap | OK | `ShadowingTest.php:16-84` | jistá |
| Take Root | 8575-8579: MA 0, bez GFI, neodstrčitelný; končí pádem | `Pathfinder.php:51` 2 GFI; `BlockHandler` odtlačí; končí jen driveem (`GameFlowResolver.php:194`) | většinou OK | – | jistá |
| Stand Firm | 8515: nikdo se nehne; 1824 vleže jen Extraordinary | `BlockHandler.php:1109,1276` přeskočí pole → do publika; platí i vleže | OK | `ChainPushTest.php:125` | jistá |
| Side Step | 8475: libovolné sousední pole | `BlockHandler.php:1134-1154` jen 3 | OK | `ChainPushTest.php:219` | jistá |
| Strip Ball vs Sure Hands | 8545 | `BlockHandler.php:1189,1211` bez kontroly | OK | – | jistá |
| Piling On | 8361-8372: sousedí, týmový přehoz, Loner, turnover | `BlockHandler.php:1008-1025` zdarma, bez podmínek, MB 2× | neimpl. | – | jistá |
| Sneaky Git | 8484-8486: vyloučen jen když zbroj prorazí | `Action/FoulHandler.php:88` nikdy | stejná | – | jistá |
| No Hands | 8319: nezachycuje | `PassResolver::checkInterceptions` pustí | OK | – | jistá |
| Pass Block | 8355: dodge, legální cíl, i proti Dump-Off | `PassResolver.php:713+` bez dodge, rovně, jen 1, ne u Dump-Off | neimpl. | `PassBlockTest.php:15,140` | jistá |
| ✅ **TTM: rozptyl 3× — OPRAVENO 21.09.** | 8609-8611: „**accurate passes are treated instead as inaccurate passes thus scattering the player three times**" | `ThrowTeamMateHandler::scatterThrownPlayer()` — přesný i nepřesný hod = 3 rozptyly po 1 poli, po výletu ze hřiště se další nehází | – | `ThrowTeamMateTest.php` (+3 testy; 6 starých přepsáno) | jistá |
| ⏰ **Zbytek řádku TTM níž je pořád otevřený**: chybí **−1** k hodu na přesnost, **Long/Long Bomb se nemá nabízet**, a **fumble má hráče položit na jeho PŮVODNÍ pole** (r. 8613), ne rozptýlit od házeče | | | | |
| Throw Team-Mate | 8607-8617: −1, jen quick/short, přesný = 3× rozptyl, fumble na místě, přistání na hráče = sražení | `Action/ThrowTeamMateHandler.php:137` bez −1; `RulesEngine.php:818` dlouhé; `:156,161,151,210-215` | OK | `ThrowTeamMateTest.php:16,41,65,153` | jistá |
| Right Stuff | 8429-8431: může jednat později; s míčem turnover | `ThrowTeamMateHandler.php:218` hasActed; `:235-254` bez turnoveru | OK | – | jistá |
| Secret Weapon | 8454: vyloučen i mimo hřiště | `GameFlowResolver.php:212` jen `isOnPitch` | OK | – | pravděpodobná |
| Really Stupid / Bone-head | 8403-8404, 1669: nechytá, neasistuje bez zón | `StrengthCalculator.php:46`, `BallResolver.php:128` | chytání stejná | – | jistá |

Neimplementováno: Nurgle's Rot (jen log). Monstrous Mouth, Swoop, Timmm-ber!, Weeping Dagger nejsou v enumu.

---

## C. TABULKY A PEVNÁ ČÍSLA *(18 odchylek)*

| co | pravidla | PHP | C++ | testy s vadou | jistota |
|---|---|---|---|---|---|
| ✅⭐ **Kostky bloku** | 567-568, 1731: **více než** dvojnásobek | `StrengthCalculator.php:100,109` `>=` → **opraveno `8623a6b5`** | OK | `StrengthCalculatorTest.php` přepsán | jistá |
| ✅⭐ **Dav (crowd)** — **+1 ODSTRANĚNO 16.09.** *(Reserves zbývá)* | 650-657: bez modifikátorů; Stunned → Reserves | `InjuryResolver.php:138` +1; Stunned zůstane s pozicí null | OK | `InjuryResolverTest.php:114,127,138` | jistá |
| Lékárník | 1202-1208: KO → Stunned/Reserves; znovu Casualty tabulka | `InjuryResolver.php:148-194` přehazuje 2D6 zranění; na KO nejde | KO chybí | `ApothecaryTest.php:21` | jistá |
| Casualty D68 | 2405-2430 | v PHP není (jen INJURED) | impl. | – | jistá |
| ✅⭐ **Throw-in** — **OPRAVENO 18.09.** *(šablona LRB6 jako C++, 2D6, stojící chytá / jinak odskok, znovu od posledního pole; + sestry: **nosič vytlačený do davu** r. 659-663 dřív jen položil míč, **odskok na ležícího/omráčeného** r. 893-898 dřív zůstal ležet)* | 868-878 | `BallResolver.php:250-251` D8 × D6 | OK | `ThrowInTest.php` | jistá |
| ✅ **TTM: hozený NOSIČ do davu — OPRAVENO 21.09.** | 8614-8616 „beaten up by the crowd in the same manner as a player who has been pushed“ ⇒ throw-in r. 659-663 | `Action/ThrowTeamMateHandler.php` vhazuje z posledního pole na hřišti (`BallResolver::resolveThrowIn`); `resolveLanding` dostal `$scatterOrigin` | ⏰ nekontrolováno | `ThrowTeamMateTest.php:236` | jistá |
| ✅⭐ **Nepřesná přihrávka** — **OPRAVENO 18.09.** *(i Dump-Off a Hail Mary: po vyletění se dál nerozptyluje, throw-in od posledního pole na hřišti, r. 868-871)* | 735, 272: 3× rozptyl po 1 poli | `PassResolver.php:249` (+ Dump-Off :661) 1 směr × min(3, D6) | OK | `PassResolverTest.php:87` | jistá |
| ✅⭐ **Faul** — **OPRAVENO 16.09.** *(asistence, paušální +1 pryč, turnover)* **a 21.09.** *(dublet i na ZRANĚNÍ)*; ⏰ zbývá jen volba u Dirty Player | 1845-1849 asistence; **1877-1878 dublet zbroj a/nebo zranění**; 384 vyloučení = turnover | `Action/FoulHandler.php` čte dublet z obou hodů; `InjuryResolver::resolveInjury` vrací i jednotlivé kostky (`dice`) — dřív jen součet `roll2D6()` | OK | `FoulTest.php` (+3 testy 21.09.) | jistá |
| ✅⭐ **KO recovery** — **OPRAVENO 16.09.** | 1009: i po touchdownu | `GameFlowResolver.php:82-121` jen v poločase | OK | – | jistá |
| ✅ **Přehozy v poločase** — **OPRAVENO 16.09.** *(`f58eecb1`; Leader zbývá)* | 942: obnovit; 8261 | neobnoví se | pevně 3 | – | jistá |
| Výkop 2 Get the Ref | 1275: +1 úplatek | `KickoffResolver.php:137` nic | stejná | – | jistá |
| Výkop 3 Riot | 1286-1296 | `KickoffResolver.php:152-173` jen přijímající, bez D6 | stejná | `KickoffResolverTest.php:163,181` | jistá |
| Výkop 5 High Kick | 1302-1307: hráč mimo zónu | `KickoffResolver.php:69` před položením míče (:82), bez zón | časování OK | – | pravděpodobná |
| Výkop 6 / 8 | 1309, 1321: D3 + FAME + roztleskávačky/asistenti; shoda = oba | `KickoffResolver.php:241-290` D6 vs D6 | stejná | `KickoffResolverTest.php:260` | jistá |
| Výkop 7 Changing Weather | 1318: Nice → +1 pole rozptylu | `KickoffResolver.php:296` chybí | stejná | – | jistá |
| Výkop 10 Blitz | 1334+: volný tah | `KickoffResolver.php:347` všichni +1 pole | stejná | `KickoffResolverTest.php:365` | jistá |
| Výkop 11 Throw a Rock | 1342-1350: D6+FAME, náhodný hráč, zranění | `KickoffResolver.php:376` oba týmy, jen stojící, auto Stunned | stejná | `KickoffResolverTest.php:391` | jistá |
| Výkop 12 Pitch Invasion | 1355: Ball & Chain KO | `KickoffResolver.php:423` neřeší | stejná | – | pravděpodobná |
| Sweltering Heat | 1479-1481: D6 za každého na konci drivu | `KickoffResolver.php:61,444` 1 náhodný stojící za tým při výkopu | OK | `WeatherTest.php:422` | jistá |

**Mimo zadání:** Blok v Blitzu nestojí MA *(agent A–L)*. **C++:** výkop mimo hřiště se posune zpět místo touchbacku (`kickoff_handler.cpp:241`); počasí přehazuje každý výkop (`:274`); výkop 7/8 prohozený (`enums.h:255-256`).
