# Item13 KROK 2 — „sebrat + poposunout + postavit klec okolo" v jednom tahu
Design 06.08. (příprava bez buildu — iterace #2 běžela); implementace 07.08.
Navazuje: item13 krok 1 (turn_planner.cpp, wiring 3d6e456), cage-corner
staging design 29.07., závazné vstupy uživatele 05.08.

## Vyřešené otázky z 05.08. (ověřeno čtením kódu 06.08.)
1. **Sure Hands při pickupu engine MODELUJE** — resolvePickup předává
   SkillName::SureHands do attemptRoll (ball_handler.cpp:17-19), tj. reroll
   je v simulaci a propisuje se do pSuccess plánovače.
2. **Výběr sběrače JE skill-aware, nic nedoplňovat:** generace kandidátů
   (macro_actions.cpp:532-561) skóruje `AG*10 − dist*3 + SureHands +15
   + BigHand +5` (Sure Hands ≈ 5 polí vzdálenosti), emituje top-2 best-first;
   plánovač (turn_planner.cpp:151-247) pak mezi nimi rozhoduje Monte Carlem
   (sampleBranch: pSuccess × valueSuccess + fail větev) — preference házeče
   se vynoří ze simulace sama. Požadavek „genericky přes skilly, ne názvy
   pozic" SPLNĚN už v krok 1 kódu.

## Cíl kroku 2
Po úspěšné pickup větvi (pickup + stall-aware ADVANCE poposun uvnitř
expandPickup) doplnit třetí fázi plánu: **cage-fill** — dice-free REPOSITION
dosud neaktivovaných hráčů na rohové sloty kolem NOVÉ pozice nositele.
Hierarchie z 05.08.: posun → doplnit → nikdy solo útěk.

## Choreografie (vzor PASIVNÍ HLÍDAČ — závazné, uživatel 05.08.)
1. **Hlídač** (hráč stojící vedle míče už PŘED tahem) se NEHÝBE — hlídá
   zdarma, aktivaci si šetří. → Vyřadit ho z kandidátů safe-backup fáze
   (dnes by ho plánovač mohl utratit jako zálohu, kterou už fyzicky je).
2. **Sběrač** se aktivuje: PICKUP → poposun (stávající větev).
3. **Cage-fill fáze (NOVÁ):** hlídač + ostatní nehnutí hráči obsadí rohy
   u nové pozice nositele — reuse tryAssign(state, carrier, …,
   reservedPlayerIds) z cage_advance; dice-free only (REPOSITION bez kostek,
   stejná hygiena prior-floorů jako krok 1 — žádná nová macro rodina).

## Konstrukční pravidla (vstupy uživatele 05.08.)
- **Neduplikovat zdroj na obsazený roh** — tryAssign už umí stayPut +
  žádné dvojí obsazení slotu; roh obsazený stojícím spoluhráčem = hotový
  roh, žádné přiřazení navíc.
- **Zálohy dopředu cílí budoucí rohové sloty (chytrá verze):** v safe fázi
  preferovat mezi kandidátními poli vedle míče ta, která jsou zároveň
  rohovým slotem PROJEKTOVANÉ pozice nositele po poposunu (známe ji ze
  sampleBranch projekce) → jedna figura = pojistka odrazu I budoucí roh.
  Implementačně: tiebreak v řazení kandidátů (turn_planner.cpp:185-198),
  NE tvrdá podmínka (pojistka odrazu má přednost).
- **reservedPlayerIds — první reálné využití:** sběrač + utracené zálohy
  nesmí být zároveň rohy; hlídač naopak rezervován PRO cage-fill.
- **Validita při odchylce:** cage-fill makra platí jen pokud míč drží náš
  nositel (rozšířit stagedMacroStillValid o tento případ — fail pickup =
  celá cage-fill fáze neplatná, spadne do search() fallbacku jako dnes).

## Validace (čtvrtek, po implementaci)
- 493 C++ testů + nové testy: hlídač se nehýbe před pickupem; cage-fill
  neobsazuje obsazené rohy; fail-pickup zneplatní fázi 3; reservedPlayerIds
  brání dvojroli.
- Harness diag_item13_staged_planner_harness.cpp na těžených stavech
  (diag_item13_states.json): metrika = obsazenost rohů po tahu na stavech
  s úspěšným pickupem (před/po), pTO nezhoršené.
- Rebuild/testy AŽ když neběží iterace (feedback-verify-no-interference).
- Do tréninku až po validaci, jako samostatná změna (one-change-at-a-time);
  pojmenovaný cíl uživatele: „sebrat míč + sestavit kolem něj klec V JEDNOM
  tahu" — dnes neumí nikdo.
