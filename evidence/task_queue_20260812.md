# SLOUČENÁ FRONTA ÚKOLŮ — 12.08.2026

Slučuje: audit spec (A1–A9, 12.08.) · fronta oprav (P0–P5, 12.08.) ·
master seznam 11.08. (položky [1]–[14]) · nástroje N1–N5 · otevřené položky
O1–O7 · audit úplnosti drillu 11.08.

Nahrazuje `fix_queue_20260812.md` — ten měl chybu, viz ⚠️ níže.

---

# ⚠️ DVĚ OPRAVY VLASTNÍCH SEZNAMŮ (dřív než se podle nich začne pracovat)

**1. `fix_queue_20260812.md` má chybné P0.** Uvedl jsem *„rozestavení podle
ROLE, ne podle pořadí v rosteru"* jako otevřené. **Není** — shipnuto
11.08. commitem **`41c3570`**, a má první důkaz účinku: **nosič je Runner
v 79–89 % kol** proti **47 %** před opravou. Z P0 zbývají jen dvě věci
(záloha u míče = potenciální nosič, ověřit bránu přihrávek) a jedno měření.
⇒ Přesně ta chyba, kterou audit vytkl ČÁSTI 6. Udělal jsem ji dneska znovu.

**2. Uživatelovo pravidlo R1 je z velké části UŽ NAPROGRAMOVANÉ.** Commit
**`c085331`** („exposure klece": nosič a rohy nekončí v TZ, když to nic
nestojí) je totéž pravidlo, jen z druhé strany. **Není živé** — sedí za
vypnutou bránou `cageAdvance`.

---

# ⛔ NEJVĚTŠÍ POLOŽKA CELÉ FRONTY

**Za vypnutou bránou `cageAdvance=false` leží hotová, otestovaná a nikdy
nezměřená práce, která míří přesně na naše hlavní číslo.**

Uvnitř brány: `c085331` exposure klece (= R1) · `cd2d98c` cage-fill ·
`dda0cdd` „jeď co můžeš" · `1445bd3` oprava rohu s GFI.

Změřeno 11.08. při zapnuté bráně:

| verdikt plánovače | podíl kol |
|---|---|
| TEMPO_INSUFFICIENT | **48 %** |
| DICEY | 37 % |
| PLAN_READY | 15 % |

**Když plán proběhne, dělá krok 5,00 pole.** `search()` dělá **1,73**.
Potřebujeme **3,14**. ⇒ **Plánovač umí tempo, které nám chybí, a v 85 %
kol odmítne běžet.**

A F1 A/B **nebyl zamítnutý, byl SMÍŠENÝ**: dwarf-skaven **+6,75 pp
(~2,5 SE) PROŠEL**. Rozhodnutí znělo „nechat OFF, dokud se nevyřeší nízká
adopce" — a to se od **05.08.** neudělalo.

⚑ **Závazný vstup uživatele z 05.08., pořád nesplněný:**
> *„fallback na search je nepřijatelný — nositel utíká z klece sám"* ·
> *„nemůžeme nechat trpaslíky zahodit pokus o TD hned v 1. kole"*

Hierarchie, kterou zadal: **posun → doplnit → NIKDY solo útěk.**

---

# PŘEROVNANÉ PRIORITY

Uživatel 12.08.: *„ať je toto co nejvíc kompletní, než se vrátíme k učení"*
a *„pak teprve budou na řadě úkoly plánované z dřívějška"*.
⇒ **T1 (repertoár) je první, jak zadal.** Poznámka k tomu je u T3.

## T0 — DLUH, KTERÝ ZNEHODNOCUJE OSTATNÍ ČÍSLA
*Musí se udělat dřív, než podle těch seznamů kdokoli měří nebo plánuje.*

