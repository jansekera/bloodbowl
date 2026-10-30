# D — recenze (Diving Catch varianta B)

Recenzent, oddělený průchod. Průběžný zápis.

## Průběh
- přečten zápis autora, diff (20 souborů, 1 commit b679118c nad main); jdu ověřovat pád testů na starém kódu (C++ i PHP).
- OVĚŘENO pád na starém kódu (zdrojáky z b679118c^, testy z HEAD, pak `git checkout HEAD --`, strom = HEAD):
  - PHP `DivingCatchKandidatiTest`: 14 testů, 1 chyba + 12 selhání (= tvrzení autora „13 z 14“ sedí);
  - C++ `DivingCatch*:KickoffLanding*`: 53 testů, 43 prošlo ⇒ **10 spadlo** (= tvrzení autora sedí).
- citace pravidel ověřeny v `rules_bb2016.txt`: ř. 985 („you never have to use a skill“), ř. 986-990 („the coach whose turn is taking place must use his skill first“), ř. 8065-8071 (Diving Catch, „attempting … neither can use it“), ř. 1820 („Skill use is not mandatory“) — sedí.
- parita C++ × PHP: `divingCatchAttempt` v obou stejná struktura (strana na tahu → druhá; člověk: otázka na každého, stop při 2 pokusech; AI: nejlepší `calculateCatchTarget`/`getCatchTarget`, shoda ⇒ první v pořadí; právě 1 ⇒ chytá). Cíl chytání v obou enginech stejný (AG, zóny, NoS, DP, Extra Arms, déšť, clamp 2–6). Výkop: C++ `finishKickoff` nastaví `activeTeam = receiving` před `landBall` (kickoff_handler.cpp:354-355), PHP explicitně `naTahu: $receivingTeam` — shodné.
- PHP mutace (moje): `KickoffResolver` `naTahu: $receivingTeam->opponent()` ⇒ **celé tests/Engine (1158) zelené** — pořadí stran u výkopu nehlídá žádný test (viz nález 1). C++ obdobná mutace běží.
- C++ mutace (moje): v `landBall` předán `divingCatchAttempt` stav s `activeTeam = kopající` ⇒ `*Kick*:*Diving*:*Coach*:*Touchback*` **213/213 zelené** (celá sada s mutací běžela >10 min — není to zaseknutí, celá `bb_tests` tu trvá ~11 min i na HEAD; ukončena přes PGID; zdroják vrácen `git checkout HEAD --`). Potvrzuje nález 1 i v C++.
- artefakty v gitu: diff obsahuje jen zdrojáky, testy a `D_autor.md` — žádné logy/korpusy/symlinky. OK.
- větev je na originu (`git ls-remote` = b679118c = HEAD).

## Nálezy

1. **MEDIUM — pořadí stran u výkopu nehlídá žádný test (oba enginy).** `src/Engine/KickoffResolver.php:105` (`naTahu: $receivingTeam`) a `engine/src/kickoff_handler.cpp:354` (`activeTeam = receiving` před `landBall`). Výkop je jediné místo, kde se „strana na tahu“ určuje zvlášť (v PHP explicitním argumentem, protože aktivní tým je tam jiný), a autor sám píše, že jde o rozhodnutí (přijímající vs. kopající první). Ověřeno mutací: PHP `naTahu: $receivingTeam->opponent()` ⇒ celé `tests/Engine` 1158/1158 zelené; C++ `activeTeam = kopající` pro `divingCatchAttempt` ⇒ 213/213 zelené. Scénář: někdo „opraví“ výkop na „kopající první“ (nebo PHP argument vypadne a vezme se aktivní tým z rozestavování) ⇒ při výkopu s přijímajícím člověkem a kopajícím člověkem na lajně dostane první otázku kopající, přijímající uvidí „už skáče“ — a žádný test nespadne. Chybí test: výkop, kandidát přijímajícího + kandidát kopajícího na lajně, oba lidé ⇒ první otázka = přijímající, druhá nese `otherId/attemptingId`.

