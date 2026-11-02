# P246 — jedna „šance chycení s přehozem“ pro celou AI (PLÁN, neprovedeno)

Uživatel 3.11.2026: *„sjednotit - zatím jen naplánuj - proveď později“*. Navazuje na P239 (Diving Catch, `46a98f85`)
a nález MEDIUM-1 recenze P239.

## Pravidlo (jedno, z P239)
Šance, že hráč chytí (cíl `calculateCatchTarget` / `getCatchTarget` s modifikátorem skutečného hodu, ořez 2–6):
- jednou: p = (7 − cíl) / 6;
- **Catch** (ř. 7992-7995): p + (1−p)·p — vždy, bez ohledu na tah a Loner;
- jinak **týmový přehoz**, jen když ho hráč smí (vlastní tah, ne výkop: C++ `teamRerollAllowed`, PHP `$rerollSide`)
  a tým ho má po ruce (`canUseReroll`): p + (1−p)·g·p, **g = 1/2 pro Loner** (ř. 8287-8290), jinak 1;
- výsledek nese i příznak „stojí na týmovém přehozu“ (při shodě dává přednost hráči s Catch — přehoz se šetří).
- Pro (ř. 8385-8389) se nepočítá — vědomé zjednodušení (P239).

## Dnešní stav (3.11.2026)
| místo | co počítá | rozdíl proti pravidlu |
|---|---|---|
| C++ `ball_handler.cpp` `divingCatchChance` (anonymní) | pravidlo výš | — vzor |
| C++ `one_cage.cpp:108` `readyMateChance` | jen Catch | chybí týmový přehoz |
| C++ `one_cage.cpp` ~262 (TD předávkou, `p2`) | Catch NEBO týmový přehoz | chybí Loner 1/2, chybí `teamRerollAllowed` |
| C++ `one_cage.cpp` ~668 (chycení u předávky v plánu) | Catch NEBO týmový přehoz | totéž |
| C++ `one_cage.cpp` ~2606 | jen Catch | chybí týmový přehoz |
| PHP `LearningAICoach.php:548` `catchChance` (volání 1548, 1638) | jen Catch | chybí týmový přehoz |
| PHP `BallResolver.php` `divingCatchChance` (privátní) | pravidlo výš | — vzor |

⛔ **Výjimka, neměnit bez uživatele:** `one_cage.cpp:32` `teamRerollFor` (GFI blitzů, P236/P238) počítá Loner jako
„bez přehozu“ — komentář „vědomě přísnější než pravidla“. Netýká se chytání; při provedení se zeptat, zda sjednotit.

## Kroky (až uživatel řekne „proveď“)
1. **C++:** `divingCatchChance` přesunout do `helpers` jako veřejnou `catchChanceWithReroll(state, player, modifier,
   bool* teamReroll = nullptr)`; volat z `divingCatchAttempt` a ze čtyř míst v `one_cage.cpp` (nahradit vlastní výpočty).
2. **PHP:** `BallResolver::divingCatchChance` zveřejnit jako `catchChanceWithReroll($state, $hrac, $rerollSide, $modifikator)`;
   `LearningAICoach::catchChance` ji volá s `$rerollSide` = vlastní strana, je-li na tahu (jinak null).
3. **Testy napřed (musí spadnout):** jeden test helperu na chokepointu (Catch, přehoz, Loner, cizí tah, výkop, přehoz už
   použitý) v obou enginech + po jednom testu chování na místo, kde se volba změní: předávka hráči s Loner vs. s Catch
   (one_cage ~262/~668), připravený spoluhráč bez Catch s přehozem (readyMateChance), PHP volba příjemce (1548/1638).
4. **Kontrola „nerozbilo se“:** `make check` + párové A/B stejnými semínky (`mcts_cli --seed`) jen na otázku „mění se volba
   příjemce/předávky a nepadá nic“ — ne jako měření síly AI.
5. Recenze odděleným průchodem, oprava nálezů, kniha P246, commit + push.

## Dopad (odhad, neměřeno)
Loner příjemci budou méně ceněni (předávka Ogrovi/Treemanovi), hráči bez Catch ve vlastním tahu s přehozem víc
(readyMateChance, ~2606, PHP AI). Diving Catch se nemění.
