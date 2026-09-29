# AUDIT PARITY PHP × C++ — 29.09.2026 (první pondělní)

Zadání: `evidence/fable_brief_parity_audit_20260929.md`. Audit ČTENÍM — žádný kód neměněn, nic nekompilováno, nic nespuštěno.
PHP = `src/`, C++ = `engine/src/`. Pravidla = `rules_bb2016.txt`.

## Souhrn

**Tabulka zadání (56 commitů):** SHODNÉ **32** · CHYBÍ **22** (z toho 12 jen „drobné/část“) · NEMÁ SMYSL **2** · NEJDE ROZHODNOUT ČTENÍM **0**.
**C++ commity od 24.08. bez PRAVIDLA (15 řádků, AI skupina sloučená):** SHODNÉ 7 · CHYBÍ (v PHP) 6 · NEMÁ SMYSL 2.
**Navíc mimo tabulku:** N1 (lékárník — dva různé modely), N2 (C++ hází počasí při každém výkopu), N3 (Stunty na zranění — oba enginy mimo BB2016), N4 (Leap v PHP).

Hlavní závěry:
- Mezery, které MĚNÍ MĚŘENÍ v C++ (všech 5 TV1200 sestav): **týmový přehoz při chytání v soupeřově kole a při výkopu** (`a7603e8f`),
  **obranná asistence u faulu v C++ nikdy nevznikne** (`d0c6e1d7`), **počasí se hází znovu při každém výkopu** (N2),
  **rozptyl nepřesné přihrávky přes aut a střed vhazování** (`cd2f72fe`). Vše ostatní, co v C++ chybí, se týká skillů, které
  v měřených sestavách nejsou (Blood Lust, Ball & Chain, Bombardier, Chainsaw, Always Hungry, Safe Throw, Dirty Player, Pro, TTM …).
- Známé mezery potvrzeny: **E28** pohyb po bloku v Blitzu — PHP pořád CHYBÍ (a follow-up je v PHP automatický, ne volba).
  **E18/E19** — C++ má zakořeněného v odtlačení správně (E18), ale E19 (konec zakořenění po sražení) a E20b (ležící Stand Firm)
  jen částečně: v řetězovém odtlačení drží pole i vleže.
- Pravidlové opravy balíku E (25.09.) se do C++ nepřenesly skoro žádné — kromě těch, které C++ měl dřív (TA3-TA10 z 24.08.).

## Seznam commitů (průběh)

- [x] `9e980cac` Blood Lust -- krmeni az na konci akce, Block -> Move (E25)
- [x] `966a871c` Regeneration az PO lekarnikovi a jen jednou (E26)
- [x] `60ba5961` Hypnotic Gaze -- Agility roll a konec efektu dalsi akci (E21+E22)
- [x] `aba4e078` odtlaceni vc. retezu na jednom miste (BlockHandler::odtlacit)
- [x] `3cec72e2` kdo drzi pole -- Take Root a Stand Firm (E18, E19, E20b)
- [x] `04e4946c` Ball & Chain -- blok podle sily, lezici odtlacit + brneni, follow-up (E14+E15)
- [x] `e373bf23` Ball & Chain -- sablona vhazovani, dav, zraneni bez brneni (E12+E13+E16)
- [x] `d9910a96` Bombardier -- sedm rozporu s pravidly (E10)
- [x] `ced20b99` Always Hungry -- dva hody, smrt, rozptyl z pole obeti, fumble (E9)
- [x] `26508221` No Hands nezachytava prihravky (E8)
- [x] `926b4a7a` Secret Weapon -- vylouceni kazdeho, kdo v drivu hral (E7)
- [x] `fa2422ad` pila nejde s Multiple Block (E3)
- [x] `78bf7601` faul pilou -- +3 k brneni a hod na zpetny raz (E4)
- [x] `9e02a834` Chainsaw -- nositel pily srazeny => souper +3 k brneni (E5)
- [x] `0003267e` Chainsaw -- +3 k brneni zasazeneho a turnover pri zpetnem razu
- [x] `04a5d0f7` Sweltering Heat -- D6 za kazdeho hrace na konci drivu
- [x] `5f9cb83a` na hristi smi jen 11 hracu, zbytek je lavicka
- [x] `266d7ae4` tabulka nasledku D68 -- smrt uz muze nastat
- [x] `51e99c83` surf -- "Stunned" jde do REZERV
- [x] `01bdfd77` TTM -- -1 k hodu, jen quick/short a fumble na PUVODNI pole
- [x] `ebee6362` (C++) po neuspesnem Pro jde tymovy prehoz na HOD PRO
- [x] `6a6537ef` Dirty Player je VOLBA; Break Tackle 1x za kolo a jen kdyz pomuze
- [x] `a7603e8f` tymovy prehoz u chytani po odskoku (vc. prehozu HODU PRO)
- [x] `1657a19e` TTM -- presny i nepresny hod rozptyluje hrace TRIKRAT
- [x] `196813c8` faul -- dublet i na hodu na ZRANENI
- [x] `c4d082e1` Pro u chytani po odskoku a vhazeni; prehoz z Catch neni potichu
- [x] `ec3feb2c` hozeny NOSIC v davu vrati mic vhazenim
- [x] `29d421c4` Dodge prehodi jen JEDEN neuspesny uhyb za kolo
- [x] `cebf9703` throw-in sablonou + 2D6, chytani/odskok, ...; nosic v davu a odskok na leziciho
- [x] `5fabdd12` interaktivni prehoz jen z nabidky; Hail Mary zapise tymovy prehoz
- [x] `84f7dc46` Pro 1x za kolo a kostka nejvys 1x prehozena; Sure Feet 1x za kolo
- [x] `cd2f72fe` nepresna prihravka se rozptyluje 3x po jednom poli
- [x] `d0c6e1d7` faul -- asistence misto pausalniho +1, vylouceni je turnover
- [x] `f58eecb1` tymove prehozy se o polocase vraci na vychozi pocet
- [x] `e278c6a2` hod na navrat KO hracu i po touchdownu
- [x] `d9c91cc3` vytlaceni do publika nema modifikator zraneni
- [x] `8623a6b5` tri kostky bloku az pri VICE nez dvojnasobne sile
- [x] `a02b6ec9` Thick Skull BB2016 -- "modifikovana 8 = Stunned"
- [x] `d674d080` tabulka vykopu -- 7 a 8 prohozene
- [x] `354a7dda` tabulka pocasi -- rozdeleni
- [x] `d852dcd6` ve vanici jen quick a short prihravky
- [x] `9c60e468` Disturbing Presence pusobi i lezici a omraceny
- [x] `e81bb57d` fumble prihravky "1 nebo mene PO modifikaci"; Safe Throw si mic udrzi
- [x] `faef31f3` Safe Throw -- hazi HAZEC na svou AG; Very Long Legs ho vypina
- [x] `534c9e40` intercepce -- zony, dest, DP, Extra Arms, NoS, podminka zony
- [x] `4c0e5240` zvedani mice -- bez Nerves of Steel, Big Hand ignoruje i dest
- [x] `8cf8e316` pocasi -- dest a vanice u prihravky, vanice u chytani a zvedani
- [x] `afc015c3` skill Pass se pocital dvakrat
- [x] `dea6d70c` Mighty Blow je VOLBA
- [x] `93043345` Stunty a Mighty Blow u Stab/Chainsaw
- [x] `2f930279` "lezici nechyta mic"
- [x] `a77941d8` hand-off a TTM vyhlasovaly turnover
- [x] `8460a2ff` neuspesny gaze NENI turnover + obet se nepocita do modifikatoru
- [x] `80852863` Wild Animal podle textu + blitz mimo dosah
- [x] `496f5a03` (C++) dodge pri VYSTUPU z tacklezony
- [x] `82673bb8` A1 Leap: mereni
- [x] C++ commity od 24.08. bez slova PRAVIDLA (strucne)

