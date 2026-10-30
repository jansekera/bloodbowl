# Noc 30.10.2026 — úkol C: opravný průchod (autor)

Vstup: `C_recenze.md` (6 nálezů, vše LOW). Průběžný zápis.

## 1. LOW — `ActionResolver` z kontejneru přehraje špatně → **opraveno**
- Test napřed: `DivingCatchKoucTest::testActionResolverZKontejneruPrehrajeAkciSeStejnymiHody` (kontejner +
  `FixedDiceRoller`, otázka → odpověď ano). Na starém kódu **spadl** („array contains 'diving_catch'“ — přehrání vzalo nové hody).
- Oprava příčiny, ne komentářem: konstruktor `ActionResolver` bere **jen `DiceRollerInterface`** a všechny resolvery staví
  sám nad `ReplayDiceRoller` — předat resolver s kostkami mimo záznam už nejde. Volitelné parametry nikdo jiný nepoužíval (grep).
  Z `ServiceProvider` pryč registrace, jejichž jediným odběratelem byl `ActionResolver` (Ball/Pass/Kickoff/GameFlowResolver,
  Scatter/StrengthCalculator, InjuryResolver). Mutace = starý kód ⇒ test spadl (výše). Po opravě 15/15 OK, PHPStan OK.

## 2. LOW — chybí test otázky u výkopu pro PŘIJÍMAJÍCÍHO hráče → **opraveno (strážné testy)**
- C++ `test_kickoff_handler.cpp`: `LiveGameAsksTheReceivingDivingCatchersCoach`, `ReceivingDiverAnsweringNoLetsTheBallBounceFromTheLanding`,
  `ReceivingDiverAnsweringYesCatches` (míč na (4,7), hráč na (4,8); ne ⇒ odraz na (3,8), ano ⇒ chytá 6).
- PHP `DivingCatchKoucTest`: `testVykopPrijimajicihoClovekaSePta`, `testVykopPrijimajiciNeMicOdskociZPoleDopaduAnoChyta`.
- Kód byl správně ⇒ na současném kódu zelené; „spadne“ ověřeno mutací z recenze („přijímající rovnou skočí / ptát se jen
  kopajícího“): C++ `kickoff_handler.cpp` podmínka `teamSide != receiving` ⇒ spadly 2 (ano-test projde, AI by skočila taky —
  očekávané); PHP `KickoffResolver` `$diver->getTeamSide() === $receivingTeam ? true : …` ⇒ spadly 2. Mutace vráceny ze zálohy.

## 3. LOW — chybí test dvou otázek Diving Catch v jedné akci (PHP fronta) → **opraveno (strážný test)**
- `DivingCatchKoucTest::testDveOtazkyVJedneAkciOdpovediSeSpotrebujiPopoade`: přihrávka → otázka hráči 2 → „ne“ → odraz ven →
  vhazování → otázka hráči 3 (`getAnswers() == [false]`) → „ano“ ⇒ chytá hráč 3. Každá odpověď jiným resolverem jen s novými
  kostkami (přehrání ze záznamu).
- Kód byl správně ⇒ test zelený; mutace v `ActionResolver::resolve`: (a) zahodit dřívější odpovědi, (b) obrátit pořadí fronty
  ⇒ v obou případech test **spadl** (chytil hráč 2). Vráceno.

## 4. LOW — `LiveGameDoesNotAskAboutGrabAgainstATeammate` projde s jakoukoli jinou otázkou → **opraveno**
- Test už žádnou otázku nepřijme (`FAIL` s číslem otázky) a po akci kontroluje, že B&C stojí na (11,7) a spoluhráč byl odtlačen
  na x = 12. Přibude-li jiná otázka (natočení B&C), test spadne a odpověď se doplní vědomě.
- Na současném kódu zelený; mutace (`pushSkills` bez `defender.teamSide != attacker.teamSide`) ⇒ spadl s „neočekávaná otázka 15
  (USE_GRAB = 15)“. Vráceno ze zálohy.

## 5. LOW / k rozhodnutí — „two or more players ATTEMPTING“ (ř. 8069-8071) → **neopraveno, návrh pro uživatele**
- Není vada proti zadání („dva a víc ⇒ nikdo, beze změny“) a změna by byla nová logika (kandidát, jehož kouč odmítne, se
  „nepokouší“ ⇒ druhý smí; u dvou kandidátů různých koučů by bylo třeba ptát se obou a řešit pořadí odpovědí).
- **Návrh:** rozhodnout výklad. Varianta A (dnes): dva v dosahu ⇒ nikdo, bez otázky. Varianta B: ptát se každého kandidáta
  (kouč/AI); skočí-li právě jeden, chytá; skočí-li dva a víc, nikdo. B je věrnější slovu „attempting“, ale znamená až dvě otázky
  pro jeden dopad a změnu v obou enginech (`divingCatcherFor` / `findDivingCatchPlayer` + volání).

## 6. LOW / k rozhodnutí — Grab × Side Step proti spoluhráči → **neopraveno, návrh pro uživatele**
- Dnes oba enginy (C++ `block_handler.cpp:561`, PHP `BlockHandler.php` `$maSideStep && !$maGrab`) ruší Side Step spoluhráče,
  kterého B&C s Grabem odtlačí, i když Grab proti spoluhráči nepůsobí. Výklad ř. 8150-8153 („When making a Block or Blitz Action,
  Grab and Side Step will cancel each other out“) vs. ř. 7830-7831 (B&C Grab jen „as if a Block Action“) je otevřený, parita drží.
- **Návrh:** pokud Grab proti spoluhráči „neexistuje“, neměl by rušit ani jeho Side Step ⇒ rušit jen proti soupeři (jedna podmínka
  v obou enginech + test). Ponecháno uživateli — není to zjevná oprava s jediným řešením.

## Kontrola
- `make check`: **„== check: vše zelené“** (log mimo git `/tmp/c_oprava_scratch/check.log`). Formát PHP OK, PHPStan bez chyb.
- Dva enginy: nález 1 a 3 jen PHP (C++ přehrání nemá — ptá se přes `CoachChoicePending`), 2 oba, 4 jen C++ test.

HOTOVO
