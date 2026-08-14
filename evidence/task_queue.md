# FRONTA ÚKOLŮ — TRVALÁ KNIHA

**Tenhle soubor se NEPŘEPISUJE.** Položky se do něj jen přidávají a mění stav.
Nahrazuje `task_queue_20260812.md` a `task_queue_20260813.md` (oba archivní).

> **Proč vznikl (14.08.2026).** Fronta se od 10.08. každý den psala znovu kolem
> nejčerstvějšího nálezu. Průtok byl slušný (12.→13.08. se uzavřelo 8 položek),
> ale ze ~40 položek z 12.08. jich nová fronta z 13.08. vypisovala **9** —
> ~20 otevřených položek z papíru zmizelo, aniž se uzavřely nebo odložily.
> Namátkou ověřeno, že žijí dál: rozhodčí N2 (soubor neexistuje), rerolly
> paušál 3 (`game_simulator.cpp:153`, `:324`).
> Je to porušení pravidla *„odložené zapsat HNED, s prioritou"* a táž rodina
> chyby jako audit měřicího aparátu: **snímek se vydává za stav.**

## Jak se to čte

| stav | význam |
|---|---|
| **OTEVŘENO** | čeká na práci |
| **BLOKOVÁNO** | čeká na jinou položku, uveden blokátor |
| **ODLOŽENO** | vědomě, uveden **spouštěč** návratu |
| **UZAVŘENO** | hotovo, uveden commit / doklad |
| **ZAMÍTNUTO** | změřeno a rozhodnuto nedělat, uveden důvod |
| **?** | neví se — **ověřit, než se podle toho plánuje** |

⚠️ **Jediná část, která se přepisuje, je poslední oddíl „CO JE TEĎ PRVNÍ".**
Vše ostatní jen mění stav ve sloupci. ID se **nikdy nepřečíslovávají** —
uživatel na položky ukazuje číslem.

---

# 1. REPERTOÁR *(rozhovor; nesoupeří o strojový čas)*

| ID | co | stav |
|---|---|---|
| T1.1 | Bilance soupeřova kola — chybějící *dimenze*, ne měření | **UZAVŘENO** — spec ČÁST 13, `evidence/exposure_scan_20260812.md`; dalo E1 (`REACH0=0`) a E2 (`FB2≤1`) |
| — | Big Guy soupeře | **UZAVŘENO** — spec ČÁST 10 (S-BG.1–6, Z15, Z16). *Nebyla na žádném seznamu; vyšla z rozhovoru.* |
| T1.2 | Chybějící situace: kolo po turnoveru · po obdrženém TD *(doktrína záporné rezervy je v paměti, ne v katalogu)* · utrácení rerollů · hranice poločasu · počasí | **OTEVŘENO** |
| T1.3 | Odehraná situace na **S5** | **ZAMÍTNUTO** — číslo „96 % dosažitelnost / 53 % pokus" bylo z korpusu 30.07. (n=53, engine o 6 oprav starší). Dnes pokus v **89 %** kol S5, čistá vada volby 2,7 %. |
| T1.3′ | Odehraná situace na **S7 (boxing-in)** — **32,4 % kol**, jediné robustní číslo rozložení | **PROBÍHÁ** 14.08. |
| **T1.8** | ⭐⭐⭐ **S7.3 „vytlačit k lajně" NENÍ bezpodmínečné — podmínkou je NÁŠ ODSUNOVÝ ROZPOČET, ne vzdálenost** *(uživatel 14.08.)*. Formulace: **tlač k lajně, když `vzdálenost k lajně ≤ odsuny, které vyrobíme V JEDNOM KOLE`** *(uživatel upřesnil 14.08.: „za jedno kolo")*. **Přes kola se to nesčítá** — mezi našimi koly soupeř zahraje a odejde, takže nedokončený výtlak je zahozená práce, ne rozdělaná; jinak je to zbožné přání — *„u GR to má smysl jen tam, jinak uteče"* (MA9 proti 1 poli za blok).<br>⭐ **Frenzy je v tom čitateli za dva** (blok s odsunem si vynutí druhý blok) ⇒ uživatelův protipříklad: *„jiná situace je, když hrajeme za Khorny a máme spoustu bloků s Frenzy — tak postupně protihráče odsuneme z prostředka až na kraj ven."*<br>⇒ **Trpaslík má Frenzy jen na 2 Troll Slayerech** (`roster.cpp:85`) ⇒ rozpočet ~4 pole, a to jen když oba dojdou. **Pro nás se pravidlo smrskne na 1–2 pole u lajny — což je přesně S7.6.** S7.6 tedy není samostatná povinnost, je to **náš speciální případ obecného pravidla**.<br>⚠️ **Implementovat obecné pravidlo, ne jeho trpasličí výsledek** ([[feedback_implement_the_rule_not_the_outcome]]) — pak vypadne khornský i trpasličí případ zadarmo a nebude v kódu zadrátovaná konstanta „1–2 pole".<br>⇒ Rozsuzuje i **O3** (web radí svádět do středu, doktrína tlačí k lajně): nejsou to protiklady, jen dvě strany té podmínky. | **OTEVŘENO** — zapsat do spec jako oprava S7.3/S7.6 a O3 |
| T1.4 | [8]/O2 **předání míče zapsat jako povinnost** — rozhodnuto („skoro nikdy"; „záloha = potenciální nosič"), nezapsáno | **OTEVŘENO** |
| T1.4′ | O6 **nouze: prorazit vs. držet** — S4 = 27,7 % kol, doktrína NEROZHODNUTÁ | **OTEVŘENO** |
| T1.5′ | **S5.3/S5.4 zajištění sběru** — záloha u míče 22,2 %, nosič krytý po sběru 28,8 %. *Sbírat umíme, pojistit sběr ne.* | **OTEVŘENO** |
| T1.6 | [4] Blitzer končí na lajně — na lajně má stát tělo, které NENÍ určeným rohem klece | **OTEVŘENO** |
| T1.7 | Rozpočet těl pro R3 — 12.08. prošla náhodou; R1 spolyká všechna pohyblivá těla | **OTEVŘENO** |

# 2. MĚŘICÍ APARÁT

