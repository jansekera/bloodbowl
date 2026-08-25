# T5.21 — dávka C: TESTY (22 citací na CRP/LRB6), čteno proti `rules_bb2016.txt`

**Zadáno 25.08.2026** (`evidence/fable_brief_t521_citations_20260825.md`). Model: Opus.
Pravidla: (1) neodpovídat z vlastní znalosti — citovat BB2016 s číslem řádku,
(2) A = špatný zdroj/týž obsah · B = špatný zdroj/JINÝ obsah (NÁLEZ) · C = nerozhodnuto,
(3) NIC neopravovat.

**Navíc proti dávce zdrojů:** u testu se ptáme ještě na jednu věc —
*PŘIŠPENDLUJE test dnešní (CRP) chování?* Tzn. asertuje-li test výsledek podle CRP,
pak oprava kódu na BB2016 shodí test a bude vypadat jako regrese.

⚠️ Poznámka ke korpusu zdrojů: `rules_crp_lrb6.txt` (344 řádků) je *Competition Rules pack*,
který sám o sobě říká, že z něj byly vyňaty „všechny popisy herních komponent". Kde tedy
citace odkazuje na popis komponenty (šablona vhazování), nelze ji ověřit ani proti CRP.

---

## Tabulka

(průběžně doplňována)

| soubor:řádek | téma | verdikt | BB2016 ř. | doslovná citace BB2016 | přišpendluje CRP? |
|---|---|---|---|---|---|
| test_ball_handler.cpp:206 | vhazování — 2D6 od posledního pole, šablona | **A** | 867-878 | „Use the Throw-in template to work out where the ball goes, using the last square the ball crossed before going off as a starting point to throw-in the ball 2d6 squares." | ne |
| test_ball_handler.cpp:228 | vhazování z ROHU (D3 volí ze tří směrů) | **C** | — | BB2016 obsahuje slovo „corner" jen 1× (ř. 3678, rohové pole end zóny) — rohové vhazování v textu **není**. V CRP taky ne (je to popis komponenty, CRP je z definice vynechal). | ne (nelze rozhodnout) |
| test_block_handler.cpp:160 | TD odsunem do end zóny v soupeřově kole | **A** | 997-1003 | „For example, a player holding the ball could be pushed into the End Zone by a block. If one of your players is holding the ball in the opposing team's End Zone at any point during your opponent's turn then your team scores a touchdown immediately…" | ne |
| test_block_handler.cpp:281 | crowd surf, Stunned → Reserves (ne KO) | **A** | 656-659 | „If a 'Stunned' result is rolled on the Injury table the player should be placed in the Reserves box of the Dugout, and must remain there until a touchdown is scored or the half ends." | ne — test asertuje jen `OFF_PITCH`, KO/Reserves nerozlišuje |
| test_block_handler.cpp:664 | blok v blitzu stojí 1 MP, jinak GFI; Frenzy 2. blok | **A** | 548-550 + 8143-8145 | 549: „The block may be made at any point during the move, but costs one square of movement for the player to make." · 8143: „If the frenzied player is performing a Blitz Action then he must pay a square of Movement and must make the second block unless he has no further normal movement and cannot go for it again." | ne |
| test_block_handler.cpp:799 | Stand Firm v řetězu odsunu | **A** | 8515-8516 | „If a player is pushed back into a player with using Stand Firm then neither player moves." (BB2016 má navíc překlep „with using") | ne |
| test_block_handler.cpp:824 | Dauntless: síla se počítá před asistencemi | **A** | 8025-8026, 8032-8034 | „The skill only works when the player attempts to block an opponent who is stronger than himself." · „The strength of both players is calculated before any defensive or offensive assists are added but after all other modifiers." | ne |
| test_enums.cpp:101 | tabulka počasí 2/3/4-10/11/12 | **A** | 1475-1494 | „2 Sweltering Heat… 3 Very Sunny… 4-10 Nice: Perfect Blood Bowl weather. 11 Pouring Rain… 12 Blizzard…" | ne |
| test_foul_handler.cpp:148 | Decay zdvojuje CASUALTY, ne Injury | **A** | 8036-8042 | „When this player suffers a Casualty result on the Injury table, roll twice on the Casualty table and apply both results." | ne |
| test_game_simulator.cpp:819 | Casualty „must miss the rest of the match" | **A** | 720-721 | „10-12 Casualty – Take the player off the pitch and place them in the Dugout in the Dead & Injured Players box. The player must miss the rest of the match." | ne |
| test_game_simulator.cpp:843 | Secret Weapon — vyloučen po konci drivu | **A** | 8451-8454 | „Once a drive ends that this player has played in at any point, the referee orders the player to be sent off to the dungeon… regardless of whether the player is still on the pitch or not." | ne |
| test_game_simulator.cpp:871 | totéž (helper `firstPlainHomePlayer`) | **A** | 8451-8454 | viz výše | ne |
| test_game_simulator.cpp:884 | KO se vrací na 4+ při příštím kick-offu | **A** | 710-717 (a 1009-1012) | „8-9 KO'd … At the next kick-off, before you set up any players, roll for each of your players that have been KO'd. On a roll of 1-3 they must remain in the KO'd box… On a roll of 4-6 you must return the player to the Reserves box…" | ne |
| test_game_simulator.cpp:914 | Sweltering Heat | **A** | 1477-1481 | „2 Sweltering Heat: … Roll a D6 for each player on the pitch at the end of a drive. On a roll of 1 the player collapses and may not be set up for the next kick-off." | ne |
| test_helpers.cpp:63 | dodge AG3 = 4+ z tabulky, +1 za dodge | **A** | 486-494, 578-582 | 578: „He gets a +1 to the roll for making a dodge, but has to subtract 2 because there are two Orc tackle zones on the square he is moving to, for a final modifier of -1." | ne |
| test_helpers.cpp:230 | asistence: nesmí být v TZ „kromě blokovaného" | **A** | 1663-1668 | „In order to make an assist, the player: 1. Must be adjacent to the enemy player involved in the block, and… 2. Must not be in the tackle zone of any other player from the opposing team, and… 3. Must be standing, and… 4. Must have his tackle zones." (BB2016 i CRP doslova stejně) | ne |

