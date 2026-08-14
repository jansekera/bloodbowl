# MATCHUPOVÁ ASYMETRIE — proč je skaven náš nejslabší a ork nejsilnější soupeř

**14.08.2026.** Vzniklo z uživatelovy námitky k Fableho analýze orků
(*„porovnáváš hrušky s jablky"*) a z ověření proti `rules_bb2016.txt`,
`engine/src/roster.cpp` a `engine/src/block_handler.cpp`.

## Číslo, které to vyvolalo

Naše TD / jejich TD na 750 zápasů každý *(korpus 3000 her, 14.08.)*:

| soupeř | naše | jejich |
|---|---|---|
| **skaven** | **451** | 299 |
| wood-elf | 260 | 421 |
| human | 178 | 281 |
| **ork** | **86** | 207 |

**5,2× rozdíl mezi skavenem a orkem. Týž engine, táž doktrína, týž náš roster.**

---

## ⭐ ZÁVĚR: nejde o sílu týmů, jde o ASYMETRII NA DVOU OSÁCH NARÁZ

### Osa 1 — odebrání míče: skavenovy nástroje jsou proti nám neutralizované

| jejich nástroj | proč proti nám neúčinkuje |
|---|---|
| **2kostkový blitz** | **nemají ST4** — max ST3 (`getSkavenRoster1200`), Gutter Runneři jsou **ST2** a proti našemu ST3 Runnerovi by šli **do kopce** |
| **Strip Ball** *(ball-hunter Blitzer)* | ruší ho **Sure Hands** našeho Runnera. BB2016: *„the Strip Ball skill **will not work against a player with this skill**"*. Engine to ctí (`block_handler.cpp:627`) |
| **Tackle** *(týž Blitzer)* | bezpředmětný — **náš Runner nemá Dodge** |
| **Wrestle** *(2× Lineman)* | ⚠️ **funguje** a jde **mimo obě naše pojistky**: Sure Hands je jen proti Strip Ballu a **Wrestle přebíjí Block** — na „Both Down" jdou oba k zemi a míč padá (`block_handler.cpp:492–508`), a **není to jejich turnover**. Ale: jen **2 hráči z 11** a **1/6** na jeden blok |

### ⭐ Proč jim Wrestle dnes nefunguje: nástroj bez dosahu, dosah bez nástroje
*(uživatel 14.08.: „skaven má blitzovat s GRunnerem — nebo linemanem, oba s Wrestle")*

| kdo blitzuje nosiče | dosah | co potřebuje, aby Wrestle vyskočil |
|---|---|---|
| **Lineman +Wrestle** MA7 | ke kleci často **nedojde** | ST3 vs ST3 ⇒ 1 kostka ⇒ „Both Down" **1/6** |
| **Gutter Runner** MA9 | **dojde vždycky** | ST2 vs ST3 je **do kopce** ⇒ kostku vybíráme **my** ⇒ Wrestle se skoro nespustí |

⇒ Dnešní skaven má **nástroj bez dosahu a dosah bez nástroje**, a to je hlavní
důvod, proč jim ani ten jediný funkční kanál nic nedává.

⛔ **Wrestle na Gutter Runnerovi by ty dvě věci spojil** — a byl by to nástroj,
který **obchází sílu úplně**: nepotřebuje ST4, neruší ho Sure Hands, nechrání
před ním Block. Stačí mu dostat se s asistencemi na jednu kostku.
**V našem TV1200 rosteru není** ⇒ patří do **T5.13** jako nejnebezpečnější
skavení varianta, hned vedle OTT Runnera a bash Blitzera.

Naproti tomu **ork má 4× Black Orc ST4 +Guard+Block** ⇒ **dvě kostky na našeho
ST3 Runnera zadarmo, bez asistence.** Změřeno: **2kostkový blitz na nosiče má
ork k dispozici v 68 % svých kol, skaven v 6 %.**

⚠️ **Uživatelova námitka byla oprávněná** — to srovnání měřilo jen **silový**
kanál. Po dohledání ale platí: skavenův **dovednostní** kanál je z velké části
mrtvý, takže těch 6 % skavena **nepodhodnocuje**. Přepočet přes všechny kanály
zadán Fablemu.

### ⭐ A proč jim ani Wrestle neprojde: GUARD *(uživatel 14.08.)*

CRP Guard: *„The player may assist an offensive or defensive block **even if he
is in another player's tackle zone**."* ⇒ **naše obranné asistence u nosiče
nejdou zrušit značkováním, jejich útočné ano** — Guard nemá **ani jeden skaven**.

Náš TV1200 má Guard na **šesti** hráčích, a čtyři z nich (Blitzeři + Slayeři)
jsou doktrinálně **rohy klece**, tedy přímí sousedé nosiče.

⇒ Skavení lineman ST3 na našeho Runnera ST3 se dvěma Guardy vedle:
**3 proti 5 ⇒ dvě kostky, vybíráme MY.** A protože vybíráme, **„Both Down"
nikdy nezvolíme** ⇒ **Wrestle se jim nespustí**. Poslední funkční kanál padá.

⇒ *„Ale zkusit to musí, nic jiného nemá."* Lezou do nás do kopce a **platí za to
těly** — a to je přesně ta attritionová bilance níž. **Nejsou slabí náhodou; my
je do té ztráty tlačíme.**

⛔ **Proti orkovi tohle NEPLATÍ:** ork má Guard taky na **šesti** hráčích
(2 Blitzeři + **4 Black Orci ST4**). Jejich asistence tedy taky nejdou zrušit —
a startují o stupeň výš. **Tam, kde skavena Guardem umlčíme, orka ne.**

### Osa 2 — attrition: naše dovednosti kousají do nich, jejich do nás ne

* **Tackle na všech 16 Longbeardech** kouše do rosteru plného **Dodge**
  (skaven, wood-elf) — a je **úplně mrtvý** proti orkovi, který **Dodge nemá**.
* **AV9 proti jejich AV7** (skaven) vs **AV9 proti AV9 + čtyři ST4** (ork).

Doloženo výsledkem *(Fable 14.08.)* — **stojících hráčů v kole 8**:

| | naši | jejich |
|---|---|---|
| proti skavenovi | **7,0** | 4,1 |
| proti orkovi | **4,9** | **6,4** |

⇒ **Ork je jediný matchup, kde soupeř bije víc než my.**

### ⭐⭐ SKUTEČNÝ SKAVENÍ PLÁN — nebrat míč, ale vyhrát závod k němu
*(uživatel 14.08.)*

Skaven nám dá **198 krádežových TD**, ork jen **31**. Dávalo mi to smysl až
s tímhle: **oni míč odebírat nepotřebují. Potřebují, aby chvíli ležel.**

| krok | čím |
|---|---|
| míč se uvolní | Wrestle · blitz · **nebo prostě náš fumble** |
| **jsou u něj dřív** | **MA9 proti MA4** |
| *(varianta)* chytí ho **v obklíčení** | **Nerves of Steel** — bez postihu za naše TZ |
| uvolní si cestu | blitz |
| utečou a skórují | **Dodge**, dvě kola |

⇒ **Proto se za skavena vyplatí blitz na míč i za cenu ztráty těl** — jedno
uvolnění míče se u nich blíží gólu. Nemusí ho protlačit, stačí, aby spadl.
Tělo je pro ně levnější než pro nás.

### ⛔ CO Z TOHO PLYNE PRO NÁS: cena ztráty míče NENÍ konstanta

**Je to funkce rychlosti soupeře k míči.** Proti skavenovi je upuštěný míč skoro
inkasovaný gól; proti orkovi (MA4–5, AG2–3) je to nepříjemnost, kterou často
sebereme zpátky.

Engine to hodnotí **plošně** (`macro_mcts.cpp:762`):
```cpp
heuristic -= 0.1;                          // loose ball is bad
if (nearestDist <= 2) heuristic += 0.08;   // ptá se JEN na nás
```
Ráno *(P10a)* jsme našli, že se neptá, **kdo je blíž**. Teď víme, že se musí
ptát i **kdo je rychlejší** — je to táž jedna oprava se dvěma vstupy místo
jednoho.

⇒ A symetricky to potvrzuje uživatelovu podmínku k P10a z druhé strany:
**my potřebujeme tři těla, abychom scramble vyhráli** (kdo srazí, kdo sebere,
kdo zavře cestu). **Oni jedno.**

---

## ⛔ CO Z TOHO PLYNE PRO VŠECHNA NAŠE ČÍSLA

**Průměr přes čtyři soupeře míchá dva opačné světy.** Doktrína laděná na
průměru bude špatná pro oba konce:

* proti **skavenovi** vyhráváme obě osy a čísla vypadají dobře i s vadnou doktrínou;
* proti **orkovi** prohráváme obě osy a táž doktrína nestačí.

⇒ **Každá kontrola, každý prediktor a každé A/B se musí číst i per-matchup.**
Agregát je vážený průměr dvou režimů, ne popis jednoho.

⇒ A opačně: **naše doktrína se nesmí validovat na skavenovi.** Tam projde skoro
cokoli, protože soupeř nemá čím trestat.

## Souvisí
* `evidence/fable_orc_scoring_gap_20260814.md` — kde přesně drive proti orkovi umírá
* `evidence/fable_open_question_1_20260814.md` — proč zlepšení procesu nehýbe výsledkem
* `evidence/task_queue.md` — P13 (Dauntless v nabídce), T5.13 (přestavba rosterů)
