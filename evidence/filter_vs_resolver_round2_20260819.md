# AUDIT FILTR vs RESOLVER — DRUHÉ KOLO (19.08.2026)

*(Stolní práce č. 1. Vzorec z 14.08.: **rozhodovací vrstva oceňuje jinou akci,
než jakou resolver provede** — tehdy 5 nálezů za den bez hledání. Tohle je
systematický průchod `macro_actions.cpp` · `action_features.cpp` ·
`action_resolver.cpp` · `block_handler.cpp`.)*

Tři nálezy, dva z nich sdílejí kořen. Čísla z `corpus_baseline_20260817_data`
(3 000 her), skript `diag_filter_vs_resolver_20260819.py`.

---

## P35 ⭐⭐⭐ BLITZ SE OCEŇUJE Z VÝCHOZÍHO POLE, ale hází se z cílového

**Kde.** `getBlockDiceCount` (`macro_actions.cpp:182`) počítá obranné asistence
jako `countAssists(state, att.position, def.teamSide, …)` — tedy kolem pole,
kde blitzující **stojí teď**. Volá se to z `estimateBlitzFailChance`, což je
funkce, kterou `expandBlitz` **vybírá, KDO blitz provede**.

**Co udělá resolver.** `action_resolver.cpp:86–118`: blitzující se ve smyčce
posouvá, dokud není soused cíle, a **teprve pak** se hází blok. Asistence se
počítají až tam (`block_handler.cpp:491`).

⇒ **Kdo blitzuje do hloučku, má na startu nula obranných asistencí a u cíle
jich má několik.** Filtr ho ocení jako čistý blok, resolver ho hodí do přesily.

⭐ **A kód tu závislost ZNÁ.** Komentář v `action_resolver.cpp:89–91` říká
doslova: *„fewer enemies next to the blitzer = fewer defender assists on the
block, see getBlockDiceCount"* — proto je `pickApproachStep` vědomě TZ-aware.
**Trasa tu závislost respektuje, výběr blitzujícího ne.** Není to opomenutí
mechaniky, je to rozpojení dvou půlek téhož rozhodnutí.

### Strop — 27 928 našich rekonstruovaných blitzů, 3 000 her

| | | |
|---|---|---|
| filtr byl optimistický (u cíle víc obranných asistencí) | 4 039 | **14,5 %** |
| **změní se počet kostek** | 4 521 | **16,2 %** |
| ⛔ **překlopí se z „vybíráme my" na „vybírá soupeř"** | **2 712** | **9,7 %** |

Nejčastější přechod je **+1 → −2** (2 424×): filtr vidí vyrovnaný blok
o jedné kostce, hází se dvoukostkový blok, ze kterého **vybírá soupeř**.
Druhý je **+2 → +1** (1 168×). Podhodnoceno **0,18 asistence na blitz**.

⇒ **≈ 0,9 blitzu na zápas se hází s obráceným znaménkem kostek.**

⚠️ **Je to DOLNÍ odhad, ne horní**, ze tří důvodů: (1) bere se pro nás
**nejpříznivější** volné pole u cíle, kdežto `pickApproachStep` vybírá podle
trasy; (2) korpus neveze **Guard**, který by asistence jen přidal;
(3) rekonstrukce blitzu *(blok, jehož útočník s cílem na začátku kola
nesousedil)* zahrne i blok po řetězovém odsunu, což číslo naopak nadhodnocuje —
ale ta část je malá proti 2 424 překlopením.

## P36 ⭐⭐ DAUNTLESS CHYBÍ V ŽEBŘÍČKU BLITZUJÍCÍCH — P13 spravila NABÍDKU, ne VÝBĚR

`estimateBlitzFailChance` volá `getBlockDiceCount(state, blitzer, target, true,
**false**)` — poslední argument je `dauntlessInOffer`. Komentář to hájí větou
*„keep the raw strengths: they describe the block as thrown"*.

⛔ **Jenže blok „jak se hází" s Dauntlessem se proti ST4 vyrovná v 83 %
případů** (d6 + 3 > 4 je 2+) — to je přesně číslo, kterým se P13 zdůvodnila.
Popsat ho jako trvale do kopce **není** popis hodu, je to jeho opak.

⇒ Nabídka Dauntless zná (P13, v produkci od 17.08.), ale **žebříček, který
z nabídnutých vybere jednoho, ho pořád nezná** ⇒ Slayer s Dauntlessem se
řadí jako podřadný kandidát právě proti ST4, kde je nejcennější.

⚠️ Strop se odsud spočítat nedá — potřebuje pozici jako P13 (Q1 test), ne
korpus. **Ale je to levné a je to táž oprava jako P13, o patro níž.**

## P37 ⚠️ `carrierIsBlitzable` NEZNÁ GFI — dosah soupeře je MA+2, ne MA

`macro_actions.cpp:1162`: `opp.position.distanceTo(carrier.position) <=
opp.stats.movement`. CRP dovolí **2 pole navíc přes GFI** (každé 1/6 na pád).
Test rozhoduje, jestli si nosič **nechá pohyb v záloze**
(`carrierStallAwareSteps`) — tedy jestli se považuje za bezpečného.

| 19 964 našich kol s nosičem | | |
|---|---|---|
| kód ví, že je nosič v dosahu (plný sprint) | 15 574 | 78,0 % |
| ⛔ **kód říká BEZPEČNO, soupeř dosáhne přes GFI** | **1 266** | **6,3 %** |
| nosič mimo dosah i s GFI | 3 124 | 15,6 % |

⇒ **0,42 kola na zápas** se nosič považuje za bezpečného neprávem.

⚠️ **Menší nález a záměrně se tak i zapisuje:** zdržet pohyb je často správně
i tak (klec se staví z rezervy), takže z těch 6,3 % **není jasné, kolik by
sprint zlepšil**. Vada je v tom, že se test tváří jako fakt o dosahu, a není.
Ležící soupeř je vyloučen úplně, ačkoli postavení stojí 3 MP ⇒ i on dosáhne
na MA−3.

---

## Co tenhle audit NEPROŠEL

* `macro_mcts.cpp` — vůbec; průchod byl po ose *nabídka → výběr → resolver*
  pro blok/blitz. Skóre a rollouty jsou samostatné kolo.
* `expandPass` / `expandHandOffScore` — **P5 je zastavená** do nového korpusu,
  tak se ta větev vědomě nechala.
* Faul — `foul_handler.cpp:19–21` počítá asistence **v resolveru**, a faul
  se nabízí jen na sousední ležící cíl ⇒ táž vada tam **není**. Ověřeno,
  ne předpokládáno.
* `scoreMoveAction`, `expandCage`, `expandReposition` — nekontrolovány.