## Tabulka výsledků

| commit | co se měnilo (1 věta) | druhý engine: soubor:řádek | verdikt | dopad na měření |
|---|---|---|---|---|
| `9e980cac` | Blood Lust: na 1 je upír „hladový“, akci DOKONČÍ a krmí se až na jejím konci (před přihrávkou/předáním/TD); Block smí změnit na Move | `engine/src/big_guy_handler.cpp:164-219` + `action_resolver.cpp:255-276` | **CHYBÍ** — C++ kouše HNED při ohlášení akce (před pohybem); bez Thralla hned rezervy + turnover, upír se vůbec nepohne; žádný stav „hungry“, žádná změna Block→Move. Hod na zranění Thralla / CAS=BH / turnover jen s míčem je shodné (TA10). | nízký (Vampire není v 5 měřených TV1200 sestavách) |
| `966a871c` | Regeneration se hází až PO lékárníkovi a jen jednou (lékárníkův hod ji znovu nehází) | `engine/src/injury.cpp:130-160` | **SHODNÉ** — C++ hází Regeneration na jednom místě, po hodu na CAS a po lékárníkovi, jednou. (Pozn.: lékárník v C++ přehazuje CASUALTY tabulku, r. 87-140; PHP přehazuje hod na zranění — mimo rozsah tohoto commitu, viz poznámka N1.) | nízký (Regeneration není v měřených sestavách) |
| `60ba5961` | Gaze: hod je Agility roll `7-AG+zóny` (bez oběti), efekt končí začátkem PŘÍŠTÍ AKCE oběti | E21: `engine/src/gaze_handler.cpp:22-31` (TA5, shodné). E22: `engine/src/game_state.cpp:72` (`lostTacklezones = bigGuyStupefied` při `resetPlayersForNewTurn`) | **CHYBÍ (drobné)** — E21 SHODNÉ. E22: C++ vrací zóny na ZAČÁTKU KOLA týmu oběti, ne začátkem její akce ⇒ oběť, která v tom kole neaktivuje, má v C++ zóny zpět, v PHP (a dle r. 8187-8188) ne. | nízký (Hypnotic Gaze není v měřených sestavách) |
| `aba4e078` | Odtlačení vč. řetězu jedním rekurzivním jádrem: nosič odtlačený v řetězu drží míč, Side Step platí i v řetězu (Grab ne), volné pole v řetězu volí tým na tahu stejně jako u 1. odtlačení | `engine/src/block_handler.cpp:291-392` (`pushOne` rekurzivně, r. 368-379 Side Step ano / Grab ne, r. 385-387 míč jde s nosičem, r. 344 stejná `choosePushSquare`) | **SHODNÉ** — C++ to tak má od dřív (jedno rekurzivní `pushOne` pro první i řetězová odtlačení). | — |
| `3cec72e2` | Kdo drží pole: zakořeněný vždy (i proti vlastním, i Juggernautovi), zakořenění končí jakmile hráč nestojí, Stand Firm drží jen STOJÍCÍ a jen proti soupeři | E18: `engine/src/block_handler.cpp:312` (řetěz), `:401` (1. odtlačení) — shodné. E19: `engine/src/game_state.cpp:83` (reset jen na ZAČÁTKU vlastního kola). E20b: `engine/src/block_handler.cpp:313` (bez kontroly STANDING). Fanatic: `ball_and_chain_handler.cpp:116-140` (neodtlačuje vůbec) | **CHYBÍ (částečně)** — E18 SHODNÉ. E19: sražený zakořeněný hráč má v C++ `rooted=true` až do začátku svého kola ⇒ v řetězovém odtlačení mezitím dál „drží pole“ vleže. E20b: ležící/omráčený Stand Firm v C++ řetěz blokuje (PHP po opravě ne, r. 1824-1825). Fanatic viz E12-E16. | nízký–střední (Treeman Wood Elf TV1200 má Take Root i Stand Firm; řetěz přes ležícího Treemana je vzácný) |
| `04e4946c` | B&C: blok podle síly vč. asistencí, ležící/omráčený v cestě = odtlačit + brnění, povinný follow-up z vlastního pole | `engine/src/ball_and_chain_handler.cpp:15-92` (`resolveAutoBlock`: vždy 1 kostka), `:116-140` | **CHYBÍ** — C++ hází VŽDY 1 kostku bez síly/asistencí; výsledek PUSHED/Stumbles cíl vůbec neodtlačí, B&C nepostoupí; ležící/omráčený v cestě se jen přeskočí (pole se neobsadí, „pojistka“, TA6 odloženo, r. 126-140). | nízký (B&C/Fanatic není v měřených sestavách a žádné makro ho neemituje) |
| `e373bf23` | B&C: směr šablonou vhazování (volba natočení + D6), mimo hřiště = dav (hod na zranění, ne auto-KO), sražený B&C = zranění bez brnění (Stunned=KO), sražení při bloku = turnover | `engine/src/ball_and_chain_handler.cpp:103-114` (D8 do 8 stran; mimo hřiště `KO`, „Never turnover“), `:27-33` (sražený B&C `resolveArmourAndInjury` = S brněním) | **CHYBÍ** — všechny čtyři body mají v C++ staré chování (D8, auto-KO, brnění, nikdy turnover). | nízký (viz výše) |
| `d9910a96` | Bombardier: nespotřebuje Pass akci, fumble vybuchne v poli házeče, sousedé na 4+, zasáhne i házeče a ležící/omráčené, turnover, přesnost přes PassResolver (fumble = modifikovaný ≤1), nesmí se před hodem hnout | `engine/src/bomb_handler.cpp:18-109` (TA4 24.08.), `rules_engine.cpp:266` (`!hasMoved`) | **CHYBÍ (drobné)** — body 1-6 a zákaz pohybu SHODNÉ. Liší se: (a) fumble jen při přirozené 1 (`bomb_handler.cpp:55`), ne „modifikovaný ≤1“; (b) omráčený zasažený hráč se v C++ přepíše na PRONE (`:94`) — PHP ho nechá omráčený. | nízký (Bombardier není v měřených sestavách) |
| `ced20b99` | Always Hungry: na 1 druhý hod; 2. jednička = sežrán (DEAD, míč 1× rozptyl z pole OBĚTI, s míčem turnover), 2-6 = vysmekne se = fumble (dopad na původní pole) | `engine/src/ttm_handler.cpp:23-51` (TA3) | **CHYBÍ (drobné)** — dva hody, DEAD, rozptyl z pole oběti a vysmeknutí = fumble na původní pole SHODNÉ. Liší se: sežraný NOSIČ míče v C++ vrací `ActionResult::ok()` (`:48`) — chybí turnover dle r. 382-384. | nízký (Always Hungry není v měřených sestavách; Ogre u Human má TTM, ne AH) |
| `26508221` | No Hands nezachytává přihrávky | `engine/src/pass_handler.cpp:53` | **SHODNÉ** | — |
| `926b4a7a` | Secret Weapon: vyloučen každý, kdo v drivu hrál (i když je už v KO/rezervách) — příznak `playedThisDrive` | `engine/src/game_simulator.cpp:318-319` (příznak při rozestavení), `:378-384` (vyloučení podle příznaku) | **SHODNÉ** — C++ má `playedThisDrive` od 11.08. Kosmetika: C++ přepíše i INJURED/DEAD na EJECTED (PHP je nechá zraněné) — na hru bez vlivu. | — |
| `fa2422ad` | Pila nejde s Multiple Block (nenabízí se, validace odmítne); s Frenzy jen jeden útok | `engine/src/rules_engine.cpp:300-318` (MB se nabízí bez ohledu na Chainsaw), `block_handler.cpp:595-633` (pila vrací před Frenzy) | **CHYBÍ (drobné)** — Frenzy SHODNÉ (pila skončí dřív, než se k Frenzy dojde). Multiple Block se nositeli pily v C++ nabízí a `resolveMultipleBlock` (`:1153`) projde `resolveBlock` ⇒ pila 2× za kolo. Prakticky nedosažitelné (žádná sestava nemá Chainsaw + MB). | nízký |
| `78bf7601` | Faul pilou: nejdřív D6 na zpětný ráz, pak +3 k brnění | `engine/src/foul_handler.cpp` (slovo Chainsaw v souboru vůbec není) | **CHYBÍ** — C++ faul pilu nezná (žádný zpětný ráz, žádné +3). | nízký (Chainsaw není v měřených sestavách) |
| `9e02a834` | Sražený NOSITEL pily ⇒ soupeř +3 k brnění (z jakéhokoli důvodu, i útočník padlý při bloku na nositele) | `engine/src/injury.cpp:171-` (`resolveArmourAndInjury` — žádná kontrola Chainsaw u oběti) | **CHYBÍ** — +3 proti sraženému nositeli pily C++ nemá (kromě zpětného rázu, kde +3 dává `block_handler.cpp:611`). | nízký |
| `0003267e` | Zásah pilou +3 k brnění (i zpětný ráz), zpětný ráz, který srazí nositele = turnover | `engine/src/block_handler.cpp:610-624` (TA7 24.08.) | **SHODNÉ** | — |
| `04a5d0f7` | Sweltering Heat: na konci drivu D6 za každého hráče na hřišti, 1 = vynechá příští rozestavení (ne KO) | `engine/src/game_simulator.cpp:389-393` (hod), `:222-227` (spotřebování `outNextSetup`) | **SHODNÉ** — PHP je port z C++. | — |
| `5f9cb83a` | Na hřišti max. 11 hráčů (horní mez u ručního rozestavení), méně jen když víc není (`min(11, dostupní)`) | `engine/src/game_simulator.cpp:221-232` (`nAvail < STARTERS`, zbytek zůstává v rezervách) | **SHODNÉ** — C++ rozestavuje jen automaticky, strop `STARTERS` i „méně, když nejsou“ má. Ruční rozestavení C++ nemá (nemá smysl). | — |
| `266d7ae4` | Tabulka následků D68 (D6 desítky × D8 jednotky), DEAD může nastat | `engine/src/injury.cpp:11-24` (`rollCasualty`), `:156-160` | **SHODNÉ** — PHP je port z C++ (balík G 10.08.). | — |
| `51e99c83` | Surf: výsledek „Stunned“ z davu ⇒ REZERVY, ne na hřiště | `engine/src/injury.cpp:244-253` | **SHODNÉ** | — |
| `01bdfd77` | TTM: −1 k hodu, jen Quick/Short, fumble ⇒ na PŮVODNÍ pole; fumble i při modifikovaném ≤1 | `engine/src/ttm_handler.cpp:57-62` (jen Quick/Short), `:76` (−1), `:84` (fumble jen `roll == 1`), `:92` (fumble na `origin`) | **CHYBÍ (drobné)** — tři hlavní body SHODNÉ (TA3). Chybí „fumble při modifikovaném ≤1“: např. AG3 Short (mod −1) hod 2 = v PHP fumble (hráč zůstane na místě), v C++ normální hod se 3 rozptyly. | nízký (TTM se v měřených hrách nehraje — Human Ogre nemá komu hodit, žádný Right Stuff) |
| `1657a19e` | TTM: přesný i nepřesný hod rozptyluje hráče 3×; po opuštění hřiště se dál nerozptyluje | `engine/src/ttm_handler.cpp:94-106` | **CHYBÍ (drobné)** — 3× rozptyl SHODNÝ. C++ ale hodí všechny 3 rozptyly bez kontroly hřiště (`:101-105`) a teprve pak `isOnPitch` ⇒ hráč, který vyletí a „vrátí se“, v C++ přistane na hřišti; v PHP jde do davu. | nízký (viz výše) |
| `ebee6362` (C++) | Blok: po neúspěšném Pro jde týmový přehoz na HOD PRO, ne na kostky bloku | `src/Engine/ProCheck.php:32-58`, `src/Engine/Action/BlockHandler.php:346-356` (`isProFailed` ⇒ přehazuje se hod Pro) | **SHODNÉ** — PHP to mělo od 18.09., C++ dorovnal. | — |
| `6a6537ef` | Dirty Player je VOLBA brnění/zranění (+1 na brnění jen když rozhodne, jinak na zranění); Break Tackle 1× za kolo a jen když ST > AG | Dirty Player: `engine/src/foul_handler.cpp:73-75` (+1 vždy k brnění). Break Tackle: `engine/src/helpers.cpp:88-93` (ST > AG ano; komentář sám říká „limit jednou za kolo nehlídáme“) | **CHYBÍ** — Dirty Player: C++ dává +1 VŽDY na brnění, na zranění nikdy. Break Tackle: podmínka ST>AG SHODNÁ, limit 1× za kolo chybí. | nízký (ani Dirty Player, ani Break Tackle nejsou v měřených sestavách) |
| `a7603e8f` | Týmový přehoz u chytání po odskoku (i přehoz HODU PRO) — ale JEN hráč týmu na tahu, ve fázi PLAY; u míče z výkopu ne (r. 929-933, r. 1263) | `engine/src/ball_handler.cpp:43-49` (`resolveCatch` → `attemptRoll(..., canUseTeamReroll=true)` NAPEVNO), `helpers.cpp:319-366`; volá se i z `kickoff_handler.cpp:316` (výkop), `pass_handler.cpp:227` (soupeř chytá rozptýlenou přihrávku), `ball_handler.cpp:89` (odskok na kohokoli) | **CHYBÍ** — část „v našem kole“ SHODNÁ (přehoz i přehoz hodu Pro, `helpers.cpp:327-338`). C++ ale týmový přehoz dovolí VŽDY: při chytání výkopu, v SOUPEŘOVĚ kole (odskok na našeho hráče po bloku, soupeř chytá naši nepřesnou přihrávku). PHP (a r. 929-933, 1263) ne. `canUseReroll()` (`team_state.h:28`) kolo na tahu nekontroluje. | **střední** — týká se všech 5 sestav, každého výkopu na hráče a každého odskoku v soupeřově kole; C++ tím pálí přehozy, které podle pravidel neexistují |
| `c4d082e1` | Pro u chytání po odskoku a po vhazování; přehoz z Catch je vidět v událostech; po Catch už Pro ne | `engine/src/ball_handler.cpp:43-49` + `helpers.cpp:302-346` (jeden řetěz skill → Pro → tým pro VŠECHNA chytání, každá větev končí návratem; Catch emituje `SKILL_USED`) | **SHODNÉ** | — |
| `196813c8` | Faul: dublet se čte i na hodu na ZRANĚNÍ (Sneaky Git odvrací i ten) | `engine/src/foul_handler.cpp:105-109` (`injuryDoubles`), `:113-121` | **SHODNÉ** — C++ od 21.08. | — |
| `ec3feb2c` | Hozený NOSIČ (TTM) v davu ⇒ míč se vrací vhazováním od posledního pole na hřišti (ne zmizí) | `engine/src/ttm_handler.cpp:108-118` (`crowd()`), `:109` pozice už je `{-1,-1}` ⇒ `ball_handler.cpp:262-270` položí míč na (−1,−1) a odrazí ho odtud | **CHYBÍ** — C++ nevhazuje: `handleBallOnPlayerDown` se volá AŽ po `projectile.position = {-1,-1}` (`:108`), takže míč „odskočí“ z pole mimo hřiště (D8 ⇒ buď vhazování z nesmyslného `origin`, nebo při směru (+1,+1) leží na (0,0)). | nízký (TTM se v měřených hrách nehraje) |
| `cebf9703` | Vhazování šablonou (strana D6 / roh D3) + 2D6, stojící musí chytat, prázdné/ležící = odskok, znovu od posledního pole; nosič v davu ⇒ vhazování od jeho posledního pole; odskok na ležícího pokračuje | `engine/src/ball_handler.cpp:128-260` (F9), `block_handler.cpp:970-976` a `:329-336` (nosič v davu, i v řetězu), `ball_handler.cpp:93-97` (odskok z ležícího) | **SHODNÉ** — PHP je port z C++ (F9, 24.08.). | — |
| `29d421c4` | Dodge přehodí jen JEDEN neúspěšný úhyb za kolo | `engine/src/helpers.cpp:307-312` (`dodgeRerollUsedThisTurn`), reset `game_state.cpp:79` | **SHODNÉ** — PHP je port z C++. | — |
| `84f7dc46` | Pro 1× za kolo (zapisuje se u všech hodů), ne po skill přehozu, po Pro už ne týmový na kostku (jen na hod Pro), blok: Pro přehodí VŠECHNY kostky; příznak Pro se nuluje KAŽDÉ kolo i soupeřovým hráčům; Sure Feet 1× za kolo | `engine/src/helpers.cpp:302-346` (řetěz), `block_handler.cpp:730-760` (Pro přehodí všechny kostky), `helpers.cpp:308` (Sure Feet 1×), `game_state.cpp:56-73` + `turn_handler.cpp:36` (reset jen týmu NA TAHU), `pass_handler.cpp:329-347` | **CHYBÍ (drobné)** — řetěz v `attemptRoll` a blok SHODNÉ. Liší se: (a) C++ nuluje `proUsedThisTurn` jen týmu, který začíná kolo ⇒ hráč, který Pro použil ve svém kole, ho v následujícím SOUPEŘOVĚ kole (např. chytání odskoku) nemá; PHP ano. (b) PŘIHRÁVKA: `engine/src/pass_handler.cpp:329-347` — po neúspěšném hodu Pro (`!rerolled`) jde týmový přehoz na HOD PŘIHRÁVKY, ne na hod Pro — přesně vada, kterou `ebee6362` opravil jen v bloku. | nízký (Pro není v měřených sestavách) |
| `5fabdd12` | Interaktivní přehoz (Pro / týmový) projde jen když byl nabídnut; Hail Mary zapíše použitý týmový přehoz při chytání | Interaktivní část: C++ nemá (přehozy rozhoduje automaticky `attemptRoll`). Hail Mary: `engine/src/pass_handler.cpp:197-238` → `resolveCatch` → `helpers.cpp:349-353` (`rerolls--`) | **NEMÁ SMYSL** (interaktivní nabídka v C++ není); část Hail Mary SHODNÁ (přehoz se odečte). | — |
| `cd2f72fe` | Nepřesná přihrávka (i Dump-Off, Hail Mary) se rozptyluje 3× po jednom poli; jakmile míč opustí hřiště, DÁL se nerozptyluje a vhazuje se od POSLEDNÍHO pole na hřišti | `engine/src/pass_handler.cpp:161-170` (`scatterThreeTimes` — bez kontroly hřiště mezi kroky), `:210-214` a `:374-378` (`resolveThrowIn(state, target, landPos, …)` — origin = CÍL přihrávky) | **CHYBÍ** — 3× po jednom poli SHODNÉ. Ale C++ (a) rozptýlí všechny 3 kroky i přes aut ⇒ míč, který vyletí a „vrátí se“, zůstane v C++ na hřišti; (b) vhazování centruje na CÍL přihrávky, ne na poslední pole, kudy míč opustil hřiště (r. 868-871) — směr šablony se navíc bere z koncového pole 3. rozptylu. Dump-Off C++ nemá (NEMÁ SMYSL). | nízký–střední (nepřesné přihrávky u lajny se v měřených hrách dějí; výsledek je vždy turnover, liší se jen kam dopadne míč) |
| `d0c6e1d7` | Faul: asistence (útočné vedle oběti − obranné vedle faulujícího, bez Guardu) místo paušálního +1; vyloučení = turnover | `engine/src/foul_handler.cpp:66-71` → `helpers.cpp:203-228` (`countAssists`), `:137-139` (turnover) | **CHYBÍ (část)** — útočné asistence, zákaz Guardu a turnover po vyloučení SHODNÉ. OBRANNÁ asistence v C++ **nevznikne nikdy**: volá se `countAssists(state, fouler.position, target.teamSide, fouler.id, target.id, /*tzExcludeId=*/-1, false)` ⇒ obránce vedle faulujícího je vždy v zóně FAULUJÍCÍHO (`helpers.cpp:221`) a neprojde. PHP má výjimku „sám faulující“ (r. 1843-1850). Faul je v C++ tedy snazší, než má být. | **střední** — fauly se v měřených hrách hrají (měřidlo exkluzivity faulu), obranná asistence u ležícího v mele je častá |
| `f58eecb1` | Týmové přehozy se o poločase vrací na výchozí počet (po TD ne) | `engine/src/game_simulator.cpp:326-330` (`resetHalfState` ⇒ `rerolls = 3`; `setupDrive` volá s `isNewHalf=false`, `:337-341`) | **SHODNÉ** — C++ nemá per-tým výchozí počet, všechny sestavy mají 3 (stejně jako `:157`). | — |
| `e278c6a2` | Hod na návrat KO hráčů i po touchdownu, ne jen o poločase | `engine/src/game_simulator.cpp:395-399` (ve společném `setupHalfOrDrive` pro poločas i drive po TD) | **SHODNÉ** | — |
| `d9c91cc3` | Vytlačení do publika: hod na zranění bez modifikátoru (+1 pryč) | `engine/src/injury.cpp:231-242` | **SHODNÉ** — C++ od 10.08. | — |
| `8623a6b5` | Tři kostky bloku až při VÍCE než dvojnásobné síle | `engine/src/helpers.cpp:230-237` (`attST > 2 * defST`) | **SHODNÉ** | — |
| `a02b6ec9` | Thick Skull (BB2016): modifikovaná 8 = Stunned, bez kostky navíc; 9 zůstává KO | `engine/src/injury.cpp:68-74` | **SHODNÉ** — C++ to měl správně dřív (PHP byl vzor edice 2020). | — |
| `d674d080` | Tabulka výkopu: 7 = Changing Weather, 8 = Brilliant Coaching (byly prohozené) | `engine/include/bb/enums.h:252-253` (`BRILLIANT_COACHING = 7`, `CHANGING_WEATHER = 8`), `kickoff_handler.cpp:275` (`static_cast` z hodu) | **CHYBÍ** — C++ má TUTÉŽ vadu (commit ji sám hlásí jako neopravenou): Brilliant Coaching padá 6/36 místo 5/36, Changing Weather 5/36 místo 6/36. Viz i poznámka N2 (C++ hází počasí při KAŽDÉM výkopu). | nízký (týká se všech her, ale jen ±1/36 extra přehozu; změna počasí je v C++ kvůli N2 skoro bez vlivu) |
| `354a7dda` | Tabulka počasí 2 Sweltering · 3 Very Sunny · 4-10 Nice · 11 Pouring Rain · 12 Blizzard | `engine/include/bb/enums.h:240-246` | **SHODNÉ** — C++ měl správně, vada byla jen v PHP. | — |
| `d852dcd6` | Ve vánici jen Quick/Short přihrávky (nenabízí se, validace i resolver); Hail Mary ve vánici vůbec | `engine/src/rules_engine.cpp:197-201` (nabídka), `pass_handler.cpp:258-261` (resolver) | **SHODNÉ** — pro běžnou přihrávku. Drobnost: Hail Mary má C++ jen jako „každá přihrávka hráče s HMP“ (`pass_handler.cpp:186-198`), ve vánici se nabídnou jen krátké cíle, ale proběhnou HMP mechanikou — PHP HMP ve vánici zakáže. HMP není v měřených sestavách. | — |
| `9c60e468` | Disturbing Presence působí i ležící a omráčený | `engine/src/helpers.cpp:35-45` (`forEachOnPitch`, bez kontroly stavu) | **SHODNÉ** | — |
| `e81bb57d` | Fumble přihrávky = přirozená 1 NEBO modifikovaný ≤1; Safe Throw: fumble jinak než přirozenou 1 ⇒ míč zůstává u házeče, bez turnoveru | `engine/src/pass_handler.cpp:289` (`isFumble`, F7 24.08.), `:348-352` (fumble ⇒ vždy odskok + turnover) | **CHYBÍ (část)** — fumble „≤1 po modifikaci“ SHODNÉ. Safe Throw v C++ při fumblu nic nedělá (žádná kontrola `SafeThrow` u fumblu). | nízký (Safe Throw není v měřených sestavách) |
| `faef31f3` | Safe Throw: po úspěšné intercepci hází HÁZEČ nemodifikovaný Agility roll (7−AG); proti Very Long Legs se nepoužije | `engine/src/pass_handler.cpp:136-144` | **CHYBÍ** — VLL výjimka SHODNÁ. Mechanika ne: C++ přehazuje kostku ZACHYCUJÍCÍHO proti jeho cíli (`reroll < intTarget`), tj. přesně to, co PHP opravil. | nízký (Safe Throw není v měřených sestavách) |
| `534c9e40` | Intercepce: jen hráč se zónou; −1 za zóny (NoS ignoruje), −1 DP, −1 Pouring Rain, +1 Extra Arms, VLL | `engine/src/pass_handler.cpp:52-78` (výběr) a `:82-110` (hod) | **SHODNÉ** — C++ měl od 10.08. / L2 09.09. (Výběr zachycujícího: C++ bere „nejpravděpodobnějšího“ z koridoru pravítka — PHP „první na dráze“, commit to nechává otevřené; mimo rozsah.) | — |
| `4c0e5240` | Zvedání: Nerves of Steel se nepočítá, Big Hand ignoruje zóny i Pouring Rain | `engine/src/helpers.cpp:167-181` | **SHODNÉ** | — |
| `8cf8e316` | Počasí: přihrávka jen Very Sunny −1; déšť jen chytání/zvedání/intercepce; vánice žádný modifikátor | `engine/src/pass_handler.cpp:276-278`, `helpers.cpp:173-175` (zvedání), `:198` (chytání), `pass_handler.cpp:108` (intercepce) | **SHODNÉ** | — |
| `afc015c3` | Skill Pass dává jen přehoz, ne −1 k cíli (+ krok PHP37 v kouči) | `engine/src/pass_handler.cpp:264-268` (modifikátory: Accurate ano, Pass ne), `:299-305` (Pass = přehoz) | **SHODNÉ** — pravidlová část. Část PHP37 (kouč oceňuje hod přihrávky) je AI vrstvy PHP — NEMÁ SMYSL srovnávat. | — |
| `dea6d70c` | Mighty Blow je VOLBA: na brnění jen když rozhodne, jinak na zranění | `engine/src/injury.cpp:198-217` | **SHODNÉ** — C++ měl od dřív. | — |
| `93043345` | Stunty při úhybu IGNORUJE zóny na cílovém poli (ne −1); Mighty Blow se nesmí použít se Stab/Chainsaw | `engine/src/helpers.cpp:126-128` (Stunty), `block_handler.cpp:610` a `:637-640` (ctx bez MB) | **SHODNÉ** — viz ale poznámka N3 (Stunty na zranění je v OBOU enginech mimo BB2016). | — |
| `2f930279` | Ležící/omráčený nikdy nechytá (stráž přímo v `resolveCatch`); předání zmizelému příjemci se nekoná (bez turnoveru), ležícímu se koná a míč odskočí | `engine/src/ball_handler.cpp:32-55` (`resolveCatch` BEZ kontroly stavu), `pass_handler.cpp:419-423` (hand-off: jen sousednost), `rules_engine.cpp:212-217` (nabídka jen STOJÍCÍMU) | **CHYBÍ (drobné)** — všichni ostatní volající v C++ stav hlídají sami (odskok, přihrávka, vhazování, výkop). Hand-off ne: když příjemce mezi nabídkou a provedením ulehne (v C++ např. Blood Lust kousne Thralla-příjemce při ohlášení předání), hodí se mu chytání jako stojícímu. Zmizelý příjemce ⇒ `fail()` bez turnoveru — SHODNÉ. | nízký |
| `a77941d8` | Hand-off: turnover rozhoduje stav míče PO odskoku (spoluhráč ho může chytit); TTM do davu je turnover jen s míčem | `engine/src/pass_handler.cpp:444-456` (hand-off), `ttm_handler.cpp:112-118` (`crowd()`) | **SHODNÉ** | — |
| `8460a2ff` | Neúspěšný gaze NENÍ turnover; oběť se nepočítá do modifikátoru zón | `engine/src/gaze_handler.cpp:28` (oběť vynechána), `:43-45` (`ok()`) | **SHODNÉ** — PHP je port z C++. | — |
| `80852863` | Wild Animal: D6 +2 při Block/Blitz, 1-3 selže (žádný auto-pass); Blitz mimo dosah nehází výjimku, hráč dojde co nejblíž | `engine/src/big_guy_handler.cpp:87-108` (od 07.08.); blitz: `rules_engine.cpp:122-167` | **SHODNÉ** (Wild Animal — PHP je port z C++). Blitz mimo dosah: výjimka byla vada jen PHP; C++ blitz mimo dosah vůbec NENABÍZÍ (`canReachAdjacentTo`), takže tah „ohlásit Blitz kvůli +2 pro Wild Animal a jen se pohnout“ v C++ neexistuje — rozdíl v nabídce, ne v pravidle. Wild Animal není v měřených sestavách. | — |
| `496f5a03` (C++) | Pathfinder (cena cesty AI): dodge se hází při OPUŠTĚNÍ pole v tacklezóně, ne při vstupu | PHP resolver `src/Engine/Action/MoveHandler.php:223-260` (`requiresDodge` z kroku), PHP `src/Engine/Pathfinder.php:95-107` (`$leavingTz` = zóny na OPOUŠTĚNÉM poli) | **SHODNÉ** — PHP resolver i PHP pathfinder hradí odchodem. | — |
| `82673bb8` | Měření příležitostí pro Leap (Wardancer) — bez změny pravidel | — | **NEMÁ SMYSL** — commit nemění chování enginu. Při kontrole ale nalezena mezera v PHP, viz N4. | — |