⚠️ **Metodická poznámka k extrakci textu:** v `rules_bb2016.txt` jsou v tabulkách
„Dodging Modifiers" (ř. 502-505) a „Pick-up Modifiers" (ř. 460-462) záporná znaménka
vytištěná jako **+1** („Per opposing tackle zone … +1"). Není to pravidlo — je to vada
extrakce PDF. Rozhoduje běžící text: ř. 578-582 (slow-motion replay) explicitně
„**has to subtract 2** because there are two Orc tackle zones". Kdo by citoval jen
tabulku, přečetl by pravidlo s **obráceným znaménkem**.

| test_injury.cpp:152 | Thick Skull: 8 po modifikátorech → Stunned | **A** | 8595-8598 | „This player treats a roll of 8 on the Injury table, after any modifiers have been applied, as a Stunned result rather than a KO'd result. This skill may be used even if the player is Prone or Stunned." (CRP doslova stejně) | ne |
| test_injury.cpp:188 | Regeneration 4-6 → Reserves box | **A** | 8407-8412 | „On a result of 1-3, the player suffers the result of this injury. On a 4-6, the player will heal the injury… and is placed in the Reserves box instead. Regeneration rolls may not be re-rolled." | ne |
| test_injury.cpp:234 | crowd surf Stunned → Reserves, ne KO | **A** | 656-659 | „If a 'Stunned' result is rolled on the Injury table the player should be placed in the Reserves box of the Dugout, and must remain there until a touchdown is scored or the half ends." | ne |
| test_injury.cpp:273 | apotékář: soupeř hodí znovu, vybíráš ty | **A** | 1204-1210 | „…you can use the Apothecary to make your opponent roll again on the Casualty table and then you choose which of the two results to apply." | ne |
| test_injury.cpp:356 | apotékář: „even if it was the original Casualty roll" | **A** | 1207-1210 | „If the player is only Badly Hurt after this roll (even if it was the original Casualty roll) the Apothecary has managed to patch him up… so that the player may be moved into the Reserves box." | ne |
| test_pass_handler.cpp:431 | Hand-Off je vlastní akce, 1× za kolo | **A** | 1675-1679 | „The Hand-Off Action is added to the list of Actions like Move, Block, Blitz and Pass. You may only declare one Hand-Off Action per turn." (+ ř. 430-433: „Only one Blitz and one Pass Action may be taken in each turn.") | ne |

---

## Souhrn

- **A (špatný zdroj, týž obsah): 20**
- **B (JINÝ obsah — NÁLEZ): 1** — `test_ball_handler.cpp:206`
- **C (nerozhodnuto): 1** — `test_ball_handler.cpp:228`

## B — plný zápis

### B1 · `test_ball_handler.cpp:206` (a stejná vada v `:228`) — VHAZOVÁNÍ LETÍ O JEDNO POLE DÁL

**Co dělá kód.** `engine/src/ball_handler.cpp:218` — `for (int step = 0; step < distance; ++step)`,
kde `distance = dice.roll2D6()`. Míč se posune **plných 2D6 polí OD** posledního pole v hřišti.
Při 2D6 = 5 z (10,0) skončí na (10,5).

**Co říká BB2016.** ř. 870-872: „…using the last square the ball crossed before going off as
a starting point to throw-in the ball 2d6 squares." — samo o sobě dvojznačné, a proto to
BB2016 **ve svém vlastním FAQ výslovně rozhoduje**, ř. 9307-9314:

