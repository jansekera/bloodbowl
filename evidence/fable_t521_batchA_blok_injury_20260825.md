# T5.21 — dávka A: RODINA BLOK + ZRANĚNÍ (21 citací)

**Zadáno 25.08.2026** (`evidence/fable_brief_t521_citations_20260825.md`).
Model: Opus. Zdroj pravidel: `rules_bb2016.txt` (9 518 ř., hlavička „Blood Bowl 2016, October 2017").

⛔ Tento soubor **jen čte a hlásí**. Nic se neopravovalo, žádná citace se nepřepisovala,
nespouštěl se test ani engine.

Verdikty: **A** = špatný zdroj, týž obsah · **B** = špatný zdroj, JINÝ obsah (NÁLEZ) · **C** = nerozhodnuto.

## Tabulka

| soubor:řádek | téma | verdikt | BB2016 ř. | doslovná citace BB2016 | sedí kód? |
|---|---|---|---|---|---|
| block_handler.cpp:150 | odsun do prázdného pole | **A** | 639 | „The player must be pushed back into an empty square if possible." | ano |
| block_handler.cpp:269 | řetězový odsun | **A** | 641-646 | „If all such squares are occupied by other players, then the player is pushed into an occupied square, and the player that originally occupied the square is pushed back in turn. This secondary push back is treated exactly like a normal push back as if the second player had been blocked by the first" | ano |
| block_handler.cpp:796 | surf nosiče → vhazování | **A** | 659-663 | „If the player who is holding the ball is pushed out of bounds, then he is beaten up by the fans, who are more than happy to throw the ball back into play! The throw-in is centred on the last square the player was in before he was pushed off the pitch." | ano |
| injury.cpp:232 | dav: jeden hod, bez modifikátorů | **A** | 651-654 | „A player pushed off the pitch, even if Knocked Down, is beaten up only by the crowd and receives one roll on the Injury Table … The crowd does not have any injury modifying skills." | ano |
| injury.cpp:245 | Stunned od davu → Reserves | **A** | 656-658 | „If a 'Stunned' result is rolled on the Injury table the player should be placed in the Reserves box of the Dugout, and must remain there until a touchdown is scored or the half ends." | ano |
| injury.cpp:8 | Casualty tabulka jako D68 | **A** | 2371-2379, 2405-2423 | „The dice scores on the Casualty table run from 11 through to 68 … 11-38 Badly Hurt / 41-48 Miss next game / 51-52 Niggling / 53-54 -1 MA / 55-56 -1 AV / 57 -1 AG / 58 -1 ST / 61-68 DEAD" | ano |
| injury.cpp:36 | Decay nesahá na Injury hod | **A** | 8036-8042 | „When this player suffers a Casualty result on the Injury table, roll twice on the Casualty table and apply both results." | ano |
| injury.cpp:83 | apotékář: druhý hod, vyber výsledek | **A** | 1204-1207 | „immediately after the player suffers the Casualty, you can use the Apothecary to make your opponent roll again on the Casualty table and then you choose which of the two results to apply." | ano |
| injury.cpp:103 | Badly Hurt = jistý návrat | **A** | 1207-1210 | „If the player is only Badly Hurt after this roll (even if it was the original Casualty roll) the Apothecary has managed to patch him up … so that the player may be moved into the Reserves box." | ano |
| block_handler.cpp:519 | Dauntless (znění CRP) | **A** | 8023-8035 | „The skill only works when the player attempts to block an opponent who is stronger than himself. … The strength of both players is calculated before any defensive or offensive assists are added but after all other modifiers." | ano |
| block_handler.cpp:533 | Dauntless (už cituje BB2016) | **A** | 8023-8035 | totéž; tvrzení „doslova shodné s CRP" **ověřeno** | ano (⚠ rozsah v komentáři je 8023-**8033**, text končí až na **8035**) |
| block_handler.cpp:648 | Juggernaut ruší Fend/Stand Firm/Wrestle; Wrestle je VOLBA | **A** | 8190-8195, 8670-8678 | Juggernaut: „If this player takes a Blitz Action, then opposing players may not use their Fend, Stand Firm or Wrestle skills against blocks, and he may choose to treat a 'Both Down' result as if a 'Pushed' result has been rolled instead." · Wrestle: „This player **may** use Wrestle when he blocks or is blocked and a 'Both Down' result … Both players are Placed Prone in their respective squares **even if one or both have the Block skill**." | ano (Both Down→Pushed je implementován, block_handler.cpp:574-576) |
| block_handler.cpp:864 | Fend zakazuje follow-up i po pádu | **A** | 8115-8116 | „Opposing players may not follow-up blocks made against this player even if the Fend player is Knocked Down." | ano |
| block_handler.cpp:852 | Sure Hands ruší Strip Ball | **A** | 8543-8546 | „A player with the Sure Hands skill is allowed to re-roll the D6 if he fails to pick up the ball. In addition, the Strip Ball skill will not work against a player with this skill." | ano |
| block_handler.cpp:898 | Mighty Blow jen na JEDEN z hodů | **A** | 8291-8297 | „Add 1 to any Armour or Injury roll … Note that you only modify one of the dice rolls, so if you decide to use Mighty Blow to modify the Armour roll, you may not modify the Injury roll as well." | ano |
| block_handler.cpp:930 | 2. blok Frenzy na blitzu stojí pohyb | **A** | 8134-8145 | „If a 'Pushed' or 'Defender Stumbles' result was chosen, the player must immediately throw a second block … If the frenzied player is performing a Blitz Action then he must pay a square of Movement and must make the second block unless he has no further normal movement and cannot go for it again." | ano |
| block_handler.cpp:416 | blok na blitzu stojí 1 pole pohybu | **A** | 347-350, 549-552, 1694-1699, 1489-1494 | „Blitz: … He may make one block during the move. The block may be made at any point during the move, and 'costs' one square of movement." · GFI: „When a player takes any Action **apart from a Block**, they may try to move one or two extra squares" (⇒ na blitzu GFI lze) · Blizzard: „any player attempting to move an extra square (GFI) will slip and be Knocked Down on a roll of **1-2**" | ano (GFI cíl 2+, v blizzardu 3+) |
| injury.cpp:60 | Thick Skull = deterministicky, jen modif. 8 | **A** | 8595-8598 | „This player treats a roll of 8 on the Injury table, after any modifiers have been applied, as a Stunned result rather than a KO'd result." | ano |
| big_guy_handler.cpp:31 | Really Stupid: spojenec, který sám není RS | **A** | 8393-8395 | „If there are one or more players from the same team standing adjacent to the Really Stupid player's square, **and who aren't Really Stupid**, then add 2 to the D6 roll." | ano |
| block_handler.cpp:155 | nosič odsunutý do koncové zóny skóruje | **A** | 997-1005 | „In some rare cases a team will score a touchdown in the opponent's turn. For example, a player holding the ball could be pushed into the End Zone by a block. If one of your players is holding the ball in the opposing team's End Zone at any point during your opponent's turn then your team scores a touchdown immediately, **but must move their Turn marker one space along the Turn track**…" | ano (⚠ posun Turn markeru — viz Pozorování P3) |
| foul_handler.cpp:83 | vyloučení za faul = turnover | **A** | 1878-1882 (+ 1280-1281, 2876-2877) | **BB2016 to říká PŘÍMO**, ne oklikou přes Bribe: „if the Armour and/or Injury roll is a doubles … the player taking the Foul Action is sent off … **In addition, his team suffers a turnover and their turn ends immediately.**" Bribe potvrzuje z druhé strany: „the bribe is effective (preventing a turnover if the player was ejected for fouling)" | ano |

## Součet dávky A

**21 citací: A = 21 · B = 0 · C = 0.**

⚠️ **Nulové B je samo o sobě nález, a je třeba ho číst správně.** Neznamená
„u bloku a zranění je vše v pořádku". Znamená: **tam, kde kód CITUJE pravidlo,
se BB2016 a CRP u rodiny blok+zranění nerozcházejí** — ověřeno doslovným textem
s číslem řádku u všech 21, ne z paměti modelu. Rozpory, které jsem v této rodině
našel, leží **mimo citace** — v místech, kde žádný odkaz na pravidla není
(viz Pozorování P1) nebo kde citace sedí, ale zdůvodnění pod ní zestárlo (P2).

Metodická poznámka k brzdě (1): u čtyř citací by „populární výklad" dal stejnou
odpověď jako text (Fend, Mighty Blow, Really Stupid, Thick Skull). U všech čtyř
jsem text dohledal a cituji ho s řádkem — shoda je tedy doložená, ne převzatá.

---

## POZOROVÁNÍ — věci, na které jsem narazil při čtení, MIMO 21 citací

Nic z toho jsem neopravoval. Řadím podle váhy.

### P1 ⭐⭐⭐ STUNTY: kód dělá „+1 k Injury", BB2016 předepisuje MAPOVÁNÍ 7→KO, 9→Badly Hurt

**Kde:** `engine/src/injury.cpp:46-49` — **bez jakéhokoli odkazu na pravidla.**
```
    // Stunty: +1 to injury
    if (player.hasSkill(SkillName::Stunty)) {
        injuryRoll += 1;
    }
```
**BB2016 ř. 8534-8536** (Stunty, Extraordinary):
> „In addition, this player treats a roll of **7 and 9** on the Injury table after
> any modifiers have been applied as a **KO'd and Badly Hurt** result respectively
> rather than the normal results."

**Co je jiné.** Pro čistý hod jsou obě verze číselně shodné (6→Stunned, 7→KO,
8→KO, 9→casualty, 10+→casualty). Rozcházejí se ve **dvou** bodech:

1. **Devítka nesmí zabít.** BB2016 říká na 9 konkrétní výsledek **Badly Hurt**
   („No long term effect", ř. 2406) — na Casualty tabulku se vůbec nehází.
   Náš kód udělá z 9 desítku a pošle ji do `rollCasualty()` (`injury.cpp:85`),
   kde je **1/6 DEAD** (ř. 2423, 61-68). Stunty hráč u nás na devítce umírá,
   podle BB2016 umřít nemůže.
2. **Kolize s Thick Skull.** Thick Skull (ř. 8596-8597) mluví o „a roll of **8**
   … **after any modifiers**". Náš +1 posouvá hod DŘÍV, než se Thick Skull ptá
   (`injury.cpp:69`, `injuryRoll == 8`), takže se u Stunty hráče ptá na špatné
   číslo v obou směrech: modif. 7 se u nás zachrání na Stunned (podle BB2016 má
   být KO), modif. 8 u nás jde do KO (podle BB2016 má být Stunned).

**Kdo z toho těží:** obojí — je to vlastnost nesená hráčem, ne stranou. V praxi
těží ten, kdo **nemá** Stunty (Stunty hráč u nás umírá častěji, než smí).

**Kdy to hraje:** ⚠️ **v současném korpusu NIKDY.** Sestavy TV1200 Stunty
neobsahují: `roster.cpp:499` říká doslova „Orc TV~1280: **goblins removed**",
a v ostatních čtyřech TV1200 sestavách (human, dwarf, skaven, wood-elf) Stunty
není. Živé je to jen v **základních** rosterech pod TV1200 (orc goblin
`roster.cpp:47`, dále halfling/goblin/lizardmen/ogre atd.). ⇒ **latentní**,
tedy podle naší vlastní konvence **odložené, ne zavřené**.

### P2 ⭐⭐ DECAY: citace sedí, ale zdůvodnění pod ní ZESTÁRLO ve stejném balíku

**Kde:** `engine/src/injury.cpp:36-44` (citace č. 14 této dávky, verdikt A).
Komentář správně cituje pravidlo (BB2016 ř. 8038-8040) a správně vysvětluje,
proč Decay **nesmí** sahat na Injury hod. Poslední věta ale říká:

> „Since this engine models a single match and **has no Casualty table** (10+ is
> simply INJURED), a correct Decay has no in-match effect at all"

To už **neplatí**: Casualty tabulku engine má — `rollCasualty()` je na
`injury.cpp:11-24` a volá se na `injury.cpp:85`. Obojí přibylo ve **stejném
balíku G z 10.08.**, co se psal tenhle komentář (viz `injury.cpp:81`).

**Důsledek:** `ctx.hasDecay` se nastavuje na **třech** místech
(`injury.cpp:239`, `foul_handler.cpp:57`, `block_handler.cpp:902`) a **nikde se
nečte** (ověřeno grepem přes `engine/src` i `engine/include`). Decay tedy není
implementovaný, ačkoli teď už implementovatelný je: BB2016 ř. 8038-8040 chce
„roll **twice** on the Casualty table and apply both results". Na výsledek
v rámci zápasu to sahá přes DEAD (druhý hod může zabít tam, kde první dal Badly
Hurt) a přes apotékáře/Regeneration.

**Kdy to hraje:** Decay nese Nurgle/Necromantic — v korpusu TV1200 **není**.
Znovu latentní.

### P3 ⭐⭐ TD V SOUPEŘOVĚ KOLE: chybí posun Turn markeru

**Kde:** citace `block_handler.cpp:155` (verdikt A) stojí na sekci
„SCORING IN THE OPPONENT'S TURN". Ta sekce má ale **dvě** věty, a druhou
neplníme. **BB2016 ř. 1002-1005:**
> „…then your team scores a touchdown immediately, **but must move their Turn
> marker one space along the Turn track to represent the extra time the players
> spend celebrating this unusual method of scoring!**"

Engine TD v soupeřově kole **uznává** (`action_resolver.cpp:242` volá
`checkTouchdown` po každé akci, bez ohledu na to, kdo je na tahu — premisa
komentáře tedy platí), ale **Turn marker neposouvá** — v cestě přes
`action_resolver.cpp:241-248` ani v `game_simulator.cpp` žádné navýšení
`turnNumber` po TD není. Skórující tým by měl o jedno kolo v půli přijít.

**Kdo z toho těží:** ten, kdo takto skóruje — u nás zadarmo. Podle měření
z 20.08. (`block_handler.cpp:160-162`) šlo o **8 soupeřových nosičů na 3 000
her** *dřív, než jsme začali koncovou zónu odmítat*; kolik jich takhle skóruje
dnes, změřené nemám a neodhaduji.

### P4 ⭐ FAUL: vyloučený nosič pouští míč z pole {-1,-1}

**Kde:** `foul_handler.cpp:72-74` — pořadí operací.
```
    fouler.state = PlayerState::EJECTED;
    fouler.position = {-1, -1};
    handleBallOnPlayerDown(state, fouler.id, dice, events);
```
`handleBallOnPlayerDown` (`ball_handler.cpp:264`) čte pozici hráče **až uvnitř**,
takže dostane už vynulované `{-1,-1}` a odraz počítá odtamtud.

**BB2016 ř. 1882-1884:**
> „If the sent off player was holding the ball, the ball **bounces from the square
> he was standing in when sent off**."

Míč tedy má odskočit z jeho **posledního pole na hřišti**. Poznámka: `{-1,-1}`
plus rozptyl `(+1,+1)` dá `(0,0)`, což **je** platné pole — míč se může
teleportovat do rohu domácí koncové zóny; jinak spadne na vhazování ze
špatného místa. Spadá to přesně do třídy z 24.08.: *nebezpečí je mimo nabídku*
(míč se pokládá mimo generátor tahů). Faul s míčem v ruce je vzácný, ale možný.

### P5 (drobnost) rozsah řádků u Dauntless

`block_handler.cpp:533` píše „BB2016 l. **8023-8033**"; text Dauntless končí až
na **8035** (věta o assistech, na které celá oprava stojí, je na 8033-8035).
Tvrzení „doslova shodné s CRP" jsem ověřil — platí.

### P6 (pro T5.17, ne pro tuhle úlohu) věci v BB2016, které cituji jen jako záchyt

Narazil jsem na ně cestou, **neprověřoval** jsem, jestli je engine plní:
- ř. 1202-1203 — apotékář na **KO'd** hráče („leave him on the pitch Stunned or
  in the Reserves box if not on the pitch"). Náš apotékář se pouští jen ze
  Casualty větve (`injury.cpp:135`), KO větev (`injury.cpp:76-79`) ho nezná.
- ř. 8117-8118 — Fend: „The opposing player **may still continue moving** after
  blocking if he had declared a Blitz Action."
- ř. 8598 — Thick Skull: „may be used **even if the player is Prone or Stunned**."
- ř. 8676-8678 — Wrestle: „**Do not make Armour rolls for either player.** Use of
  this skill does not cause a turnover unless the active player was holding the ball."
- ř. 599-600 — v textu BB2016 je **překlep**: „Per opposing tackle zone on the
  square that the player is dodging to **+1**" (má být −1); správné znaménko
  drží příklad na ř. 580-582.