2. **LOW — počet „dalších kandidátů“ v otázce je pevný, nezohlední, kdo už odmítl.** `engine/src/ball_handler.cpp:144` (`candidates.size() - 1`), `src/Engine/BallResolver.php:74` (`count($kandidati) - 1`). Scénář: HOME má dva kandidáty, na prvního kouč odpoví „ne“; druhá otázka pořád říká „další 1 s Diving Catch to stihne — skočí-li dva a víc, ruší se“ (živá partie) / „1 other player … If two or more try … nobody catches“ (web), přestože ten druhý už odmítl a riziko zrušení od něj není. Neovlivní výsledek, jen může kouče odradit. Zadání („kolik dalších kandidátů“) to doslova splňuje — k rozhodnutí uživatele, ne vada pravidel.

3. **LOW (návrh, NEPROGRAMOVAT — nová logika AI) — kopající AI u výkopu neskočí ani tehdy, když by tím zrušila pokus přijímajícího.** `engine/src/kickoff_handler.cpp:92`, `src/Engine/KickoffResolver.php:103-104`. Ve variantě B rozhoduje kopající jako druhý a ví, že přijímající skáče; jeho „pokus“ by pak nevedl k touchbacku (dva pokusy ⇒ nikdo, ř. 8069-8071), jen by přijímajícímu vzal jisté chytání. Dnešní predikát `aiJumps` (kopající na lajně ⇒ ne) je podle zadání („dnešní pravidlo“), takže to není vada implementace — návrh pro uživatele: kopající AI na lajně skočí, právě když už skáče přijímající. Obdobně volba „nejlepšího“ AI kandidáta podle cíle hodu ignoruje Catch (přehoz) — také jen návrh.

4. **LOW — nepravdivé tvrzení v zápisu autora.** `evidence/noc_20261030/D_autor.md:40`: v oddílu „Nedělal jsem“ stojí „commit + push větve dc-varianta-b“, ale commit b679118c existuje a je na originu (ověřeno `git ls-remote`). Patrně položka průběhu zapsaná do špatného oddílu; ráno by mohla svést k dalšímu commitu/pushi.

### Ověřeno a v pořádku (bez nálezu)
- Testy padají bez opravy: PHP 13/14, C++ 10 (sám ověřeno, viz Průběh); pokryty všechny body zadání (jeden kandidát; dva téže strany ano/ne, ne ⇒ druhý chytá; dvě strany — pořadí, oba ano ⇒ nikdo, jen jeden ⇒ chytá; AI×AI téže strany ⇒ jeden, a to nejlepší — test odliší „první“ od „nejlepšího“ kostkou 3 při AG3/AG4; AI×člověk v obou pořadích; uložení otázky přes toArray/fromArray; výkop AI se dvěma přijímajícími).
- Odražený míč se dál nechytá (`bounced` v C++, odraz v PHP nevolá `resolveLanding`); obsazené pole ⇒ žádní kandidáti.
- Po dvou pokusech se další otázky nekladou — výsledek už je daný (ř. 8069-8071), odpověď by nic nezměnila; v pořádku.
- Replay odpovědí v PHP (`array_shift` v deterministickém pořadí kandidátů) konzistentní mezi přerušením a přehráním.

## Kontrola
- `make check` na HEAD (b679118c, strom = HEAD + tento soubor) ⇒ **„== check: vše zelené“**: PHPUnit 1595 OK, vitest 101, C++ 1323 PASSED (683 s), kouř 6 her. Čísla sedí s tvrzením autora. Log mimo git `/tmp/d_rec/check.log`.
- Shrnutí: vada pravidel ani parity nenalezena; 1× MEDIUM (chybí test pořadí stran u výkopu, mutace přežije v obou enginech), 3× LOW (text otázky, návrh AI, chybný řádek v zápisu autora).

HOTOVO
