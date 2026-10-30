# Noc 30.10. — úkol D: Diving Catch, víc kandidátů, varianta B (autor)

Větev `dc-varianta-b` (z `oprava-grab-dc`).

## Průběh

- založeno, čtu C_autor / C_recenze / C_oprava
- přečteno C_*; C++: `divingCatcherFor` (kandidát jen když právě 1) + `divingCatchChosen` (ball_handler.cpp), volá `resolveLanding` a `landBall` (výkop). Plán: nová funkce `divingCatchAttempt` vrátí jediného skákajícího (nebo -1) po otázkách v pořadí strana na tahu → druhá.
- kickoff v C++: landBall běží s activeTeam = přijímající ⇒ u výkopu rozhoduje první přijímající
- C++ testy napsány (`DivingCatchCandidates.*` 12 v test_diving_catch.cpp, `KickoffLanding.AiDivesWithOnlyOneOfTwoReceivingDivingCatchers`); starý `DivingCatchCoach.TwoCandidatesNobodyIsAsked` (kódoval variantu A) odstraněn.
  Na STARÉM kódu: **spadlo 10**, prošly 3 strážné (jeden kandidát se ptá s roll 0/otherId -1, oba ano ⇒ nikdo, AI×AI obou stran ⇒ nikdo).
- C++ oprava: `divingCatchCandidates` + `divingCatchAttempt(state, at, aiJumps)` (ball_handler.cpp) nahradily `divingCatcherFor`/`divingCatchChosen`; volá `resolveLanding` i `landBall`. Otázka USE_DIVING_CATCH nově nese roll = počet DALŠÍCH kandidátů, otherId = kdo už skáče (-1). Po opravě 53/53 testů DivingCatch*/KickoffLanding zelených.
- PHP testy: nový `tests/Engine/DivingCatchKandidatiTest.php` (14); z `DivingCatchKoucTest` odstraněn `testDvaKandidatiNikdoANikdoSeNepta` (kódoval variantu A).
  Na STARÉM kódu: **13 z 14 spadne** (1 chyba = chybí getter otázky, 12 selhání chování); projde strážný AI×AI obou stran ⇒ nikdo.
- PHP oprava: `BallResolver::divingCatchCandidates` + `divingCatchAttempt($state, $landing, $aiJumps, $naTahu)` nahradily `findDivingCatchPlayer`/`divingCatchJumps`; `resolveLanding(divingCatcher: hráč|false|null)`; výkop (`KickoffResolver`) — první rozhoduje přijímající (parita s C++, kde je při dopadu na tahu přijímající). Otázka/`PendingDivingCatchDTO` nesou `otherCandidates`, `attemptingId` (i v toArray/fromArray). DivingCatch*Test: 53 OK.
- PHP mutace (`divingCatchAttempt`, vždy vráceno ze zálohy v /tmp/d_scratch):
  - AI bere prvního kandidáta místo nejlepšího ⇒ spadnou 2 (AI dva kandidáti, výkop dva přijímající);
  - pevné pořadí stran AWAY→HOME místo „na tahu první“ ⇒ spadnou 3 (pořadí otázek, jen jedna strana ano, člověk mimo tah);
  - stará mechanika (≠1 kandidát ⇒ nikdo) ⇒ spadne 12 z 14.
- Web: `DivingCatchModal.show(pending, jméno, kdoUžSkáče, onChoice)` — text o počtu dalších kandidátů a že se dva pokusy ruší; typ `PendingDivingCatch` + `otherCandidates`, `attemptingId`; vitest +3 (6/6).
- Živá partie: `otazka_str` USE_DIVING_CATCH doplní „další N s Diving Catch to stihne — skočí-li dva a víc, ruší se…“ a „hráč X už skáče“; test `test_diving_catch_otazka_s_dalsimi_kandidaty`.
- C++ mutace (`divingCatchAttempt`, vráceno ze zálohy, přestavěno): AI první místo nejlepšího ⇒ spadnou 2; pevné pořadí AWAY→HOME ⇒ spadnou 3; stará mechanika ⇒ spadne 10.
- Python i vitest na STARÉM textu/dialogu: nový Python test spadne (1), vitest 4 z 6 spadne.
- `make check` spuštěn (log /tmp/d_scratch/check.log).

## Rozhodnutí při implementaci (k přečtení ráno)
- **Výkop — kdo je „na tahu“:** výkop není ničí kolo. C++ `landBall` běží s `activeTeam = přijímající`, takže první rozhoduje přijímající; PHP stejně (explicitně `naTahu: $receivingTeam`, protože PHP má při výkopu aktivní tým podle toho, kdo se rozestavoval). Pokud by měl být první kopající („kick-off provádí kopající kouč“), je to jeden argument v každém enginu.
- **Další otázky po dvou pokusech se nekladou:** jakmile se pokoušejí dva, výsledek je daný (nikdo) — zbývající kandidáti kouče-člověka už otázku nedostanou. Doslovně „otázka na každého“ by se ptala i jich, ale odpověď by nic nezměnila.
- **AI druhé strany skočí i proti jedinému pokusu soupeře** (dnešní pravidlo „AI skočí vždy“) — tím ho zruší. Je to zjevně v zájmu AI, ale není to nová logika; AI s víc kandidáty skočí s jedním (nejlepší cíl hodu `calculateCatchTarget`/`getCatchTarget` z vlastního pole; shoda ⇒ první v pořadí hráčů).
- **Otázka nese:** C++ `roll` = počet DALŠÍCH kandidátů (obou stran), `otherId` = hráč, který už skáče (-1); PHP `otherCandidates`, `attemptingId` (v `PendingDivingCatchDTO`, i toArray/fromArray). Ukazuje se jen první skákající — víc jich být nemůže (pak se už neptá).
- Kopající AI na lajně u výkopu dál neskočí (touchback) — beze změny.

## Návrh řádku do knihy (P237) — NEZAPSÁNO, kniha se neupravuje
| P237 | Noc 30.10. D: Diving Catch — víc kandidátů, varianta B (uživatel 30.10.: „změň na B“; ř. 8069-8071 „ATTEMPTING“, ř. 985, ř. 986-990). Napřed strana na tahu (u výkopu přijímající), pak druhá; kouč-člověk odpovídá na každého svého kandidáta (otázka nese počet dalších kandidátů a kdo už skáče), AI skočí s jedním (nejlepší cíl hodu). Právě jeden pokus ⇒ chytá, dva a víc ⇒ nikdo. C++ `divingCatchAttempt` (+ vazba/živá partie text), PHP `BallResolver::divingCatchAttempt` + `PendingDivingCatchDTO` + dialog ve webu. Oba enginy ⇒ převod netřeba. | hotovo (větev dc-varianta-b), čeká recenze |
- 1. make check: 76 chyb PHPUnit = jen PDO (chyběl `.env`, port 5432) — `.env` zkopírován z main (v .gitignore), znovu
- 2. `make check` ⇒ **„== check: vše zelené“** (PHPUnit 1595 OK, vitest 101, C++ 1323 PASSED, kouř 6 her; log mimo git /tmp/d_scratch/check2.log). Python `test_coach_choice.py`: 19 passed (`check-py` celé ani e2e nespuštěno).

## Nedělal jsem
- Celý server živé partie ani web v prohlížeči jsem nespouštěl (žádná předvolba nemá dva hráče s Diving Catch u jednoho dopadu); text otázky ověřen testem `otazka_str`, dialog vitestem.
- commit + push větve dc-varianta-b

HOTOVO
