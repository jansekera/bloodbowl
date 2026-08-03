# Item 7 — podklad pro rekapitulaci (03.08.2026, 16:00 CEST)

Rekapitulace shipped fixu `3953393` (BLITZ approach: TZ tiebreak) nad konkrétními
situacemi z post-fix korpusu `diag_replay_mine_20260730_data/` (24 her, mined po fixu).
Analýza: rekonstrukce blitz approachů z replay eventů (řetěz MOVE/DODGE/GFI hráče
zakončený jeho BLOCK eventem) — skript `diag_item7_blitz_dodge_attribution_20260803.py`.

## 1) Co fix dělá (rekapitulace jednou větou)

BLITZ byl jediný pohyb v enginu vybírající kroky čistě podle vzdálenosti (action_resolver.cpp
greedy smyčka) — fix přidal tackle zóny jako **tiebreak mezi stejně blízkými poli**
(vzdálenost zůstává primární, žádné detoury). Regresní test přesného tvaru situace
z 21.07.: `ActionResolver.BlitzApproachAvoidsTacklezonesOnEquallyCloseSteps`
(test_action_resolver.cpp:277) — zelený, dnes součást 453/453.

## 2) Čísla z post-fix korpusu (24 her)

| metrika | hodnota |
|---|---|
| blitz approachů (pohyb→block) | 447 |
| z toho s ≥1 dodge během approache | 46 dodge eventů (59 % všech 78 DODGE v korpusu — blitz zůstává dominantní zdroj dodgů) |
| dodge do VÍCE TZ během approache | 11 |
| z nich s dostupným stejně-blízkým bezpečnějším polem (= co má tiebreak chytat) | **0** ✅ |

**Závěr: fix dělá 100 % své navržené práce.** Všech 11 zbývajících „do horšího" dodgů
nemělo žádnou stejně blízkou alternativu — jsou to přesně **detour případy** (vyhnuli
bychom se jen delší trasou), tedy vědomě odložený scope (backlog rozhodnutí 30.07.).

## 3) Pozitivní situace — tiebreak v akci (kandidáti na projití)

1. **g0009, tah 1, hráč 20**: dodge (13,9)→(12,10) do 1 TZ; stejně blízká alternativa
   měla **3 TZ** — tiebreak vybral výrazně bezpečnější pole.
2. **g0006, tah 1, hráč 19** (dlouhý approach d=10→7): poslední krok (11,3)→(10,2) do
   **0 TZ**, k dispozici 2 stejně blízké alternativy s ≥1 TZ. Tentýž approach ale
   obsahuje i vynucené kroky do 2 TZ (viz §4/3) — hezká ukázka obojího v jednom běhu.
3. **g0014, tah 30, hráč 20**: (13,9)→(12,10), vzal 1 TZ, stejně blízká alternativa 3 TZ.

## 4) Zbývajících 11 — všechny detour-třída (kandidáti na projití)

1. **g0017, tah 1, hráč 12**: (14,5)→(13,5), **1→4 TZ**, žádná stejně blízká alternativa —
   nejkřiklavější případ; vyhnutí = jen delší cestou (nebo jiný cíl blitzu).
2. **g0000, tah 8, hráč 6**: (20,8)→(21,8), 1→3 TZ, žádná alternativa.
3. **g0000, tah 19, hráč 5** (řetěz): (11,7)→(12,6) 1→1, pak (12,6)→(13,5) **1→4 TZ** —
   blitzer se krok za krokem zavrtává do hloučku; detour ani jiný cíl nezvažován.

## 5) Co fix vědomě NEřeší (backlog z 30.07., k potvrzení priorit)

- **Detoury** (dodge proti směru blitzu): rozhodnutí mělo padnout „podle dat z nového
  korpusu" — data teď MÁME: 11/46 dodge eventů (24 % dodgů v approachích) je
  detour-třída, včetně případů 1→4 TZ. K diskuzi: stojí to za implementaci s
  celo-trasovým srovnáním (dodge řetěz + GFI rizika, vzor estimateApproachFailChance)?
- **Skórování finálního pole počtem kostek bloku** (méně protiassistů), oddělené od
  mezikroků (dodge řetěz) — korekce zdůvodnění z 30.07.; zapadá do BLITZ review
  taxonomie (účel PRŮLOM vs ZRANĚNÍ chce jiné finální pole).
- Vazba na taxonomii účelů (03.08.): detour dává smysl hlavně u účelu ZRANĚNÍ/MÍČ
  (cíl stojí za delší cestu); u PRŮLOMU často existuje alternativní cíl.

## 6) Otázky k rozhodnutí na rekapitulaci

1. Akceptujeme 11/46 detour-třídy jako známý strop konzervativního fixu, nebo zadat
   detour implementaci (celo-trasové srovnání) jako další malou MR v BLITZ sérii?
2. Má se detour řešit před, nebo až spolu s block-dice skórováním finálního pole
   (oba body se potkávají v „kam přesně doskočit a kudy")?
3. Souhlas s uzavřením item7 TZ tiebreak rekapitulace (fix funguje na 100 % svého
   scope, regresní test drží)?


## DODATEK z rekapitulace (03.08. ~13:15, proběhla dřív) — KOREKCE a závěry

1. **Korekce „odměn":** v 5 z 10 hazardních approachů byl sražen ÚTOČNÍK (KNOCKED_DOWN
   player_id == blocker), ne obránce: bilance 10 hazardů = 5× push, 5× útočník down +
   TURNOVER (1× vlastní INJURY, g0022), **0× sražený obránce**. Logika: dodge do 3-4 TZ
   = cíl má plné protiassisty = blok 1d/proti → vysoká šance attacker down. Chyby se násobí.
2. **P(celá cesta) úspěchu 2,6–33 %** (výpočet 7−AG+destTZ; survivor bias — korpus-wide
   344 dodge, 28 % fail, 90× okamžitý turnover).
3. **Kontext: NOUZE to nebyla** — 3 hazardy při vedení, 2 v 1. tahu hry; jediný kandidát
   na nouzi g0009 (0:1, 2. poločas), odměnou ale jen push.
4. **ZÁVĚR REKAPITULACE (dohodnuto s uživatelem):** tiebreak fix uzavřen (100 % svého
   scope). Detoury samostatně NE — nová malá MR v BLITZ sérii: **celo-hrové acceptance
   kritérium** = P(cesta) × P(blok nespadne — kostky/protiassisty na finálním poli) ×
   hodnota odměny dle účelu − odveta na tělo blitzera; **práh ≥ ~50 % na SOUČIN, podkročení
   jen v nouzi (skóre deficit × turnsLeft) a jen pro účel MÍČ/SCORE, nikdy push**.
   Block-dice skórování finálního pole tím přestává být samostatná vrstva — je povinnou
   součástí acceptance.