## C++ commity od 24.08. bez slova PRAVIDLA (stručně)

`git log --since=2026-08-24 -- engine/src/` = 102 commitů. Většina je AI/plánovač/měřidla (KLEC/K*, B-ROUND, W-GFI,
W-CIL, W-DOSAH, Q3, P9c, M14b, P35, M12, B2, T5.3x, `greedyPolicy`, `movePlayerToward` BFS, Leap rameno, měřidla,
merge) — ty chování PRAVIDEL nemění a v PHP protějšek nemají (**NEMÁ SMYSL**). Pravidlové zbývají tyto
(TA3/TA4/TA5/TA7/TA10 z 24.08. jsou pokryté výš u `ced20b99`/`d9910a96`/`60ba5961`/`0003267e`/`9e980cac`):

| C++ commit | co se měnilo (1 věta) | PHP: soubor:řádek | verdikt | dopad na měření |
|---|---|---|---|---|
| `771d1ecc` (+`14c172cd`, `dc4afb68`, `a280af3c`, `36d81dc4`) | M1/N10: follow-up je VOLBA; po bloku v Blitzu hráč pokračuje v pohybu (r. 551-552, 608-611) | `src/Engine/Action/BlitzHandler.php:170-190` (po bloku konec), `src/Engine/Action/BlockHandler.php:905-913` (follow-up VŽDY, bez volby) | **CHYBÍ** — E28 potvrzeno (pohyb po bloku v Blitzu PHP nemá). NAVÍC: follow-up je v PHP automatický, ne volba. | — (měří se v C++; pro PHP hry vysoký) |
| `366fda3e` | P37b: blok v Blitzu stojí 1 pole pohybu (r. 549-550), bez něj se blitz nenabídne | `src/Engine/Action/BlitzHandler.php:74-168` (dojde na sousední pole libovolnou cestou vč. 2 GFI a blok je zdarma) | **CHYBÍ** — PHP za blok v Blitzu pole pohybu nestrhává (ani GFI na ránu). | — (C++ OK) |
| `6e2f084c` (N11) + `092d1ac0` (M4/N15) | Zakořeněný nesmí GFI ani se pohnout (ani v Blitzu); nabídky počítají Sprint | `src/Engine/Pathfinder.php:38-52` (`$maxRange = $ma + GFI` bez ohledu na `rooted`), `BlockHandler.php:905` (zakořeněný útočník follow-upuje) | **CHYBÍ** — v kole, kdy už je hráč zakořeněný z dřívějška (`BigGuyCheckResolver.php:76` pak nehází), má PHP `movementRemaining=0`, ale pathfinder mu dá 2 GFI pole ⇒ zakořeněný Treeman v PHP chodí. Follow-up zakořeněného útočníka (r. 8581-8582 „without following-up“) PHP nezakazuje. Sprint v pathfinderu SHODNÉ. | — (C++ OK; Treeman je ve Wood Elf TV1200 ⇒ PHP hry s elfy ano) |
| `018b230e` (N6, N7) | Leap do GFI pásma = 2 GFI hody; Tentacles/Shadowing platí i na Leap | `src/Engine/Action/MoveHandler.php:139-219` (skok: žádné GFI, žádné Tentacles/Shadowing, `continue` přeskočí vše) | **CHYBÍ** — viz i N4 (modifikátor a přehoz u Leap). | — (C++ OK) |
| `f17802d1` (TA8, TA9) | Tentacles = 2D6 + ST(utíkající) − ST(chapadla), chycen na ≤5; Shadowing = 2D6 + MA(utíkající) − MA(stín), následuje na ≤7 | `src/Engine/Action/MoveHandler.php:228-230` (D6+ST vs D6+ST), `:405-408` (1× D6 + MA stínu − MA utíkajícího ≥ 6) | **CHYBÍ** — PHP má obě staré mechaniky. | — (skilly nejsou v měřených sestavách) |
| `38e4fd85` + `061cf4a3` (M13) | Ležící smí ohlásit MOVE i BLITZ (r. 669-676) | `src/Engine/RulesEngine.php:185-187` (Blitz jen `canAct()` = STOJÍCÍ, `PlayerState.php:25-28`) | **CHYBÍ (Blitz)** — ležící v PHP Blitz nedostane. MOVE z lehu PHP umí (`Pathfinder.php:40-48`). | — (C++ OK) |
| `5a01591f` (N8) | Both Down neodtlačuje a nefollow-upuje | `src/Engine/Action/BlockHandler.php:804-857` | **SHODNÉ** | — |
| `ff5d2253` | Skill Dodge nedává modifikátor, jen přehoz | `src/Engine/TacklezoneCalculator.php:183-194` | **SHODNÉ** (PHP 14.09.) | — |
| `12e36c0e` + `3cbcf540` (M2) | Promarněná akce (Bone-head, Take Root na Blitz…) týmu akci stejně spotřebuje | `src/Engine/ActionResolver.php:240-243`, `BigGuyCheckResolver.php:125,166,224,297` (`wastesTeamAction`) | **SHODNÉ** | — |
| `ab31a1fb` + `cf59e383` (M3, M3b) | Stav Bone-head/Really Stupid trvá do úspěšného hodu (ne do konce kola); omámený spoluhráč Really Stupid podpírá | `src/DTO/GameState.php:288-306` (`bigGuyStupefied`), `BigGuyCheckResolver.php:429` | **SHODNÉ** | — |
| `d7756ee5` (L6) | Multiple Block + Frenzy: zakázat kombinaci, ne volbu | `src/Engine/RulesEngine.php:168` (MB se nabízí i s Frenzy), MB cesta Frenzy nevolá | **SHODNÉ** | — |
| `131a1779` (L2) | Disturbing Presence i na intercepci | viz `534c9e40` | **SHODNÉ** | — |
| `8776ad39` (F12) | Leap se nabízí jako akce | `src/Engine/Pathfinder.php:170-215` | **SHODNÉ** (nabídka); mechanika viz N4 a `018b230e` | — |
| `7d135f4e` | Každý měřený tým dostal 2 Linemany s Wrestle | — | **NEMÁ SMYSL** (měřicí sestavy C++) | — |
| `862e1454`, `81f2c29f`, `f26b6287`, `12b7137d`, `0630f854`, `f3382513` a další AI | vstání neukončí aktivaci v makru, „vstát a odskočit“, dosah blitzu na nosiče, … | — | **NEMÁ SMYSL** (makro/AI vrstva C++) | — |

