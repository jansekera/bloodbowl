# Noc 30.10. — úkol B: opravný průchod (autor) po recenzi `B_recenze.md`

Průběžný zápis.

Prostředí: `vendor` je symlink do main ⇒ PHP testy pouštím s `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini`
(prepend přemapuje `App\` na worktree, viz nález 5).

## 1. MEDIUM — touchback hlídán jen na 3 z 8 míst — **opraveno (testy)**

`tests/Engine/KickoffResolverTest.php`: `testKickoffBallReachingAStandingKickingPlayerOverTheLineIsATouchback`
s poskytovatelem 5 cest — dopad na hráče bez zón / s No Hands (`resolveCatch` ř. 140, 153), odraz na hráče, který
nechytí (`resolveCatchFromBounce` ř. 615), odraz na No Hands (ř. 531), odraz přes ležícího (`resolveBounce` ř. 366).
Na konci každé cesty stojí kopající na (13,7) s hodem 6; požadavek = míč drží přijímající tým (ř. 280-282,
FAQ ř. 9315-9318). Fáze `KICKOFF` (bez týmového přehozu, ř. 1261-1263).
- Na kódu z main (bez opravy P223): **5/5 spadlo**.
- Mutace: na každém z 5 řádků zvlášť zahozen `$kickoffReceiving` ⇒ pokaždé **spadne právě 1 případ** (ten svůj).
  Soubor vrácen ze zálohy, `git diff src/` prázdný.
- Strukturální sjednocení (jeden mechanismus místo strážce + dodatečné kontroly v `KickoffResolver`) nedělám —
  refaktor nad rámec nálezu; zapsáno jako návrh na konci.

## 2. LOW — citace pravidel posunuté o řádek — **opraveno**

Ověřeno v `rules_bb2016.txt`: „scatters or bounces off the pitch or into the kicking team's half … ‘touchback’“
= ř. **280-282**; „the player can attempt to catch any pass, kick“ = ř. **8065**, „off or crowd throw-in, but not
bouncing ball“ = ř. 8066. Opraveno:
- `279-281` → `280-282`: `BallResolver.php`, `KickoffResolver.php`, `KickoffResolverTest.php`, `B_autor.md`.
- „can attempt“ / „any pass“ `8066` → `8065`, „any pass, kick off …“ `8066(-8067)` → `8065-8066(-8067)`:
  `BallResolver.php`, `KickoffResolver.php`, `DivingCatchTest.php`, `engine/src/kickoff_handler.cpp`,
  `engine/tests/test_diving_catch.cpp`, `engine/tests/test_kickoff_handler.cpp`, `B_autor.md`.
- „but not bouncing ball (ř. 8066)“ ponecháno — sedí.

## 3. LOW — testy Dump-Off jen spodní hranice — **opraveno (testy)**

`DivingCatchTest`: `testAccurateDumpOffIsNotCaughtOneBelowTheAccuratePassTarget` (hod 3, cíl 4+) a
`testAccurateDumpOffToADivingCatchPlayerIsNotCaughtOneBelowItsTarget` (hod 2, cíl 3+). Na správném kódu zelené.
Mutace: přesná přihrávka +2 místo +1 (`resolveLanding`) ⇒ spadnou **oba**; Diving Catch −2 místo −1
(`getCatchTarget`) ⇒ spadne **ten s DC**. Vráceno ze zálohy. (Test napřed spadnout nemohl — chování bylo
správné, jde o chybějící hranici; roli „musí spadnout“ plní mutace.)

## 4. LOW — test čte heuristiku příjemce touchbacku — **opraveno**

`KickoffResolverTest::testKickoffFailedCatchBouncingOverTheLineOfScrimmageIsATouchback`: místo `assertSame(3, …)`
teď „nosič patří přijímajícímu týmu“ (ř. 282 „give the ball to any player“). Totéž v novém testu z nálezu 1.
Ostatní touchback testy mají jediného přijímajícího hráče ⇒ `assertSame(1, …)` tam heuristiku nečte, ponecháno.

## 5. LOW / info — prostředí — **bez změny kódu (není vada kódu)**

`vendor` ve worktree je symlink na main ⇒ composer autoload mapuje `App\` na main/src. Ověřoval jsem
s `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini` (prepend přemapuje na worktree); bez něj nové testy z nálezu 1 ve worktree
padají 5/5 (= důkaz, že testují kód z main). **Ráno ověřovat buď s tímto přemapováním, nebo až po merge v main.**
Návrh pro uživatele: noční worktree zakládat s vlastním `vendor` (`composer install`), ne se symlinkem.

## 6. LOW — C++ hloubka odrazů přes vhazování — **opraveno (převod PHP opravy do C++)**

Test napřed: `engine/tests/test_ball_handler.cpp` `BallHandler.BounceChainThroughThrowInsWithFailedCatchesStopsAtTheSafetyCap`
(zrcadlo PHP testu; 300 kol smyčky kostek). Na starém kódu **spadl** (`std::out_of_range: no more rolls`, kostky
došly — pojistka nezabrala). Oprava: `resolveThrowIn(…, int depth = 0)` a `resolveLanding(…, bool accuratePass,
int depth = 0)` nesou hloubku; odraz ven → `resolveThrowIn(depth)`, dopad vhazování `depth + 1`, strop vhazování
`depth + 1`, odrazy v `resolveLanding` z `depth` (stejně jako PHP). Ostatní volající (blok, přihrávka, TTM) beze
změny — default 0 = začátek řetězu. Mutace: dopad vhazování zpět na `0` ⇒ test spadne; vráceno ze zálohy.
Řádek „převést do druhého enginu“ pro tuto opravu tím odpadá (PHP už má).

## Drobnost z recenze (hráč 3 vs. 15 v zápisu o Side Step)

Jen nepřesnost v textu `B_autor.md`, kód i test v pořádku — beze změny.

## Návrhy pro uživatele (neprogramováno)

- Touchback u výkopu v PHP stojí na dvou mechanismech (strážce v `resolveBounce` přes `$kickoffReceiving`
  + dodatečná kontrola v `KickoffResolver`); jeden mechanismus (např. výsledek „míč opustil přijímající polovinu“
  vracený z odrazu) by byl SSOT a nemaskoval mutace. Refaktor, ne oprava vady — k rozhodnutí.
- Vlastní `vendor` v nočních worktree (nález 5).

## Kontrola

`PHP_INI_SCAN_DIR=:/tmp/noc_b/ini make check` ⇒ **„== check: vše zelené“** (PHPUnit 1562 OK, vitest 95,
C++ 1295 PASSED, kouř). Log mimo git (`/tmp/noc_b/oprava/check.log`). `task_queue.md` nedotčen.

HOTOVO
