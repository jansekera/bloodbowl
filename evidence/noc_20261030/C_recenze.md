# Noc 30.10.2026 — úkol C: recenze (oddělený průchod)

Rozsah: `git diff main...oprava-grab-dc` (590893aa Grab B&C, ed780a67 Diving Catch). Pravidla: `rules_bb2016.txt` (main checkout).

## Ověření „test bez opravy spadne“ (vlastní běh)
- PHP Grab: odstraněna podmínka spoluhráče v `BlockHandler::grabSquares` ⇒ spadnou 3:
  `BallAndChainGrabTest::testGrabSeNaSpoluhraceNepouzije`, `::testKoucSeNaGrabProtiSpoluhraciNepta`,
  `GrabKoucTest::testStandFirmVlastnihoTymuBncProtiVlastnimuGrabOtazku`. Po `git checkout` zpět 32/32 OK. ✔
- C++: `pushSkills` bez podmínky týmu + `divingCatchChosen` = vždy `aiJumps` ⇒ spadnou 2 Grab
  (`GrabIsNotUsedAgainstATeammate`, `LiveGameDoesNotAskAboutGrabAgainstATeammate`) + 6 Diving Catch (přesně těch 6,
  které uvádí autor). Po `git checkout` + přestavbě 54/54 zelené. ✔
- PHP Diving Catch: `BallResolver::divingCatchJumps` = vždy `$aiJumps` ⇒ spadne 10 ze 14 testů `DivingCatchKoucTest`
  (sedí s tvrzením autora). Po `git checkout` zpět. ✔
- Python `python/tests/test_coach_choice.py`: 18 passed (vlastní běh). ✔
- Strom po každé mutaci vrácen (`git status` = jen tento soubor + nesledované symlinky).

## Ověřená tvrzení autora
- Citace pravidel sedí: Grab ř. 8146-8150 („grab his opponent … adjacent to his opponent to push back his opponent“),
  B&C ř. 7820-7822 („friend or foe“), ř. 7830-7831 (Grab „with his blocks“), Diving Catch ř. 8065-8071, ř. 1820.
- Strážné testy „ležící ⇒ bez Grabu“ existují v obou enginech (`P222PushingAProneOccupantIsNotABlockSoNoGrab`,
  `testOdtlaceniLezicihoNeniBlokTakzeBezGrabu`).
- AI beze změny: logika `aiJumps` u výkopu je přesná negace staré podmínky (C++ `kickoff_handler.cpp:92`, PHP
  `KickoffResolver.php:105`); přihrávka/vhazování mimo kouče skočí jako dřív.
- Parita C++/PHP u obou změn drží (Grab: `pushSkills` × `grabSquares`; DC: `divingCatchChosen` × `divingCatchJumps`).
- Přehrání v PHP: engine nemá jiný zdroj náhody než `DiceRollerInterface` (grep `random_int|mt_rand|shuffle` v `src/Engine`
  prázdný mimo kostky), DTO jsou immutable, interaktivní příznaky resolveru při přehrání shodné (tah AI ⇒ `isHumanTurn`
  false v obou průchodech); výkop jde jen přes `SetupHandler` uvnitř `ActionResolver::resolve`, takže výjimka
  `DivingCatchQuestion` nikde neuteče mimo `resolveSPrehranim`.
- Artefakty v gitu: žádné (diff obsahuje jen zdrojáky, testy a `C_autor.md`).
- Živá partie (C++): výkop běží přes `bb.kickoff_coach` se scope kouče ⇒ otázka u výkopu v partii přijde (obecný mechanismus).

## Nálezy