## Poznámky k jednotlivým commitům

**N1 — lékárník (mimo tabulku, našlo se u `966a871c`).** C++ `engine/src/injury.cpp:87-140` používá lékárníka na
CASUALTY tabulku (přehoz D68, vybere mírnější; BH ⇒ rezervy) a jen na BH/DEAD u nenahraditelného hráče.
PHP `InjuryResolver` (lékárníkův přehoz, r. ~217) přehazuje HOD NA ZRANĚNÍ. Dva různé modely téhož pravidla —
stojí za samostatnou kontrolu proti `rules_bb2016.txt` (Apothecary). Neověřováno do hloubky.

**N2 — počasí se v C++ hází při KAŽDÉM výkopu (mimo tabulku, našlo se u `d674d080`).**
`engine/src/kickoff_handler.cpp:278-281`: „Roll weather (if not changed by CHANGING_WEATHER)“ — nové počasí
2D6 před každým drivem. PHP hází počasí jednou na začátku zápasu (`src/Service/MatchService.php:185`) a mění ho
jen výsledek Changing Weather (`KickoffResolver.php:296-305`). Průměrný podíl drivů v daném počasí vyjde podobně,
ale v C++ je každý drive nezávislý (žádné „deštivé odpoledne“) a Changing Weather je tam prakticky bez vlivu.
Dopad na měření: nízký–střední (všech 5 sestav, všechny hry). Krátký test: 1 zápas se seedem, vypsat
`state.weather` po každém výkopu.