| # | co | zdroj |
|---|---|---|
| T0.1 | **srovnat K9 se S2.7** — kontrola měří konstantu, povinnost přikazuje funkci odporu. Tichá chyba. | audit N5 |
| T0.2 | **vyškrtat uzavřené**: O3, O4, O5 (uzavřené v témž dokumentu), X6 (z ~80 % hotové přes `TurnPlanRecord`) | audit N3, N4 |
| T0.3 | **opravit P0** ve frontě oprav podle ⚠️ výše | dnešek |

## T1 — REPERTOÁR *(rozhovor a odehraná situace, nesoupeří o strojový čas)*

| # | co | zdroj |
|---|---|---|
| **T1.1** | ⭐ **„Bilance soupeřova kola"** — nová část spec. Všech 11 situací popisuje NAŠE kolo; nikde není povinnost tvaru *„na konci našeho kola smí mít soupeř nejvýš X"*. Jediná díra, která není o měření, ale **o způsobu uvažování**. | audit N6 |
| **T1.2** | **Chybějící situace**: kolo po turnoveru · kolo po obdrženém TD (doktrína záporné rezervy je v paměti, **ne v katalogu**) · utrácení rerollů (zdroj jako blitz, 8 na půli, žádné pravidlo) · hranice poločasu · počasí | audit N7, drill audit 11.08. |
| **T1.3** | **Odehraná situace cílená na S5** — největší měřená díra (96 % dosažitelnost, **53 % pokus**), a 3 ze 6 jejích povinností jsou dnes nekontrolovatelné | audit N8 |
| T1.4 | **[8] / O2 předání míče** — z 11.08. odpověď *„skoro nikdy"*, z 12.08. *„záloha = potenciální nosič"*. Zbývá **zapsat jako povinnost**, ne rozhodnout. | O2 |
| T1.5 | **O6 nouze: prorazit vs. držet** — jediná trpasličí učební úloha; dokud není napsaná, nemá učení co dostat | O6 |
| T1.6 | **[4] Blitzer končí na lajně** — na lajně má stát tělo, které NENÍ určeným rohem klece | [4] |
| T1.7 | **rozpočet těl pro R3** — 12.08. prošla náhodou; R1 spolyká všechna pohyblivá těla | audit N10 |

## T2 — MĚŘICÍ APARÁT
*Bez něj se ze ~60 povinností nekontroluje ani jedna.*

| # | co | zdroj |
|---|---|---|
| **T2.1** | **N2 rozhodčí** `diag_turn_referee_20260811.py` — konec kola z `turnLogs[i+1]`, klasifikace S0–S10, karta kola s polem `proc` | audit N1, N2 |
| T2.2 | **K29–K33** pro ČÁST 9 (R1–R4, pořadí blitzu, `K-noblock`). ⭐ **R1 je hotová** — `diag_play_session_20260812.py` ji už počítá, stačí přenést | audit N2 |
| T2.3 | **N3 sestavy** (na kolo / drive / zápas / rasu) | N3 |
| T2.4 | **K28 rozložení S0–S10** — nevíme, jak často která situace nastane | audit N9 |
| T2.5 | **N4 kalibrace proti uživateli** — 20 kol, shoda ≥ 18/20, **než se agregátu uvěří** | N4 |
| T2.6 | **X2 + X3** (kostky bloku · deklarovaná makra s pořadím) — jedna oprava odemkne **Z4, Z5, Z9, Z14, S2.14, S10.3** | audit N4 |
| T2.7 | **[14] diag binárky staticky** — jinak stará binárka tiše měří jiný engine | [14] |

## T3 — TEMPO: HOTOVÁ PRÁCE ZA VYPNUTOU BRÁNOU
*Tady je to jediné číslo, o kterém víme, že stojí ~30 procentních bodů výhry.*

