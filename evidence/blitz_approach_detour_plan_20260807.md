# Plán: detoury a přesnější cena přiblížení u BLITZE (item7 backlog)

Sepsáno 07.08. Vychází ze zadání uživatele z 30.07. (post-implementační
revize item7, uzavřena) — tři podmínky k VYHODNOCENÍ, ne k předpokladu:
1. Je detour skutečně bezpečnější?
2. Dojde blitzer vůbec k cíli (delší trasa spotřebuje pohyb)?
3. Srovnat dodge dopředu vs dodge dozadu + nutné GFI — **celá cesta**,
   ne jeden krok.

## Dnešní stav (ověřeno v kódu 07.08.)
`pickApproachStep` (helpers.cpp:33-51), sdílená exekutorem
(`action_resolver.cpp:98`) i odhadem (`macro_actions.cpp:186`,
`estimateApproachFailChance`) — sjednoceno commitem 23d652a, takže odhad
a exekuce nemůžou tvrdit každý něco jiného. **Tuhle vlastnost musí každá
změna zachovat.**

```
score = vzdálenost_k_cíli * 100 + (stojím_už_v_TZ ? 12 : 20) * TZ_cíle
```
⇒ vzdálenost je absolutně nadřazená: krok stranou ani dozadu nikdy
nevznikne (100 > jakýkoli počet TZ × 20). TZ tedy rozhodují jen mezi
stejně blízkými poli. Naměřeno: dodgy do většího nebezpečí 16,7 % → 11,6 %
(z≈1,9); zbytek můžou být právě zakázané detoury — dnešní data to
nerozlišují.

---

## FÁZE 0 — MĚŘENÍ PŘED IMPLEMENTACÍ (odpovídá na podmínky 1 a 3)
Bez toho se implementovat nemá. Diagnostika, nulový zásah do produkce.

Pro každé blitz přiblížení v korpusu spočítat a porovnat:
* **trasa A = dnešní** (pickApproachStep) — celková šance selhání cesty
  = 1 − Π(1 − p_fail) přes dodge hody i GFI hody;
* **trasa B = nejlepší „bezpečná" alternativa** připouštějící až N kroků
  bez postupu (N = 1, 2);
* **dorazí?** obě trasy proti rozpočtu pohybu (a kolik GFI si vynutí).

Výstup: v kolika % situací je B striktně bezpečnější, o kolik, a jak často
za cenu nedoražení. **Pokud B vyhrává vzácně nebo málo → fázi 2 nedělat.**

Znovupoužití: `estimateApproachFailChance` už celou trasu oceňuje —
rozšířit o generování alternativních tras (parametr „povolený počet
kroků bez postupu"). Harness po vzoru `diag_item13_staged_planner`.

---

## FÁZE 1 — ROZDĚLIT SKÓROVÁNÍ (nezávislé na detourech, levnější, dělat první)
Dnešní jednotná penalizace nerozlišuje PROČ je pole nebezpečné. Z revize
30.07. (pravidlově ověřeno):

* **Mezikroky:** TZ nesou reálné riziko — vstup do zóny vynutí dodge při
  dalším kroku; a při už probíhajícím dodgi zvyšuje TZ cílového pole
  přímo házený hod. ⇒ cenit **očekávaným řetězem dodge hodů**, ne
  konstantou 20/12.
* **Finální pole (odkud se bloká):** TZ tam dodge riziko NEZVYŠUJÍ vůbec.
  Skutečná cena je **méně kostek bloku** (soupeřovy protiasisty se počítají
  z okolí útočníka). ⇒ cenit přímo `getBlockDiceCount` pro každé kandidátní
  finální pole, ne počtem TZ.

Tohle může být celá výhra i bez detourů — proto první.

Testy: (a) mezi dvěma stejně blízkými finálními poli se vybere to
s lepšími kostkami bloku, i když má víc TZ; (b) mezikrok s vyšší
pravděpodobností řetězu dodgů prohraje s bezpečnějším stejně blízkým;
(c) regrese: blitz skrz neprůchodnou TZ zeď se pořád provede.

---

## FÁZE 2 — POVOLIT OMEZENÝ DETOUR (jen když fáze 0 ukáže, že se vyplatí)
* Povolit kroky bez postupu, **tvrdý strop** (nejvýš 1, případně 2 za celé
  přiblížení) — brání „obcházení půlky hřiště".
* **Rozpočtová podmínka:** detour se nesmí vzít, pokud po něm blitzer
  nedorazí; GFI hody se do ceny trasy počítají (1/6 selhání každý).
* Ochrany proti oscilaci: `lastPos` + strop kroků (vzor `movePlayerToward`).
* Rozhodovat celou trasou (fáze 0 výpočet), ne lokálním skóre kroku.

---

## Rizika a pořadí
* `pickApproachStep` je sdílená **i s výběrem blitzera** (item14) — každá
  změna mění i to, KDO a KOHO blitzuje. Měřit párově, ne jen na sondě.
* Pořadí: **fáze 0 (měření) → fáze 1 (rozdělení skórování) → znovu změřit
  → fáze 2 (detoury) jen při doložené výhře.**

## ⚑ ZAŘAZENÍ (rozhodnutí uživatele 07.08.): FÁZE 0 PROBĚHNE **PŘED DOKONČENÍM BLITZ AKCE**
Fáze 0 není dodatek na konci — je to **vstupní podmínka BLITZ série**: bez
jejích čísel se o pohybu blitzera rozhoduje odhadem a D1 review nemá čím
podložit „detoury ano/ne".

* **Fáze 0 (měření) musí doběhnout PŘED D1 review / před finálním
  dokončením BLITZ vrstvy** (~17.08.). Její výstup je podklad pro D1.
* Fáze 1 se rozhoduje na jejích datech; fáze 2 jen při doložené výhře.
* Uvnitř série je to její přirozený první technický kus.
* Přednost před ní má jen trpasličí fronta ze 07.08. (denial člen →
  dvojrozměrné rozhodnutí → otevírací blitz). Pozor na vazbu: „otevírací
  blitz" se přiblížení blitzera přímo dotýká → pustit měření hned, jak
  bude volné CPU/noční okno, ideálně PŘED ním.
* Náklad: diagnostika, ~hodina práce + minuty běhu; nulový zásah do
  produkce, takže nekoliduje se zmrazením stromu ani s měřicími řetězy
  (jen o CPU).
