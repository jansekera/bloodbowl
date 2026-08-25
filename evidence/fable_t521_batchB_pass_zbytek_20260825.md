# T5.21 — dávka B: PŘIHRÁVKY + ZBYTEK ZDROJŮ (25 citací)

Zdroj pravidel: `rules_bb2016.txt` (9 518 ř., hlavička „Blood Bowl 2016, October 2017").
Verdikt: **A** = špatný zdroj, týž obsah · **B** = špatný zdroj, JINÝ obsah (NÁLEZ) · **C** = nejde rozhodnout.
⛔ Nic se neopravuje, citace se nepřepisují.

| soubor:řádek | téma | verdikt | BB2016 ř. | doslovná citace | sedí kód? |
|---|---|---|---|---|---|
| pass_handler.cpp:38 | způsobilost interceptora (TZ, „blíž než konce k sobě", jen JEDEN) | A | 1763-1775 | „To be able to make an interception, the player must: • have the plastic ruler pass over at least part of the square the intercepting player is standing in, and … • have a tackle zone, and … • be closer to the thrower than the thrower is to the target player/square of the pass, and … • be closer to the target player/square of the pass than the thrower is to the target… Note that only one player can attempt an interception, no matter how many are eligible." | ano |
| pass_handler.cpp:55 | „closer to each end than the ends are to each other" | A | 1768-1772 | (týž odstavec, dvě odrážky „be closer to…") | ano |
| pass_handler.cpp:88 | Pouring Rain −1 i na intercept | A | 1486-1488 | „11 Pouring Rain: It's raining, making the ball slippery and difficult to hold. A ‑1 modifier applies to all catch, intercept, or pick-up rolls." | ano |
| pass_handler.cpp:99 | Catch přehazuje neúspěšný intercept | A | 7992-7995 | „Catch (Agility) A player who has the Catch skill is allowed to re-roll the D6 if he fails a catch roll. It also allows the player to re-roll the D6 if he drops a hand-off or fails to make an interception." | ano |
| pass_handler.cpp:113 | Very Long Legs blokuje Safe Throw | A | 8655-8659 | „Very Long Legs (Mutation) The player is allowed to add 1 to the D6 roll whenever he attempts to intercept or uses the Leap skill. In addition, the Safe Throw skill may not be used to affect any Interception rolls made by this player." | ano |
| pass_handler.cpp:240 | Blizzard omezuje RANGE, nedaní hod | A | 1489-1494 | „12 Blizzard: … while the snow means that only quick or short passes can be attempted." | ano |
| pass_handler.cpp:266 | jen Very Sunny daní hod přihrávky | A | 1482-1484 | „3 Very Sunny: A glorious day, but the blinding sunshine causes a ‑1 modifier on all passing rolls." | ano |
| pass_handler.cpp:394 | hand-off má vlastní jednou-za-kolo deklaraci | A | 359-361 | „NOTE: The Extra Rules section adds two additional Actions: Hand-off and Foul. Neither of these Actions may be declared by more than one player per turn." | ano |
| game_simulator.cpp:351 | injury table 8-9 KO / 10-12 Casualty | A | 710-722 | „8-9 KO'd – … At the next kick-off, before you set up any players, roll for each of your players that have been KO'd. On a roll of 1-3 they must remain in the KO'd box … On a roll of 4-6 you must return the player to the Reserves box … 10-12 Casualty – … The player must miss the rest of the match." | ano |
| game_simulator.cpp:360 | Secret Weapon: vyloučen po drivu, kterého se zúčastnil | A | 8452-8455 | „Once a drive ends that this player has played in at any point, the referee orders the player to be sent off … regardless of whether the player is still on the pitch or not." | ano |
| game_simulator.cpp:364 | totéž (duplicitní komentář o 4 řádky níž) | A | 8452-8455 | (tatáž věta) | ano |
| game_simulator.cpp:448 | Kick: kicker nesmí stát na LOS ani ve wide zone | A | 8205-8213 | „Kick (General) … The player may not be set up in either wide zone or on the line of scrimmage. Only if all these conditions are met is the player then allowed to take the kick-off." | ano (výběr nejhlubšího hráče LOS/wide zone splňuje) |
| ball_handler.cpp:123 | throw-in šablona: D6, 3-4 = rovně zpět | A | 1504-1508 (a 1613-1616) | „place the Throw-in template over the ball so that the 3-4 result is pointing in the same direction as the wind, then roll a D6 and move the ball one space in the corresponding direction" | ano (1-2 / 3-4 / 5-6 = tři směry) |
| ball_handler.cpp:125 | rozdíl proti Bounce (scatter) šabloně + **rohové vhazování D3** | **C** | 902-903 (bounce); rohy NENALEZENY | bounce: „To find out where the ball bounces to, roll for scatter one more time." — pro **rohový výjezd** BB2016 text nic neříká | ⚠️ rohová větev (D3, tři směry) v BB2016 **nemá oporu ani protiklad** |
| helpers.cpp:64 | dodge: plošné +1 „za hod na dodge", −1 za TZ na CÍLOVÉM poli | A | 597-600 + 578-585 | tabulka: „DODGING MODIFIERS / Making a Dodge roll +1 / Per opposing tackle zone on the square that the player is dodging to **+1**" ⚠️ znaménko je v této kompilaci **překlep** — prózový příklad l. 578-585 to opravuje: „He gets a +1 to the roll for making a dodge, but has to **subtract 2** because there are two Orc tackle zones on the square he is moving to, for a final modifier of -1." | ano |
| helpers.cpp:185 | asistence: TZ blokovaného hráče se nepočítá | A | 1663-1670 | „In order to make an assist, the player: 1. Must be adjacent to the enemy player involved in the block, and… 2. Must not be in the tackle zone of any **other** player from the opposing team, and … 3. Must be standing, and … 4. Must have his tackle zones." | ano |
| macro_actions.cpp:333 | blok při blitzu stojí 1 MP | A | 346-350 | „Blitz: The player may move a number of squares equal to his MA. He may make one block during the move. The block may be made at any point during the move, and ‘costs' one square of movement." | ano |
| macro_actions.cpp:724 | „casualty table verified exact against CRP" | A | 2406-2422 | „11-38 Badly Hurt / 41…48 Miss next game / 51-52 Niggling Injury / 53-54 -1 MA / 55-56 -1 AV / 57 Broken Neck -1 AG / 58 Smashed Collar Bone -1 ST / 61-68 DEAD" — `injury.cpp:8-23` má přesně tohle | ano |
| rules_engine.cpp:102 | blok při blitzu stojí 1 MP (rezerva v nabídce) | A | 346-350 | „Blitz: … He may make one block during the move. The block may be made at any point during the move, and ‘costs' one square of movement." | ano |
| rules_engine.cpp:189 | Bombardier: nespotřebuje Pass Action, nesmí se hnout ani vstát | A | 7948-7955 | „A coach may choose to have a Bombardier who is not Prone or Stunned throw a bomb instead of taking any other Action with the player. **This does not use the team's Pass Action for the turn.** … except that the player may not move or stand up before throwing it (he needs time to light the fuse!)" | ano |
| move_handler.cpp:272 | Leap: čistý AG hod, jediný modifikátor Very Long Legs | A | 8270-8280 | „Leap (Agility) … move the player to any empty square 1 to 2 squares from their current square and then make an Agility roll for the player. **No modifiers apply to this D6 roll unless he has Very Long Legs.**" | ano |
| ttm_handler.cpp:56 | TTM: strop je Short Pass (LP/LB nelze) | A | 8607-8608 | „…except the player must subtract 1 from the D6 roll when he passes the player, fumbles are not automatically turnovers, and **Long Pass or Long Bomb range passes are not possible**." | ano |
| turn_handler.cpp:9 | Secret Weapon se NEvylučuje na konci kola, ale drivu | A | 8452-8455 | „Once a drive ends that this player has played in at any point, the referee orders the player to be sent off…" | ano |
| kickoff_handler.cpp:60 | Get the Ref: obě mužstva dostanou úplatek | A | 1270-1283 | „2 Get the Ref: … **Each team receives 1 additional Bribe to use during this game.** A Bribe allows you to attempt to ignore one call by the referee…" | ano (kód to vědomě NEimplementuje — „Simplified: no-op") |
| bomb_handler.cpp:14 | bomba nespotřebuje Pass Action týmu | A | 7948-7951 | „A coach may choose to have a Bombardier who is not Prone or Stunned throw a bomb instead of taking any other Action with the player. This does not use the team's Pass Action for the turn." | ano |