**N3 — Stunty na hodu na zranění, OBA enginy (mimo tabulku, našlo se u `93043345`).** `rules_bb2016.txt`
r. 8534-8536: Stunty „treats a roll of 7 and 9 on the Injury table after any modifiers … as a KO'd and Badly
Hurt result“. C++ `engine/src/injury.cpp:45-48` i PHP `src/Engine/InjuryResolver.php:337-338` místo toho dávají
+1 k hodu na zranění. Parita drží, pravidlo ne. Stunty v měřených sestavách není ⇒ nízký dopad. Patří do
knihy jako pravidlová vada obou enginů, ne jako PORT.

**N4 — Leap v PHP (mimo tabulku, našlo se u `82673bb8`).** `rules_bb2016.txt` r. 8276-8277: „No modifiers apply
to this D6 roll unless he has Very Long Legs.“ C++ `engine/src/helpers.cpp:78-82` (`7−AG`, jen VLL) a
`move_handler.cpp:297-298` (přehoz Pro/týmový přes `attemptRoll`, ne Dodge) — v pořádku. PHP
`src/Engine/Pathfinder.php:176-177` počítá cíl skoku `7−AG+zóny na cíli` a `MoveHandler.php:139-150` ho použije
⇒ PHP přidává modifikátor za zóny, který pravidlo nezná; navíc při neúspěchu PHP nenabízí žádný přehoz (ani týmový,
ani Pro). Směr je PORT do PHP. Dopad na měření (C++) žádný; Wardancer je ve Wood Elf TV1200, takže PHP hry s elfy
to zasáhne.

