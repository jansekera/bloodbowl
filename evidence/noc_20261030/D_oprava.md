# D — opravný průchod (Diving Catch varianta B)

Autor, opravný průchod podle `D_recenze.md`. Průběžný zápis.

## Průběh
- založeno; čtu nálezy a kód.
- Nález 1 C++: nový test `KickoffLanding.ReceivingCoachIsAskedAboutDivingCatchBeforeTheKickingCoach` (výkop, dopad (12,7), kopající DC na (13,7), přijímající DC na (11,8), oba lidé). Na HEAD zelený (kód je správně — strážní test). Mutace `state.activeTeam = state.kickingTeam` v `finishKickoff` ⇒ **spadl** (první otázka na hráče 14 místo 8). Zdroják vrácen.
- Nález 1 PHP: nový test `DivingCatchKandidatiTest::testVykopNaprvSePtaPrijimajiciPakKopajici` (výkop, dopad (12,7), kopající AWAY DC na lajně, přijímající HOME DC na (11,8), oba lidé; aktivní tým schválně KOPAJÍCÍ). Na HEAD zelený (strážní). Mutace `naTahu: $receivingTeam->opponent()` ⇒ **spadl**; mutace „argument `naTahu` vypadne“ (vezme se aktivní tým) ⇒ **spadl**. Zdroják vrácen.
- Nález 2 (C++ i PHP): testy `DivingCatchCandidates.OtherCandidatesLeaveOutThoseWhoDeclined` a `DivingCatchKandidatiTest::testDalsiKandidatiNepocitajiTyKdoOdmitli` — na starém kódu **spadly** (C++: po „ne“ roll 1 místo 0; AI se dvěma kandidáty ⇒ kouč soupeře roll 2 místo 1; PHP: 1 místo 0). Oprava: počet dalších = kandidáti − 1 − ti, kdo už rozhodli „ne“ (člověk odpověděl ne / AI kandidát své strany, se kterým AI neskočila). Ten, kdo skáče, se počítá dál (riziko zrušení trvá). Text otázky (Python, web) beze změny — nese dál „počet dalších“. Po opravě zelené; mutace = vrácení na `- 1` je přesně starý kód, na kterém testy padly.
- Nález 4: `D_autor.md` — řádek „commit + push“ přesunut z „Nedělal jsem“ do průběhu jako „3. commit b679118c + push“ s poznámkou (záloha původního mimo git `/tmp/d_opr/D_autor.md_zaloha`). Test netřeba (dokument).
- Nález 3: **neopraveno — nová logika AI, návrh pro uživatele** (viz níže).
- `make check` běží na pozadí, log mimo git `/tmp/d_opr/check.log`.

## Nález 3 — návrh pro uživatele (NEPROGRAMOVÁNO, nová logika AI)
- **Kopající AI u výkopu na lajně skočí, právě když už skáče přijímající.** Dnes `aiJumps` (C++ `kickoff_handler.cpp` landBall, PHP `KickoffResolver` predikát) říká: kopající na lajně ⇒ nikdy (skok = touchback). Ve variantě B ale kopající rozhoduje druhý a ví, že přijímající skáče; jeho „pokus“ by nevedl k touchbacku (dva pokusy ⇒ nikdo, ř. 8069-8071), jen by přijímajícímu vzal chytání. Změna = nová volba AI (predikát by musel dostat seznam už skákajících), proto jen návrh.
- **Volba „nejlepšího“ AI kandidáta podle cíle hodu ignoruje Catch (přehoz)** — hráč s Catch a horší AG může být lepší volba. Také nová logika, jen návrh.

## Kontrola
- `make check` ⇒ **„== check: vše zelené“** (PHPUnit 1597 OK, vitest 101, C++ 1325 PASSED, kouř 6 her; log mimo git `/tmp/d_opr/check.log`).

## Shrnutí nálezů
| # | výsledek |
|---|---|
| 1 MEDIUM | opraveno — strážní testy pořadí stran u výkopu v C++ i PHP; mutace (kopající první / PHP bez `naTahu`) padají |
| 2 LOW | opraveno v C++ i PHP — „další kandidáti“ bez těch, kdo už odmítli; testy na starém kódu padaly |
| 3 LOW | neopraveno — nová logika AI, návrh pro uživatele výše |
| 4 LOW | opraveno — řádek v `D_autor.md` |

Obě opravy jsou v obou enginech ⇒ převod do druhého enginu netřeba.

HOTOVO