## Součet

**A = 24 · B = 0 · C = 1** (z 25)

## B — NÁLEZY

**Žádné.** V téhle dávce se BB2016 a citovaný CRP/LRB6 obsah **nikde nerozešly**.

⭐ Strukturální důvod, který stojí za zapsání: `rules_bb2016.txt` ř. 28-29 sám říká
„These rules are a collection of all new Blood Bowl rules **merged together with
previous Living Rulebook version 6**." Tam, kde BB2016 pravidlo nezměnil,
je v textu **doslova reprodukovaný LRB6**. Proto je u téhle dávky A drtivá většina
a **není to důkaz, že jsme kontrolovali povrchně** — je to důkaz, že hranice mezi
edicemi vede jinudy než přes přihrávky, počasí, zranění a základní dovednosti.
⇒ Očekávání „B se najdou napříč vším" je u tohohle korpusu pravidel mylné;
hledat se mají tam, kde BB2016 něco **přidal** (Death Zone S1/S2, kick-off tabulky,
inducements), ne v jádru hry.

## C — nerozhodnuté

**ball_handler.cpp:125 — rohové vhazování (D3, tři směry).**
- Co dělá kód: při výjezdu míče **rohem** hřiště (`TOP_LEFT`/`TOP_RIGHT`/
  `BOTTOM_LEFT`/`BOTTOM_RIGHT`) hodí D3 a vybere jeden ze tří směrů —
  podél jedné hrany, čistá diagonála do hřiště, podél druhé hrany.