| # | co | zdroj |
|---|---|---|
| **T3.1** | ⭐ **brána klece — opce b) veto jen když `achievable == 0`** + **cage-fill jako minimum**. Míří přímo na těch 48 % TEMPO_INSUFFICIENT. Opce a) grind vyzkoušena = nula; c) eskalace nevyzkoušena. | 05.08., nedodělané |
| T3.2 | **[1] kontrola `c085331`** — přeměřit K7 na korpusu vyrobeném AŽ PO opravě klece (dnešní korpusy jsou baseline) | [1] |
| T3.3 | **[2] tempo — cílit na 3,14, ne 2,61**; první kola drivu mají jet rychlostí klece, ne mletí (`cage_advance.cpp`, `achievablePace`) | [2] |
| T3.4 | **[3] změřit `41c3570`** párovým srovnáním ér — první kolo s míčem (dnes 4,1), podíl kol s Longbeardem | [3] |
| T3.5 | **[5] na lajně stojí 4 hráči místo 3** — dává soupeři blok navíc; je to v poli formace, ne v přiřazení | [5] |

> ⚠️ **Poznámka k pořadí, ne námitka.** T1 je konverzace, T3 je strojový čas —
> **nesoupeří spolu o zdroj.** Doporučuju je nechat běžet vedle sebe:
> T1 s tebou, T3 na pozadí. Uvnitř T3 platí „jedna změna najednou".

## T4 — ZBYTEK DNEŠNÍ FRONTY OPRAV

| # | co | zdroj |
|---|---|---|
| T4.1 | **záloha u míče = potenciální nosič** (Blitzer AG3 MA5) | P0 zbytek |
| T4.2 | **ověřit bránu přihrávek na korpusu** — dopočítané, ne změřené | P0 zbytek |
| T4.3 | **priorita blitzu**: zeď kupředu → odmarkovat nosiče → příležitost | P3 |
| ~~T4.4~~ | **generování chain pushe** — ✅ **VYŘÍZENO Fable 12.08.**, viz níže | P4 |
| T4.5 | **Jump Up: Block Action vleže (+2)** — spící, ty rostery nehrajeme | P5 |

## T5 — PARITA A ÚDRŽBA

| # | co | zdroj |
|---|---|---|
| T5.1 | **[7] tabulka výkopu — 5 z 11 vadných** *(uživatel ODLOŽIL)*; 22 % výkopů zahazuje volné tempo | [7] |
| T5.2 | **[6] Kick-Off Return** — 3 vady; v TV1200 ji nemá nikdo | [6] |
| T5.3 | **zranění nepřetrvávají přes drive** — každý TD staví 11 čerstvých ⇒ attrition čísla měří jen poslední drive | 07.08. |
| T5.4 | **[13] M1 přeběhnout** — smazat falešný `M1_DONE`, přestavět `diag_m1` | [13] |
| T5.5 | **O1 kopat, nebo přijímat** — volbu vůbec nemodelujeme; potenciálně větší páka než cokoli uvnitř kola | O1 |
| T5.6 | **O7 Underworld** | O7 |

## T6 — AŽ NAPOSLED
**N5 vstřelení plánu priorem `f(rezerva)`** — jediná změna chování, až po
T2. A **učení** až po dokončení repertoáru, s úplnou procedurou jako
nulovou hypotézou.

---

# CO SE ZMĚNILO PROTI DŘÍVĚJŠÍM PRIORITÁM

| dřívější pořadí | nové | proč |
|---|---|---|
| P0 „rozestavení podle role" nahoře | **zrušeno** | shipnuto `41c3570`, 47 % → 79–89 % |
| brána klece = „rozcestí k rozhodnutí" | **T3.1, největší položka fronty** | není to rozcestí, je to **nedodělaný úkol z 05.08.** s hotovým kódem a smíšeným (ne zamítnutým) A/B |
| N2 rozhodčí v „ČÁSTI 7, krok 1" | **T2.1** | pořád první v měřicím aparátu, ale repertoár jde před ním (zadání uživatele) |
| audit A1/A2 | **T1.1, T1.2** | beze změny — nejlevnější a největší přírůstek repertoáru |
| [7] tabulka výkopu | **T5.1** | odložil uživatel, drží se odložená |


