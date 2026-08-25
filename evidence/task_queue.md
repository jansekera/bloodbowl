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

⭐ **PROČ SE ZAPISUJE I TO, CO SE NEDĚLÁ** *(uživatel 14.08.)*:
*„ať máme dohledatelné nejen proč jsme něco dělali, ale i proč jsme něco
nedělali."* **ZAMÍTNUTO a ODLOŽENO jsou plnocenný záznam, ne absence záznamu.**
Bez důvodu se za měsíc nedá odlišit *„změřili jsme to a nevyplatilo se"* od
*„nikoho to nenapadlo"* — a druhé se pak dělá znovu. Proto každé **ZAMÍTNUTO**
nese důvod a čísla, každé **ODLOŽENO** spouštěč a každé **?** větu, co by to
rozhodlo.

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
| **T1.9** | ⭐⭐⭐ **ROZPOČET JEDNOHO BLITZU ZA KOLO — CHYBĚJÍCÍ KAPITOLA, NE CHYBĚJÍCÍ MĚŘENÍ** *(uživatel 18.08.: „je ten blitz na nosiče někde v dlouhém plánu blitz akce?“ — není)*.<br>Blitz je ve frontě v **šesti** položkách, každá odpovídá na jinou lokální otázku: **P0.6** roh vs zeď (Δx +1,80 vs +2,52; 45,5 % blitzů na roh padlo v kolech, kdy šel blok zdarma) · **P2 (2)** blitz na roh jen jako záloha · **P6** cíl se vybírá podle surových kostek · **P15** práh nabídky nezná cenu cíle · **K32** blitz se v logu nepozná od bloku · **P33** blitz na nosiče.<br>⛔ **Nikde ale nestojí, K ČEMU ten jeden blitz za kolo je a jak se jeho uchazeči řadí proti sobě.** Je to jediná akce s tvrdým limitem 1/kolo — tedy jediné místo, kde je doktrína nutně **rozpočtem**, ne seznamem povinností. Bez kapitoly se každý nález o blitzu měří proti jinému pozadí a nedá se sečíst.<br>⭐ **Čísla už z velké části existují, jen nikdy nebyla vedle sebe** *(vše na 3 000 hrách)*: nosič v dosahu blitzu **4,12 kola/zápas** — blitz na něj **48,5 %**, jinam 26,4 % *(z toho polovina cílů stála hned vedle nosiče)*, **neutracen vůbec 25,1 % = 1,04 kola/zápas**; blitz na roh proti blitzu do zdi **+1,80 vs +2,52** (−6,4σ); **61 % polluterů jde srazit BLOKEM zdarma** ⇒ blitz na roh je z velké části vyhozený rozpočet.<br>⇒ **Zadání:** kapitola ve spec, která uchazeče o blitz seřadí jedním kritériem a řekne, kdy se blitz NEUTRÁCÍ. Teprve pod ní visí P0.6, P6, P15, P33 jako případy. ⚠️ **Napřed kapitola, pak jednotlivé opravy** — jinak se opraví čtyři místa proti čtyřem různým pozadím ([[feedback_complete_dwarf_repertoire_not_learn]]). | **KAPITOLA NAPSÁNA 18.08.** — spec **ČÁST 14 „ROZPOČET BLITZU“**. ⚠️ Zjištěno při psaní: kapitola nebyla jen nenapsaná, byla **rozdělaná a přerušená** — blitz byl rozsypaný na **dvanácti** místech spec a měl **DVA různé žebříčky** (S2.10 a 9.5), plus S7.5, S8.3, S5.6, S6.3, S9.4, Z10 a K10/K11/K32. Žádný neříkal, **podle čeho se řadí**. ⭐ **Kritérium: blitz se kupuje DOSAH, ne násilí** — blok je zadarmo, ale jen pro toho, kdo už sousedí; blitz je jeden a jeho jediná vlastnost navíc je, že tělo někam DOJDE ⇒ **patří výhradně tam, kam se blokem nedosáhne**. Z toho žebříček **B1–B7** (jeden pro útok i obranu), seznam **kdy se NEutrácí**, a nová kontrola **K37 „blitz utracen na dosažitelný cíl“**. K32 odblokována rekonstrukcí bez nového logu. ⛔ **Zbývá změřit:** kolik z 1,04 „neutraceno“ bylo správně · B3 vs B2 se nikdy neměřily proti sobě · jestli je 26,4 % blitzů „jinam“ chyba (polovina cílů stála vedle nosiče). |
| **T1.11** | ⭐⭐⭐ **OBRANA JAKO FÁZE — D1 sloupce → D2 L** *(uživatel 20.08.: „na začátku musí trpaslík začít s 2 sloupce a postupně to tlačit do L — jak mu poroste převaha“)*. Zapsáno jako spec **ČÁST 17** (nová obranná kapitola; do 20.08. byla obrana ve spec jen S1 a S8, díra přesně v **S7 = 32,4 % kol**). ⛔ **V kódu NENÍ ani jedno** *(ověřeno 20.08., `expandReposition` větev `onDefense`)*: obranný repertoár je 6 rolí, každá posílá **JEDNO** tělo (cage tag · intercept · safety · marker · ≤2 endzone guardi), zbytek jde na **jednořadý** screen na jedno `x` s natvrdo danými `y ∈ {3,5,7,9,11}` ⇒ obklíčení může vzniknout jen náhodou a **hloubka 2 neexistuje vůbec**.<br>⭐ **Web dodal geometrii D1** (column defence): **1 pole od soupeře** *(nutí blitz, ne blok)* · **2 pole do šířky** · **2 do hloubky** *(„i když jednoho blitzneš a srazíš, neprojdeš kvůli obránci za ním“)* · **5 obránců pokryje šířku** a udrží až dvojnásobek vlastního počtu.<br>⛔⛔ **BLOKÁTOR: „převaha“ nemá definici** — bez ní se přechod nedá implementovat ani měřit. Kandidáti: stojící těla · attrition · poziční · iniciativa. ⚠️ **A žádná konstanta.** | **OTEVŘENO — ODBLOKOVÁNO 20.08.** *(Q6: převaha = stojící těla)*. ⚠️ Ale **stroj zůstává na kleci**, takže stavět se začne až po ní. ⭐ Z Q6 plyne tvar: **spojitý, ne prahový.** |
| **T1.12** | ⭐⭐ **VŠECHNA OBRANNÁ MÍSTA JSOU NATVRDO NA PEVNÉM Y.** Screen `{3,5,7,9,11}` · safety `y=7` · guardi `y=5`/`y=9`. **Komentář v kódu tu díru sám přiznává** *(u Strategy 0.5)*: „pevný safety spot (y=7) a pevné screen Y {3,5,7,9,11} nikdy nepokryjí nosiče sprintujícího po křídle (y=1–2 / 12–13), takže v jeho pruhu nikdy nikdo doopravdy nestál — díra ‚screen=0‘“. Vyřešilo se přidáním **jednoho** interceptora, ne opravou tvaru.<br>⇒ **TÁŽ VADA „KAM“ jako v útoku** (P38 `expandAdvance` · P9 `choosePushSquare` · P32 posun klece · P35 blitz landing) — *engine vybírá KDO a JESTLI, ale ne KAM*. ⛔ **V obraně je to ale horší: není to jedna funkce, je to CELÝ REPERTOÁR.** | **OTEVŘENO — zapsáno 20.08.** |
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
| T2.4 | K28 rozložení S0–S10 | **UZAVŘENO s výhradou** — spec ČÁST 12; hranice S2/S3/S4 stojí na `paceAch`, který je 0.0 (`NOT_CONSULTED` 100 %) ⇒ robustní je jen S7<br>⚠️ **UPŘESNĚNO 25.08.: jsou to DVĚ RŮZNÁ POLE a jen jedno je mrtvé.** `plan.achievable_pace` *(kopie uvnitř plánovače)* je opravdu 0.0 / `NOT_CONSULTED`. Ale **`kolo.achievable_pace` a `kolo.required_pace` na úrovni záznamu kola ŽIJÍ** — ověřeno na korpusu: 8,0 vs 4,4 · 6,0 vs 6,33 · 7,0 vs 10,0. ⇒ **veličina existuje a je spočítaná každé kolo**, nespouští se jen plánovač, který ji měl číst. **Nezaměňovat — jinak se zahodí metr, který máme.** |
| T2.5 | N4 kalibrace proti uživateli — 20 kol, shoda ≥ 18/20, **než se agregátu uvěří** | **OTEVŘENO** |
| T2.6 | **X2 + X3** (kostky bloku · deklarovaná makra s pořadím) — jedna oprava odemkne **Z4, Z5, Z9, Z14, S2.14, S10.3** | ⏰⏰ **POVÝŠENO 20.08. večer** — nově **blokuje hlavní hypotézu o kleci**: `expandCage` kotví rohy na `carrier.position` **v okamžiku provedení** (`macro_actions.cpp:1466`), a **nic nevynucuje pořadí CAGE vs ADVANCE** ⇒ zahraje-li se CAGE dřív, rohy se postaví kolem STARÉHO pole a nosič jim odejde *(= kategorie „nosič odešel a rohy nešly s ním", **65,9 % rozpadů**)*. **Potvrdit to jde JEDINĚ pořadím maker v logu, tedy X3.** ⚠️ **Neopravovat před tím testem.** Doklad `evidence/cage_absent_and_collapse_20260820.md` část D. |
| T2.7 | [14] diag binárky staticky — jinak stará binárka tiše měří jiný engine | **OTEVŘENO** |
| **T2.10** | ⭐⭐ **`run_night_ab.sh` — ARM blok hlásí falešný poplach a čte jen 1 shard z 8.** Grepuje `^  ARM `, což je řádek **pouze pro mode 4** (`diag_f1_cage_advance_harness.cpp:528`); v mode 0 neexistuje ⇒ v noci 17.→18.08. se vytiskl fallback „(harness nic netiskl — stará binárka?)“ přesně na řádku, který předregistrace označuje za nejcennější ranní čtení — přitom test proběhl a byl **čistý 8/8**. Navíc `head -1` = jen shard 0, takže leak v shardu 5 by neprobublal. Opravit vzor na `MOVED WITHOUT THE ARM ACTING` (tiskne se v každém režimu) a **sečíst přes všechny shardy**. | **UZAVŘENO** — `056f85ed`; vzor je `MOVED WITHOUT THE ARM ACTING` a čte se **přes všechny shardy** |
| **T2.11** | ⭐⭐⭐ **Nic neslučuje shardy — noc nemá výsledek.** `chain.log` končí `NIGHT DONE` bez sloučené delty; 6 000 párů existuje jen jako 8× ±0,019, tj. **osm jednotlivě neprůkazných čísel**, a skutečnou odpověď musel ráno spočítat člověk. Táž rodina jako audit aparátu: **snímek se vydává za stav** — a je to přesně krok, kde si unavené čtení vybere shard, který se hodí. Chce to `night_summarize`: sloučená delta + sdružená SE + **empirická SE mezi shardy** *(kontrola overdisperze — v noci 0,0053 < 0,0068 ⇒ sloučení legitimní)* + počet záporných shardů + součet leaku + `n_nonzero`. | **UZAVŘENO** — `056f85ed`; `night_summarize.py` — sloučená delta · sdružená SE · empirická SE mezi shardy · počet záporných shardů · součet leaku · `n_nonzero`, **v pořadí čtení z předregistrace**; při leaku se delta NEVYTISKNE |
| **T2.12** | ⛔ **Dva různé prahy v jedné noci.** Harness tiskne natvrdo `[pre-reg: >= +0.03 on dwarf matchups]`, předregistrace na tutéž noc říká **±0,015**. Práh se nikde **strojově nevyhodnotí** ⇒ verdikt zůstává na ranním čtení, což je otevřená branka pro dodatečné doladění. Práh má být **vstup běhu** (proměnná prostředí zapsaná do `chain.log` při startu), ne konstanta ve zdrojáku. | **UZAVŘENO** — `056f85ed`; `THRESHOLD` je vstup běhu, zapsaný do `chain.log` PŘI STARTU, verdikt se vynáší strojově |
| **T2.13** | **Preflight neověří, že kontrola vůbec existuje.** Hlídá mtime binárky i `libbb_engine.so` (T2.7 na úrovni enginu), ale ne to, jestli binárka umí vytisknout `MOVED WITHOUT THE ARM ACTING`. Kdyby neuměla, noc vypadá normálně a **verdikt stojí na kontrole, která neproběhla**. Oprava: sonda 1 páru v cílovém režimu, grep na ten řádek, jinak `exit`. | **UZAVŘENO** — `056f85ed`; sonda na 1 páru v cílovém režimu (47 s); když binárka kontrolu netiskne, noc se **nespustí** (`exit 4`) |
| **T2.19** | ⏰ **FABLE AUDIT PO KLECI — a nad ním DISKUZE** *(uživatel 20.08.: „někdy po kleci provést fable audit a nad ním dizkuzi — možná už naplánované je — ale dnešek ukázal na důležitost")*.<br>⚠️ **Ověřeno, že naplánované NEBYLO** — v paměti jsou jen **minulé** audity *(`simulateGame` parity a narrow-scope-gap, obojí 10.07.)*. ⭐ Uživatel: *„možná byl jen jako zmínka někde — teď bude správně ukotvený v prioritách."*<br>⭐ **Proč audit, a ne další jednotlivé nálezy: audit hledá TŘÍDY.** 20.08. patřila skoro každá vada do **jedné rodiny** — *něco se tiše nepočítá a nula se čte jako fakt*: kola s TD se vyhazovala · po opravě se vyhazovaly ztráty *(asymetrie, kterou jsem vyrobil sám)* · **N/A větší než n** skrývalo hlavní nález **17 dní** · tisk pevného seznamu počítadel · slepené počítadlo *(587 místo 9)* · **13 commitů do plánovače, který se nespouští**. ⇒ **Každou z nich by audit rodiny našel za odpoledne; nám trvaly týdny, po jedné.**<br>**Obsah:** (1) **měřicí aparát** — u každé kontroly *„co je v N/A?"* a *„běží ta cesta vůbec?"* *(navazuje na T2.18)* · (2) **doktrína vs kód** — spec má po 20.08. **15.0b′ · 15.0b″ · 15.0b‴ · 15.0d · ČÁST 17**, kolik z toho engine dělá? · (3) **mrtvý kód** · (4) **rules-parity proti BB2016, NE proti CRP**.<br>⭐ **A nad výsledkem DISKUZE, ne jen zápis** — tři diskuze 20.08. daly **sedm pravidel do spec**, která z měření nevyplynula.<br>⚠️⚠️ **A VÝBĚR PŘÍPADŮ SE MUSÍ ZADAT, NE NECHAT NA AGENTOVI** *(uživatel 20.08.: „spoléháme na to, že vybere zajímavé případy… tuhle pochybnost mi jistě vyvrátíš" — **nevyvrácena, je správná**)*. ⛔ **Hlavní nález 20.08. nevzešel ze zajímavého případu**, ale z toho, že si **dva agregáty odporovaly 24×** — jako „zajímavý" by vybrán nebyl. A **N/A kbelík skrýval hlavní věc 17 dní** právě proto, že v něm nic zajímavého nebylo: **zajímavé se vybírá z VIDITELNÉ populace**. ⇒ Riziko: **agent vybere ILUSTRACI, ne VZOREK**.<br>**Pravidlo do každého zadání:** ⛔ nikdy *„najdi zajímavý případ"*; místo toho **(1)** definuj POPULACI, **(2)** vyber **náhodně** nebo **extrémy na PŘEDEM vyhlášené ose**, **(3)** výběrové pravidlo napiš **do zadání**. | **ODLOŽENO — ⏰ spouštěč: AŽ BUDE KLEC HOTOVÁ** *(P45 · X3 · korpus · P46 · P37 · P44 · P42)*; ne dřív — audit uprostřed rozdělané opravy najde rozdělanou opravu |
| **T2.20** | ⭐⭐⭐ **TRIÁŽ PŘED NOCÍ — ROZHODNOUT, JESTLI ZMĚNA NOC VŮBEC POTŘEBUJE** *(uživatel 25.08.: „to vyhodnocení light testem — jestli daná změna potřebuje celou noc — už bysme měli mít zakomponované i ve strategii, nejen že to umíme")*.<br>**Dnes:** light test dokazuje, že **řetěz dojede**. To je málo — sbírá i čísla, podle kterých jde rozhodnout, jestli má smysl obětovat noc, a **my je nepoužíváme jako rozhodnutí**. Noc je **úzké hrdlo** *(25.08. byly zamluvené tři dopředu)*, takže špatně zamluvená noc stojí den.<br><br>**PROTOKOL (návrh k odsouhlasení):**<br>&nbsp;&nbsp;**(1)** Každé rameno dostane **sondu ~40 párů** dřív, než se mu zamluví noc.<br>&nbsp;&nbsp;**(2)** Čtou se **JEN APARÁTOVÉ veličiny**: `arm_picks` *(je rameno vůbec zapojené?)* · `MOVED WITHOUT THE ARM ACTING` *(leak)* · **`n_nonzero`** *(mění rameno VÝSLEDEK, ne jen rozhodnutí?)*.<br>&nbsp;&nbsp;**(3) Rozhodnutí:** `arm_picks ≈ 0` ⇒ ⛔ **rameno není zapojené**, oprav ho, noc ne · `n_nonzero ≈ 0` ⇒ **rameno nic nemění** ⇒ **noc NE** *(je to refaktor, ne změna)* · `n_nonzero` vysoké ⇒ noc oprávněná · mezi tím ⇒ **větší sonda**, ne rovnou noc.<br>&nbsp;&nbsp;⛔⛔ **(4) NIKDY se nerozhoduje podle DELTY ze sondy.** To by byl výběr podle výsledku. Platí táž hranice jako u přitvrzování kontrol: **aparát se po vidění dat měnit smí, hypotéza ne.**<br>&nbsp;&nbsp;⚠️ **(5) SONDA MUSÍ MÍT DISJUNKTNÍ SEEDY.** Dnes je nemá: sondy i light testy jedou od offsetu 0, tedy **na týchž hrách jako první chunk noci**. Disciplína to zatím udržela *(z light testu se přitvrdila jen aparátová podlaha)*, ale **návrh to má znemožnit, ne spoléhat na kázeň**. Oprava: vyhradit sondám vlastní pásmo offsetů.<br><br>⛔⛔⛔ **A CO SE PŘI PSANÍ TOHOHLE NAŠLO: PRÁH ±0,015 A DÉLKA NOCI SPOLU NESEDÍ.**<br>Kalibrace z noci 24.→25.08. *(6 800 párů → SE 0,0060)*, práh je vstup běhu **±0,015**:<br><pre>párů    SE       práh 0,015   hodin
| **P58 = FÁZE-3** | ⭐⭐⭐ **SPOUŠTĚČ VÝBĚHU Z KLECE JE SPOČÍTANÁ REZERVA, NE ČÍSLO KOLA** *(rozhodnuto v rozhovoru 25.08.)*. Uživatel dodal chybějící proměnnou: *„tam se to dá odvodit od počtu zbývajících kol a počtu polí k TD"* a *„a MA runnera s míčem"*.<br>**⛔ PRVNÍ FORMULACE BYLA O DVĚ KOLA POZDĚ — opraveno týž den.** Napsal jsem *„vyjít v posledním kole, kdy `achievable ≥ required`"*. Uživatel 25.08.: *„když rozvrh nestíháme, je už pozdě ptát se — musíme se ptát dříve a rozhodnout se riskovat."* **Má pravdu:** v tom kole je rezerva **nula**, takže výběh musí vyjít **napoprvé** — jeden neúspěšný dodge a drive je mrtvý.<br>**⭐ PLATNÉ PRAVIDLO:** vyjít v **posledním kole, kdy rozvrh PŘEŽIJE JEDEN NEZDAR** — tedy dokud je rezerva `achievable − required` větší než cena jednoho zaváhání.<br>**Změřeno 25.08.** *(500 her, trpasličí kola)* — rezerva se hroutí mezi 4. a 5. kolem:<br><pre>kolo   kol   medián  25.pct  unese >=1  unese >=2
  2    471   +4,33   +4,00     100 %      100 %
  3    482   +3,60   +3,20     100 %       99 %
  4    461   +2,50   +2,00      97 %       80 %
  5    439   +1,00   +0,33      57 %       21 %
  6    417   -1,50   -2,50      10 %        5 %
  7    408   -8,00  -11,00       2 %        1 %</pre>⇒ **rozhodovací bod je KOLO 4** *(medián +2,50, osm z deseti kol unese dvě pole navíc)*, ne kolo 6. ⚠️ A my dáváme **68 % svých TD až v kole 7-8** — rozhodujeme se tedy hluboko za bodem, kdy bylo riziko únosné.<br>⭐⭐⭐ **A TÍM SE TO SPOJÍ S `P59` DO JEDNOHO TVARU:** *neodhaduj, co udělá soupeř — dopočítej, kam dosáhne* · *neodhaduj, jestli ti to vyjde — dopočítej, jestli přežiješ, když nevyjde.* **V obou případech se pravděpodobnost nahrazuje DŮSLEDKEM** — a důsledek se dá spočítat z desky, kdežto „jaká je šance" ne.<br>*(Původní znění pro pořádek: vyjít v posledním kole, kdy `achievable_pace ≥ required_pace`.)* ⛔ **NE zadrátovaná hrana typu „v kole 6 jdi"** — to je konstanta, tedy přesně to, co jsme si zakázali u obranného D1→D2. Je to **poměr dvou veličin, které se už teď počítají každé kolo**.<br>**Změřeno 25.08.** *(150 her, trpasličí kola, 289 kol s platným tempem)* — podíl kol, kde je výběh ještě dokončitelný:<br><pre>kolo   2-4    5      6      7      8
| **P59 = HORIZONT** | ⭐⭐⭐ **SOUPEŘOVA ODPOVĚĎ SE NEODHADUJE, DOPOČÍTÁVÁ SE — A STAČÍ JEDNO KOLO** *(uživatel 25.08.: „nic nemusíš odhadovat, co udělá soupeř — vždy dopočítáš, kam dojde; jestli má možnost přihrát na někoho v dosahu TD… vždy ti stačí tvoje aktuální kolo a co na to soupeř v jeho následujícím — to vše dopočítáš a dál počítat nemusíš")*.<br>**Pravidlo má tři části a všechny jsou spočítatelné z desky:**<br>&nbsp;&nbsp;**(1) HORIZONT = JEDNO SOUPEŘOVO KOLO.** Ne víc. Dál se nepočítá, protože dál už to je odhad.<br>&nbsp;&nbsp;**(2) BĚH:** kam každé jeho **stojící** tělo dojde *(MA + GFI, + Leap)* — a jestli mezi tím je míč a odtud endzóna.<br>&nbsp;&nbsp;**(3) ⭐ PŘIHRÁVKA: má KOHO, kdo je už teď v dosahu TD?** ⛔ **Tohle nám v celé doktríně chybí** — je to týž kanál, na který ukazuje otevřená otázka u T5.13: *„celá naše doktrína řeší kontakt a proti přihrávce nedělá nic; pasovací hrozba není v žádné z našich kontrol."*<br>⚠️ **BYLO ZAPSANÉ JEN Z POLOVINY.** `P47` nese uživatelův dřívější vstup *„vážený dosah soupeře (max MA těch, kdo dosáhnou, + Leap)"* — ale **jen pro rozhodnutí „skórovat hned vs posunout klec"** a **bez přihrávky**. ⇒ ⭐ **Není to nová myšlenka, je to ZOBECNĚNÍ té staré na obecné pravidlo** — a mezitím se nepoužila ani v tom užším případě.<br>⛔ **DŮSLEDEK PRO MĚŘENÍ (4) Z 25.08.: MĚLO ŠPATNÝ TVAR.** Počítal jsem `vzdálenost / zbývající kola ≤ MA + 2` — tedy **tempo na několik kol**, což je odhad. Správná veličina je **binární a jednokolová**: *když míč ztratíme tady, skóruje soupeř ve SVÉM PŘÍŠTÍM kole — během, nebo přihrávkou?* ⇒ číslo **64 %** se nemá citovat, přepočítat v tomhle tvaru.<br>⭐⭐ **A JE TO ZRCADLO OBRANNÉ DOKTRÍNY.** Uživatelovo *„mít nachystané hráče blízko, aby se situace dala potrestat"* je **tentýž dopočet z druhé strany**: co dosáhne on ⇒ kde musíme stát my. ⇒ útok i obrana stojí na **jedné funkci**, ne na dvou doktrínách. <br>⛔⛔ **A PROTO JE TO NALÉHAVÉ, NE AKADEMICKÉ: JEDNOKOLOVÁ HROZBA (OTT) JE PŘESNĚ TENHLE HORIZONT.** *(doplněno 25.08. — slíbil jsem to zapsat a neudělal to hned; zapsáno až na uživatelovu připomínku)*<br>⛔ **OPRAVA TÉHOŽ DNE — pletl jsem DVA RŮZNÉ DOPOČTY** *(uživatel 25.08.: „počkej, tady pleteš dvě věci do kopy")*. Napsal jsem, že OTT Gutter Runner *„skóruje z libovolného místa desky"*. **To platí, jen když už míč MÁ.**<br>&nbsp;&nbsp;**(A) MÍČ DRŽÍME MY** — soupeř musí **začít odebráním**, a to ho stojí **blitz, přiblížení k nosiči a hod**, plus zvednutí volného míče. ⇒ počítá se **ZBYTEK** jeho tahu proti vzdálenosti k endzóně, ne celých 13 polí. ⭐ A sem patří uživatelovo *„elf má být rozestaven tak, že když nám sebere míč, dá TD hned"*: **elf tu cenu odebrání PŘEDPLÁCÍ rozestavením** — postaví příjemce dopředu, aby po blitzu zbývalo jen hodit. *(Viz `P62`.)*<br>&nbsp;&nbsp;**(B) MÍČ MÁ ON, NEBO LEŽÍ VOLNÝ** — **čistý dosah**: MA + GFI + Leap, bez ceny odebrání. **Tady** platí „z libovolného místa" a **tady** je OTT.<br>⇒ ⭐⭐ **Dopočet má tedy DVĚ VĚTVE podle toho, kdo drží míč** — a splést je znamená přecenit hrozbu ve větvi (A) a podcenit ji ve větvi (B).<br>⛔⛔ **A DRUHÁ OPRAVA TÉHOŽ DNE — VE VĚTVI (A) NEPOTŘEBUJE OTT VŮBEC** *(uživatel: „v tom nepotřebuje OTT")*. Ke skóre v témž tahu po odebrání stačí **blitz, po kterém je míč volný u jeho lidí, a příjemce už stojící v dosahu TD**. To je **obyčejná elfí sestava, dnes, bez přestavby rosterů** — a `P62` to i naměřilo: wood-elf **6,2 %**, skaven **5,5 %** ze sražených nosičů, **bez OTT**.<br>⇒ ⭐⭐⭐ **NALÉHAVOST P59 NEZÁVISÍ NA T5.13.** Napsal jsem *„musí být hotové dřív, než se rostery přestaví, kvůli OTT"* — postavené na **špatné větvi**. Nebezpečná věc není budoucí roster, je to **dnešní elf, který ten plán hraje pořádně**; OTT je až (B) a přijde s přestavbou. ⇒ **(A) je živá TEĎ a nikdo nás v ní netestuje.**<br>Dnes jednokolový TD nikdo nedá *(skavení GR má 11 polí, potřebuje 13 — `T5.13`)*, takže „vydrž do osmého kola" zatím vychází i ve větvi (B).<br>⇒ ⭐⭐ **P59 musí být hotové DŘÍV, než se rostery přestaví, ne potom.** Je to jediná obrana, kterou proti OTT máme — bez dopočtu jeho dosahu se nedá ani poznat, že je ohrožení. A `P62` k tomu dodává druhou půlku: soupeř ten plán dnes **nehraje**, takže nás nic nevaruje. | **OTEVŘENO — ⏰⏰ PRIORITA: větev (A) je živá DNES, nezávisle na T5.13** |
| **P60 = ZA ROZVRHEM** | ⭐⭐⭐ **KDYŽ UŽ NESTÍHÁME, MĚNÍ SE ÚČEL TAHU — a dnes se nemění nic** *(uživatel 25.08.: „když už nestíháme, tak držíme míč v kleci a posunujeme dopředu, ale tentokrát bez rizika a hlavně mlátíme — ale to už jsem taky odpovídal takto")*.<br>**Doktrína má za rozhodovacím bodem DVĚ větve:**<br>&nbsp;&nbsp;**(A) rozvrh přežije nezdar** ⇒ **vyjdi** *(P58, rozhodovací bod = kolo 4)*<br>&nbsp;&nbsp;**(B) nestíháme** ⇒ ⛔ **nehazarduj**: drž míč v kleci, posunuj **bez rizika**, a **hlavní hodnotou tahu se stává OPOTŘEBENÍ** — ne postup. TD v téhle půli nepřijde; co přejde do další, jsou **jejich zranění**.<br>⛔⛔ **ZMĚŘENO 25.08. — VĚTEV (B) V CHOVÁNÍ NEEXISTUJE** *(500 her, trpasličí kola, bloky na kolo podle toho, jestli `achievable >= required`)*:<br><pre>kolo  stav          kol   bloků/kolo  sražení/kolo
| **P61 = MÁLO TD** | ⚠️ **CELÝ KORPUS SKÓRUJE ŘÁDOVĚ MÍŇ, NEŽ BY MĚL** *(uživatel 25.08.: „wood-elf skóruje 0,26× za 16 kol? — málo. Ale řešit budeme později.")*.<br>**Změřeno na 18 000 hrách, TD na NÁSTUP rasy** *(ne na hru korpusu — viz past níž)*:<br><pre>wood-elf 0,655 · skaven 0,564 · human 0,311 · ork 0,165 · trpaslík 0,160</pre>⇒ **wood-elf dá TD ve dvou zápasech ze tří.** Skutečný elfí tým dává **1-2 TD za zápas**. A je to **napříč rasami**, tedy vada **společné vrstvy** (rozhodování / skórovací řetěz), ne rasové sestavy.<br>⭐ Sedí to na dřívější měření z 21.08. *(TD 0,740 → 0,443 po pravidlových opravách)* a na `project_bloodbowl_where_lost_20260820` *(v 89,9 % nevyhraných máme NULU)*.<br>⏰ **ODLOŽENO uživatelem 25.08.** — *„řešit budeme později"*. ⛔ Ale je to **kandidát na velký okruh**, ne drobnost: kdyby skórovací řetěz nefungoval, měří se všechno ostatní na hře, která se nedohrává.<br><br>⛔⛔ **A PAST, KTERÁ TOHLE ČÍSLO NEJDŘÍV SCHOVALA — JMENOVATEL.** Nejdřív jsem dělil **počtem her korpusu (18 000)** a vyšlo `wood-elf 0,262`. Jenže **rasa nehraje ve všech hrách**: 15 dvojic z 5 ras ⇒ každá nastoupí v **5 dvojicích**, a ve své zrcadlové na **obou stranách** = **7 200 nástupů**. ⇒ číslo bylo **2,5× menší** a vypadalo úplně věrohodně. **Našel to uživatel tím, že mu to přišlo málo.**<br>⭐ **Kontrola, která to potvrdila:** opravená čísla sedí **přesně** na nezávislé měření z 24.08. *(wood-elf 0,655, dwarf 0,160)*. ⇒ **shoda dvou nezávislých průchodů je levný a silný test jmenovatele.** | **ODLOŽENO — uživatel 25.08., „později"** |
| **P62 = ZMĚKČENÝ SOUPEŘ** | ⛔⛔ **SOUPEŘ NEHRAJE JEDNOKOLOVOU HROZBU — a naše obranná měření tím stojí na změkčeném protihráči** *(uživatel 25.08.: „elf má být rozestaven tak, že když nám sebere míč, má být schopen hned to samé kolo dát TD — plán na celotah. Skaven taky.")*.<br>**Změřeno na 3 300 hrách ze VŠECH 15 dvojic** — soupeřovo kolo, na jehož začátku držíme míč my; srazí nosiče a skóruje **týž tah**?<br><pre>útočí      kol s cizím míčem  srazil nosiče  TD TÝŽ TAH  podíl
wood-elf              10 635          2 043        127    6,2 %
skaven                11 109          1 961        108    5,5 %
human                  9 857          1 961         53    2,7 %
ork                    9 124          1 797         10    0,6 %
trpaslík               9 527          2 109          9    0,4 %</pre>⭐ Rozdíl rychlí vs pomalí je **desetinásobný**, takže ta schopnost **reálná je**. Ale **jen 6 %** — a obě strany běží přes **tutéž makro vrstvu**, která pojem *„postav příjemce tak, aby se odebrání a TD vešly do JEDNOHO tahu"* **nemá vůbec** ⇒ těch 6 % nejspíš vzniká **náhodou, ne plánem**.<br>⇒ ⛔ **STROP NA DŮVĚRYHODNOST VŠECH OBRANNÝCH MĚŘENÍ.** Klec, rohy, zeď i L vypadají bezpečněji, než jsou, protože protihráč nedělá **to nejnebezpečnější, co proti nám dělat může**.<br>⭐⭐ **Je to TÁŽ VADA JAKO U ROSTERŮ (`T5.13`), jen o patro výš:** tam soupeři chybí **výbava**, tady mu chybí **plán**. Dohromady měříme proti změkčenému soupeři **na dvou úrovních naráz**.<br>⚠️ **A po přestavbě rosterů to přestane být 6% náhoda:** OTT Gutter Runner *(MA10 + Sprint = 13 polí)* dělá z jednokolového TD **záměr**. ⇒ **P62 se musí řešit spolu s T5.13, ne po něm.**<br>⇒ Vstup pro `P59` (HORIZONT): dopočet soupeřova dosahu musí počítat **odebrání i skóre v jednom tahu**, ne „vezme teď, skóruje příště".<br><br>⭐⭐⭐ **UPŘESNĚNO UŽIVATELEM 25.08. — NEMÍCHAT OTTD S TÍMHLE, A NEJDE TO VŠEM STEJNĚ:** *„elfové OTTD nemají… jejich je dát TD hned ten sám tah, co seberou míč; skavenům to jde hůř včetně toho sebrání míče."* **Ověřeno na sestavách TV1200:**<br><pre>              kdo bere míč                     proč to jde / nejde
wood-elf   Wardancer MA8 ST3 AG4 ×2          ST3 proti našemu ST3 = VYROVNANÝ blitz,
           Block+Dodge+LEAP                  kostku nevybíráme my; Leapem projde clonou
skaven     Gutter Runner MA9 ST2 ×4          ST2 proti ST3 je DO KOPCE => kostku
                                             vybíráme MY
           Blitzer ball-hunter MA7 ST3       ST3 má, ale MA7 a zadny Dodge ani Leap
           Block+StripBall+Tackle            => ke kleci se hur dostane</pre>⇒ ⭐⭐ **Skaven má RYCHLOST BEZ SÍLY a SÍLU BEZ RYCHLOSTI; elf má obojí na jednom těle.** Proto je **elf v téhle větvi nebezpečnější než skaven**, ač skaven vypadá rychleji. *(Sedí to na zápis ze 14.08.: „nástroj bez dosahu a dosah bez nástroje".)*<br>⭐ **A příjemce elf má:** `Catcher MA8 ST2 AG4, Catch+Dodge+SPRINT` = **11 polí**. Není to OTTD *(to chce 13)*, ale je to přesně to tělo, které má **stát nachystané vepředu**.<br>⇒ **Elfí plán bez jakéhokoli OTTD:** Wardancer sebere *(vyrovnané kostky, Leap na cestu)* → Catcher už stojí v dosahu → **hod, nebo doběh**. Uživatel: *„elf musí hodit, skaven spíš předat a doběhnout — a oba musí být předem nachystaní."* | **OTEVŘENO — ⏰ strop na obranná měření** |
  5   na rozvrhu    392      2,90        1,55
  5   ZA rozvrhem    47      2,79        1,32
  6   na rozvrhu     86      3,26        1,70
  6   ZA rozvrhem   331      3,30        1,81
  8   ZA rozvrhem   347      3,04        1,63</pre>⇒ v **kole 6**, kde je srovnání nejpoctivější *(86 proti 331 kolům)*, mlátíme **3,26 vs 3,30** — **rozdíl žádný**. Bloky sice s číslem kola rostou *(1,48 → 3,3)*, ale to je efekt **kola**, ne rozvrhu. **Uvnitř téhož kola se nemění nic.**<br>⇒ ⭐ **Chybí obě větve naráz:** nevyjdeme, když je čas *(P58)*, a nepřepneme na opotřebení, když už je pozdě *(P60)*. **Engine mele jedno tempo bez ohledu na to, jestli plán ještě žije.**<br>⚠️ **A ZASE BYLO ZAPSANÉ JEN Z POLOVINY:** spec **§8.4** má *„je tah zaseknutý a řeší se jinak — doplnit, držet, mlátit"*, ale jen jako **záchranu jednoho tahu** po nepovedeném prolomení zdi, ne jako **větev doktríny podle rozvrhu**. *(A o dva řádky výš tamtéž leží další kus `P59`: „sólový výběh nelze potrestat, když po něm soupeř už nemá tah.")* ⇒ **potřetí dnes: doktrína zapsaná v úzkém případě a nezobecněná.** | **OTEVŘENO — ⏰ druhá půlka P58** |
      100 %  90,1 % 32,5 %  4,2 % 23,3 %</pre>⇒ okno se zavírá **mezi 5. a 6. kolem** *(90 % → 33 %)*, a ostře.<br>⛔ **OPRAVENO 25.08. TÝŽ DEN — první výpočet byl na špatných kolech.** Filtr zněl `t["active_team"] == 0`, jenže **`active_team` je ŘETĚZEC `"home"`/`"away"`**, ne číslo. ⇒ podmínka byla vždy nepravdivá: u trpaslíka-doma se přeskočila **všechna** kola, u trpaslíka-venku se braly **i soupeřovy**. Vzorek 289 kol → po opravě **913**. ⭐ **Tvar závěru se nezměnil, čísla ano** — a MA rozpad stojí teď na **54/87/772** kolech místo 9/13/137, takže se smí citovat.<br>⚠️ **Poučení do metodiky:** typ pole se OVĚŘUJE, nepředpokládá. Tichý filtr, který vyhodí všechno, vypadá v tabulce úplně normálně — táž rodina jako `feedback_na_bucket_is_the_finding`.<br>⭐⭐ **A ROZPAD PODLE MA NOSIČE MĚNÍ ZÁVĚR — uzávěrka není číslo kola, je to funkce toho, KDO NESE:**<br><pre>MA nosiče  kol  dokončitelné  ⌀dosaž.  ⌀potř.  kdo
   4        54      33,3 %      4,54    8,57   Longbeard
   5        87      41,4 %      5,71    9,23   Blitzer +Guard+Tackle
   6       772      71,9 %      7,03    6,50   Runner +Block</pre>⇒ **S pomalým tělem v ruce fáze 3 prakticky NEEXISTUJE** *(potřebné 8,57 proti dosažitelnému 4,54 = musel by běžet skoro dvakrát rychleji, než umí)*. **Správná odpověď tam není „vyjdi", ale „PŘEDEJ Runnerovi"** — nebo nehrát na TD. ⭐ A harness už každou noc tiskne `HAND_OFF offer evals ~14/hru — nabídnuto`, s poznámkou *„když se žádný nezahrál, zabíjí to HLEDÁNÍ"*. ⇒ **předání se nabízí a nehraje.**<br>⭐ V **84,6 %** *(772/913)* nese Runner ⇒ doktrína „míč patří Runnerovi" v praxi drží; špatné případy jsou situace **po ztrátě**, kdy míč sebral kdokoli hluboko *(vysoké `⌀potřebné` u pomalých nosičů tomu odpovídá)*.<br>Korpus je **předpravidlový (21.08.)**, ale je to aritmetika vzdálenosti a kol, ne výsledky hodů ⇒ tvar uzávěrky by měl držet. A **kvalitu `achievable_pace` nikdo neověřil** — je to odhad enginu.<br>⇒ **CO ZBÝVÁ:** veličina existuje *(viz upřesnění u T2.4)*, chybí ji **přečíst** a udělat z ní rozhodnutí. | **OTEVŘENO — ⏰ metr existuje, chybí ho zapojit** |
  800   0,0175      0,9 σ         2,0
 3000   0,0090      1,7 σ         7,6
 4800   0,0071      2,1 σ        12,2
 6800   0,0060      2,5 σ        17,2
 9600   0,0050      3,0 σ        24,3</pre>⇒ **Efekt PŘESNĚ NA PRAHU není v jedné noci rozlišitelný od nuly na 3σ** — potřeboval by **24 hodin**. Dosud to nekouslo, protože nálezy byly řádově větší *(P38 = +0,082 = 13,6σ)*. ⚠️ **Ale u malého ramene bychom „POMÁHÁ" vyhlásili na ~2σ**, a to je přesně ta síla, kterou jinde odmítáme.<br>⇒ ⭐ **Důsledek pro triáž:** k rameni se má PŘEDEM napsat, **jak velký efekt nás zajímá**. Když je blízko prahu, **jedna noc na to odpovědět neumí** — buď se přizná 2σ, nebo se noc nezamlouvá. <br><br>⭐⭐⭐ **DRUHÁ POLOVINA TÉŽE METODIKY: SMÍ SE BĚHY ZŘETĚZIT?** *(uživatel 25.08.: „to zřetězení mi přijde ošemetné — dá se tam najít výsledek testů změn za sebou pro každou z nich zvlášť?")* — instinkt byl správný.<br>**✅ CO ZŘETĚZENÍ UNESE.** Každé párové A/B srovnává proti **témuž vypnutému stavu** *(ramena jsou default OFF)*, takže „základní" rameno je v obou bězích **bit po bitu tentýž engine**. ⇒ delta z běhu #1 a #2 jsou **každá svá a správně přiřaditelné**. Nevadí ani různé seedy per mode *(8 = 149M, 9 = 163M)*: jsou to jiné hry, ale oba odhady jsou nestranné na témž enginu a matchupu, takže se smí i **porovnat mezi sebou** se sdruženou SE.<br>⛔ **CO NEUNESE — a je to PŘESNĚ NÁŠ PŘÍPAD.** Zřetězení **nedá efekt KOMBINACE**: změří `X sám` a `Y sám`, ale `efekt(X+Y) ≠ efekt(X) + efekt(Y)`, kdykoli se ramena potkávají na téže mechanice. **P35** rozhoduje, **KOHO pošleme blitzovat**; **M1/N10** rozhoduje, **co ten blitzující dělá POTOM** — hodnota ústupu závisí na tom, kdo byl vyslán, a výběr blitzujícího by se měl změnit, když se smí ustoupit. ⇒ **kdybychom po víkendu zapnuli obojí, nasadíme stav, který nikdo neměřil.**<br>⇒ ⭐⭐ **PRAVIDLO: zřetězit se smí ramena z RŮZNÝCH mechanik. Ramena z téže mechaniky potřebují běh navíc na KOMBINACI — a ten stojí tolik co oba předchozí.**<br>⛔⛔ **A PAST V POŘADÍ, kterou jsem sám navrhl a je špatná: KORPUS NEPATŘÍ NA KONEC ŘETĚZU.** Korpus se sbírá s produkčním nastavením, tedy **s rameny VYPNUTÝMI** ⇒ popisuje **základní stav**, ne ten, který se po vyhodnocení nasadí. V okamžiku, kdy se cokoli zapne, je **31 h sběru zastaralých** — doslova to, co se stalo 21.-22.08. *(korpus stál 31 h a vydržel DVA DNY)*. ⇒ **korpus patří AŽ ZA ROZHODNUTÍ, ne za měření.**| **OTEVŘENO — ⏰ metodika, patří do rozhovoru** |
| **T2.18** | ⏰⏰ **KONTROLY MUSÍ UMĚT ŘÍCT, CO JE V JEJICH N/A** *(uživatel 20.08.: „kdyžtak tyto kontroly pak rozšiř — dnes to jen zapiš do plánu a dej tomu prioritu")*.<br>⛔ **Proč:** klecové kontroly měly jednotku *„kola s POSTAVENOU klecí"* — **n = 16 517, N/A = 27 575 (63 %)**. Otázka *„jak často klec nestojí vůbec"* byla **definičně mimo každý metr** — ne přehlédnutá, **neměřitelná**. Stálo to **17 dní** práce na posunu klece, než se to spočítalo jinudy. ⚠️ Audit 13.08. to **napůl spravil** *(přidal N/A, aby se prázdná množina nepočítala jako splněno)* — **zviditelnil kbelík a nechal ho zavřený**.<br>**Zadání, dvě části:**<br>**(a) OBECNĚ, levné:** `Check.line()` ať **sám hlásí, když `deg > n`** — *„⚠️ N/A je větší než měřená množina, nález je nejspíš tam"*. Jedna podmínka, chytá to celou rodinu dopředu.<br>**(b) ADRESNĚ u klece:** N/A populace *(klec nestojí)* se má reportovat **jako plnohodnotné číslo s rozpadem příčin**, ne jako prázdno — kategorie už existují v `diag_cage_absent_20260820.py` *(tělo ortogonálně 49,2 % · dosáhli by a nestojí tam 48,5 % · nedosáhne 2,1 % · leží 0,2 %)*.<br>⭐ **A druhá otázka k téže rodině:** u každé kontroly se ptát i **„běží ta cesta vůbec?"** — 20.08. jsem na mrtvý kód mířil **dvakrát**. | **OTEVŘENO — ⏰⏰ PRIORITA, zadáno 20.08.** |
| **T2.15** | **Pevné dělení shardů nechává jádra stát.** Shard dostane pevný blok seedů, ale zápasy nejsou stejně dlouhé ⇒ shardy se rozejdou a běh čeká na nejpomalejšího. Změřeno: **17.08.** rozptyl **2,8 h** (první shard 11,9 h, poslední 14,7 h ze 14,7 h běhu); **18.08.** po 4,3 h rozptyl už **125 párů** (350 vs 225). ⇒ Polovina jader stojí poslední hodiny naprázdno a **odhad konce se řídí nejpomalejším shardem, ne průměrem** (dnes ~01:16 vs ~04:23 SELČ). ⇒ Nahradit pevné dělení **frontou úloh**: víc menších shardů, worker si bere další, až dodělá. Levné (mění se jen `run_one` a smyčka v `run_night_ab.sh`), a zkrátí noc o ty hodiny doběhu. ⚠️ **Musí zachovat CRN**: seed se váže na index páru, ne na pořadí zpracování, jinak se rozbije párování.<br>⭐⭐ **A hlavně: NEMĚNÍ N** *(uživatel 20.08.: „jestli by noc zkrátit mělo za následek spadnutí výsledku do šumu, jsem proti zkrácení noci“)*. Párů zůstává **6 800**, SE i síla beze změny — ubírají se **jen prostoje**. ⛔ **Ubrat páry se nesmí:** 2 pp potřebuje 5 300 párů, 1 pp rovných 21 200. | **OTEVŘENO — zapsáno 18.08.** |
| **T2.14** | **Předregistrované předpovědi se nikde nekonfrontují s výsledkem.** Noc 17.→18.08.: dvě ze šesti (K9a tempo dolů, bloky nahoru) byly **nezodpověditelné**, protože běh měl `CORPUS=0`; a minutá předpověď `n_nonzero` **62,8 % vs čekaných >80 %** je informace o rameni *(brána sahá na méně kol, než jsme mysleli)*, která by bez zápisu propadla. Buď `CORPUS=1`, nebo předpovědi na korpus z předregistrace vyškrtnout — a přidat krok „předpověď vs výsledek“ do shrnutí. | **UZAVŘENO 18.08.** — uživatel: *„nemělo toto být součástí oprav s prioritou? dej tomu prioritu"*. Předregistrace je teď **vstup běhu** (`PREREG=soubor`): (1) spouštěč ji zapíše do `chain.log` PŘI STARTU a **odmítne se spustit**, když nějaká předpověď potřebuje korpus a `CORPUS=0` (exit 6) — chytne to za minutu místo za 14 h; (2) `night_summarize.py` na konci tiskne **PŘEDPOVĚĎ vs VÝSLEDEK** se štítky **TREFA / MIMO / NEZODPOVĚDITELNÁ**. Ověřeno zpětně na noci 17.→18.08.: vytiskne přesně ty čtyři vady (delta MIMO, `n_nonzero` MIMO, 2× nezodpověditelná). Testy **29/29 → 35/35**. |
| **T2.16** | ⭐⭐ **„NEROZHODNUTO" a „PRAKTICKY NULA" se tisknou stejným slovem.** `night_summarize.py:117` porovnává **jen bodový odhad** s prahem (`ŠKODÍ / POMÁHÁ / NEROZHODNUTO`) a **nekouká na CI**. Noc 18.→19.08.: delta **+0,0017 ± 0,0060**, 95 % CI **[−0,0100; +0,0134]** ⇒ **celé uvnitř prahu ±0,015**. To není „nemáme sílu" — je to **doložená ekvivalence** (TOST projde i na 90 % CI). ⇒ Dva opačné příkazy k akci pod jedním slovem: *málo párů* → přidej páry a opakuj noc; *CI uvnitř prahu* → **zastav, rameno předpovězený efekt nedodává**. Cena záměny je **další 14hodinová noc**. Oprava: čtvrtý verdikt **EKVIVALENCE (|CI| ⊂ práh)** a pátý **NEROZHODNUTO — MÁLO SÍLY (CI přesahuje práh)**, plus dopočet, kolik párů by na rozhodnutí ještě chybělo. ⚠️ Drobnost k témuž souboru (`:81`): `arm acted 6799/6800` se vytiskne jako **„100,0 % ⚠️ neúplný jmenovatel"** — varování a číslo si odporují; chce toleranci nebo tisk rozdílu v kusech. | **UZAVŘENO 19.08.** — `night_summarize.py` má **čtyři verdikty**: ŠKODÍ · POMÁHÁ · **EKVIVALENCE** *(CI celé uvnitř prahu ⇒ „PŘÍKAZ: ZASTAV", plus povinná výhrada „NENÍ to nula: efekt až do meze CI vyloučit neumíme")* · **NEROZHODNUTO — MÁLO SÍLY** *(CI přesahuje práh ⇒ dopočte, kolik párů by při nezměněném odhadu chybělo)*. Noc 18.→19.08. se přečte jako **EKVIVALENCE**. Testy **35/35 → 42/42**. |
| T2.8 | E1/E2 jako K34/K35 | **UZAVŘENO** — `bc9cf17` |
| T2.9 | **K36 `LOCKED`** — zamčená vlastní těla jako chybějící člen tempa | **UZAVŘENO** — `bc9cf17`; potvrzeno na 3000 hrách, monotónní: ≤2 → Δx **+2,18** (n=15314) · 3–5 → **+1,89** (n=2162) · 6–8 → **+1,06** (n=50) |
| K9b | **ODPOR SE MUSÍ POČÍTAT MIMO BRÁNU — zadáno 18.08.**<br>**Co odpor je:** kolik soupeřových **stojících** těl stojí v koridoru před nosičem (`CORRIDOR_DEPTH` vpřed, `CORRIDOR_HALF_WIDTH` do stran). Každé stojí blok nebo obloučkovou cestu navíc, tedy tempo — je to to, co odlišuje „tři pole prázdným polem“ od „tří polí skrz zeď“.<br>**Kde dnes žije:** počítá se **jen uvnitř plánovače klece** (`cage_advance.cpp:479–486`); jinde se už jen přenáší (`turn_plan_record.h:36`, `bb_module.cpp:272`). V produkci je brána vypnutá ⇒ `plán: NOT_CONSULTED` ve **100 %** kol ⇒ **odpor je vždy 0** a K9b nemá z čeho měřit.<br>⛔ **Blokátor se dnes stal TRVALÝM:** K9b čekala na T3.1, ale T3.1 byla 18.08. **zamítnuta** (brána škodí) — takže věc, na kterou čeká, se už nikdy nezapne. Čekat dál znamená položku tiše pohřbít.<br>⇒ **Zadání:** vytáhnout výpočet odporu do samostatné funkce nezávislé na plánovači, aby existoval v každém našem kole bez ohledu na to, jestli brána běží. Je to pár řádků (dnes jedna `forEachOnPitch` lambda).<br>⭐ **Není to volba, je to PŘEDPOKLAD T0.1:** přepis K9 na fáze potřebuje odpor — podlaha fáze „klec“ se bez něj nedá napsat jinak než konstantou, a konstanta je 18.08. zakázaná. | **UZAVŘENO 18.08.** — `corridorResistance()` je volná funkce v `cage_advance.{h,cpp}`; plánovač volá **tutéž** (jediná definice), a `captureTurnSnapshot` ji počítá **každé naše kolo s míčem** do nového pole `TurnLog::corridorResistance` *(vlastní pole, ne `plan.resistance` — simulátor přepisuje `curLog.plan` z thread-local záznamu, takže cokoli tam orazítkované by přepsaly nuly)*; −1 = N/A, ne nula. Export do pythonu `corridor_resistance`. Ověřeno na 3 hrách: `plan: NOT_CONSULTED` ve 100 % kol a **odpor přesto naměřen** (36 kol, ⌀ 1,44, max 5). Testy **544 → 545**. |
| K32 | blitz se v logu nepozná od bloku | **BLOKOVÁNO** na X1 |
| **P32** | ⭐⭐⭐ **KLEC SE POSOUVÁ JEN ROVNĚ KUPŘEDU — SMĚR SE NEVOLÍ** *(uživatel 18.08.: „bod 1 nesmí škodit — musí stavět čistou klec a kdyžtak ne vždy jen přímo rovně kupředu“)*. Ověřeno v kódu 18.08.: cíl posunu vzniká na dvou místech (`cage_advance.cpp:41` v `cageExposure` a v `tryAssign`) vždy jako **`dest{carrier.x + dx*step, carrier.y}`** — **`y` se NIKDY nemění**. Plánovač tedy volí jen **JAK DALEKO**, nikdy **KAM**: všechny zvažované cíle leží na jedné přímce vpřed. Řádek 548 sice vybírá „nejméně exponovaný cíl“, ale jen mezi kroky po té přímce.<br>⭐ **To je pravděpodobný mechanismus, proč brána postavila víc klece a HORŠÍ klec** (rohů 2,22→2,54, čistota 79,4→72,6 %): když rovně vpřed nejdou rohy udržet čisté, plánovač nemá jak uhnout do strany — umí jen zkrátit krok, nebo vetovat.<br>⚠️ **Táž rodina jako P9** (`choosePushSquare` = „rovně dozadu první“, cílové pole se nehodnotí): **geometrie se nevybírá, jen se vykoná.**<br>⇒ Zadání: cílem posunu smí být i pole do strany/šikmo; kritérium výběru je **čistota rohů v cíli** (počet ŠPINAVÝCH rohů, ne počet rohů — σ-tabulka 18.08.), při shodě dál vpřed. | **OTEVŘENO — zadáno 18.08., přímý vstup pro T3.1** |
| **P39** | ⛔⛔ **NOSIČ SE V KOLE VŮBEC NEAKTIVUJE — a je to největší ztráta tempa, jakou jsme změřili** *(19.08.)*. Zpětný rozvrh z **mechanických** stropů *(SÓLO a VÝBĚH = MA nosiče · KLEC = min(MA rohů, MA nosiče); uživatel 19.08.)* ukazuje, že rozvrh je nesplnitelný jen ve **27,6 %** kol — ne v 95,8 %, jak vycházelo z historického tempa. ⇒ **Drivy neprohráváme rozvrhem, ale nevyužitím.**<br>**Ze splnitelných kol se nosič v 37,7 % NEHNUL VŮBEC.** A z kol s Δx = 0 *(1 641, vzorek 800 her)*:<br>• **nosič NEJEDNAL VŮBEC — žádná událost: 83,8 %**<br>• z toho byl přitom **úplně volný, bez jediné soupeřovy TZ: 58,3 %**<br>• v soupeřově TZ (dodge by stál hod): 41,7 %<br>⇒ **Nosič nestojí proto, že by nemohl. Neaktivuje se.** Není to tempo, je to **chybějící akce**.<br>**Kandidáti na příčinu, NEOVĚŘENO — první krok je rozhodnout mezi nimi, ne opravovat:** (a) `carrierStallAwareSteps` drží pohyb **schválně** v záloze (`maxSafe = MA/2`) a jeho test bezpečí `carrierIsBlitzable` **nezná GFI** (**P37**) ⇒ považuje se za bezpečného neprávem; (b) makro ADVANCE se v search vůbec nevybere; (c) `expandAdvance` skončí na `if (steps <= 0) return result;`.<br>⚠️ **Vzorek 800 her, ne 3 000** — první věc ráno je přepočítat na celém korpusu. Doklad `evidence/cage_ma_cap_20260819.md`, skripty `diag_cage_ma_cap_20260819.py` · `diag_mech_schedule_20260819.py`. | **OTEVŘENO — PŘEPOČÍTÁNO NA CELÉM KORPUSU 20.08.** ✅ **replikuje**: nosič nejednal vůbec v **83,9 %** kol s Δx = 0 (3 000 her, 5 213 / 6 216) proti 83,8 % ze vzorku 800; rozvrh nesplnitelný 27,6 % a nehnuli jsme se 37,7 % také replikují. ⛔ **Doklad z 19.08. měl rozbité jmenovatele** (957 + 684 = 1 641 proti uvedenému „z toho“ z 1 375) ⇒ nosič je volný v **63,3 %**, ne v 58,3 % — číslo se opravou **zhoršilo**. ⭐ **Příčinu ale nejspíš rozhodne P40, ne samostatné měření**: rameno P38 obchází právě tu záložní smyčku, která v základu stáhne `steps` na 0 (kandidát (c)). Doklad `evidence/carrier_idle_20260820.md`, skript `diag_carrier_idle_20260820.py`. |
| **P38** | ⭐⭐⭐ **POLE NOSIČE SE DOPOČÍTÁVÁ ZE ZAMÝŠLENÉ KLECE** *(uživatel 19.08.: „podle toho, kde bude stát nosič v našem kole, přece dopočítáme vše včetně toho, aby byly rohy čisté“)*. Pravidlo o **pořadí rozhodování**: dnes nosič popojde rovně kupředu a klec se dopočítává k místu, kam došel; podle pravidla se **cílové pole nosiče vybírá podle klece, která z něj vyjde**. Pole nosiče určuje **všechna čtyři** rohová pole naráz ⇒ táž chybějící dimenze **KAM** jako P9 a P35, na nejdražším místě.<br>⛔ **STROP: možné v 95,6 %, plníme ve 2,7 %.** 19 964 kol, ⌀ 7,49 volného stojícího těla: pole, ze kterého vyjde plná čistá klec bez dalších sousedů **a na jehož rohy dosáhnou čtyři naše těla**, existuje v **19 095 kolech (95,6 %)**; rozpočet těl brání ve **3,7 %**, soupeř v **0,7 %**. A **ve 25,7 % těch kol nosič na takovém poli UŽ STOJÍ** (dalších 22,6 % = jedno pole). ⇒ Rozdíl nedělá rozpočet ani soupeř, ale **volba pole**.<br>⚠️ Strop, ne plán: dosah Chebyshevem z `ma` bez TZ/dodge/GFI (horní mez), ležící těla nepočítána, a **neptá se na cenu na tempu** (K9a 20,7σ) ⇒ pole se musí vybírat **v rámci** postupu, ne proti němu. Doklad `diag_carrier_square_plan_20260819.py`, spec **15.0c**. | ✅ **IMPLEMENTOVÁNO 19.08.** — rameno `setCageAwareAdvanceArm(side,on)`, per side, default OFF, **mode 6** v nočním harnessu. `expandAdvance` prochází pole v TÉMŽE krokovém rozpočtu a bere to, ze kterého vyjde plná čistá klec (4 rohy na hřišti · žádný obsazený soupeřem · u žádného nestojí soupeř · nosič bez dalších sousedů · čtyři volná stojící těla na ně dosáhnou). ⭐ **Tempo se neprodává:** kandidáti jsou omezeni na pole do **jednoho pole** od nejlepšího dostupného postupu (K9a 20,7σ) ⇒ mění se KTERÉ pole, ne jak daleko. Pole v TZ se vylučuje jako v původní záložní smyčce, a když rameno pole najde, smyčka se přeskočí. Čítač `takeCageAwareAdvancePicksInSearch()` tiká jen při skutečném posunu cíle. Testy **552 → 555**. ⛔ **Sonda odhalila, že mode 6 nebyl v seznamu módů, pro které se tiskne per-pair leak test** — noc by se odmítla spustit; opraveno v mode listu, ne obcházením sondy. Předregistrace `evidence/night_prereg_20260819.md`. ⏰ ~~Čeká na noční A/B.~~ ✅ **NOC 19.→20.08. ROZHODLA: POMÁHÁ** — **+0,0827 ± 0,0065 (+12,80σ)**, 6 800 párů, 8/8 shardů kladných, leak 0, nulový test přesně 0,0. **Největší efekt roku.** ⛔⛔ **Ale rozklad po rasách vyvrací jméno nálezu:** rameno pomáhá TOMU, KDO HO DRŽÍ, a oběma rasám skoro stejně — trpaslík 0,4401→0,5378 TD/hru (+0,098), **wood-elf 0,5513→0,6466 (+0,095)**. Elf trpasličí klec nehraje ⇒ zisk nese něco **rasově neutrálního**. Attrition beze změny ⇒ efekt jde **pohybem míče, ne bitím**; trpaslík bez TD **59,6 % → 51,5 %**. ⇒ **NENASAZOVAT do produkce, dokud neproběhne rozklad** (**P40**). Doklad `cageadvance_20260819/chain.log`, commit `841943a5`. |
| **P40** | ⭐⭐⭐ **ROZKLAD P38 — KOLIK Z TOHO JE KLEC A KOLIK JEN BOČNÍ VOLNOST?** *(20.08.)* Rameno P38 má přibalené **tři** změny naráz (`macro_actions.cpp`): **(A) boční volnost** — základ počítá cíl aritmeticky (`x + dx*steps`, `y` o jedno pole ke středu) ⇒ všechny cíle leží na přímce, rameno prochází celý čtverec `[-budget, budget]²`; **(B) kritérium klece** `cageScoreForSquare`; **(C) obejití záložní smyčky** — v základu se cíl stahuje zpět, dokud není TZ-free, a když nedojde nikam, `if (steps <= 0) return result;` ⇒ **nosič se nehne vůbec = doslova P39**.<br>⛔ **STAŽENO:** rasovou neutralitu zisku (elf +0,095, trpaslík +0,098) jsem četl jako důkaz proti (B). **Uživatel 20.08.: „klec má podle mne pomáhat všem“** ⇒ klec je univerzální ochrana nosiče, neutralita je s (B) plně slučitelná a **nedokazuje nic v žádném směru**. Zbývající důvod k podezření je jen mechanický (rameno obchází smyčku z (C)).<br>⭐⭐⭐ **A cena je RASOVÁ** *(uživatel 20.08.)*: 7,03 volných těl z 11, z toho nosič 1 + rohy 4 + blitz 1 = **6 ze 7**, zbývá **jedno tělo**. ⚠️ Posun vlastní zdi do toho součtu NEPATŘÍ — je to **obrana** (uživatel ji vede jako **„L“**, v naší spec chybí).<br>⭐⭐⭐ **A ZEĎ LÁME VÝKLAD CELÉHO P38** *(uživatel 20.08.: „u útoku jde trpaslíkům naopak o pokus prolomit protivníkovu zeď“ · „elfové zase zkusí např trpasličí zeď oběhnout“)*: zeď je univerzální objekt, ale **odpověď na ni je rasová** — trpaslík **PROLOMÍ**, elf **OBĚHNE**. A **boční volnost z ramene = oběhnutí = elfí nástroj** ⇒ elfí zisk je jí plně vysvětlitelný, kdežto u trpaslíka buď zisk nese (B)/(C), **nebo engine vyhrává tím, že hraje za trpaslíka elfa** — což by bylo varování, ne úspěch. ⛔ **A `cageScoreForSquare` o odporu koridoru NEVÍ NIC** *(ověřeno 20.08. — v celé funkci ani jedna zmínka)*: vybírá pole slepé k tomu, jestli leží za soupeřovou zdí. ⭐ **Metr existuje**: `corridorResistance()` (`cage_advance.cpp:60`), export `corridor_resistance`, v σ-tabulce **−9,6σ** ⇒ rozhodovací měření **z korpusu bez noci**: pro každé vybrané pole porovnat odpor s polem základu a rozdělit na OBĚHNUTÍ / PROLOMENÍ, **zvlášť pro trpaslíka a pro elfa**. ⛔ **Tím padá i strop 95,6 %**: počítal těla, která na rohy DOSÁHNOU, ne těla VOLNÁ od jiné povinnosti — a `cageScoreForSquare` zná jen `movementRemaining`, o zdi a markování neví nic. ⇒ strop se musí počítat **zvlášť pro trpaslíka a pro elfa**. Váže na **T1.7** a **T1.9**.<br>⇒ **Rozhodne placebo rameno (mode 7)**: identické s P38 ve všem *(týž krokový rozpočet, týž `prog >= maxProgress − 1` pás, týž TZ filtr, totéž obejití smyčky, týž per-side čítač)*, lišící se **jedině vypuštěným `cageScoreForSquare`**. Placebo ≈ P38 ⇒ nález se jmenuje **„nosič neumí uhnout do strany"** a P32/P9/P35 se povyšují na jedno společné zadání; placebo ≪ P38 ⇒ kritérium klece si zisk zaslouží.<br>⭐ **(C) jde rozhodnout z KORPUSU bez noci** — v 5 213 kolech s Δx = 0 a bez události nosiče ověřit, jestli by rameno pole našlo. 14 hodin → pár minut.<br>⚠️ **Předpovědi psát DVOUSTRANNĚ** (viz T2.17). Zadání pro Fable `evidence/fable_brief_p38_decomposition_20260820.md`. | **OTEVŘENO — ⏰ PRIORITA, blokuje nasazení P38** |
| **T2.17** | ⚠️ **PŘEDREGISTRACE A SUMMARIZER MĚŘÍ JINOU VELIČINU.** `deltaHomeRace() = chessCandHome + chessCandAway − 1` (`diag_f1_cage_advance_harness.cpp:216`) je **dvoustranný** kontrast *(my s ramenem proti nám proti rameni)*, protože rameno je v páru vždy zapnuté jedné straně. Když rameno pomáhá oběma stranám, **oba členy se sčítají**. Předregistrace na noc 19.→20.08. byla psaná **jednostranně** (`delta in 0,005 0,040`) ⇒ summarizer vytiskl **❌ MIMO** na efektu, který jednostranně vyšel **+0,0413** proti horní mezi **0,040**. ⚠️ **Upřesnění z psaní testu:** je to tedy MIMO i jednostranně — ale **o 0,0013, ne o dvojnásobek**. Zavádějící nebyl štítek, ale **VELIKOST**: „2× nad stropem“ a „3 % nad stropem“ vedou k jinému rozhodnutí.<br>⛔ **Cena záměny je špatné jméno nálezu** — „2× nad stropem" svádí k tomu vyhlásit průlom tam, kde je efekt v očekávaném pásmu. Táž rodina jako **T2.12** (dva prahy) a **T2.16** (dvě slova pro dva verdikty): **číslo se tiskne bez toho, co znamená.**<br>⇒ Oprava: summarizer tiskne u delty **explicitně „dvoustranná"** a vedle ní **jednostranný odhad (delta/2)** s výhradou, že dělení dvěma platí, jen když je efekt na obou stranách podobný *(a to jde zkontrolovat z rows — TD po rasách)*; předregistrace dostane pole, kterou z těch dvou veličin předpovídá. | ✅ **UZAVŘENO 20.08.** — `night_summarize.py` tiskne u delty výslovně **„DVOUSTRANNÁ (chessCandHome + chessCandAway − 1) = my S RAMENEM proti nám PROTI RAMENI“**, vedle ní **jednostranný odhad** s vlastní SE a CI, a **podmínku symetrie hned u něj** *(dělení dvěma platí jen při podobném efektu na obou stranách — ověř z rows, TD po rasách)*. Předregistrace umí nově předpovídat **`delta_1s`** samostatně. ⭐ **Klíčový test: táž delta proti témuž pásmu vyjde opačně** podle toho, kterou veličinu předpovídáš. Přibrána i drobnost z T2.16: chybějící páry se tisknou **v kusech** *(„1 párů rameno NEMĚŘILA“)* místo schované ve „100,0 %“. Testy **42 → 48**. |
| **P42** | ⛔⛔⛔ **ZÁKAZ: NOSIČ NESMÍ KONČIT KOLO V KONTAKTU** *(uživatel 20.08.: „block na nosiče se nesmí stávat vůbec“)*. ⭐ **Není to přísnost navíc, je to správný tvar pravidla:** blok vyžaduje, aby útočník u nosiče **UŽ STÁL** na začátku svého kola ⇒ je to **náš** stav, vyrobený rozhodnutím o cílovém poli, a **soupeře nestojí nic**. Blitz naproti tomu stojí jeho **jedinou akci za kolo** a lze ho jen **zdražit** ⇒ **dvě různé tvrdosti, ne dvě velikosti téhož** *(uživatel: „uznáváme, že to ne vždy vyřešíme tak striktně“)*.<br>⛔ **Zároveň OPRAVENO nejcitovanější číslo kapitoly:** „nosič končí v kontaktu ve **39,3 %**“ počítalo i **LEŽÍCÍ** soupeře, kteří nemají TZ a nemohou blokovat. Přeměřeno na 24 754 kolech: **stojící soupeř 12,3 %** · jakýkoli 38,9 % · naše tělo 80,7 %. ⇒ **Zákaz je 3× levnější, než spec zněla.**<br>**Strop:** expozice 12,3 % → soupeř udeří 51,4 % → ztráta 47,4 % ⇒ **3,0 % našich kol ≈ 0,74 ztráty míče na zápas**. ⚠️ „Ztráta“ je horní odhad *(týž nosič už nedrží — spadne sem i sebrání spoluhráčem)*: 743 tady proti 564 v rozpadu konců držení.<br>⇒ **Zapsat jako zákaz do ČÁST 3 a jako kontrolu**; splnitelný **volbou pole**, tedy dimenzí KAM *(P38 · P9 · P32 · P35)*. Doklad `evidence/carrier_contact_20260820.md`. | **OTEVŘENO — ⏰ levné a adresné** |
| **T1.13** | ⭐⭐⭐ **CÍL KLECE MÁ DVĚ POLOŽKY A NEPLNÍME ANI JEDNU** *(uživatel 20.08.: „cílem klece — co řešíme jako první okruh — je: dojít co nejdál k soupeřově TD, a chránit nosiče — neplníme — a řešíme už celkem dlouho totok“)*.<br>**C1 dojít co nejdál:** ⌀ Δx ~2 pole/kolo, max **7 kol na TD, nikdy 8**, ale **81,8 % držení neskončí ničím**.<br>**C2 ochránit nosiče:** **48,9 % držení končí ztrátou** — z toho **70,1 % blitz**, **16,5 % blok**.<br>⇒ Spec **15.0b′**. ⚠️ **Je to rámec, pod který patří P38–P42 a T0.1** — ne další položka vedle nich. **Kontrola má měřit obojí zvlášť**, ne jedno souhrnné číslo. | **OTEVŘENO — rámec, ne úkol** |
| **P43** | ⭐ **ZÁTĚŽOVÝ TEST DOKTRÍNY: Gutter Runner + Jump Up** *(nápad uživatele 20.08.)*. Otázka: **přežije naše doktrína klece soupeře, který je tvrdší přesně v té dimenzi, o kterou se opírá?** Jump Up dělá postavení **zdarma**, takže sražený Gutter Runner si drží **plný dosah MA 9** místo MA−3 ⇒ ležící soused nosiče přestává být „výstraha" a stává se **zákazem** *(K38 to už tak má napsané — plyne to z pravidel, ne z měření)*.<br>⛔ **NEDĚLAT do baseline rosteru:** všechna dnešní čísla *(P38 +0,0827, noc P40, celá σ-tabulka)* jsou měřená proti současným rosterům — posílit soupeře teď znamená, že **dnešní noc přestane být srovnatelná se včerejší**.<br>⛔ **A chybí předpoklad: log NEEXPORTUJE dovednosti** *(hráč má jen `id/x/y/state/has_ball/name/ma/st/ag/av`)* ⇒ K38 by Jump Up **neviděla** a dál by ten případ počítala jako výstrahu — kontrola by proběhla a **odpověď by v ní nebyla**.<br>**Předpoklady, v pořadí:** (1) **dovednosti do exportu logu**, (2) **vlastní korpus**, (3) **baseline zůstane nedotčený**. | **ODLOŽENO — ⏰ spouštěč: FÁZE ZMĚN V ROSTERECH** *(uživatel 20.08.: „nejdříve až do fáze změn v rosterech — OK")*; váže na **balík E (roster)** |
| **P44** | ⛔⛔ **FOLLOW-UP NENÍ VOLBA — a tím se ruší dvojrole blitzujícího** *(uživatel 20.08.: „blitzující může být roh pouze když skončí volný — i když neshodí cíl, může jej odsunout a sám zůstat a být volný")*.<br>⭐ **Pravidlo uživatele:** blitzující se stává **rohem klece**, když **skončí volný** *(žádný stojící soupeř vedle)*. Sražení cíle **není podmínka** — stačí **odsunout a zůstat stát**.<br>⛔ **U nás se to zahrát NEDÁ.** `resolveBlock` má parametr `noFollowUp`, ale **oba skuteční volající** (`action_resolver.cpp:60` a `:131`) berou default `false`; `true` se předává jen vnitřně u chain pushe a Frenzy (`:843`, `:882`). Follow-up tedy proběhne **vždy**: `att.position = defOldPos` (`block_handler.cpp:695` a `:747`).<br>⛔ **A není to jen chybějící volba — je to volba se špatným defaultem:** follow-up stěhuje blitzujícího **kupředu, hlouběji mezi soupeře**, tedy přesně tam, kde **nezůstane volný**. Engine si sám ruší dvojroli, kterou doktrína potřebuje.<br>⇒ **Zadání:** follow-up udělat **volitelný** a rozhodovat ho podle toho, jestli blitzující zůstane **volný** *(a tedy použitelný jako roh)*. ⚠️ Frenzy má follow-up **povinný** — to musí zůstat.<br>⭐ **Proč to má cenu:** blitzující jako roh mění rozpočet z **6 ze 7** na **5 ze 7**, tedy **zdvojnásobuje volnou kapacitu**; a stojí už vpředu, takže to má k novému rohu blíž než těla zezadu ⇒ **blitz kupuje dosah pro KLEC, ne jen ránu**. | **OTEVŘENO — ⏰ zadáno 20.08., blokuje doktrínu dvojrole** |
| **P45** | ⛔⛔⛔ **LEŽÍCÍ SE U NÁS POSTAVÍ A HNED BLOKUJE — klec je proto systematicky slabší než v pravidlech.**<br>⭐ **Pravidlo (uživatel 20.08.):** *„pokud stojíme rohem vedle ležícího, je to stejné jako stát rohem ne hned vedle, ale v dosahu někoho dalšího — na odstranění rohu spotřebuje jeden nebo druhý jeden BLITZ."* ⇒ **Ležící soused a vzdálený soupeř v dosahu jsou EKVIVALENTNÍ: obojí stojí jeden blitz.** A blitz je **jeden za kolo** ⇒ ⭐⭐⭐ **skutečná obrana klece není „roh je v bezpečí", ale „soupeř umí za kolo sundat NEJVÝŠ JEDEN roh".**<br>⛔ **U nás to neplatí.** Ležící dostane akci „postav se" jako **MOVE na vlastní pole** (`rules_engine.cpp:208–218`), a `resolveStandUp` **nenastaví `hasActed`** — `action_resolver.cpp:45–48` vrátí `ok()` a skončí. V dalším průchodu je hráč **STANDING s `hasActed == false`** ⇒ nabídne se mu **BLOCK**. ⇒ **Postaví se za 3 MA a blokuje za cenu obyčejné akce, ne blitzu.**<br>⇒ **Soupeř tak u nás umí za kolo sundat VÍC rohů**, protože ležící sousedé ho nestojí jeho jediný blitz. ⭐ **Kandidát na vysvětlení, proč se klec nevyplácí a proč brána klece 18.08. ŠKODILA (−3,7σ).**<br>⭐⭐⭐ **A JUMP UP JE PŘEPÍNAČ KATEGORIE** *(uživatel 20.08.)*: **stojící** = blok zdarma ⇒ ZÁKAZ · **ležící S Jump Up** = postavení zdarma ⇒ blok zdarma ⇒ **taky ZÁKAZ** · **ležící BEZ Jump Up** = jeden blitz ⇒ **výstraha**. ⇒ Ostřejší tvar P45: **náš engine dává prakticky Jump Up KAŽDÉMU**, takže **prostřední kategorie, na které doktrína stojí, u nás vůbec neexistuje**.<br>⚠️ **Zpětná oprava dnešního čtení:** ranní závěr *„zákaz je 3× levnější (12,3 % místo 38,9 %)"* platí **v pravidlech, ne u nás** — v našem enginu je pravé číslo **38,9 %**. ⇒ **Zákaz je u nás 3× dražší, než jsem ráno napsal.**<br>⛔⛔ **A OVĚŘUJE SE PROTI BB2016, NE PROTI CRP** *(uživatel 20.08.)* — zápis ze 07.08. obě edice **ztotožnil** *(„JEDNA EDICE: BB2016 (CRP/LRB6)“)*, takže doklad u všech položek „ověřeno proti staženému CRP textu“ je **špatný ZDROJ**, ne nutně špatný výsledek.<br>✅ **ROZHODNUTO TEXTEM 20.08. — a obě edice se SHODUJÍ.** CRP/LRB6 ř. 46 natvrdo: *„a player who stands up **may not take a Block Action**, because you may not move when you take a Block Action."* BB2016 tu větu v *Standing Up* nemá, ale **Jump Up má v obou edicích identické znění**: *„The player may **also** declare a Block Action while Prone which requires an **Agility roll with a +2 modifier**… A failed roll means the Block Action is wasted."* — to **„also"** dokazuje, že Jump Up to právo teprve **uděluje**. ⇒ ✅ **ekvivalence platí, engine je VADNÝ.** ⚠️ Na tomhle pravidle se edice **neliší** — citace CRP tu vyšla správně, ale **náhodou, ne metodou**.<br>⛔ **A P45 má TŘI části:** (1) ležící nesmí vzít Block akci — ⭐ **dáváme mu VÍC NEŽ JUMP UP**, ta chce hod AG +2 a může selhat, u nás blokuje **bez hodu a vždy**; (2) ⛔⛔ **chybí hod 4+ při MA < 3** *(`move_handler.cpp:319` vrátí `fail()`)* ⇒ **hráč s méně než 3 MA se u nás NEPOSTAVÍ NIKDY**. ⭐ **Testovací případ od uživatele 20.08.: TREEMAN** *(„to je průšvih, jestli zůstane ležet")* — a **není to hypotetické**: wood-elf roster TV1200 má **Treeman +Guard s MA 2** *(`roster.cpp:590`)*, tedy **vždy pod prahem**, a je to **přesně sestava našeho nočního matchupu `dw-we`**. ⇒ **Jedním sražením trvale odstraníme soupeři ST 6 / Guard / Mighty Blow / Stand Firm z celého zápasu.** ✅ **ZMĚŘENO 20.08. na 750 hrách** *(`diag_treeman_standup_20260820.py`)*: **911 sražení Treemana = 1,21 na zápas, a VSTAL v 0 z nich — 100 % zůstane ležet do konce drivu.** ⚠️ *(První měření hlásilo „vstal v 82,8 %" — počítalo ale **resety při výkopu**, ne postavení ve hře. Opraveno.)* ⇒ **Důsledek: wood-elf byl ve VŠECH dosavadních nocích systematicky oslabený** o svou nejcennější figuru; (3) po postavení na 4+ nesmí dál chodit *(jen GFI)* — neřešeno.<br>~~Otázka, na které P45 stojí:~~ dovolí BB2016 postavení za 3 MA na začátku **libovolné** akce (i BLOCK), nebo jen v rámci **pohybové** (Move/Blitz)? **Jen Move/Blitz** ⇒ ekvivalence platí a **engine je vadný**. **Libovolná akce** ⇒ **engine je správně a vadná je DOKTRÍNA**. ⚠️ *„Dohoda dvou nejistých není ověření“* (07.08.) — čeká to na text. | **OTEVŘENO — ⏰⏰ zadáno 20.08., vysoký doktrinální dopad** |
| **P41** | ⛔⛔⛔ **FÁZE VÝBĚH JE 2,9 % — A NIKDY NEBYLA MĚŘENA** *(20.08., vypadlo z T0.1)*. Ve fázi, kdy je endzona **na dosah** (`dist ≤ MA + 2`), splníme rozvrhovou podlahu ve **2,9 %** kol *(n = 414 z 3 000 her)* a průměrně nám chybí **3,38 pole** — **o řád nejhorší ze tří fází**. Replikuje ze vzorku (100 her: 5,3 %).<br>⛔ **A boří to předpoklad, na kterém T0.1 stálo:** *„rovnoměrná podlaha trestá klec“* — **SÓLO 34,6 % a KLEC 33,8 % jsou prakticky totéž**. Klec proti sólu nezaostává; **zaostává VÝBĚH**.<br>⚠️ **Poctivá výhrada:** v POSLEDNÍM kole půle je podlaha `min(dist, MA)`, tedy „sprintuj naplno“, takže nízký podíl je **zčásti dán konstrukcí** — ale **schodek 3,38 pole tím vysvětlený není**. ⇒ **První krok je rozpad podle `turns_left`, ne oprava.**<br>⭐ **Napojení:** přímo na to, že **v 68,4 % zápasů nedáme ani jeden TD** (`diag_where_lost_20260820.py`) a na **P39** *(nosič se neaktivuje)*. Fáze VÝBĚH je poslední metr před endzonou a **jediná fáze, kterou nikdo nikdy samostatně neměřil**. | **OTEVŘENO — ⏰ silný kandidát na další zadání** |
| **T1.10** | ⭐ **LAJNA — samostatný rozebraný příklad k diskuzi** *(uživatel 19.08.: „mimo hřiště je zajímavé, ale měli bysme se tomu vyhýbat — pokud nejde, tak nejde; toto si zaslouží samostatný příklad k diskuzi“)*. Roh mimo hřiště je 2,1 % chybějících rohů; na postranní čáře jsou dva rohy geometricky nemožné ⇒ **4 rohy 0,0 %** (193 kol), jedno pole od lajny **1,8 %**, uprostřed **13,6 %** *(⚠️ gradient je korelace, ne důkaz)*. ⭐ **Není to vada k opravě, je to stav, kterému se má vyhnout výběr pole.** Doktrína existuje (**15.5**), chybí **odehraná situace**: kdy je tlak k lajně přijatelná cena za postup a kdy ne. | **OTEVŘENO — rozhovor, nesoupeří o stroj** |
| **P35** | ⭐⭐⭐ **BLITZ SE OCEŇUJE Z VÝCHOZÍHO POLE, ale hází se z cílového.** `getBlockDiceCount` (`macro_actions.cpp:182`) počítá **obranné asistence** kolem pole, kde blitzující STOJÍ; resolver (`action_resolver.cpp:86–118` → `block_handler.cpp:491`) je počítá až tam, kam DOJDE. Kdo blitzuje do hloučku, má na startu nula a u cíle několik.<br>⭐ **Kód tu závislost ZNÁ:** komentář `action_resolver.cpp:89–91` — *„fewer enemies next to the blitzer = fewer defender assists on the block, see getBlockDiceCount"* — proto je `pickApproachStep` TZ-aware. **Trasa ji respektuje, výběr blitzujícího ne.**<br>**Strop (27 928 blitzů, 3 000 her):** kostky se změní v **16,2 %**, a v **9,7 % (2 712) se překlopí z „vybíráme my" na „vybírá soupeř"** — nejčastěji **+1 → −2**. ⇒ **≈ 0,9 blitzu na zápas s obráceným znaménkem kostek.** ⚠️ DOLNÍ odhad: bere se nejpříznivější pole u cíle, korpus neveze Guard. ⭐ **Upřesnění z psaní testu:** `pickApproachStep` je TZ-scored, takže **když u cíle existuje čisté pole, executor po něm dojde sám** ⇒ vada kouše jen tam, kde je **každé** volné pole u cíle pokryté. Měření to nezeslabuje — strop se počítal z nejpříznivějšího pole, tedy **už za dokonalé trasy**, a 9,7 % je **zbytek PO ní**. Doklad `evidence/filter_vs_resolver_round2_20260819.md`. | ✅ **IMPLEMENTOVÁNO 19.08.** — rameno `setBlitzLandingArm(side,on)`, per side, default OFF; cílové pole se počítá **touž chůzí jako executor** (`estimateApproachFailChance` ho vrací přes `landingOut`); ošetřena **obě** místa výběru (`expandBlitz` i `expandBlitzAndScore`); čítač `takeBlitzLandingRepicksInSearch()` tiká jen při skutečné změně volby ⇒ nulový test. Testy **549 → 552**. ✅ **MODE DOPLNĚN 25.08. — `mode 8` v `diag_f1_cage_advance_harness.cpp`** *(seed base 149M, disjunktní; vlastní `diag_blitzlanding_rows.jsonl`; nové pole `cand_landing` v řádcích; mode 8 dopsán do `armSignalAvailable`, takže projde preflight sondou a leak test se spustí)*. **Light test 8 párů:** 2 040 repicků = **127,50/hru**, `arm acted 8/8`, **leak 0**, 46,8 s/pár na jednom procesu. Předregistrace `evidence/night_prereg_20260825.preds`, launcher `run_night_20260825.sh` *(4 800 párů, CORPUS=1)*.<br>⚠️⚠️ **A ROVNOU POZNÁMKA O PRIORITĚ, ať se to nečte jako pořadí podle dopadu:** P35 dostala noc **protože je HOTOVÁ a měřitelná bez řádky kódu**, ne protože je první v okruhu POHYB. **Podle dopadu je první M1/N10** a podle uživatelova zadání 24.08. je jmenovanou prioritou **Leap do pátku 28.08.** ⇒ **noc = příležitost, den = priorita.** Kdyby se to jednou četlo jako „P35 byla důležitější než M1", je to špatné čtení.<br>⭐ **Důvod, proč se to přesto vyplatí změřit hned:** je to **hotová práce čekající na rozhodnutí** za default-OFF ramenem, a tenhle projekt má doloženou třídu vad „hotové a nikdo o tom neví" — **T5.14 ležela 11 dní, P31 33 dní**. Dokud se P35 nezměří, **nesmí se zapnout**, a každý den je to další den, kdy rameno stárne proti hýbající se bázi. |
| **P36** | ⭐⭐ **DAUNTLESS CHYBÍ V ŽEBŘÍČKU BLITZUJÍCÍCH — P13 spravila NABÍDKU, ne VÝBĚR.** `estimateBlitzFailChance` volá `getBlockDiceCount(…, dauntlessInOffer=false)`; komentář to hájí jako *„the block as thrown"*, jenže blok jak se hází se s Dauntlessem proti ST4 **vyrovná v 83 %** — což je právě číslo, kterým se P13 zdůvodnila. ⇒ Slayer s Dauntlessem se řadí jako podřadný kandidát přesně proti ST4, kde je nejcennější. ⚠️ Strop potřebuje postavenou pozici (Q1 jako u P13), z korpusu nejde. Levné, táž oprava o patro níž. | **OTEVŘENO — zapsáno 19.08.** |
| **P37** | ⚠️ **`carrierIsBlitzable` NEZNÁ GFI** (`macro_actions.cpp:1162`): `distance <= opp.stats.movement`, ale CRP dovolí **2 pole navíc**. Test rozhoduje, jestli si nosič nechá pohyb v záloze. **19 964 kol: v 6,3 % (1 266 = 0,42 kola/zápas) kód říká BEZPEČNO a soupeř dosáhne přes GFI.** Ležící soupeř je vyloučen úplně, ač postavení stojí 3 MP ⇒ dosáhne na MA−3. ⚠️ ~~Menší nález~~ **(povýšeno 20.08.)**: zdržet pohyb je často správně i tak, takže kolik by sprint zlepšil, se odsud říct nedá — vada je, že se test tváří jako fakt o dosahu. | ⏰⏰ **POVÝŠENO 20.08.** *(uživatel: „povyš P37“)* — ⭐ **důvod: blitz na nosiče je HLAVNÍ příčina ztráty míče.** Z 3 417 ztrát je **2 396 = 70,1 %** sražený nosič **blitzem** *(blokem dalších 16,5 %)*, kdežto vlastní chyba nosiče je dohromady **7,1 %** ⇒ **míč neztrácíme hloupostí, sundají nám ho**. A `carrierIsBlitzable` je právě ta funkce, která rozhoduje, **jestli si nosič nechá pohyb v záloze** — takže její vada se platí přesně tam, kde je nejdražší. ⚠️ Výhrada z 19.08. **platí dál**: zdržet pohyb je často správně i tak, takže **kolik by sprint zlepšil, se odsud říct nedá** — vada je, že se test tváří jako fakt o dosahu. Doklad `evidence/possession_end_20260820.md`. |
| **P33** | ⭐⭐ **BLITZ NA NOSIČE — 4,5× ČASTĚJŠÍ SITUACE NEŽ BLOK NA NĚJ, A BEREME SI JI TŘETINOVĚ** *(uživatel 18.08.: „přijde mi zajímavější situace blitz na ballcarriera“)*. Vypadlo jako vedlejší řádek z měření P10a (3 000 her): u nosiče **stojí** naše tělo v **3 733** kolech a udeříme v **81,5 %**; u nosiče **nikdo nestojí** v **16 832** kolech a doblitzujeme **6 143× = 36,5 %**.<br>⛔ **Ten rozdíl se NESMÍ číst jako vada, dokud nemá jmenovatel:** blitz je **jeden za kolo** a v části těch 16 832 kol na nosiče nedosáhneme, nebo je blitz potřeba jinde (**P0.6 roh vs zeď**: blitz na roh dá Δx +1,80 proti +2,52 do zdi). ⇒ **Otázka zní: z kol, kde je nosič V DOSAHU blitzu, kolikrát jsme blitz utratili jinam — a za co?** Rozpad utracených blitzů podle cíle (roh · zeď · nosič · jinam) proti Δx.<br>⭐ **Měřitelné bez běhu**, na `corpus_baseline_20260817_data`. Je to podstatně větší položka než P10a (16 832 kol proti 3 733) a napojuje se na **P0.6** i na **T1.8** (rozpočet jedné akce za kolo). | **ZMĚŘENO 18.08. — a povýšeno na T1.9.** Se správným jmenovatelem: nosič je v dosahu blitzu ve **12 370 kolech = 4,12/zápas**; blitz na něj **5 997 = 48,5 %** *(ne 36,5 %, to bylo bez jmenovatele)*, jinam 3 264 = 26,4 % *(z toho **50,2 % cílů stálo hned vedle nosiče** — eskorta, nejspíš volba, ne chyba)*, **neutracen vůbec 3 109 = 25,1 % = 1,04 kola/zápas**. Na nosiče dosáhnou **4+ naše těla v 60,4 %** kol ⇒ není to otázka dosažitelnosti. ⛔ **Narazilo to na K32**: v logu **žádná událost `BLITZ` neexistuje**, jen `BLOCK` — první běh vrátil 0,0 % a vypadalo to jako nález. ⭐ **Rekonstrukce, která K32 obchází a nepotřebuje nový log: blok, u kterého útočník s cílem na začátku kola NESOUSEDIL, je blitz** (blok vyžaduje sousedství, blitz je 1/kolo). Použitelné i pro ostatní kontroly visící na K32. ⚠️ Dosah je horní odhad (bez dodge z TZ a bez obsazených polí). Doklad `evidence/blitz_on_carrier_20260818.txt`. |
| T0.1 | **K9 SE PŘEPÍŠE CELÉ — ROZHODNUTO uživatelem 18.08.** *(„jsem pro přepsat celé — minimálně na fáze — a ty kroky asi měřit zvlášť“)*. Ověřeno 18.08.: oprava **neproběhla**, `need = ceil(vzdálenost/zbývající kola)` (`diag_rules_checks_20260812.py:401`) je čistá geometrie bez členu odporu ⇒ rovnoměrná podlaha trestá klec za to, že jede pomalu, ačkoli **nesmí jet rychleji než 2 pole/kolo**. Nový tvar: **rozvrh po FÁZÍCH** (sólo Runner → klec → sólo výběh) a **každá fáze se měří zvlášť**, ne jedním číslem. ⛔ **A ŽÁDNÁ KONSTANTA** *(uživatel 18.08.: „je zbytečné uvedení konstanty někam, kde by stačilo — nesmí se rozpadnout, ale má jet co nejrychleji“)*: podlaha fáze „klec“ se nepíše jako **2 pole/kolo** — to je jen to, co dnešní trpasličí roster vydá (7,03 volných těl z 11, 5,5 kandidáta na 4 rohy). Pravidlo zní **„klec jede tak rychle, jak rychle se dokáže znovu složit“**, strop je funkce volných těl a **cíl je maximum, ne ten strop**. Týž zásah jako T1.8; viz [[feedback_implement_the_rule_not_the_outcome]]. ⚠️ Váže na **P3** (fázový plán trasy) — bez fáze v modelu nejde odlišit chybu od záměru. ⚠️ A pozor: K9a je s **20,7σ** nejsilnější prediktor dnešní σ-tabulky, takže se přepisuje kontrola, podle které je seřazená fronta; blízkost k Δx (13,9σ) navíc připouští, že měří spíš „drive šel dobře“ než „dobře jsme rozvrhli“. | ✅ **HOTOVO 20.08.** — `phase_floor()` v `diag_rules_checks_20260812.py`, **jediná definice** pro kontrolu (`K9c_solo`/`K9c_cage`/`K9c_run`/`K9x`) i σ-tabulku. Fáze ze ZAČÁTKU kola, strop z těl, podlaha `min(rovnoměrný podíl, strop fáze)`, **žádná konstanta** *(+2 = dva GFI, ≥2 rohy = mez z K29)*.<br>⛔ **Ale jako KONTROLA přepis NEUSPĚL:** fér srovnání stejného tvaru je **K9c 16,3σ proti K9a 20,8σ**. ⭐⭐⭐ Důvod stojí vedle: *„K9a žádala nemožné“* má samo **−21,6σ** ⇒ **rovnoměrná podlaha je lepší prediktor PRÁVĚ PROTO, že je nespravedlivá** (slepuje „nechtěli jsme“ a „nemohli jsme“, a to druhé je taky špatná zpráva); odpustit to znamená **zahodit signál**. ⇒ **Pravidlo a metr nemusí mít týž tvar** — K9a je dobrý METR a špatný PŘÍKAZ, K9c naopak ⇒ **necháváme obojí**.<br>⭐ Uživatelova námitka doložena: **ve 29,2 % kol stará podlaha žádala víc, než šlo fyzicky udělat** (⌀ +1,12 pole).<br>⛔⛔ **A vypadl z toho nález větší než samo zadání — viz P41.** Doklad `evidence/k9c_phased_floor_20260820.md`, commit `7d30f85c`. |
| T0.2 | Vyškrtat uzavřené: O3, O4, O5, X6 | **UZAVŘENO — nulová** (ověřeno 18.08.): v trvalé knize ty položky **vůbec nejsou**, žijí jen v archivu `task_queue_20260812.md`; O3 se navíc věcně vyřešilo uvnitř T1.8. Není co vyškrtávat. |

# 3. TEMPO A POSTUP

| ID | co | stav |
|---|---|---|
| T3.1 | Brána klece — veto jen při `achievable == 0` + cage-fill | **ZAMÍTNUTO — PŘEMĚŘENO 18.08. A POTVRZENO** (`gate_crn_20260817/`, 6 000 párů s CRN, pre-reg `evidence/night_prereg_20260817.md`): leak `MOVED WITHOUT THE ARM ACTING` **0 v 8/8 shardech** ⇒ delta se smí číst · `arm acted` **6000/6000** · `n_nonzero` **62,8 %** *(pre-reg čekal >80 % — předpověď MIMO, práh se podle pravidla NEPOSOUVÁ)* · **delta −0,0248 ± 0,0068 SE (−3,7σ), 8/8 shardů záporných**, 95 % CI [−0,038; −0,012] ⇒ pod pre-reg prahem **−0,015 = BRÁNA ŠKODÍ**, tedy ne NEROZHODNUTO. ⛔ **Není to replikace 13.08.** — mezi tím 3 commity enginu (P13, hand-off, darované TD) ⇒ platí „v dnešním enginu škodí“, ne „tehdy jsme měli pravdu“. ⚠️ Výhrada z 13.08. platí dál: **chybí jí plán trasy, ne schopnost.** *(původní zápis 13.08.:)* 1500 párů, dw-we **−0,0297 (−2,0σ)**, dw-sk +0,008. ⚠️ Nezahazovat kód: zlepšila skoro všechny kontroly, vyměnila tempo (20,6→28,4 %) za bití (76,1→73,2 %) a čistotu rohů (79,4→72,6 %) ⇒ nula. **Chybí jí plán trasy, ne schopnost.** |
| T3.2 | [1] Kontrola `c085331` (exposure = uživatelovo R1) — přeměřit K7 na korpusu PO opravě klece | **BLOKOVÁNO** — viz A2 |
| A2 | ⭐ **Exposure/R1 vyzvednout z `cage_advance.cpp` do obecného pohybu** | **OTEVŘENO** *(nové 14.08.)* — ověřeno: `c085331` sahá jen do `cage_advance.cpp`, ten se instancuje výhradně při `config.cageAdvance` (`macro_mcts.cpp:907`) ⇒ **hotová práce nejde zapnout nezávisle na zamítnuté bráně.** Je to tvoje vlastní pravidlo *„BLITZ pohyb → obecný pohyb"*. |
| T3.3 | [2] Tempo cílit na 3,14, ne 2,61 | **PŘEFORMULOVÁNO** → P3 (rovnoměrná podlaha je špatný model) |
| T3.4 | [3] Změřit `41c3570` — kdo nese, kdy je první držení | **UZAVŘENO** — korpus 3000: Runner **88–91 %** ve všech kategoriích, Longbeard 1–4 % |
| T3.5 | [5] Na lajně stojí 4 hráči místo 3 — dává soupeři blok navíc; je to v poli formace, ne v přiřazení | **OTEVŘENO** |
| **P11** | ⭐⭐⭐ **ENGINE SKÓRUJE, JAKMILE MŮŽE — a trpaslík nemá.** *(uživatel 14.08.: „kdyby Runner s míčem utekl a skóroval dříve, je to také špatně, protože soupeř dostane čas na re-TD"; a upřesnění: „ujet víc polí není špatně — ale je špatně skórovat dříve".)*<br>**Cíl není dojít co nejdřív, cíl je překročit čáru tak pozdě, aby soupeř neměl čas odpovědět.** Doktrína záporné rezervy tohle říká od 10.08., ale **v enginu není nikde**.<br>**Tři vady:**<br>**(1)** `greedyMacroRank(SCORE) = 100` (`macro_mcts.cpp:39`) — nejvyšší prior ze všech maker, bezpodmínečně.<br>**(2)** `scoringBonus += 0.4` za „safe walk-in" (`:702`) a `+0.8` v posledním kole (`:724`) — odměna za *„umím teď dojít do endzony"* nemá podmínku na zbývající kola soupeře. **Chybí zdržovací člen** *„umíš, ale ještě ne"*.<br>**(3)** `pacing` (`:712`) trestá `dist < idealDist`, tedy **předstih ve vzdálenosti** — a to je špatně: víc ujetých polí je vždy zisk, dá se stát pole před čárou a čekat. Trestat se má **předčasné překročení čáry**, ne pozice.<br>⛔ **Opravuje mou vlastní úvahu z téhož dne:** navrhl jsem „K9a přepsat na koridor" (trestat i předstih) a uživatel to zamítl. **K9a zůstává jednostranné**; místo něj vzniká samostatná kontrola **„v kolikátém kole jsme skórovali a kolik kol zbylo soupeři"**.<br>⇒ Týká se **každého drivu, který dojde k endzoně** — ne okrajové situace. | **OTEVŘENO — VYSOKÁ PRIORITA**, kandidát na víkend |
| **P12** | **Fáze 3 (sólo výběh) má vlastní podmínku puštění — PRONÁSLEDOVÁNÍ, ne vzdálenost** *(uživatel 14.08.: „taky záleží na tom, kolik soupeřů pak Runnera doběhne v poslední fázi")*. Runner MA6 proti Gutter Runner MA9 / wood-elf Catcher MA8 / ork MA5–6.<br>⇒ proti **wood-elfovi a skavenovi** musí být výběh **krátký** (klec donese míč skoro až tam), proti **orkovi a humanovi** může být delší.<br>⭐ **Dává mlácení účel v TRASE, ne jen v attrition:** *každý sražený pronásledovatel prodlužuje fázi 3.* Dosud se bití obhajovalo jen statisticky (+2,7σ na TD) a nikdo neuměl říct, k čemu je v plánu drivu. ⇒ Priorita blitzu má možná mířit na **ty, kdo umí doběhnout Runnera**, ne na rohy klece. Měřitelné na stávajícím korpusu. | **OTEVŘENO** — třetí parametr P3 |
| P3 | ⭐ **Fázový plán trasy** — sólo Runner + kick-off return → klec → sólo výběh u endzone. Rozvrh pozpátku od TD **po fázích**. Vstup pro `classifyTurnGoal` i `K9a`. **Bez fáze v modelu nejde odlišit chybu od záměru.** | **OTEVŘENO** |

# 4. ROZHODOVÁNÍ ENGINU *(živé chyby)*

| ID | co | stav |
|---|---|---|
| **P9c** | ⭐⭐⭐ **ÚČEL BLOKU NA POLLUTERA JE ODKLIDIT HO OD ROHU** *(uživatel 14.08.: „priorita u špinavého rohu je odklidit protihráče pryč od rohu — ne jej nechat u rohu a posunout blíž k balonu")*. **Není to kompromis, je to pořadí:** odsun, po kterém polluter roh **pořád špiní**, není částečný úspěch — je to **selhání akce**, protože roh byl jediný důvod ji dělat. A když ho takový odsun navíc přiblíží k nosiči, je to **záporný obchod**.<br>⇒ Řazení cílových polí při bloku na pollutera: **(1) přestane sousedit s rohem** *(to je účel)* → **(2) nepřiblíží se k našemu nosiči** → (3) zbytek.<br>⚠️ **Váže to i výběr blokujícího, ne jen směru:** tři nabízená pole jsou dána vektorem `polluter − blokující`, takže **kdo udeří, určuje, kam se dá odsunout**. Blokující se má vybírat tak, aby pole splňující (1) vůbec existovalo.<br>⭐ **Nepotřebuje nové logování** — je to otázka na začátek kola (pozice pollutera, rohu, nosiče a kandidátů na blok), ne na průběh. Jde spočítat na **stávajícím korpusu**. <br><br>✅ **IMPLEMENTOVÁNO 18.08. A JDE NA NOC.** `choosePushSquare` teď cílové pole **skóruje**, v uživatelově pořadí: (1) přestane sousedit s rohem naší klece → (2) nepřiblíží se k našemu nosiči (REACH0) → (3) rovně dozadu jako tiebreak. **Bez našeho nosiče na hřišti skóruje přesně jako dřív ⇒ na obraně je rameno konstrukčně no-op.** Per SIDE (`setPushGeometryArm`), default OFF, čítač tiká **jen když se pole opravdu přesměrovalo**. Testy 545 → 549. **Strop spočítán PŘED během:** 21,93 odsunu na zápas, **17,34 se skutečnou volbou**, **1,04 na zápas přisunuto BLÍŽ k našemu nosiči** ač šlo dál (0,27 přímo k němu) a **0,24 ponecháno u rohu** ač šlo jinam ⇒ **~1,28 prokazatelně horší volby na zápas** (P10a 0,23 · P8 0,056, obojí zamítnuto). Předregistrace `evidence/night_prereg_20260818.md`.<br><br>⚖️ **ZMĚŘENO 18.→19.08. — EFEKT SE NEKONAL.** `pushgeom_20260818/`, 6 800 párů, leak **0**, `n_nonzero` **57,5 %**, bez overdisperze: **+0,0017 ± 0,0060 SE (+0,28σ)**, 95 % CI **[−0,0100; +0,0134]**. Předpověď 0,005–0,020 **MIMO**. CI je celé uvnitř prahu ⇒ efekt **≥ 1,5 pp vyloučen v obou směrech**; ⚠️ malý kladný efekt **do ~1,3 pp vyloučit neumíme**, takže „neškodí" ANO, „je k ničemu" NE. ⇒ **Kód se nezahazuje** (je to doložená vada volby, 1,28 horší volby na zápas), ale **sám o sobě chess neposouvá** — druhou půlku drží **P32**. |
| **P9** | ⭐⭐⭐ **SMĚR ODSUNU SE VYBÍRÁ SLEPĚ — a je to společný kořen dvou dnešních nálezů.** CRP FAQ: *„The coach of the moving team decides all pushback directions unless the pushed player has Side Step."* Máme tedy volbu ze **tří polí** (`getPushbackSquares`) a `choosePushSquare` (`block_handler.cpp:113`) ji zahodí: `score = count - i` = **„rovně dozadu první"**, čistě geometricky. Cílové pole se **nikdy nehodnotí** — nedívá se na nosiče, na klec, na endzonu ani na tackle zóny. Heuristiky existují jen pro Side Step a Grab.<br>⇒ **Každý náš odsun je volné přemístění soupeře, a tu volbu zahazujeme.**<br>⭐ **Nejde jen o geometrii, ale o OBSAZENÍ** *(uživatel 14.08.: „je důležité kdo stojí na a — jestli náš nebo soupeř")*. Když prázdné pole není, odsun **řetězí** a druhý článek je ten, kdo tam stojí. Kód rozlišuje jen prázdné/neprázdné (`anyEmpty`) a pak jede straight-back — **komu to tělo patří, neřeší**. Žebříček cílového pole podle obsazení:<br>• **soupeř** → dobré, řetěz posune **dva jejich** ⇒ když je vedle straight-backu pole se soupeřem a straight-back řetězí přes nás, je současná volba **striktně horší** ⇒ patří k **P9a**, ne k doktríně<br>• **prázdné** → výchozí<br>• **naše řadové tělo** → malá cena<br>• **náš roh klece** → vysoká cena *(úder, který měl roh očistit, ho rozebere)*<br>• **náš nosič** → veto<br>⇒ 44,2 % odsunových polí je obsazených, takže tohle není okrajový jev.<br>**Dopad 1 (uživatel 14.08.):** při čištění rohu blokem *„může odsun nechat soupeře nejen jako stojícího souseda rohu, ale nově navíc i souseda ballcarriera"*. Bije to do **27,2 %** bloků, kde polluter zůstane stát (Fable: na zemi je 72,8 %). A míří to na `REACH0`, což je podle E1 rozdíl mezi **1,8 %** a **33 %** ztráty míče.<br>**Dopad 2:** ranní **8 darovaných TD** ve 3000 hrách má týž kořen — ověřeno na `g0289`: pusher (23,8), nosič (24,7), „rovně dozadu" = **(25,6) = endzona**. Nebyla to smůla, byla to ta konstanta. | **OTEVŘENO — VYSOKÁ PRIORITA** *(blokuje bezpečné nasazení P2)* |
| **P10** | ⭐⭐⭐ **HODNOTA BLOKU SE NEODVOZUJE OD CÍLE — a nosič se odměňuje za MARKOVÁNÍ, ne za sražení.** *(uživatel 14.08.: „když je vedle našeho Longbearda možnost block na GR s míčem a navíc jsou kolem naši — co může být lepšího než jej blocknout?")* Odpověď: nic — a engine to neví.<br>**(1) Prior je plochý:** `greedyMacroRank` (`macro_mcts.cpp:47-48`) dává `BLITZ 20`, `BLOCK 15` — **jedna hodnota pro všechny bloky**. Blok na nosiče s Tackle a 3 kostkami má týž prior jako blok na linemana v protilehlém rohu. Kategorie „udeř na míč" v žebříčku není.<br>**(2) Tři existující členy o nosiči odměňují jen STÁNÍ VEDLE:** marking `+0,08×min(TZ,3)` (max +0,24, `:776`), sideline trap `+0,10` (`:808`), contain-vs-AG≥4 `+0,06×…` (max +0,12, `:819`). **Sražení nosiče nemá člen žádný.**<br>⚠️ **DOPOČÍTÁNO 14.08. — podezření POTVRZENO a je silnější, než vypadalo.** Všechny tři členy o nosiči visí na `ball.isHeld`, takže sražením **zmizí naráz** (−0,24 −0,10 −0,12), a místo nich naskočí `heuristic -= 0.1  // loose ball is bad` (`:762`) — ⭐ **který nerozlišuje „upustili jsme ho" od „právě jsme ho soupeři vyrazili".**<br>Bilance členů, které se mění (soupeřův nosič, 3 naše TZ, AG4):<br>• **uprostřed hřiště** (12 polí od endzony): **+0,13 → −0,02 = −0,15**<br>• **u lajny** (y=2): **+0,23 → −0,02 = −0,25**<br>• **může skórovat** (8 polí, MA9): −0,31 → −0,02 = **+0,29** ✅<br>⇒ **Čím blíž je soupeř skórování, tím víc heuristika blok chce; uprostřed hřiště se mu aktivně brání.** A S7 boxing-in = 32,4 % kol je právě ten střed.<br>⚠️ Poctivě: je to **listová evaluace**, MCTS to může přebít hledáním (sebráním míče hlouběji ve stromě). Netvrdím „AI nikdy nebije nosiče" — tvrdím, že úspěšný výsledek akce se hodnotí hůř než výchozí stav.<br>⇒ Sedí na starý nález *„trpaslík markuje a bije, nehoní"*: markovat jsme ho naučili, bít nedopsali.<br>⛔ **PODMÍNKA, BEZ KTERÉ JE OPRAVA ŠPATNĚ** *(uživatel 14.08.: „zkontroluj před blitz Wardancera na balon, že máš v záloze druhého pro pickup a třetího pro zablokování cesty k uzmutému balonu")*: **vyražený míč je zisk jen tehdy, když ho posbíráme.** Jinak jsme vyrobili volný míč uprostřed hřiště a dali ho rychlejšímu týmu — a trpaslík je v souboji o volný míč nejhorší možná rasa (MA4, AG2 u většiny těl).<br>⇒ `loose ball is bad` **není nesmysl, je to správné pravidlo se špatnou podmínkou**: platí, když scramble prohrajeme, neplatí, když ho vyhrajeme. ⇒ Rameno **nesmí** znít „bij nosiče", ale **„bij nosiče, když scramble vyhrajeme"**, a to je **rozpočet tří těl**: (1) kdo srazí · (2) kdo sebere · (3) kdo zavře cestu.<br>⭐ **Pravidlo je BEZPODMÍNEČNÉ a rasa soupeře o něm nerozhoduje.** Uživatel je řekl dvakrát a pokaždé stejně — u Longbearda proti GR jako součást zadání (*„a navíc jsou kolem naši"*), u Wardancera jako kontrolu. Rychlost soupeře neurčuje, **jestli** pravidlo platí, jen **jak těsně se ta trojice počítá**: proti Wardancerovi musí být třetí tělo blíž a cesta zavřenější, protože je na míči dřív. ⛔ *(Zapsal jsem to nejdřív jako dva protikladné případy lámající se podle rychlosti soupeře — to byl můj konstrukt, ne jeho pravidlo. Opraveno.)*<br>⇒ Dnešní člen se ptá **jen na nás** (`nearestDist` našeho nejbližšího, max +0,08) — nikdy na to, **kdo je blíž, my nebo oni**, a tělo zavírající cestu nemodeluje vůbec.<br>⭐⭐ **A musí se ptát i KDO JE RYCHLEJŠÍ** *(uživatel 14.08.)*: **cena ztráty míče není konstanta, je funkcí rychlosti soupeře k míči.** Proti skavenovi (MA9 + Dodge, a s Nerves of Steel i chycení v obklíčení) je upuštěný míč **skoro inkasovaný gól** — proto se jim vyplatí blitz na míč i za cenu ztráty těl, a proto nám dají **198 krádežových TD proti orkovým 31**. Proti orkovi (MA4–5, AG2–3) je to nepříjemnost, kterou často sebereme zpátky. ⇒ Táž jedna oprava, dva vstupy místo jednoho. Podrobně `evidence/matchup_asymmetry_20260814.md`.<br>⇒ Symetricky potvrzuje rozpočet tří těl: **my potřebujeme tři, abychom scramble vyhráli. Oni jedno.** Souvisí s [[project_bloodbowl_loose_ball_denial_doctrine_20260807]].<br>⛔ **POZOR NA ZÁMĚNU SITUACÍ** *(uživatel 14.08.)*: „nosič" a „polluter" **nejsou dva cíle v jednom kole**, jsou to cíle ve **dvou různých situacích**. Roh klece existuje jen v **našem kole s míčem** (S2–S5); soupeřův nosič jen v **obranném kole** (S7/S8). Prior na blok podle cíle proto **musí být indexovaný situací**, ne jeden plochý žebříček — jinak se opakuje táž chyba o patro výš. ⇒ P10 se dělí: **P10a** blok na soupeřova nosiče *(obrana)* · **P10b** blok na pollutera *(útok)*. **P10b NENÍ levnější cesta k P2, je to P2.** <br><br>⛔⛔ **P10a ZAMÍTNUTO JAKO NOČNÍ RAMENO 18.08. — Q1 pojistka ho zastavila.** Oprava napsána (`MCTSConfig::carrierBlockPrior`: blok/blitz na nosiče dostane prior floor 0,20 místo plochých 0,12, **jen když u pole nosiče máme aspoň tolik těl v dosahu co soupeř** — podmínka, ne preference, protože sražení nosiče míč UVOLNÍ a soupeř má v dosahu víc těl v 54,1 %). **Q1 na přeplněné desce:** podmínka funguje (nesporné pole 0,0 pp), ale **sporné pole se nehnulo taky — search si nosiče bere v 98 % už bez ramene**. **Korpus se jmenovatelem:** z **3 733** příležitostí jsme na nosiče udeřili v **81,5 %**, a bez souseda jsme na něj **6 143×** doblitzovali. ⇒ **Strop ~0,23 kola na zápas**, z toho část se udeřit nemá. Táž logika jako P8 (0,056 faulu/zápas). Kód zůstává za vypnutým flagem, testy 549/549. Doklad `evidence/p10a_q1_result_20260818.md`. ⚠️ **Neodpovězeno:** jestli je těch 81,5 % úderů dobře NAČASOVANÝCH a jestli se udeří SPRÁVNÝM tělem (P9). |
| **P13** | ⛔⛔ **ZMĚŘENO Q1 TESTEM 14.08. — OPRAVA NABÍDKY SAMA NESTAČÍ.** Postavená pozice: náš Slayer (ST3, Dauntless+Block) mezi **Black Orkem ST4** a **linemanem ST3**, 120 opakování na rameno, `diag_q1_target_choice_20260814.cpp`.<br><br>| | bloků zvoleno | **z toho Black Orc** |<br>|---|---|---|<br>| Dauntless v nabídce **OFF** | 84 | **0** |<br>| Dauntless v nabídce **ON** | 112 | **0** |<br><br>⇒ **Search si Black Orka nevybere ani jednou, ani když je mu nabízený.** Vybere vždy linemana vedle něj. Nabídka stoupla, volba se nezměnila.<br>⚠️ Počet bloků se zvedl 84 → 112, takže ta nabídka **něco** udělala — jen ne to zamýšlené: rozhýbala prohledávání a skončila u linemana. **Noční A/B by měřilo vedlejší efekt přidané volby, ne bití Black Orků.**<br>⛔ **NOČNÍ BĚH 14.08. PROTO ZASTAVEN** po ~5 minutách, ne po 14 hodinách. *(uživatel to předpověděl: „bojím se, že to toho Black Orka nevybere a měření bude nula")*<br>⇒ **P13 zůstává správnou opravou** (filtr oceňoval jinou akci, než jaká se provede) — **ale sama o sobě nemá co změřit.** Musí jít **spolu s úrovní 2**: cena cíle v prioru (**P15 / P10a**). To je přesně ten pětiúrovňový řetěz.<br>⚠️ **Výhrada:** jedna postavená pozice, 120 vzorků na rameno. 0/112 je průkazné pro **tuhle** pozici, ne důkaz pro všechny.<br>*(původní popis nálezu níž — platí, jen nestačí)*<br>⭐ **NABÍDKA BLOKU NEPOČÍTÁ S DAUNTLESS, PROVEDENÍ ANO** *(uživatel 14.08.: „soustředili jsme se na welfy a přitom bolí proti orkům — tam je Dauntless na ST4 orky úplně super plán")*.<br>`getBlockDiceCount` (`macro_actions.cpp:126`) počítá jen `Horns`, a jen u blitzu; **Dauntless nikde**. ⇒ Troll Slayer ST3 proti Black Orkovi ST4 se ocení jako **do kopce**, `dice` vyjde záporné a filtr `if (dice >= 2 || oneDieWorthOffering)` nabídku **zahodí**. Přitom `block_handler.cpp:386` Dauntless při provedení uplatní **správně** (před asistencemi, CRP, opraveno `9f98070`).<br>⇒ **Slayerovi se blok na Black Orka nikdy nenabídne**, ačkoli by se srovnal na ST4 a s jednou asistencí by z toho byly **dvě kostky pro nás**.<br>⭐ **Dauntless je nejsilnější právě proti ST4:** d6+3 > 4 ⇒ 2+ ⇒ **83 %**; proti ST5 67 %, proti Treemanovi ST6 jen 50 %. **Ork je jediný soupeř se čtyřmi ST4 hráči** — proti wood-elfovi je jeden Treeman, proto si toho nikdo nevšiml. Souvisí s tím, že proti orkovi dáváme **86 TD na 750 zápasů** proti 451 na skavena.<br>⚠️ **Třetí výskyt téhož vzorce za den** (po ceně hand-offu a po směru odsunu): **filtr oceňuje jinou akci, než jaká by se provedla.** ⇒ Projít `macro_actions.cpp` systematicky touž otázkou.<br>*(Zadáno Fablemu 14.08. jako doplněk k analýze orků — změřit, kolik bloků to bere a jestli to vůbec souvisí se ztrátami míče.)* | ✅ **ZMĚŘENO 14.→15.08., PROŠLO** — `evidence/weekend_result_20260817.md`. **dw-orc +4,08 pp** (SE 0,80; rameno vyskočilo ~1 739×/hru) · dw-sk **+2,28 pp** · orc-sk **−1,30 pp** (kontrola uvnitř 2 SE). Práh předregistrace splněn. ⛔ **ALE obě nuly se hnuly, ačkoli tam rameno prokazatelně NEBĚŽELO** (`cand_daunt = 0` v 6 000/6 000) ⇒ poctivý efekt je **dw-orc proti sdružené nule: +3,59 pp, ~3,4σ**, ne +4,08. Fable čekal +1–2 pp. **Zbývá: zapnout v produkci** (`dauntlessInOffer` je dál `default false`). |
| **P15** | ⭐⭐⭐ **PRÁH NABÍDKY BLOKU NEZNÁ CENU CÍLE** *(uživatel 14.08.: „kdyby třeba balon nesl Black Ork bez Blocku a my kvůli dvě proti po něm nešli")*.<br>Brána (`macro_actions.cpp:562`):<br>`bool oneDieWorthOffering = dice == 1 && att.hasSkill(Block) && !(ball.isHeld && ball.carrierId == att.id);`<br>`if (dice >= 2 \|\| oneDieWorthOffering) { ... }`<br>Ptá se, jestli **útočník** není náš nosič. **Nikde se neptá, jestli CÍL není soupeřův nosič.** A `dice` je záporné, když vybírá obránce ⇒ **blok do kopce se nenabídne NIKDY**, bez ohledu na terč.<br>⇒ **ST4 s míčem je pro 9 z 11 našich těl nedotknutelný**, přestože jeho sražení je nejcennější událost na desce. **Filtr tu volbu ani nepředloží — search nemůže zvážit, co nedostane.**<br>**Je to kompromis, ne jasná chyba:** blok do kopce dá soupeři ~**30,6 %** šanci vybrat si „Attacker Down" (naše Both Down kryje Block) = turnover, proti tomu stojí uvolněný míč. **Právě proto to má rozhodovat search, ne konstanta ve filtru.**<br>⚠️ **P13 to pokrývá jen zčásti** — Dauntless srovná Slayera proti ST4 na jednu kostku a s Blockem projde, ale **Slayeři jsou dva**; zbylých devět těl zůstane zablokovaných.<br>⛔⛔ **ZAMÍTNUTO JAKO VYSVĚTLENÍ ORKA (Fable §9.5, 14.08.)** — a je to oprava mého vlastního tvrzení z téhož odpoledne. Zóna „dosáhneme, ale nemáme 2 kostky" je proti orkovi 64,7 % kol, ale **98,2 % z toho je BLZ=1 — a ty akce SE nabízejí** (blitz na nosiče má prior +10 i do kopce; 1k sousední blok mají všichni naši přes Block). **Skutečná ne-nabídka (blok do kopce) je 1,8 % zóny**, konverze 1k na Throwera 0,109. ⇒ **Limit je fyzika, ne filtr.** P15 zůstává platnou opravou rozhodovací vrstvy, ale **na orka nemíří** a strop má malý.<br>⚠️ **Ponecháno jako záznam, co jsem tvrdil a čím to bylo vyvráceno** — původní (chybné) zdůvodnění níž.<br>⭐⭐ ~~ZOSTŘENO 14.08. odpoledne — není to okrajový případ, je to hlavní kanál.~~ Změřeno: **Guard v rozích klece vs. náš dostupný 2k blitz na jejich nosiče** — skaven 6,7 % / **54,4 %** · wood-elf 5,6 % / 43,4 % · human 20,6 % / 19,0 % · **ork 50,9 % / 7,5 %**. Monotónní a téměř dokonale inverzní: Guard v rohu znamená, že jejich obranná asistence **nejde zrušit značkováním**, takže proti orkovi jsme **do kopce skoro vždycky**.<br>⇒ **Těch 7,5 % není měření naší volby — je to měření toho, co nám filtr vůbec dovolí zvážit.** Proti orkovi se náš útok na jejich nosiče **prakticky nikdy nedostane do nabídky**, a to je přesně ten matchup, kde dáváme 86 TD proti 451 na skavena. **P15 tedy nemíří na okrajový případ, ale na hlavní příčinu našeho nejhoršího matchupu.**<br>⇒ Jejich nosič je přitom **nejměkčí cíl v týmu**: ork Thrower 78,5 %, human 74,4 %, skaven 48,1 % — ST3, AV7–8. Stojí uprostřed nejtvrdší ochrany, jakou mají.<br>⭐ **Pátý a nejobecnější výskyt vzorce dne:** u ostatních čtyř filtr neznal dovednost; tady **nezná cenu cíle vůbec**. Souvisí s **P10a** (plochý prior `BLOCK 15` pro všechny bloky) — je to týž defekt na dvou místech. | **OTEVŘENO — VYSOKÁ PRIORITA** |
| **P19** | ⭐⭐ **HODNOTA CÍLE JE VZÁCNOST × ROLE — a řetěz „srazit → faulovat → odstranit"** *(uživatel 14.08.: „někam tam bude patřit zničení Black Orka — a třeba i následný faul je hodnotnější než block na linemana, na kterého nemá cenu plýtvat faul")*.<br>Black Orků mají **4**, jsou **ST4 + Guard** a tvoří **50,9 % jejich rohů klece**. Linemanů mají **11** a jsou zaměnitelní. ⇒ Odstranit Black Orka degraduje jejich klec **natrvalo**; sražený lineman se nahradí.<br>**Faul to dotahuje:** je **1 na kolo** a riskuje vyloučení ⇒ patří na nejcennější ležící tělo. **P8** říká, že dnes bere **prvního ležícího v pořadí sousedních polí a nehodnotí nic** — takže se plýtvá na linemany.<br>⛔⛔ **HÁČEK, KTERÝ TU HODNOTU V NAŠEM MĚŘENÍ NIČÍ: T5.3 — zranění nepřetrvávají přes drive.** Po každém TD se staví 11 čerstvých. ⇒ **Odstraněný Black Orc je pryč do konce DRIVU, ne zápasu**, a celý řetěz je **systematicky podhodnocený ve všech našich A/B**, včetně dnešního Dauntless běhu.<br>⇒ **Pořadí oprav: T5.3 PŘED P19 a P8.** Dokud zranění nepřetrvávají, měří se attrition proti pravidlům, která ji zlevňují — a každý výsledek o bití je dolní odhad. | **OTEVŘENO** — Q2; blokováno na **T5.3** <br><br>⭐⭐⭐ **DOPLNĚNO 21.08. — ŘEZ ZMĚNIL CENU TŘETÍHO ČLÁNKU ŘETĚZU.** Uživatel 21.08.: *„proti orkům nám pomůže v prolamování Troll Slayer blitz na silného orka s naší asistencí a následný faul."*<br>**Před opravou vstávání bylo sražení TRVALÉ** *(nikdo nevstával — 0,4 %)* ⇒ **sražení BYLO odstranění** a faul přidával skoro nic. **Od 21.08. je sražení DOČASNÉ** ⇒ články se rozdělily: **blok otevře okno, odstraní až FAUL.** ⇒ **Hodnota řetězu prudce vzrostla.**<br>⛔ **A NENÍ to P8** *(výběr cíle mezi ležícími — strop spočítán 17.08., dělat se nemá, protože v 74,8 % je cíl jediný)*. Tohle je **výroba cíle a jeho následné odstranění**, tedy sekvence, ne vybírátko. **Ta rozvaha se dnes málem zopakovala znovu — ověřeno, že P8 zamítnutí platí dál.**<br>**Změřeno 21.08. (300 her, jen dvojice dwarf-orc):** ✅ Troll Slayer si silné orky **vybírá sám** — **583 bloků na ST 4 proti 241 na ST 3 (2,4×)**. ⛔ Ale řetěz nepokračuje: 1 184 našich kol · **283 kol s ležícím orkem** · **180 faulů**, z toho **jen 73 na nejsilnějšího ležícího**; podle síly **84× ST 3 · 96× ST 4**, tedy skoro nastejno, přestože ST 4 orků je menšina.<br>⭐ **Metr na ocenění řetězu od 21.08. existuje: `corridor_strength`** *(součet ST stojících těl v koridoru)* — řetěz má být oceněn tím, **o kolik sníží sílu zdi**, což je přesně to, co žádá doktrína *„trpaslík zeď PROLOMÍ"* při naměřených **0,3 % prolomení**.<br>⚠️ **Nepřibalovat P36** *(Dauntless chybí ve VÝBĚRU blitzujícího; proti ST 4 vychází na 2+, tedy 83 %)* — je to druhá polovina téhož tahu a jde **až po** této. *(Uživatel 21.08.: „jedna změna najednou.")* |
| **P16** | ⭐⭐ **ROH KLECE SE VYBÍRÁ BEZ OHLEDU NA VHODNOST TĚLA** *(uživatel 14.08.: „zakaž skavenům GR do rohu, jestli to umíš zobecnit")*.<br>Změřeno: **skaven staví 36,4 % rohů z Gutter Runnerů (ST2, AV7)** — nejhorší možné tělo do klece: neuassistuje, neudrží pole, a je to zároveň hráč, který je jinde nejcennější.<br>**Kód pravidlo má, ale ptá se na špatnou věc.** `eligibleCornerPlayer` (`cage_advance.cpp:59`) filtruje jen **spolehlivost aktivace** (Bone-head, Really Stupid, Wild Animal, Take Root, Secret Weapon, Ball & Chain). **Na vhodnost ani na cenu jinde se neptá.** A žije to jen se zapnutou branou — **v produkci vypnutou** ⇒ dnes rohy vznikají samovolně přes heuristiku (`macro_mcts.cpp:686`), která **počítá každé stojící tělo do 4 polí stejně**.<br>⭐ **ZOBECNĚNÉ PRAVIDLO** *(ne „zakázat GR", ale proč)*: **roh klece je tělo, které (a) pole udrží pod blokem a (b) je jinde méně cenné než to pole.** Preferovat **Guard → ST → AV**; vyhnout se **nejrychlejšímu volnému tělu** a **určenému nosiči** (Sure Hands / Catch / vysoké AG) — ti jsou cennější v pohybu.<br>Vyjde správně pro všechny rostery: skaven → linemani místo GR · ork → Black Orci · wood-elf → linemani místo Wardancerů · my → Longbeardi a Slayeři s Guardem *(co už děláme)*. ([[feedback_implement_the_rule_not_the_outcome]])<br>**Dvě místa k opravě:** (1) `eligibleCornerPlayer` — doplnit vhodnost, ne jen spolehlivost *(účinné až se zapnutou branou)*; (2) **escort člen heuristiky — vážit vhodností místo počítání těl** *(účinné hned, a na OBOU stranách)*.<br>⭐ **Zase „jedna oprava, obě strany"** — táž vlastnost jako u P10a: heuristika je slepá k tomu, KDO to tělo je. | **OTEVŘENO** | <br>⭐ **ZMĚŘENO 17.08. — longbeardy tam UŽ dáváme, ale AG3 těla tam pořád stojí.** Podíl kol, kdy hráč stojí v rohu klece: **Longbeard +Guard 20,2 %** · Troll Slayer +Guard 16,7 % · **Blitzer +Guard+Tackle 12,6 %** · **Runner +Block 11,5 %** · Longbeard 8,0 %. ⇒ Guard těla rohy drží nejvíc *(to je správně)*, ale **~čtvrtina kol našich dvou AG3 pozic se stráví jako nábytek klece** — a AG3 těla jsou jediní použitelní příjemci hand-offu (67 % vs 50 %). ⇒ **Pravidlo: roh patří Guard tělu se špatnýma rukama (Longbeard +Guard, Troll Slayer). Runner a Blitzer nejsou nábytek — jsou to příjemci** *(viz **P29**)*.
| **P17** | ⚠️ **HRÁČ S BLOCK+WRESTLE WRESTLE PŘI ÚTOKU NIKDY NEPOUŽIJE — natvrdo, ne volbou.** CRP: *„This player **may** use Wrestle when he blocks or is blocked … Both players are Placed Prone **even if one or both have the Block skill**."* Engine (`block_handler.cpp:494`): `attWrestle = hasWrestle && !hasBlock`.<br>⇒ Je to **volba, kterou kód udělal za nás** — a udělal ji špatně přesně tam, kde na ní záleží: **blok na soupeře s Blockem**, kde se „Both Down" jinak vzájemně vyruší a **nestane se nic**. S Wrestle by šli oba k zemi.<br>⇒ Navíc **vnitřně nekonzistentní**: u obránce (`defWrestle`) žádné takové omezení není.<br>**Proč to teď vyplavalo:** uživatel 14.08. — *„proti orkovi musíme mít někoho s Wrestle."* Má pravdu (Black Orci i Blitzeři mají Block ⇒ naše Both Down se vyruší; Wrestle to přebije a složí jejich ST4 tělo), ale **v dnešním enginu by to nefungovalo**, protože Longbeard má Block. **Bez P17 je ta rosterová změna k ničemu.**<br>⭐ **Šestý výskyt vzorce dne** — rozhodnutí zadrátované do resolveru místo vyhodnocení. | **OTEVŘENO — blokuje rosterovou změnu** |
| **P20** | ⭐⭐⭐ **PÁR NENÍ TÁŽ HRA S JEDNÍM PŘEHOZENÝM BITEM — a náš práh to nezná.** 15.08.: na `dw-sk` a `orc-sk` se rameno **ani jednou nespustilo** (`cand_daunt = 0` v 6 000/6 000 her, větev `macro_actions.cpp:156` je jediné místo rozdílu) — obě ramena tam běžela na **stejném kódu** — a přesto vyšlo **+2,28 pp (+2,3 SE)** resp. **−1,30 pp**. ⇒ **Předregistrovaný práh „PROŠLO" splnilo rameno, které nedělá nic.**<br>Příčina: orientace 0 hraje `seed*2`, orientace 1 `seed*2+1`, MCTS seedy se liší o 1 ⇒ pár jsou **dvě různé hry na spřízněných seedech**, ne tatáž hra. Redukce rozptylu je mnohem slabší, než na čem stály všechny naše prahy — **včetně „brána klece ZAMÍTNUTA −0,0297 (−2,0σ)"**.<br>⇒ **Každé A/B musí od teď obsahovat matchup s expozicí NULA a efekt se čte PROTI NĚMU**, ne proti nule. *(Souvisí se šumovým dnem ±5,3 pp a s tvrzením „harness je DETERMINISTICKÝ" — to platí o hře při daném seedu, ne o páru.)*<br>⚠️ **Přehodnotit ZAMÍTNUTÍ brány klece** touž optikou: běželo bez nulového ramene. | **OTEVŘENO — VYSOKÁ PRIORITA** |
| **P21** | ⛔⛔ **HAND-OFF: 0 VÝSKYTŮ VE 3 000 HRÁCH.** Fronta si 14.08. vymínila *„nejdřív ověřit hand-off kritérium na doběhlém korpusu — kdyby nefungovalo, nesmí být v baseline"*. **Neověřilo se**, a korpus, který v baseline **je**, má `HAND_OFF` **nula** (`BLOCK` 44,7/hru · `PASS` 0,30 · `CATCH` 1,97 · **`HAND_OFF` 0,00**) — přestože logování bylo přidáno právě proto (`3b11d33b`) a Q1 sweep dává na postavených pozicích **18,3 %**.<br>Dvě čtení, obě levná: **(a)** situace „Longbeard nese a vedle je volný Runner" je v reálné hře tak vzácná, že za 3 000 her nenastala ⇒ P5 je správná, ale bezcenná oprava; **(b)** makro cesta hand-offu se resolvuje jinudy než `pass_handler.cpp` a log ji nepokrývá ⇒ nevíme nic.<br>⇒ **Rozhodne to spočítání VÝSKYTU té situace ve snímcích korpusu**, bez dalšího běhu. | ✅ **ZODPOVĚZENO 17.08.** — `evidence/handoff_offered_never_chosen_20260817.md`. ⛔ **PŮVODNÍ ZÁVĚR (c) ZRUŠEN TÝŽ DEN.** Nula byla **vada exportu logu**: `bb_module.cpp:325` měl stráž `typeIdx < 21` proti 22 jménům ⇒ `HAND_OFF` se ukládal jako `UNKNOWN` (commit `3b11d33b` přidal jméno a stráž nezvedl). **Naše strana zahrála 130 hand-offů ve 3 000 hrách** (349 událostí celkem, 51 vedlo k TD v témž kole) ⇒ **hand-off ŽIJE**. Stráž opravena. ⚠️ Ověřoval jsem emisi eventu i poziční mapu jmen — obojí správně — a **stráž o řádek výš ne**. Původně jsem psal: ~~nabízí se 10,4× za zápas a nevybere se ani jednou~~ (počítadlo `takeHandOffOfferCount`, 50 her: 519 nabídek, 0 zahrání). Situace nastává v **329 kolech ze 3 000 her** (0,69 % kol, 7,3 % zápasů; podlaha — snímek je začátek kola), ověřeno i na čerstvé binárce. Log je v pořádku (`pass_handler.cpp:428` + poziční mapa v `bb_module.cpp` sedí). ⛔ Cestou nalezena vada nástroje: **Q1 sweep počítal `HAND_OFF` a `PASS` do jednoho čísla** — po rozdělení je těch 18,3 % opravdu předání (PASS 0). ⇒ **Vada se přesunula z NABÍDKY do VOLBY** (týž vzorec jako P13). **Patří k P15 / P10a**, ne k P5. ⚠️ Strop je malý (~0,1 akce na zápas) — spočítat napřed, než na to půjde rameno. |
| **P22** | ⛔ **KORPUS A JEHO BASELINE BĚŽELY NA JINÉM ENGINU.** Korpus 14.08. `engine@e273a369`, baseline `night_big_20260813/` `engine@9f98070c` — **šest commitů do `engine/`, z toho čtyři mění chování** (2× hand-off `38dcad6d`/`4f2c658d`, odmítnutí darovaného TD `eb231c5c`, čítače). Rozklad drivů navíc jel s opravou atribuce TD (`14c7d035`), kterou baseline neměla (proto tam je sekce ANOMÁLIE a v novém korpusu ne). ⇒ **A 17 % → 21 % a C 41 % → 37 % NELZE připsat Dauntlessu.** Předregistrované předpovědi vyšly (K33 76,6 → 78,9 % · C proti orkovi 59 → 57 % · REACH0 41,0 → 41,5 %), ale **nejsou přiřaditelné**.<br>✅ **Opraveno systémově:** `night_stamp_head` / `night_check_baseline` v `run_night_lib.sh`; otisk `ENGINE_HEAD` **doplněn zpětně** do obou korpusů. | **UZAVŘENO jako vada aparátu** *(nález o měně drivů zůstává NEZMĚŘENÝ)* |
| **P23** | ⭐⭐ **SPOUŠTĚČ NEPŘEŽIJE DRUHÉ SPUŠTĚNÍ — a 14.08. se spouštělo dvakrát.** Zabití po 5 minutách a znovuspuštění v 15:15 bylo **úsudkově správné**; vadný byl aparát: **(1)** zámek `mkdir` + `trap EXIT` nepřežije `kill -9` ⇒ druhé spuštění tiše skončí; **(2)** 12 shardů na `&` — zabití rodiče **nezabije děti**, sirotci píšou do týchž adresářů jako nový běh *(prošlo jen proto, že se shardy nestihly rozjet)*; **(3)** `chain.log` **nemá první spuštění**, noc vypadá jako jedno čisté — *snímek se vydává za stav*; **(4)** řádky se otevíraly `fopen(…,"a")` ⇒ znovuspuštění **přidá druhou sadu** a po deduplikaci podle seedu to i vypadá správně. Táž křehká kopie zámku byla v **devíti** spouštěčích. | ✅ **UZAVŘENO 17.08.** — `run_night_lib.sh` (PID zámek se sebráním starého · úklid dětí na EXIT/INT/TERM · číslované POKUS n · `fopen "w"`); **každý nový noční spouštěč ho musí sourcovat** |
| **P24** | ⭐ **VYHODNOCENÍ SE NESHODLO SAMO SE SEBOU.** Binárka tiskla práh `>= +0.03`, předregistrace říkala **±0,02**; hlavička attrition v mode 4 tvrdila `cageAdvance on/off` (v pondělí by se četlo rameno brány místo Dauntlessu); a řádek *„spustilo se to rameno vůbec?"* — **nejcennější řádek celého čtení** — **netiskl nikdo** a musel se 17.08. dolovat skriptem. | ✅ **UZAVŘENO 17.08.** — harness tiskne práh i s názvem dokumentu, `Dauntless ON/off`, a nově `ARM Dauntless: N offers … NEVER FIRED => TRUE NULL` |
| **P25** | ⭐⭐⭐ **AUDIT MĚŘICÍCH NÁSTROJŮ — „počítá tenhle čítač to, co tvrdí jeho jméno?"** *(uživatel 17.08.: „zase jsem se dočetl, že jsme měřili jinak, než jsme chtěli")*.<br>**Je to DRUHÉ KOLO auditu z 13.08.** To první mířilo na **kontroly plnění** (chybějící jmenovatel, prázdná množina = „splněno") a dalo `Check(ok,n,deg)`+N/A. Tohle míří na **diagnostické nástroje**: čítače, printy a A/B harness. Za jediný den (17.08.) se jich našlo **sedm**, všechny bez hledání:<br>**(1)** pár není tatáž hra ⇒ práh splnilo mrtvé rameno *(**P20**, opraveno CRN)*<br>**(2)** korpus a baseline na jiném enginu *(**P22**)*<br>**(3)** binárka tiskla práh `+0.03`, dokument `±0.02` *(**P24**)*<br>**(4)** hlavička attrition v mode 4 tvrdila `cageAdvance on/off` *(**P24**)*<br>**(5)** řádek „vyskočilo to rameno vůbec?" **netiskl nikdo** *(**P24**)*<br>**(6)** Q1 sweep počítal `HAND_OFF` a `PASS` **do jednoho čísla** *(**P21**, opraveno)*<br>**(7)** „12 nezávislých nullů" z 13.08. bylo **8 unikátních** — zbytek bit-identické duplikáty *(Fable 17.08.)*<br>⭐ **ZBÝVÁ PROJÍT** *(to je ta práce, (1)–(7) jsou hotové)*:<br>&nbsp;&nbsp;• **čítače měří vnitřek prohledávání, ne hranou hru** — `cand_daunt` 1 739/hru **nejsou bloky**, jsou to evaluace v MCTS. Kdekoli se takové číslo cituje jako „kolikrát jsme to udělali", je to špatně. **Projít všechny `take*Count()` a k jménu doplnit, CO počítají.**<br>&nbsp;&nbsp;• **`diag_rules_checks_*.py`** — druhé kolo po opravě z 13.08.: má každá kontrola jmenovatele a je N/A odlišené od „splněno"?<br>&nbsp;&nbsp;• **`diag_drive_failure_*.py`** — definice kategorií A/B/C/D1/D2: co přesně je „A"? Nikde to není napsané a čte se to jako „skórovali jsme".<br>&nbsp;&nbsp;• **ostatní `diag_*` binárky a skripty** — jedinou otázkou: *počítá to, co tvrdí jeho jméno, a na jakém jmenovateli?*<br>⚠️ **Pravidlo, které z toho plyne pro každý nový nástroj:** k číslu patří **jmenovatel** a **věta, co se počítá** — jinak se za měsíc přečte jako něco jiného. | ⏳ **Z VĚTŠÍ ČÁSTI HOTOVO 17.08.** — `evidence/instrument_audit_20260817.md`. ⭐ **NÁLEZ: čítač přeceňoval 186×** — `takeDauntlessRollCount` hlásil 349/zápas, skutečně odehraných Dauntless hodů je **1,88/zápas** (5 651 celkem, srovnalo 73,8 %, aspoň jeden v 60,3 % her — čísla, která šla citovat celou dobu). Padá i tvrzení, že *rozdíl nabídka vs. odebrání* říká, jestli si to search vzal — obě čísla jsou z vnitřku prohledávání. ⇒ **přejmenováno na `take*EvalsInSearch`** (jméno je dokumentace, kterou lidé opravdu čtou). Dál: **legenda kategorií A/B/C/D1/D2 se tiskne do `drives.txt`** (definice byly jen v `.py`, čte se ale výstup) a **legenda jednotek `n`** do kontrol (K33 = kola, K30 = příležitosti k dodgi — lišily se 5× a četly se jako srovnatelné). ⚠️ **ZBÝVÁ:** ostatních 20+ `diag_*` souborů + hlídání rebuildu za běhu. | **ČÁSTEČNĚ OTEVŘENO — vysoká priorita** |
| **P26** | ⛔ **`CHAIN_SCORE` JE POTVRZENĚ MRTVÝ — a je to vada PROVEDENÍ, ne volby** *(Fable 17.08., `evidence/fable_offered_not_chosen_20260817.md`)*. Makro se nabídne v **270 kolech** ze 48 000 a **řetěz se nezahraje ani jednou**: krok 1 (pass) spálí `passUsedThisTurn` a krok 2 (hand-off) pak `rules_engine.cpp:99` odmítne. Potvrzuje **P4** jako živý bug. ⚠️ Strop malý (270 kol / 3 000 her), ale oprava je v pravidlové vrstvě, ne v doktríně. | ✅ **OPRAVENO 17.08.** — a je to **PRAVIDLOVÁ CHYBA, ne doktrína**, což mění její váhu proti stropu. CRP, HANDING-OFF: *„The Hand-Off Action is added to the list of Actions like Move, Block, Blitz and Pass. A coach may only declare one Hand-Off Action per turn."* ⇒ hand-off má **vlastní limit na kolo**. Sdílel ale `passUsedThisTurn`, a to **v OBOU směrech**: přihrávka blokovala hand-off (`rules_engine.cpp:99`) a hand-off spálil přihrávku (`pass_handler.cpp:408`). ⇒ `CHAIN_SCORE` byl **nesplnitelný z definice**. Přidán `handOffUsedThisTurn`; každé makro hlídá svůj limit, `CHAIN_SCORE` oba. ⚠️ Existující test `NoPassActionsWhenPassUsed` tvrdil staré chování a **procházel jen proto, že spoluhráč nebyl soused** — opraven, doplněny 3 testy na obojí směr. **544/544 zelených.** |
| **P27** | **`BLITZ_AND_SCORE` se nabízí a nekonvertuje** *(Fable 17.08.)*. Nabídnuto v **1 123 kolech** (789 bez současné nabídky SCORE), nosič blitzoval 38×, **TD 2×**. Vada ve **VOLBĚ**. ⚠️ **Strop 0,26 kol/zápas**, a před psaním kódu se musí změřit, **kolik z těch kol je při vedení** — tam je cap prioru 0,02 na celou SCORE-rodinu **záměr** (doktrína stall), ne chyba. | **OTEVŘENO — brzda dostala ZADÁNÍ 24.08.: `M10`** *(dřív visela bez čísla úkolu, tedy stejně jako P31, které tak leželo 33 dní)* |
| **P28** | ⛔ **OPRAVA P10a: PRIOR NENÍ PLOCHÝ** *(Fable 17.08.)*. Tvrzení o plochém prioru `BLOCK 15` **neplatí** — je to `greedyMacroRank`, používaný jen při leaf-lookahead. Skutečné priory mají floory **0,08–0,90** a capy, mj. **cap 0,02 na celou SCORE-rodinu při vedení** *(doktrína stall, tedy záměr)*. Jediné útočné makro bez flooru je `PASS_ACTION` (~0,05 po renormalizaci). ⇒ **Plochý prior mrtvá makra nevysvětluje; hlavní vrah je listová evaluace (Q).** Přeformulovat P10a i P15 tím směrem. | **OTEVŘENO — přeformulovat P10a/P15** |
| **P29** | ⭐⭐⭐ **PŘÍPRAVA PŘÍJEMCE NA POSLEDNÍ TAH — „elfí catcher" pro trpaslíky** *(uživatel 17.08.: „přidat specificky pro poslední tah, kdy je druhý Runner nachystaný vepředu, ať prodlouží dosah… a čím víc cílů, tím lepší šance, že pak jeden bude volný")*.<br>**Polovina toho už v enginu JE:** větev `emergency` v nabídce přihrávky **obchází bránu `swap` úplně** (`worthIt \|\| emergency`), takže **Runner → Runner se v kolech 7–8 nabídne**, když nosič na endzonu nedosáhne a příjemce ano. Pohyb před předáním taky funguje — **49,9 % hand-offů v korpusu předává až po pohybu** (průměrně 3,8 pole).<br>⛔ **Chybí ta DRUHÁ polovina: nikdo příjemce nepřipraví.** Změřeno na 3 000 hrách: kol 7–8 s naším nosičem **5 625** (1,88/zápas), z toho nosič sám nedosáhne v **78,5 %** — a **v 73,8 % těch kol NENÍ KOMU PŘEDAT**. Příjemce existuje jen ve **4,8 %** (volný ve 3,2 %), průměrně **1,22 kandidáta**. ⇒ Emergency větev je napsaná dobře a **nemá na koho střílet**.<br>⭐⭐ **STROP — a je to nejvyšší, jaký jsme dosud spočítali.** Ze 4 136 kol bez příjemce chybí nejlepšímu kandidátovi: **1 pole 6,5 % · ≤2 14,8 % · ≤3 27,1 % · ≤6 74,2 %**. Runner urazí za kolo 6 (s GFI 8). ⇒ konzervativně **≤3 pole = 1 121 kol = 0,37 na zápas** šancí navíc, volněji ≤6 polí = **1,02/zápas**. Při konverzi ~50 % je to **~0,18 TD/zápas proti dnešním 0,75 = +25 %**.<br>⚠️ **Háček v kvalitě příjemce:** nejbližším kandidátem bývá **Troll Slayer** (340×) a **Blitzer** (278×), Runner jen 252×. Slayer je **AG2** ⇒ chytá na 4+ (50 %). Příprava tedy musí tlačit dopředu **Runnera**, ne libovolné tělo — jinak se strop propadne.<br>⇒ **Úkol: doktrína „v kole 6–7 pošli druhého Runnera dopředu do dosahu endzony a nech ho volného."** Souvisí s (5) „čím víc cílů, tím větší šance, že jeden bude volný" — redundance proti markování. <br><br>⭐⭐ **ZPŘESNĚNO 17.08. — PŘIPRAVOVANÝ PŘÍJEMCE JE BLITZER, NE DRUHÝ RUNNER** *(uživatel: „druhý Runner je jediný, kdo může vyrazit dopředu — a být sražen?")*. Změřeno na 3 000 hrách, kola 6–7 (n = 5 729):<br><br>| | roh klece | v kontaktu | **VOLNÝ** | **LEŽÍ** | volný a doběhl by k EZ |<br>|---|---|---|---|---|---|<br>| Troll Slayer | 15,9 % | 12,9 % | **50,5 %** | 20,6 % | 0,47/kolo |<br>| **Blitzer** | 14,5 % | 14,7 % | **48,4 %** | 22,5 % | **0,47/kolo** |<br>| Longbeard | 12,0 % | 11,9 % | 43,5 % | 32,7 % | 0,60/kolo |<br>| **Runner** | 9,6 % | 11,3 % | **33,7 %** | **45,4 %** | **0,24/kolo** |<br><br>⇒ **Obava se potvrdila: druhý Runner LEŽÍ v 45,4 % kol 6–7** — je nejrychlejší (MA6) a má nejlepší ruce, ale **AV8** je nejměkčí brnění v týmu. **Není jediný kandidát, je ten nejhůř dostupný.**<br>⇒ **Blitzer je lepší připravovaný příjemce:** AG3 stejně jako Runner (**chytá na 3+, 67 %**), o pole kratší dosah (MA5), ale **AV9** a **volný dvakrát častěji**. Runner je první volba jen tehdy, když zrovna stojí.<br>⇒ Redundance („čím víc cílů, tím větší šance, že jeden bude volný") má konkrétní tvar: **4 těla s AG3** = 2 Runneři + 2 Blitzeři. Longbeard a Slayer jsou **AG2 ⇒ chytají na 50 %** a na tuhle roli nepatří. <br><br>⭐⭐ **CENA DOPOČÍTÁNA 17.08. — a je malá.** Pravidlo ze 14.08. žádá vedle výnosu i rozpočet, z něhož se platí. Kola 6–7 s **volným AG3 kandidátem na dosah endzony do dvou kol: 3 212 = 1,07 na zápas.** Z toho:<br>• **60,7 % — kandidát u nosiče NESTOJÍ ⇒ jeho odchod nestojí NIC** *(neubere roh ani zeď)*<br>• 39,3 % — kandidát je součástí těla u nosiče, **ale v 19,4 % všech těch kol je k dispozici JINÝ kandidát mimo klec**<br>⇒ **v ~80 % případů jde příjemce poslat dopředu bez dotyku na ochranu nosiče**; jen ~20 % vyžaduje volbu „roh, nebo příjemce".<br>Hustota v těch kolech: **2,21 našich sousedů nosiče z 8** a jen **0,48 soupeře** u nosiče ⇒ nosič v kolech 6–7 **není pod tlakem**, takže se tam neplatí ani expozicí.<br>⚠️ **Poctivá výhrada:** měřil jsem **sousedství**, ne `REACH0` (BFS „dosáhnou bez dodge"). Sousedství je proxy; přímý přepočet REACH0 s odebraným tělem zbývá.<br>⇒ **Strop 0,37–1,02 šance na zápas, cena ~0 ve 4 z 5 případů. Jediná dnešní položka, která prošla oběma testy** *(P8 padlo na stropu, P26 na stropu ale prošlo jako parita)*. | **OTEVŘENO — PRVNÍ V POŘADÍ, strop i cena změřeny** |
| **P30** | ⭐⭐ **CENA ODSTRANĚNÍ KONKRÉTNÍHO TĚLA — spočítat ZVLÁŠŤ pro Gutter Runnera a pro Black Orka** *(uživatel 17.08.: „pak by to chtělo zvlášť později vyčlenit výpočet na sražení a faul GRunnera nebo silnýho orka")*.<br>Dosud se hodnota cíle řeší **obecně** (P19 „vzácnost × role", P15 cena cíle) a obecné číslo nikoho nepřesvědčí. Tyhle dva cíle jsou **kvalitativně různé** a mají se počítat každý po svém:<br>• **Gutter Runner** *(skaven, MA9 AG4 AV7 Dodge)* — je to **stroj na TD**, ne článek formace. Odstranit ho znamená sebrat soupeři **schopnost skórovat**; AV7 je nejměkčí terč, ale Dodge ho drží mimo dosah. Starý odhad z P8: **4,4× lepší cíl** než lineman *(číslo je nutné přeměřit, pochází z doby před dnešními opravami)*.<br>• **Black Orc** *(ork, ST4 Guard, 4 v týmu, 50,9 % jejich rohů klece)* — je to **nosná konstrukce**, ne skórer. Odstranit ho znamená **degradovat klec**, a to trvale.<br>⭐ **Co se změnilo dnes a proč to má teď smysl:** **T5.3 je uzavřené** — zranění PŘETRVÁVAJÍ (0 návratů ze 648 casualty). Odstranění tedy platí **do konce zápasu, ne do konce drivu**, což hodnotu obou cílů **zvyšuje** a dělá z výpočtu smysluplnou věc. Do dneška se to počítalo proti pravidlům, která ji zlevňovala *(a ten předpoklad byl mimochodem taky chybný, viz rámec o násobiteli)*.<br>**Co má výpočet obsahovat:** (a) pravděpodobnost odstranění na pokus, rozpadlá na blok → armor → injury → casualty, pro každý terč zvlášť *(AV7 vs AV9, Thick Skull, Mighty Blow)*; (b) **kolik drivů v zápase ještě zbývá** v okamžiku odstranění — tím se výnos násobí; (c) co soupeři reálně ubude *(skaven: podíl jeho TD nesených GR; ork: čistota a rychlost jeho klece)*; (d) **cena pokusu**: faul je 1/kolo a riskuje vyloučení na dvojici, blok/blitz se platí z rozpočtu.<br>⏰ **SPOUŠTĚČ:** až (1) bude přepočítaná σ-tabulka na `corpus_baseline_20260817` *(dnešní tabulka je pozastavená, viz P25)* a (2) bude hotové **P29**. Do té doby se nepočítá — priority by se řadily podle metru, který se právě přeměřuje. | **ODLOŽENO — spouštěč uveden** |
| **P31** | ⛔ **SLOUČENO 24.08. DO `M-ROUND` / M1 — NEEDITOVAT ZDE.** Táž vada byla vedená na DVOU místech, a právě to je důvod, proč se „ztratila“.<br>**Historie: 22.07. (uživatel) → ztraceno 26 dní → 17.08. (úklid paměti) → 24.08. (Fable audit pohybu).** Třikrát nalezeno, ani jednou opraveno.<br>⭐ **A nebylo zapomenuté, bylo ZABLOKOVANÉ** — vlastní podmínkou *„napřed strop: jak často má blitzer po bloku zbývající MA a kam by se stáhl“*. Ta podmínka je **správná** *(pravidlo ze 14.08.: neopravovat, dokud neznáme velikost)*, ale **ten strop 33 dní nikdo nezměřil** ⇒ položka nečekala na rozhodnutí, **čekala na měření, které nikdo nezadal**. ⇒ zadáno jako **M9**. | **SLOUČENO → M-ROUND / M1** |
| **P18** | ⭐⭐ **DOVEDNOSTI JSOU V ENGINU POVINNÉ, ALE V PRAVIDLECH VOLITELNÉ** *(uživatel to psal už dřív; tehdejší odpověď byla „to bude složitější změna" a **nikde se to nezapsalo** — čtvrtý takový případ 14.08.)*.<br>CRP u řady dovedností říká **„may"**, tedy je to **volba kouče**, ne automatika: **Stand Firm** *(„may choose to not be pushed back")* · **Fend** · **Side Step** · **Wrestle** *(„may use Wrestle when he blocks or is blocked")*. Náš engine je vyhodnocuje **natvrdo** — `holdsGround()` rozhodne za Stand Firm, `attWrestle` za Wrestle *(viz P17)*.<br>⛔ **Bez toho nefunguje kombinace Block + Wrestle** *(uživatel 14.08.: „u toho, když máme i Wrestle i Block, to bez té úpravy fungovat nebude")* — a tím padá i balík proti Black Orkovi.<br>⚠️ **Ta tehdejší odpověď byla věcně správná:** udělat z každé dovednosti rozhodovací bod nafoukne akční prostor MCTS a je to projekt, ne úkol.<br>⭐ **ALE P17 na to čekat nemusí.** Nepotřebuje obecnou volitelnost, jen **jedno pravidlo**: použij Wrestle tehdy, když by se „Both Down" jinak vyrušilo — tedy když má obránce Block. To je pár řádků, ne framework.<br>⇒ **Rozděleno: P17 = cílená oprava (teď), P18 = obecná volitelnost (projekt).** | **OTEVŘENO — P18 projekt, P17 hned** |
| P4 | **`CHAIN_SCORE` je aktivní bug** — krok 1 (pass) spálí `passUsedThisTurn`, krok 2 (hand-off) se pak nenabídne ⇒ přihrávka se provede, předání selže, **tah je pryč**. Opravit nebo odstranit. | **PŘÍČINA ODSTRANĚNA `f5998575`** *(ověřeno 18.08.)* — pass a hand-off mají od 17.08. vlastní povolenky (`handOffUsedThisTurn`), takže krok 2 řetězu se **už nenabídne prázdný**. ⚠️ **Zbývá ověřit sám řetěz:** `expandChainScore` (`macro_actions.cpp:1670`) se od té opravy nikdy neproměřil a `CHAIN_SCORE` se v korpusu 3 000 her **nezahrál ani jednou** (jako hand-off, P21) ⇒ stav je **„příčina pryč, účinek nezměřen"**, ne UZAVŘENO. |
| P5 | **Hand-off pro výměnu nosiče** — filtr váží předání cenou přihrávky (33 %), i když by ho provedl jako hand-off (83 %); práh 0,5 zahodí i Runner→Runner (44 %) ⇒ nenabízí se žádné předání. Kritérium: **„nosič je špatný"** (AG≤2 bez Sure Hands a nedoběhne), ne „příjemce je lepší". Patch: `scratchpad/handoff_fix_plan.md` | **ZASTAVENO 18.08. — CHYBÍ KORPUS.** Doktrína napsána (spec **ČÁST 16**), ale ⛔ **`corpus_baseline_20260817_data` exportuje `HAND_OFF` jako `UNKNOWN`**: sběr začal 17.08. v **10:15**, oprava exportu `c943e8b8` je z **11:59** téhož dne (ověřeno: není předkem `5e5ab352`, na kterém korpus běžel). ⇒ **Jakékoli měření hand-offu nad dnešní baseline neplatí.** Podle pravidla 18.08. *(spravit kontroly vždy povýšit před měření)* se P5 nedělá, dokud nevznikne korpus na opraveném exportu. ⚠️ Ostatní dnešní měření to NEzasahuje (σ-tabulka, odsuny, blitzy, rohy stojí na `BLOCK`/`PUSH`/pozicích). ⭐ Co platí i tak: nabídka 10,4/zápas proti **130 zahraným** za 3 000 her (0,043/zápas) = **1 : 240** ⇒ vada je ve VOLBĚ, jen nestojí na nule. *(Předchozí stav: OTEVŘENO — POVÝŠENO uživatelem 14.08.)* |
| P6 | **Zobecnit item 14 na výběr cíle a na pickup** — BLITZ vybírá cíl podle surových kostek (blitzera podle kostek + cesty); PICKUP váží cenu sebrání, ne cestu k míči (připouští 2 GFI = 30 % pád). Nástroj existuje: `estimateApproachFailChance` (`macro_actions.cpp:206`), použitý jen 2× a oba u blitzu. Porušené vlastní pravidlo z 03.08. | **OTEVŘENO** |
| P8 | **Výběr cíle faulu** bere prvního ležícího v pořadí sousedních polí, nehodnotí nic. Přitom Gutter Runner je 4,4× lepší cíl a Thick Skull se nefauluje. | **OTEVŘENO** |  <br>⛔⛔ **STROP SPOČÍTÁN 17.08. — VÝBĚR CÍLE SE DĚLAT NEMÁ** (`diag_foul_choice_20260817.py`, 3 000 her, 11 175 našich faulů). Kód je horší, než se psalo: `macro_actions.cpp:805` po prvním nalezeném cíli **`return`** ⇒ na faulujícího se nabídne **právě jeden** faul a search **nedostane na výběr** (u bloků alternativu vidí a Black Orka si bere v 76,8–83,3 %). **Jenže vybírat skoro není z čeho:** v **74,8 %** faulů leží u faulujícího **jediný** cíl, průměr výběru je **1,32**. Black Ork je mezi dostupnými jen v **12,1 %** faulů a **už teď ho v 87,6 % z nich faulujeme**. ⇒ **Dokonalá oprava přesměruje 168 faulů za 3 000 her = 0,056 na zápas**, z nichž jen část někoho odstraní — a to se ještě dělí článkem 7 (**T5.3**). ⚠️ Netýká se to širší otázky *kdo má faulovat a kam se kvůli tomu postavit*, která zůstává nezměřená a je to jiný úkol. | **ZAMÍTNUTO — strop 0,056 faulu na zápas** |
| T4.1 | Záloha u míče = potenciální nosič (Blitzer AG3 MA5) | **OTEVŘENO** |
| T4.2 | Ověřit bránu přihrávek na korpusu — dopočítané, ne změřené | **OTEVŘENO** |
| T4.3 | Priorita blitzu: zeď kupředu → odmarkovat nosiče → příležitost | **OTEVŘENO** — P2 je jeho zostření |
| P2 | **Doktrína „BÍT TOHO, KDO ŠPINÍ ROH" — ODBLOKOVÁNA a PŘEPSÁNA** (P0.1 + P0.5 + P0.6 uzavřeny 14.08.). Ne „blitz na roh" a ne „bít víc", ale: **(1) priorita BLOKU na pollutera s volným stojícím sousedem** — pokrývá 61 % polluterů, blokovaný polluter je v 72,8 % na zemi a v 92 % přestane roh špinit, neblokovaný špiní dál v 64 %. **(2) Blitz na roh jen jako záloha**, když soused není (39 %) a blitz nepotřebuje nosič. **(3) R4 „tělo bez úkolu" dostane úkol *dojdi k polluterovi / postav se na asistenci*** — 2,14 idle těl/kolo, 94,7 % dosáhne. **(4) NEzvedat obecný počet bloků** kvůli rohům (−4,5σ). ⚠️ **(5) Blokující se vybírá podle GEOMETRIE ODSUNU, ne jen podle dostupnosti** *(uživatel 14.08.)* — těch 61 % počítá, kdo **může** udeřit, ne jestli výsledný odsun **odklidí pollutera od rohu**, a při 27,2 % bloků zůstane stát. ⇒ **Fableho 61 % je horní mez a potřebuje přísnějšího nástupce:** *podíl polluterů, u nichž existuje volné stojící tělo, z jehož pozice aspoň jedno ze tří odsunových polí pollutera od rohu odklidí a nepřiblíží ho k nosiči.* Teprve tohle číslo smí řídit P2. Viz **P9c**. | **OTEVŘENO — ČEKÁ NA P9c** |
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
| P7 | **Sdílený limit pass / hand-off** — CRP má dva nezávislé limity, engine jeden (`passUsedThisTurn`). ⚠️ **Jediný nález, který po opravě hraje PROTI nám** ⇒ měřit zvlášť. | **UZAVŘENO — a fronta to 26 dní nevěděla.** Opraveno **`f5998575`** *(„the hand-off has its own action allowance, and always did", 17.08.)*: `rules_engine.cpp:102` se ptá na vlastní `handOffUsedThisTurn`. ⚠️ **Ověřeno až 18.08. při psaní kapitol** — položka stála OTEVŘENO ještě den po opravě. Táž rodina jako T0.2: **fronta věří svému snímku víc než kódu.** ⇒ Před plánováním podle položky se ověřuje kód, ne řádek. Uzavírá i příčinu **P4**. |
| T5.2 | [6] **Kick-Off Return** — 3 vady | **OTEVŘENO — POVÝŠENO** *(fáze 1 fázového plánu na něm stojí, přestává být okrajové)* |
| T5.1 | [7] Tabulka výkopu — 5 z 11 výsledků vadných; 22 % výkopů zahazuje volné tempo | **ODLOŽENO uživatelem** — **spouštěč:** až se bude řešit fáze 1 / kick-off (tedy spolu s T5.2) |
| T5.3 | Zranění nepřetrvávají přes drive — každý TD staví 11 čerstvých ⇒ attrition čísla měří jen poslední drive | ✅ **UZAVŘENO — BYLO OPRAVENO UŽ 10.08.**, commit `918fc589` *(balík G, casualties survive the drive)*, a kniha to od té doby vedla jako otevřené. **Ověřeno empiricky 17.08., ne podle komentáře:** z **648 hráčů s událostí `CASUALTY` se za 600 her nevrátil na hřiště ANI JEDEN**. Návrat soupisky po TD (8→11) je **legální návrat KO** (CRP: 4+ na kick-offu). ⇒ **Tvrzení o 11 čerstvých po TD NEPLATÍ.** |
| T5.8 | Frenzy druhý blok se v hodnocení příležitostí nikde nemodeluje — týká se Slayerů | **OTEVŘENO** |
| **T5.14** | ⚠️ **MIGHTY BLOW SE PŘIČÍTÁ K OBĚMA HODŮM — ŽIVÁ CHYBA, HRAJE PROTI NÁM.** CRP: *„you only modify **one** of the dice rolls, so if you decide to use Mighty Blow to modify the Armour roll, you may not modify the Injury roll as well."* Engine (`block_handler.cpp:673`) nastaví **oba**: `defCtx.armourModifier += 1; defCtx.injuryModifier += 1;`. Má to být **volba kouče**, ne součet.<br>⭐ **SPECIFIKACE OPRAVY** *(uživatel 14.08.: „na zranění MB může, pokud se nevyužila aktivně na brnění")* — není to „vyhoď `injuryModifier`", je to **rozhodnout, kam ten jediný bonus dát, podle výsledku hodu na brnění**:<br>• brnění prorazí **i bez** `+1` ⇒ **nespotřebovat**, nechat na zranění<br>• neprorazí, ale s `+1` ano ⇒ **spotřebovat na brnění**, na zranění už ne<br>• neprorazí ani s `+1` ⇒ nespotřebováno, nerozhoduje<br>Totéž platí s Claw: `+1` smí posunout 7 na 8, kterou Claw vyžaduje — to je legitimní spotřebování.<br>⚠️ **Naivní varianta „vždycky na brnění" by soupeře oslabila víc, než pravidla chtějí.** Opravou se odebírá jen dvojí započtení, ne bonus sám.<br>**Claw je naopak správně** (`injury.cpp:181`: `armourRoll` už obsahuje modifikátory ⇒ „8+ po modifikacích" sedí) — a právě proto se chyba znásobuje: `+1` legitimně pomáhá dosáhnout osmičky pro Claw **a navíc** neoprávněně zvedá zranění.<br>**Dopad:** MB má v TV1200 **ork** (Blitzer), **human** (Blitzer + Ogre), **wood-elf** (Treeman); **skaven ani trpaslík ani jednoho** ⇒ **chyba jen bije nás**, ve 3 ze 4 matchupů. Táž rodina jako T5.7 Dauntless, jen opačným směrem.<br>⚠️ **OPRAVA NEOSLABUJE CLAW+MB** *(uživatel 14.08.: „u Skaven Blitzera s Block MB Claw je to síla i bez Piling On … my zůstáváme teď u silné kombinace podle 2016")*. Stacking na **brnění** — `+1` z MB pomáhá dosáhnout osmičky, kterou Claw vyžaduje, takže na AV9 stačí **7+** — je podle 2016 **správně a zůstává**. Odebírá se **jen** druhý dip na hod na **zranění** v témž bloku. Pozdější edice nerfly i Claw; **my u 2016 zůstáváme.** <br><br>✅ **UZAVŘENO 25.08. — A UŽ 11 DNÍ BYLO.** Ověřeno v kódu: `injury.cpp:190-217` implementuje **přesně tuhle specifikaci** — `spentOnArmour` utratí `+1` na brnění **jen když ho brnění potřebuje k proražení**, jinak si ho nechá na zranění; skládání s Claw drží *(„+1 smí posunout 7 na 8, kterou Claw vyžaduje")*. **Tři testy** v `test_block_handler.cpp:315-370`: `MightyBlowAndClaw` · `MightyBlowIsKeptForTheInjuryWhenArmourBreaksWithoutIt` · `MightyBlowIsSpentOnArmourOnlyWhenArmourNeedsIt`.<br>⛔⛔ **PROČ TO FRONTA NEVĚDĚLA: oprava přijela ve VEDLEJŠÍM commitu.** `e7d93ed1` (14.08. 14:19) se jmenuje *„feat(diag): count the Dauntless offers"* — název o měření, obsah i pravidlová oprava. ⇒ **Třetí případ téže třídy** *(vedle „UZAVŘENO — a fronta to 26 dní nevěděla")*: **co se nezapíše pod vlastním jménem, to fronta neuvidí, i když je to hotové a otestované.**<br>⭐ **Důsledek pro měření: není co doměřovat.** Oprava je z 14.08., tedy **před** korpusem 18 000 her i před všemi A/B od té doby — všechna ta čísla ji už obsahují.<br>⚠️ **Zbytek T5.14 (volba kouče jako rozhodnutí plánovače) NENÍ otevřený:** pravidla dovolují jen „jeden z hodů", a heuristika „utrať na brnění jen když je potřeba" je optimální za všech okolností *(nespotřebovaný bonus nemá kam jinam jít)*. | **UZAVŘENO 25.08.** — hotovo `e7d93ed1`, 3 testy |
| **T5.15** | **Piling On není implementovaný** — `SkillName::PilingOn` je v `enums.h:115`, ale v celém `engine/src/` se nevyskytuje. Mrtvá hodnota enumu. Dnes neškodí (nikdo v TV1200 ho nemá), ale je to druhá polovina **CLAWPOMB** a ožije s **T5.13**. | **ODLOŽENO** — **SPOUŠTĚČ: spolu s T5.13** |
| **T5.16** | ⭐⭐ **KTEROU EDICI VLASTNĚ MODELUJEME? — nikde to není napsané.** Náš zdroj `rules_crp2016.txt` se sám představuje jako *„BLOOD BOWL **COMPETITION RULES** … Competition Rules pack"*; řetězce `BB2016`, `Death Zone`, `Living Rulebook` v něm **nejsou ani jednou**. ⇒ **Je to CRP/LRB6 a „2016" v názvu je matoucí** — i paměť ho vede jako „CRP/LRB6 (BB2016)", což slučuje dvě různé edice.<br>**Kde se to rozchází** *(uživatel 14.08.)*: v **CRP je Piling On zdarma** (přehodí brnění nebo zranění, hráč jde prone); v **BB2016 stojí týmový reroll**, což CLAWPOMB prakticky zabilo. Kombinace Claw+MB je v obou stejná — na AV9 stačí 7+, protože `+1` z MB pomáhá k osmičce, kterou Claw vyžaduje.<br>⛔ **ROZHODNUTO — a bylo rozhodnuto UŽ DŘÍV.** Uživatel 14.08.: *„já jsem minule už hlásil, že chci pravidla 2016."* **Cílová edice je BB2016.** Rozhodnutí padlo dřív a **nikde se nezapsalo** — paměť vede zdroj jako „CRP/LRB6 (BB2016)", takže se ty dvě edice slily a stáhl se CRP. ⇒ **Všechny pravidlové audity od 07.08. běžely proti ŠPATNÉ edici.** Táž rodina chyby jako ztracených ~20 položek fronty: rozhodnutí bez zápisu. | **UZAVŘENO rozhodnutím** → viz **T5.17** |
| **T5.17** | ⭐⭐⭐ **OPATŘIT TEXT BB2016 A PŘEAUDITOVAT ROZDÍL.** Cílová edice je **BB2016** (T5.16), zdroj `rules_crp2016.txt` je **CRP/LRB6**. Kroky:<br>**(1)** sehnat autoritativní text BB2016 (Death Zone Season 1/2 + BB2016 rulebook) týmž postupem jako 07.08. — stáhnout PDF, rozebrat `pypdf`, grepovat; **nespoléhat na AI, ta edice míchá** (to je přesně, co se stalo);<br>**(2)** přejmenovat současný soubor na `rules_crp_lrb6.txt`, ať název nelže;<br>**(3)** projít text na změny **uvnitř zápasu**, které v seznamu nejsou;<br>**(4)** ověřit body 1–5 z `evidence/rules_edition_crp_vs_bb2016.md`.<br><br>⭐ **PŘEŠKÁLOVÁNO 14.08. — poplach z velké části odvolán.** Uživatel dodal celý přehled rozdílů (zapsán do `evidence/rules_edition_crp_vs_bb2016.md`; **posílal ho už dříve a nezapsal se** — třetí případ téhož dne). Po triáži proti tomu, co engine modeluje: **v zápase se edice skoro neliší.** Drtivá většina změn BB2016 je **liga a ekonomika po zápase** (MVP, Spiralling Expenses, Expensive Mistakes, Redrafting, Wizards, karty, měřítko 32 mm) — z toho **nemodelujeme nic** (ověřeno grepem: `inducement`, `SPP`, `MVP`, `treasury` nejsou v `engine/src/`).<br>⇒ **Všech ~15 dosud auditovaných pravidel je mezi edicemi beze změny.** Sahá na nás jen: **Piling On** (T5.15, odloženo) · **Argue the Call** (T5.18, nové) · Weeping Dagger a Human Catcher 60k (jen rostery, T5.13) · **Timmm-ber! wood-elfí Treemani NEDOSTALI** ⇒ náš je správně bez ní.<br>⚠️ Zbývá jediné skutečné riziko: **seznam je z AI a jeho úplnost není zaručená** ⇒ krok (3).<br>*(Fable na to netřeba — uživatel 14.08.)* | **OTEVŘENO — STŘEDNÍ**, malá ověřovací práce |
| **T5.18** | **Argue the Call není implementované.** BB2016: kouč smí zkusit zvrátit vyloučení za faul / Secret Weapon — `1` = kouč vykázán a −1 na Brilliant Coaching · `2–5` = platí · `6` = hráč jen na střídačku (tah **stále končí turnoverem**). Vyloučení za faul **modelujeme** (`foul_handler.cpp:64`, dvojice → ejected, Sneaky Git brání), odvolání ne. ⇒ Dnes jsme **přísnější**, než 2016 káže — ztrácíme hráče, které bychom na 6 udrželi. Týká se obou stran. | **OTEVŘENO — NÍZKÁ** *(faulujeme málo; P8 „výběr cíle faulu" jde napřed)* |
| **T5.20** | ⭐ **ČTYŘI MRTVÉ DOVEDNOSTI BEZ VLASTNÍKA — dosud jen odrážka, ne úkol** *(uživatel 25.08.: „zapiš do fronty")*. `check_rules_citations.py` (běh 25.08.): **5 hodnot v enumu, které v `engine/src/` nepoužívá nikdo** — `PilingOn` *(má vlastníka: **T5.15** se spouštěčem)* a **bez vlastníka**: **`Leader`** · `DumpOff` · `Animosity` · `PassBlock`.<br>⭐⭐ **`Leader` není kosmetika — je to CHYBĚJÍCÍ REROLL EKONOMIKA.** BB2016: tým s Leaderem dostává **rerollovou známku navíc** na začátku zápasu i o poločase, a platí, dokud je nositel na hřišti *(i ležící nebo omráčený)*. ⇒ dokud to nemáme, **každý roster s Leaderem hraje s méně rerolly, než pravidla dávají**.<br>⚠️ **Dnes to nic nestojí:** v pěti korpusových sestavách TV1200 nemá žádnou z těch čtyř nikdo *(ověřeno grepem `roster.cpp`)* ⇒ **latentní, ne živé**.<br>⭐ **A pravidlo z 24.08. platí i tady: „latentní" = „odložené", ne „neškodné".** Každá z nich ožije **v okamžiku, kdy se sáhne na rostery** ⇒ ⛔ **SPOUŠTĚČ: přestavba rosterů (T5.13)** — projít tenhle seznam **před** ní, ne po ní.<br>⏰ **A druhá polovina téhož výpisu: 7 implementovaných dovedností BEZ CITACE** *(`Regeneration` · `DisturbingPresence` · `ExtraArms` · `KickOffReturn` · `DivingCatch` · `NurglesRot` · `MultipleBlock`)* — mechanika běží, ale **nikdo ji proti textu nepřečetl**. ⚠️ `MultipleBlock` je zároveň **A6** v M-ROUND *(chybí makro)* ⇒ tam se ověří i citace. | **ODLOŽENO — latentní** · **SPOUŠTĚČ: přestavba rosterů (T5.13)** |
| **T5.21** | ⛔ **68 CITACÍ V KÓDU UKAZUJE NA ŠPATNOU EDICI** *(uživatel 25.08.: „zapiš do fronty")*. Nález z 20.08., **pořád živý** — přeměřeno 25.08. `check_rules_citations.py`: **217 citací celkem** *(src 124, testy 93)*, z toho **68 odkazuje na CRP/LRB6** místo BB2016: **src 46** — `block_handler` 12 · `pass_handler` 8 · `injury` 7 · `game_simulator` 4 · `ball_handler` 2 · `helpers` 2 · `macro_actions` 2 · `rules_engine` 2 · po jedné `big_guy_handler`, `bomb_handler`, `foul_handler`, `kickoff_handler`, `move_handler`, `ttm_handler`, `turn_handler`; **testy 22**.<br>✅ **Citace mimo rozsah textu: 0** ⇒ **není to rozbité, je to nepřečtené.**<br>⭐ **Není to samo o sobě chyba chování** — je to **68 míst, kde se chování obhajuje edicí, kterou nehrajeme**, a nikdo je proti BB2016 nezkontroloval. *(A 24.08. se ukázalo, že přesně takhle vznikaly vady: kód se psal ze sekundárních zdrojů — viz `project_bloodbowl_vampire_ai_text_refuted_20260824`.)*<br>⭐⭐ **Je to PROVEDITELNÁ ČÁST kroku (3) z T5.17.** T5.17 zní „projít text BB2016 na změny uvnitř zápasu, které v seznamu nejsou" — což je hledání **neznámého v 9 519 řádcích**. Tohle je jeho **ohraničený protějšek**: 68 konkrétních míst, každé s číslem řádku, každé s odpovědí ANO/NE. ⇒ **udělat T5.21 PŘED T5.17**, protože zúží, co v textu ještě hledat.<br>⚠️ **Nespravovat to přepsáním čísel řádků na BB2016** — to by jen zamaskovalo, že se obsah neověřil. Každá citace se **přečte v BB2016** a buď potvrdí *(pak přepsat)*, nebo **založí nález**. <br><br>✅ **HOTOVO 25.08. — tři agenti na Opusu, všech 68 přečteno.** `evidence/fable_t521_batch{A,B,C}_*_20260825.md`. **A = 65 · B = 1 · C = 2.**<br>⭐⭐⭐ **HLAVNÍ ZJIŠTĚNÍ: ÚLOHA BYLA POLOŽENÁ VEDLE — a to je dobrá zpráva.** Náš text říká v hlavičce (`rules_bb2016.txt` ř. **28-29**): *„These rules are a collection of all new Blood Bowl rules **merged together with previous Living Rulebook version 6**."* ⇒ **kde BB2016 pravidlo nezměnil, je v něm doslova reprodukovaný LRB6**, takže „citace míří na CRP" je **problém ŠTÍTKU, ne obsahu**. ⇒ ⭐ **Pro T5.17 to zužuje hledání:** rozchody edic se mají hledat tam, kde BB2016 něco **PŘIDAL** (Death Zone S1/S2, kickoff tabulky, inducements), ne v základních mechanikách.<br>⭐⭐ **A SKUTEČNÝ VÝNOS LEŽÍ MIMO CITACE.** Nulové B neznamená „vše v pořádku" — agenti cestou našli **šest rozporů s BB2016, a ani jeden nebyl u citovaného řádku**: **T5.23** Stunty · **T5.24** vhazování · **T5.25** Safe Throw · **T5.26** míč z `(-1,-1)` · **T5.27** Turn marker · **T5.28** mrtvý Decay. Plus tři položky kickoff tabulky *(Riot, Cheering Fans, High Kick)*, které padají do **F13**.<br>⇒ ⭐⭐⭐ **METODICKÝ ZÁVĚR: audit citací našel víc VEDLE citací než V nich.** Kontrola byla užitečná ne tím, co ověřila, ale tím, že donutila přečíst text kolem.<br>⛔⛔ **VAROVÁNÍ O NÁSTROJI — tabulky v `rules_bb2016.txt` mají OBRÁCENÁ ZNAMÉNKA.** DODGING MODIFIERS (ř. 502-505, 597-600) i PICK-UP MODIFIERS (ř. 460-462) tisknou *„Per opposing tackle zone … **+1**"*, zatímco próza o 15 řádků výš počítá **−1** (*„has to subtract 2 because there are two Orc tackle zones"*, ř. 578-585). Je to **vada extrakce PDF**. ⇒ **Tabulky v tom souboru se MUSÍ ověřovat prózou**; kdo cituje jen tabulku, přečte pravidlo s obráceným znaménkem. | **UZAVŘENO 25.08.** → výnos v T5.23-T5.28 |
| **T5.22** | ⚠️ **DIRTY PLAYER UTRÁCÍ SVŮJ `+1` VŽDYCKY NA BRNĚNÍ** *(nalezeno 25.08. při uzavírání T5.14)*. BB2016 ř. **8044-8049** dává Dirty Playerovi **doslova tutéž větu** jako Mighty Blowovi: *„you may only modify **one** of the dice rolls."* `foul_handler.cpp:26-29` přičte `+1` do `assistMod`, tedy **do hodu na brnění**, a na zranění nejde nikdy *(`ctx.armourModifier = 0`, „already applied to armor roll")*.<br>✅ **NENÍ to porušení pravidel** — utratit ho na brnění je legální volba. ⛔ **Je to ta „naivní varianta", kterou T5.14 výslovně zamítlo**: `+1` se spálí i tehdy, když brnění prorazí i bez něj, místo aby zvedl zranění. ⇒ **necháváme hodnotu ležet**, symetricky k tomu, co jsme si u Mighty Blow vzali.<br>⭐ **Oprava je hotová jinde a dá se převzít:** `injury.cpp:190-217` (`spentOnArmour`) dělá přesně tohle rozhodnutí — u faulu ale hod na brnění vzniká **v `foul_handler` samostatně** *(2D6 + assisty, kvůli kontrole dublů)*, takže to není přesun jednoho řádku.<br>⚠️ **Dnes to nic nestojí:** `DirtyPlayer` má v `roster.cpp:88` jediný nositel — **trpasličí Deathroller** *(Secret Weapon, jen základní roster)*; v žádné z pěti sestav TV1200 není. **A až ožije, hraje v NÁŠ prospěch** — proto to nespěchá.<br>⇒ ⭐ **A je to argument pro P8** *(„výběr cíle faulu")*: než se řeší, komu `+1` dát, má smysl vědět, koho vůbec faulovat. | **ODLOŽENO — latentní, hraje pro nás** · **SPOUŠTĚČ: spolu s P8, nebo s přestavbou rosterů (T5.13)** |
| **T5.23** | ⭐⭐ **STUNTY JE „+1 KE ZRANĚNÍ" MÍSTO MAPOVÁNÍ 7/9** *(T5.21, 25.08.; našly nezávisle DVĚ dávky auditu)*. `injury.cpp:46-49` dělá `injuryRoll += 1` a **nemá u sebe žádnou citaci**. BB2016 ř. **8534-8536** *(a CRP shodně)*: hráč se Stunty *„treats a roll of **7 and 9** … as a **KO d and Badly Hurt** result respectively"*.<br>**Dva reálné rozdíly:** (1) na přirozené **9** u nás vyleze 10 ⇒ plný hod na Casualty tabulku, kde je **1/6 DEAD**; pravidla dávají **Badly Hurt** (Reserves) a umřít nelze. (2) `+1` posouvá hod **dřív**, než se Thick Skull ptá na „modifikovanou 8" ⇒ ptá se na špatné číslo, a to v obou směrech.<br>⚠️ **Latentní:** `roster.cpp:499` říká doslova *„Orc TV~1280: goblins removed"*, žádná TV1200 sestava Stunty nemá. ⛔ Ale `test_injury.cpp:244` dnešní chování **přišpendluje**, i když u něj žádná citace není. | **ODLOŽENO — latentní** · **SPOUŠTĚČ: přestavba rosterů (T5.13)**, nebo dřív spolu s T5.24 |
| **T5.24** | ⛔⛔ **VHAZOVANÝ MÍČ LETÍ O POLE DÁL, NEŽ MÁ — ŽIVÁ VADA NA VŠECH CESTÁCH** *(T5.21, 25.08.)*. `ball_handler.cpp:218` posune míč **plných 2D6** polí od posledního pole v hřišti; BB2016 to ve vlastním FAQ ř. **9307-9314** rozhoduje opačně: *„The square with the Blood Bowl logo over it **counts as the first square** of the ball s movement"* ⇒ má být **2D6 − 1**.<br>**Hraje na všem, co vede do `resolveThrowIn`:** odsun mimo hřiště, rozptyl přihrávky i výkopu, odraz ven, opakované vhazování. **Symetrické** — bije obě strany.<br>⛔⛔ **A JE TO PAST NA TOHO, KDO TO BUDE OPRAVOVAT: dva testy dnešní chování PŘIŠPENDLUJÍ** *(`test_ball_handler.cpp:204` asertuje `(11,5)`, má být `(11,4)`; `:226` totéž + vymyšlené D3 pro roh)*, a **oprava shodí SEDM testů v jednom souboru** *(`:206, :216, :228, :244, :259, :272, :289` si půjčují tytéž kostky)*. **Bez tohohle zápisu se to přečte jako regrese.**<br>⚠️ **Ohraničení poctivě:** doloženo je *„kód nehraje podle BB2016"*. Že se **edice rozcházejí**, doložit neumíme — CRP u vhazování jen odkazuje „see page 3" a LRB6 FAQ nemáme. | **OTEVŘENO — ŽIVÉ, STŘEDNÍ**; opravovat **spolu se sedmi testy**, ne po jednom |
| **T5.25** | ⛔ **SAFE THROW JE VEDLE V OBOU PŮLKÁCH** *(T5.21, 25.08.)*. BB2016 ř. **8434-8441**. `pass_handler.cpp:116-124` hází proti `intTarget`, což je **cíl INTERCEPTORA** *(jeho AG, jeho −2, jeho tackle zóny)*; pravidlo chce **nemodifikovaný AG hod HÁZEČE**. A **druhá věta skillu** *(fumble na jiný než přirozený 1 se jako fumble nepočítá)* v kódu **není vůbec**.<br>⚠️ **Pomáhá to SOUPEŘŮM** — Safe Throw je passingová dovednost, trpaslík ji nemá. Táž rodina jako F6/F7/F8. <br>⛔ **OPRAVA VLASTNÍHO ZÁPISU (25.08., tentýž den):** označil jsem to nejdřív jako **ŽIVÉ**. **Není.** `grep SafeThrow engine/src/` má zásah **jen v `pass_handler.cpp`** — **v žádném rosteru tu dovednost nemá nikdo**, ani v základních, ani v TV1200. ⇒ **latentní**, a hraje teprve po přestavbě rosterů. *(Táž past, na kterou jsem dnes upozorňoval u T5.14: stav se ověřuje v kódu, ne ve vlastním předchozím odhadu.)*<br>⭐ **PRIORITA UVNITŘ ODLOŽENÝCH: STŘEDNÍ** *(uživatel 25.08.: „ať má svou prioritu ve frontě, ať se nezapomene")*. **Proč ne nízká, ač je latentní:** až rostery ožijí, Safe Throw sedí na **obou** koncích passovací hry naráz — ruší zachycení **a** brání turnoveru z fumblu *(ř. 8441-8443: „and the team does not suffer a turnover")*. To není okrajová dovednost, to je **pojistka celé passovací strategie soupeře**.<br>⭐ **A půlka práce je hotová:** výjimka Very Long Legs *(ř. 8655-8659, „Safe Throw skill may not be used to affect any Interception rolls made by this player")* **implementovaná JE** — `pass_handler.cpp:117`. ⚠️ **VLL má 16 nositelů v rosterech**, takže tahle půlka páru **hraje už dnes**. ⇒ opravovat se bude jen samotný Safe Throw, ne ta výjimka.<br>⭐ **Ověřeno DVĚMA nezávislými zdroji 25.08.** — agentem čtoucím `rules_bb2016.txt` a textem, který dodal uživatel. ⛔ **A je to opačný případ než tři z 24.08.:** tam AI text reprodukoval NÁŠ starý kód; tady našemu kódu **odporuje** ⇒ shoda dvou zdrojů je tentokrát skutečné potvrzení, ne ozvěna. | **ODLOŽENO — latentní, priorita STŘEDNÍ** · **SPOUŠTĚČ: přestavba rosterů (T5.13)**; věcně patří k odloženému modelu přihrávek (T5.19) |
| **T5.26** | ⛔ **VYLOUČENÝ NOSIČ PUSTÍ MÍČ Z `(-1,-1)`** *(T5.21, 25.08.)*. `foul_handler.cpp:72-74` vynuluje pozici hráče **PŘED** voláním `handleBallOnPlayerDown` ⇒ míč se odráží z pole mimo hřiště. `(-1,-1)` + rozptyl `(+1,+1)` = **`(0,0)`, což je platné pole** ⇒ míč se teleportuje do rohu. BB2016 ř. **1883-1884** chce odraz *„from the square he was standing in when sent off"*.<br>⭐⭐ **Třída „NEBEZPEČÍ JE MIMO NABÍDKU"** — přesně to, co 24.08. dostalo vlastní zápis: resolvery invariant dědí jen z nabídky, a tohle míč pokládá mimo ni. ⭐ **A shodou okolností je to přesně to, co hledá noční kontrola invariantů** *(`MIC_NIKDE` / `drzeny_mic_mimo_hriste`)* ⇒ **noc 25.→26.08. řekne, jak často to nastává.** | **OTEVŘENO — ŽIVÉ, oprava je přesun dvou řádků**; ⏰ počkat na číslo z noci |
| **T5.27** | ⚠️ **TD V SOUPEŘOVĚ KOLE NEPOSUNE TURN MARKER** *(T5.21, 25.08.)*. BB2016 ř. **1003-1005**: skórující tým *„must move their Turn marker one space along the Turn track"*. Engine TD uzná (`action_resolver.cpp:242`), marker neposune nikde ⇒ **skórující tým dostane kolo navíc**, na které nemá nárok. Sekce má dvě věty, plníme jednu. | **OTEVŘENO — ŽIVÉ, ale vzácné** *(TD v cizím kole)*; ověřit četnost na korpusu z noci |
| **T5.28** | ⚠️ **`ctx.hasDecay` SE NASTAVUJE NA TŘECH MÍSTECH A NIKDE NEČTE** *(T5.21, 25.08.)*. Citace u Decay **sedí** (verdikt A), ale zdůvodnění pod ní zestárlo **ve stejném balíku, ve kterém se psalo**: komentář tvrdí *„this engine … has no Casualty table"*, jenže `rollCasualty()` je na `injury.cpp:11` a volá se z ř. 85.<br>⭐ **Metodicky je to zajímavější než dopadem:** je to komentář, který **lhal už v okamžiku commitu**. Latentní *(Decay nemá nikdo v TV1200)*. | **ODLOŽENO — latentní** · **SPOUŠTĚČ: přestavba rosterů (T5.13)** |
| **T5.29 = M1c** | ⛔⛔ **FOLLOW-UP JE U NÁS POVINNÝ, PRAVIDLA Z NĚJ DĚLAJÍ VOLBU** *(uživatel 25.08.: „zkontroluj, že odstraníš chybu s mandatory follow — to blitzerovi ulehčí ten následný pohyb")*. **Ověřeno:** `resolveBlock` má parametr `noFollowUp`, ale **defaultuje na `false` a ŽÁDNÉ volací místo ho nikdy nepředá** *(`action_resolver.cpp:87` i `:158`)* ⇒ útočník se posune do uvolněného pole **vždy**.<br>**BB2016 ř. 608-611:** *„A player who has made a block **is allowed** to make a special follow up move and occupy a square vacated by the player that they have pushed back. The player s **coach must decide** whether to follow up before any other dice rolls are made."* ⇒ je to **rozhodnutí kouče, ne důsledek**.<br>⭐⭐ **A JE TO TŘETÍ PŮLKA M1/N10, ne samostatná drobnost.** Hit-and-run potřebuje **oboje**: smět se po bloku hnout *(hotovo, `14c172cd`)* **a smět odmítnout follow-up**. Dnes blitzujícího po pushi **vtáhne** do uvolněného pole — tedy hlouběji mezi soupeře — a teprve odtud se má stahovat. **Vynucený follow-up tu opravu z velké části sní.**<br>⚠️ **Výjimky, které ZŮSTÁVAJÍ povinné** *(nesmí se smazat spolu s tím)*: **Frenzy** ř. **8138** *(„must always follow up if they can")* a ř. **8142** pro druhý blok · ř. **7825** *(„The player must follow up if they push back")* · ⛔ **Multiple Block** ř. **8302** *(„cannot follow up either block")* · **Fend** ř. **8115** *(„Opposing players may not follow-up blocks made against this player")* — ten poslední je v kódu ošetřený (`fendPrevents`).<br>⛔⛔ **JAK TO NEIMPLEMENTOVAT: heuristikou v resolveru.** *„Nenásleduj, když by tě to dalo do víc zón"* je **výsledek, ne pravidlo** — a uživatelovo vlastní pravidlo říká implementovat pravidlo *(viz `feedback_implement_the_rule_not_the_outcome`)*. Volba patří **plánovači**: follow-up musí být **parametr rozhodnutí**, které makro vydá, ne dopočet uvnitř `resolveBlock`.<br>⚠️ **A pravidlo má ještě jednu větu, kterou stojí za to vytěžit:** follow-up je **ZDARMA a ignoruje tackle zóny** *(„This move is free, and the player can ignore enemy tackle zones… he does not have to dodge to enter the square")* ⇒ **následovat je často levnější než dojít**; ta volba není jednosměrná a plánovač ji musí umět zvážit oběma směry. <br><br>✅ **HOTOVO 25.08. — a moje „⛔ neimplementovat heuristikou" bylo VEDLE.** Uživatel: *„psal jsi, že udělat ze všech skillů volbu je komplikované — ale tenhle konkrétní případ by samostatně mohl vyjít s levnějším řešením."* **Má pravdu, a doklad je v repozitáři:** `d0093607` (24.08.) udělal **Block+Wrestle volbou přesně tak** — rozhodovacím pravidlem v resolveru, sepsaným i s důvody. Zakázal jsem tedy vzor, který jsme před dvěma dny sami zavedli.<br>⭐ **A follow-up je LEVNĚJŠÍ případ než Wrestle:** volba je **binární a lokální** *(jedno pole)*, follow-up je **zdarma a nedodgeuje se**, takže se nemusí předpovídat nic o budoucnosti — stačí porovnat **dvě pole, která už na desce jsou**. Vlastní vrstvu rozhodování to nepotřebuje.<br>**Implementace:** `wantsFollowUp()` v `block_handler.cpp`, volaná na **obou** místech *(`:818` i `:877`)*. **Default je NÁSLEDOVAT** — pole zdarma je obvykle výhoda. Odmítne se jen když: **je to blitz** · blitzující **stojí a má ještě MA** · a uvolněné pole má **víc soupeřových zón** než to, kde stojí. **Povinné zůstává:** Frenzy *(ř. 8138)* a Ball & Chain *(ř. 7825)*; Fend a Stand Firm řeší stávající `fendPrevents` / `defVacated`.<br>**628/628 zelených**, z toho dva nové testy na samotnou volbu; Frenzy hlídá už existující `BlitzFrenzySecondBlockDeniedWithoutMovement` *(asertuje follow-up na (11,7))*.<br>⛔⛔ **CO TÍM NENÍ VYŘEŠENO — a uživatel 25.08. řekl proč: „decide before roll je důležité PRO LIDI."**<br>&nbsp;&nbsp;**(a) Webová PHP hra follow-up NENABÍZÍ VŮBEC.** `src/Engine/Action/BlockHandler.php:937` ho provede automaticky; jediné výjimky jsou Multiple Block *(`$noFollowUp`)* a Fend. ⇒ **člověk u našeho stolu se na to nikdy nezeptá.** Moje oprava v C++ enginu s tím **nedělá nic** — je to samostatná vada v tom, co lidi skutečně hrají, a pro zkušeného kouče **viditelná**.<br>&nbsp;&nbsp;**(b) POŘADÍ: ř. 608-611 — „The player s coach must decide whether to follow up BEFORE ANY OTHER DICE ROLLS ARE MADE."** My rozhodujeme **až po hodu na zbroj**. U AI to dnes nevadí *(rozhodnutí čte jen pozice)*, ale **u člověka je to informační výhoda, kterou mít nemá**: viděl by, jestli obránce vstane, jestli je KO, a teprve podle toho by se rozhodoval, kam se postaví.<br>&nbsp;&nbsp;⚠️ **A není to boolean, je to ROZDĚLENÍ AKCE NA DVA KROKY.** Dnes PHP vyřeší blok, odsun, zbroj i zranění v jednom volání. Nabídnout volbu ve správnou chvíli znamená přerušit ji uprostřed a počkat na kouče — tedy zásah do **toku hry a UI**, ne do jedné podmínky.<br>&nbsp;&nbsp;⇒ **Vyčleněno jako `T5.30` (webová hra).** ⚠️ Napsal jsem k tomu, že je to „vyšší priorita než parita `hasActed`" — **uživatel 25.08. to přebil: „zapiš to na později, nepřidávej tomu prioritu."** Zápis to má, prioritu ne. | **HOTOVO 25.08.** — třetí půlka M1/N10 |
| **M1/N10 — stav 25.08.** | ✅ **HOTOVÉ VE VŠECH TŘECH PŮLKÁCH, na větvi `m1-n10-blitz-continuation`, 628/628:** *(1)* blitz nechává aktivaci otevřenou `14c172cd` · *(2)* blitzující dostane kam jít `dc4afb68` · *(3)* follow-up je volba `a280af3c`.<br>⛔ **VĚDOMĚ NEMERGOVÁNO PŘED NOCÍ 25.→26.08.** *(uživatel: „nemergovat před nocí — přesně ať se nemíchá víc věcí do jedné kontroly")*. Noc měří **B1/P35**; kdyby v ní jela i nezměřená změna blitzu, nešlo by pak říct, co z čeho. ⇒ **není to skluz, je to rozhodnutí** — a je to táž zásada, kterou máme zapsanou jako `feedback_one_change_one_verification_in_order`.<br>⏰ **CO ZBÝVÁ, NEŽ SE TO DÁ ZMĚŘIT: RAMENO.** Stejně jako `setBlitzLandingArm` u P35 potřebuje M1/N10 **vypnutelný přepínač s default OFF a čítač**, jinak nejde udělat párové A/B ani nulový test. Přepínač musí zakrýt **všechny tři půlky naráz** — jinak by se měřila jejich směs. | **ČEKÁ NA RAMENO**, pak vlastní noc |
| **T5.30** *(webová hra)* | **FOLLOW-UP V PHP HŘE: nenabízí se, a kdyby se nabízel, byl by ve špatnou chvíli** *(zapsáno 25.08.)*.<br>**(a)** `src/Engine/Action/BlockHandler.php:937` provede follow-up **automaticky**; výjimka jen Multiple Block *(`$noFollowUp`)* a Fend ⇒ **kouč-člověk není dotázán nikdy**. Enginová oprava `a280af3c` je v C++ a na PHP **nesahá**.<br>**(b)** BB2016 ř. **608-611**: rozhodnout se má **„before any other dice rolls are made"**. U AI to nevadí *(rozhodnutí čte jen pozice)*; **u člověka je to informace navíc** — viděl by, jestli obránce leží, je KO nebo je pryč, a teprve podle toho volil.<br>⚠️ **Není to boolean.** PHP dnes vyřeší blok, odsun, zbroj i zranění **v jednom volání**; zeptat se ve správnou chvíli znamená akci **přerušit a počkat na kouče** ⇒ zásah do toku hry a UI.<br>⛔ **BEZ PRIORITY — rozhodnutí uživatele 25.08.:** *„zapiš to na později, nepřidávej tomu prioritu."* ⇒ leží tu, dokud si ho někdo nevyzvedne; **nemá spouštěč a nemá předbíhat** nic z okruhů PRAVIDLA / POHYB / KLEC.<br>*(Příbuzné: `project_bloodbowl_php_hasacted_parity_20260716`, rovněž nízká.)* | **ODLOŽENO — bez priority, na později** |
| **T5.31 = LEAP-EV** | ⛔⛔ **SKOK SE V LOGU NEDÁ ODLIŠIT OD DODGE — a to BLOKUJE MĚŘENÍ** *(nález Fable 25.08., **ověřeno v kódu**)*. `resolveLeap` emituje `GameEvent::Type::DODGE` *(`move_handler.cpp:283`)* ⇒ skok na jedno pole je v korpusu k nerozeznání od úniku. **Dokud to platí, nejde spočítat ani „kolikrát se skočilo"**, natož rozpad podle výsledku.<br>⭐ **Je to APARÁTOVÁ vada, ne herní** ⇒ platí `feedback_fix_checks_before_measuring`: **opravit DŘÍV, než se Leap pustí do A/B**, jinak se měření zaplatí dvakrát. Oprava: přidat rozlišitelný zápis *(`SKILL_USED` s `Leap`)*. | **OTEVŘENO — ⏰ PREREKVIZITA měření Leapu** |
| **T5.32** | ⚠️ **`leapUsedThisTurn` se propálí PŘED validací** *(nález Fable 25.08., **ověřeno**)*. `action_resolver.cpp:192-193` nastaví příznak a **teprve pak** volá `resolveLeap`, který smí selhat na neplatném cíli ⇒ hráč o skok **přijde, aniž skočil**. BB2016 ř. 8283 dává jeden skok za kolo, ne jeden pokus. Hygiena, dnes latentní *(makro skok neemituje)*, ale **ožije v okamžiku, kdy Leap zapojíme** — táž rodina jako „latentní = odložené". | **OTEVŘENO — drobná, spolu s Leapem** |
| T5.4 | [13] M1 přeběhnout — smazat falešný `M1_DONE`, přestavět `diag_m1` | **OTEVŘENO** |
| T5.5 | O1 **kopat, nebo přijímat** — volbu vůbec nemodelujeme; potenciálně větší páka než cokoli uvnitř kola | ✅ **UZAVŘENO 24.08.** Nebyla to jen nemodelovaná volba — **los se neházel vůbec**: `openingKickingTeam` byla konstanta `AWAY`, takže domácí zahajovali 1. půli a hosté 2. půli v **18 000 z 18 000** her křížového korpusu. Změřený dopad v **zrcadlových** utkáních (stejná rasa na obou stranách ⇒ rozdíl má být šum): host vyhrával o **+4,87σ** (human), **+4,68σ** (orc), **+2,84σ** (skaven), **+2,61σ** (wood-elf); ⭐ **trpaslík jediný imunní** (−0,40σ) — poslední drive půle neumí proměnit. Opraveno `df5960e2` (los, BB2016 ř. 304-307) + `a78470e7` (volba je **rasová**: křehcí/rychlí volí ÚTOK, ostatní včetně trpaslíka OBRANU — bbtactics + 2-1 grind). ⚠️ **Zbývá jedno doměření**, které korpus rozlišit neumí: je výhoda ve **druhé půli**, nebo v tom, že **soupeř začíná**? |
| T5.6 | O7 Underworld | **OTEVŘENO** |
| **T5.13** | **Přestavba testovacích rosterů — OTT skaven** *(uživatel 14.08.)*. Dnešní TV1200 skaven má GR s Dodge+Sure Feet (**11 polí/kolo**) a žádného Rat Ogra. Jednokolová hrozba potřebuje **`+MA` + Sprint**: MA10 + 3 GFI = **13 polí**. ⭐ **Nepotřebuje ani jeden double** — Sprint i Sure Feet jsou Agility a GR má A v normálním přístupu (CRP: `Gutter Runners 80,000 9 2 4 7 Dodge · GA · SPM`), `+MA` je zvýšení statu. ⇒ **Není to exotický roster, je to nejběžnější cesta vývoje GR.**<br>Rat Ogra **ne**: 160k, mutace jen na double, a bash-skaven se překrývá s orkem, kterého už měříme.<br>⭐⭐ **TŘETÍ a podle rozboru NEJNEBEZPEČNĚJŠÍ varianta: `Gutter Runner + Wrestle`** *(uživatel 14.08.)*. Dnešní skaven má **nástroj bez dosahu a dosah bez nástroje**: Lineman +Wrestle (MA7) ke kleci nedojde, Gutter Runner (MA9) dojde vždycky, ale ST2 proti našemu ST3 je do kopce a kostku vybíráme my. **Wrestle na GR ty dvě věci spojí** — a obchází sílu úplně: nepotřebuje ST4, **neruší ho Sure Hands** (to je jen proti Strip Ballu) a **nechrání před ním Block** (`block_handler.cpp:492`: na „Both Down" jdou oba k zemi, míč padá, a **není to jejich turnover**). Stačí mu s asistencemi na jednu kostku. Podrobně `evidence/matchup_asymmetry_20260814.md`.<br>⭐ **Čtvrtá varianta — jiný KANÁL, ne jiný nástroj: `Gutter Runner + Nerves of Steel`** *(uživatel 14.08.)*. Mechanika je **implementovaná a důkladně** — zohledňuje se na všech čtyřech místech, kde ji CRP zmiňuje: chytání (`helpers.cpp:146`), přihrávka a intercept (`pass_handler.cpp:64, 83, 250`), bomby, Throw Team-Mate. V rosterech je ale **jen na Pro Elf Catcherovi** (`roster.cpp:377`); **skavení GR ji nemá ani v základu, ani v TV1200**.<br>⚠️ **Je to na DOUBLE** — Nerves of Steel je *Passing*, a GR má normální přístup jen ke **G+A** (CRP: `Gutter Runners … GA · SPM`). Tedy vzácnější build než OTT Runner (Sprint i Sure Feet jsou Agility = obyčejný hod).<br>⭐ **Proč je nebezpečná jinak než ostatní tři:** není o odebrání míče, je o tom, že **nemusí projít naší zdí** — GR s NoS chytá v našich tackle zónách bez postihu, takže si míč **přihrají za klec** místo aby ho protlačili skrz. **Celá naše doktrína (klec, rohy, REACH0, značkování) řeší kontakt a proti přihrávce nedělá nic.**<br>⇒ **Samostatná otevřená otázka, kterou dnes neumíme zodpovědět:** jak často nám kdokoli přihrává přes klec a co s tím děláme? **Pasovací hrozba není v žádné z našich kontrol.**<br>⭐ **Druhá varianta: Blitzer `Block + Mighty Blow + Claw`** *(uživatel 14.08.: „je to síla i bez Piling On")*. Proti našemu AV9 stačí **7+** na proražení. Dnešní skavení Blitzeři mají Guard a Strip Ball+Tackle, tedy **žádnou** bash hrozbu. ⇒ **Dvě varianty skavena k otestování: OTT Gutter Runner (rychlost) a bash Blitzer (attrition).** Claw je Mutation ⇒ **na double**, MB je Strength ⇒ Blitzer má S v normálním přístupu.<br>Engine je připravený — `Sprint` funguje (`pathfinder.cpp:34,113` dává `maxGfi=3`), změna je **jeden řádek v rosteru**.<br>⚠️ **Proč to nejde teď:** změna soupeře rozbije srovnatelnost s korpusem 3000 her i se všemi dosavadními A/B.<br>⛔⛔ **VŠECHNY TŘI ÚPRAVY MÍŘÍ JEDNÍM SMĚREM** *(uživatel 14.08.: „všechny tři moje dnešní úpravy skavenů oslabují trpaslíky")* — OTT Runner, bash Blitzer i CLAWPOMB dělají skavena silnějším proti nám. ⇒ **Jejich přidáním spadne chess a bude to vypadat jako zhoršení naší AI, přestože se změnil jen metr.**<br>⚠️ **A není to dorovnání asymetrie, je to zavedení nové:** náš trpasličí TV1200 je vyvinutý stejně jako soupeřovy — 2× Longbeard +Guard, 2× Blitzer a 2× Slayer **+Guard+Tackle** (rohy klece), 2× Runner +Block (`roster.cpp:544`). Podvybavení nejsme.<br>⇒ **Každá změna rosteru vyžaduje NOVÝ baseline**; výsledky přes tu hranici se neporovnávají. Zapsat jako éru, ne jako pokračování.<br>⭐⭐ **BALÍK PROTI BLACK ORKOVI: DAUNTLESS + WRESTLE NA TÉMŽ TĚLE** *(uživatel 14.08.: „Black Orci mají i sílu i dost pohybu na klec — musíme na ně něco vymyslet, třeba Dauntless a Wrestle")*. Black Orc: **MA4 ST4 AG2 AV9, Guard+Block** — MA4 je stejně jako náš Longbeard, takže na klec mu to stačí, na honičku ne ⇒ **bít ho v kleci, ne v běhu**.<br>Řetěz: **Dauntless** srovná ST3 na ST4 (**83 %**) → s asistencemi 1–2 kostky → **Wrestle** přebije jejich **Block** na „Both Down" ⇒ **oba k zemi**. **Bez Wrestle se Both Down vzájemně vyruší a nestane se nic** (obě strany mají Block).<br>⭐ **Troll Slayer už Dauntless má** (Block+Frenzy+Dauntless+ThickSkull) ⇒ je to přirozený nositel obojího; stačí doplnit Wrestle.<br>⛔ **BLOKOVÁNO NA P17** — Slayer má Block, a engine hráči s Blockem Wrestle při útoku nedovolí. **Bez P17 je celý balík mrtvý.**<br>⭐ **PROTI ORKOVI POTŘEBUJEME WRESTLE** *(uživatel 14.08.: „proti orkovi musíme mít někoho s Wrestle — připomínka ke stavbě týmů později")*. Black Orci i Blitzeři mají **Block** ⇒ na „Both Down" se to s naším Blockem vyruší a **nestane se nic**. **Wrestle to přebije a složí jejich ST4 tělo** — a my za to vyměníme AV9 tělo. Wrestle je **General**, takže Longbeard ho vezme obyčejným hodem. A hlavně: **náš Tackle je proti orkovi mrtvý** (nemají Dodge) ⇒ je to slot, kterým stejně plýtváme.<br>⛔ **BLOKOVÁNO NA P17** — dnešní engine hráči s Block+Wrestle Wrestle při útoku vůbec nedovolí, takže by ta změna byla k ničemu.<br>⭐ **NAŠE STRANA PŘESTAVBY** *(uživatel 14.08.)*: **Mighty Blow má dostat co nejvíc trpaslíků** — dnes ho nemá **ani jeden**, zatímco ork, human i wood-elf ho mají. To dorovnává tři ze čtyř matchupů a je to jediná změna, která míří v NÁŠ prospěch. **TV smí vylézt až na ~1500** *(„když nám TV vyleze na 1500, tak OK")*, takže rozpočet na to je.<br>⇒ Sem patří i **T5.14 Mighty Blow (jedna kostka místo obou)** — uživatel ho 14.08. odsunul z dnešní noci sem: *„s MB si budeme pak hrát při změnách rosterů"*. Dnes je dopad malý (Treeman + Ogre, pár případů), po přestavbě bude velký na obou stranách. Oprava je **napsaná včetně testů**, čeká na větvi.<br>⛔ **ODLOŽENO uživatelem 14.08.** *(„to přestavování týmů máme v plánu na později"; „korigování a doladění rosterů odloženo až poté, co doběhne toto vše")*. **SPOUŠTĚČ: až doběhnou víkendové běhy a bude se smět měnit baseline.** Otázka, kterou to má zodpovědět: *„obstojí naše doktrína proti soupeři, který umí odpovědět v jednom kole?"* — protože **proti OTT buildu není bezpečné ani kolo 8**, a tím se bortí dostatečnost P11. | **ODLOŽENO**<br><br>⛔⛔ **KONTROLNÍ SEZNAM, KTERÝ SE MUSÍ PROJÍT PŘI TÉHLE POLOŽCE — jinak se latentní vady probudí NEZKONTROLOVANÉ** *(zavedeno 25.08.)*. Dnešek ukázal, že „latentní" v praxi znamená „zapsané a zapomenuté": **T5.14 ležela hotová 11 dní**, **P31 33 dní**. ⭐ **Pojistka není štítek u nálezu, je to SEZNAM U SPOUŠTĚČE.** Než se změní kterýkoli roster, projít:<br>&nbsp;&nbsp;· **T5.25 Safe Throw** *(priorita STŘEDNÍ — ruší zachycení i turnover z fumblu; VLL výjimka už hotová)*<br>&nbsp;&nbsp;· **T5.23 Stunty** *(mapování 7/9; dnes hrozí DEAD tam, kde pravidla dávají Badly Hurt; `test_injury.cpp:244` to přišpendluje)*<br>&nbsp;&nbsp;· **T5.28 Decay** *(`ctx.hasDecay` se nastavuje a nikde nečte)*<br>&nbsp;&nbsp;· **T5.20** *(`Leader` = chybějící reroll ekonomika · `DumpOff` · `Animosity` · `PassBlock`)*<br>&nbsp;&nbsp;· **T5.15 Piling On** *(vlastní spouštěč, týž okamžik)*<br>&nbsp;&nbsp;· **N3-N5, N9, N19** z M-ROUND *(Diving Tackle, Break Tackle, Titchy, Tentacles, Timmm-ber!)*<br><br>⭐⭐⭐ **DOPLNĚNO 21.08. — TV 1500 A ROZPOČET DOVEDNOSTÍ JAKO BLOKÁTOR DOKTRÍNY.** Uživatel 21.08.: *„nápad na zvýšení TV na 1500… trpaslíci nebudou tak hladoví po rozšíření lavičky — o to víc se vejde sem."*<br>**Proč to přestalo být ladění:** **klec** chce **Guard + Tackle** na čtyřech rozích *(sestava z 07.08. je postavená přesně kolem toho)*, ale **screen** chce **Stand Firm** — a ⛔⛔ **náš trpaslík TV1200 nemá Stand Firm ANI JEDNOU** *(v celé pětici sestav ho má jediný hráč, elfí Treeman)* ⇒ **trpasličí screen se dnes zahrát NEDÁ.** Po **P50** *(začátek drivu se běží a staví screen)* se tedy **dvě fáze téhož drivu perou o tatáž těla** a TV1200 unese jen jednu.<br>**Uživatelův argument o lavičce ZMĚŘEN (300 her, dnešní engine), trvale mimo hru na zápas:** skaven **1,895** · wood-elf **1,658** · human **1,203** · **trpaslík 1,077** · ork **0,797**; zranění **3,21** u trpaslíka proti **11,20** u skavena. ⇒ **Trpaslík ztrácí o 44 % míň hráčů než skaven** ⇒ přírůstek TV mu může jít skoro celý do dovedností.<br>⇒ **Mění to Q12** *(Guard vs Stand Firm)*: nemusí se rozhodovat kompromisem, jen se posune na jinou TV. ⏳ Čeká na souhlas uživatele.<br>⚠️ **Dvě výhrady:** **(1)** TV se musí zvednout **OBĚMA** stranám, jinak měříme sílu rozpočtu, ne doktrínu — a **je to DALŠÍ ŘEZ v porovnatelnosti**, hned po tom dnešním; všechna dosavadní čísla jsou z TV1200. **(2)** Ta úspora na lavičce je měřená **PO** opravě vstávání; před ní byla jiná, protože sražený mizel z desky, ale nešel do zranění. |
| **T5.13c** | ⭐⭐⭐ **WRESTLE SE MUSÍ NĚKAM VEJÍT** *(uživatel 24.08.: „některý z našich musí dostat Wrestle — vlož to do kroku přerovnání TV a soupisek")*.<br>✅⭐ **P17 JE ODBLOKOVANÉ** — celý balík „Dauntless + Wrestle na témž těle" a „proti orkovi potřebujeme Wrestle" byl v T5.13 označený **BLOKOVÁNO NA P17** *(„engine hráči s Blockem Wrestle při útoku nedovolí")*. **Dnes se to opravilo**: `d0093607` udělal z Block+Wrestle **volbu** *(ř. 8672-8676 „even if one or both have the Block skill")* a `cac87c3d` naučil **vybírač kostky** tu volbu ocenit — bez druhého commitu by byla větev nedosažitelná, protože `scoreFace` dával Both Down **4** a odsunu **5**. ⇒ **Balík je hratelný, jakmile se ten skill někomu přidá.**<br>⛔ **Dnes ho nemá nikdo z nás.** `roster.cpp:548-549`: Longbeard = **Block, Tackle, Thick Skull** (+Guard u dvou). Wrestle je v enginu jen u **skaven Lineman +Wrestle** (2×) a u Bretonců.<br>⭐⭐ **ROZPOČET KOSTKY — proč to není „přidat skill", ale VÝMĚNA** *(uživatel 24.08.)*: **každá tvář blokové kostky chce jiný nástroj**.<br>&nbsp;&nbsp;**(1) Defender Stumbles** proti soupeři s **Dodge** potřebuje **TACKLE** — bez něj se DS smrskne na odsun *(`scoreFace`: 9 s Tacklem, **5** bez něj)*.<br>&nbsp;&nbsp;**(2) Both Down** proti soupeři s **Blockem** potřebuje **WRESTLE** — bez něj je to **mrtvá kostka** *(nestane se nic)*.<br>⇒ ⭐ **Tackle je proti ORKOVI mrtvý** *(nemají Dodge)* a **Wrestle je proti WOOD-ELFOVI a SKAVENOVI slabší** *(mají Dodge, ne Block na všech)*. **Jsou to komplementární sloty, ne konkurenční** — a rozhoduje se podle toho, **proti komu to tělo staví**.<br>⚠️ **Wrestle na Longbeardovi je kompromis:** v OBRANĚ si Block nechá *(implementováno — obránce s Blockem Wrestle nevolí)*, ale platí se slotem.<br>⭐ **Nositel se nabízí sám: Troll Slayer** — už má **Dauntless** *(srovná ST3 na ST4 v 83 %)*, takže řetěz **Dauntless → asistence → Wrestle** je na jednom těle. Longbeard je druhá volba *(Wrestle je General, vezme ho obyčejným hodem)*.<br>⇒ **Úkol: rozhodnout, KOLIK těl dostane Wrestle a MÍSTO ČEHO** — a to je věc TV, tedy sedí přesně do kroku přerovnání TV a soupisek.<br><br>⭐⭐ **TŘETÍ POLOŽKA ROZPOČTU: `Kick`** *(uživatel 24.08.: „při TV dát všem jednoho hráče s Kick a kopat tak, ať není touchback")*. ⛔ Dnes ho **nemá v rosteru NIKDO** *(0 výskytů v `roster.cpp`)*, ale `game_simulator.cpp:461` ho **přidá nejhlubšímu hráči kopajícího týmu při KAŽDÉM setupu** — tedy i po každém TD. Je to **schopnost, kterou nikdo nezaplatil, a mají ji všichni pořád**.<br>⇒ **V tomhle kroku: dát KAŽDÉMU týmu jednoho hráče s Kick, zrušit rozdávané `skills.add`, a mířit výkop tak, aby touchback nevznikl.** *(Kick je **General**, ř. 8205 ⇒ vezme ho obyčejným hodem kdokoli.)*<br>⛔ **A doplnit kontrolu umístění** — ř. 8207-8210: kopající nesmí stát **v širokém pásmu ani na LOS**; dnes to kód jen tvrdí v komentáři, netestuje.<br><br>⭐⭐⭐ **ČTVRTÁ POLOŽKA: ORKSKÝ TROLL A GOBLINI — a TVRDÁ PODMÍNKA K NIM** *(uživatel 24.08.: „před TTM chci mít dorovnané vzájemné TV — když orkům přidáme trolla a goblina, musíme něco přidat i ostatním")*.<br>**Pravidla** *(ř. 9069-9075)* orkovi dávají **0-4 Goblins** *(40k, `6 2 3 7`, Right Stuff + Dodge + **Stunty**)* a **0-1 Troll** *(110k, `4 5 1 9`, Loner, Always Hungry, Mighty Blow, Really Stupid, Regeneration, **Throw Team-Mate**)*. Náš základní `getOrcRoster()` je má; **`getOrcRoster1200` je oba vynechává**, a proto je celý balík TTM latentní.<br>⛔ **NEPŘIDÁVAT SAMOSTATNĚ.** Lineman stojí 50k, takže troll za linemana je **+60k** a goblin za linemana **−10k** ⇒ *(1 troll + 2 goblini) za 3 linemany = **+40k** jen orkovi*. **To by nezměřilo doktrínu, ale rozpočet** — a je to táž výhrada, která u T5.13c stojí od 21.08.: *„TV se musí zvednout OBĚMA stranám."*<br>⇒ **Pořadí je tvrdé: (1) dorovnat vzájemné TV všem pěti rasám, (2) teprve pak vracet orkovi trolla a gobliny, (3) a až potom má smysl kontrolovat, jestli TTM balík hraje.**<br>✅ **Pravidlová část je hotová už teď** *(TA3, commit `6d845b2e`: postih −1, tři rozptyly, fumble do vlastního pole, dopad na hráče, Right Stuff bez dopadového hodu, turnover jen s míčem; Always Hungry dvě jedničky; Stunty ignoruje cílové TZ)* ⇒ **až se soupisky dorovnají, nebude se na tom muset nic dodělávat, jen ověřit.** | **ODLOŽENO** — **SPOUŠTĚČ: krok přerovnání TV a soupisek (T5.13)**; P17 už nebrání |
| **T5.19** | ⏰ **PŘIHRÁVKY — ROZDĚLENO** *(uživatel 24.08.: „rozděl to — 3 opravy teď a zbytek, velký pass s detaily na později; **opravy prioritně dopředu**")*.<br>✅ **OPRAVY HOTOVÉ, commit `ed1a1d93`, 606/606 zelených.** Odloženo zůstává jen **modelování přihrávek jako celku**.<br>**Tři vady, ověřené přímo v `rules_bb2016.txt`** *(pátecní audit dvě z nich popsal nepřesně — tohle je opravené znění)*:<br>&nbsp;&nbsp;**F7 fumble, ř. 1742-1744:** *„1 or less **before or after modification**"* — my fumblujeme **jen na přirozenou 1** ⇒ fumblujeme MÍŇ, než máme *(při 2 TZ je hod 3 podle pravidel fumble)*. ⚠️ **Past:** modifikátory sypeme do CÍLE, ne do HODU, a `clamp(passTarget,2,6)` součet zkresluje ⇒ vést modifikátory zvlášť.<br>&nbsp;&nbsp;**F6 Pass skill, ř. 8336-8337:** reroll *„if he throws an inaccurate pass **or** fumbles"* — my rerollujeme **jen na fumble**. ⚠️ A `pass_handler.cpp:275-340` má **VLASTNÍ kaskádu** Pass → Pro → týmový, mimo `attemptRoll` ⇒ **je to táž vada jako TA1** a musí se opravit spolu.<br>&nbsp;&nbsp;**F8 rozptyl, ř. 735-737:** *„Roll for scatter **three times, one after the other**"* — my jedeme **výkopovou šablonu** (D8 směr × D6 vzdálenost) ⇒ nepřesná přihrávka letí **až 6 polí rovně** místo tříkrokové procházky o max 3 pole, která se může vrátit i do cíle. **Největší z těch tří.**<br>⚠️ **Všechny tři míří PROTI NÁM** — trpaslík nemá **Pass** ani jednou *(Runner = SureHands, ThickSkull, Block)*, takže F6 pomůže výhradně soupeřům a F8 tomu, kdo přihrává. ⇒ **odklad nás krátkodobě drží nahoře, ale i příští korpus měří špatnou přihrávku.**<br>⛔ **Pasovací hrozba není v ŽÁDNÉ z kontrol K\*.** ⭐ **A obě vakuózní aserce pod F8 jsou pryč** — nahrazeno pěti testy, které něco tvrdí. | **ČÁSTEČNĚ HOTOVO** — **ZBYTEK ODLOŽEN, SPOUŠTĚČ: až se přihrávky budou modelovat jako celek** *(kdy se přihrávka nabídne — dnes 0,49 PASS/hru; doktrína interceptu; Nerves of Steel u skavena; obranná kontrola proti přihrávce přes klec)* |
| **TA10b** | ⛔ **UPÍR SE KRMÍ VE ŠPATNOU CHVÍLI — na začátku akce místo na konci.**<br>**Pravidlo ř. 7936-7937:** *„at the **END of the declared Action**, but before actually passing, handing off, or scoring, the vampire must feed."* Náš `resolveBigGuyCheck` krmí **hned po deklaraci** ⇒ upír, který by se během akce k Thrallovi **došel**, jde zbytečně do rezerv. Je to **přísnější než pravidla** a ruší to celou taktiku týmu *(„vždy mít u aktivního upíra záložního Thralla" se u nás nedá zahrát pohybem, jen postavením)*.<br>⭐ **A druhá půlka: ř. 7934-7936** *„The Vampire may continue with his declared Action or **if he had declared a Block Action**, he may take a **Move Action** instead."* Tuhle výměnu nemáme vůbec. **Je napsaná jen pro BLOCK, ne pro Blitz** — a je to logické: **Block Action nemá žádný pohyb**, takže bez ní by upír, který ohlásil blok, ze svého pole neodešel.<br>⭐⭐ **KONKRÉTNÍ SCÉNÁŘ K OTESTOVÁNÍ** *(uživatel 24.08.)*: **upír ohlásí BLITZ, hodí 1, dojde k Thrallovi, kousne, blok nehodí — a tým už ten tah blitzovat nesmí.** Blitz výměnu nepotřebuje *(pohyb už v sobě má)*, ale **je utracený**: byl deklarovaný. ⚠️ **Blood Lust NEMÁ větu o ztrátě deklarované akce** — na rozdíl od Bone-heada a Really Stupid *(ř. 7980-7983, i s příkladem přímo o blitzu)*, protože upír akci provést může.<br>⛔⛔ **U nás ten scénář selže NA DVOU MÍSTECH:** (1) akce se zablokuje hned při deklaraci, takže se upír nepohne; (2) `blitzUsedThisTurn = true` je v `action_resolver.cpp:95` **uvnitř `case BLITZ`**, kam se při zablokované akci nedojde ⇒ **tým smí blitzovat znovu**. To druhé je **P55** a je pořád otevřené.<br>⚠️ **Oprava potřebuje HÁK NA KONEC AKCE**, který dnes v enginu není ⇒ není to jednořádkovka a **patří spíš do okruhu POHYB než do pravidel** *(je to o tom, KDY se akce vyhodnocuje)*.<br>✅ **Co je hotové 24.08.** *(`3097efab`)*: kousnutí je **hod na zranění** *(casualty se čte jako Badly Hurt ⇒ Thrall může být Stunned i KO, ale nikdy mrtvý ani s trvalým následkem)* · Thrall smí být **ležící i omráčený** · bez Thralla jde upír do **REZERV a je to TURNOVER**. | **ODLOŽENO — latentní** *(upír v korpusu nehraje)*; **SPOUŠTĚČ: okruh POHYB**, spolu s **P55** |
| **F13** | ⏰ **KICKOFF TABULKA — ROZHODNUTO 24.08.: ODLOŽIT.** *(uživatel: „rozhodni, jestli ji řešit nebo odložit")*<br>**Proč odložit, čtyři důvody:**<br>&nbsp;&nbsp;**(1)** Korpus běží na `simpleKickoff`; tabulka žije v `resolveKickoff`, což je **mrtvá cesta** ⇒ oprava **nezmění ani jedno měřené číslo**, dokud se nezapne `useFullKickoff`.<br>&nbsp;&nbsp;**(2)** Zapnout ji **není pravidlová oprava, je to NOVÁ MECHANIKA** — jedenáct výsledků tabulky *(Get the Ref, Riot, Perfect Defence, High Kick, Cheering Fans, Changing Weather, Brilliant Coaching, Quick Snap, Blitz!, Throw a Rock, Pitch Invasion)*, každý s vlastními pravidly. **Velký posun základní čáry**, ne doladění.<br>&nbsp;&nbsp;**(3)** ⭐ **Lekce z F12/Leap a ze Seznamu A auditu pohybu:** opravit resolver bez nabídky vyrobí **mrtvý kód se zelenými testy**. U F13 je skutečné rozhodnutí **ten přepínač**, ne obsah tabulky.<br>&nbsp;&nbsp;**(4)** ⛔ **NOVÝ důvod z 24.08.: obě výkopové cesty se ROZEŠLY.** `resolveKickoff` touchback měl, `simpleKickoff` ne *(dnes doplněno, F10)*; `simpleKickoff` má odraz na prázdné pole, plná cesta ne; zaokrouhlení Kicku se opravovalo v obou zvlášť. ⇒ **Než se tabulka zapne, musí se ty dvě cesty SJEDNOTIT** — a to je větší práce než tabulka sama.<br>⏰ **SPOUŠTĚČ: až padne rozhodnutí zapnout `useFullKickoff`.** Do té doby je to **spící vada** a ví se o ní.<br>⚠️ **Známý obsah vady** *(z parity auditu 21.08.)*: Riot má jednostranný marker · **Blitz! je jen šoupnutí o pole místo volného kola** · Throw a Rock dává jen stun. | **ODLOŽENO — spící** |
| **TA6** | ⏰ **BALL & CHAIN — ODLOŽENO 24.08. ZÁMĚRNĚ, jako poslední položka skupiny C.**<br>**Proč odloženo:** je to **goblinský Fanatic = secret weapon**, tedy jediná skutečná exotika z celé skupiny *(na rozdíl od TTM, který nese ORKSKÝ TROLL)*. **V korpusu nehraje a po dorovnání TV hrát nebude** — Fanatic je zbraň, ne pozice. ⇒ **Na noc ani na tabulku ras nemá vliv žádný.**<br>⛔ **Rozsah je přitom velký — audit pohybu (N17) napočítal SEDM odchylek**, celý handler je vymyšlená mechanika:<br>&nbsp;&nbsp;**(1)** směr: pravidla chtějí **throw-in šablonu** *(trenér ji natočí, D6 ⇒ 1 ze 3 směrů)*, engine hází **D8 do 8 směrů bez volby**;<br>&nbsp;&nbsp;**(2)** bloky proti obsazeným polím mají jet **normální bloková pravidla** *(počet kostek podle ST, asistence, povinný follow-up)*, engine má **jednokostkový „auto-blok"** bez ST;<br>&nbsp;&nbsp;**(3)** **ležící hráč v cílovém poli se má odsunout + hod na zbroj** — engine ho **PŘEKRYJE**, tedy dva hráči na jednom poli *(skutečná vada, ne jen parita)*;<br>&nbsp;&nbsp;**(4)** mimo hřiště = **„beaten up by the crowd"** *(hod davem)*, engine dává **automatické KO**;<br>&nbsp;&nbsp;**(5)** sražen ⇒ **rovnou hod na ZRANĚNÍ bez zbroje**, a **Stunned se čte jako KO**; engine hází zbroj a nechává PRONE;<br>&nbsp;&nbsp;**(6)** **GFI** stejným postupem po vyčerpání MA — engine ho nemá vůbec;<br>&nbsp;&nbsp;**(7)** sražení hráče aktivního týmu na hřišti **JE turnover** *(katalog ř. 368-369, B&C výjimku nemá)*; engine hlásí „nikdy turnover".<br>⚠️ **A i po opravě se to nezahraje** — **A5 z auditu pohybu**: akce i resolver existují, ale **žádné MAKRO ji neemituje** a všechna makra hráče s Ball & Chain výslovně vynechávají *(`macro_actions.cpp:424,572,602,761,939,989`)*. **Fanatik by se za celý zápas ani nepohnul.** ⇒ **Oprava resolveru bez makra = mrtvý kód se zelenými testy** *(lekce F12/Leap)*.<br>⏰ **SPOUŠTĚČ: až se bude hrát goblin nebo jiný roster se secret weapons** — tedy nejdřív v kroku rozšíření ras, ne v TV dorovnání. **Do té doby vědomě spící.** | **ODLOŽENO — poslední ve skupině C** |
| **M-ROUND** | ⭐⭐⭐ **OKRUH POHYB — SBĚRNÁ POLOŽKA** *(uživatel 24.08.: „pravidla → pohyb → teprve klec")*. Vstupem je `evidence/fable_movement_parity_20260824.md` *(19 nálezů N1-N19, Seznam A 7 položek, Seznam B 5 položek, 8 zadání na měření M1-M8)*.<br>**✅ HOTOVO 24.08. v pravidlovém kole:** N1 Dodge · N2 Stunty · N6+N7 Leap · N8 Both Down · N11 rooted · N14 Take Root perzistence · N16 TTM · invariant dvou těl na poli.<br><br>⛔ **ŽIVÉ V KORPUSU, OTEVŘENÉ — pořadí podle dopadu:**<br>&nbsp;&nbsp;**M1 · N10 — blitzer nesmí pokračovat v pohybu po bloku.** ⛔⛔ **NENÍ TO NOVÝ NÁLEZ — uživatel to hlásil 22.07.2026** *(`project_bloodbowl_hasacted_after_block_gap_20260722`)*, bylo to zapsáno jako **HIGH priority, „confirmed structural rules-fidelity gap"**, a **33 dní se neopravilo**. Fable audit ho 24.08. **znovuobjevil**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐ **Uživatelův původní případ:** *„wardancer musí mít možnost po blitzu utéct zpět a schovat se za svoje"* — dnes po bloku **nejde ani dál, ani zpět**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔ **A NESTAČÍ opravit `hasActed`: `REPOSITION` se NEGENERUJE pro hráče sousedícího se stojícím soupeřem** *(`macro_actions.cpp:584`)* — tedy přesně pro toho, kdo právě blokoval. **I kdyby směl jít, nemá kam.** *(Zase „resolver bez nabídky".)*<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **Proč bolí zrovna wardancera:** při dvou asistencích odchází z drivu **AV7 kus (Wardancer/Catcher/Gutter Runner) ve 30,1 %** proti **Ogrovi 6,9 %** *(měřeno 12.08.)* ⇒ necháme ho stát vedle toho, koho uhodil, a je to **4× lépe odstranitelný cíl než big guy**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐⭐ **A DŮSLEDEK PRO ČTENÍ TABULKY RAS:** N10 poškozuje **křehké a rychlé hit-and-run kusy nejvíc a trpaslíka nejmíň** *(ten chce v kontaktu zůstat)*. ⇒ **tlačí wood-elfa a skavena DOLŮ, zatímco TA1 a N1 je tlačily NAHORU** — **tři zkreslení ve DVOU směrech, částečně se vyrušila.** Další důvod, proč se z té tabulky nedá číst doktrína.<br> ř. 347-350: *„He may make one block during the move. **The block may be made at any point during the move**"*. `resolveBlock` nastaví `hasActed = true` na KAŽDÉ cestě ⇒ **hit-and-run a „nosič si blitzem otevře cestu a doběhne" jsou nemožné**; `expandBlitzAndScore` s nosičem-blitzerem tiše umře na `canAct()==false`. **Audit: VELKÝ pro sílu hry.** Hraje: každý blitz všech 5 ras.<br>&nbsp;&nbsp;**M2 · N13 = P55 — propadlá akce nespotřebuje týmový limit.** `blitzUsedThisTurn` až uvnitř `case BLITZ`, brána vrací `ok()` PŘED switchem ⇒ tým dostane **druhý blitz**. ⚠️ **Opravovat PER DOVEDNOST**: Bone-head *(ř. 7980-7983)* a Really Stupid *(8397-8401)* tým o akci připraví výslovně, **Take Root ne** *(8582-8583 mluví jen o bloku)*, Wild Animal *(8668-8669)* „the Action is wasted".<br>&nbsp;&nbsp;**M3 · N12 — Bone-head/Really Stupid dostávají TZ zpátky ZADARMO** každé vlastní kolo *(`game_state.cpp:70`)*. Pravidlo: stav trvá *„until he manages to roll a 2 or better at the start of a future Action or the drive ends"*. Hraje: human Ogre.<br>&nbsp;&nbsp;**M4 · N15 — Sprint se v nabídkách nepočítá.** Dosah paušálně `movementRemaining + 2` i pro Sprint *(3 GFI, ř. 8487-8490)* ⇒ wood-elf Catcher se Sprintem nedostane SCORE nabídku, kterou by resolver zvládl. Třída „akce se nenabídne".<br>&nbsp;&nbsp;⭐⭐ **RÁMEC K M5, ať se zítra nezačíná od „P45 něco pokazil"** *(uživatel 24.08.)*: **oprava vstávání byla OBCHOD, ne ztráta.** Ležící tělo nedělalo **nic**; teď stojí, a **stojící tělo má tackle zónu** ⇒ vyměnil se **DOSAH** *(útok: aktivace se spotřebují na vstávání, klec postupuje pomaleji)* za **PŘÍTOMNOST NA DESCE** *(obrana: víc zón a asistencí)*. Sedí to s měřením z 21.08.: **kroků pohybu stejně** *(12,83 → 12,87)*, ale **bloků +65 %, aktivací +40 %**, a **TD dolů**. **Dnešní číslo to dělá konkrétním: 24,31 vstávání na hru × 3 MA ≈ 73 polí za zápas utracených za postavení, a 98,6 % z toho se nepromění v krok.**<br>⇒ **Druhá půlka opravy ten obchod NERUŠÍ, vrací do něj dosah.**<br>&nbsp;&nbsp;**M5 · A7 — stand-and-go strop.** Makro umí postavit hráče **jen NA MÍSTĚ**, ř. 670-671 dovolují utratit zbytek pohybu ⇒ **strop na naměřenou hodnotu P45**; ležící hráč u volného míče se k němu ten tah nikdy nedostane.<br>&nbsp;&nbsp;⭐⭐⭐ **JAK SE BUDE HRÁT WRESTLE — dvě situace, různě těžké** *(uživatel 24.08.: „budeme si pak umět poradit s blitzem na Wrestle, nebo blitzem s Block+Wrestle pro vytvoření mezery?")*<br>&nbsp;&nbsp;&nbsp;&nbsp;**(A) blitz DO obránce s Wrestle — MALÁ oprava.** Vybírač kostky to už umí *(24.08.: `defUsesWrestle → 1`)*; chybí jen **plánovač** — `blockDieBadFraction` pořád počítá Both Down jako bezpečný pro útočníka s Blockem. To je **B2**, tři řádky v `macro_actions.cpp:294-296`. **Změřeno: 1,82 bloků/hru oceněných špatně, 0,27/hru realizovaně.**<br>&nbsp;&nbsp;&nbsp;&nbsp;**(B) blitz S Block+Wrestle pro MEZERU — potřebuje CELOTAH.** ⭐ **Kdo Wrestle použije útočně, položí i SEBE** ⇒ **mezeru neprojde ten, kdo ji udělal**; musí tudy projít **jiné tělo v témž kole**. To není rozhodnutí o jedné aktivaci, je to **SEKVENCE napříč dvěma hráči** — **táž architektonická věc jako stand-and-go**. *(N10 tu mimochodem nevadí: blitzer po Wrestle stejně leží.)*<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **CENA — ale POZOR NA SPRÁVNÉ SROVNÁNÍ** *(uživatel 24.08. opravil moje rámování: „blitzer projde dál a může být užitečný jinde — toto je výjimečnější situace")*. Wrestle položí i našeho hráče, a ten podle **M4** příští kolo vstane za 3 MA a v 98,6 % nikam nejde. ⛔ **Ale tahle cena padá jen na 9,8 % trpasličích bloků** *(4,50 mrtvých kostek z 45,9 bloků na hru)*. V **ostatních ~90 %** padne Defender Down / Stumbles / odsun, **blitzer zůstane stát**, a po opravě **N10** **projde dál a je užitečný jinde**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⇒ ⭐⭐ **A v té desetině je alternativa NULA.** Neměníme „stojící tělo za díru", ale **„nestane se nic" za „obránce leží, moje tělo leží"**. Úplně jiná bilance.<br>&nbsp;&nbsp;&nbsp;&nbsp;⇒ ⭐⭐⭐ **PŘEROVNÁNÍ: Wrestle NENÍ nástroj na prolomení zdi, je to ZÁCHRANA MRTVÉ KOSTKY.** Zeď se prolamuje běžnými tvářemi; Wrestle jen zajistí, že **ta jedna tvář ze šesti nepřijde vniveč**. ⇒ sedí to s rozpočtem kostky: **doplněk k Tackle, ne konkurent.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⇒ **Pořadí uvnitř okruhu: (A) hned s B2 · (B) až po M5/A7.**<br>&nbsp;&nbsp;**M6 · Seznam B — plánovač oceňuje jiný pohyb, než resolver provede** *(rodina ze 14.08.)*: **B1/P35** blitz se cení z VÝCHOZÍHO pole, hází se z cílového *(16,2 % blitzů má jiný bracket; oprava existuje za default-OFF ramenem)* · **B2** `estimateBlockFailChance` nezná OBRANNÝ Wrestle *(skaven staví 2 linemany +Wrestle)* · **B3** cena dodge kroku ignoruje rerolly · **B5** nabídka HAND_OFF_SCORE chce `adjDist ≤ 2`, executor umí dojít `movementRemaining`.<br>&nbsp;&nbsp;⭐⭐⭐ **PRIORITA V SEZNAMU A: JEN LEAP** *(uživatel 24.08.: „z toho seznamu mne teď zlobí jen Leap — a ten je součástí pohybu")*. **Doklad z kontroly citací:** Leap byl **JEDINÝ** mrtvý skill označený **„⛔ A JE V KORPUSOVÉM ROSTERU"**. Gaze, TTM, Ball & Chain ani Multiple Block **nemají v pěti korpusových sestavách nositele** ⇒ jejich chybějící makro **dnes nestojí nic**. **Leap má dva wardancery a stojí nás každou hru s wood-elfem.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐ **A je LEVNĚJŠÍ než zbytek seznamu, protože to není nová INTENCE, je to ZPŮSOB POHYBU.** Gaze a TTM by chtěly vlastní `MacroType` — nový druh tahu, který plánovač musí umět zvážit. **Leap ne: je to další primitivum v GENEROVÁNÍ CESTY** *(„přeskoč tělo a pokračuj")* uvnitř `ADVANCE` / `SCORE` / `REPOSITION`. ⇒ **nesahá se do žebříčku maker, jen do expanze cesty.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⏰ **CÍL: stihnout do pátku 28.08.** *(vstup pro `PLAN-WKND`)*.<br>&nbsp;&nbsp;&nbsp;&nbsp;⏰⏰ **DOMLUVA DNES 25.08. VE 14:00 (SELČ): LEAP JAKO TÉMA PRO FABLE.** Uživatel 25.08.: *„Leap tedy může být to téma pro Fable — ale zatím jej nespouštěj, domluvíme se dnes ve 14:00."* ⛔ **NESPOUŠTĚT dřív.** *(T5.21 na Opusu běží zvlášť a domluvou není dotčené.)*<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐⭐ **ROZHODNUTO 25.08. (uživatel): LEAP DOSTANE VLASTNÍ NOC.** *„leap je dost velký aby potřeboval svoji samostatnou noc — plán zítra, přidá se hned za pohyb ke kterému patří."* ⇒ **plán se píše 26.08.**, měření pak vlastní nocí; **řadí se hned za okruh POHYB**, ne mezi něj a klec.<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **Důsledek pro noční okno:** jedna noc = jedno měření *(engine se od startu nesmí přestavět)*. Noc 25.→26.08. je obsazená **B1/P35**, takže Leapova noc je **nejdřív 26.→27.08.** — a to sedí na termín pátek 28.08. jen tehdy, když se plán i implementace stihnou 26.08. **Víc ze Seznamu A se před víkendem NEČEKÁ** — a je to vědomé rozhodnutí, ne skluz.<br>&nbsp;&nbsp;**M7 · Seznam A — akce existuje, ale MAKRO ji neemituje** *(P45/F12 o patro výš, a korpus hraje PŘES MAKRA)*: **A1 Leap** *(nabídka doplněna 24.08., makro chybí ⇒ wardancer pořád neskočí)* · **A3 gaze** · **A4 TTM** · **A5 Ball & Chain** · **A6 Multiple Block** · **A2 Jump Up blok z lehu** *(ř. 8200-8204, ležícímu se BLOCK vůbec nenabízí)*.<br><br>⏰ **LATENTNÍ, zapsané a čekají na rasu/roster:** **N3 = B2 Diving Tackle** *(−2 zdarma, bez volby, bez položení do opuštěného pole)* · **N4 Break Tackle bez limitu 1/kolo** · **N5 Titchy vyzařuje −1, i když nemá** · **N9 Tentacles neukončí akci** *(retry smyčka hází nový únik každou iterací)* · **N17 = TA6 Ball & Chain** · **N18 = TA10b upír krmí na začátku akce** · ⛔ **N19 Timmm-ber! v enginu NEEXISTUJE** *(chybí i v enumu; ř. 8625-8635, asistence vstávání +1 za volného souseda)* · **B3 Piling On** · **rozdaný Kick + kontrola umístění kopajícího** *(→ T5.13c)*.<br>⭐⭐ **ZADÁNÍ NA MĚŘENÍ (M1-M8) — co čtením rozhodnout NEJDE.** Audit je odděluje schválně, aby se z nich nedělaly závěry:<br>&nbsp;&nbsp;**M1** *(k N11)*: jak často se na blitz pošle **zakořeněný** hráč? Offline z eventů: `SKILL_USED TakeRoot fail` → tentýž hráč později v témže drivu `PLAYER_MOVE`/`GFI`.<br>&nbsp;&nbsp;**M2** *(k N8)*: kauzální dopad opravy Both Down na TD/hru, zranění a délku drivů — **jen párové A/B**; čtením jde říct pouze směr.<br>&nbsp;&nbsp;**M3** *(k N1)*: posun úspěšnosti dodge po odebrání −1 jde **spočítat offline** *(přepočítat cíle dodge hodů z uložených her)*; dopad na výsledky jen A/B.<br>&nbsp;&nbsp;**M4** *(k A7)*: kolik % vstávání má **nevyužité MA ≥ 1 a smysluplný cíl** *(míč/EZ/klec v dosahu)*? ⇒ řekne, **jaký strop P45 pořád drží**.<br>&nbsp;&nbsp;**M5** *(k A7-nosič)*: jak často MCTS vloží mezi nosičovo vstávací makro a jeho pokračování **makro jiného hráče**? *(actor-switch pak nosiče uzavře přes `hasMoved→hasActed`)* ⇒ rozhodne, jestli nosičův stand-and-go **reálně funguje**.<br>&nbsp;&nbsp;**M6** *(k B1)*: zapnout `blitzLandingArm` v párovém A/B — čítač repicků **už existuje**.<br>&nbsp;&nbsp;**M7** *(k B2)*: četnost a výsledky blitzů do obránců s **Wrestle** v korpusu.<br>&nbsp;&nbsp;&nbsp;&nbsp;✅ **ZMĚŘENO 25.08.** *(`diag_wrestle_blitz_m7_20260825.py`, 6 000 her se skavenem)*:<br><pre>obránce      bloků   /hru   Both Down   co se stalo
s Wrestle    32 681   5,45     15,1 %    OBA K ZEMI ve 100 %
ostatní     333 834  55,64     13,0 %    naše tělo padne ve 13,9 %</pre>⇒ ⭐ **STROP PRO B2 = 0,821 našeho těla na hru** padne na kostce, kterou plánovač počítá jako **bezpečnou**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔ **A KÓD TO POTVRZUJE:** `blockDieBadFraction(bool attackerHasBlock)` *(`macro_actions.cpp:294`)* má **jediný parametr — útočníkův Block** — a vrací `attackerHasBlock ? 1/6 : 2/6`. Proti obránci s Wrestle je pravda **2/6** *(BB2016 ř. 8670-8676: „Both players are Placed Prone… EVEN IF ONE OR BOTH HAVE THE BLOCK SKILL")* ⇒ **plánovač se u 5,45 bloku na hru mýlí přesně DVOJNÁSOBNĚ**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐ **Oprava je malá a jednoznačná:** druhý parametr *(má obránce Wrestle?)*, a při něm `2/6` bez ohledu na útočníkův Block. ⚠️ Oprava z 24.08. *(`cac87c3d`)* se týkala **vybírače kostky** *(útočníkův Wrestle)*, ne **oceňování** — B2 zůstává otevřené.<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ Korpus je z **21.08.**, tedy strop vady, ne dnešní stav.<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔⛔ **PAST MĚŘENÍ, KTERÁ TO NEJDŘÍV UKÁZALA OBRÁCENĚ:** Wrestle je **Placed Prone**, ne knockdown ⇒ **`KNOCKED_DOWN` se NEEMITUJE** *(nemá hod na zbroj)*; emituje se `SKILL` na obránci a obě těla přejdou do stavu „leží". První běh proto napočítal **„nic ve 100 %"** — číslo, které vypadalo úplně věrohodně a znamenalo pravý opak. ⇒ **stav se ověřuje ze STAVU, ne z názvu události.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐ **KONKRÉTNÍ PŘÍKLADY** *(uživatel 25.08.: „my hlásíme je bezpečno a pak upadnem — dej příklad")*, hra `dwarf-skaven g0000`:<br><pre>půle 1 kolo 6  Runner +Block (má Block) z (3,2) -> Lineman +Wrestle (3,1)
               BOTH DOWN -> útočník 0->1, obránce 0->1   (oba lezi)
půle 2 kolo 1  Troll Slayer +Guard+Tackle z (11,3) -> Lineman +Wrestle (12,4)
               BOTH DOWN -> oba lezi
půle 2 kolo 4  tentýž Slayer z (12,4) -> Lineman +Wrestle (13,5)
               BOTH DOWN -> oba lezi</pre>**Co plánovač spočítal:** útočník má Block ⇒ `bad = 1/6`. Na **jedné** kostce **16,7 %** rizika, na **dvou** *(vybíráme my)* **2,8 %**.<br>**Co byla pravda:** proti Wrestle je `bad = 2/6` ⇒ **33,3 %** na jedné a **11,1 %** na dvou. ⇒ ⭐ **na dvou kostkách je to podcenění ČTYŘNÁSOBNÉ**, ne dvojnásobné.<br>⚠️ A třetí případ ukazuje, že se to opakuje **témuž tělu** — Slayer to schytal dvakrát ve dvou kolech.<br>&nbsp;&nbsp;⭐⭐ **M9** *(k M1/N10/P31 — ZADÁNO 24.08.; chybělo 33 dní a BLOKOVALO opravu)*: **strop na hodnotu ústupu po blitzu.** **(a)** jak často má blitzer po bloku **zbývající MA ≥ 1**? · **(b)** jak často **sousedí se stojícím soupeřem** hned po akci, tedy zůstává vystavený? · **(c)** jak často by měl **KAM** — volné pole mimo soupeřovy TZ v dosahu zbytku MA? · **(d)** rozdělit **podle AV**: AV7 kusy *(Wardancer, Catcher, Gutter Runner)* proti AV9. ⇒ **Řekne, jestli je N10 velká vada, nebo hygiena — a PRO KOHO.**<br>&nbsp;&nbsp;&nbsp;&nbsp;✅ **ZMĚŘENO 24.08.** *(`diag_blitz_retreat_ceiling_20260824.py`, commit `71cc3181`, 18 000 her)*: **317 107 blitzů = 17,62 na hru.** (a) zbývá MA ≥ 1 v **76,9 %** · (b) zůstává v cizí TZ ve **42,6 %** · (c) **a měl by kam: 23,2 % = 4,09 NA HRU.**<br>&nbsp;&nbsp;&nbsp;&nbsp;**Podle AV:** **AV7 27,2 %** *(Wardancer/Catcher/GR)* · AV8 23,3 % · **AV9+ 18,2 %** ⇒ **křehké kusy to potkává 1,5× častěji než obrněné.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⇒ ⭐⭐ **PODMÍNKA Z 22.07. JE SPLNĚNA — VELIKOST JE ZNÁMÁ A NENÍ MALÁ. P31/M1 SE SMÍ OPRAVOVAT.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **Dvě výhrady:** *„má kam"* ≠ *„zadarmo"* — je v cizí TZ z definice (b), takže **ústup stojí dodge** *(táž past jako u vstávání, kde 85,6 % bylo v kontaktu)*; a neřeší se follow-up ani odsun, počítá se jen **jednokrokový** ústup *(konzervativní)*.<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐ **Ale první výhrada se tu OTÁČÍ V ARGUMENT:** kus, který ústup nejvíc potřebuje **(AV7)**, je zároveň ten, kdo **dodge zvládá nejlíp** *(AG4 + Dodge)* ⇒ hodnota opravy se na wardancerovi koncentruje z obou stran.<br>&nbsp;&nbsp;⭐ **M10** *(k `P27` — ZADÁNO 24.08. při prohlídce zabržděných položek)*: `BLITZ_AND_SCORE` se nabídl v **1 123 kolech**, nosič blitzoval 38×, TD 2×. Brzda v P27 zní *„před psaním kódu se musí změřit, kolik z těch kol je PŘI VEDENÍ"* — tam je nízký prior **záměr** *(doktrína stall, „nepřekračuj čáru dřív, než musíš")*, ne chyba. **Zadání:** rozdělit ta kola podle **stavu skóre** *(vedeme / remíza / prohráváme)* a podle **zbývajících kol v půli**; teprve zbytek *(nabídnuto, nevede, a přesto se nešlo)* je skutečný strop na opravu. **Offline nad `crosses_20260821_data`.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⏰ **ODLOŽENO 25.08. NA 26.08.** *(uživatel: „M10 zapiš na zítra ke kontrole noci")*. ⛔ **Důvod je datový, ne časový: nabídky maker v korpusu NEJSOU** — jsou to vnitřnosti hledání, ne zaznamenané události *(korpus veze `events`: MOVE, BLOCK, PUSH, KNOCKED_DOWN, DODGE, SKILL… ale ne nabídky)*. ⇒ M10 se musí **přehrát přes engine** nad 18 000 hrami, a to vedle běžící noci nejde. **Ráno 26.08., až dojede korpus.**<br>&nbsp;&nbsp;**M8**: pořadí dodge vs. GFI v jednom kroku — **text ho nefixuje**, výsledek je ekvivalentní, liší se jen spotřeba rerollů. ⛔ **Není to dnes nález** *(a je dobře, že to audit takhle přiznal)*.<br><br>✅ **A CO JE OVĚŘENĚ SPRÁVNĚ** *(oddíl „Co proti pravidlům SEDÍ", ~25 položek s čísly řádků)* — **znovu to neauditovat**: MA a spotřeba · GFI vč. Sprintu a Blizzardu · Sure Feet · mechanika dodge · Tackle jako rušič rerollu · **celá reroll ekonomika** *(jeden hod = jeden reroll, týmový 1/kolo, Pro brána, Loner)* · vstávání vč. 4+ pro MA<3 · Jump Up *(mimo blok z lehu)* · **Take Root** *(mimo N11/N14)* · Bone-head/RS/Wild Animal hody · **push řetěz** · crowd surf · Stand Firm · Side Step · Grab · **Frenzy** · blitz blok za 1 MA · Shadowing · Tentacles · **Leap resolver** · omráčení face-up na konci kola · **Wrestle**.<br><br>⚠️ ⭐ **A pravidlo, které z dneška plyne: „latentní" NENÍ „neškodné", je to „odložené".** Zapojení Leapu 24.08. okamžitě probudilo N6 a N7, které za ním roky spaly — stejně jako 21.08. oprava vstávání probudila F1 a B1. **Každé zapojení nabídky se musí projít se seznamem latentních vad v ruce.** | **OTEVŘENO — okruh 2 (POHYB)**, po dokončení pravidel |
| **Q13** | ⏰ **VOLBA PO LOSU — OVĚŘIT MĚŘENÍM.** Nasazeno 24.08. *(`a78470e7`)*: **křehcí/rychlí volí ÚTOK, ostatní včetně TRPASLÍKA volí OBRANU**, potvrzeno uživatelem.<br>⚠️ **Je to jediná dnešní změna ze SEKUNDÁRNÍHO zdroje** *(bbtactics.com)* — v den, kdy se **třikrát** ukázalo, že komunitní výklady u nás zakódovaly vadu do kódu.<br>⭐⭐ **Ale je tu zásadní rozdíl:** u pravidel rozhoduje **rulebook**; u volby po losu **žádný primární zdroj NEEXISTUJE** — ř. 304-307 říkají jen *„vítěz volí"*. ⇒ komunitní zdroj tam **není náhražka pravidla, je to jediný existující druh důkazu**. Je to otázka o **hraní**, ne o pravidlech.<br>⭐ **A nestojí to jen na něm:** vlastní data *(18 000 her)* dala **+4,87σ** (human) a **+4,68σ** (orc) ve prospěch toho, kdo dostane druhou půli, a **trpaslíkova výjimka má vysvětlení** *(−0,40σ; poslední drive půle neumí proměnit)*.<br>⇒ ⛔ **Číst jako PRACOVNÍ VÝCHOZÍ VOLBU, ne jako pravdu.**<br>**ZADÁNÍ:** párové A/B na `defaultTossElection`, rameno = **trpaslík volí ÚTOK** místo obrany. Levné rameno, mění jednu funkci. | **ODLOŽENO — ⏰ spouštěč: volné noční okno** |
| **PLAN-WKND** | ⭐⭐⭐ **VÍKENDY PATŘÍ DLOUHÝM BĚHŮM — ale co v nich poběží, rozhoduje ZODPOVĚDITELNOST, ne kalendář** *(uživatel 24.08.: „víkendy dát křížovému korpusu; po tomto budoucím víkendu bychom měli mít křížové tréninky jako základ pro porovnání")*.<br>
**Slot je správný:** minulý sběr běžel **pá 11:54 → so 18:31 ≈ 31 h** *(18 000 her, 15 dvojic × 1 200)*, do víkendu se vejde i s rezervou, a nestojí pracovní čas.<br>
⛔ **ALE PODMÍNKA NENÍ „skončil týden", je to „JE OKRUH UZAVŘENÝ?"** Korpus z 21.-22.08. stál **31 h stroje a vydržel DVA DNY** — zneplatnilo ho 20 pravidlových oprav. **Sbírat uprostřed okruhu = zopakovat tu ztrátu.**<br>
⭐ **ROZHODOVACÍ PRAVIDLO NA PÁTEK** *(ať se to nerozvažuje pod tlakem)*:<br>
&nbsp;&nbsp;**(a) korpusově živé položky okruhu POHYB hotové** *(N10 ústup po blitzu · P55 · N12 · N15 · A7 stand-and-go)* **⇒ SBÍREJ KŘÍŽOVÝ KORPUS.** Bude to první snímek enginu, který **počítá podle pravidel A hraje podle nich**.<br>
&nbsp;&nbsp;**(b) ještě se v nich hrabeme ⇒ dej víkend VELKÉMU PÁROVÉMU A/B místo sběru.** ⭐ **Párové A/B je vůči pohybu báze IMUNNÍ** *(obě ramena týž engine, tytéž seedy)*, takže se ten čas **neznehodnotí, ať se v pondělí opraví cokoli** — viz [[feedback_moving_baseline_only_paired_ab]].<br>
⚠️ **Realistický odhad na 4 dny (út-pá):** korpusově živá část okruhu **je zvládnutelná** *(má změřený strop a jsou to ohraničené opravy)*. **Seznam A se do toho NEVEJDE** — chybějící **makra** pro gaze, TTM, Ball & Chain, Multiple Block a **Leap** jsou nová vrstva rozhodování, ne oprava. ⇒ **Snímek po tomhle víkendu bude „pravidla + pohyb resolverů", ne „a AI to umí hrát".** <br><br>⭐⭐⭐ **DOPLNĚNO 25.08. — VÍKEND NENÍ JEDEN SLOT, JSOU TO 3-4** *(uživatel: „do víkendu, kde by mohl být prostor dohánět noci, jsme místo toho vložili X měření")*. Pátek 18:00 → pondělí 08:00 je **~62 h strojového času**; noc je 12-17 h. Dnešní znění pravidla ale nabízí **jednu volbu na celý víkend**, takže při variantě (a) se spotřebuje 31 h a **zbylých ~30 h se nevyužije** — a fronta nocí se nezkrátí ani o jednu.<br>⇒ **Rozhodnutí v pátek má TŘI varianty, ne dvě:**<br>&nbsp;&nbsp;**(a)** okruh uzavřen ⇒ **křížový korpus** *(~31 h)*<br>&nbsp;&nbsp;**(b)** hrabeme se v něm ⇒ **jedno velké párové A/B** *(imunní vůči pohybu báze)*<br>&nbsp;&nbsp;**(c)** **ŘETĚZ několika A/B za sebou** — ⛔⛔ **VÝJIMKA, KTEROU JE TŘEBA OBHÁJIT, NE VÝCHOZÍ VOLBA** *(uživatel 25.08. po rozboru: „mi z toho vychází — s řetězením opatrně")*. Nabízí se to jako úspora času, ale zaplatí se to tím, co se nedozvíme: **kombinaci**. ⇒ **kdo řetěz navrhne, musí PŘEDEM napsat, proč jsou ta ramena nezávislá** — jinak platí (a) nebo (b). Podmínky, obě z **T2.20**:<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔ **(c1) ramena musí být z RŮZNÝCH mechanik.** Dvě ramena téže mechaniky *(P35 × M1/N10 — obojí blitz)* dají každé svou deltu, ale **ne kombinaci**, kterou bychom nasazovali.<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔ **(c2) v řetězu se NESBÍRÁ KORPUS.** Sbírá se s rameny vypnutými ⇒ zastará v okamžiku, kdy se po vyhodnocení cokoli zapne.<br>&nbsp;&nbsp;⚠️ **A cena řetězu je tvrdá:** celý běží na **jednom stavu enginu** — zamrzne v pátek večer a rozmrzne v pondělí ⇒ **o víkendu se nesmí opravovat.**<br><br>&nbsp;&nbsp;**(d)** ⭐⭐⭐ **CYKLUS `build → testy → překlad harnessu → běh → log → CLEAR`, několikrát za sebou** *(uživatel 25.08.: „a co takhle na ten víkend naplánovat build test log clear build test log clear?")*. **NAHRAZUJE (c) a je lepší z jednoho konkrétního důvodu:** každý cyklus **staví znovu**, takže engine **není zmrazený 62 h** a mezi běhy se **smí opravovat**. Přiřaditelnost zůstává — každé A/B je párové na svém buildu, obě ramena týž engine.<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **Daň:** mizí **porovnatelnost MEZI běhy** *(každý na jiném enginu)* — přijatelné, je to `feedback_moving_baseline_only_paired_ab`. ⛔ **Kombinaci to nedá pořád** *(`efekt(P35 + M1/N10)` chce vlastní běh)*.<br>&nbsp;&nbsp;&nbsp;&nbsp;⛔⛔ **ČTYŘI MÍSTA, KDE SE TO ROZBIJE — tři z nich nás už stály běh:**<br>&nbsp;&nbsp;&nbsp;&nbsp;**(d1) „CLEAR" MUSÍ ZAHODIT CELÝ ADRESÁŘ.** `run_night_ab.sh`: `if [ -f "$OUT/AB_DONE" ] → přeskakuji` *(totéž `COLLECT_DONE`, `NIGHT_DONE`)*. ⇒ **zopakovaný OUT znamená, že cyklus tiše NEUDĚLÁ NIC** a do logu napíše „už hotové". **Víkend by proběhl naprázdno a vypadal úspěšně.** Každý cyklus = nový OUT.<br>&nbsp;&nbsp;&nbsp;&nbsp;**(d2) POŘADÍ UVNITŘ CYKLU NENÍ VOLNÉ:** build → testy → **překlad harnessu** → běh. Harness se linkuje proti `.so` za běhu ⇒ přeložený dřív sáhne na staré offsety `struct Player`. **To je SEGFAULT, co zabil fázi B (32f93c8f).**<br>&nbsp;&nbsp;&nbsp;&nbsp;**(d3) ČERVENÉ TESTY NESMÍ ZABÍT VÍKEND.** Dnešní launcher dělá `exit 1`; v cyklu ve 03:00 v neděli by to odstavilo **zbylých 40 h**. ⇒ řidič ten cyklus **přeskočí, hlasitě zaloguje a jede dál**.<br>&nbsp;&nbsp;&nbsp;&nbsp;**(d4) KAŽDÝ CYKLUS STAVÍ KÓD S RAMENY DEFAULT OFF.** Kdyby cyklus 2 stavěl kód, kde je rameno z cyklu 1 zapnuté, **posune se základní čára**. ⇒ **cykly kód PŘIDÁVAJÍ, nikdy ho NEZAPÍNAJÍ.**<br>&nbsp;&nbsp;&nbsp;&nbsp;⏰ **A DVĚ VĚCI SE PÍŠOU V PÁTEK, NE V SOBOTU:** seznam cyklů *(čtveřice: commit/větev · mode · předregistrace · počet párů)* a **předregistrace ke každému**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⭐⭐⭐ **A PŘED OSTRÝM VÍKENDEM SE PROVĚŘÍ SÁM ŘIDIČ** *(uživatel 25.08.: „jsem pro pořádnou kontrolu před spuštěním s malým počtem cyklů uvnitř")*. Táž disciplína o patro výš: dnes light test prověřil **noc**, tady musí prověřit **řidiče**. Návrh testu: **3 cykly po ~4 párech**, různé mody, a ⭐ **jeden cyklus ZÁMĚRNĚ ROZBITÝ** *(červený test nebo neúspěšný build)*, protože **jen tak se ověří (d3)** — jinak se testuje jen šťastná cesta. Kontroluje se: každý cyklus dostal **vlastní OUT a opravdu běžel** *(ne „přeskakuji")* · harness přeložen **až po** `.so` · předregistrace načtena per cyklus · rozbitý cyklus **přeskočen a zbytek dojel**.<br>&nbsp;&nbsp;&nbsp;&nbsp;⚠️ **A dnešní lekce, proč zrovna tuhle kontrolu nedělat grepem:** 25.08. mi build testů selhával třikrát po sobě a **nevšiml jsem si**, protože jsem filtroval `error`, zatímco make hlásí `Error 1`. ⇒ **řidič se řídí NÁVRATOVÝMI KÓDY, ne hledáním v textu.** | **ROZHODNOUT V PÁTEK 28.08.** |

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
| P1 | **Přepsat K33 a K34 na spojité** — jako prahy nepředpovídají nic (0,6σ / 0,8σ), jako počty patří k nejsilnějším. Platí i pro E1: *„ani jeden otevřený roh"* je správný **cíl**, ale jako **kontrola** se má měřit `REACH0` jako počet (1 → 8,3 %, 4+ → 33 % ztráty). | **UZAVŘENO 18.08.** — `Check.num()`; K33 ⌀ **+1,62 bloku/kolo** vedle podílu 79,1 % · K34 ⌀ **+1,84 REACH0** vedle 41,0 % · K35 ⌀ **+0,95 FB2** vedle 72,2 %. Tisk to označuje `← metr`, aby bylo vidět, že **podíl je pravidlo a číslo je metr**. Ověřeno na 3 000 hrách. |
| A1 | **Anomálie: TD flag nesedí se skóre delta** — 9 z 3000 her (0,3 %) | **OTEVŘENO — zadáno uživatelem na 14.08. přes den** |

# 7. AŽ NAPOSLED

| ID | co | stav |
|---|---|---|
| T6.1 | N5 vstřelení plánu priorem `f(rezerva)` — jediná změna chování, až po měřicím aparátu | **OTEVŘENO** |
| T6.2 | **Učení** — až po dokončení repertoáru, s úplnou procedurou jako nulovou hypotézou | **OTEVŘENO** |

---

# ⚠️ VÝHRADY, PODLE KTERÝCH SE NESMÍ NAVRHOVAT

* ⛔⛔ **SOUPEŘ NEHRAJE SVOU RASU** *(uživatel 14.08.: „skaven se místo toho učí
  hrát jako elf a to ho musíme odnaučit")* — **nejzávažnější omezení, jaké
  máme**, protože se týká **všech** naměřených čísel najednou.
  **Kořen je strukturální: obě strany řídí týž `MacroMCTSPolicy` s toutéž
  heuristikou**, psanou pro trpaslíka (klec, rohy, tempo, „loose ball is bad"
  jako konstanta). Skaven ji dostane taky ⇒ hraje obecné nošení míče v tvaru
  místo svého plánu (donutit míč spadnout i za cenu těl → vyhrát závod MA9 →
  utéct Dodgem). Přihrávky prakticky nepoužívá — **40 na 3000 her**.
  ⇒ **„451 TD proti skavenovi" je číslo o naší AI hrající proti sobě samé
  v jiném dresu.** Podrobně `evidence/matchup_asymmetry_20260814.md`.
  ⚠️ **Není to totéž** co výhrada níž: ta říká, že nás soupeř netrestá; tahle
  říká, že **nehraje ani vlastní hru**. ⇒ **ÚKOL: soupeřova AI má hrát plán své
  rasy** — patří k **T5.13** (přestavba rosterů), protože roster bez plánu je
  jen jiná čísla na papíře.
  ⭐ **Rozpadá se to na dvě velmi různě drahé části** *(uživatel 14.08.: „tohle
  bude možná těžší než odstřihnout dwarfa od učení rutinou, co musí stihnout
  dodržet")*:
  **(A) rasově citlivé hodnocení — LEVNÉ, a platíme za ně tak jako tak.**
  Heuristika není trpasličí doktrína, je **slepá k rase**: `heuristic -= 0.1`
  za volný míč dostane skaven s MA9 stejně jako trpaslík s MA4 — **proto hraje
  jako elf**. ⇒ **Je to týž člen, který opravujeme kvůli P10a.** Udělat cenu
  volného míče funkcí toho, kdo je blíž a **kdo rychlejší**, zlepší **naše**
  rozhodování **a zároveň** naučí skavena hrát skavena. **Jedna oprava, obě
  strany**; odhadem pokryje největší půlku rozdílu.
  ⭐ **Zdroje k skavení doktríně jsou už sebrané:**
  `evidence/skaven_doctrine_sources_20260814.md` — a **rozsuzují starý spor**
  („hraješ skaveny moc bash"): doktrína violence **výslovně žádá**
  (*„be violent like a bash team while simultaneously playing like elves"*),
  ale **ne Gutter Runnery** (*„keeping Gutter Runners … screened from attack"*).
  A výslovně říká, že skaven **není stavěný na stalling** — což je přesně hra,
  kterou ho dnes naše AI nechává hrát.
  **(B) skutečný plán rasy — DRAHÉ, ale u skavena MÉNĚ.** Přihrávky za klec,
  Nerves of Steel, kdy obětovat tělo. Z (A) nevypadne. **Vlastní projekt, ne
  úkol.** ⭐ **ALE: skaven je uživatelova silnější stránka než trpaslík**
  *(14.08.: „na tu skavení hru jsem zvědav, tam snad pomůžu trochu líp než
  u dwarfů")* ⇒ **u skavena levnější než u trpaslíka, ne dražší** — trpaslík
  vyhrává NEDĚLÁNÍM chyb (samé zákazy, doktrína se rodila těžce), skaven má plán
  ve třech krocích, který jde vyslovit. ⇒ **Část (B) začít skavenem**, ne
  wood-elfem ani orkem: nejlevnější vstup a zároveň náš nejzkreslenější matchup.
  ⇒ **(A) zkusit samostatně**, protože se za ni platí prací, kterou stejně
  děláme.
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

Platí na **P2 (5)**, **P9c**, **T1.8** i na Fableho „idle těla dosáhnou".

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

# ⭐⭐⭐ NÁLEZ DNE, PŘESNĚJI: JEDNA AKCE, PĚT VYPNUTÝCH ÚROVNÍ
*(uživatel 14.08.: „tady jde o několik levelů, které byly všechny vypnuté")*

Není to šest nezávislých chyb. Je to **jedna akce, které je samostatně vypnutá
každá úroveň.** Na příkladu „Slayer udeří na Black Orka":

| # | úroveň | stav | položka |
|---|---|---|---|
| 1 | **nabídka** — počítá se Dauntless? | ⛔ ne | P13 |
| 2 | **prior** — je ten cíl cennější? | ⛔ ne, `BLOCK 15` pro všechny | P10a / P15 |
| 3 | **volba kostky** — ví o Wrestle? | ⛔ ne | P14 |
| 4 | **reroll** — ví o Wrestle? | ⛔ ne | P14 |
| 5 | **odsun** — kam ho pošleme? | ⛔ naslepo | P9 |

**Akce musí projít všemi pěti a každá ji zahodí samostatně.**

## ⚠️ OTEVŘENÁ METODICKÁ OTÁZKA: řetěz vs. „jedna změna najednou"

**Měřit je po jedné znamená systematicky podhodnocovat každou z nich.** Opravím
úroveň 1 → akce se dostane do nabídky → spadne na úrovni 2. Strop
*„86 → 110 TD"*, který Fable spočítal pro Dauntless, je strop **první úrovně
v pořád rozbitém řetězu**, ne strop toho tahu.

| postup | co dá | co ztratí |
|---|---|---|
| **po jedné** | čistou atribuci | každá vyjde jako nula a **postupně se všechny zamítnou**, přestože dohromady fungují |
| **všechny naráz** | skutečný potenciál | při záporném výsledku **nepůjde říct, která úroveň to pokazila** |

⛔ **NEROZHODNUTO — k rozhodnutí přes víkend.** Dnešní běh na tom nic nemění.
⭐ **Částečná odpověď už je zabudovaná:** čítače `cand_daunt` (nabídek) a
`cand_roll` (skutečných hodů) ukážou, **na které úrovni to spadne** — ne jen
že to nikam nevedlo. Když nabídky stoupnou a hody ne, je vinen prior (úroveň 2),
ne Dauntless. **Tohle je levnější než měřit obojí zvlášť.**
⇒ Možná cesta ven: **opravit celý řetěz, ale každou úroveň vybavit čítačem**,
a atribuci dělat z čítačů místo z oddělených běhů.

# ⭐⭐⭐ VÝSLEDKY Q1 TESTŮ (14.08., `diag_q1_decisions_20260814.cpp`)

Postavené pozice, 120 opakování na rameno, plná produkční konfigurace
(MCTS-100, vf_blend 0,15, policy načtená). **Tři jednoznačné odpovědi za
patnáct minut** — a jedna z nich vyvrací můj vlastní zápis z téhož dne.

| # | otázka | výsledek |
|---|---|---|
| **1** | Slayer mezi **Black Orkem ST4** a linemanem — vybere BO, když ho nabídneme? | **0 ze 112** ⛔ *(a 0 z 84 bez nabídky)* |
| **2** | Longbeard mezi jejich **NOSIČEM** a stejným linemanem — všímá si, kdo drží míč? | **119 ze 120 = NOSIČ** ✅ |
| **3** | náš **LONGBEARD** drží míč, vedle volný Runner — předá? | **0 ze 120** ⛔ |

## Co z toho plyne

* **P13 sama nestačí** — nabídka stoupla (84 → 112 zvolených bloků), ale **cíl
  se nezměnil ani jednou**. Noční A/B by měřilo vedlejší efekt. ⇒ **Běh 14.08.
  zastaven po 5 minutách místo po 14 hodinách.**
* ⛔ **P15 SE ČÁSTEČNĚ VYVRACÍ.** Zapsal jsem odpoledne, že rozhodovací vrstva
  cenu cíle nezná vůbec. **Nabídka ji nezná, ale SEARCH ANO** — přes tři členy
  listové evaluace o nosiči si ho vybere prakticky vždycky. Sedí to na Fableho
  závěr, že P15 z orčí mezery bere skoro nic. ⇒ **Chybí hodnota SÍLY cíle, ne
  hodnota cíle obecně.**
* **P5 je mrtvé na úrovni VOLBY, ne nabídky.** Situace, která ve 3000 hrách
  nenastala, tady nastane vždy — a policy předání přesto nikdy nezvolí.
  Unit test dokazuje, že se **nabízí**. ⇒ Zbytek řetězu.

## ⛔⛔ TÁŽ VÝHRADA SE HNED VYPLNILA — SWEEP PŘES 36 GEOMETRIÍ OBRÁTIL DVA ZE TŘÍ

Napsal jsem k tomu výhradu *„jedna pozice, průkazné pro ni, ne pro všechny —
obměnit geometrii"* — a **rozhodl jsem podle toho dřív, než se obměnila.**
`diag_q1_sweep_20260814.cpp`, 36 geometrií × 12 seedů = **432 pozic na rameno**
*(obměna: vzdálenost nosiče, 0/1/2 asistence, kolo 3 vs 7, střed vs u lajny)*:

| scénář | jedna pozice | **36 geometrií** |
|---|---|---|
| **Dauntless — vybere Black Orka?** | **0 %** | **76,8 % → 83,3 %** *(OFF → ON)* |
| nosič | 99 % | **98,5 %** ✅ potvrzeno |
| **hand-off** | **0 %** | **18,3 %** |

⇒ **Ta postavená pozice byla PATOLOGICKÁ** — jedna z mála geometrií, kde lineman
vyhraje vždy. Přes 36 geometrií je obrázek opačný: policy si Black Orka bere
v **76,8 %** už dnes a zapnutá nabídka to zvedne na **83,3 %**, plus 29 akcí
navíc (228 → 257).
⇒ **Rameno chování MĚNÍ. Zabití nočního běhu 14.08. bylo předčasné a běh byl
znovu spuštěn** (15:15 UTC, HEAD `1dc9ecd2`).
⇒ **Hand-off taky není mrtvý** — 18,3 % napříč geometriemi. V 3000 hrách
nenastal proto, že situace „Longbeard nese a vedle je volný Runner" je vzácná,
ne proto, že by ji policy odmítala.

## ⛔ A DRUHÁ VADA Q1 NÁSTROJE: VIDÍ JEN VOLBU CÍLE, NE KVALITU BLOKU
*(uživatel 14.08.: „s 1 asistencí je to na 1 kostku a s Dauntless na 2 —
a s 0 asistencemi to taky není k zahození")*

Rozpad podle asistencí (Slayer ST3 vs Black Orc ST4+Guard):

| asistence | bez Dauntless | s Dauntless | volba cíle (OFF → ON) |
|---|---|---|---|
| **0** | ST3 v ST4 ⇒ **2 kostky PROTI nám** | srovná ⇒ **1 kostka** | 0 % → 0 % |
| **1** | 4 v 4 ⇒ **1 kostka** | 5 v 4 ⇒ **2 kostky PRO nás** | 100 % → 100 % |
| **2** | 5 v 4 ⇒ 2 kostky | 6 v 4 ⇒ 2 kostky *(strop)* | 76,3 % → 93,9 % |

⇒ **Při jedné asistenci povyšuje Dauntless 1 kostku na 2 — a Q1 test to NEVIDÍ**,
protože cíl je zvolený 100 % v obou ramenech. Test měřil *„přesunula se volba?"*
a odpověděl *„ne"*; správná odpověď je *„volba ne, ale kostky ano"*.
⇒ Při nule asistencí se z **dvou proti nám** stane **jedna** — taky zisk, jen si
ho search nevybere, protože vedle je lineman na jednu kostku bez rizika.

⛔ **Q1 test tedy Dauntless PODHODNOCUJE.** Vidí přesun mezi cíli, ne zlepšení
téhož cíle. **Skoro celý zisk leží v Q2** — a ten měří jedině A/B na výsledek.
⇒ **To je důvod, proč noční běh po druhé úvaze zůstal spuštěný.**

⭐⭐ **POUČENÍ, KTERÉ PLATÍ NA CELÝ Q1 NÁSTROJ:** *jedna postavená pozice je
vzorek o velikosti jedna.* Q1 test je levný právě proto, že se dá pustit přes
desítky geometrií — a **bez toho je horší než žádný**, protože vypadá
přesvědčivě. **Nikdy nerozhodovat podle jedné pozice.**

# ⭐⭐⭐ Q1 vs Q2 — DVĚ OTÁZKY, KTERÉ SPLÝVAJÍ A NESMÍ
*(uživatel 14.08.: „mám DTS vedle BO a LO a v A/B jej donutím v jedné z větví
vybrat BO a ve druhé náhodně — pak ale změřím, že vybral BO vs vybral náhodně —
nevidím přidanou hodnotu, ale ani nevím, co chci tím dokázat")*

**Ta pochybnost je správná: vynutit volbu a pak měřit, že se ta volba stala, je
měření DODRŽOVÁNÍ, ne měření HODNOTY.** Přesně to udělala brána klece — zlepšila
skoro všechny kontroly a chess se nehnul. Je to **otevřená otázka č. 1 vyslovená
na úrovni návrhu experimentu.**

| | **Q1 — VYBERE SI TO SÁM?** | **Q2 — JE TO VŮBEC LEPŠÍ?** |
|---|---|---|
| **vynucuje se volba?** | ⛔ **NE** — policy je volná | ✅ **ANO** — jedna větev cíl vynutí |
| **co se měří** | **podíl voleb** proti náhodě mezi nabídnutými | **výsledek** — chess, měna drivů |
| **na co odpovídá** | stačí **nabídnout**, nebo to spolkne plochý prior? | vyplatí se ta **doktrína**? |
| **cena** | minuty *(postavená pozice)* | noc |
| **čemu slouží** | **diagnostika** — aby se nespustila zbytečná noc | **hodnota** — kvůli tomu se rozhoduje |

⛔ **Nejčastější chyba je Q1 řešené nástrojem pro Q2:** vynutit volbu a změřit,
že se stala. Vyjde to vždycky a nedokazuje to nic.

⭐ **Q2 je zajímavější experiment než dnešní Dauntless**, protože netestuje
instalatérskou opravu, ale **doktrínu**: *vyplatí se cílit bloky na nejsilnější
tělo u nosiče?* Odpověď platí bez ohledu na to, čím se toho dosáhne — Dauntless,
prior, nebo něco třetího. **A kdyby vyšla záporně, ušetří naráz P13, P15
i balík proti Black Orkovi.**

⇒ **U každé položky fronty napsat, jestli je to Q1, nebo Q2**, a podle toho
zvolit nástroj i metriku.

# ⭐⭐⭐ NÁSTROJ PRO Q1: TEST ROZHODNUTÍ NAD POSTAVENOU POZICÍ
*(uživatel 14.08.: „pokud to nevynutíš podstrčením situace — kdy má na výběr —
a porovnáváš náhodu vs vybrání"; „dal jsem při bloku s Dauntless vědomě cíl ST4
proti náhodně — je pro mne měřitelné")*

**Neměřit výsledek zápasu, ale ROZHODNUTÍ.** Postavit pozici, kde volba
existuje, a spočítat, co si policy vybere — proti **náhodě mezi nabídnutými
možnostmi** jako nulové hypotéze.

| problém celozápasového A/B | co s ním udělá test rozhodnutí |
|---|---|
| **příležitost je vzácná** *(„bloků s Dauntless na Black Orka není mnoho")* | situaci **podstrčíme**, kolik potřebujeme |
| **pět vypnutých úrovní v řetězu** | měří **jednu** úroveň izolovaně |
| **14 h na odpověď** | minuty |
| **efekt pod rozlišením chess** | měří se **podíl voleb**, ne výsledek |

**Vyhodnocení:** podíl zvolení cíle ST4 proti podílu, který by dala náhoda mezi
nabídnutými cíli. 1:1 ⇒ search si toho cíle nevšímá a je jedno, co dělá zbytek.

⇒ **Platí na CELOU frontu A/B, ne jen na Dauntless.** Tutéž vadu mají
**P2+P9c** *(vybere si policy pollutera?)*, **roh vs. zeď** *(vybere si blitz
do zdi?)* i **P10a** *(vybere sražení nosiče, když scramble vyhraje?)*.
Všechny tři měří výsledek zápasu tam, kde je otázka o **jednom rozhodnutí**.

⚠️ **Dnešní čítače tohle NENAHRAZUJÍ.** Ověřeno probem 14.08.: počítají
vyhodnocení **uvnitř MCTS** (1829 „ocenění" a 368 hodů na hru — search si každý
tah probírá tisíce variant), a čítač hodů navíc **míchá obě ramena**. Jako
pojistka *„je ta větev živá?"* stačí; jako odpověď *„vybere si ten cíl?"* ne.

# ⛔ METODA: NEJDŘÍV POJISTKA MECHANISMU, TEPRVE PAK MĚŘENÍ
*(uživatel 14.08.: „když jdeme měřit Dauntless — naučili jsme se jej nabízet
a logovat předtím? tohle zobecnit")*

**Rameno, které nic nezměnilo, se MUSÍ dát poznat od změny, která nemá efekt.**
Bez toho se nula čte jako verdikt, přestože je to porucha měřidla.

**Dvakrát za jeden den to málem chybělo:**
* **Hand-off** — oprava ceny shipnuta, ověřovací běh hlásil **„0 hand-offů ve
  3000 hrách"**. `HAND_OFF` v enumu událostí **neexistoval**; počítal se řetězec,
  který engine nikdy neemitoval. *(Doplněno `3b11d33`.)*
* **Dauntless** — řádek A/B nesl jen `cand_plans`, což je diagnostika **brány
  klece** a v mode 4 je nula. Gate analyzer přitom takovou pojistku **měl**
  (*„BRÁNA NEADOPTOVALA ANI JEDEN PLÁN — měření neměří bránu"*).
  *(Doplněn čítač `cand_daunt` do řádku i do vyhodnocení.)*

## Povinný postup před KAŽDÝM A/B

| # | krok |
|---|---|
| **1** | **Je akce vůbec v nabídce?** Filtr, který ji zahodí, znamená, že search nedostane co vážit. |
| **2** | **Nechává po sobě stopu v datech?** Vlastní událost nebo čítač — ne odvozování z něčeho jiného. |
| **3** | **Nese tu stopu TÁŽ data, ze kterých se čte verdikt?** Korpus vedle A/B nestačí: když spadne noc, zůstane verdikt bez pojistky. |
| **4** | **Vyhodnocení tiskne „rameno nic nezměnilo" jako VLASTNÍ hlášku**, ne jako nulovou deltu. |

⚠️ **Krok 3 je ten, na který se zapomíná.** Dauntless by se dal spočítat
z `SKILL_USED` v korpusu — jenže korpus běží **až po** A/B, takže verdikt by se
četl bez něj.

# ⭐ METODA: VÝBĚR MATCHUPŮ SE DĚLÁ PODLE MECHANISMU
*(uživatel 14.08.: „tohle jsou specifická měření a moje zobecnění na všechny rasy
by tu nic nepřineslo")*

Dvě pravidla, která vypadají protichůdně a nejsou — **jsou to dva různé kroky**:

| krok | pravidlo |
|---|---|
| **zadání běhu** | ⭐ **Měřit jen tam, kde mechanismus MŮŽE vyskočit.** Jinak se platí hodinami stroje za zaručené „nerozhodnuto". |
| **vyhodnocení** | ⭐ **Číst per-matchup vždycky.** Průměr přes čtyři soupeře míchá dva opačné režimy *(viz `matchup_asymmetry_20260814.md`)*. |

**Postup u každého dalšího A/B:**
1. Spočítat **expozici mechanismu per matchup** *(u Dauntless: kolik cílů je
   silnějších než náš blokující a s jakou šancí — ork 1,00 · human 0,20 ·
   wood-elf 0,15 · **skaven 0**)*.
2. Vzít **matchup s nejvyšší expozicí** jako otázku.
3. Vzít **matchup s expozicí NULA** jako pravý null — je to silnější kontrola
   než matchup se slabou expozicí, protože tam delta **musí** být nula.
4. Zbytek **neběžet**, a do předregistrace napsat **proč** *(kolik párů by na
   jejich efekt bylo potřeba)*.

⚠️ **Nezaměňovat s „běž vždycky totéž".** Právě to se dělalo dosud: každé A/B
tohoto projektu jelo na `dw-sk` + `dw-we`, tedy proti dvěma **rychlým** týmům —
a ork s humanem, naše dvě nejhorší utkání, **nebyly nikdy v žádném rameni**.

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

⛔⛔ **POZASTAVENO 17.08. — TATO TABULKA NESMÍ DÁL ŘADIT FRONTU** (P25).
Doplnil jsem do nástroje tisk původu a vyšlo najevo, na čem stojí:
**120 her · 195 „plných" drivů · jen 35 drivů se skórováním · 11 veličin
porovnávaných naráz bez korekce · filtr „plné drivy" je VÝBĚR, ne vzorek ·
engine nejméně o šest commitů starší než dnešní.**
A hlavně: **na jiném korpusu téže velikosti (`20260813_gate`, 194 drivů)
nevychází stejně** — bloky 2,7σ → **5,6σ**, K33 0,6σ → **4,0σ**,
REACH0 −1,8σ → **−4,0σ**. Z čísla v knize se přitom nedalo poznat ani to,
z jakého korpusu je.
⇒ **Přepočítat na `corpus_baseline_20260817`** (3 000 her, s otiskem enginu;
sbírá se od 17.08.) a teprve pak o pořadí mluvit.
*(Detail `evidence/instrument_audit_20260817.md`.)*

Plné drivy ≥7 kol, 195 drivů *(korpus `20260811b`, 120 her — doplněno zpětně)*:

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

# ⭐⭐⭐ PROGRAM: TRPASLIČÍ ATTRITION — ZÁKLAD HRY, ROZBITÝ NA KAŽDÉM ČLÁNKU
*(uživatel 14.08.: „toto je základ trpasličí hry, to musíme dotáhnout")*

Trpaslík nevyhrává během, vyhrává tím, že soupeře **odstraní z hřiště**.
Všechno ostatní — klec, rohy, tempo — je podpora. Řetěz té hry má sedm článků
a **14.08. se ukázalo, že vadný je každý**:

| # | článek | co je špatně | položka | stav |
|---|---|---|---|---|
| **1** | **vyber cíl** | hodnota cíle = **vzácnost × role** není nikde; nabídka ji nezná, prior je plochý `BLOCK 15` | **P15 · P19** | otevřeno |
| **2** | **získej kostky** | `Dauntless` chybí v nabídce; s 1 asistencí povyšuje 1 kostku na 2 | **P13** | ⏳ **měří se dnes v noci** |
| **3** | **vyhraj blok** | volba kostky ani reroll neznají `Wrestle`; hráč s Block+Wrestle Wrestle nikdy nepoužije | **P14 · P17** | otevřeno |
| **4** | **odsuň užitečně** | směr se vybírá naslepo („rovně dozadu"), nezná endzonu, náš roh ani soupeře vedle | **P9 · P9a · P9c** | P9a ✅ *(15 → 2 darovaných TD)* |
| **5** | **prolom brnění** | `Mighty Blow` se přičítá k **oběma** hodům místo k jednomu | **T5.14** | odloženo k rosterům |
| **6** | **faulni to správné tělo** | bere **prvního ležícího v pořadí sousedních polí**, nehodnotí nic | **P8** | otevřeno |
| **7** | ✅ **udrž ho mimo hřiště** | ~~zranění nepřetrvávají přes drive~~ — **NEPLATÍ, opraveno 10.08.** (`918fc589`); 0 návratů ze 648 casualty za 600 her | **T5.3** | **hotovo** |

## ⛔⛔ RÁMEC O NÁSOBITELI PADL *(17.08.)*

Stálo tu, že dokud se po každém TD staví jedenáct čerstvých, je odstraněné tělo
pryč jen do konce drivu, takže **každá oprava článků 1–6 se měří v prostředí,
které jejich přínos dělí**. **Není to pravda a nebyla už od 10.08.**

Ověřeno na datech: **0 ze 648 hráčů s `CASUALTY` se za 600 her vrátilo na
hřiště.** Balík G to opravil commitem `918fc589`; kniha to jen nezaznamenala.
Návrat soupisky po TD je **návrat KO**, což pravidla dovolují (4+ na kick-offu).

⇒ **Attrition čísla NEJSOU systematicky podhodnocená** a výsledky o bití
**nejsou z tohohle důvodu dolní odhad**. Argument o dělení článkem 7, který
jsem 17.08. dvakrát použil (u P8 i u faulu na Black Orka), **neplatí** —
u P8 rozhodl strop 0,056 faulu na zápas sám o sobě.

⚠️ **Poučení, ne úleva:** tvrzení žilo v knize sedm dní po své opravě a řadilo
podle sebe priority. **Stav položky se musí ověřovat proti kódu a datům, ne
proti tomu, co je zapsané** — táž rodina jako P25.

## Proč to nikdo nenašel dřív

Bití bylo **systematicky podhodnocené ve všech třech vrstvách naráz**:
* **kontroly** — Fable 14.08.: čistota rohů 0σ, tempo z 58 % artefakt; **bloky/kolo
  jsou jeden ze dvou prediktorů, které kontrolu proveditelnosti PŘEŽILY (+2,8σ)**
* **nástroje** — Q1 test vidí jen volbu cíle, ne kvalitu bloku
* **engine** — zranění mizí s drivem, MB se počítá dvakrát, tři dovednosti chybí
  v rozhodování

# CO JE TEĎ PRVNÍ

*(jediný oddíl, který se přepisuje — přepsáno 20.08.2026 ráno)*

## ⭐ PRAVIDLO DNE: STROJ ZŮSTÁVÁ NA KLECI

*(uživatel 20.08.: „stroj pokračuje v práci na kleci")* — a podepřeno
měřením `diag_where_lost_20260820.py`: **v 89,9 % nevyhraných zápasů máme
nulu**, a *„dali jsme, ale míň"* je **11 zápasů ze 3 000**. Přestřelku
neprohrajeme prakticky nikdy. ⇒ **Obrana je ROZHOVOR, ne strojová práce.**

## Pořadí na dnešek

| # | co | proč teď | blokuje |
|---|---|---|---|
| **1** | ✅ **HOTOVO** — T2.17 oprava summarizeru *(dvoustranná vs jednostranná delta)* | ⭐ **oprava kontroly se povyšuje před měření** — jinak dnešní noc přečteme týmž zkresleným metrem jako včerejší | pre-reg noci |
| **2** | ✅ **HOTOVO** — P40 placebo rameno (mode 7), testy 555 → 559 | jediný funkční rozdíl je `macro_actions.cpp:1403`; Fable vypsal i ostatní místa *(setter za ř. 178, podmínka ř. 1377, harness ř. 244–249 seedy 127M, 251–257, 277, ~400–406, 440–441)* | noc |
| **3** | ✅ **HOTOVO** — `evidence/night_prereg_20260820.preds` | placebo **+0,090** [+0,04; +0,14] · rozdíl placebo−P38 **0,00** [−0,04; +0,05] · **placebo musí mít VÍC picků než P38** · leak == 0 | noc |
| **4** | ✅ **noc SPUŠTĚNA 10:52 SELČ** — `placebo_20260820`, mode 7, 8×850 párů, práh ±0,015, `CONTROL_MODE2=1`, preflight OK | konec ~01:00–04:00 | — |
| **5** | *(za běhu noci)* **Q6 — v čem se měří „převaha"** | rozhovor, stroj nestojí; **blokuje T1.11** | T1.11 |
| **6** | ✅ **HOTOVO** — T0.1 K9 po fázích; přepis jako kontrola neuspěl (16,3σ vs 20,8σ), ale vypadl z toho **P41: fáze VÝBĚH 2,9 %** | — | — |

## ⭐⭐⭐ POLOŽKY SE ŘADÍ PODLE DVOU CÍLŮ KLECE *(uživatel 20.08.)*

Spec **15.0b′**: klec má **dvě položky — dojít co nejdál · ochránit nosiče**
([[project_bloodbowl_cage_two_goals_20260820]]). Otevřené položky se proto
vedou pod nimi, ne podle pořadí vzniku:

### 🛡️ OCHRÁNIT NOSIČE *(tady je 70 % ztrát míče)*

| | co | proč sem patří |
|---|---|---|
| ⏰⏰ **P45** | ležící smí BLOCK + chybí hod 4+ | ⭐ *(uživatel 20.08.: „do ochránit nosiče patří i opravit darovaný jumpup ze dneška")* — **není to parity úklid, je to největší jednotlivá vada ochrany**: dokud dáváme prakticky Jump Up každému, je **každý ležící soused blokující zdarma**, a to ruší přesně to, co klec dělá |
| ⏰⏰ **P37** | `carrierIsBlitzable` nezná GFI | blitz na nosiče = **70,1 %** ztrát, a tahle funkce rozhoduje o záloze pohybu |
| ⏰ **P44** | follow-up je povinný | blokuje **dvojroli blitzujícího jako rohu** ⇒ rozpočet 6 ze 7 → 5 ze 7 |
| ⏰ **P42** | zákaz „nosič nekončí v kontaktu" | kontrola **K38 stojí**, chybí rameno |

### 🏃 DOJÍT CO NEJDÁL

| | co | |
|---|---|---|
| ⏸ **P32** | klec se posouvá **jen rovně vpřed**, `y` se nikdy nemění | ⭐ **povýšeno 20.08.**: není to vada posunu, ale **chybějící polovina pravidla** ze spec 15.0b‴ *(„tvar pevný, cíl nosiče vpřed A DO BOKU“)*. ⛔ **POVÝŠENÍ ODVOLÁNO 20.08. — je to MRTVÝ KÓD.** `plán: NOT_CONSULTED` ve **100 %** ze 44 092 našich kol: plánovač klece (`cage_advance.cpp`), kde ta vada sedí, se **nikdy nespustí**, a brána, která by ho zapnula, byla 18.08. **zamítnuta** (−3,7σ). ⇒ Oprava by dnes **nezměnila nic**. ⚠️ Povýšil jsem ho, protože jsem doktrinální mezeru přečetl správně, ale **neověřil, kde ta mezera doopravdy je** — podruhé týž den míření na mrtvý kód. **Viz P46.** |
| ⏰⏰ **P46** | ⭐⭐⭐ **TĚLA KLECE NENÁSLEDUJÍ NOSIČE DO BOKU — a je to v ŽIVÉM kódu** *(20.08.)* | ⭐ **Uživatelova formulace vady, přesnější než moje:** *„máme v enginu cíl — zavazet — a proti tomu klec, co se neumí hnout do strany."* Není to souběh dvou vad, je to **jedna past**: obě strany sdílí `expandReposition`, takže **soupeř zavazí podle NAŠEHO vlastního repertoáru**, a nosič, který umí jen rovně, do toho vjede.<br>⚠️ A soupeřův screen stojí na natvrdo daných **`y ∈ {3,5,7,9,11}`** *(T1.12)* — je to **hřeben s dírou v každé druhé řadě**. Kdo umí uhnout, projde; kdo jede po přímce, narazí do zubu.<br>⭐ **Kde ta mezera doopravdy je:** rameno P38/P40 už **2D hledá, ale jen pro NOSIČE**. Chybí, aby **těla klece následovala jeho cíl ve tvaru** *(spec 15.0b‴: „tvar pevný, cíl vpřed a do boku")* — a to je v **`expandReposition`**, tedy v témže místě jako natvrdo daná `y` v obraně.<br>✅ **ZMĚŘENO 20.08.** *(`diag_lane_blocked_20260820.py`, 3 000 her)*: u nás je rovně vpřed **zablokováno v 61,2 %** kol s míčem, a **ve 43,7 % z nich je do boku VOLNO** ⇒ **26,7 % všech našich kol s míčem** by uhnutí rozhodovalo. ⭐ **Uživatelův odhad, že ztrácíme hodně, POTVRZEN.**<br>⛔ **Druhá půlka odhadu (hbití elfové ještě víc) se NEPOTVRDILA a není ani pořádně otestovaná:** soupeř vyšel na **25,5 %**, tedy o kousek **míň** než my. ⚠️ Dvě meze: (1) soupeř jsou tu **všechny nedvarfské rasy dohromady**, ne jen elf; (2) metrika měří **PŘÍLEŽITOST, ne ZTRÁTU** — elf s vyšším MA může z téže příležitosti ztrácet víc i míň. ⇒ **Rozpad po rasách přidat k P41.** | **OTEVŘENO — ⏰⏰ nahrazuje P32 v živém kódu** |
| 🔄 **P40** | placebo mode 7 | **běží v noci** |
| ⛔ **P41** | ~~fáze VÝBĚH 2,9 %~~ | ⛔⛔ **STAŽENO TÝŽ DEN — byl to ARTEFAKT KONTROLY.** `diag_rules_checks` vyhazoval hned na začátku smyčky **kola, která skončila TD** *(`if S.get("touchdown") … continue`)* ⇒ K9c měřila *„z kol, kdy jsme byli u endzony a NESKÓROVALI, jak často jsme postoupili dost"*. Po opravě: **VÝBĚH 69,3 %** (n=1308), tedy **NEJLEPŠÍ fáze, dvojnásobně** proti SÓLO 34,8 % a KLEC 34,0 %. ⭐ Dopadlo to i na **K9a**: 23,7 % → **27,6 %**. Doklad `evidence/td_excluded_from_checks_20260820.md`. | **ZAMÍTNUTO 20.08.** |
| ⏰ **P47** | **KDE JE HRANICE „skórovat hned vs posunout klec"** | ⭐ Spec **15.0d** dala **obě větve**, ne hranici *(uživatel 20.08.: „nemyslím si, že na otázku hranice umím odpovědět" — a nemá; **je to dopočet, ne doktrína**)*.<br>**Co uživatel dodal a změřit nejde:** čím se to **váží** *(rychlost a Leap, ne počet)* a **proti čemu se porovnává** *(ztráta míče, ne jistota)*.<br>**Postup:** vzít kola s nosičem na dosah endzony, ke každému spočítat **cenu v kostkách** *(dodge + GFI)* a **vážený dosah soupeře** *(max MA těch, kdo dosáhnou, + Leap)*, a proti tomu postavit **výsledek drivu**.<br>⚠️ **Past, kterou už teď vidím:** engine **nezdržuje schválně**, takže v korpusu nemusí být dost případů „držel a skóroval později" ⇒ **vyhladovělá metrika** jako u sloupců. Když to tak dopadne, **je to výsledek** a hranice bude potřebovat cílený běh, ne korpus. <br>⭐ **ZOBECNĚNO 25.08. → `P59` (HORIZONT):** týž dopočet platí obecně, ne jen u téhle hranice — a **přibyl kanál PŘIHRÁVKY**, který tu chyběl.<br><br>⭐⭐⭐ **A 25.08. DOŠLA I SAMA HRANICE — uživatel dodal dopočet, který tu chyběl od 20.08.** *(„rychlí proti pomalému nemusí držet až do 6. kola — proti trpaslíkům může klidně dát TD ve 4. kole, a spočítáš, že trpaslík nemá ve 4 kolech šanci dát TD zpátky")*.<br>**Pravidlo:** *„drž a nemel do konce půle"* **NENÍ univerzální** — je podmíněné tím, jestli soupeř **stihne odpovědět**. A to je **jeho `required_pace`**: vzdálenost jeho k naší endzóně proti zbývajícím kolům a MA jeho nejrychlejšího těla. **Když nestihne, skóruj hned a ber si další drive.**<br>⇒ **Je to TŘETÍ větev téhož dopočtu:** `P58` kdy vyrazit · `P60` co dělat za rozvrhem · `P47` kdy skórovat a kdy zdržet. Všechny tři čtou tutéž trojici *(pole · kola · MA)*, jen jednou u sebe a dvakrát u soupeře.<br>⚠️ **Změřeno 25.08., a doktrínu to NEPOTVRZUJE — jen nevylučuje** *(3 750 her)*: proti pomalému skórují všichni dřív *(skaven 46,7 % vs 37,7 % v kole 2-5, human 40,8 vs 26,5, trpaslík 22,4 vs 12,9; wood-elf skoro nerozlišuje: 42,0 vs 40,2)*. ⛔ **Ale proti pomalému je menší odpor, takže drivy postupují rychleji tak jako tak** — táž data by vznikla bez jakéhokoli záměru. A protože **žádný kód nepočítá, jestli soupeř stihne odpovědět**, záměr je vyloučený: je to **příležitost, ne plán**. *(Táž past jako „korelace přes stav partie" u kontaktu a sloupců.)*<br>⭐ **A goblin hozený trolem** *(uživatel: „tady je pak zajímavé dát TD goblinem hozeným trolem — ale menší pravděpodobnost")*.<br>&nbsp;&nbsp;⚠️ **UPŘESNĚNO — napsal jsem to nejdřív jako obecné tvrzení a je to omezení NAŠÍ TV1200 sestavy** *(uživatel: „TTM má troll u orků taky — human Ogre teď nemá kým házet")*. **V ZÁKLADNÍM rosteru ork ten kanál má CELÝ:** `roster.cpp:47` **4× Goblin** MA6 ST2 AG3 AV7 `Dodge+RightStuff+Stunty`, `:50-53` **1× Troll** MA4 **ST5** AG1 AV9 `ThrowTeamMate` *(+Loner, AlwaysHungry, ReallyStupid, Regeneration)*.<br>&nbsp;&nbsp;⛔ **V TV1200 ale ne:** ork má *(`roster.cpp:499`, „goblins removed")* jen 11 Linemanů, 4 Blitzery, 4 Black Orky a Throwera — **ani troll, ani goblini**. A **human Ogre `ThrowTeamMate` má, ale nemá koho házet** — v human TV1200 nemá `RightStuff` nikdo. ⇒ **kde je házeč, chybí náklad.**<br>&nbsp;&nbsp;⇒ ⭐ **Kanál tedy chybí ze TŘÍ důvodů, a jen jeden je enginový:** *(1)* sestava nemá goblina · *(2)* sestava nemá trolla · *(3)* **plánovač TTM neumí** *(žádné makro, `A4` v M-ROUND)*. **Body (1) a (2) spraví `T5.13`** — a v ten okamžik se *(3)* stane živým, ne latentním. | **OTEVŘENO — 🏃 dojít co nejdál** |
| ⏰ **P39** | nosič se neaktivuje | P40 to možná rozhodne |

⚠️ **Ostatní otevřené položky** *(z 63)* se tím **nemažou ani nezamítají** —
jen nejsou na řadě, dokud klec nedělá to, co má.

## ⏰ Co je na řadě PO noci P40

1. **Q8** — nasadit P38? *(podle výsledku P40)*
2. ⏰⏰ **P37** — `carrierIsBlitzable` nezná GFI. **Povýšeno 20.08.**, protože **blitz na nosiče je hlavní příčina ztráty míče** (70,1 % ztrát) a tahle funkce rozhoduje, jestli si nosič nechá pohyb v záloze. Levné, lokální, s vlastním ramenem měřitelné.
3. **P41** — fáze VÝBĚH 2,9 %; první krok je **rozpad podle `turns_left`**, ne oprava.
4. **P39** — nosič se neaktivuje *(P40 to možná rozhodne za nás)*.

## ✅ P45 PRAVIDLOVÁ ČÁST HOTOVA 21.08. — testy 566/566

**Opraveno** (`move_handler.cpp` · `rules_engine.cpp` · `game_event.h`):
1. **hod 4+ pod 3 MA** — dosud `fail()` ⇒ MA 2 se nepostavil NIKDY;
   po úspěchu `movementRemaining = 0`, takže **další krok je GFI**;
   **neúspěch NENÍ turnover** (ř. 694-695).
2. **vstávání nastavuje `hasMoved`** — je to pohyb ⇒ uzávěrka aktivace se
   spustí a **po vstání už nejde Block** (ř. 675).
3. **BLOCK se nenabízí hráči, který se pohnul** — pohyb+blok **je blitz**
   a ten je jeden za kolo *(uživatel 21.08.)*. Míří na 2 641 nadbytečných
   případů (2,61 % kol).
4. ⭐ **nový event `STAND_UP`** — vstávání dosud **neemitovalo nic**, takže
   se v korpusu nedalo odlišit *„nikdo nevstává"* od *„vstávání se neloguje"*.
   Append-only na konec enumu, staré korpusy se nepřejmenují.

**Testy:** 6 nových (`StandUp.*`) + 2 přepsané legacy *(starý
`StandUpNotEnoughMA` pinoval to špatné chování)*. Před opravou padaly 4 ze 6.

⛔ **ZBÝVÁ: plánovač nikoho nepostaví.** `turn_planner.cpp` slovo `PRONE`
neobsahuje ani jednou; vstávání je zakódované jako pohyb na vlastní pole,
tedy nulový postup, a plánovač takovou akci nikdy nevybere.
⇒ **Bez toho je pravidlová oprava neúčinná** — měřeno 0,4 % (1 067
z 280 719 ležících).

### ⭐⭐ Q3 — KDY VSTÁVAT VEDLE SOUPEŘE *(uživatel 21.08.: „nejtěžší na vyhodnocení")*

*„Když jen vstanu vedle někoho, tak vstanu, abych dostal ránu — to chce nějak
kvantifikovat, protože někdy chci záměrně, ať tam zavazí, a jindy ne."*
⭐ *„A to se bavíme o pohybu — jednodušší akci."*

**Dvě protichůdné položky, obě měřitelné:**
* ⛔ **cena** — ⭐⭐⭐ vstáním se sám převedu z *„stojím soupeře BLITZ"* na
  *„dávám mu BLOK ZDARMA"* *(pravidlo z 20.08.)*, a když mě složí, ležím
  zpátky a zaplatil jsem 3 MA;
* ✅ **zisk** — stojící tělo má **TZ**: zdražuje odchod o dodge, dává
  **asistenci** našim blokům a **obsazuje pole**.
⇒ **Vstát vedle soupeře je DRAŽŠÍ než zůstat ležet**, a zisk to musí přebít.
⚠️ **Nestavět plánovač dřív, než se tohle změří.**

### ⭐⭐⭐ Q3 ZODPOVĚZENO 21.08. — TŘI VĚTVE, A ROZHODUJE CENA NA MNĚ

Uživatel 21.08.: *„zůstat ležet — riskuju faul — jsem dost levný, abych
soupeři nestál za faul, a je ten, vedle koho bych se postavil, silný a např.
i s MB, a mám riskantní dodge, na kterém bych nejspíš upadl a mohl se hned
zranit?"*

⇒ **První diskriminátor NENÍ „kam to tělo patří", ale CENA TŘÍ MOŽNOSTÍ PRO
TOHO HRÁČE.** Rozpočet těl *(„je potřeba jinde")* je až přebíjející člen —
musí porazit všechny tři ceny, ne je nahradit.

| větev | co mě to stojí |
|---|---|
| **zůstat ležet** | **FAUL** — vážený tím, jestli za faul vůbec stojím |
| **vstát a zůstat** | soused mě **udeří zadarmo**; váha = jeho ST, asistence, **Mighty Blow / Claw** |
| **vstát a dodgnout pryč** | **selhaný dodge** = padnu a hned mi jde armour/injury |

### ⛔⛔ A TÍM SE OPRAVUJE PRAVIDLO Z 20.08.

Pravidlo znělo: *„ležící soused ≡ vzdálený soupeř v dosahu: obojí stojí JEDEN
BLITZ"* — a z toho plynulo, že ležet je relativně bezpečné, protože blitz je
jeden za kolo.
⛔ **Platí to jen pro BLOK. Faul je OBYČEJNÁ AKCE, a ta scarcity se na něj
nevztahuje.** Ležící tělo tedy tou úvahou chráněné NENÍ.

**Změřeno (korpus 19.08., 500 her / 15 999 kol):**
* fauly: **3 428 = 6,86 na zápas**, jsou v **21,4 %** kol — ⛔ **není to
  okrajový jev**;
* soupeřových ležících na desce: 2,68/kolo ⇒ **7,98 % příležitostí skončí
  faulem**;
* **36,3 %** faulů prorazí AV (1 244 z 3 428).
⇒ Tělo, které proleží čtyři kola, má **~28 % šanci, že ho aspoň jednou
faulnou**. **Ležet není pasivní bezpečí, je to expozice.**

⚠️ **Měřeno na PŘEDřezovém korpusu** *(nikdo nevstával, takže ležících bylo
2,68/kolo; po opravě je jich ~1,42)*. Méně cílů může sazbu na cíl zvednout.
**Přeměřit na křížovém korpusu.**


### ⭐⭐ UPŘESNĚNÍ CENY 21.08. — uživatel

*„Já teď vstávám. A když nedodgnu, tak je cena toho, co bude vedle mě, až
vstanu, a praští mě. Když dodge provedu, je cena fail dodge — moje AG —
vždy dodge do 0 TZ."*

⭐ **Cena se počítá na stavu PO POSTAVENÍ, ne na tom, jak deska vypadá teď.**
* **vstát a zůstat** → cena = **blok od toho, kdo bude vedle mě, až budu stát**;
* **vstát a odejít** → cena = **jediný selhaný dodge**, a to je čistá funkce
  mého AG, protože ⭐⭐ **cíl dodge má VŽDY 0 TZ** *(tvrdé pravidlo uživatele)*
  ⇒ žádné postihy za TZ na cílovém poli, jeden hod, a pak jsem volný.

### ⇒ Tři důsledky, které z toho plynou (moje odvození, ne uživatelovo)

1. ✅ **OPRAVENO uživatelem 21.08. (odpověď na Q9): NENÍ to tvrdá podmínka.**
   *„Vždy dopočítat cenu dodge — obecně preferovat míň rizikové."*
   ⇒ ⛔ **Větev „odejít" se NIKDY neškrtá.** Když pole s nulovým TZ není,
   dodge se **ocení** do toho, co je, a porovná se s ostatními dvěma větvemi.
   „Do 0 TZ" je tedy **vyjádření preference nízkého rizika v běžném případě**,
   ne brána.
   ⭐⭐ **Tím je celé Q3 JEDNOTNÉ: tři větve, všechny OCENĚNÉ, vyhrává nejmíň
   riziková.** Žádný rozhodovací strom, žádné prahy — a to je přesně důvod,
   proč to uživatel „nerozepíše striktně". ⇒ **Tvar úlohy je POROVNÁNÍ CEN,
   a to je práce pro search, ne pro `if`.**
2. ⭐⭐ **Rozpočet pohybu tu větev skoro zavírá sám.** Postavení stojí **3 MA**,
   takže po něm zbývá **MA − 3**:
   * **trpaslík MA 4 → 1 pole.** Musí to trefit **na první krok**: pole přímo
     od souseda pryč *(vzdálenost 2 od něj = mimo jeho TZ)*, a musí být volné
     a nekryté nikým dalším. Jsou to **tři pole z osmi**.
   * **wood-elf MA 7-8 → 4-5 polí**, tedy skoro vždy se kam dostat.
   ⇒ **Tatáž doktrína je pro elfa levná a pro trpaslíka na hraně** — a NENÍ to
   rasové pravidlo, je to **rozpočet pohybu po zaplacení vstávání**.
3. ⛔ **Pod 3 MA je ta větev dvojitý hod:** postavení na **4+**, a protože pak
   `movementRemaining = 0`, je každý další krok **GFI**. Dodge + GFI = dva hody.
   *(Treeman MA 2 ⇒ „vstát a odejít" je prakticky mimo hru.)*

⚠️ **A pořád platí, že se to NEROZEPISUJE STRIKTNĚ** *(uživatel 21.08.)* —
tohle je popis CENY, ne rozhodovací strom. Rozhoduje search; naším úkolem je
dát mu ty členy vidět, ne mu předepsat výsledek.


### ⭐⭐⭐ A KDO TAM VŮBEC LEŽÍ — uživatel 21.08.

*„Nejsem nosič s míčem, abych se strachoval o budoucí blitz. A pokud jsem
cenný, tak nemám co dělat ležící ve skrumáži."*

**Dvě pravidla v jedné větě:**
1. ⭐ **Cena se počítá proti SOUSEDOVI, ne proti dosahu.** Dosah *(U1, hák
   lajny)* je starost **NOSIČE**. Obyčejné tělo řeší, kdo stojí vedle něj,
   až vstane — a hotovo. ⇒ **Q8 zodpovězeno: sousedství, ne dosah.**
2. ⭐⭐⭐ **Cenný hráč ležící ve skrumáži je chyba, která se stala DŘÍV.**
   Není to volba při vstávání — je to vada v tom, koho jsme tam poslali.
   ⇒ Rozhodování o vstávání se tím **zjednodušuje**: kdo tam leží, je z definice
   levný, takže větev *„jsem dost levný, abych nestál za faul"* je **default**,
   ne výpočet.

⚠️ ⇒ **A tím se otevírá otázka O ÚROVEŇ VÝŠ, která je měřitelná:**
**končí naši cenní hráči** *(Guard, vysoké ST, blodgeři)* **ležící ve
skrumáži?** Když ano, je to nález o **přidělování těl**, ne o vstávání — a týká
se to přímo klece a screenu *(rohy a výplně mají být levná těla)*.
**Změřit na křížovém korpusu.**

### ⚠️ NEKUPIT — rozhodnuto 21.08.

Uživatel: *„chceme jednu změnu a tu testovat, a nedávno jsme porušili
a spálili se."* Precedens je konkrétní: volba pole nosiče měla **tři** změny
naráz a stálo to noc navíc (placebo) plus Fableho rozbor.
⇒ **Bezpodmínečné vstávání se změří první.** *„Sražený vstane" jsou PRAVIDLA;
kdy vstát a zavazet je NAŠE DOKTRÍNA na ně nasazená* — a doktrína potřebuje
základní čáru, proti které se dá měřit.

## ⏰ P50 — KICK-OFF RETURN: ZAČÁTEK DRIVU SE BĚŽÍ, NE KLECUJE *(uživatel 21.08.)*

*„Pak v budoucnu máme nachystanou úpravu — kick off return — a ze začátku běž
rychle a postav zeď — screen a ne klec."*

### Číslo, které to podpírá (změřeno 21.08.)

Vzdálenost nosiče k EZ podle zbývajících kol *(korpus 19.08., 2 051 kol)*:
**20,8 → 18,0 → 16,4 → 15,2 → 14,2 → 13,2 → 11,7**
⇒ ⭐⭐ **trpaslík ujde ⌀ 1,5 pole za kolo, a potřebuje 3,46 už od prvního kola.**
**Nedostane se do skluzu během drivu — vychází z něj rovnou.**
⚠️ Ze starého korpusu; po opravě vstávání to bude nejspíš horší.

### ⛔⛔ PAST V METRU POŽADOVANÉHO TEMPA — NEPRŮMĚROVAT

`required = dist / max(1, turnsLeft − RESERVE_TURNS)`. V posledních **dvou**
kolech se dělitel usekne na 1, takže veličina **přestane být tempo** a stane
se z ní **zbývající vzdálenost**:

| zbývá kol | 7 | 6 | 5 | 4 | 3 | **2** | **1** |
|---|---|---|---|---|---|---|---|
| ⌀ required | 3,46 | 3,60 | 4,11 | 5,06 | 7,09 | **13,20** | **11,72** |
| dělitel | 6 | 5 | 4 | 3 | 2 | **1** | **1** |

⇒ Průměr přes všechna kola (**6,99**) je z velké části tohle a **čte se jako
„potřebujeme 7 polí za kolo", přitom to znamená „zbývá 13 polí a jedno kolo"**.
**Metr je čitelný jen po kolech a jen dokud je dělitel > 1.**
*(Uživatel to odchytil jako „magická čísla" hned, jak je uviděl.
⛔ Do kódu se zatím nesahá — jen se to nesmí průměrovat.)*

### ⭐⭐⭐ A TÍM SE MĚNÍ STATUS P48 (SCREEN)

P48 byl odložen úplně na konec s odůvodněním *„pomáhá ELFOVI, a elf už dnes
skóruje víc"*. **To odůvodnění tímhle padá:** screen tu chce **trpaslík**,
a na fázi, kde je nejpomalejší. ⇒ Screen **není elfí kapitola** — je to
**nástroj rychlé fáze pro každého**, přesně jako klec je nástroj kontaktní.
*(Táž oprava jako u U1 a u klece: pravidlo zúžené na rasu, u které se našlo.)*

### Co to je za tvar

**Fázová doktrína** — už **ČTVRTÁ** *(13.08. trasa · 18.08. T0.1 · 20.08.
obrana · dnes útok od kick-offu)* — a **engine pojem fáze nemá nikde**
*(viz [[project_bloodbowl_phase_model_missing_20260820]])*.
⇒ Bez fáze nejde odlišit *„nepostavil klec, protože běží"* od *„nepostavil
klec, protože ji neumí"*.
⚠️ **Přechod rychlá → kontaktní musí řídit měřitelná veličina**, ne konstanta
(zákaz konstanty, uživatel 20.08.). Kandidát je na to nově **`corridor_strength`**
*(přidáno dnes)* — zeď se přestane obíhat, až má sílu, ne až má hustotu.

### ✅ CO Z TOHO UŽ VE FRONTĚ JE — prověřeno 21.08. na uživatelův dotaz

| co | kde | stav |
|---|---|---|
| obrana jako fáze (D1 sloupce → D2 L) | **T1.11** | zapsáno, v kódu NENÍ nic |
| útočné fáze SÓLO/KLEC/VÝBĚH (podlaha) | **T0.1 / K9c** | metr hotov |
| klec se posouvá jen rovně, směr se nevolí | **P32** | ⛔ **NENASAZENO** |
| pole nosiče = boční volnost, +12,8σ | **P38** | ⛔ **NENASAZENO, čeká na Q8** |
| začátek drivu se běží + screen | **P50** | nové 21.08. |
| ⛔ **pojem FÁZE v enginu jako takový** | **NIKDE** | jen zápis v paměti |

⇒ ⭐ **„Běž na max dopředu a do boku" v plánech JE — rozdělené na P32 (směr)
a P38 (pole nosiče) — ale ANI JEDNO NENÍ ZAVEDENÉ.** Máme to změřené, ne
nasazené; P38 čeká přesně na víkendovou fázi B.

### 🔎 CHECK NA TŘETÍ FÁZI (VÝBĚH) — doplněno 21.08.

**Dnešní stav: opravovat nepotřebuje.** Nález *„VÝBĚH 2,9 %"* byl 20.08.
**stažen jako artefakt kontroly** *(`diag_rules_checks` vyhazoval kola, která
skončila TD)*; po opravě je **VÝBĚH 69,3 % — NEJLEPŠÍ fáze, dvojnásobně**
(SÓLO 34,8 · KLEC 34,0).

⚠️⚠️ **ALE TO ČÍSLO JE Z PŘEDŘEZOVÉHO ENGINU** *(nikdo nevstával)*, a výběh je
fáze, kterou **víc stojících obránců musí zasáhnout ze všech nejvíc** —
protrhnout se ven skrz dav je něco jiného než skrz ležící.
⇒ **CHECK: přeměřit podíl fází na křížovém korpusu a ověřit, že VÝBĚH
zůstal nejlepší.** Když spadne, je to nález, ne regrese měření — a mění
pořadí fází v doktríně.

## 📋 T2.20 — AUDIT TESTŮ *(zadání nachystáno 21.08., čeká na rules-parity)*

Uživatel 21.08.: *„a něco jako fable kontrola pokrytí a úplnosti testů
v projektu by nepomohlo?"*

⛔ **Ale NE jako pokrytí kódu.** Obě dnešní pravidlové vady **pokrytí měly
a testy prošly** — a ⭐⭐⭐ **nejhorší z nich byl test sám**:
`MoveHandler.StandUpNotEnoughMA` tvrdil *„pod 3 MA vstávání selže"*, což
BB2016 ř. 691-693 **popírá** *(žádá hod 4+)*. Někdo napsal kód, pak test, který
s kódem souhlasí, a od té chvíle byla vada **certifikovaná**: Treeman nevstal
z 911 sražení ani jednou.
⇒ **Chybějící osa je pokrytí PRAVIDEL, ne kódu.**

**Tři úlohy** *(zadání `evidence/fable_brief_test_audit_20260821.md`)*:
* **A ⭐** testy, které fixují chování **bez opory v pravidlech** — nejlevnější
  a nejúčinnější;
* **B** dovednosti a akce **bez jediného testu** *(precedens: `PilingOn` je
  v enumu a v `engine/src/` se nevyskytuje vůbec, T5.15)*;
* **C** co je v **N/A kontrol** — ⚠️ to už máme jako **T2.18**, sem patří jako
  táž rodina.

⏰ **Spustit AŽ PO rules-parity auditu** — jeho výstup je vstupem téhle úlohy
*(u každého rozporu kódu a pravidla se ptát „a proč to nechytil test?")*.
Souběžně by se tatáž práce zaplatila dvakrát.

⭐ **Proč to vzniklo:** uživatel si všiml, že **obě dnešní pravidlové vady
vzešly z ROZHOVORU a ani jednu nenašla kontrola** — při 572 testech, z nichž
se **ani jeden nedotýká vstávání ani asistencí u faulu** *(ověřeno)*.
⇒ **Kontrola ověřuje to, co nás napadlo ověřit**, a pravidlová vada je z
definice to, co nás nenapadlo.

## ✅ VÁRKA OPRAV 21.08. PŘED RESTARTEM — testy 577/577

Sběr zabit **11:26** *(989 her `dwarf-dwarf`, zahozeno)*. Opraveno jen to, co
**mění, co se ve hře stane** — pozorovací věci a neživé rasy odloženy.
**Každé pravidlo ověřeno v `rules_bb2016.txt`, ne z citace agenta.**

| # | co | pravidlo | proč to hoří |
|---|---|---|---|
| **F1** | omráčený se probíral **na začátku** kola | ř. **703-708** *(„at the END of their team's next turn")* | **aktivace navíc** každému omráčenému, 6,2 stunů/hru; ⭐ **spící vada, kterou probudilo P45** |
| **F2a** | Guard asistoval faulu | ř. **8160** *(„may not be used to assist a foul")* | trpaslík 6 + ork 6 Guardů ⇒ **všechna čísla o faulech nadhodnocená** |
| **F2b** | dublet se hlídal **jen na armour** | ř. **1878** *(„Armour **and/or** Injury")* | ≈**0,42 vyloučení a turnoverů/hru** navíc; faul byl levnější, než má být |
| **F3** | Frenzy házel druhý blok **po každém** výsledku | ř. **8139-8141** *(jen po Pushed / Defender Stumbles)* | blok zdarma; v TV1200 má Frenzy **jen trpaslík** ⇒ hrálo to PRO nás |
| **F4/F5** | Dodge a Sure Feet reroll **bez limitu** | ř. **8089-8090**, **8541-8542** *(jednou za kolo)* | nadržovalo **dodge týmům** (skaven, wood-elf) |
| **B1** | big-guy hod **za každé pole** místo za akci | ř. **8573** *(„after declaring an ACTION")* | ⭐ **taky probudilo P45** — Ogre a Treeman rolovali Bone Head / Take Root za každý krok |
| **B2** | Take Root bránil **vstávání** | ř. **8583-8584** *(„he can still roll to stand up if he is Prone")* | Treeman vstával s **41,7 %** místo 50 %, a jen wood-elf |

**A moje dnešní regrese** *(našel je code review — všechny z commitů 21.08.)*:
* ⛔ **blitz přicházel o svůj blok** — `hasMoved` zábrana nerozlišovala „deklaroval
  blitz" od „jen se hnul"; nově `hasMoved && !usedBlitz`;
* **vstávání pod 3 MA obcházelo `attemptRoll`** ⇒ nešel na něj **týmový reroll**
  *(u Treemana s Loner dvojnásob citlivé — přesně to selhání, které měl P45 odstranit)*;
* **Jump Up neemitoval `STAND_UP`** ⇒ volná vstání byla v logu neviditelná;
* **BallAndChain** chyběl ve vstávací smyčce *(makro vrstva ho měla — dvě
  vrstvy si odporovaly uvnitř jednoho commitu)*;
* **pojistka `avoid`** — vstávací makro má cíl == vlastní pole; kdyby na něm
  ležel volný míč, veto by udělalo z expanze prázdno a z toho **END_TURN**,
  tedy zahození kola celého týmu.

### ⚠️ NÁLEZY AGENTŮ, KTERÉ JSEM PO OVĚŘENÍ NEPŘIJAL

* **„Rozbil jsi Jump Up"** — ⛔ **ne.** ř. 8198 dává volné vstání jen tomu, kdo
  deklaroval **jinou akci než blok**, a pak platí ř. 674. Blok z lehu je
  samostatná cesta s hodem **AG+2** (ř. 8200), kterou engine **nikdy neuměl**.
  Moje `hasMoved` odstranilo **nelegální** blok zdarma.
* **„prone Looney (roster.cpp:308)"** — ⛔ špatné jméno: ř. 308 je **Fanatic**
  (ST 7, BallAndChain), Looney je ř. 307 (Chainsaw). Oprava věcně sedí.
⭐ **Dvakrát za odpoledne přinesl agent správný nález se špatným zdůvodněním.**

### 📌 ODLOŽENO — nemá vliv na korpus

* **Stand-and-go** — makro umí postavit jen **na místě**; ř. 670-671 dovolují
  utratit zbytek pohybu *(Longbeard MA 4 vstane a jde 1 pole)*. **Není to
  porušení pravidel, je to STROP na naměřenou hodnotu P45.** ⇒ přeměřit potom.
* **Ball & Chain** *(goblini — v TV1200 nikdo)*: ř. **7827-7830** žádá hod na
  zranění **bez hodu na brnění** a **Stunned → KO**; my házíme i brnění
  a Stunned necháváme. → k P49.
* **Jump Up blok z lehu za AG+2** *(v TV1200 nikdo)* → k P49.
* **Metry**: `achievable` počítá přirážku z **počtu**, ne ze síly *(dědí přesně
  tu slepotu, kvůli které vznikla `corridor_strength`)*, a `penalty` **saturuje
  už při odporu 3**, přičemž 5 je běžné ⇒ člen nese skoro nulovou informaci.
  `TempoSnapshot::turnsLeft` se počítá a **neexportuje**. → k **P53**.

## ❓ P55 — PROPADLÁ AKCE PO BONE-HEAD / TAKE ROOT: SPOTŘEBUJE SE DEKLARACE? *(NEOVĚŘENO)*

⚠️ **Tohle je otevřená otázka, ne nález.** Vznikla 21.08. při opravě B1 *(hod
big guye jednou za akci, ne za pole)* — všiml jsem si klauzule a **nekontroloval
jsem ji**.

**BB2016 ř. 7975-7983 (Bone-head):**
> *„…you must roll a D6 immediately after declaring an Action for the player,
> but before taking the Action. On a roll of 1 … The player can't do anything
> for the turn, **and the player's team loses the declared Action for the
> turn.** (So if a Bone-head player declares a Blitz Action and rolls a 1, then
> the team cannot declare …)"*

⇒ **Propadlý blitz se podle pravidel nedá deklarovat znovu** — tým o něj pro
to kolo přišel. **Nevím, jestli to tak u nás je.**
`resolveAction` při zablokované akci vrací `ActionResult::ok()` *(„Action
wasted, not turnover")*, ale `blitzUsedThisTurn` se nastavuje **až v případu
`ActionType::BLITZ`**, tedy AŽ ZA big-guy kontrolou *(`action_resolver.cpp`)*.
**Ověřit obojí:** spotřebuje se týmový blitz? a spotřebuje se hráčova aktivace?

### Proč to není akademické

**Bone-head má v TV1200 jediný hráč — humanní Ogre** `{5, 5, 2, 9}`
*(Loner + Bone-head + Mighty Blow)*. **Take Root taky jediný — elfí Treeman.**
Really Stupid nikdo. ⇒ Týká se to **přesně dvou sestav**, ale v obou jde
o jejich **nejsilnější tělo**.

⭐ **Kontext, kvůli kterému to teď hoří víc:** do opravy B1 se hod dělal
**za každé pole**, takže Ogre s MA 5 měl šanci na propadlou akci
**1 − (5/6)⁵ ≈ 60 %** místo 16,7 % — byl prakticky nepoužitelný, a proto se
nikdy neukázalo, co se s propadlou deklarací děje dál. **Oprava tu cestu
zprovoznila**, takže se to od teď bude dít 6× častěji a je čas se podívat.
*(Táž rodina jako F1 a B1: vada byla němá, dokud jinou opravou neožila.)*

### ✅ ZODPOVĚZENO TÝŽ DEN — čtením kódu, běh to neohrozilo

⭐ **Zmrazení se týká OPRAV, ne zjišťování.** Odpověď je **půl na půl**:

* ✅ **aktivace hráče se spotřebuje správně** — Bone-head na 1 nastaví
  `hasActed`, `hasMoved` i `lostTacklezones` *(`big_guy_handler.cpp:16-19`)*,
  což sedí na *„the player can't do anything for the turn"*;
* ⛔ **týmový blitz se NESPOTŘEBUJE.** `blitzUsedThisTurn = true` je na
  `action_resolver.cpp:90`, tedy **uvnitř** case `ActionType::BLITZ` — a tam
  se nikdy nedojde, protože big-guy kontrola se vrátí dřív.
  ⇒ **Tým smí blitznout znovu jiným hráčem**, přesně to, co ř. 7981-7983
  zakazuje. **Volný blitz navíc** pokaždé, když Ogrovi propadne deklarovaný
  blitz.

### ⚠️⚠️ A NEZOBECŇOVAT NA VŠECHNY BIG GUYE — texty se LIŠÍ

* **Bone-head** ř. 7980-7983: *„…and the player's team **loses the declared
  Action** for the turn."* ⇒ týmový blitz padá.
* **Take Root** ř. 8582-8584: *„if a player fails his Take Root roll as part of
  a Blitz Action **he may not block that turn**…"* — ⛔ **o ztrátě týmového
  blitzu tam NENÍ NIC.**
⇒ **Opravovat po dovednostech, ne plošně jedním `if` v big-guy kontrole.**
*(Really Stupid, Wild Animal a Bloodlust si přečíst zvlášť — neověřeno.)*

⏰ **Oprava až po víkendu** — běží sběr, do enginu se nesahá. Jsou to dva
řádky, ale musí být napsané per dovednost.

## 📋 P56 — ZBYLÝCH OSM PRAVIDLOVÝCH VAD Z AUDITU 21.08. *(neopraveno)*

⚠️ **Vzniklo proto, že tyhle nálezy žily jen v `evidence/fable_rules_parity_20260821.md`.**
Nález, který nikdo nepřepíše do fronty, je ztracený — táž vada jako všechno
ostatní dnes. Opraveno bylo **F1 · F2a · F2b · F3 · F4 · F5**; tady je zbytek.

| # | co | ř. `rules_bb2016.txt` | náš kód | dopad |
|---|---|---|---|---|
| **F12** ⭐⭐ | **Leap je MRTVÝ KÓD** — `resolveLeap` je hotový a správný, ale **nemá VŮBEC ŽÁDNÉHO VOLAJÍCÍHO** *(žádný ActionType, nic v nabídce, žádné makro)* | 8270-8283 | — | ⛔ **oba wardanceři nikdy neskočili**; wood-elf bez klíčového nástroje |
| **F6** | Pass reroll jen na **přirozenou 1** ⇒ **skill/týmový reroll se na nepřesnou přihrávku nikdy nepoužije** | 8335-8337 | `pass_handler.cpp:275-346` | deformuje pass ekonomiku |
| **F7** | Fumble má být **modifikovaný ≤ 1**, ne přirozená 1 | 1742-1745 | tamtéž | opačný směr než F6 |
| **F8** | Nepřesná přihrávka létá **kickoffovou šablonou D8×D6** místo rozptylu 3×1 *(vlastní Hail Mary přitom 3×1 má — kód si protiřečí)* | 735-741 | `pass_handler.cpp:363-365` | každý nepřesný pass |
| **F11** | **Wrestle**: chybí turnover, když jde k zemi vlastní nosič | 8677-8678 | `block_handler.cpp:602` | vzácné |
| **F9** | Throw-in: místo chycení na dopadu vždy bounce + klamp místo re-throw | 871-877 | `ball_handler.cpp:186-205` | část z 6,6 bounce/hru |
| **F10** | Výkop bez povinného odrazu a bez touchbacku | 276-281 | `game_simulator.cpp:544-552` | ~3,5 výkopů/hru, ±1 pole, symetrické |
| **F13** | Kickoff tabulka *(Riot jednostranný · Blitz! = šoupnutí o pole místo volného kola · Rock jen stun)* | 1284-1345 | `kickoff_handler.cpp:60-180` | ⚠️ **SPÍCÍ** — korpus běží na `simpleKickoff`, je to mrtvá cesta |

### ⭐ F12 je táž třída jako vstávání

**Schopnost, kterou plánovač nikdy nevyvolá.** `resolveLeap` je napsaný,
otestovaný a pravidlově správný — a nevolá ho nic. Z logu to nejde poznat
*(Leap nemá ani event typ)*, přesně jako vstávání před P45.
⇒ **Wood-elf byl v každém dosavadním měření oslabený TŘETÍM způsobem** —
vedle Treemana, který nevstával, a Take Rootu, který mu to bral.

### 📌 Pozorovací, dá se batchovat kdykoli

* **BLITZ deklarace se neloguje** ⇒ blitzy se dnes rekonstruují heuristicky
  *(„pohyb + blok" — právě proto se dal spolehlivě spočítat jen jako
  „nadbytečné nad jeden za kolo")*;
* **asistence u faulu se nelogují** ⇒ ⛔ **skutečnou velikost F2a po opravě
  nezměříme**;
* **Leap nemá event typ.**

### ⛔ Edice tu nerozhoduje

Všech 13 nálezů je **shodně vadných proti CRP i BB2016** *(texty ověřeny
v obou souborech)*. Žádný nestojí na volbě edice.
⚠️ **Otevřená otázka z P45** *(dovolí BB2016 postavení za 3 MA na začátku
LIBOVOLNÉ akce, i BLOCK?)* **tímto auditem rozhodnuta NENÍ.**

⏰ **Vše až po víkendu** — běží sběr.

## 📋 T2.21 — SPUSTIT `/code-review max` A PŘEHODNOTIT JEHO NÁLEZY *(uživatel 21.08.)*

*„Máme tedy za úkol spustit command code-review max — s tím, že pak
přehodnotíš, co vrátil za nálezy."*
⏰ **Pondělí nebo později** — není to urgentní, ale nemá to zapadnout.

### Rozsah

Dnešní zásah do enginu: **`7a228f73~1..96809734`** *(oprava vstávání · metry
desky · sedm pravidlových oprav · pět regresí — 12 změn chování za jeden den)*.
⚠️ **Nedávat to bez rozsahu** — jsme na `main`, takže bez argumentu by review
bralo celou větev.
⭐ **Proč zrovna tenhle rozsah:** dvanáct změn chování v jednom dni je přesně
prostředí, kde se chyby schovávají v **interakcích**, ne v jednotlivých
změnách — a dvě z dnešních vad *(F1, B1)* byly spící a probudila je jiná
oprava.

### ⭐⭐ A PAK JE PŘEHODNOTIT — to je vlastní úkol, ne formalita

**Doloženo 21.08., dvakrát za jedno odpoledne:**
* ⛔ *„Rozbil jsi Jump Up"* — **nerozbil.** BB2016 ř. 8198 dává volné vstání
  jen tomu, kdo deklaroval **jinou akci než blok**; blok z lehu je samostatná
  cesta s hodem AG+2, kterou engine nikdy neuměl. **Moje změna odstranila
  NELEGÁLNÍ blok zdarma.** Kdybych nález přijal, vrátil bych vadu zpátky.
* ⛔ *„prone Looney (roster.cpp:308)"* — ř. 308 je **Fanatic** (ST 7,
  BallAndChain); Looney je ř. 307 (Chainsaw). Oprava věcně seděla, **jméno ne**
  — a jméno hráče člověk instinktivně nekontroluje.
⇒ **Agent přinese správný nález se špatným zdůvodněním, a to je horší než
špatný nález** — projde to.
⚠️ **Každé pravidlo si ověřit v `rules_bb2016.txt`**, ne z citace agenta.

### ⚠️ Mez toho nástroje: `/code-review` běží jako FORK mého kontextu

⇒ **Sdílí moje slepá místa.** 21.08. našel fork pět regresí, ale **nezávislý
Opus s čistým kontextem našel výrazně víc** *(Take Root u vstávání, big-guy hod
za každé pole, past s `avoid`)* — ⭐ **rozhodla nezávislost kontextu, ne
úroveň.**
⇒ **Pouštět v páru**: `/code-review max` **a k tomu čerstvého recenzenta**,
který o změnách neví nic než diff a texty pravidel.

### ⛔⛔ `/code-review ultra` — ZAMÍTNUTO 21.08., nenavrhovat znovu

Multi-agentní review v cloudu, **placený zvlášť**; spustit ho může jen
uživatel. **Byl to MŮJ návrh a uživatel ho týž den zamítl:**
*„jsem proti dalším platbám — když nevyužijeme ani tokeny, co máme teď."*
⇒ ⭐ **Kritérium je jasné a platí obecně: dokud je nevyužitá kapacita, kterou
už máme, neplatí se za další.** Fable je na **30 % spotřeby na konci týdne**,
protože prioritu má něco jiného — ne protože by došla.
⇒ **Nenavrhovat ultra znovu, dokud se ta situace nezmění** *(např. že by si
dva audity odporovaly A Fable by byla vyčerpaná)*.

⚠️ **Nic ostatního ve frontě žádnou platbu navíc nepotřebuje:** `/code-review`
ve všech úrovních *(low až max)* je běžný nástroj, a obě nachystaná Fable
zadání jdou z existujícího rozpočtu.

## ⛔⛔ P57 — `attemptRoll` ŘETĚZÍ TŘI REROLLY JEDNOHO HODU *(živé v korpusu 21.08.)*

Našel **audit testů 21.08.**, rules-parity audit to **NEMĚL**. **Ověřeno mnou
v `rules_bb2016.txt`, ne převzato.**

**ř. 925-927:** *„**VERY IMPORTANT:** No matter how many re-rolls you have, or
what type they are, **you may never re-roll a single dice roll more than
once**."* *(totéž ř. 950-951)*
**ř. 8385-8387 (Pro):** *„On a roll of 1, 2 or 3 the original result stands and
**may not be re-rolled with a skill or team re-roll**; however you can re-roll
the Pro roll with a Team re-roll."*

⛔ **Náš `attemptRoll` (`helpers.cpp:241`) vrství skill → Pro → týmový reroll**,
tedy **až tři rerolly téhož hodu**. A po **neúspěšné** Pro bráně *(1-3)* pustí
týmový reroll **originálu**, což ř. 8386 zakazuje výslovně.

### Rozsah: největší ze všech dnešních

Týká se **každého dodge, GFI a sebrání míče** — nejčastějších hodů ve hře.
Systematicky zvyšuje úspěšnost, a **nejvíc týmům se skill rerolly**, tedy
zase **skavenovi a wood-elfovi** *(Dodge, Sure Feet)*.
⚠️ **Dnešní oprava F4/F5** *(limit 1× za kolo)* **tuhle kaskádu NEŘEŠÍ** —
je to jiná vada v téže funkci: F4/F5 omezuje, **kolikrát za kolo** smí hráč
skill reroll použít, P57 řeší, **kolikrát po sobě smí být přehozen JEDEN hod**.

### ⚠️ A test ji CERTIFIKUJE

`Helpers.AttemptRollFullChain` s kostkami `{2,1,4,2,5}` prochází celou
kaskádou *(fail → Dodge reroll fail → Pro brána prošla → Pro reroll fail →
týmový reroll uspěl = **tři rerolly jednoho hodu**)* — a je **zelený**.
⇒ Čtvrtý test, který dnes vadu zafixoval. Patří do **T2.20**.

### ⛔⛔ ŽIVÉ V BĚŽÍCÍM KORPUSU — a NEOPRAVUJE SE

**Uživatel 21.08.: „tohle zapiš, ale do aktuálního běhu už nesahej."**
⇒ **Křížový korpus z 21.-24.08. se sbírá s TOUHLE VADOU.** Není to omyl,
je to **vědomé rozhodnutí uživatele** a stojí na třech důvodech:

1. ⭐ **Není to táž třída jako ranní vady.** Omráčení a vstávání měnily,
   **KDO stojí na hřišti** *(třetina mužstva)*. P57 mění **úspěšnost hodů** —
   posune čísla, ale nemění tvar hry.
2. ⭐ **Korpus se sbírá hlavně kvůli patnácti dvojicím, z nichž deset jsme
   nikdy neviděli.** K tomu, CO v nich vůbec je, ta vada nebrání.
3. ⭐⭐ **Rozhodující: alternativou není lepší korpus, ale ŽÁDNÝ.** Uživatel:
   *„nepřijde mi to zásadní pro ničení běhu — zvlášť relativně blízko před
   breakem."* Kdyby restart selhal a nikdo tu nebyl, přijdeme o celý víkend.
   ⇒ **Zapsaná vada se dá při čtení odečíst; chybějící data ne.**
⚠️ **Kdo ten korpus bude číst, MUSÍ to vědět:** rerolly jsou v něm
nadhodnocené, nejvíc u dodge týmů.

⏰ **Opravit po víkendu**, spolu s P53, P54, P56. Oprava je malá — příznak
„tenhle hod už byl přehozen" v `attemptRoll`.

## 📋 T2.20 HOTOVO 21.08. — audit testů: **13/13 vad testy nechytily**, ale našel 10 NOVÝCH

Výstup: **`evidence/fable_test_audit_20260821.md`** *(končí `HOTOVO`)*.
Jmenovatel přiznaný: **plně prošlo 268 mechanických testů** + vzorky,
**vynecháno ~229** AI/aparát.

### Verdikt: hypotéza potvrzena — ale NENÍ to osud

**Žádný z 13 nálezů rules-parity auditu žádný test nechytil**, a rozpadá se to
do čtyř mechanismů:
* **3× test vadu přímo CERTIFIKOVAL** *(`StandUpNotEnoughMA`,
  `ResetPlayersForNewTurn`, kostky ve `FoulHandler`)*;
* **6× chyběl test hranice** přesně tam, kde je vada;
* **3× VAKUÓZNÍ ASERCE** — `InaccuratePassScatters` a `HailMaryPassScatters3Times`
  tvrdí `EXPECT_GE(turnover + success, 0)`, což je **vždycky pravda**;
  ⭐ dva z nich kryly F8;
* **1× zelené testy MRTVÉHO KÓDU** — `resolveLeap` má **tři zelené testy
  a žádného volajícího** *(F12)*.

⭐⭐ **A to podstatné: postup „přečti pravidlo → najdi test → otestuj hranici"
našel za jedno odpoledne 10 NOVÝCH certifikovaných rozporů (TA1-TA10),
které pokrytí kódu nikdy nenajde.** ⇒ **Jde to hledat systematicky.**

### ⛔ TÝKÁ SE KORPUSU — do balíku oprav po víkendu

* **TA1 = P57** *(řetěz rerollů, už zapsáno)*.
* ⛔ **TA2 — TAKE ROOT, dvě vady, obě ověřeny mnou v textu:**
  1. **rozsah:** ř. **8572** *„Immediately after declaring an **Action**"* —
     jakoukoli. Náš `big_guy_handler.cpp:87` hází **jen na `MOVE` a `BLITZ`**
     ⇒ **Treeman si BLOK vezme bez hodu.**
  2. **perzistence:** ř. **8575** *„his MA is considered 0 **until a drive
     ends**, or he is Knocked Down or Placed Prone"* — ⛔ **žádný trvalý stav
     zakořenění u nás NEEXISTUJE**, hod se zapomene.
  ⇒ Obojí dělá elfího Treemana **silnějším, než má být**, a **interaguje
  s dnešní opravou B2**.

### 📌 LATENTNÍ *(goblin / vampire rostery — v TV1200 nikdo)* → k P49

**TTM** *(5 rozporů — accurate má scatterovat 3×, fumble padá do PŮVODNÍHO
pole a test asertuje opak, turnover bez míče, Always Hungry žere po jedné
místo dvou, chybí −1)* · **Bombardier** *(test se JMENUJE `NeverTurnover`,
ale ř. 7956 říká „Fumbles … ARE turnovers")* · **Hypnotic Gaze** *(fail není
turnover; cíl 2+TZ místo Agility)* · **Ball & Chain** · **Chainsaw**
*(chybí +3 na armour)* · **Tentacles** a **Shadowing** *(obě mají úplně jinou
kostkovou mechaniku než ř. 8585/8455 — 2D6±ST/MA)* · **Blood Lust**.

### 📌 Mrtvé a netestované

* **5 mrtvých dovedností v enumu**: `PilingOn` · **`Leader`** *(= chybějící
  reroll ekonomika!)* · `DumpOff` · `Animosity` · `PassBlock`;
* **11 implementovaných dovedností bez jediného testu** — z toho **`Catch`,
  `Pass` a `Fend` HRAJÍ v korpusu**, a ⭐ **F6 sedí přesně v netestovaném
  místě skillu `Pass`**.

### Úloha C → patří pod T2.18

Aparát kontrol N/A **už počítá** *(deg + `st[]` důvody)*; zbývá **důvod
v typu**, jedna **tichá cesta v K30** a **kontrakt exportu `plan.*`**.

## 🐢 P49 — UPÍŘI A HYPNOTIC GAZE *(uživatel 21.08., NÍZKÁ PRIORITA)*

Uživatel 21.08.: *„přidej si na nízkou prioritu upíry a skill GAZE — protože
ten to svým způsobem porušuje a neporušuje. Zatím nás toto neovlivňuje."*

**Kontext:** vzniklo u opravy vstávání, kde platí *„nesmíš se hýbat, když bereš
Block akci"* (BB2016 ř. 675). **Hypnotic Gaze je akce, která do téhle dvojice
pohyb/blok nezapadá** — je to třetí druh akce proti sousedovi, a proto to
pravidlo zdánlivě porušuje, aniž ho porušuje.

⛔ **Znění NEDOHLEDÁNO — neodhadovat z hlavy**, ověřit proti
`rules_bb2016.txt`. Otevřené: smí se upír před Gaze hýbat? je Gaze akce sama
o sobě, nebo součást Move akce? co s aktivací po ní?

⚠️ **Dnes nás to nijak neovlivňuje** — v sestavách TV1200 `dw-we` upír není.
⇒ Řadí se **k P48**, tedy až za vším ostatním. Je to **poznámka, aby se to
neztratilo**, ne úkol.

## 🐢 P48 — SCREEN PRO ELFA *(zavedeno 21.08., ZÁMĚRNĚ NEJNIŽŠÍ PRIORITA)*

**Původ:** rešerše 21.08. potvrdila, že klec je *„a staple tactic used by pretty
much every team"*, **ale agility týmy ji na útoku často vynechávají a staví
místo ní SCREEN** — šířka a flexibilita místo shluku; soupeř nesmí projít mezi
elfy bez dodge rollu. Uživatel 21.08.: *„elf teoreticky může zkusit i screen
místo klece — ale klec je univerzální pravidlo s jasným zadáním."*

⛔ **Priorita je nejnižší ZÁMĚRNĚ a je to rozhodnutí uživatele 21.08.:
screen pomáhá ELFOVI, a elf už dnes skóruje víc než trpaslík**
*(0,55–0,67 proti 0,44–0,54 TD/hru)* ⇒ **nejmenší volný prostor na zlepšení.**
Jde až **po kleci a nejspíš po všem ostatním.**

### Definice, kterou je nutné zapsat PŘED měřením

*(jinak si screen nadefinujeme tak, aby vyšel — lekce z K29 a ze σ-tabulky)*

**SCREEN = řetěz našich STOJÍCÍCH těl mezi nosičem a hrozbou, bez volného
průchodu.** Tři klauzule, opět **konjunkce**:
1. **spojitost** — sousední členové řetězu jsou Chebyshev **≤ 3** *(při rozestupu
   4 vznikne pole, které nemá TZ ani jednoho souseda = volný pruh;
   při **≤ 2** je každá mezera kryta DVĚMA TZ = „těsný screen")*;
2. **ukotvení** — oba konce končí na lajně, nebo je obcházka dražší, než má
   soupeř MA;
3. **separace** — každá soupeřova cesta k nosiči kříží řetěz.

⭐ **Klauzuli 3 nemusíme stavět od nuly: je to ZRCADLO metriky 17.6**
*(„kolik má nosič levných únikových polí", `diag_basing_vs_columns_20260820.py`,
prediktivní: gradient +1,41 → +2,86)*. Screen se ptá na totéž z druhé strany —
kolik má **soupeř** levných cest k **našemu** nosiči.

### ⚠️ Dvě pasti, které se musí přiznat dopředu

* **Metr bude VYHLADOVĚLÝ.** Engine pojem screenu nemá vůbec ⇒ na baseline
  korpusu vyjde jednotky procent a σ ≈ 0, což **neznamená „neplatí"**
  *(klec 2,7 % / 0,0σ · sloupce 2,3 %)*. Měřit se smí až s ramenem.
* **Screen NEMÁ kanonickou geometrii.** Klec jsou čtyři pevná pole, screen je
  RODINA tvarů ⇒ je to **měkké kritérium**, a s měkkými kritérii má projekt
  špatnou zkušenost *(σ-tabulka konjunkci rozložila na sčítance a neviděla nic)*.

### ⭐⭐ Proč to bude levnější, než to vypadá — screen ≡ D1

**Sloupce po dvou (obranná fáze D1) jsou fragment screenu**: dvojice
s mezerou je přesně zařízení proti „projít mezi". A grumbbl *„nezůstávat
v base kontaktu"* je screenová logika, ne klecová. ⇒ **Až na P48 dojde,
většina geometrie už bude stát z obrany** *(T1.11, spec ČÁST 17)*.

### ✅ NAPĚTÍ O LAJNĚ ROZŘEŠENO 21.08. — uživatel

⭐⭐⭐ **Zákaz lajny platí pro NOSIČE, ne pro FORMACI.**
Uživatel 21.08.: *„screen u lajny mi přijde OK — ušetřím těla a zároveň si dám
pozor na vysurfování jednoho krajního. Samotný ballcarrier za screenem
samozřejmě stojí v klidu dál od lajny a počítá si, kde stát — podle toho, kam
doběhne nejrozptýleněji."*

⇒ **U1 se nemění, jen se upřesňuje jeho subjekt.** Lajna je soupeřova asistence
zdarma **proti tomu, kdo u ní stojí**. Screen u ní stát SMÍ, protože pro něj je
lajna kotva zdarma *(ušetřená těla)*; **nosič u ní stát NESMÍ**, protože pro něj
je to hák. Klec a screen se tedy nerozhodují rasou ani doktrínou, ale **tím,
čí tělo se k lajně tlačí**.

### 🌊 Cena kotvy zdarma — SURF, a řeší se GEOMETRIÍ (klauzule 2)

Krajní tělo screenu je nejlevnější cíl: **crowd push je zranění bez armour rollu.**
⭐ **Řešení je v tvaru, ne v extra pravidle: kotva stojí na `y = 1`, ne na `y = 0`.**
Odvozeno z geometrie odsunu *(ověřit proti enginu, P9/P9c)*:
* mezera ke lajně zůstává **krytá vlastním TZ kotvy** ⇒ **screen se neotevře**;
* na `y = 0` jde kotva ven **JEDNÍM** pushem; na `y = 1` **žádný jediný push ven
  nevede** — útočník od lajny odsouvá směrem OD sebe, tedy dovnitř, a útočník
  zevnitř dotlačí kotvu nejvýš na `y = 0`.
⇒ **`y = 1` stojí soupeře o jedno kolo víc a nás nestojí nic.**

### 🎯 NOVÁ KLAUZULE 4 — POZICE NOSIČE ZA SCREENEM

Screen bez pravidla o nosiči je poloviční tvar. Nosič **není členem řetězu**;
stojí **za ním a dál od lajny**, a své pole si **dopočítává** — táž logika jako
pravidlo z 19.08. *(pole nosiče se dopočítává z klece, která z něj vyjde)*,
ale s **jiným kritériem**: ne „z jakého pole vyjde klec", ale
⭐ **„z jakého pole se pokračování nejvíc ROZPTÝLÍ".**

⚠️ **„Nejrozptýleněji" se musí zapsat jako počítatelná veličina PŘED měřením.**
Kandidát k předložení uživateli *(nepotvrzeno)*: pro každé kandidátní pole S
vzít pole dosažitelná nosičem příští kolo a měřit **úhlový rozptyl směrů, které
nejsou levně kryté** *(TZ / dosah blitzu)* — ne pouhý POČET polí, protože deset
polí v jednom klínu je jedna cesta, ne deset.

### ⭐⭐ PODEZŘENÍ NA ROZSAH — kritérium rozptylu nejspíš NENÍ screenové

Metodická lekce U1 i klece: *když pravidlo najdu u jedné rasy/tvaru, první
podezřelý je, že jsem ho omylem zúžil.* **Rozptyl pokračování je kandidát na
chybějící dimenzi „KAM"** *(vzorec 19.08.: engine vybírá KDO a JESTLI, ale ne
KAM — P9 · P34 · P35 je jedna chybějící dimenze)*.
⛔ **Pokud generalizuje, NEPATŘÍ do P48** *(nejnižší priorita, pomáhá elfovi)*,
ale k **volbě pole nosiče**, což je vysoká priorita. **Rozhodnout dřív, než se
to začne stavět** — jinak se vysoká priorita omylem zakope do nízké.



### ⭐⭐⭐ PŘEPÍNAČ FÁZE — uživatel 21.08.

> **„Klec staví brzo pomalý proti rychlému."**

⇒ Není to vlastnost rasy ani soupeře, je to **VZTAH RYCHLOSTÍ**. Jedním
pravidlem to rozliší čtyři případy: pomalý proti rychlému kleci **hned**
*(neuteče)*, rychlý proti pomalému **běží a drží screen** *(uteče)*, zbylé dva
jsou mezi tím podle velikosti rozdílu.

**Operační přepis** *(moje formulace, ne uživatelova — ať se to nepřipíše
špatně)*, bez konstanty a bez rasy:

> **Dokáže nosič skončit kolo mimo dosah všech?**
> **ANO ⇒ běž a drž screen. NE ⇒ stav klec.**

⭐ **Umíme to spočítat už dnes:** je to zrcadlo **metriky 17.6** *(levná úniková
pole nosiče, gradient +1,41 → +2,86)*, a rys **`carrier_blitzable` [63]**
v hodnotové funkci už existuje.
⇒ **Přechod fáze tedy NENÍ číslo kola ani konstanta** — sedí na uživatelův
zákaz konstant z 20.08.

⚠️ Uživatelův vlastní příklad: *„u trpaslíků preferuju sestavit klec co
nejdříve, zvlášť proti ELFŮM a SKAVENŮM"* — tedy proti **MA 8-9**. Sedí to na
U1, kde je hák **max MA toho, kdo na nosiče DOSÁHNE** *(6→×1,31 · 7→×1,15 ·
8→×1,62 · 9→×1,89)*.

### Odstup nosiče od screenu — Q11 ZODPOVĚZENO

*„Ze začátku — kam dojde. Runner jde z hloubi pole s míčem a screen jde od
půle. Pak asi jednu mezeru za screenem a pak on."*

* **na začátku drivu odstup vzniká GEOMETRIÍ, ne pravidlem** — runner startuje
  vzadu, screen se staví od půlky;
* **ustálený stav: jedna mezera**, tedy nosič **dvě řady** za linií.

⚠️ **Jedna mezera je slabší, než jsem u Q11 navrhoval:** kdo protrhne clonu,
stojí na linii a k nosiči mu zbývají 2 pole — **blitz s pohybem je dojde**.
Drží to proti protlačení, **ne proti rychlému blitzeru**.
⭐ **A to je přesně důvod, proč pravidlo výš existuje:** proti rychlému se
screen na odstup spolehnout nedá, tak se staví klec.

### ⭐⭐⭐ ROH SCREENU — HOTOVÁ GEOMETRIE *(uživatel 21.08., potvrzeno)*

*„Když má soupeř frenzy, musím poslední roh u kraje postavit o 1 pole dál od
kraje — a za něj druhého o 1 blíž kraji, ale ne u kraje."*
*„Tím, že bude o 1 pole k nám, se brání sám vysurfování."*

```
              x=10   x=11   x=12
        ╔═══════════════════════════╗   ← LAJNA
   y=0  ║   ·      ·      ·         ║
   y=1  ║   ·      B      ·         ║
   y=2  ║   ·      ·      A         ║
   y=3  ║   ·      ·      ·         ║
   y=4  ║   ·      ·      S         ║      soupeř útočí zprava
```
**A** = kotva · **B** = záloha o jedno pole HLOUBĚJI (k nám) · **S** = screen

### Pravidlo kotvy — obecné, ne „proti Frenzy"

⭐ **Kotva stojí tolik polí od kraje, kolik má soupeř ODSUNŮ.**
* obyčejný blokař = 1 odsun ⇒ **`y = 1`** *(z `y=1` jde push jen na `y=0`)*
* **Frenzy = 2 odsuny** ⇒ **`y = 2`** *(`y=2 → y=1 → y=0`, pořád na hřišti;
  z `y=1` by to bylo `y=1 → y=0 → VEN`)*
⇒ Zobecňuje se samo na cokoli dalšího, co odsouvá. **Není to výjimka
pro Frenzy, je to funkce počtu odsunů.**

### Proč záloha, a proč o pole hlouběji

Posunem kotvy na `y = 2` se otevře pruh `y = 0` a `y = 1` ⇒ `B` ho zavře
**tackle zonou**, aniž stojí v linii. ⛔ **A NE na `y = 0`** — tam je
nejlevnější cíl na hřišti.
⭐⭐ **A hloubka ji brání sama** *(uživatel)*: aby soupeř `B` surfoval, musí
stát na opačné straně než kraj, tedy na `(10,2)`, `(11,2)` nebo `(12,2)` —
a **`(12,2)` obsazuje sama kotva `A`**, zbylá dvě leží **za linií screenu**.
⇒ **Dvě těla držící roh si navzájem ubírají soupeřova pole** — týž princip
jako klec: hodnota je v **OBSAZENÍ**, ne v markování.

### ⚠️ Praktický dosah v TV1200

**Frenzy má z pěti sestav jediná — trpaslík** *(2 Troll Slayeři)*.
⇒ Tohle pravidlo nás váže **jen v dvojici trpaslík vs. trpaslík**; ve všech
ostatních jsme my ta strana s Frenzy. ⭐ **A `dwarf-dwarf` je jedna z patnácti
dvojic křížového korpusu — dosud NIKDY neměřená.**
Engine druhý blok Frenzy i crowd surf implementovaný má *(`block_handler.cpp`
:788, :86)*, takže to půjde změřit, ne jen napsat.

### ⛔ OTEVŘENO — kolize nosiče se zálohou

Nosič má stát **za screenem a dál od kraje** *(klauzule 4)* a záloha **za
screenem a blíž ke kraji**. Obě pravidla míří do téže druhé řady a **jak se
skládají, jsme neřešili**. ⚠️ Nosič navíc své pole vybírá podle **rozptylu**,
ne podle screenu ⇒ může vyjít vedle zálohy, což je právě to nahuštění, kterému
se screen vyhýbá.

### ⛔ A screen se dnes NEHRAJE ANI ELFEM — Fable 21.08.

Elf místo klece screen **nehraje, hraje rozsyp**: SCREEN **12,0 %**
(510/4 258) proti trpaslíkovým **11,4 %** (661/5 779) — **stejně**.
⇒ To, že elf klec neplní, **není volba jiné doktríny, je to ABSENCE doktríny.**
⚠️ Metr měří náhodné formace *(přiznáno Fablem)*. ⇒ Poslední důvod, proč by
P48 měl být výš než poslední, tím padá.

### ⚠️ Původní znění napětí *(archiv)*

**U1 zakazuje tlačit VLASTNÍ KLEC k lajně** *(bere dva ze čtyř rohů zadarmo)*,
ale **elfí doktrína staví screen U LAJNY záměrně.**
⭐ Kandidát na rozřešení: **lajna je špatná pro KLEC (ubírá rohy) a dobrá pro
SCREEN (je to zeď, kterou nemusíš obsadit)** ⇒ přepínač klec/screen by pak
nebyl **rasa**, ale **GEOMETRIE**, což je použitelnější. **Zadáno Fablovi
21.08.** jako podotázka k P40 — první dílčí odpověď přijde odtamtud.
Souvisí s **T1.10**.

## ⛔ Co se dnes NEDĚLÁ a proč

* **Q8 — nasadit P38** → **až podle P40.** Rozklad ukázal, že zisk nenese
  klec, a že trpaslík ramenem **obíhá jako elf** (prolomení 0,3 %).
  Nasadili bychom „klec" a dostali obíhání.
* **σ pravidla klece přeměřit** → čeká na korpus **po** nasazení, tedy po Q8.
* **T1.11 obranu stavět** → ~~blokováno Q6~~ **odblokováno 20.08.**, ale **stroj patří kleci**.
* **P39 samostatně měřit** → **P40 to nejspíš rozhodne za nás**: rameno
  obchází právě tu záložní smyčku, která v základu stáhne `steps` na 0.

## Fronta rozhovoru

Viz oddíl níž — **Q4–Q8 + T1.10**. Nejvíc blokuje **Q6**.

---

# FRONTA ROZHOVORU — CO ČEKÁ NA UŽIVATELE *(zavedeno 20.08.)*

⚠️ **Tenhle oddíl se NEPŘEPISUJE, jen mění stav.** Vznikl 20.08., protože se
za jedno dopoledne nakupilo šest věcí, které **měření nerozhodne** — jsou to
volby doktríny. Uživatel 20.08.: *„hromadí se nám tu věci pro nás k rozebrání
— nevadí mi to, je to moje oblíbené.“* ⇒ **Není to dluh, je to fronta.**

| # | otázka | stav |
|---|---|---|
| **Q4** | L vs sloupce rozebrat jako **odehranou situaci** *(konkrétní kolo z korpusu, tah po tahu)*, nebo napřed jako **doktrínu na papíře**? | **ČEKÁ** |
| **Q5** | ⭐ **Postupné svírání, nebo vybrané kolo a all-in?** ⚠️ **Q6 tomu 20.08. přidalo argument: spojitá veličina unese spojitý tvar, práh ne ⇒ svědčí pro POSTUPNÉ.** Uživateli nepředloženo.  Uživatel řekl *„postupně to tlačit do L“*, ale jediný webový zdroj o načasování říká *„vyber si kolo a jdi all-in“*. ⇒ Jiná implementace: postupné chce **spojitou** míru převahy a tvar, který se umí zavírat po jednom poli; all-in chce **práh** a dva hotové tvary. | **ČEKÁ** |
| **Q14** | ✅ **ZODPOVĚZENO 20.08.** — *„tak jak minule, nad a pod nosiče"* ⇒ **degradovaná klec = dvě těla na ORTOGONÁLY kolmo na směr postupu**, ne na diagonály. ⭐ Spočítáno: obě dvoutělové varianty nechají soupeři 6 polí, ale **nad a pod má všech 6 ve svých TZ**, diagonály jen 3. ⚠️ **Zákaz „nosič bez dalších sousedů" na to NEPLATÍ** — předpokládá čtyři těla ⇒ **kontrola musí rozlišit „vybrali jsme špatně" od „nebylo z čeho"**. ⛔ **Mez metriky:** TZ nebrání VSTUPU, jen zdražuje odchod ⇒ dvě těla se plné kleci nevyrovnají, protože hodnota klece je **OBSAZENÍ** (půlí počet polí, odkud jde blokovat). ⭐ **A tvar je PEVNÝ** *(upřesnění uživatele: „dvojice je přibitá na střed, ale nechci zahodit pohyb vpřed a do boku na volnější stranu")* — dvojice drží pevný odstup nad/pod nosičem a **neodpojuje se**; k volnější straně se posouvá **CÍL NOSIČE** (vpřed a do boku) a dvojice ho následuje ve stejném tvaru. Mez = co stihnou **obě** těla. Spec **15.0b‴**. |
| **Q6** | ✅ **ZODPOVĚZENO 20.08.** — *„převaha = stojící těla, počítej to tak“* ⇒ `naše stojící − jejich stojící`, na začátku kola. ⭐ **Fable tou definicí náhodou už měřil**, takže test iniciativy se neopakuje: znaménko se **v žádném pásmu nepřeklápí** (Δ postupu −0,62 / −0,13 / **−0,84**) ⇒ převaha **není spouštěč, co překlápí znaménko**, ale **veličina, podle které se past vyplácí víc nebo míň** — největší přínos kontaktu je při KLADNÉ převaze. ⇒ **D1→D2 se nesmí napsat jako `if (převaha > X)`**, tvar je **spojitý** *(shoduje se se zákazem konstanty)*. Spec **17.4c**. **T1.11 odblokována.** |
| **Q7** | ⚠️ **Rozpor o téže rase:** trpaslík **je basher**, takže obecná basher-doktrína ho posílá do kontaktu **hned** (*„base opponents as often as possible“*), kdežto jeho **vlastní rasová příručka** ho posílá čekat na převahu. Kdo má pravdu? | **ČEKÁ** |
| **Q8** | **Nasadit P38 do produkce?** Rozklad ukázal, že zisk nenese klec, ale boční volnost — a že trpaslík ramenem **obíhá jako elf** (prolomení 0,3 %). ⇒ Nasadit, nebo napřed postavit prolomení? | **ČEKÁ** |
| **T1.10** | **Lajna jako samostatný rozebraný příklad** *(uživatel 19.08.)* — kdy je tlak k lajně přijatelná cena za postup. | **ČEKÁ od 19.08.** |