1. **LOW — latentní past: `ActionResolver` z kontejneru přehraje špatně.** `src/Container/ServiceProvider.php:99-110`
   předává `BallResolver`/`PassResolver`/`KickoffResolver` postavené s vlastním `RandomDiceRoller` (ř. 79-90), takže jejich
   hody nejdou přes `ReplayDiceRoller` (`src/Engine/ActionResolver.php:70-73`). Scénář: kdo by použil `$c->get(ActionResolver::class)`,
   po odpovědi `RESOLVE_DIVING_CATCH` se přihrávka přehraje s NOVÝMI hody — míč dopadne jinam a uložená odpověď se použije na
   jinou otázku/jiného hráče, nebo zůstane nevyužitá. Dnes registraci nikdo nepoužívá (grep `ActionResolver::class` jen
   definice) a autor to přiznává v bodě 2 jen komentářem. Podle zásady „žádné fallbacky“ by registrace měla zmizet nebo
   konstruktor odmítnout předané resolvery; nenaprogramováno (není to zjevná oprava s jediným řešením).

2. **LOW — chybí test: otázka u výkopu pro PŘIJÍMAJÍCÍHO hráče kouče-člověka** (nejčastější případ výkopu).
   `engine/tests/test_kickoff_handler.cpp:1525-1575` a `tests/Engine/DivingCatchKoucTest.php` (`vykop`) testují jen kopajícího
   na lajně. Mutace typu „ptát se jen kopajícího“ / „přijímající v přijímající polovině rovnou skočí“
   v `kickoff_handler.cpp:92` nebo `KickoffResolver.php:105` by testy nechytily.

3. **LOW — chybí test: dvě otázky Diving Catch v jedné akci (PHP fronta odpovědí).** `ActionResolver.php:240-246`
   skládá `[...$pending->getAnswers(), nová]`; žádný test nemá frontu délky 2. Scénář: přihrávka → skok, neúspěšné chytání →
   odraz z pole hráče za postranní čáru → vhazování na prázdné pole vedle jiného hráče s DC → druhá otázka; přehrání musí
   spotřebovat první odpověď u první otázky. Kód podle čtení správně, jen nehlídaný.

4. **LOW — test, který projde i s jinou otázkou.** `engine/tests/test_ball_and_chain_handler.cpp:279-288`
   (`LiveGameDoesNotAskAboutGrabAgainstATeammate`): v `catch` přijme JAKOUKOLI jinou `CoachChoicePending` a nic dalšího
   nekontroluje. Kdyby se v budoucnu kouč ptal dřív na něco jiného (např. natočení B&C), test projde, aniž by B&C došel ke
   spoluhráči. Dnes spadne (ověřeno mutací), takže jen křehkost.

5. **LOW / k rozhodnutí uživatele — „two or more players ATTEMPTING“ (ř. 8069-8071).** Teď, když je skok volba,
   lze číst, že kandidát, jehož kouč odmítne, se „nepokouší“, a druhý tedy smí. Oba enginy ruší oba kandidáty bez ptaní
   (`divingCatcherFor` / `findDivingCatchPlayer`) — přesně podle zadání („dva a víc ⇒ nikdo, beze změny“), takže to není
   vada proti zadání, jen upozornění.

6. **LOW / k rozhodnutí uživatele — Grab × Side Step proti spoluhráči** (souhlasím s bodem 1 autora). C++
   `block_handler.cpp:561` a PHP `BlockHandler.php:1868-1882` (`$maSideStep && !$maGrab`) dál ruší Side Step SPOLUHRÁČE,
   kterého B&C s Grabem odtlačí, i když Grab proti němu teď nepůsobí. Ř. 8150-8151 mluví o „Block or Blitz Action“; B&C používá
   Grab jen „as if a Block Action“ (ř. 7830-7831). Parita drží, výklad otevřený.

Nesplněné body zadání: žádné jsem nenašel. Nepravdivá tvrzení v zápisu autora: žádná nalezena (ověřené počty spadlých
testů sedí; tvrzení „žádná předvolba partie nemá hráče s Diving Catch“ jsem neověřoval). Žádný HIGH/MEDIUM nález.

## Kontrola
- `make check` na HEAD (ed780a67) + tento soubor: **„== check: vše zelené“** (log mimo git `/tmp/c_recenze_scratch/check.log`).

HOTOVO