---

# ⇐ ZAŘAZENÍ VÝSLEDKŮ FABLE (12.08., 280 her, 4488 našich kol)

## T4.4 chain push — ROZHODNUTO, NEIMPLEMENTOVAT

| výplata | naměřeno | brána | verdikt |
|---|---|---|---|
| **tempo** | RAW **0,345** · vážené EV **0,294** pole/kolo | ≥ 0,3 | **na hraně ⇒ zapsat, neimplementovat** |
| **únik** | **0,29 / 10 kol** (vážené 0,18) | ≥ 1 / 10 kol | ⛔ **LINKA SE ZAVÍRÁ** — chybí řádově |

**Předpoklad zadání seděl:** z 2172 vzorů s kladným tempem je hotových bez
dostavby jen **8 %** — 93 % se musí dostavět jedním až dvěma těly. Hledat
neúplné vzory bylo správné rozhodnutí.

⚠️ **Moje hypotéza o úniku byla VYVRÁCENA.** Sázel jsem na to, že únik
označkovaného nosiče bude cennější než tempo. Jenže **nosič stojí v TZ jen
v 17,5 % kol** — naše doktrína ho schovává, takže situace skoro nenastává.
A když nastane, je to v 124 ze 131 případů první článek, kde soupeř po
odsunu stejně zůstane vedle nosiče.

**Tvar, který to vyrábí, není to, v co jsme doufali:** dvě volná těla dojdou
zezadu na dvě ze tří odsunových polí, blok pošle soupeře do třetího a tělo
tam dostane pole zadarmo. Beneficientem je v **78 %** sám filler a v **79 %**
„jiný" hráč — **nosič jen v 1,6 %, roh klece v 6 %**. Je to *„MA+1 pro
jednoho pěšáka za dvě Move akce a blok"*, ne posun klece.
**Dobrá zpráva pro doktrínu:** rozbití klece to vyžaduje jen ve 12 % případů
⇒ konflikt s R1 je okrajový.

**A EV 0,294 je HORNÍ odhad** — nezapočítaný opportunity cost dvou Move akcí,
Frenzy druhý blok u 19 % případů, ani skull 1/36. Poctivé čtení je tedy ještě
níž než hraniční číslo. ⇒ **neimplementovat.**

**Jediné obhajitelné otevření je matchupové:** skaven 0,379 a human 0,317
projdou i váženě, wood-elf 0,206 zřetelně ne. Zapsáno jako podmíněná
příležitost, nezařazeno k práci.

## ✅ Vedlejší nález Fable → OPRAVENO TÝŽ DEN
**Stand Firm se neptal v řetězu** (`0ec69f3`). A pod tím starší a horší chyba:
**follow-up nekontroloval, jestli obránce pole vůbec opustil** — blok na Stand
Firm hráče srazil útočníka NA NĚJ. Wood Elf TV1200 má Treemana se Stand Firm,
takže obojí bylo **živé v dw-we**. 528 testů zelených.

## Nové drobné položky z Fableho „co jsem nezměřil"

| # | co | kam |
|---|---|---|
| **T5.7** | ⚠️ **DAUNTLESS SE SPOUŠTÍ ŠPATNĚ — ŽIVÁ CHYBA, HRAJE PRO NÁS.** CRP: *„The strength of both players is calculated **before any defensive or offensive assists are added**"*. Engine gate je `effDefST > effAttST`, tedy **po asistencích** (`block_handler.cpp:342`). ⇒ Dauntless se spustí i proti **stejně silnému** soupeři, jakmile má asistence — a pak uspěje **vždy** (d6+3 > 3). Sonda: 0 asistencí = nespustí se · 1 asistence = spustí a uspěje · 2 = totéž. Navíc efekt `effAttST = effDefST` přebírá **soupeřovy** asistence místo vlastních. Týká se **Troll Slayerů ve VŠECH matchupech**, ne jen proti ST4+. | parita, **rozhodnout** |
| T5.8 | **Frenzy druhý blok** se v hodnocení příležitostí nikde nemodeluje — týká se Slayerů, nejen chain pushe | parita/hodnocení |
| T5.9 | **snapshot je začátek kola** — skutečné pořadí akcí může vzor zničit dřív; obecné omezení všech našich skenů, zapsat jako výhradu k metodě | metodika |