## Pořadí CHYBÍ podle dopadu na výsledky her v C++ (tam se měří)

1. `a7603e8f` — C++ `resolveCatch` bere týmový přehoz VŽDY (výkop, soupeřovo kolo, soupeř chytá naši přihrávku). **střední**
2. `d0c6e1d7` — C++ faul: obranná asistence nikdy nevznikne (`tzExcludeId=-1`). **střední**
3. N2 — C++ hází počasí při každém výkopu (mimo tabulku, ale všechny hry). **nízký–střední**
4. `cd2f72fe` — C++ nepřesná přihrávka: 3 rozptyly i přes aut; vhazování od CÍLE místo od posledního pole. **nízký–střední**
5. `3cec72e2` — C++ E19/E20b: sražený zakořeněný Treeman a ležící Stand Firm drží pole v řetězu. **nízký–střední** (Wood Elf)
6. `d674d080` — C++ výkop 7/8 prohozené. **nízký** (všechny hry, malý efekt)
7. `84f7dc46` — C++ Pro: příznak se nenuluje soupeři; u přihrávky jde týmový přehoz po neúspěšném Pro na hod přihrávky. **nízký**
8. Zbytek — skilly mimo měřené sestavy, dnes v C++ korpusu nesepnou: `9e980cac` Blood Lust · `04e4946c` + `e373bf23` Ball & Chain ·
   `d9910a96` Bombardier · `ced20b99` Always Hungry · `78bf7601` + `9e02a834` + `fa2422ad` Chainsaw · `01bdfd77` + `1657a19e` +
   `ec3feb2c` TTM · `6a6537ef` Dirty Player/Break Tackle · `e81bb57d` + `faef31f3` Safe Throw · `60ba5961` Gaze (E22) ·
   `2f930279` hand-off na ležícího. **nízký**

