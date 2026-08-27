# FABLE 27.08.2026 — AUDIT NABÍDEK MAKER PROTI DOSAHU

**Otázka:** BLITZ_AND_SCORE (`+3`) a LEAP (z TZ) měly dnes týž tvar vady —
makro se nabídne do stavu, ve kterém nemůže splnit, co slibuje. Kolikrát se
ten tvar opakuje u ostatních maker?

**Odpověď napřed:** opakuje se u **osmi ze čtrnácti** typů maker, a u tří
z nich je to většina nabídek, ne okraj. Největší tři:

(1) **ADVANCE** — brána nabízí „jdi vpřed" v 18 135 kolech; expanze
    **v 54,9 % z nich nosičem nepohne na zvolené pole**: v 35,0 % se vzdá
    ještě před chůzí (záložní smyčka stáhne kroky na nulu), v dalších 19,9 %
    chůze selže po cestě. V 34,0 % nabídek bylo přitom volné pole vpřed hned
    vedle přímky.
(2) **REPOSITION** — 247 919 nabídek bez jakékoli kontroly dosahu; na cíl
    dojde **27,5 %**. 37,9 % má za cíl OBSAZENÉ pole (dojít nelze z definice),
    23,8 % má cíl dál než MA, 7,2 % je v dosahu a chůze přesto selže.
    Speciálně obranný „marker na nosiče" skončí vedle nosiče v **0,8 %**
    případů, kdy na to má pohyb.
(3) **BLITZ** — brána vybírá cíl podle skóre BEZ ohledu na dosah; v 6,4 %
    z 69 000 nabídek k cíli **nikdo nedosáhne**, takže BLITZ akce neexistuje a
    expanze vrátí prázdno. V 7,2 % kol s nabídkou je top-1 cíl nedosažitelný,
    ačkoli JINÝ soupeř dosažitelný je.

Dále: PICKUP nedojde na míč v 15,1 % (1 880 z 12 448), CAGE nepohne nikým
v 17,2 % (3 271 z 19 037), HAND_OFF_SCORE 27,7 % (81 z 292), CHAIN_SCORE
26,6 % (58 z 218), PASS_SCORE 9,8 % (46 z 469), SCORE 9,2 % (83 z 903).
Konzistentní jsou BLOCK, FOUL, PASS_ACTION, END_TURN a vstávání; u BLOCK
je rozpor jiného druhu (kostka Dauntless, ne dosah). BLITZ_AND_SCORE po
dnešní opravě: zbytkový rozpor 1,5 % + 7,1 % nejistých.

⚠️ **Důsledek prázdné expanze je tvrdý:** když vyhledávání takové makro
vybere, `MacroMCTSPolicy` z prázdného plánu udělá **END_TURN** a zahodí zbytek
kola celého týmu (`engine/src/macro_mcts.cpp:1180-1183`). V korpusu je
**1 189 našich kol (2,5 %) bez jediné naší akce**, z toho 581 s míčem v ruce
a nabídnutým ADVANCE. Zda je to právě tenhle mechanismus, ze snímku
nepoznám (viz § 7).

---

## 1. Metoda a její meze

- Korpus `blitzlanding_replic_20260825_corpus_data`: **3 000 her, 48 000
  našich (trpasličích) kol**, snímek ZAČÁTKU kola. Skript
  `diag_offer_reach_audit_20260827.py`, surový výstup
  `evidence/fable_offer_reach_audit_20260827.raw.txt`. Běh 188 s na celý
  korpus (0,06 s/hra), rozpočtová brzda tedy nezasáhla.
- **Nabídnuto** = rekonstrukce brány `analyze_turn_offers` importovaná
  z `diag_fable_offered_played_20260817.py` (dnes srovnaná s enginem, včetně
  T5.35a). Identity kandidátů (kdo je příjemce, který cíl blitzu, kam
  REPOSITION) jsem k tomu dopočítal stejnou logikou; **kontrola: v 0 kolech
  se počet mých kandidátů liší od počtu z importované brány.**
