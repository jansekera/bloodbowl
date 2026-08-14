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

> ⚠️ **ČTI I ODDÍL „OPRAVA SMĚRU" NÍŽE.** Oddíly „Osa 1" a „Osa 2" popisují
> **výbavu** obou stran a platí tak, jak jsou. Ale **příčinu rozdílu 451 vs 86
> vysvětlují špatně** — přepočet přes všechny kanály ukázal, že hrozba na našeho
> nosiče je napříč rasami skoro stejná a rozdíl leží v **našem útoku na jejich
> nosiče**. Ta čísla „68 % vs 6 %" níž jsou pravdivá, ale měří **jejich** hrozbu,
> ne příčinu naší neúspěšnosti. Nezahazovat, jen nečíst jako mechanismus.

## ⭐ VÝBAVA OBOU STRAN: asymetrie na dvou osách naráz

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

## ⭐⭐⭐ OPRAVA SMĚRU (14.08. odpoledne, po přepočtu kanálů)

**Uživatelova námitka („hrušky s jablky") byla trefa a přepočet obrátil příčinu.**
Sjednocená hrozba přes všechny kanály (síla ≥2k · Strip Ball jen proti nosiči
bez Sure Hands · Wrestle · hrubé sražení):

| | THREATnet **na nás** | P(náš nosič ↓) |
|---|---|---|
| human | 0,450 | 0,162 |
| ork | 0,424 | 0,173 |
| skaven | **0,400** | **0,125** |
| wood-elf | 0,413 | 0,151 |

⇒ **Hrozba na našeho nosiče je napříč rasami skoro stejná**, a kalibrace ukazuje,
že **při stejné hrozbě je konverze rasově nezávislá** (P(↓) 0,090–0,105 ve
stejném pásmu). **Jejich obrana rozdíl 451 vs 86 nevysvětluje.**

### Rozdíl je v NAŠEM útoku na JEJICH nosiče

| | P(srazíme jejich nosiče) | náš 2k blitz dostupný | STEAL+TD | naše TD |
|---|---|---|---|---|
| **skaven** | **0,347** | **54,4 %** | **198** | **451** |
| wood-elf | 0,266 | 43,4 % | 102 | 260 |
| human | 0,183 | 19,0 % | 82 | 178 |
| **ork** | **0,113** | **7,5 %** | **31** | **86** |

**Monotónní přes všechny čtyři rasy, v obou sloupcích.**
⇒ Formulace se mění z *„ork nás ohrožuje víc"* na
⭐ **„ork si svého nosiče uhlídá a skaven ne."** Jiná příčina, jiné opravy —
míří to na **P15** (nabídka bloku nezná cenu cíle: blok do kopce na soupeřova
nosiče se **nenabídne nikdy**) a na **P13** (Dauntless).

### ⭐⭐⭐ MECHANISMUS TĚCH 7,5 %: GUARD V ROZÍCH, jen z druhé strany
*(uživatel 14.08.: „z koho která rasa staví klec a kdo je nosič?")*

Změřeno na 3000 hrách — složení rohů klece v kolech, kdy daná strana drží míč:

| | Guard v rozích | náš 2k blitz na jejich nosiče | obsazených rohů |
|---|---|---|---|
| skaven | **6,7 %** | **54,4 %** | 1,28 / 4 |
| wood-elf | 5,6 % | 43,4 % | 1,13 / 4 |
| human | 20,6 % | 19,0 % | 1,21 / 4 |
| **ork** | **50,9 %** | **7,5 %** | 1,37 / 4 |
| *trpaslík* | *70,6 %* | — | *1,31 / 4* |

**Monotónní a téměř dokonale inverzní.** Guard v rohu znamená, že jejich obranná
asistence **nejde zrušit značkováním** — proto se k jejich nosiči nedostaneme na
dvě kostky.
* **ork:** Blitzer +Guard **28,4 %** + Black Orc +Guard+Block **22,5 %**
* **skaven:** jen Blitzer +Guard 6,7 %, a do rohů si staví **Gutter Runnery ST2
  (36,4 %)** — nejhorší možné tělo do klece — plus Lineman +Wrestle 25,8 %

⭐ **Ráno vyšlo, že Guard je důvod, proč skaven nemůže ublížit NÁM. Teď se
ukazuje, že je to i důvod, proč MY nemůžeme ublížit orkovi. Táž dovednost,
oba směry.**

### ⚠️ A nepříjemné číslo o nás: ORK SI STAVÍ KLEC LÉPE NEŽ MY
*(uživatel 14.08.: „to si hlídá klec lépe než my")*

Obsazených rohů průměrně: **ork 1,37** · **trpaslík 1,31** · skaven 1,28 ·
human 1,21 · wood-elf 1,13.

**Máme na klec doktrínu, ~60 povinností, kontrolu K29 a celý den debat o rozích
— a ork jich obsadí víc.** Přitom ho řídí **táž generická heuristika** jako nás.
⇒ Naše převaha není v tom, **že** klec stavíme, ale v tom, **z čeho** ji
stavíme (Guard 70,6 % proti jejich 50,9 %).
⇒ Souvisí s tím, že jsme nejpomalejší (MA4): klec se nám hůř tvoří, ne hůř
plánuje. A s nálezem *„nehrajeme s jedenácti, ale se sedmi"*.

**Kdo nese:** ork **Thrower +Block 78,5 %** · skaven Thrower 48,1 % +
**Gutter Runner 37,5 %** · wood-elf Catcher 43,2 % / Lineman 41,1 % ·
human Thrower 74,4 % · *my Runner +Block 82,1 %*.

### Dílčí čísla, která potvrdila obě uživatelovy námitky
* Skavenův **nejlepší kanál je 1k Wrestle v 51,1 % kol** — má ho, ale konvertuje
  nejhůř ze všech ras.
* **Strip Ball:** syrově dosažitelný stripper ve **26,4 %** kol, **po Sure Hands
  5,3 %** ⇒ naše Sure Hands mu ubere **~80 %**.
* Kdo u nich nosí: skaven **Thrower 47 % / Gutter Runner 39 %** (ST3/ST2, AV7),
  ork **Thrower 78 %** — krytý čtyřmi ST4 těly.

## ⛔⛔ NEJVĚTŠÍ VÝHRADA: SOUPEŘ NEHRAJE SVOU RASU
*(uživatel 14.08.: „skaven se místo toho učí hrát jako elf a to ho musíme odnaučit")*

Všechno výše popisuje, **co by skaven mohl dělat**. Náš skaven to nedělá.

**Kořen je strukturální: obě strany řídí týž `MacroMCTSPolicy` s toutéž
heuristikou.** Ta heuristika je psaná pro trpaslíka — klec, rohy, tempo,
markování nosiče, „loose ball is bad" jako konstanta. Skaven ji dostane taky,
takže hraje **obecné nošení míče v tvaru**, ne skavení plán:

| skavení plán | co místo toho dělá naše AI |
|---|---|
| donutit míč spadnout, i za cenu těl | chová si těla, blituje „bezpečně" |
| vyhrát závod k volnému míči (MA9) | hodnotí volný míč **plošně** jako špatný |
| přihrát za klec, chytit v obklíčení | přihrávky prakticky nepoužívá (40 na 3000 her) |
| utéct Dodgem a skórovat ve dvou kolech | veze míč pomalu a v tvaru |

### ⚠️ Co všechno tím pádem měřilo proti nehrané roli

* **Balík G (11.08.), attrition** *(uživatel 14.08.)*: skaven vyšel
  **DEAD 0,14 / KO 1,82**, tedy prakticky **stejně jako wood-elf**
  (0,15 / 1,74). Skaven hrající svůj plán se má vystavovat **víc** než elf —
  když oba hrají tutéž opatrnou hru, vyjdou stejně.
  ⇒ **Uživatelova předpověď o zraněních skavenů nebyla vyvrácená, jen měřená
  proti roli, kterou soupeř nehrál.** Výměna *„~10:1 v náš prospěch"* je
  **horní odhad** a po opravách P10a/P16/P17/P14 se má přeměřit.
* **451 TD proti skavenovi** — viz níž.
* **Všechny prediktory a kontroly** vážené přes čtyři soupeře.

⇒ **Naše měření proti skavenovi jsou nadhodnocená dvakrát:** jednou proto, že
skavení nástroje jsou proti našemu rosteru neúčinné (výše), a podruhé proto,
že je jejich AI **stejně nepoužívá**.

⇒ **Úkol:** soupeřova AI má hrát plán své rasy. Dokud ho nehraje, je „451 TD
proti skavenovi" číslo o naší AI hrající proti sobě samé v jiném dresu.
**Pro trénink i pro validaci doktríny je to nejzávažnější omezení, jaké máme** —
větší než kterákoli jednotlivá pravidlová chyba, protože se týká **všech**
naměřených čísel najednou.

⚠️ Souvisí, ale není totéž, co dřívější výhrada *„soupeřova AI nehraje proti
našim slabinám cíleně"*. Ta říkala, že nás **netrestá**. Tahle říká, že
**nehraje ani vlastní hru**.

### ⭐ Jak drahé to je: rozpadá se to na dvě velmi různě drahé části

*Uživatel 14.08.: „tohle bude možná těžší než odstřihnout dwarfa od učení
rutinou, co musí stihnout dodržet."* — Pravděpodobně ano, **pro tu druhou část**.
U trpaslíka máme doktrínu, ~60 povinností, kontroly a uživatelovu expertizu.
Napsat druhou takovou sadu pro každou rasu je práce na měsíce.

**Část A — rasově citlivé hodnocení. LEVNÁ, a platíme za ni tak jako tak.**
Heuristika není „trpasličí doktrína", je **slepá k rase**:
```cpp
heuristic -= 0.1;                          // volný míč je špatný -- VŽDYCKY
if (nearestDist <= 2) heuristic += 0.08;   // a ptá se jen na nás
```
Skaven s **MA9** dostane tutéž větu jako trpaslík s **MA4**. ⇒ **Proto hraje
jako elf** — nikdo mu neřekl, že volný míč je pro něj **příležitost**.

⭐ **A je to týž člen, který stejně opravujeme kvůli P10a.** Udělat cenu volného
míče funkcí toho, **kdo je blíž a kdo rychlejší**, znamená:
* **naše** rozhodování se zlepší *(to je P10a)*,
* **a skaven začne sám od sebe hrát skavena**, protože heuristika mu konečně
  řekne, že za volný míč stojí riskovat těla.

**Jedna oprava, obě strany.** Odhad: pokryje **největší půlku** rozdílu, protože
skavení hra stojí právě na závodu k míči.

**Část B — skutečný plán rasy. DRAHÁ.** Přihrávky za klec, Nerves of Steel,
volba, kdy obětovat tělo — z části A nevypadnou. Srovnatelné s trpasličím
projektem.

⇒ **Část A zkusit samostatně**, protože se za ni platí prací, kterou stejně
děláme. Část B je vlastní projekt, ne úkol.

## ⭐ KARTA „HRAJE SKAVEN SKAVENA?" — co sledovat po každé rasově citlivé opravě
*(uživatel 14.08.: „napřed navíc kouknem, jestli skaven začne hrát víc sebe")*

Zapsáno **předem**, ať se to po opravě neposuzuje dojmem. Všechno jde odečíst
z korpusů, které stejně sbíráme — **nestojí to žádný běh navíc**.

| # | ukazatel | dnes | čekaný směr |
|---|---|---|---|
| 1 | **Gutter Runner nese míč** | 37,5 % *(Thrower 48,1 %)* | **nahoru** — míč patří tomu, kdo uteče |
| 2 | **Gutter Runner v rohu klece** | **36,4 %** | **dolů** — má být volný, ne zazděný *(P16)* |
| 3 | **Wrestle použit ofenzivně** | **0,16 / hru** *(49 sražení našeho nosiče na 750 her)* | **prudce nahoru** *(P17, P14)* |
| 4 | **získané volné míče** | — | **nahoru** — jejich hra stojí na závodu k míči *(P10a)* |
| 5 | **přihrávky** | **40 na 3000 her** = prakticky nula | **nahoru** — bez nich neexistuje hra přes klec |
| 6 | **jejich vlastní ztráty těl** | — | **nahoru je v pořádku** — tělo je pro ně levnější než míč |

⚠️ **Ukazatel 6 je past na čtení výsledku:** když začnou hrát správně, budou
**ztrácet víc těl** a část jejich statistik se zhorší. To **není** regrese.
A zároveň: **náš náskok proti nim klesne** — část našich 451 TD je artefakt
toho, že svůj plán nehrají.

⇒ **Tuhle kartu odečíst při KAŽDÉ z oprav P10a · P16 · P17 · P14**, ne až na
konci. Když se nehne ani po nich, je část (A) vyvrácená a zbývá jen drahá
část (B).

## ⭐⭐ HUMAN JE JEDINÝ TÝM S PODMÍNĚNÝM PLÁNEM
*(uživatel 14.08.: „strategicky je z nich nejzajímavější human — protože proti
trpaslíkům a orkům musí hrát AGI a proti skavenům a elfům bash")*

Ostatní čtyři mají **jeden plán a hrají ho vždycky**:

| tým | plán |
|---|---|
| trpaslík | mlátit a nedělat chyby — vždy |
| ork | zeď a síla — vždy |
| skaven | shodit míč, vyhrát závod k němu — vždy |
| wood-elf | rychlost a přihrávka — vždy |
| **human** | ⭐ **záleží na soupeři** |

Human je uprostřed všeho (MA6–7, ST3, AG3, AV8), takže **nemá vlastní hru — má
relativní**:
* proti **trpaslíkovi a orkovi** *(pomalí, AV9, ST3–4)* je **rychlejší strana**
  ⇒ musí hrát **AGI**: obejít, ne prorazit;
* proti **skavenovi a wood-elfovi** *(AV7, ST2–3)* je **tvrdší strana**
  ⇒ musí hrát **bash**: elfa nepředběhne, ale rozbije ho.

### Proč je to pro projekt důležité

1. ⛔ **Rasově citlivá heuristika na humana NESTAČÍ.** Část (A) — cena volného
   míče podle rychlosti, vhodnost těla do rohu — je pořád vlastnost **jednoho
   týmu**. Human potřebuje veličinu **relativní k soupeři** (jsem tady rychlejší,
   nebo tvrdší?). ⇒ **Human je nejtvrdší test toho, jestli část (A) stačí.**
2. ⭐ **Vysvětluje, proč šla trpasličí doktrína napsat.** Je **absolutní** —
   mlátíme vždycky, nezávisle na tom, kdo stojí naproti. Proto se dala vyjádřit
   ~60 povinnostmi. **Humanovská by musela být podmíněná** a byla by řádově
   složitější než skavení.
3. Praktické: human nám dá **281 TD proti našim 178** — druhý nejhorší matchup
   po orkovi.

⇒ **Pořadí pro část (B) tím dostává smysl i technicky:** skaven *(nejlevnější,
absolutní plán)* → ork/wood-elf *(absolutní)* → **human až naposled**, protože
teprve on vyžaduje podmíněný plán.

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