Na měření v C++ NEPŮSOBÍ (směr PORT do PHP): `771d1ecc` (E28 + follow-up jako volba), `366fda3e`, `6e2f084c`, `018b230e`,
`f17802d1`, `38e4fd85`, N4. Pro PHP hry proti člověku jsou ale E28, follow-up a zakořeněný Treeman (`6e2f084c`) nejviditelnější.

## Navržené řádky do knihy (zapíše hlavní sezení, `evidence/task_queue.md` jsem needitoval)

PORT do C++ (seřazeno podle dopadu na měření):

| **P??** | PORT do C++: týmový přehoz při chytání jen hráči týmu NA TAHU a ne u míče z výkopu (`resolveCatch`/`attemptRoll` dostane `canUseTeamReroll` podle kola; r. 929-933, 1263) | commit a7603e8f | OTEVŘENO |
| **P??** | PORT do C++: obranná asistence u faulu — výjimka „sám faulující“ ze zón (`foul_handler.cpp:70` `tzExcludeId=fouler.id`; r. 1843-1850) | commit d0c6e1d7 | OTEVŘENO |
| **P??** | PORT do C++: počasí jen na začátku zápasu + Changing Weather, ne při každém výkopu (`kickoff_handler.cpp:278-281`; N2) | commit d674d080 (nález N2) | OTEVŘENO |
| **P??** | PORT do C++: nepřesná přihrávka / Hail Mary — rozptyl se po opuštění hřiště zastaví, vhazování od posledního pole na hřišti (`pass_handler.cpp:161-170, 210-214, 374-378`) | commit cd2f72fe | OTEVŘENO |
| **P??** | PORT do C++: `holdsGround` — Stand Firm jen STOJÍCÍ; `rooted=false` hned při sražení/položení, ne až na začátku kola (E19, E20b) | commit 3cec72e2 | OTEVŘENO |
| **P??** | PORT do C++: tabulka výkopu 7 = Changing Weather, 8 = Brilliant Coaching (`enums.h:252-253`) | commit d674d080 | OTEVŘENO |
| **P??** | PORT do C++: Pro — nulovat `proUsedThisTurn` oběma týmům každé kolo; přihrávka: po neúspěšném Pro týmový přehoz jen na hod Pro (`pass_handler.cpp:329-347`) | commit 84f7dc46 | OTEVŘENO |
| **P??** | PORT do C++: Blood Lust — hladový upír akci dokončí, krmí se na konci (před přihrávkou/předáním/TD), Block→Move | commit 9e980cac | OTEVŘENO |
| **P??** | PORT do C++: Ball & Chain (TA6) — šablona vhazování, dav, zranění bez brnění, turnover, blok podle síly, ležící odtlačit + brnění, follow-up | commit e373bf23, 04e4946c | OTEVŘENO |
| **P??** | PORT do C++: Bombardier — fumble při modifikovaném ≤1; zasažený omráčený zůstává omráčený | commit d9910a96 | OTEVŘENO |
| **P??** | PORT do C++: Always Hungry — sežraný nosič míče = turnover | commit ced20b99 | OTEVŘENO |
| **P??** | PORT do C++: Chainsaw — nenabízet Multiple Block, faul pilou (zpětný ráz + +3), +3 proti sraženému nositeli pily | commit fa2422ad, 78bf7601, 9e02a834 | OTEVŘENO |
| **P??** | PORT do C++: TTM — fumble při modifikovaném ≤1; rozptyl se zastaví mimo hřiště; hozený nosič v davu = vhazování od posledního pole | commit 01bdfd77, 1657a19e, ec3feb2c | OTEVŘENO |
| **P??** | PORT do C++: Dirty Player jako volba brnění/zranění; Break Tackle 1× za kolo | commit 6a6537ef | OTEVŘENO |
| **P??** | PORT do C++: Safe Throw — nemodifikovaný AG hod házeče proti intercepci; při fumblu jinak než přirozenou 1 míč zůstává | commit faef31f3, e81bb57d | OTEVŘENO |
| **P??** | PORT do C++: Hypnotic Gaze — zóny se vrací začátkem AKCE oběti, ne začátkem kola | commit 60ba5961 | OTEVŘENO |
| **P??** | PORT do C++: `resolveCatch` — ležící/omráčený nechytá (stráž přímo ve funkci, hand-off) | commit 2f930279 | OTEVŘENO |