- **Co expanze ujde** = emulace `movePlayerToward` + `findMoveToward` +
  `scoreMoveAction` (`engine/src/macro_actions.cpp:42-178, 1369-1411`):
  stejné skóre kroku (10/pole, +20/+12 za TZ v cíli kroku, +8 GFI, +6
  sideline, manhattan tiebreak), stejné pořadí sousedů
  (`engine/src/position.cpp:5-16`), stejná pravidla „max 1 pole zajížďky"
  (`:1393`) a „smyčka" (`:1394`), stejný rozpočet MOVE akcí MA + 2 GFI
  (`engine/src/rules_engine.cpp:36-37`). Vedle toho BFS přes neobsazená pole
  (přepis `engine/src/pathfinder.cpp:71-102`) jako „ideální cesta": říká,
  jestli cíl VŮBEC JDE dojít v rozpočtu.
- ⛔ **Kostky se neházejí.** Dodge, GFI, hand-off, pass, blok — vše počítáno
  jako úspěšné. „Nedojde" tu znamená **nedojde, ani když padne všechno**.
  Skutečnost je proto horší, ne lepší.
- Snímek je začátek kola: MA plná, nic nepoužito. Makra vzniklá až během
  kola nevidím ⇒ všechna N jsou **podlaha**. V dw-dw se čte jen `home`.
- Ramena: korpus 25.08. sebrán s **mode 8 (P35 blitz landing) zapnutým**
  (`run_night_ab.sh:306-308`); to mění volbu blitzera, ne dosah cíle. P38
  (cage-aware ADVANCE) vypnuto, emulace ADVANCE tedy odpovídá baseline větvi.
- `std::sort` u BLITZ kandidátů (`macro_actions.cpp:801-804`) **není
  stabilní**: v 43,6 % nabídek je na hraně výběru remíza skóre a engine mohl
  emitovat jiný cíl než já. Proto u BLITZ uvádím i číslo na výběru nezávislé.

---

## 2. Makra, kde se brána a expanze ROZCHÁZEJÍ

### 2.1 ADVANCE — nejčastější a nejtěžší

(a) Brána `macro_actions.cpp:710-717`: nosič může jednat, MA > 0,
    `dist > MA + 2`. **Žádná kontrola, že je kam jít.**
(b) Expanze `expandAdvance` `:1585-1667`: kroky ze
    `carrierStallAwareSteps` (`:1498-1518`), cíl = `x + dx·steps`, y
    přitažené ke středu; záložní smyčka `:1656-1663` stahuje `steps`, dokud
    je cílové pole obsazené nebo v TZ — **mění jen x, y nechává**; při
    `steps <= 0` vrátí prázdno (`:1663`). Jinak chůze `steps + 2` kroků
    (`:1665`).
(c) Rozchází se: brána nezná obsazenost přímky ani cestu k cíli.
(d) N = 18 135 nabídek:

| výsledek | N | % |
|---|---|---|
| dojde na zvolené pole | 8 178 | 45,1 |
| **VZDÁ SE** (smyčka stáhla `steps` na 0, prázdná expanze) | 6 342 | 35,0 |
| …z toho bylo volné TZ-free pole vpřed **mimo přímku** | 6 167 | 34,0 (97,2 % vzdání) |
| chůze selže — smyčka (`:1394`, žádný krok neubírá vzdálenost) | 2 807 | 15,5 |
| chůze selže — vyčerpá `steps + 2` kroků | 507 | 2,8 |
| chůze selže — žádný MOVE k dispozici | 301 | 1,7 |

Replikuje nález 26.08. (`diag_c_fallback_20260826.py`, 400 her: vzdá se
v 21 % kol s nosičem, tam se ale bralo `steps = MA`, tedy víc pokusů).
S přesnými kroky ze stall-throttle je to **35 % nabídek**. Těch 15,5 %
„smyčka" je nový údaj: cílové pole bylo volné a TZ-free, ale první krok
k němu nešel udělat (typicky vlastní rohy klece na diagonálách,
srov. `project_bloodbowl_own_wall_blocks_carrier_20260826`).