---

# STAV FRONTY NA KONCI 12.08.

## Odbaveno dnes (po sepsání fronty)

| položka | stav |
|---|---|
| **T5.7 Dauntless** | ✅ `9f98070` — řeší se PŘED asistencemi, jak žádá CRP |
| **Stand Firm v řetězu + follow-up** | ✅ `0ec69f3` — nebylo ve frontě, vyplynulo z Fableho nálezu; a pod ním starší chyba: útočník šlapal na pole, které obránce neopustil |
| **T4.4 chain push** | ✅ rozhodnuto Fablem — únik zavřen, tempo pod branou ⇒ **neimplementovat** |
| **T1 repertoár** | ✅ **ČÁST 10 spec: Big Guy soupeře** — S-BG.1–6, Z15, Z16, žebříček „koho srazit ≠ koho faulovat", potvrzeno webem |
| ~~T5.7 Dauntless jako parita~~ | uzavřeno |

⚠️ **ČÁST 10 nebyla na žádném seznamu.** Audit ji nenašel, protože audit
porovnává spec proti tomu, **co už víme**. Vyšla z rozhovoru — a to je přesně
ten důvod, proč T1 (repertoár) běží přes debatu a odehranou situaci, ne přes
analýzu. **Potvrzuje to zvolené pořadí.**

## Co je teď první

| pořadí | co | proč právě to |
|---|---|---|
| **1.** | **T0.1** srovnat K9 se S2.7 | tichá chyba — kdyby rozhodčí běžel, měřil by špatně a nikdo by to nepoznal |
| **2.** | **T0.2 + T0.3** vyškrtat uzavřené | seznamy jsou dnes nedůvěryhodné (O3/O4/O5 uzavřené, X6 z 80 % hotové) |
| **3.** | **T1.1** bilance soupeřova kola | jediná chybějící *dimenze*; ČÁST 10 je její první polovina (Big Guy), zbytek chybí |
| **4.** | **T1.2** chybějící situace — po turnoveru, po obdrženém TD, rerolly | doktrína existuje v paměti, ne v katalogu |
| **5.** | **T1.3** odehraná situace na **S5** | největší měřená díra (96 % dosažitelnost vs 53 % pokus) |
| **6.** | **T3.1** brána klece — veto jen při `achievable == 0` + cage-fill | největší strojová položka; míří na 48 % TEMPO_INSUFFICIENT |

**1–5 jsou konverzace, 6 je strojový čas** ⇒ nesoupeří o zdroj a dají se
nechat běžet vedle sebe.

---

# T2.2 ODBAVENO + PRVNÍ ČÍSLA (12.08. večer)

`diag_rules_checks_20260812.py` — R1–R4 poprvé **změřené**, ne jen zapsané.
Korpus 120 her / 1761 našich kol. Zapsáno i do spec jako **ČÁST 11**.
⚠️ Baseline: korpus je **před** `c085331` a s **vypnutou** bránou.

| kontrola | výsledek |
|---|---|
| **K29 (R1)** plná čistá klec | **9,3 %** · obsazeno **1,48 ze 4** rohů |
| roh markovaný | 13,7 % |
| **K9a** podlaha splněna | **32,5 %** · schodek **−2,27 pole/kolo** |
| **K30 (R3)** AG3 drženo | **23,3 %** |
| **K31 (R4)** těla bez úkolu | **0,86 / kolo** |
| **K33** kolo bez bloku | **23,9 %** |
| plán | **NOT_CONSULTED 100 %** |

## ⚠️ Nález, který mění pořadí ve frontě