PORT do PHP (C++ měření neovlivní):

| **P??** | PORT do PHP: follow-up jako VOLBA + pohyb po bloku v Blitzu (E28) | commit 771d1ecc | OTEVŘENO |
| **P??** | PORT do PHP: blok v Blitzu stojí 1 pole pohybu (případně GFI), r. 549-550 | commit 366fda3e | OTEVŘENO |
| **P??** | PORT do PHP: zakořeněný nesmí GFI ani follow-up (pathfinder `$maxRange` bez GFI při `rooted`; r. 8577-8582) | commit 6e2f084c | OTEVŘENO |
| **P??** | PORT do PHP: Leap — cíl bez modifikátorů (jen VLL), přehoz Pro/týmový, GFI za skok do deficitu, Tentacles/Shadowing i na Leap | commit 018b230e (nález N4) | OTEVŘENO |
| **P??** | PORT do PHP: Tentacles 2D6+ST rozdíl ≤5, Shadowing 2D6+MA rozdíl ≤7 | commit f17802d1 | OTEVŘENO |
| **P??** | PORT do PHP: ležící smí ohlásit Blitz (r. 676) | commit 38e4fd85 | OTEVŘENO |

Pravidlo v obou enginech (ne PORT):

| **P??** | PRAVIDLA (PHP i C++): Stunty na zranění — modifikovaná 7 = KO a 9 = Badly Hurt místo +1 k hodu (r. 8534-8536) | nález N3 | OTEVŘENO |
| **P??** | KONTROLA (PHP × C++): lékárník — C++ přehazuje CASUALTY tabulku, PHP hod na zranění; ověřit proti BB2016 a sjednotit | nález N1 | OTEVŘENO |

HOTOVO