### 2.2 REPOSITION — bez kontroly dosahu, z definice

(a) Brána `:1126-1350`: každý volný stojící hráč bez sousedícího soupeře
    dostane cíl podle strategie; **kontrola dosahu není žádná** (přiznáno
    v komentáři expanze `:1943-1949`).
(b) Expanze `expandReposition` `:1940-1973`: chůze `maxSteps = MA`
    (bez GFI, `:1956-1957`).
(c) Rozchází se, a to trojím způsobem: cíl je obsazené pole (dojít nelze
    nikdy), cíl je dál než MA, cíl je v dosahu a chůze přesto selže.
(d) N = 247 919 nabídek (5,17 na kolo):

| výsledek | N | % |
|---|---|---|
| dojde | 68 084 | 27,5 |
| cíl = vlastní pole (stůj) | 8 975 | 3,6 |
| cíl je **OBSAZENÉ** pole (jen přiblížení) | 93 922 | 37,9 |
| cíl **dál než MA** | 59 067 | 23,8 |
| v dosahu MA, chůze **selže** (smyčka 13 595 / vyčerpání 4 258 / žádný MOVE 8) | 17 861 | 7,2 |
| cíl mimo hřiště (`:1238`, `x + 2·dx` bez clampu) | 10 | 0,0 |

Po strategiích (N nabídek → dojde / obsazený cíl / dál než MA / selže):

| strategie (řádek brány) | N | dojde | obsazený cíl | > MA | selže v dosahu |
|---|---|---|---|---|---|
| útok: 2 pole před nosiče (`:1236-1239`) | 62 401 | 12 902 (20,7 %) | 35 778 (57,3 %) | 1 236 | 8 189 (13,1 %) |
| míč na zemi: k volnému poli vedle míče (`:1188-1201`) | 44 884 | 15 930 (35,5 %) | 0 | 25 174 (56,1 %) | 3 780 (8,4 %) |
| obrana: screen (`:1331-1339`) | 43 996 | 23 890 (54,3 %) | 6 961 | 10 212 (23,2 %) | 1 470 |
| útok: k nosiči, cíl = nosičovo pole (`:1240-1243`) | 27 996 | — | 27 996 (100 %) | — | — |
| obrana: intercept lane (`:1290-1310`) | 19 910 | 11 042 (55,5 %) | 3 505 | 3 618 | 608 |
| obrana: marker, cíl = pole soupeřova nosiče (`:1319-1322`) | 19 401 | — | 19 401 (100 %) | — | — |
| obrana: roh soupeřovy klece (`:1251-1278`) | 13 860 | 1 864 (13,4 %) | 0 | 8 319 (60,0 %) | 3 677 (26,5 %) |
| obrana: safety (`:1313-1317`) | 8 956 | 1 258 (14,0 %) | 99 | 7 363 (82,2 %) | 11 |
| obrana: endzone guard (`:1324-1330`) | 3 811 | 1 145 (30,0 %) | 170 | 2 352 (61,7 %) | 119 |
| útok: receiver u endzóny (`:1228-1235`) | 865 | 53 (6,1 %) | 12 | 793 (91,7 %) | 7 |

⭐ **Obsazený cíl není neutrální „přiblížení".** Chůze na obsazené pole nikdy
nekončí podmínkou `p.position == target` (`:1380`), takže po dosažení
sousedství **přešlapuje dál**, dokud nenarazí na smyčku nebo limit kroků; a
protože vstup do TZ ze čtverce bez TZ stojí +20 bodů proti 10 za pole
vzdálenosti (`:98-99`), **raději zůstane o pole dál**. Změřeno u cílů
v dosahu MA:

