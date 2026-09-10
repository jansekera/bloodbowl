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

## 2. „DO KLECE NA BLITZ NOSIČE" — EXISTUJE, ALE CHŮZE JE OBVYKLE LEVNĚJŠÍ

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

## ⇒ ZÁVĚR: HODNOTA NASAZENÍ JE ~0,3 AKTIVACE NA HRU

Leap je **prokazatelně lepší** v `2+ dodge` a `nedosažitelné` ⇒ **61 ze 4 396
aktivací = 1,4 %**, tedy **~0,3 aktivace na hru**. Ve 86 % případů, kdy nějaká
příležitost u nosiče je, je chůze stejně dobrá nebo lepší.

⭐ **Uživatelovo pravidlo „C — nepoužij leap" tím dostává tvrdou hranici:** leap
se má zvážit **jen když chůze stojí ≥ 2 dodge, nebo tam nevede vůbec**. Cokoliv
jinak je hod zaplacený za nic.

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