| ID | co | stav |
|---|---|---|
| — | Audit aparátu: 7 míst, kde se měřilo něco jiného; `Check(ok,n,deg)` + N/A | **UZAVŘENO** — `dd295e5`, `e4b99ee`, `95e0223` |
| T2.1 | **N2 rozhodčí** `diag_turn_referee_20260811.py` — konec kola z `turnLogs[i+1]`, klasifikace S0–S10, karta kola s `proc` | **OTEVŘENO** — ⚠️ ověřeno 14.08.: soubor **neexistuje**. Základní nástroj, na kterém stojí víc kontrol. |
| T2.2 | K29–K33 pro ČÁST 9 (R1–R4) | **UZAVŘENO** — `diag_rules_checks_20260812.py` |
| T2.3 | N3 sestavy (na kolo / drive / zápas / rasu) | **OTEVŘENO** |
| T2.4 | K28 rozložení S0–S10 | **UZAVŘENO s výhradou** — spec ČÁST 12; hranice S2/S3/S4 stojí na `paceAch`, který je 0.0 (`NOT_CONSULTED` 100 %) ⇒ robustní je jen S7 |
| T2.5 | N4 kalibrace proti uživateli — 20 kol, shoda ≥ 18/20, **než se agregátu uvěří** | **OTEVŘENO** |
| T2.6 | **X2 + X3** (kostky bloku · deklarovaná makra s pořadím) — jedna oprava odemkne **Z4, Z5, Z9, Z14, S2.14, S10.3** | **OTEVŘENO** — nejlepší poměr odemčeno/cena v celém aparátu |
| T2.7 | [14] diag binárky staticky — jinak stará binárka tiše měří jiný engine | **OTEVŘENO** |
| T2.8 | E1/E2 jako K34/K35 | **UZAVŘENO** — `bc9cf17` |
| T2.9 | **K36 `LOCKED`** — zamčená vlastní těla jako chybějící člen tempa | **UZAVŘENO** — `bc9cf17`; potvrzeno na 3000 hrách, monotónní: ≤2 → Δx **+2,18** (n=15314) · 3–5 → **+1,89** (n=2162) · 6–8 → **+1,06** (n=50) |
| K9b | — | **BLOKOVÁNO** na T3.1 (`resistance` je 0, plánovač se nezeptá) |
| K32 | blitz se v logu nepozná od bloku | **BLOKOVÁNO** na X1 |
| T0.1 | Srovnat K9 se S2.7 — kontrola měří konstantu, povinnost přikazuje funkci odporu. Tichá chyba. | **?** — mohlo padnout v auditu 13.08., neověřeno |
| T0.2 | Vyškrtat uzavřené: O3, O4, O5, X6 | **?** — neověřeno |

# 3. TEMPO A POSTUP