> „Q. When I use the throw-in template, does the square where the Blood Bowl logo is centred
> count as the first square of the distance that the ball is thrown, or is it placed there and
> then moved 2D6 squares?
> **A. The square with the Blood Bowl logo over it counts as the first square of the ball's
> movement.** So, if the result of the 2D6 roll was a 2, the ball would be placed in the square
> with the Blood Bowl logo over it, then move one square in the relevant direction."

Tedy **2D6 − 1 kroků** od výchozího pole. Při 2D6 = 5 má míč skončit na (10,4), ne (10,5).
(FAQ je součástí tohoto korpusu — hlavička souboru, ř. 15: „Errata and FAQ (pdf May 2017)".)

**⚠️ Poctivé ohraničení verdiktu.** Že se **edice rozcházejí**, doložit neumím: `rules_crp_lrb6.txt`
říká jen „(see page 3 for how the Throw-in template is used to throw-in the ball 2d6 squares)"
a stránku 3 (popis komponenty) Competition Rules pack neobsahuje; LRB6 FAQ nemáme.
Doložená je druhá půlka, a to je ta, na kterou se úloha ptá: **kód nehraje podle BB2016.**

**Kdo z toho těží.** Obojí, ale nesymetricky: každé vhazování letí o pole dál od postranní
čáry doprostřed hřiště. Kdo stojí blíž středu, má větší šanci, že míč dopadne na něj nebo
vedle něj. **Neměřeno** — nemám k tomu číslo a nevymýšlím ho.

**V jakých situacích to hraje.** Všechny cesty do `resolveThrowIn`: odsun mimo hřiště
(`block_handler.cpp:311, 807`), rozptyl přihrávky/kopu ven (`pass_handler.cpp:193, 358`),
odraz míče ven (`ball_handler.cpp:79`) — a rekurzivně i opakované vhazování.

**⛔ PŘIŠPENDLUJE?** **ANO, dvakrát.** `ThrowInSideExitStraight` asertuje `(11,5)`
a `ThrowInCornerExit` asertuje `(1,4)`. Oprava na BB2016 obě shodí — a k tomu i zbylých
**pět** necitovaných throw-in testů v témž souboru (`ThrowInSideExitDiagonal:216`
a čtveřici z 24.08. `:244, :259, :272, :289`), protože všechny počítají dopad ze stejné
vzdálenosti. **Kdo by opravu udělal, uvidí 7 červených testů a přečte to jako regresi.**

## C — plný zápis

### C1 · `test_ball_handler.cpp:228` — VHAZOVÁNÍ Z ROHU

Komentář popisuje mechaniku „D3 vybírá ze tří směrů: podél jedné hrany / úhlopříčně /
podél druhé hrany" a připisuje ji LRB6.

- V `rules_bb2016.txt` se slovo „corner" vyskytuje **jednou** (ř. 3678) a je to rohové pole
  end zóny, ne vhazování. Rohové vhazování v textu **není**.
- V `rules_crp_lrb6.txt` **taky ne** — a má to systémový důvod: pack sám v úvodu říká
  „All descriptions of game components … have been removed" a u vhazování odkazuje na
  „page 3" hlavního rulebooku.

⇒ **C, a je to poctivé C:** citace připisuje LRB6 pravidlo, které v našem LRB6 textu není.
Kód tu tedy nedodržuje CRP ani BB2016 — **dodržuje sám sebe**, a test to přišpendluje.
Rozhodnout to z textů, které máme, nelze; potřebný je obrázek šablony vhazování
(BB2016 i LRB6 ji mají jako komponentu).

## Testy, které PŘIŠPENDLUJÍ chování nesouhlasící s BB2016

1. `test_ball_handler.cpp:204` `ThrowInSideExitStraight` — asertuje `(11,5)`; podle BB2016 FAQ `(11,4)`.
2. `test_ball_handler.cpp:226` `ThrowInCornerExit` — asertuje `(1,4)`; táž vada vzdálenosti + vymyšlené D3.

Zbylých 20 citací nic nepřišpendluje: buď asertují chování, které je v obou edicích stejné,
nebo asertují slabší tvrzení než citace (např. `CrowdSurf` kontroluje jen `OFF_PITCH`,
KO vs Reserves nerozlišuje).

## By-catch (MIMO těchto 22 citací, NENÍ to nález T5.21)

`engine/src/injury.cpp:46-48` implementuje **Stunty jako „+1 ke hodu na zranění"**.
BB2016 ř. 8534-8536 i CRP shodně říkají něco jiného: „this player treats a roll of **7 and 9**
on the Injury table after any modifiers have been applied as a **KO'd and Badly Hurt** result
respectively". Rozdíl se projeví na přirozené **9**: náš kód z ní udělá 10 = plný hod na
Casualty tabulku (může být DEAD), pravidla dávají přesně **Badly Hurt** (= Reserves).
**Obě edice stejně** ⇒ pravidlová vada, ne edice. `test_injury.cpp:244` `StuntyInjuryBonus`
tenhle mechanismus přišpendluje (asertuje 7+1=8 → KO), i když u něj žádná citace není.