**T2 a T3 nejsou nezávislé.** Plánovač se v korpusu nezeptal ani jednou ⇒
`resistance` je všude 0 ⇒ **K9b nejde spočítat, dokud se nezapne brána klece**.
Není to díra v logování (X6 přistálo), je to **T3.1**.
⇒ Rozdělit T2: **K29/K30/K31/K33/K9a hotové teď**, **K9b až po T3.1**,
**K32 pořád blokovaná na X1** (blitz se v logu nepozná od bloku).

## A jedno upřesnění priority uvnitř T3

Prázdný roh (1,48 ze 4) je **mnohem větší díra než markovaný** (13,7 %).
⇒ Uvnitř brány klece má **cage-fill přednost před exposure** — nejdřív rohy
obsadit, teprve pak řešit jejich čistotu. To je opačné pořadí, než v jakém ty
dva commity vznikly.

---

# ⚠️ PŘEROVNÁNÍ PO DRUHÉM FABLE SCANU (12.08. večer)

**Zdroj:** `evidence/situation_scan_20260812.md`, 280 her / 4 488 kol.
Zapsáno i do spec jako **ČÁST 12**.

## Dvě čísla, která ruší dvě priority

**1. „96 % dosažitelnost / 53 % pokus" je MRTVÉ.** Pocházelo z korpusu
z 30.07., n=53 kol, engine o šest oprav starší. **Dnes je pokus v 89 % kol
S5.** Čistá vada volby = **2,7 %**. ⇒ **T1.3 (odehraná situace na S5) ztrácí
důvod v té podobě.** Generátor je navíc v pořádku — předregistrovaná varianta
„S5 patří do generátoru" nenastala, fronta se po téhle ose nepřeskupuje.

**2. S2 je 6,8 % kol, S7 je 32,4 %.** Spec o S2 tvrdí, že je to *„situace,
ve které se hraje většina trpasličích kol"* — a dala mu 14 povinností.
S7 má 9 povinností a je čtyřapůlkrát častější. **Detail procedury sedí na
nejřidší situaci.**

## Nové pořadí uvnitř T1

| | co | proč |
|---|---|---|
| **T1.1** | bilance soupeřova kola | beze změny — běží na to Fable |
| **T1.2** | chybějící situace (po turnoveru, po TD, rerolly) | beze změny |
| **T1.3′** | ~~S5 session~~ → **S7 (boxing-in): nejčastější situace, 32,4 %** | tam se hraje třetina kol a pozornost tam nešla |
| **T1.4′** | **O6 — nouze prorazit vs. držet** *(bylo T1.5)* | **S4 = 27,7 % kol** a doktrína je NEROZHODNUTÁ ⇒ není to okrajová učební úloha, je to díra uprostřed |
| **T1.5′** | **S5.3 / S5.4 — ZAJIŠTĚNÍ sběru** | nové největší díry S5: záloha u míče **22,2 %**, nosič krytý po sběru **28,8 %**. Sbírat umíme, pojistit sběr ne. |

⚠️ **Výhrada k číslům S2/S3/S4:** jejich hranice stojí na `paceAch`, který se
loguje jako 0.0, protože plánovač se nezeptal (`NOT_CONSULTED` 100 %).
S2 se podle volby konstanty hýbe **mezi 6 a 354 koly**. **Robustní je jen
S7 (32,4 %).** ⇒ ještě jeden důvod pro **T3.1** (brána klece).

## Sbíhá se to na jeden kořen
Dvě nezávislá dnešní měření — kontroly (ČÁST 11) i rozložení situací
(ČÁST 12) — narazila na **totéž**: vypnutá brána znemožňuje K9b i hranici
S2/S3/S4. **T3.1 přestává být „největší strojová položka" a stává se
předpokladem měření.**

---

# T1.1 ODBAVENO — bilance soupeřova kola (třetí Fable scan)

