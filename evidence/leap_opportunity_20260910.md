# A1 LEAP — MĚŘENÍ PŘÍLEŽITOSTÍ (10.09.2026)

**Zadání uživatele:** *„ať se měří jen přeskočení zdi — dostane se někam, kam
jinak ne, a do klece na blitz nosiče."* Plus jeho pravidlo k třetí škatulce:
*„C — nepoužij leap."*

Skript: `diag_leap_opportunity_20260910.py` · korpus `corpus_m11_20260909_data`
*(1000 her, z toho **200 s wood-elfem**, engine `cf8634e8`)*.

## ⛔⛔ PROČ SE TO NEMĚŘILO ZAPNUTÝM RAMENEM — VADA, KTERÁ BY BĚH ZNEČITELNILA

`macro_actions.cpp:2494`: když je `leapWalkArm` **ON**, `movePlayerToward` se
vrátí na **starý hladový výběr** (`findMoveToward`) místo nasazeného BFS
(`nextStepToward`). ⇒ **Rameno mění DVĚ věci naráz** — leap jako kandidát
**a** regrese pohybu na greedy. Jakýkoli běh s ním by měřil jejich součet
a přiřkl ho jednomu jménu *(táž vada, kvůli které se noc Q3 musela dělit na
16/17)*.
⇒ **Proto měření PŘÍLEŽITOSTÍ, ne chování** — a to rameno vůbec nepotřebuje.

## Jmenovatele

| | |
|---|---|
| her s wood-elfem | 200 |
| jejich tahů | 3 200 |
| **aktivací stojícího Wardancera** | **4 396** *(1,37 na tah, 22 na hru)* |
| legálních cílů leapu | 85 347 *(19,4 na aktivaci)* |

⭐ **Pozitivní kontrola dosažitelnosti:** průměrně **205 polí** dosažitelných
chůzí *(min 0, max 248)*. Kdyby BFS vracel prázdno, vypadalo by všechno jako
„za zdí" a nález by byl **obrácený** — nula i jednička se dají rozeznat.

## 1. „PŘESKOČENÍ ZDI" — PREMISA SE NEPOTVRDILA

