# Item 14 — podklad pro rekapitulaci (03.08.2026)

## CO PŘESNĚ SE ŘEŠÍ (a nic jiného)
Item14 = **výběr dvojice blitzer+cíl** v `expandBlitz` (macro_actions.cpp).
Před fixem: výběr ignoroval riziko PŘÍSTUPOVÉ CESTY blitzera (bral nejbližšího/
nejsilnějšího). Fix `2ff47ca` (29.07.): kandidáti se hodnotí přes
`estimateBlitzFailChance` — odhad rizika celé cesty k cíli.
MIMO scope dnešní rekapitulace: acceptance blitzu (MR-A série), exekuce kroků
(item7, uzavřeno), asistenti (item13 krok 2), účely blitzu (taxonomie).

## Co fix dělá a jak je ukotvený
- `estimateBlitzFailChance` modeluje approach STEJNOU chůzí, jakou pak provede
  exekutor — po `23d652a` (30.07.) sdílejí `pickApproachStep` (helpers.cpp),
  odhad a exekuce se nemohou rozjet; estimátor zná obsazenost, vrací 1.0 kde
  by exekutor selhal.
- Regresní test: `MacroExpansion.BlitzSelectsBestBlitzer`
  (test_macro_actions.cpp:1103) — fixture „dvojitá zeď" (zesílena 30.07.):
  blitzer za jednořadou zdí ji korektně obchází, výběr preferuje kandidáta,
  jehož cesta nevede skrz TZ. Zelený v dnešních 453/453.
- Původní motivace: stav S2 z item10 analýzy (82 % pTO risky-aktivace) —
  root cause byl právě path-blind výběr blitzera.

## Korpusová čísla PRE (21.07., před item14+item7+1MP) vs POST (30.07., po nich)

| metrika | PRE | POST |
|---|---|---|
| dokončených blitz approachů | 354 | 447 |
| podíl approachů s ≥1 dodge | 9 % | 8 % |
| dodge/approach | 0,12 | 0,10 |
| dodge do více TZ (podíl approachů) | 4,0 % | 2,5 % |
| chain_p < 50 % | n/a (korpus bez statů) | 19/447 (4 %) |

**Poctivá interpretace:** agregátní posun je MALÝ a nelze ho čistě připsat
item14 (mezi korpusy jsou i item7 tiebreak a 1MP pravidla; jiné hry, jiné
situace). Očekávatelné: item14 zlepšuje VOLBU mezi kandidáty — neprojeví se
tam, kde lepší kandidát neexistuje, a nesnižuje počet blitzů. Tvrdá evidence
item14 zůstává jednotková (fixture) + případ S2. Hlubší korpusový důkaz by
vyžadoval kontrafaktuál (přehodnotit kandidáty enginem per situace) — vědomě
NEděláno (malé kroky); MR-A1/A2 logging dá tato data zadarmo do budoucna.

## Co item14 vědomě NEřeší (hranice — dnes potvrzené jinde)
- Jestli se blitz VŮBEC má hrát (19/447 dokončených approachů má i po item14
  P(cesta)<50 %) → MR-A série (acceptance, práh 50 % na součin, nouze).
- Hodnotu/křehkost těla blitzera (Catcher case) → MR-A5 odveta.
- Výběr podle ÚČELU blitzu (průlom/zranění/míč/poziční) → po MR-A sérii.

## Otázky k rozhodnutí
1. Stačí jednotková evidence + S2 pro uzavření item14 rekapitulace, nebo chceš
   kontrafaktuální korpusový audit (engine přehodnotí kandidáty v N situacích —
   samostatná malá analýza, ~hodiny CPU po iteraci)?
2. Souhlas, že item14 zůstává „hotovo, hranice zdokumentované" a další zlepšení
   výběru blitzera jdou výhradně přes MR-A sérii (ne úpravou item14)?
