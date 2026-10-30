# Noc 30.10.2026 — úkol B (nálezy review P222, P223)

Průběžný zápis.

## Prostředí (pozor pro ráno)
- `vendor` ve worktree je symlink na main ⇒ `vendor/bin/phpunit` načítá `App\` z **main/src**, ne z worktree.
  Testy proto pouštím s bootstrapem `/tmp/noc_b/bootstrap.php` (přemapuje `App\` a `App\Tests\` na worktree).
  `make check` tím trpí taky — pouštím jeho kroky ručně se stejným bootstrapem (viz níže).
- Chyběl `.env` (DB port 5439) ⇒ 76 chyb DB testů; zkopírován z main (je v `.gitignore`).
- Soubor pravidel `rules_bb2016.txt` je jen v main (necommitovaný), čtu ho odtud.

## 1. MEDIUM P223 — PHP výkop: touchback po odrazu ven / za lajnu (parita s C++)
- **Co bylo špatně:** PHP po dopadu výkopu volalo `BallResolver::resolveLanding` a dál běžná pravidla:
  odraz mimo hřiště ⇒ **vhazování**, odraz do kopající poloviny ⇒ míč tam ležel / chytil ho kopající.
  Pravidla ř. 279-281 („scatters **or bounces** off the pitch or into the kicking team's half … touchback“),
  FAQ ř. 9316-9318 („any event that causes the ball to go out of bounds or over the line of scrimmage during
  a kick-off results in a touchback“). C++ `kickoff_handler.cpp` `landBall` to dělá správně (kontrola
  na začátku každé smyčky odrazu) — tentýž případ.
- **Test** (`tests/Engine/KickoffResolverTest.php`, 4 nové): odraz z (12,7) za lajnu; odraz na stojícího
  kopajícího za lajnou (nesmí chytit); odraz ven (ne vhazování); neúspěšné chytání a odraz za lajnu.
  **Spadly na starém kódu: ano, 4/4.**
- **Oprava:** `resolveLanding` / `resolveCatch` / `resolveBounce` / `resolveCatchFromBounce` dostaly
  volitelné `?TeamSide $kickoffReceiving`; při výkopu odraz mimo přijímající polovinu míč nechá na tom poli
  a nehází dál; `KickoffResolver` pak dá touchback (stejná podmínka jako u rozptylu výkopu).
- **Mutace:** (a) vypnutý strážce v `resolveBounce` ⇒ 2 testy padají; (b) vypnutá kontrola v
  `KickoffResolver` ⇒ 4 testy padají. **Zabity obě.**
- PHPUnit celý: 1546 OK (před přidáním 4. testu).

## 2. LOW P223 — `depth` u vhazování v PHP
- **Co bylo špatně:** P223 přepojilo chytání po vhazování na `resolveLanding` → `resolveCatch`; ten při
  neúspěchu odrážel s hloubkou 0 ⇒ pojistka `MAX_BOUNCES` (strop řetězu odrazů, není to pravidlo) neplatila
  pro smyčku „odraz ven → vhazování → nechytí → odraz ven“. Dřív (`resolveCatchFromBounce`) se hloubka nesla.
- **Test:** `BallResolverTest::testBounceChainThroughThrowInsWithFailedCatchesStopsAtTheSafetyCap` — dva hráči
  u horní lajny, kteří nikdy nechytí, kostky na 30 kol smyčky. **Spadl na starém kódu: ano** (řetěz spotřeboval
  všech 211 kostek).
- **Oprava:** `resolveCatch(..., int $depth = 0)`; `resolveLanding` hloubku předá.
- **Mutace** (`depth: 0`): test padá. **Zabita.**

## 3. LOW P223 — Dump-Off +1 bez testu / komentáře (PHP)
- **Co bylo:** P223 dalo přesnému Dump-Off `accuratePass: true` ⇒ +1 za přesnou (ř. 891) a nově i +1 z Diving
  Catch (ř. 8063-8064). Chování je podle pravidel (Dump-Off = Quick Pass, „normal throwing rules apply“,
  ř. 8097-8098), ale bez testu a komentáře.
- **Testy** (`DivingCatchTest`): přesný Dump-Off na AG2 bez DC chytá 4 (jen s +1); s DC chytá 3 (jen s oběma +1).
  Na starém kódu (před P223: jen `modifier: 1`) by padl druhý; na současném kódu procházejí (chování bylo
  správné, chyběl test).
- **Komentář** v `PassResolver::resolveDumpOff`.
- **Mutace** (`accuratePass: false` u Dump-Off): oba testy padají. **Zabita.**

## 4. LOW P223 — chybějící testy Diving Catch (oba enginy) + strážní testy C++
Chování bylo v obou enginech správné ⇒ testy jsou strážní; že umí spadnout, ověřeno mutací.

| případ | PHP (`DivingCatchTest`) | C++ | mutace (PHP / C++) |
|---|---|---|---|
| neúspěch skoku u výkopu — odraz z pole skákajícího (ř. 8068-8069) | `testFailedDivingCatchOnAKickOff…` | `KickoffLanding.FailedDivingCatchBounces…` | skok vypnut ⇒ padá / padá |
| přehoz při vhazování — vlastní tah smí (ř. 929-933) | `testDivingCatchOfAThrowInMayUseATeamReroll…` | `DivingCatch.ThrowInDivingCatchMayUse…` | bez přehozu ⇒ padá / padá |
| přehoz při vhazování — soupeřův tah nesmí | `testDivingCatchOfAThrowInGetsNoTeamReroll…` | `DivingCatch.ThrowInDivingCatchGetsNo…` | přehoz komukoli ⇒ padá / padá |
| skok soupeře u přesné přihrávky (+1, ne +2) | `testOpposingDivingCatchPlayerCatchesAnAccuratePass…` | `DivingCatch.OpposingDivingCatchPlayerCatchesAnAccurate…` | +2 při skoku ⇒ padá / padá |
| Pro po neúspěšném skoku (ř. 8385-8389) | `testDivingCatchMayUseProToReroll…` | `DivingCatch.DivingCatchMayUsePro…` | Pro vypnut ⇒ padá / padá |

- Touchback po odrazu ven a po nechyceném míči za lajnu C++ už testy měl (`BounceOffThePitchIsATouchback`,
  `FailedCatchBouncingOverTheLineOfScrimmageIsATouchback`) — duplikáty jsem nepřidával. Opravena citace
  v tom druhém: FAQ ř. 9312-9314 → **9315-9318** (Q/A o touchbacku je na 9315-9318).
- **Týž princip jinde (C++, NEopraveno, návrh):** C++ `resolveThrowIn` → `resolveLanding` → `resolveBounce(…, 0, …)`
  nenese hloubku vůbec (ani před P223) ⇒ pojistka `depth > 200` v C++ smyčku „odraz ven → vhazování → nechytí“
  nezastaví. S náhodnými kostkami prakticky nenastane; s `FixedDiceRoller` v testu, který kostky cyklí, by mohla
  přetéct zásobník. Oprava = přidat `depth` do `resolveThrowIn`/`resolveLanding` (změna hlaviček) — nechávám na ráno.

## 5. LOW P222 — testy a komentáře (C++, Python)
- **Testy Side Step bez pozitivní kontroly** (`test_coach_choice.cpp` `SideStepWithNoOpenSquare_NotAsked`,
  `SideStepInChainWithNoOpenSquare_NotAsked`): jen `EXPECT_NO_THROW` ⇒ prošly by, i kdyby se na Side Step neptalo
  nikdy. Teď každý projde tutéž sestavu i s jedním volným polem a čeká otázku `USE_SIDE_STEP` od správného hráče
  (12, v řetězu 14). Při psaní kontrola napoprvé spadla — uvolnil jsem pole u hráče 15, přes kterého řetěz nevede
  (dobrý důkaz, že kontrola něco měří). **Mutace:** „ptá se i bez volného pole“ ⇒ oba padají; „neptá se nikdy“
  ⇒ oba padají (tuhle staré testy nechytily). **Zabity.**
- **Test vazby nečte číslo 15** (`python/tests/test_coach_choice.py`): `test_grab_otazka_a_odpoved` si slovník
  `"USE_GRAB"` vyrobí sám. Nový `test_grab_otazka_prijde_z_vazby_jako_use_grab`: Black Ork TV1500 s Grabem blokuje
  člověka bez Side Step / Stand Firm, otázka přijde z enginu přes `execute_action_coach`. **Mutace** (`names[15]`
  → `"?"` v `bb_module.cpp`): nový test padá, starý prochází. **Zabita.**
- **Rozpůlený komentář P201(12)**: blok P222 (3) byl vložen doprostřed komentáře P201(12); hlavička P201(12)
  vrácena celá nad testy `CoachChoiceFollowUp` (znění z původního commitu).
- **`REROLL` jako „žádná otázka“**: `firstQuestion` vracela `REROLL`, když se engine nezeptal ⇒ test
  `OpponentsGrabStaysAutomatic` by prošel i se skutečnou otázkou na přehoz. Teď `std::optional`, `nullopt` = ticho.
  (Úprava testu, bez mutace — nejde o chování enginu.)
- ❓ **B&C s Grabem proti spoluhráči** — NEMĚNĚNO, otázka pro uživatele. Oba enginy dovolí Grab i při bloku B&C
  na spoluhráče. Pravidla ř. 7820-7822: B&C „will throw a block following normal blocking rules against whoever
  is in that square, **friend or foe**“; ř. 7829-7831: „may use the Grab skill … **with his blocks**“; Grab
  ř. 8148-8150 „adjacent to **his opponent**“. Čtu „opponent“ jako „blokovaný hráč“ ⇒ dnešní chování sedí.
  Opačné čtení (Grab jen proti soupeři) by byla změna v obou enginech (`pushSkills` / `grabSquares`).

## NEDĚLÁNO — návrhy a otázky pro uživatele
- **MEDIUM P223: Diving Catch jako otázka kouči** (nová volba). Dnes skáče automaticky každý jediný kandidát
  (kromě kopajícího u výkopu, kde by skok dal touchback). Pravidla ř. 8066 „can attempt“ ⇒ kouč-člověk by měl
  dostat otázku (C++ `CoachQuestion` nové číslo 16, PHP čekající otázka jako `PendingGrabDTO`). Pro AI pravidlo:
  skočit vždy, kromě případů, kdy skok škodí (výkop do kopající poloviny; soupeř chytá u jeho lajny?). ❓
- **Sjednocení „rozhodnutí na dvou místech“** (LOW P223, refaktor): v PHP rozhoduje o skoku u výkopu
  `KickoffResolver` i `BallResolver::resolveLanding` (parametr `divingCatch`); v C++ `landBall` i
  `resolveLanding`. Návrh: jedna funkce „kdo smí/chce skočit“ (kandidát + výjimka kopajícího) v každém enginu.
- **AI ocenění +1 DC v `one_cage.cpp`** (mění chování AI) — nedotčeno.
- ❓ z P222: fanatik odtlačující LEŽÍCÍHO a Grab (ř. 7823-7825 + 7830); otázka na Grab i ve webové hře — PHP
  ji podle `PendingGrabDTO` (P224) už má, ověřit, zda je P222 bod uzavřen; B&C Grab proti spoluhráči (viz bod 5).
- **C++ hloubka odrazů přes vhazování** — viz bod 4 (týž princip jako PHP `depth`, v C++ odjakživa).
- **Prostředí worktree:** `make check` jsem pouštěl s `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini` (auto_prepend, který
  přemapuje `App\` na worktree) — jinak by PHPUnit/PHPStan běžel na kódu z main. Pro příští noční worktree:
  `vendor` kopírovat, ne symlinkovat (nebo `composer dump-autoload` ve worktree).

## Kontrola
- `PHP_INI_SCAN_DIR=:/tmp/noc_b/ini make check` ⇒ **„== check: vše zelené“** (lint čistý, PHPStan bez chyb,
  PHPUnit 1555 OK, vitest 95, C++ 1294 testů, kouř 6 her). PHPStan napoprvé našel chybějící `@param list<SkillName>`
  v novém testu — opraveno.
- Python `python/tests/test_coach_choice.py`: 16 passed (`check-py` celé nespuštěno).

Commity: `0d2c8a0e` (PHP), `f8a3668f` (C++ a Python testy), souhrn tento soubor. Větev pushnuta na origin/noc-p223-nalezy.

HOTOVO
