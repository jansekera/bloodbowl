# Noc 30.10. — úkol B: recenze (oddělený průchod)

Recenzent, čerstvý kontext. Předmět: `git diff main...noc-p223-nalezy` (commity `0d2c8a0e`, `f8a3668f`, `de010ca9`),
zadání `B_autor.txt`, zápis `B_autor.md`. Pravidla: `rules_bb2016.txt` (v main, necommitovaný).

## Co jsem sám ověřil

- **Testy padají bez opravy (PHP):** `src/Engine/{Ball,Kickoff,Pass}Resolver.php` dočasně vráceny na `main`
  ⇒ padá 5 nových testů: 4× `KickoffResolverTest::testKickoff…Touchback…` (failure) a
  `BallResolverTest::testBounceChainThroughThrowInsWithFailedCatchesStopsAtTheSafetyCap` (error — došly kostky).
  Po vrácení na HEAD 73/73 OK. Tvrzení autora „4/4 spadly“ a „depth test spadl“ **sedí**.
- **Mutace C++ (Side Step):** `usesSideStep` dočasně „neptá se nikdy“ (`if (false && coachDecides…)`,
  `block_handler.cpp:434`) ⇒ oba přepsané testy `CoachChoiceBlock.SideStep*` padají (staré by prošly). **Sedí.**
- Strom po každém pokusu vrácen `git checkout HEAD -- <soubor>`; `git status` čistý (kromě tohoto souboru
  a necommitovaných symlinků). `bb_tests` po mutaci přestavěn z HEAD.