Zapsáno do spec jako **ČÁST 13**. Report `evidence/exposure_scan_20260812.md`.
**Chybějící dimenze z auditu (N6) je doplněná, a to i s ověřením predikce.**

**Dvě čísla do procedury:**
* **E1: `REACH0 = 0`** — nikdo nesmí dosáhnout na nosiče bez dodge.
  0 → ztráta míče **1,8 %** · 1 → 8,3 % · 2–3 → 22 % · 4+ → 33 %.
  Dnes plněno ve **42 %** kol ⇒ dosažitelné.
* **E2: `FB2 ≤ 1`** (bezplatné ≥2kostkové bloky), cíl 0 — 0,41 vs 1,00
  sraženého na kolo.

⭐ **Opravuje výklad R1:** roh klece v TZ nemá samostatný signál, škodí jen
tím, že otevírá cestu k nosiči. **R1 je správné pravidlo ze špatného důvodu**
— nechrání roh, drží REACH0 na nule. Kdyby stálo proti sobě „čistý roh" a
„nulový REACH0", vyhrává REACH0.

⇒ **Nová položka T2.8: přidat E1/E2 do `diag_rules_checks_20260812.py`** jako
K34/K35. Levné, veličiny jsou definované a skript existuje.


---

# NOVÝ PRAVIDLOVÝ NÁLEZ (12.08. večer) — TÝMOVÉ REROLLY JSOU PAUŠÁL

**`game_simulator.cpp:154` a `:324`: `ts.rerolls = 3;` pro KAŽDÝ tým, bez ohledu
na rasu.** Obnovuje se to na začátku poločasu (`resetHalfState`), po TD ne;
omezení „jeden na kolo" (`TeamState::canUseReroll`) je správně.

⚠️ **Roster přitom cenu rerollu VEZE a liší se:** dwarf **40**, skaven **60**
(`roster.h:24 rerollCost`, v tisících). V reálné hře si trpaslík za týž
rozpočet koupí **víc** rerollů právě proto, že jsou u něj levnější — to je
součást identity pomalé bijící rasy, která si nemůže dovolit zahodit kolo.

⇒ **Paušál 3 pro všechny je nepřesnost v NÁŠ NEPROSPĚCH** a nikde zapsaná
nebyla. Týká se každého hodu, který jde rerollovat — GFI, dodge, pickup,
catch, blok bez Blocku.

**Kde to bolí nejvíc:** u AG2 nosiče je reroll rozdíl mezi 50 % a 75 % na
dodge, a u GFI mezi 16,7 % a 2,8 % na turnover.

**Zařazení: T5 — parita.** Není to blokující, ale je to levné a měřitelné:
odvodit počet rerollů z rozpočtu a `rerollCost` místo konstanty.
**Zvlášť zapsat i to, že náš herní nástroj rerolly vůbec nesleduje** — celá
dnešní partie se hraje, jako by nebyly, takže odhady rizika v ní jsou
konzervativní (horní mez).


---

# NOVÁ POLOŽKA T2.9 — `LOCKED` jako chybějící člen tempa (12.08. večer)

**Hypotéza ze zobecnění odehrané situace:** *počet našich hráčů stojících na
konci kola v soupeřových tackle zónách záporně předpovídá postup nosiče
v NÁSLEDUJÍCÍM kole.*

**Mechanismus:** postup vykoupený kontaktem **zamyká vlastní těla** — kdo stojí
v zóně, nemůže se hnout bez hodu, a zamčené tělo neumí být rohem klece.
Na konci měřeného kola bylo **9 z 11 zamčených**, volná těla dvě.

⇒ **K36 `LOCKED`** do `diag_rules_checks_20260812.py`, korelovat s `Δx`
dalšího kola. **Kdyby to vyšlo, je to chybějící člen v K9a** — schodek
−2,27 pole/kolo by nebyl selháním jednoho kola, ale **akumulací zámků**.
Levné: veličina je definovaná, skript existuje, korpus taky.