| příležitost na aktivaci | | |
|---|---|---|
| cíl **nedosažitelný chůzí vůbec** *(„za zdí")* | **9** | **0,2 %** |
| cíl za zdí **A ZÁROVEŇ u nosiče** | **0** | **0,0 %** |
| cíl dosažitelný, ale jen přes TZ | 3 565 | 81,1 % |
| cíl u soupeřova **nosiče** | 465 | 10,6 % |
| **bez jakékoli příležitosti (ZBYTEK)** | 829 | 18,9 % |

⇒ ⛔ **Skok „za zeď, kam se jinak nedostanu" se prakticky nevyskytuje** — 0,2 %
aktivací. A **v kombinaci s nosičem ANI JEDNOU ve 200 hrách.** Důvod je
geometrický a dal se předvídat: **leap má dosah 2, ale Wardancer má MA 8** ⇒
skoro cokoliv jde obejít.

⚠️ **Výhrada, která to nevyvrací, ale ohraničuje:** snímek je **začátek tahu**,
tedy MA je na maximu a dochozích polí je nejvíc, co kdy bude. ⇒ **0,2 % je
DOLNÍ hranice**; uprostřed tahu s menším zbytkem MA by „za zdí" bylo častější.
⭐ Ale `A&B = 0` platí **při plném MA**, a to je pro klecový případ silný signál.

## ⭐⭐⭐ 1b. CENA TÉ PŘÍLEŽITOSTI — „S RIZIKEM" *(uživatel 10.09.)*

Čísla z kódu, ne z hlavy: `calculateLeapTarget` *(`helpers.cpp:79-83`)* =
`7 − AG`, clamp [2,6] ⇒ **Wardancer AG 4 hodí 3+**, tedy **selže ve 33,3 %**.
Při nezdaru *(`resolveLeap`, `move_handler.cpp`)*: **hráč leží v CÍLOVÉM poli,
hod na brnění a zranění, a je to TURNOVER.** Wardancer je **AV 7** ⇒ brnění
padne na 8+ = **41,7 %**.

| na jeden pokus | |
|---|---|
| skok **selže** | **33,3 %** ⇒ **turnover** *(konec tahu celému týmu)* |
| ...a z toho prolomí brnění | 41,7 % ⇒ **13,9 % pokusů končí hodem na zranění** vlastního AV7 těla |
| navíc `N7` | dvoupolový skok může stát **dva GFI hody** *(ř. 1701, D6 za KAŽDÉ pole navíc)* |
| navíc `N6` | `Tentacles` a `Shadowing` platí i na skok *(ř. 8586-8587, 8456-8458)* |

⇒ ⛔ **Spojeno s příležitostí: 0,2 % aktivací × 22 aktivací na hru ≈ jedna
příležitost na ~20 her — a za cenu třetinového turnoveru.**
⭐ To je v terminologii zadání z 25.08. *(bod 4: „řekne, jestli je to velká vada,
nebo hygiena — přesně jako M9 u P31")* odpověď **HYGIENA**, ne velká vada.

## 2. ⚠️ „DO KLECE NA NOSIČE" — a POZOR, TENHLE ODDÍL NENÍ ROZHODUJÍCÍ ARGUMENT

⛔⛔ **UŽIVATEL 10.09.: *„to, že nesmíme porovnávat dodge a leap přímo, jsme už
zjistili — proto jen situace, kam se dostane jen leap, s rizikem."*** A měl
pravdu: zadání to říká **od 25.08.** *(`fable_brief_leap_20260825_DRAFT.md`,
bod 4)* a tentýž dokument o dva odstavce výš uzavírá i to srovnání: *„neplatí se
dodge za opuštění výchozího pole ⇒ z obklíčení je skok LEVNĚJŠÍ než dodge, a to
je jeho hlavní hodnota."*
⇒ ⛔ **Tabulka níž tedy znovuotevřela uzavřenou otázku** — nechávám ji jako
záznam měření, ale **verdikt na ní nestojí.** Rozhoduje oddíl `1` + `1b`:
*kam se dostane JEN leap, a co ten pokus stojí.*

Pole u nosiče v dosahu leapu existuje ve **465 aktivacích (10,6 %)**. Protože
`A&B = 0`, byla **vždycky nějak dochozí** ⇒ otázka není *„dostane se tam?"*, ale
**„za kolik se tam dostane jinak?"**. Minimální počet dodgí přes všechny chodící
cesty do MA kroků *(dodge se platí za OPUŠTĚNÍ pole v cizí TZ)*:

| cena chůze k nejlevnějšímu poli u nosiče | | | verdikt |
|---|---|---|---|
| **0 dodgí** | 121 | **26,0 %** | ⛔ **leap nemá smysl — uživatelovo „C"** |
| **1 dodge** | 283 | **60,9 %** | ⚠️ viz asymetrie níž — **chůze je LEPŠÍ** |
| 2 dodge | 54 | 11,6 % | ✅ leap je zisk |
| 3 dodge | 4 | 0,9 % | ✅ leap je zisk |
| 5 dodgí | 1 | 0,2 % | ✅ leap je zisk |
| nedosažitelné chůzí | 2 | 0,4 % | ✅ leap je jediná cesta |

## 3. ⭐⭐⭐ A PRAVIDLA TU ASYMETRII ROZHODUJÍ — LEAP NEMÁ REROLL

`rules_bb2016.txt` ř. 8270-8283: leap je **plochý hod na AG bez modifikátorů**
*(mimo Very Long Legs)*, stojí **dvě pole pohybu**, smí se **jednou za kolo**,
při selhání je hráč **Knocked Down v cílovém poli** — a **žádný reroll k němu
pravidla nedávají**.
⇒ ⛔ **Wardancer má ale `Dodge`**, tedy **reroll na selhaný dodge** *(ř. 8078-8090,
jednou za kolo)*. ⇒ **Jeden dodge s rerollem je bezpečnější než jeden leap bez
něj**, takže bucket „1 dodge" (60,9 %) **nepatří leapu, ale chůzi**.

⭐⭐⭐ **UPŘESNĚNO UŽIVATELEM 10.09.: „na leap neplatí dodge reroll — ten je jen
na dodge. Na leap jen team RR."** ⇒ Asymetrie je tím **ještě větší, než jak je
popsaná výš**, a je to rozdíl v **druhu zdroje**, ne v pravděpodobnosti:

| | čím se zachrání selhání | cena toho zdroje |
|---|---|---|
| **dodge** | **`Dodge` — hráčův vlastní**, zdarma, 1× za tah | **žádná** pro zbytek týmu |
| **leap** | **jen TÝMOVÝ reroll** | **soutěží s celým tahem** — blok, GFI, přihrávka, pickup |

⇒ ⛔ **Leap tedy neplatí jen vyšším rizikem, ale SDÍLENÝM zdrojem.** A jeho
skutečnou cenu dnes **neumíme ocenit ani v plánovači** — je to přesně
[[project_bloodbowl_celotah_situace]] `A7` *(týmový reroll je sdílený zdroj tahu;
`gfiSequenceFailProb` má proto `rerollAvailable` natvrdo `false`)*.
⇒ ⭐ **Tím je „1 dodge" definitivně chůze**, a i „2 dodge" je slabší argument
pro leap, než by čistá pravděpodobnost napovídala.

⚠️ Jediné, co to může otočit, je **hodně tacklezón na cíli**: dodge má
modifikátory podle TZ, leap ne. To tenhle běh nerozpadá.

## ⭐⭐⭐ 4. UŽIVATELOVA KOREKCE 10.09. — „ZEĎ" NENÍ VYVRÁCENÁ, JE PŘEDČASNÁ

> *„Z výsledků jsem pochopil, že část leap o přeskočení zdi budeme řešit až
> v celotahu — kde nejdříve trpaslík postaví zeď."*

⛔ **Tím se mění výklad těch 0,2 %, a k lepšímu pro Leap.** Číslo neříká
*„přeskakovat zeď je bezcenné"* — říká, že **v korpusu skoro žádná zeď nestojí**.
A to je vlastnost NAŠEHO enginu, ne pravidel: zeď je týmová struktura a engine ji
neumí postavit ani udržet *(`celotah_situace.md` `A6`: „zeď budeme umět až
v celotahu — a i tam doufám"; dnešní `A8` totéž pro udržování formace)*.
⇒ ⭐ **`A-tvrdé` je proto PODMÍNĚNÉ MĚŘENÍ, ne verdikt:** platí *„dokud zdi
nestojí"*. Až je celotah postaví, musí se to **přeměřit** — a teprve pak to bude
odpověď na otázku „má přeskočení zdi cenu".
⛔ **Nezaměňovat s částí `B` (klec):** tam žádná taková podmínka není, klec
v korpusu stojí běžně, takže její čísla platí už dnes.

## 5. „LEAP DO KLECE" Z HLEDISKA KÓDU *(zadání uživatele 10.09.)*

⛔⛔ **DNES TO NEJDE VŮBEC, a ne kvůli rameni.** `pathfinder.cpp` má **nula**
zmínek o leapu *(ověřeno grepem)* — Dijkstra zná jen normální kroky. A doběh
blitzu jde přes ni: `action_resolver.cpp:337,397` volá `nextStepTowardAdjacent`.
⇒ **Blitz neumí skočit, ani se zapnutým `leapWalkArm`** — to rameno sedí
v `findMoveToward`, kterou používá `movePlayerToward` *(obecné pohybové makro)*,
a **blitz ji nepoužívá**. „Leap do klece a blitz nosiče" tedy dnes **není
vyjádřitelné**, ne špatně oceněné.

**Dvě možné podoby, a nejsou rovnocenné:**

**(i) naučit Dijkstru hranu leapu** — z pole `u` na prázdné `v` s `cheb == 2`,
cena `2 MA + riziko AG`. ⛔ **Tři důvody, proč to je špatná cesta:**
* změnilo by to **nasazený a změřený doběh M14b** pro každého nositele Leapu
* leapové **selhání není dodge**: hráč leží **v cílovém poli**, hod na brnění,
  turnover ⇒ jiný tvar škody, takže `kRiskMultiplier` na to nesedí
* ⭐⭐ **„1× za kolo" je zdroj celého TAHU, a Dijkstra na uzel to neumí vyjádřit**
  — je to **přesně týž tvar jako týmový reroll**, dnes zaparkovaný jako
  `celotah_situace.md` `A7`. Dvouvrstvová Dijkstra z `3bd48fc2` ukázala, kde je
  hranice: stav *„zdroj ještě mám"* jde přidat, ale **jen jako aproximace**.

**(ii) vlastní makro `LEAP_AND_BLITZ`** — plán o dvou krocích *(doskoč vedle
nosiče → blitzuj ho)*, tak jak to dělá `BLITZ_AND_SCORE`. ⭐ **To je správná
podoba**, protože: nechává M14b nedotčený · „1× za kolo" se kontroluje v jednom
místě, ne v každé hraně · a je to **sekvence**, tedy přesně to, na co makra jsou.
⚠️ **Ale dědí to slabinu `A5`:** engine bude takovou sekvenci umět **PROVÉST**,
ne **NAJÍT** — bude to další ručně napsaný plán, a `T5.35a` ukázalo, že ruční
plán **nekontroluje vlastní předpoklady** *(nabízel se, kde se nedalo dojít)*.
⇒ **Podmínka pro (ii): brána musí ověřit, že doskok je legální A že po něm
zbývá blitz** — jinak vznikne třetí `BLITZ_AND_SCORE`.

## ⇒ ZÁVĚR: HYGIENA, NE VELKÁ VADA — A ODPOVÍDÁ TO NA ZADÁNÍ Z 25.08.

Bod, na kterém verdikt stojí *(a jediný, který stát smí)*:

> **Situace, kam se dostane JEN leap: 0,2 % aktivací Wardancera. V kombinaci
> s nosičem 0 ze 4 396. A jeden pokus stojí 33,3 % turnover a 13,9 % hod na
> zranění vlastního AV7 těla.**

⇒ **~0,05 příležitosti na hru** *(jedna na ~20 her)* za třetinové riziko
turnoveru. To je přesně to, na co se zadání z 25.08. ptalo, a odpověď je
**hygiena** — analogie `M9` u `P31`.

⭐ **Uživatelovo pravidlo „C — nepoužij leap" tím dostává tvrdou hranici, a
UŽŠÍ, než jsem psal poprvé:** leap má smysl **jen tam, kam chůze nevede vůbec** —
tedy v těch 0,2 %. Srovnávat ho s dodgem se nesmí *(jiný zdroj rerollu, viz oddíl
3 a `CELOTAH/A7`)*, takže „chůze by stála 2 dodge" **není** argument pro leap.

⏰ **A co z toho plyne pro rozhodnutí o nasazení** *(rozhodne uživatel)*:
* Nasadit rameno **tak, jak dnes je, nelze** — napřed se musí opravit, že
  zapnutí regreduje pohyb na greedy *(viz výše)*.
* I po opravě je strop **0,3 aktivace na hru** ⇒ na chess deltě **neměřitelné**
  *(a měřit to nocí by bylo přesně to plýtvání, které triáž zakazuje)*.
* ⇒ Rozumné varianty: **(a)** rameno retirovat jako `B2`/`P9c`
  *(„ať za sebou nenecháme něco, co neškodí")*, nebo **(b)** nasadit
  bezpodmínečně s tou úzkou branou `≥ 2 dodge`, protože **v té podmnožině to
  není taktika, ale správná cena** — a tam se podle `FRONTA A` nasazuje
  bez ohledu na deltu.