| strategie s obsazeným cílem | skončí VEDLE cíle | skončí DÁL (d ≥ 2) |
|---|---|---|
| obrana: **marker na nosiče** | **71** (0,8 %) | **8 316** (d=2: 4 314, d=3: 2 926, d=4: 886, d=5: 184, d=6: 6) |
| útok: 2 pole před nosiče | 24 824 (70,9 %) | 10 200 |
| útok: k nosiči | 8 521 (67,0 %) | 4 316 |
| obrana: screen | 4 729 (85,9 %) | 774 |
| obrana: intercept lane | 2 507 (90,6 %) | 261 |

⇒ Makro, jehož smysl je **postavit se soupeřovu nosiči do TZ**, to udělá
v 0,8 % případů, kdy na to má pohyb; nosičovo pole je vždy obsazené, sousední
pole jsou vždy v jeho TZ, a chůze je vyhodnotí jako „špatný krok".
Totéž u „rohu soupeřovy klece": cílový čtverec je od TZ osvobozen (`:85-88`),
ale předposlední pole ne — proto 26,5 % selhání v dosahu.

### 2.3 BLITZ — cíl se vybírá bez dosahu

(a) Brána `:732-812`: pro každého stojícího soupeře nejlepší skóre přes
    všechny volné blitzery (`isFreeToAct`, `:752`; kostky `:755`), top-1
    (útok) / top-2 (obrana) podle skóre. **Dosah blitzera se nezkoumá.**
(b) Expanze `expandBlitz` `:1707-1754`: hledá BLITZ akci na `macro.targetId`
    v `getAvailableActions`; není-li, vrátí prázdno (`:1747`). BLITZ akce
    vzniká jen pro cíl, k němuž blitzer sousedí nebo dojde BFS s rezervou
    1 MP na blok (`rules_engine.cpp:87-109`, `pathfinder.cpp:20-109`).
(c) Rozchází se: skóre blitzera stojícího 15 polí od cíle je stejné jako
    skóre souseda.
(d) N = 69 000 emitovaných cílů v 47 990 kolech:

| | N | % |
|---|---|---|
| cíl dosažitelný aspoň jedním blitzerem (BFS, MA + 2 GFI − 1) | 64 611 | 93,6 |
| **NEJDE: nikdo nedosáhne** ⇒ žádná BLITZ akce, prázdná expanze | 4 389 | 6,4 |
| …z toho cíl dál než 8 polí od kohokoli (ani MA6 + 2 GFI) | 2 255 | 3,3 |
| remíza skóre na hraně výběru (cíl v emulaci nejistý) | 30 058 | 43,6 |
| kol, kde top-1 cíl je nedosažitelný, ač JINÝ soupeř dosažitelný je | 3 473 / 47 990 | 7,2 |
| kol, kde nikdo nedosáhne na nikoho (na výběru nezávislé) | 3 / 47 990 | 0,0 |

⇒ Nula u „nikdo na nikoho" znamená, že **skoro vždy existoval dosažitelný
cíl** — vada je ve VÝBĚRU cíle (skóre bez dosahu), ne v nedostatku možností.
Číslo 6,4 % je zatíženo nestabilním řazením; 7,2 % kol je horní hranice
toho, co by stabilní řazení mohlo posunout.

### 2.4 PICKUP — dosah geometrický, cesta ne

(a) Brána `:912-993`: Chebyshev vzdálenost k míči `<= MA + 2` (`:936-937`).
(b) Expanze `expandPickup` `:1871-1901`: chůze na pole míče, `maxSteps =
    MA + 2` (`:1880`).
(c) Rozchází se: vzdálenost ignoruje těla kolem míče a zajížďky.
(d) N = 12 448 nabídek: dojde 10 568 (84,9 %); **nedojde 1 880 (15,1 %)**:
    ideální cesta se nevejde 133 (1,1 %), greedy chůze selže smyčkou
    1 477 (11,9 %) nebo vyčerpáním 270 (2,2 %), ač BFS by došel.
    ⇒ 93 % selhání je slepota chůze, ne geometrie.

### 2.5 CAGE — bez dosahu, s tvrdým limitem 4 kroků