- **Prostředí (tvrzení autora pravdivé):** `vendor` je symlink do main ⇒ `vendor/bin/phpunit` bez přemapování
  načítá `App\` z **main/src** — `KickoffResolverTest` tak ve worktree hlásí 4 failures. Holé `make check` v tomto
  worktree by tedy PHP testovalo na kódu z main. Autor to obešel `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini`.
- **C++ hloubka odrazů přes vhazování (návrh autora, NEopraveno):** potvrzeno — `ball_handler.cpp:144`,
  `:156`, `:310` volají `resolveBounce(…, 0, …)`, `resolveThrowIn` (`:305`) hloubku nenese.
- **Parita s C++:** PHP teď u výkopu dělá totéž co C++ `landBall` (`kickoff_handler.cpp:72-114`): každý odraz
  ven / do kopající poloviny = touchback, kopající za lajnou nechytá, odraz na prázdné pole zůstane. Rozdíl jen
  v pojistce (PHP `MAX_BOUNCES = 5` × C++ 200 kol) — není pravidlo, předchozí stav.
- **Artefakty v gitu:** žádné — diff obsahuje jen `src/`, `tests/`, `engine/tests/`, `python/tests/`, `B_autor.md`.
- **Body zadání:** všechny splněné nebo zapsané jako návrh/otázka (DC jako otázka kouči, sjednocení dvou míst,
  `one_cage.cpp`, B&C Grab proti spoluhráči ❓). `task_queue.md` nedotčen.

## Nálezy

Žádný HIGH. Opravy jsou proti pravidlům správné, nález MEDIUM P223 opravdu odstraněn a paritní s C++.

1. **MEDIUM — oprava touchbacku je testem hlídaná jen na 3 z 8 míst, kudy se `kickoffReceiving` šíří.**
   `src/Engine/BallResolver.php:140`, `:153`, `:366`, `:531`, `:615`. Ověřeno mutací: na každém z těch pěti řádků
   jsem zvlášť zahodil argument `$kickoffReceiving` ⇒ **celé `tests/Engine/` zelené (1118 OK) pětkrát**. Hlídané jsou
   jen `:258` (dopad na prázdné pole), `:212` (neúspěšné chytání) a strážce v `resolveBounce`.
   Scénář, který dnes nic nehlídá: výkop na prázdné (12,6), odraz na stojícího přijímajícího na (12,7), ten nechytí
   (`resolveCatchFromBounce`, ř. 615 bez parametru), odraz na V na (13,7) ke **stojícímu kopajícímu** ⇒ kopající
   chytá a drží míč (`isHeld()` ⇒ `KickoffResolver.php:122` touchback nedá). Totéž: odraz přes ležícího hráče
   (ř. 366) nebo hráče bez zón / No Hands (ř. 140, 153) ven ⇒ vhazování místo touchbacku. Pravidla ř. 280-282,
   FAQ ř. 9315-9318. Chování je dnes správné; chybí test na chokepoint — doporučuji jeden test „odraz → nechytí →
   odraz za lajnu na stojícího kopajícího“ (zabije ř. 615) a jeden přes ležícího hráče ven (ř. 366).
   Pozn.: dvojí mechanismus (strážce v `resolveBounce` + dodatečná kontrola v `KickoffResolver`) část mutací maskuje
   — míč ležící v kopající polovině chytí dodatečná kontrola, ale kopající, který míč chytil, ne.

2. **LOW — citace pravidel posunutá o řádek.** „ř. 279-281“ (`BallResolver.php:340`, `KickoffResolver.php:121`,
   `KickoffResolverTest.php:143`, `B_autor.md` bod 1) — věta „If the ball scatters or bounces off the pitch or into
   the kicking team's half … ‘touchback’“ je na **ř. 280-282**. Podobně „ř. 8066 can attempt“ je na ř. 8065
   (zděděné z P223). FAQ ř. 9315-9318, ř. 8097-8098 (Dump-Off), ř. 891, ř. 929-933 sedí.

3. **LOW — testy Dump-Off hlídají jen spodní hranici.** `tests/Engine/DivingCatchTest.php:262-270` (`testAccurateDumpOff…`): oba testy
   hodí přesně na práh (4 resp. 3) a čekají chycení. Ověřují, že bonus **je** (mutace `accuratePass: false` je
   zabije — tvrzení autora sedí), ne že **není větší** (+2 bez DC / +3 s DC by prošlo). Doplnit případ „o jedna
   méně nechytí“ — stejně jako to dělá `testOpposingDivingCatchPlayerCatchesAnAccuratePass…` (hod 2 × 3).

4. **LOW — test touchbacku čte volbu příjemce touchbacku.** `KickoffResolverTest.php:209`
   `assertSame(3, …getCarrierId())`: hráči 1 a 3 jsou oba přijímající; že míč dostal 3, je heuristika
   `resolveTouchback`, ne pravidlo („give the ball to any player“, ř. 282). Změna heuristiky test shodí, aniž by
   se rozbil touchback. Stačí „míč drží přijímající tým“.

5. **LOW / info — prostředí, ne kód.** `make check` v tomto worktree bez `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini`
   testuje PHP na **main/src** (ověřeno: holé `vendor/bin/phpunit tests/Engine/KickoffResolverTest.php` ⇒ 4
   failures). Kdo bude větev ráno ověřovat ve worktree, musí přemapování použít (nebo ověřit až po merge v main).
   Autor to v zápisu uvádí správně.

6. **LOW — C++ hloubka odrazů přes vhazování** (autorův návrh, potvrzuji): `engine/src/ball_handler.cpp:144`,
   `:156`, `:310` a `resolveThrowIn`→`resolveLanding` (`:305`) startují hloubku od 0; pojistka `depth > 200` smyčku
   „odraz ven → vhazování (max 8) → nechytí → odraz ven“ nezastaví. S `FixedDiceRoller`, který kostky cyklí, hrozí
   přetečení zásobníku v testu. Převod PHP opravy do C++ = řádek „převést do druhého enginu“.

Nepravdivá tvrzení v `B_autor.md` jsem nenašel. Drobnost: u pozitivní kontroly Side Step v řetězu autor píše
„uvolnil jsem pole u hráče 15“ — v kódu je odebrán hráč 3 na (11,8), který sousedí s 14 i 15; komentář v testu
(„řetěz jde přes 14“) i test (`playerId == 14`) jsou v pořádku.

## Kontrola

- `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini make check` ⇒ **„== check: vše zelené“** (PHPUnit 1555 OK, C++ 1294 PASSED,
  kouř). Čísla shodná se zápisem autora. Pozn.: běh se časově překryl s mými mutacemi `BallResolver.php` z nálezu 1;
  ty jsou všechny vůči testům neutrální (proto nález), takže výsledek platí; soubor byl po nich vrácen na HEAD.
- `python/tests/test_coach_choice.py`: 16 passed.
- Strom na konci = HEAD + tento soubor (necommitované symlinky `vendor`/`node_modules`/`venv` beze změny).

Verdikt: **sloučit**; nález 1 (test na chokepoint) doporučuji doplnit před nebo hned po sloučení.

HOTOVO