| ID | co | stav |
|---|---|---|
| T3.1 | Brána klece — veto jen při `achievable == 0` + cage-fill | **ZAMÍTNUTO** — 1500 párů, dw-we **−0,0297 (−2,0σ)**, dw-sk +0,008. ⚠️ Nezahazovat kód: zlepšila skoro všechny kontroly, vyměnila tempo (20,6→28,4 %) za bití (76,1→73,2 %) a čistotu rohů (79,4→72,6 %) ⇒ nula. **Chybí jí plán trasy, ne schopnost.** |
| T3.2 | [1] Kontrola `c085331` (exposure = uživatelovo R1) — přeměřit K7 na korpusu PO opravě klece | **BLOKOVÁNO** — viz A2 |
| A2 | ⭐ **Exposure/R1 vyzvednout z `cage_advance.cpp` do obecného pohybu** | **OTEVŘENO** *(nové 14.08.)* — ověřeno: `c085331` sahá jen do `cage_advance.cpp`, ten se instancuje výhradně při `config.cageAdvance` (`macro_mcts.cpp:907`) ⇒ **hotová práce nejde zapnout nezávisle na zamítnuté bráně.** Je to tvoje vlastní pravidlo *„BLITZ pohyb → obecný pohyb"*. |
| T3.3 | [2] Tempo cílit na 3,14, ne 2,61 | **PŘEFORMULOVÁNO** → P3 (rovnoměrná podlaha je špatný model) |
| T3.4 | [3] Změřit `41c3570` — kdo nese, kdy je první držení | **UZAVŘENO** — korpus 3000: Runner **88–91 %** ve všech kategoriích, Longbeard 1–4 % |
| T3.5 | [5] Na lajně stojí 4 hráči místo 3 — dává soupeři blok navíc; je to v poli formace, ne v přiřazení | **OTEVŘENO** |
| **P11** | ⭐⭐⭐ **ENGINE SKÓRUJE, JAKMILE MŮŽE — a trpaslík nemá.** *(uživatel 14.08.: „kdyby Runner s míčem utekl a skóroval dříve, je to také špatně, protože soupeř dostane čas na re-TD"; a upřesnění: „ujet víc polí není špatně — ale je špatně skórovat dříve".)*<br>**Cíl není dojít co nejdřív, cíl je překročit čáru tak pozdě, aby soupeř neměl čas odpovědět.** Doktrína záporné rezervy tohle říká od 10.08., ale **v enginu není nikde**.<br>**Tři vady:**<br>**①** `greedyMacroRank(SCORE) = 100` (`macro_mcts.cpp:39`) — nejvyšší prior ze všech maker, bezpodmínečně.<br>**②** `scoringBonus += 0.4` za „safe walk-in" (`:702`) a `+0.8` v posledním kole (`:724`) — odměna za *„umím teď dojít do endzony"* nemá podmínku na zbývající kola soupeře. **Chybí zdržovací člen** *„umíš, ale ještě ne"*.<br>**③** `pacing` (`:712`) trestá `dist < idealDist`, tedy **předstih ve vzdálenosti** — a to je špatně: víc ujetých polí je vždy zisk, dá se stát pole před čárou a čekat. Trestat se má **předčasné překročení čáry**, ne pozice.<br>⛔ **Opravuje mou vlastní úvahu z téhož dne:** navrhl jsem „K9a přepsat na koridor" (trestat i předstih) a uživatel to zamítl. **K9a zůstává jednostranné**; místo něj vzniká samostatná kontrola **„v kolikátém kole jsme skórovali a kolik kol zbylo soupeři"**.<br>⇒ Týká se **každého drivu, který dojde k endzoně** — ne okrajové situace. | **OTEVŘENO — VYSOKÁ PRIORITA**, kandidát na víkend |
| **P12** | **Fáze 3 (sólo výběh) má vlastní podmínku puštění — PRONÁSLEDOVÁNÍ, ne vzdálenost** *(uživatel 14.08.: „taky záleží na tom, kolik soupeřů pak Runnera doběhne v poslední fázi")*. Runner MA6 proti Gutter Runner MA9 / wood-elf Catcher MA8 / ork MA5–6.<br>⇒ proti **wood-elfovi a skavenovi** musí být výběh **krátký** (klec donese míč skoro až tam), proti **orkovi a humanovi** může být delší.<br>⭐ **Dává mlácení účel v TRASE, ne jen v attrition:** *každý sražený pronásledovatel prodlužuje fázi 3.* Dosud se bití obhajovalo jen statisticky (+2,7σ na TD) a nikdo neuměl říct, k čemu je v plánu drivu. ⇒ Priorita blitzu má možná mířit na **ty, kdo umí doběhnout Runnera**, ne na rohy klece. Měřitelné na stávajícím korpusu. | **OTEVŘENO** — třetí parametr P3 |
| P3 | ⭐ **Fázový plán trasy** — sólo Runner + kick-off return → klec → sólo výběh u endzone. Rozvrh pozpátku od TD **po fázích**. Vstup pro `classifyTurnGoal` i `K9a`. **Bez fáze v modelu nejde odlišit chybu od záměru.** | **OTEVŘENO** |

# 4. ROZHODOVÁNÍ ENGINU *(živé chyby)*

| ID | co | stav |
|---|---|---|
| **P9c** | ⭐⭐⭐ **ÚČEL BLOKU NA POLLUTERA JE ODKLIDIT HO OD ROHU** *(uživatel 14.08.: „priorita u špinavého rohu je odklidit protihráče pryč od rohu — ne jej nechat u rohu a posunout blíž k balonu")*. **Není to kompromis, je to pořadí:** odsun, po kterém polluter roh **pořád špiní**, není částečný úspěch — je to **selhání akce**, protože roh byl jediný důvod ji dělat. A když ho takový odsun navíc přiblíží k nosiči, je to **záporný obchod**.<br>⇒ Řazení cílových polí při bloku na pollutera: **① přestane sousedit s rohem** *(to je účel)* → **② nepřiblíží se k našemu nosiči** → ③ zbytek.<br>⚠️ **Váže to i výběr blokujícího, ne jen směru:** tři nabízená pole jsou dána vektorem `polluter − blokující`, takže **kdo udeří, určuje, kam se dá odsunout**. Blokující se má vybírat tak, aby pole splňující ① vůbec existovalo.<br>⭐ **Nepotřebuje nové logování** — je to otázka na začátek kola (pozice pollutera, rohu, nosiče a kandidátů na blok), ne na průběh. Jde spočítat na **stávajícím korpusu**. | **OTEVŘENO — VYSOKÁ PRIORITA**, měřitelné hned |
| **P9** | ⭐⭐⭐ **SMĚR ODSUNU SE VYBÍRÁ SLEPĚ — a je to společný kořen dvou dnešních nálezů.** CRP FAQ: *„The coach of the moving team decides all pushback directions unless the pushed player has Side Step."* Máme tedy volbu ze **tří polí** (`getPushbackSquares`) a `choosePushSquare` (`block_handler.cpp:113`) ji zahodí: `score = count - i` = **„rovně dozadu první"**, čistě geometricky. Cílové pole se **nikdy nehodnotí** — nedívá se na nosiče, na klec, na endzonu ani na tackle zóny. Heuristiky existují jen pro Side Step a Grab.<br>⇒ **Každý náš odsun je volné přemístění soupeře, a tu volbu zahazujeme.**<br>⭐ **Nejde jen o geometrii, ale o OBSAZENÍ** *(uživatel 14.08.: „je důležité kdo stojí na a — jestli náš nebo soupeř")*. Když prázdné pole není, odsun **řetězí** a druhý článek je ten, kdo tam stojí. Kód rozlišuje jen prázdné/neprázdné (`anyEmpty`) a pak jede straight-back — **komu to tělo patří, neřeší**. Žebříček cílového pole podle obsazení:<br>• **soupeř** → dobré, řetěz posune **dva jejich** ⇒ když je vedle straight-backu pole se soupeřem a straight-back řetězí přes nás, je současná volba **striktně horší** ⇒ patří k **P9a**, ne k doktríně<br>• **prázdné** → výchozí<br>• **naše řadové tělo** → malá cena<br>• **náš roh klece** → vysoká cena *(úder, který měl roh očistit, ho rozebere)*<br>• **náš nosič** → veto<br>⇒ 44,2 % odsunových polí je obsazených, takže tohle není okrajový jev.<br>**Dopad 1 (uživatel 14.08.):** při čištění rohu blokem *„může odsun nechat soupeře nejen jako stojícího souseda rohu, ale nově navíc i souseda ballcarriera"*. Bije to do **27,2 %** bloků, kde polluter zůstane stát (Fable: na zemi je 72,8 %). A míří to na `REACH0`, což je podle E1 rozdíl mezi **1,8 %** a **33 %** ztráty míče.<br>**Dopad 2:** ranní **8 darovaných TD** ve 3000 hrách má týž kořen — ověřeno na `g0289`: pusher (23,8), nosič (24,7), „rovně dozadu" = **(25,6) = endzona**. Nebyla to smůla, byla to ta konstanta. | **OTEVŘENO — VYSOKÁ PRIORITA** *(blokuje bezpečné nasazení P2)* |
| **P10** | ⭐⭐⭐ **HODNOTA BLOKU SE NEODVOZUJE OD CÍLE — a nosič se odměňuje za MARKOVÁNÍ, ne za sražení.** *(uživatel 14.08.: „když je vedle našeho Longbearda možnost block na GR s míčem a navíc jsou kolem naši — co může být lepšího než jej blocknout?")* Odpověď: nic — a engine to neví.<br>**① Prior je plochý:** `greedyMacroRank` (`macro_mcts.cpp:47-48`) dává `BLITZ 20`, `BLOCK 15` — **jedna hodnota pro všechny bloky**. Blok na nosiče s Tackle a 3 kostkami má týž prior jako blok na linemana v protilehlém rohu. Kategorie „udeř na míč" v žebříčku není.<br>**② Tři existující členy o nosiči odměňují jen STÁNÍ VEDLE:** marking `+0,08×min(TZ,3)` (max +0,24, `:776`), sideline trap `+0,10` (`:808`), contain-vs-AG≥4 `+0,06×…` (max +0,12, `:819`). **Sražení nosiče nemá člen žádný.**<br>⚠️ **DOPOČÍTÁNO 14.08. — podezření POTVRZENO a je silnější, než vypadalo.** Všechny tři členy o nosiči visí na `ball.isHeld`, takže sražením **zmizí naráz** (−0,24 −0,10 −0,12), a místo nich naskočí `heuristic -= 0.1  // loose ball is bad` (`:762`) — ⭐ **který nerozlišuje „upustili jsme ho" od „právě jsme ho soupeři vyrazili".**<br>Bilance členů, které se mění (soupeřův nosič, 3 naše TZ, AG4):<br>• **uprostřed hřiště** (12 polí od endzony): **+0,13 → −0,02 = −0,15**<br>• **u lajny** (y=2): **+0,23 → −0,02 = −0,25**<br>• **může skórovat** (8 polí, MA9): −0,31 → −0,02 = **+0,29** ✅<br>⇒ **Čím blíž je soupeř skórování, tím víc heuristika blok chce; uprostřed hřiště se mu aktivně brání.** A S7 boxing-in = 32,4 % kol je právě ten střed.<br>⚠️ Poctivě: je to **listová evaluace**, MCTS to může přebít hledáním (sebráním míče hlouběji ve stromě). Netvrdím „AI nikdy nebije nosiče" — tvrdím, že úspěšný výsledek akce se hodnotí hůř než výchozí stav.<br>⇒ Sedí na starý nález *„trpaslík markuje a bije, nehoní"*: markovat jsme ho naučili, bít nedopsali.<br>⛔ **PODMÍNKA, BEZ KTERÉ JE OPRAVA ŠPATNĚ** *(uživatel 14.08.: „zkontroluj před blitz Wardancera na balon, že máš v záloze druhého pro pickup a třetího pro zablokování cesty k uzmutému balonu")*: **vyražený míč je zisk jen tehdy, když ho posbíráme.** Jinak jsme vyrobili volný míč uprostřed hřiště a dali ho rychlejšímu týmu — a trpaslík je v souboji o volný míč nejhorší možná rasa (MA4, AG2 u většiny těl).<br>⇒ `loose ball is bad` **není nesmysl, je to správné pravidlo se špatnou podmínkou**: platí, když scramble prohrajeme, neplatí, když ho vyhrajeme. ⇒ Rameno **nesmí** znít „bij nosiče", ale **„bij nosiče, když scramble vyhrajeme"**, a to je **rozpočet tří těl**: ① kdo srazí · ② kdo sebere · ③ kdo zavře cestu.<br>⭐ **Pravidlo je BEZPODMÍNEČNÉ a rasa soupeře o něm nerozhoduje.** Uživatel je řekl dvakrát a pokaždé stejně — u Longbearda proti GR jako součást zadání (*„a navíc jsou kolem naši"*), u Wardancera jako kontrolu. Rychlost soupeře neurčuje, **jestli** pravidlo platí, jen **jak těsně se ta trojice počítá**: proti Wardancerovi musí být třetí tělo blíž a cesta zavřenější, protože je na míči dřív. ⛔ *(Zapsal jsem to nejdřív jako dva protikladné případy lámající se podle rychlosti soupeře — to byl můj konstrukt, ne jeho pravidlo. Opraveno.)*<br>⇒ Dnešní člen se ptá **jen na nás** (`nearestDist` našeho nejbližšího, max +0,08) — nikdy na to, **kdo je blíž, my nebo oni**, a tělo zavírající cestu nemodeluje vůbec. Souvisí s [[project_bloodbowl_loose_ball_denial_doctrine_20260807]].<br>⛔ **POZOR NA ZÁMĚNU SITUACÍ** *(uživatel 14.08.)*: „nosič" a „polluter" **nejsou dva cíle v jednom kole**, jsou to cíle ve **dvou různých situacích**. Roh klece existuje jen v **našem kole s míčem** (S2–S5); soupeřův nosič jen v **obranném kole** (S7/S8). Prior na blok podle cíle proto **musí být indexovaný situací**, ne jeden plochý žebříček — jinak se opakuje táž chyba o patro výš. ⇒ P10 se dělí: **P10a** blok na soupeřova nosiče *(obrana)* · **P10b** blok na pollutera *(útok)*. **P10b NENÍ levnější cesta k P2, je to P2.** | **OTEVŘENO — VYSOKÁ PRIORITA**, kandidát na víkend |
| P4 | **`CHAIN_SCORE` je aktivní bug** — krok 1 (pass) spálí `passUsedThisTurn`, krok 2 (hand-off) se pak nenabídne ⇒ přihrávka se provede, předání selže, **tah je pryč**. Opravit nebo odstranit. | **OTEVŘENO** |
| P5 | **Hand-off pro výměnu nosiče** — filtr váží předání cenou přihrávky (33 %), i když by ho provedl jako hand-off (83 %); práh 0,5 zahodí i Runner→Runner (44 %) ⇒ nenabízí se žádné předání. Kritérium: **„nosič je špatný"** (AG≤2 bez Sure Hands a nedoběhne), ne „příjemce je lepší". Patch: `scratchpad/handoff_fix_plan.md` | **OTEVŘENO — POVÝŠENO uživatelem 14.08.** („spravil bych ho dřív"). Zisk je pod šumovým dnem (Longbeard nese 1–4 % kol) ⇒ **neměřit A/B, opravit a ověřit na kontrolách.** |
| P6 | **Zobecnit item 14 na výběr cíle a na pickup** — BLITZ vybírá cíl podle surových kostek (blitzera podle kostek + cesty); PICKUP váží cenu sebrání, ne cestu k míči (připouští 2 GFI = 30 % pád). Nástroj existuje: `estimateApproachFailChance` (`macro_actions.cpp:206`), použitý jen 2× a oba u blitzu. Porušené vlastní pravidlo z 03.08. | **OTEVŘENO** |
| P8 | **Výběr cíle faulu** bere prvního ležícího v pořadí sousedních polí, nehodnotí nic. Přitom Gutter Runner je 4,4× lepší cíl a Thick Skull se nefauluje. | **OTEVŘENO** |
| T4.1 | Záloha u míče = potenciální nosič (Blitzer AG3 MA5) | **OTEVŘENO** |
| T4.2 | Ověřit bránu přihrávek na korpusu — dopočítané, ne změřené | **OTEVŘENO** |
| T4.3 | Priorita blitzu: zeď kupředu → odmarkovat nosiče → příležitost | **OTEVŘENO** — P2 je jeho zostření |
| P2 | **Doktrína „BÍT TOHO, KDO ŠPINÍ ROH" — ODBLOKOVÁNA a PŘEPSÁNA** (P0.1 + P0.5 + P0.6 uzavřeny 14.08.). Ne „blitz na roh" a ne „bít víc", ale: **① priorita BLOKU na pollutera s volným stojícím sousedem** — pokrývá 61 % polluterů, blokovaný polluter je v 72,8 % na zemi a v 92 % přestane roh špinit, neblokovaný špiní dál v 64 %. **② Blitz na roh jen jako záloha**, když soused není (39 %) a blitz nepotřebuje nosič. **③ R4 „tělo bez úkolu" dostane úkol *dojdi k polluterovi / postav se na asistenci*** — 2,14 idle těl/kolo, 94,7 % dosáhne. **④ NEzvedat obecný počet bloků** kvůli rohům (−4,5σ). ⚠️ **⑤ Blokující se vybírá podle GEOMETRIE ODSUNU, ne jen podle dostupnosti** *(uživatel 14.08.)* — těch 61 % počítá, kdo **může** udeřit, ne jestli výsledný odsun **odklidí pollutera od rohu**, a při 27,2 % bloků zůstane stát. ⇒ **Fableho 61 % je horní mez a potřebuje přísnějšího nástupce:** *podíl polluterů, u nichž existuje volné stojící tělo, z jehož pozice aspoň jedno ze tří odsunových polí pollutera od rohu odklidí a nepřiblíží ho k nosiči.* Teprve tohle číslo smí řídit P2. Viz **P9c**. | **OTEVŘENO — ČEKÁ NA P9c** |
| P0.6 | ⭐ **Co očištění rohu STOJÍ — rozpočet, ne jen výnos** *(uživatel 14.08.: „když použijeme blitz na očištění rohu, bude nám chybět pro prolomení zdi — omezený počet zdrojů = hráčů a jeden blitz na kolo")*. **Blitz je 1/kolo, blok je bez limitu** — kdo už sousedí, udeří zadarmo; blitz je potřeba jen tehdy, když se k cíli musí DOJÍT. ⇒ klíčové číslo: **jaký podíl špinavých rohů jde očistit BEZ blitzu**. Když je vysoký, konflikt se zdí mizí a P2 se má formulovat jako priorita **bloku**, ne blitzu. Dál: cena, když blitz nutný je (Δx v N a N+1 podle toho, na co blitz padl) · jsou ta **1,77 idle těla** (K31) na roh vůbec použitelná, nebo jsou zamčená (K36)? | **UZAVŘENO** — Fable 14.08. ⭐ **61,1 % polluterů má volného stojícího souseda** ⇒ blok zdarma, bez blitzu (35,0 % na ≥2k); na úrovni kol **69,2 %**. **Konflikt „roh vs zeď" se z většiny rozpouští.** Když blitz na roh padne: Δx **+1,80 vs +2,52** u blitzu do zdi (−6,4σ, ~−0,7 pole) a **žádný výnos potom**; **45,5 % dnešních blitzů na roh padlo v kolech, kde blok zdarma šel** = vyhozený rozpočet. **Těla jsou:** v 52,6 % kol s polluterem ≥1 idle (2,14/kolo), **94,7 % na pollutera dosáhne** ⇒ nedostatek je **alokační**. „Kdy roh vs zeď" podle fáze: **NEROZHODNUTO**, rozdíl nula, práh si nevymýšlel. |
| T4.4 | Generování chain pushe | **ZAMÍTNUTO** (Fable 12.08.) — tempo RAW 0,345 / vážené 0,294 (brána 0,3, a je to HORNÍ odhad); únik 0,29/10 kol proti brance 1/10 ⇒ **řádově chybí**. Beneficient je v 78 % sám filler, nosič v 1,6 %. Podmíněná výjimka: skaven 0,379 / human 0,317 projdou i váženě. |
| T4.5 | Jump Up: Block Action vleže (+2) | **ODLOŽENO** — **spouštěč:** až budeme hrát roster, který Jump Up má |

# 5. PARITA S CRP

| ID | co | stav |
|---|---|---|
| — | Push back hledá prázdné pole, řetěz pokračuje | **UZAVŘENO** — `fea042c` |
| — | Stand Firm zastaví řetěz + follow-up nešlape na neuvolněné pole | **UZAVŘENO** — `0ec69f3` |
| T5.7 | Dauntless se řeší PŘED asistencemi | **UZAVŘENO** — `9f98070` |
| T5.10 | ⚠️ **Týmové rerolly jsou paušál 3** pro všechny rasy (`game_simulator.cpp:153`, `:324`), ačkoli roster veze `rerollCost` a liší se (dwarf **40**, skaven **60**) ⇒ trpaslík by si za týž rozpočet koupil **víc**. **Nepřesnost v NÁŠ NEPROSPĚCH.** Bolí u AG2 dodge (50 % vs 75 %) a u GFI (turnover 16,7 % vs 2,8 %). | **OTEVŘENO** — ⚠️ ověřeno živé 14.08. |
| T5.11 | Náš **herní nástroj rerolly vůbec nesleduje** ⇒ odhady rizika v ručních partiích jsou konzervativní horní mez | **OTEVŘENO** |
| T5.12 | **TD v soupeřově kole nestojí skórujícího tým kolo.** CRP *(„Scoring in the opponent's turn")*: kdo skóruje tím, že je odsunut do endzony, *„must move their Turn marker one space along the Turn track"*. `action_resolver.cpp:207–213` bod připíše a značku tahu **neposune**. | **OTEVŘENO — NÍZKÁ PRIORITA** *(uživatel 14.08.: „zajímavé pravidlo s menším výskytem")*. Výskyt 15 z 2183 TD (0,7 %), bilance **8× proti nám / 7× pro nás** ⇒ prakticky neutrální. ⚠️ Ale viz výhrada níže — 8 je **podlaha, ne strop**. |
| — | **Doktrína: nikdy netlačit soupeřova nosiče směrem k JEHO endzoně.** Blok, který odsune nosiče do endzony, kterou útočí, mu **daruje TD** (CRP to výslovně umožňuje). 8× ve 3000 hrách. | **OTEVŘENO** — zapsat jako zákaz do spec (patří k prioritě blitzu, T4.3/P2) |
| P7 | **Sdílený limit pass / hand-off** — CRP má dva nezávislé limity, engine jeden (`passUsedThisTurn`). ⚠️ **Jediný nález, který po opravě hraje PROTI nám** ⇒ měřit zvlášť. | **OTEVŘENO** |
| T5.2 | [6] **Kick-Off Return** — 3 vady | **OTEVŘENO — POVÝŠENO** *(fáze 1 fázového plánu na něm stojí, přestává být okrajové)* |
| T5.1 | [7] Tabulka výkopu — 5 z 11 výsledků vadných; 22 % výkopů zahazuje volné tempo | **ODLOŽENO uživatelem** — **spouštěč:** až se bude řešit fáze 1 / kick-off (tedy spolu s T5.2) |
| T5.3 | Zranění nepřetrvávají přes drive — každý TD staví 11 čerstvých ⇒ attrition čísla měří jen poslední drive | **OTEVŘENO** |
| T5.8 | Frenzy druhý blok se v hodnocení příležitostí nikde nemodeluje — týká se Slayerů | **OTEVŘENO** |
| **T5.14** | ⚠️ **MIGHTY BLOW SE PŘIČÍTÁ K OBĚMA HODŮM — ŽIVÁ CHYBA, HRAJE PROTI NÁM.** CRP: *„you only modify **one** of the dice rolls, so if you decide to use Mighty Blow to modify the Armour roll, you may not modify the Injury roll as well."* Engine (`block_handler.cpp:673`) nastaví **oba**: `defCtx.armourModifier += 1; defCtx.injuryModifier += 1;`. Má to být **volba kouče**, ne součet.<br>**Claw je naopak správně** (`injury.cpp:181`: `armourRoll` už obsahuje modifikátory ⇒ „8+ po modifikacích" sedí) — a právě proto se chyba znásobuje: `+1` legitimně pomáhá dosáhnout osmičky pro Claw **a navíc** neoprávněně zvedá zranění.<br>**Dopad:** MB má v TV1200 **ork** (Blitzer), **human** (Blitzer + Ogre), **wood-elf** (Treeman); **skaven ani trpaslík ani jednoho** ⇒ **chyba jen bije nás**, ve 3 ze 4 matchupů. Táž rodina jako T5.7 Dauntless, jen opačným směrem. | **OTEVŘENO — snadná oprava, měřit zvlášť** |
| **T5.15** | **Piling On není implementovaný** — `SkillName::PilingOn` je v `enums.h:115`, ale v celém `engine/src/` se nevyskytuje. Mrtvá hodnota enumu. Dnes neškodí (nikdo v TV1200 ho nemá), ale je to druhá polovina **CLAWPOMB** a ožije s **T5.13**. | **ODLOŽENO** — **SPOUŠTĚČ: spolu s T5.13** |
| **T5.16** | ⭐⭐ **KTEROU EDICI VLASTNĚ MODELUJEME? — nikde to není napsané.** Náš zdroj `rules_crp2016.txt` se sám představuje jako *„BLOOD BOWL **COMPETITION RULES** … Competition Rules pack"*; řetězce `BB2016`, `Death Zone`, `Living Rulebook` v něm **nejsou ani jednou**. ⇒ **Je to CRP/LRB6 a „2016" v názvu je matoucí** — i paměť ho vede jako „CRP/LRB6 (BB2016)", což slučuje dvě různé edice.<br>**Kde se to rozchází** *(uživatel 14.08.)*: v **CRP je Piling On zdarma** (přehodí brnění nebo zranění, hráč jde prone); v **BB2016 stojí týmový reroll**, což CLAWPOMB prakticky zabilo. Kombinace Claw+MB je v obou stejná — na AV9 stačí 7+, protože `+1` z MB pomáhá k osmičce, kterou Claw vyžaduje.<br>⛔ **ROZHODNUTO — a bylo rozhodnuto UŽ DŘÍV.** Uživatel 14.08.: *„já jsem minule už hlásil, že chci pravidla 2016."* **Cílová edice je BB2016.** Rozhodnutí padlo dřív a **nikde se nezapsalo** — paměť vede zdroj jako „CRP/LRB6 (BB2016)", takže se ty dvě edice slily a stáhl se CRP. ⇒ **Všechny pravidlové audity od 07.08. běžely proti ŠPATNÉ edici.** Táž rodina chyby jako ztracených ~20 položek fronty: rozhodnutí bez zápisu. | **UZAVŘENO rozhodnutím** → viz **T5.17** |
| **T5.17** | ⭐⭐⭐ **OPATŘIT TEXT BB2016 A PŘEAUDITOVAT ROZDÍL.** Cílová edice je **BB2016** (T5.16), zdroj `rules_crp2016.txt` je **CRP/LRB6**. Kroky:<br>**①** sehnat autoritativní text BB2016 (Death Zone Season 1/2 + BB2016 rulebook) týmž postupem jako 07.08. — stáhnout PDF, rozebrat `pypdf`, grepovat; **nespoléhat na AI, ta edice míchá** (to je přesně, co se stalo);<br>**②** přejmenovat současný soubor na `rules_crp_lrb6.txt`, ať název nelže;<br>**③** **sestavit seznam rozdílů CRP → BB2016** a projít proti němu všech dosud auditovaných ~15 pravidel (dodge, leap, TakeRoot, Wild Animal, pickup, catch, GFI, Dauntless, Stand Firm, řetěz odsunu, TD v soupeřově kole, Tackle, Sprint, Claw, Mighty Blow);<br>**④** známý rozdíl zatím jeden: **Piling On stojí v BB2016 týmový reroll** (v CRP zdarma) ⇒ CLAWPOMB je v 2016 prakticky mrtvý.<br>⚠️ **Dokud ③ neproběhne, nevíme, kolik našich „oprav podle pravidel" opravilo něco jiného, než jsme chtěli.** | **OTEVŘENO — VYSOKÁ PRIORITA** *(podmiňuje důvěru ve všechny pravidlové audity)* |
| T5.4 | [13] M1 přeběhnout — smazat falešný `M1_DONE`, přestavět `diag_m1` | **OTEVŘENO** |
| T5.5 | O1 **kopat, nebo přijímat** — volbu vůbec nemodelujeme; potenciálně větší páka než cokoli uvnitř kola | **OTEVŘENO** |
| T5.6 | O7 Underworld | **OTEVŘENO** |
| **T5.13** | **Přestavba testovacích rosterů — OTT skaven** *(uživatel 14.08.)*. Dnešní TV1200 skaven má GR s Dodge+Sure Feet (**11 polí/kolo**) a žádného Rat Ogra. Jednokolová hrozba potřebuje **`+MA` + Sprint**: MA10 + 3 GFI = **13 polí**. ⭐ **Nepotřebuje ani jeden double** — Sprint i Sure Feet jsou Agility a GR má A v normálním přístupu (CRP: `Gutter Runners 80,000 9 2 4 7 Dodge · GA · SPM`), `+MA` je zvýšení statu. ⇒ **Není to exotický roster, je to nejběžnější cesta vývoje GR.**<br>Rat Ogra **ne**: 160k, mutace jen na double, a bash-skaven se překrývá s orkem, kterého už měříme.<br>Engine je připravený — `Sprint` funguje (`pathfinder.cpp:34,113` dává `maxGfi=3`), změna je **jeden řádek v rosteru**.<br>⚠️ **Proč to nejde teď:** změna soupeře rozbije srovnatelnost s korpusem 3000 her i se všemi dosavadními A/B.<br>⛔ **ODLOŽENO uživatelem 14.08.** *(„to přestavování týmů máme v plánu na později")*. **SPOUŠTĚČ: až doběhnou víkendové běhy a bude se smět měnit baseline.** Otázka, kterou to má zodpovědět: *„obstojí naše doktrína proti soupeři, který umí odpovědět v jednom kole?"* — protože **proti OTT buildu není bezpečné ani kolo 8**, a tím se bortí dostatečnost P11. | **ODLOŽENO** |

# 6. DOMĚŘENÍ NA VELKÉM KORPUSU *(korpus stojí hotový, strojový čas nestojí)*

Korpus: `diag_replay_mine_20260813_big_data`, **3000 her**, brána OFF, HEAD `e4b99ee`,
44 177 našich kol. Doklad `night_big_20260813/`.

| ID | co | stav |
|---|---|---|
| P0.1 | **Předpovídá blok v kole N čistotu rohů v N+1?** | **UZAVŘENO** — Fable 14.08., `evidence/fable_dirty_corner_chain_20260814.md`. **ANO, ale jen ADRESNĚ:** sražený polluter → špinavé rohy v N+1 **0,27 vs 1,00 (−22,9σ**, n=3864), Δx **+1,62 vs +0,76** (+9,7σ). ⚠️ **Obecné bloky čistotu ZHORŠUJÍ** (−4,5σ; pre-registrace čekala opak) a pomáhají jen tempu (+6,1σ). ⇒ *„bít víc" je špatná rada, „bít toho správného" správná.* |
| P0.2 | REACH0 **jako počet** (na 195 drivech jen −1,8σ) | **OTEVŘENO** |
| P0.3 | K36 `LOCKED` — koše měly n=4 až 16 | **UZAVŘENO** — viz T2.9 |
| P0.4 | Skórovací podíl **po fázích** | **OTEVŘENO** — potřebuje P3 |
| P0.5 ✅ | ⭐ **Řetěz „špinavý roh → zamčené tělo → chybějící roh příště"** — koreluje počet špinavých rohů v N s počtem zamčených těl a obsazených rohů v N+1? **Vysvětluje, proč je špinavý roh −2,2σ, i když ztrátu míče nezpůsobí hned: účet nepřijde v kole, kdy se chyba udělá.** Táž chyba má proti různým soupeřům různou splatnost — proti wood-elfovi okamžitě (ztráta míče), proti skavenovi na splátky (zámky). **Odložená varianta je nebezpečnější, protože se u stolu nespojí s příčinou.** *(uživatel 13.08., z vlastní hry)* | **UZAVŘENO** — Fable 14.08. **Řetěz platí, ale účet se platí v TĚLECH, ne v tempu:** tělo ze špinavého rohu je v N+1 v **94,3 % nedostupné** (49,6 % zamčené, 43,5 % na zemi), jako čistý roh poslouží **13,8 %**; špinavé_N → čisté_N+1 −11,4σ, → volná těla −21,3σ i po kontrole hustoty; Δx_N+1 jen −1,7σ ⇒ *pomalost nese hustota, ne roh*. **Splatnost po rasách platí půlkou:** „skaven na splátky" ✅ (−4,3σ, jediná průkazná tempová větev), „wood-elf hned" ❌ — okamžitě účtují **ork a human**; u wood-elfa rozhoduje base REACH0, ne rohy. Odložené složky jsou univerzální ⇒ **pointa potvrzena**. |
| **P0.7** | ⭐⭐⭐ **PRÁZDNÝ ROH JE HORŠÍ NEŽ ŠPINAVÝ** *(uživatel 14.08.: „když X nesrazíme, roh neočistíme — pak volíme, jestli má u rohu zůstat náš hráč, nebo X. Co je pro roh lepší?")*. Změřeno na 3000 hrách, **jen na srovnatelných situacích** (rohové pole, u kterého soupeř STOJÍ VEDLE):<br><br>| stav rohu | n | REACH0 | Δx | **držíme míč v N+1** | opp≤3 |<br>|---|---|---|---|---|---|<br>| prázdný | 7368 | 0,66 | 0,86 | **72,5 %** | 5,04 |<br>| **špinavý** | 4667 | 0,64 | **0,96** | **80,1 %** | 4,87 |<br><br>⇒ **+7,6 pp v držení míče ve prospěch „naše tělo tam nechat"**.<br>**Kontrola konfoundéru** *(roh zůstane prázdný nejčastěji proto, že jsme neměli koho tam dát)* — rozděleno podle zásoby volných těl, efekt **drží ve všech koších**: ≤4 → **+9,2 pp** (59,1/68,3) · 5–6 → **+6,4 pp** (69,6/76,0) · ≥7 → **+6,0 pp** (77,4/83,4). Konfoundér je skutečný (držení stoupá 59→77 % jen podle zásoby), ale rozdíl nevysvětluje. Hustota po koších mírně **v neprospěch** špinavého ramene.<br>⚠️ **Sloupec REACH0 v téhle tabulce NEPOUŽÍVAT** — když v N+1 míč nemáme, není koho měřit a započetla se nula ⇒ měří se jen na kolech, která dopadla dobře (**survivorship**). Důvěryhodné je jen držení míče.<br>⚠️ Pozorování jsou **klastrovaná** ⇒ brát jako **směr, ne jako σ**; na přesné číslo bootstrap po hrách.<br>**Mechanismus (hypotéza, nezměřeno):** roh není „obsazené pole", je to **pole sousedící s nosičem**, a žebříček je `čistý > špinavý > prázdný` podle toho, **co soupeře stojí se tudy dostat k míči**: do prázdného vejde zadarmo, naše tělo musí nejdřív vyblokovat. „Špinavý" popisuje **pozici soupeře, ne vadu našeho těla**.<br>⛔ **OPRAVUJE MOJI DEDUKCI Z TÉHOŽ DNE.** Složil jsem „počet rohů 0σ" + „špinavé rohy −2,2σ" a vyvodil, že prázdný vyhrává. **Ani jeden z těch koeficientů ale neměří volbu, která je na desce:** −2,2σ porovnává špinavý proti **čistému**, 0σ je průměr přes **všechny** prázdné rohy včetně těch bez soupeře vedle. ⇒ **Vzor k zapamatování: složit dva koeficienty z různých analýz není totéž co změřit rozdíl.** | **UZAVŘENO** — potvrzuje žebříček obsazení v P9 |
| P1 | **Přepsat K33 a K34 na spojité** — jako prahy nepředpovídají nic (0,6σ / 0,8σ), jako počty patří k nejsilnějším. Platí i pro E1: *„ani jeden otevřený roh"* je správný **cíl**, ale jako **kontrola** se má měřit `REACH0` jako počet (1 → 8,3 %, 4+ → 33 % ztráty). | **OTEVŘENO** — levné, opravuje metr |
| A1 | **Anomálie: TD flag nesedí se skóre delta** — 9 z 3000 her (0,3 %) | **OTEVŘENO — zadáno uživatelem na 14.08. přes den** |

# 7. AŽ NAPOSLED

| ID | co | stav |
|---|---|---|
| T6.1 | N5 vstřelení plánu priorem `f(rezerva)` — jediná změna chování, až po měřicím aparátu | **OTEVŘENO** |
| T6.2 | **Učení** — až po dokončení repertoáru, s úplnou procedurou jako nulovou hypotézou | **OTEVŘENO** |

---

# ⚠️ VÝHRADY, PODLE KTERÝCH SE NESMÍ NAVRHOVAT

* **Soupeřova AI nehraje proti našim slabinám cíleně.** Runner nevypadne ze
  hřiště ani jednou ve 120 hrách, protože si pro něj nikdo nechodí. Lidský
  soupeř by to dělal (AG3 máme jen 4 z 11).
  ⚠️ **Platí i na naměřené četnosti chyb, ne jen na naše ztráty.** „8 darovaných
  TD ve 3000 hrách" (T5.12) je četnost, jak často do toho **naše AI** shodou
  okolností spadne. Kolikrát by nás do toho dostal soupeř, který tu situaci
  hledá, korpus neříká. **Nízký výskyt v korpusu = podlaha, ne strop.**
* **Sdílený limit pass/hand-off** (P7) dělá hbité rasy slabšími, než jsou.
* **SPP se nesledují** — elfí AI nemá důvod házet kvůli bodům.
* **T5.9: snímek je začátek kola** — skutečné pořadí akcí může vzor zničit
  dřív. Obecné omezení všech našich skenů.
* **Šumové dno ±5,3 pp na 400 párech** ⇒ na efekt 3 pp je potřeba **1500+
  párů** (= ~7 h stroje). Harness je deterministický.
* **T2.4: hranice S2/S3/S4** nejsou robustní, dokud `paceAch` loguje 0.0.

# ⛔ ZÁVAZNÝ ZÁKAZ NAD CELOU DNEŠNÍ DOKTRÍNOU *(uživatel 14.08.)*

> *„Na ty následné blocky — musí být situace nachystaná. A jako trpaslíci
> nesmíme porušit pravidlo: hnát se za jedním cílem a otevřít prostor jinde."*

Platí na **P2 ⑤**, **P9c**, **T1.8** i na Fableho „idle těla dosáhnou".

* **Odsun se plánuje na těla, která UŽ STOJÍ, kde mají.** „Posuň ho k dalšímu
  blokujícímu" je kritérium nad **současnou** deskou, ne nad tou, kam bychom
  někoho došli. U MA4 znamená doběh na pozici opuštění tvaru — a to je přesně
  **S7.1** *(„nehonit, nevybíhat, nepřevažovat — krýt šířku")*.
* ⇒ **Fableho „94,7 % idle těl na pollutera DOSÁHNE" je číslo o DOSAHU, ne
  o tom, že tam mají jít.** Stejná past jako u „61 %" a u „61 % → 39,4 %":
  **potřetí dnes** je permisivní číslo zaměnitelné za doporučení.
* ⇒ Každé pravidlo tvaru „pošli tělo na X" musí mít protějšek **„co se otevře
  tam, odkud odešlo"**. Bez toho se doktrína čištění rohů změní v honičku,
  což je pro trpaslíka nejdražší možná chyba.

⭐ **Obecný tvar:** *dosažitelnost není povinnost.* Zapsat jako zákaz do spec
vedle S7.1, ne jen sem.

# ⭐ CHYBĚJÍCÍ DIMENZE — dvě, a jsou to sourozenci

Audit spec (12.08.) našel, že celá procedura popisuje jen NAŠE kolo a chybí jí
dimenze **„co pozice dává soupeři"** — doplněno jako ČÁST 13 (E1/E2).

14.08. vyšla najevo **druhá, stejného tvaru: „co ta akce bere jiným akcím".**
Doporučení se u nás počítají jako čistý výnos, protože se měří jedna veličina
proti výsledku. Jenže **blitz je 1 za kolo a těl je 11** — každé „dělejme X"
je ve skutečnosti „dělejme X **místo** Y". Objevilo se to na P2 (očistit roh
blitzem = nemít ho na prolomení zdi), ale platí to na každou položku, která
předepisuje akci.

⇒ **Pravidlo pro každý budoucí návrh doktríny:** vedle výnosu uveď, z jakého
rozpočtu se platí (blitz 1/kolo · pass 1/kolo · foul 1/kolo · 11 těl, z toho
~2,6 leží a ~1,0 zamčené) a co se za to nedělá. Bez toho je σ jen polovina
odpovědi.

# ⭐ OTEVŘENÁ OTÁZKA Č. 1

**Proč zlepšení procesu nevede k výsledku.** Brána zlepšila skoro všechny
kontroly a chess se nehnul. Část je vysvětlená (vyměnila tempo za bití),
zbytek ne. Dokud to nevíme, stojí *„úplná procedura jako nulová hypotéza pro
učení"* (T6.2) na kontrolách, o kterých nevíme, že k něčemu jsou.

# ⭐⭐⭐ CO PŘEDPOVÍDÁ TD *(podle čeho se řadí zbytek)*

Plné drivy ≥7 kol, 195 drivů:

| | σ |
|---|---|
| K9a tempo | **4,2σ** |
| bloků na kolo | **2,7σ** |
| čistota rohů / `FB2 ≤ 1` | 2,6σ |
| **špinavých rohů** | **−2,2σ** |
| REACH0 (počet) | −1,8σ |
| *počet rohů klece* | *−0,2σ = nic* |
| *K33, K34 jako ano/ne* | *0,6σ / 0,8σ = nic* |

**Tempo a bití jsou dva nezávislé prediktory a brána je proti sobě vyměnila.**

---

# CO JE TEĎ PRVNÍ
*(jediný oddíl, který se přepisuje — stav k 14.08.2026 ráno)*

| | co | proč |
|---|---|---|
| **1.** | **A1 anomálie** TD flag vs skóre delta | zadáno uživatelem na dnešek; 0,3 % her měří nesprávný výsledek — dokud to platí, každé A/B má šum navíc |
| **2.** | **P5 hand-off** | povýšeno uživatelem; oprava bez A/B, ověřit na kontrolách |
| **3.** | **P0.1 + P0.5 + P0.2** na velkém korpusu | korpus stojí hotový, nestojí strojový čas, odemyká P2 |
| **4.** | **P1** K33/K34 na spojité | levné, opravuje metr |
| **5.** | **P3** fázový plán trasy | bez fáze nejde odlišit chybu od záměru |
| **6.** | **A2** vyzvednout exposure/R1 z plánovače klece | jediná cesta, jak změřit hotovou práci za zamítnutou branou |

**Na strojový čas víkendu** (~60 h, jedno A/B = 7 h) se vejde 5–6 A/B.
Kandidáti v pořadí: P2 *(po P0.1)* · A2 · P6 · P7 *(zvlášť, zhorší nás)*.

**T1 (repertoár) a strojová část spolu nesoupeří o zdroj** — dají se nechat
běžet vedle sebe. Uvnitř strojové části platí „jedna změna najednou".