(a) Brána `:719-730`: máme míč a existuje aspoň jeden volný spoluhráč.
(b) Expanze `expandCage` `:1669-1705`: pro každý volný diagonální roh
    nejbližší volný hráč (Chebyshev, `findNearestFreePlayer` `:468-483`, bez
    kontroly dosahu) a chůze **max 4 kroky** (`:1701`).
(c) Rozchází se: „volný spoluhráč" ≠ „spoluhráč, který dojde na roh".
(d) N = 19 037 nabídek; rohů k obsazení 36 829:

| | N | % |
|---|---|---|
| aspoň jeden roh obsazen | 14 239 | 74,8 |
| klec už hotová (4 naše rohy) ⇒ expanze nic nedělá, neškodí | 1 527 | 8,0 |
| **žádný volný roh, klec NENÍ hotová** (rohy drží soupeř / ležící / sideline; expanze roh přeskočí `:1687-1694`) | 2 521 | 13,2 |
| **nikdo nedojde na žádný volný roh** | 750 | 3,9 |
| rohů dojito / nejbližší > 4 pole / chůze selže do 4 kroků | 30 581 / 3 647 / 2 590 | 83,0 / 9,9 / 7,0 |

⇒ Prázdná expanze v 3 271 nabídkách (17,2 %) při nedokončené kleci.

### 2.6 HAND_OFF_SCORE — dva přechody, ani jeden hlídaný

(a) Brána `:592-617`: nosič „stuck" (`:598`), spoluhráč do 2 polí
    (`:608`), příjemce `dist <= MA + 2` k endzóně (`:611-612`).
(b) Expanze `:1983-2030`: nosič dojde k příjemci (chůze `maxSteps = MA`,
    `:1999-2000`), HAND_OFF akce musí existovat (`:2010-2018` ⇒ soused),
    příjemce chůze 14 kroků do endzóny (`:2028`).
(c) Rozchází se: „do 2 polí" neznamená „dojde do sousedství" (nosič je
    z definice v ≥ 2 TZ nebo daleko), „MA + 2" příjemce ignoruje cestu.
(d) N = 292: dojde 211 (72,3 %); **nedojde 81 (27,7 %)**: nosič neskončí
    vedle příjemce 44 (15,1 %; smyčka 33, žádný MOVE 7, vyčerpání 4),
    příjemce nedojde 37 (12,7 %; greedy 28, ani BFS 9).
    ⭐ Vedlejší nález: v 114 nabídkách (39,0 %) nosič po dosažení
    sousedství **přešlapuje dál** (viz § 2.2, obsazený cíl), v 88 (30,1 %)
    přitom **opouští TZ** = dodge hody navíc, které brána nikde neocenila.

### 2.7 CHAIN_SCORE — tři přechody

(a) Brána `:655-708`: pass 1–10 Chebyshev (`:676`), relay do 2 polí od
    střelce (`:684`), střelec `MA + 2` (`:687-688`).
(b) Expanze `:2065-2120`: PASS akce (`:2077-2085`), relay dojde k střelci
    (`:2096-2098`), HAND_OFF (`:2102-2110`), střelec 14 kroků (`:2118`).
(c) Rozchází se jako 2.6 plus pravítko (viz 2.8).
(d) N = 218: dojde 160 (73,4 %); **nedojde 58 (26,6 %)**: relay neskončí
    vedle střelce 35 (16,1 %), střelec nedojde 22 (10,1 %; ani BFS 3),
    blizzard zakáže long pass 1.

### 2.8 PASS_SCORE — Chebyshev proti pravítku

(a) Brána `:619-653`: `passDist` 2–10 **Chebyshev** (`:636`), příjemce
    `MA + 2` (`:640`).
(b) Expanze `:2032-2063`: PASS akce musí existovat (`:2044-2052`); ta vzniká
    jen z pravítka `passRangeFromOffset` (`rules_engine.cpp:117-125`) a
    v blizzardu ne pro long pass / bomb (`:126-130`).
(c) Rozchází se: Chebyshev 10 diagonálně je mimo pravítko (GRID v
    `diag_fable_offered_played_20260817.py:87-101`); PASS_ACTION (`:1052-1061`)
    obě kontroly má, PASS_SCORE ani jednu.