- Co říká BB2016: **nic.** Sekce THROW-INS (ř. 867-878) mluví jen o „the last
  square the ball crossed before going off" a o šabloně; grep na `corner` má
  v celém dokumentu **jediný zásah** (ř. 3678) a týká se rozestavení v end zone,
  ne vhazování. BB2016 tedy rohovou situaci **ani nepotvrzuje, ani nevyvrací**.
- Boční část téže citace („distinct from the uniform 8-way Bounce scatter
  template") **potvrzena je** — ř. 902-903: „To find out where the ball bounces
  to, roll for scatter one more time." (odraz = scatter šablona, vhazování =
  throw-in šablona; jsou to dvě různé šablony).

## Vedlejší pozorování (mimo A/B/C — NEopravováno, jen hlášeno)

Tyhle věci nejsou rozpor **mezi edicemi**, ale rozpor **kód vs. BB2016**. Narazil
jsem na ně cestou; patří spíš do fronty pravidlových oprav než do T5.21.

**(1) Safe Throw — obojí půlka pravidla je vedle.** BB2016 ř. 8434-8441:
„If a pass made by this player is ever intercepted then the Safe Throw player
may make an **unmodified Agility roll**. If this is successful then the
interception is cancelled out … In addition if this player **fumbles a pass on
any roll other than a natural 1** then he manages to keep hold of the ball
instead of suffering a fumble and the team does not suffer a turnover."
- `pass_handler.cpp:116-124` hodí D6 a porovná ho s `intTarget` — to je
  **cíl INTERCEPTORA** (jeho AG, jeho −2 za pokus, jeho tackle zóny), ne
  **nemodifikovaný AG hod HÁZEČE**. Špatný hráč i špatné modifikátory.
- Druhá věta (ochrana proti fumblu na modifikovaném hodu) **není v kódu vůbec** —
  `grep SafeThrow *.cpp` má tři zásahy, všechny v bloku interceptu.
- Kdo tím trpí: majitelé Safe Throw (passingové týmy = **soupeři**, trpaslík
  Safe Throw nemá). Stejná strana jako F6/F7/F8 z 24.08.

**(2) Kick-off tabulka — tři výsledky se s BB2016 rozcházejí.**
`kickoff_handler.cpp:63-99` proti BB2016 ř. 1285-1313:
- **Riot** (ř. 1285-1296): pravidlo má **tři větve** (turn 7 → oba zpět; ještě
  se nehrálo → oba dopředu; jinak D6, 1-3 dopředu / 4-6 zpět) a hýbe
  **oběma** ukazateli kol. Kód má dvě větve a hýbe **jen útočícím** týmem.
- **Cheering Fans** (ř. 1303-1308): pravidlo je **D3 + FAME + cheerleaders**,
  a při **shodě dostanou reroll OBA**. Kód hází D6 bez FAME/cheerleaders
  a při shodě nedostane **nikdo**.
- **High Kick** (ř. 1298-1302): pravidlo dovolí posunout jen hráče, který
  **není v soupeřově tackle zóně**. Kód (ř. 79-86) bere prostě nejbližšího.
⚠️ Get the Ref i Perfect Defence jsou v kódu vědomé no-opy — to je označené.

**(3) Break Tackle „once per turn"** — `helpers.cpp:56-58` limit sám přiznává
jako nehlídaný. BB2016 ř. 7987-7991 ho potvrzuje: „This skill may only be used
**once per turn**." (Tedy potvrzení komentáře, ne nový nález.)

**(4) Překlep v samotném `rules_bb2016.txt`.** Tabulka DODGING MODIFIERS
(ř. 597-600) tiskne „Per opposing tackle zone … **+1**", ale prózový příklad
o 15 řádků výš (ř. 578-585) i celý zbytek pravidel počítají **−1**.
⇒ Tabulky v tomhle souboru nejsou spolehlivé samy o sobě; **ověřovat je prózou**.
Stejné riziko hrozí u dalších tabulek (viz duplikované AGILITY TABLE bloky).