(d) N = 469: dojde 423 (90,2 %); **žádná PASS akce 6 (1,3 %)** (pravítko 4,
    blizzard 2); příjemce nedojde 40 (8,5 %; ani BFS 2).

### 2.9 SCORE — nejtěsnější, ale ne nulový

(a) Brána `:583-590`: `0 < dist <= MA + 2`.
(b) Expanze `expandScore` `:1413-1476`: y s nejméně TZ (`:1441-1470`), chůze
    14 kroků (`:1474`).
(c) Rozchází se: vzdálenost v x ignoruje obsazená pole a zajížďky.
(d) N = 903: dojde 820 (90,8 %); **nedojde 83 (9,2 %)**: ani ideální cesta
    26 (2,9 %), greedy selže 57 (6,3 %). Kontext: 320 nabídek (35,4 %) má
    `dist == MA + 2`, tj. spoléhá na **oba** GFI (to je kostka, ne rozpor;
    uvádím, protože „dojde" tu znamená 69 % i při hladké cestě).

### 2.10 BLITZ_AND_SCORE — po T5.35a

(a) Brána `:835-872`: `dist <= MA + GFI` (`:839-844`), blokující v cestě.
(b) Expanze `:1756-1852`: BLITZ akce na blokujícího od kohokoli (`:1800`),
    pak nosič 14 kroků (`:1850`).
(c) Zbytkový rozpor: brána nezkoumá, kdo blitzuje; když jen nosič, stojí ho
    blok 1 MP + přístup.
(d) N = 337: spoluhráč může blitzovat 325 (96,4 %); jen nosič a dosah po
    bloku stačí 7; **jen nosič a nezbývá dosah 5 (1,5 %)**. V 24 (7,1 %)
    nosič nemá cestu ani BFS — blokující stojí v ní; zda ji blitz otevře
    (push vs. knockdown), je kostka, **z tohoto měření nezjistitelné**.

---

## 3. Makra, kde se brána a expanze NEROZCHÁZEJÍ (v dosahu)

- **BLOCK** (`:874-910` / `:1854-1869`): soused stojící, BLOCK akce existuje
  vždy (`rules_engine.cpp:66-85`). Rozpor je jiného druhu: brána počítá
  Dauntless jako vyrovnaný (`:363-372`, `dauntlessInOffer`), blok se hází.
  N = 192 146 nabídek; 6 496 (3,4 %) je Slayer proti silnějšímu, z toho
  **4 398 (2,3 %) by se bez vyrovnání nenabídlo** — nabídka platí, jen když
  padne Dauntless. Není to dosah; zapisuji pro úplnost.
- **PASS_ACTION** (`:995-1105` / `:1903-1921`): hlídá pravítko i blizzard
  i správný limit (hand-off vs. pass) — jediné „přihrávkové" makro, které to
  dělá. Neměřeno dál.
- **FOUL** (`:1107-1124` / `:1923-1938`): soused ležící, FOUL akce existuje
  (`rules_engine.cpp:151-162`). Neměřeno.
- **END_TURN**: triviální.
- **Vstávání** (`:499-520` / `rules_engine.cpp:245-262`): MOVE na vlastní
  pole existuje pro každého ležícího bez `hasActed`; chůze ho vybere (skóre
  0). Konzistentní; hod 4+ při MA < 3 je kostka.
- **Blitz-continuation REPOSITION** (`:550-581`): počítá jen `movementRemaining`
  bez GFI a cíl je nejbližší volné pole mimo TZ; není v snímku začátku kola
  (vyžaduje `usedBlitz`), **neměřeno — N/A, ne nula**.

---

## 4. Co je společný mechanismus

Ve všech osmi případech je brána **skalár** (Chebyshev vzdálenost, nebo
vůbec nic) a expanze je **greedy chůze po jednom poli** s pravidly
`:1393-1394`, která ji zastaví, jakmile žádný krok neubírá vzdálenost —
a se skórem `:98-103`, které vstup do TZ cení víc než pole vzdálenosti.
Rozpor je proto dvojí:

(1) brána nezná obsazenost (ADVANCE 35 %, CAGE, BLITZ 6,4 %, PASS_SCORE
    pravítko),
(2) i když cesta existuje, chůze ji nenajde (SCORE 6,3 %, PICKUP 14,0 %,
    ADVANCE 15,5 %, HAND_OFF/CHAIN „neskončí vedle"). U PICKUP je to
    **93 % všech selhání**, u SCORE 69 %.

Sloupec „ani BFS" je malý všude (SCORE 2,9 %, PICKUP 1,1 %, příjemci
0,4–3,1 %) — geometrický dosah brány je většinou správně; **co chybí, je
CESTA**, ne vzdálenost.

---

## 5. Nuly a co znamenají

- BLITZ „nikdo nedosáhne na nikoho" = 3 kola ze 47 990: **měřeno, děje se to
  prakticky nikdy** ⇒ vada je ve výběru cíle.
- REPOSITION „k nosiči" a „marker": 0 dojití je **z definice** (cíl je
  obsazený), ne nález.
- PASS_ACTION, FOUL, vstávání: **0 nálezů, protože neměřeno** — kód je
  konzistentní čtením, N chybí.
- Blitz-continuation: **N/A**, snímek ho neobsahuje.
- „Kolikrát MCTS prázdné makro skutečně VYBRAL": **N/A** — ze snímku
  nezjistitelné, potřebuje přehrání s nabídkami (T5.33). Viz § 7.

---

## 6. Co emulace nezachytí (meze měření)

- Kostky: každé „dojde" je podmíněno úspěchem všech dodge/GFI. U SCORE
  35 % nabídek stojí na dvou GFI.
- Ramena: P35 zapnuto v korpusu mění, KDO blitzuje, ne zda někdo dosáhne.
- Remízy v `std::sort` u BLITZ (43,6 % nabídek): číslo 6,4 % je bodový
  odhad; kolové číslo 7,2 % je na tom nezávislé jen shora.
- Podlaha: nabídky vzniklé během kola (po jiných makrech) nevidím.
- REPOSITION iteruje hráče v pořadí id; stavové příznaky (`hunterPlaced`
  atd.) závisí na pořadí — použil jsem pořadí snímku, které je id.

---

## 7. Pozorovatelný příznak: kola bez jediné akce

`macro_mcts.cpp:1180-1183`: prázdný plán ⇒ END_TURN. V korpusu:

| | N | % našich kol |
|---|---|---|
| našich kol | 48 000 | |
| **kolo bez jediné naší akce** | 1 189 | 2,5 |
| …s míčem v ruce | 624 | 1,3 |
| …s míčem v ruce a nabídnutým ADVANCE | 581 | 1,2 |
| …v obraně | 548 | 1,1 |
| …s míčem na zemi | 17 | 0,04 |

⚠️ Tohle je **korelace, ne důkaz**: kolo bez akce může být i END_TURN,
který vyhledávání vybralo záměrně (stall při vedení, `macro_mcts.cpp:494`).
Co lze říci: 581 kol, kdy jsme drželi míč, ADVANCE byl v nabídce a tým
neudělal nic, je horní mez toho, co mechanismus „vzdal se → prázdno →
END_TURN" mohl způsobit; kolik z nich to opravdu bylo, rozhodne jen
přehrání.

---

## 8. Tabulka

| makro | podmínka brány (soubor:řádek) | co expanze vyžaduje | rozchází se? | jak často (N) |
|---|---|---|---|---|
| SCORE | `dist <= MA+2` (`macro_actions.cpp:583-590`) | greedy chůze 14 kroků do endzóny (`:1474`) | ANO (cesta) | 83 / 903 = **9,2 %** (ani BFS 2,9 %) |
| ADVANCE | `dist > MA+2`, MA > 0 (`:710-717`) | volné TZ-free pole na přímce (`:1656-1663`) + chůze `steps+2` (`:1665`) | ANO (obsazenost + cesta) | 9 957 / 18 135 = **54,9 %** (vzdá se 35,0 %, chůze 19,9 %) |
| CAGE | ≥ 1 volný spoluhráč (`:719-730`) | nejbližší hráč dojde na roh do 4 kroků (`:1697-1701`) | ANO (dosah) | 3 271 / 19 037 = **17,2 %** nic neudělá (+ 8,0 % klec hotová) |
| BLITZ | žádná (skóre bez dosahu, `:732-812`) | BLITZ akce = BFS s rezervou 1 (`rules_engine.cpp:87-109`) | ANO (dosah) | 4 389 / 69 000 = **6,4 %** cílů; 7,2 % kol top-1 nedosažitelný |
| BLOCK | soused, kostky ≥ 2 / 1 s Block (`:874-910`) | BLOCK akce (`:1854-1869`) | NE (dosah); kostka Dauntless | 4 398 / 192 146 = 2,3 % stojí na hodu |
| PICKUP | Chebyshev `<= MA+2` (`:936-937`) | chůze na míč `MA+2` kroků (`:1880`) | ANO (cesta) | 1 880 / 12 448 = **15,1 %** (ani BFS 1,1 %) |
| PASS_ACTION | pravítko + blizzard + limit (`:1052-1061`, `:1028`) | HAND_OFF/PASS akce (`:1911-1919`) | NE (čtením) | neměřeno |
| FOUL | soused ležící (`:1107-1124`) | FOUL akce (`:1930-1936`) | NE (čtením) | neměřeno |
| REPOSITION | žádná (`:1126-1350`) | chůze `MA` kroků na cíl (`:1956-1957`) | ANO (z definice) | dojde 68 084 / 247 919 = **27,5 %**; obsazený cíl 37,9 %, > MA 23,8 %, selže v dosahu 7,2 %; marker vedle nosiče 71 / 8 387 |
| END_TURN | vždy (`:497`) | END_TURN | NE | — |
| BLITZ_AND_SCORE | `dist <= MA+GFI` + blokující (`:835-872`) | BLITZ akce od kohokoli (`:1800`) + nosič 14 kroků (`:1850`) | zbytkově | 5 / 337 = 1,5 %; 24 (7,1 %) nejisté (cesta přes blokujícího) |
| HAND_OFF_SCORE | stuck, spoluhráč ≤ 2, příjemce `MA+2` (`:592-617`) | nosič do sousedství (`:1999-2000`), HAND_OFF (`:2010`), příjemce 14 kroků (`:2028`) | ANO (oba přechody) | 81 / 292 = **27,7 %**; přešlapování 39,0 %, dodge navíc 30,1 % |
| PASS_SCORE | Chebyshev 2–10, příjemce `MA+2` (`:619-653`) | PASS akce z pravítka (`rules_engine.cpp:117-131`) + příjemce 14 kroků (`:2061`) | ANO (pravítko + cesta) | 46 / 469 = **9,8 %** (bez akce 1,3 %) |
| CHAIN_SCORE | Chebyshev 1–10, relay ≤ 2, střelec `MA+2` (`:655-708`) | PASS + relay do sousedství (`:2096-2098`) + HAND_OFF + střelec (`:2118`) | ANO (tři přechody) | 58 / 218 = **26,6 %** |
| vstávání (REPOSITION) | ležící, `!hasActed` (`:499-520`) | MOVE na vlastní pole (`rules_engine.cpp:260`) | NE | neměřeno |
| blitz-continuation (REPOSITION) | `usedBlitz`, MA > 0, v TZ (`:550-581`) | chůze bez GFI | ? | N/A (není v snímku) |

Zdroje: všechna N z `diag_offer_reach_audit_20260827.py` nad
`blitzlanding_replic_20260825_corpus_data` (3 000 her, 48 000 našich kol),
surový výstup `evidence/fable_offer_reach_audit_20260827.raw.txt`; řádky
kódu k HEAD 27.08.2026 (po T5.35a).
