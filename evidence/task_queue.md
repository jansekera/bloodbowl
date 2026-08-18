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
| **T1.9** | ⭐⭐⭐ **ROZPOČET JEDNOHO BLITZU ZA KOLO — CHYBĚJÍCÍ KAPITOLA, NE CHYBĚJÍCÍ MĚŘENÍ** *(uživatel 18.08.: „je ten blitz na nosiče někde v dlouhém plánu blitz akce?“ — není)*.<br>Blitz je ve frontě v **šesti** položkách, každá odpovídá na jinou lokální otázku: **P0.6** roh vs zeď (Δx +1,80 vs +2,52; 45,5 % blitzů na roh padlo v kolech, kdy šel blok zdarma) · **P2 ②** blitz na roh jen jako záloha · **P6** cíl se vybírá podle surových kostek · **P15** práh nabídky nezná cenu cíle · **K32** blitz se v logu nepozná od bloku · **P33** blitz na nosiče.<br>⛔ **Nikde ale nestojí, K ČEMU ten jeden blitz za kolo je a jak se jeho uchazeči řadí proti sobě.** Je to jediná akce s tvrdým limitem 1/kolo — tedy jediné místo, kde je doktrína nutně **rozpočtem**, ne seznamem povinností. Bez kapitoly se každý nález o blitzu měří proti jinému pozadí a nedá se sečíst.<br>⭐ **Čísla už z velké části existují, jen nikdy nebyla vedle sebe** *(vše na 3 000 hrách)*: nosič v dosahu blitzu **4,12 kola/zápas** — blitz na něj **48,5 %**, jinam 26,4 % *(z toho polovina cílů stála hned vedle nosiče)*, **neutracen vůbec 25,1 % = 1,04 kola/zápas**; blitz na roh proti blitzu do zdi **+1,80 vs +2,52** (−6,4σ); **61 % polluterů jde srazit BLOKEM zdarma** ⇒ blitz na roh je z velké části vyhozený rozpočet.<br>⇒ **Zadání:** kapitola ve spec, která uchazeče o blitz seřadí jedním kritériem a řekne, kdy se blitz NEUTRÁCÍ. Teprve pod ní visí P0.6, P6, P15, P33 jako případy. ⚠️ **Napřed kapitola, pak jednotlivé opravy** — jinak se opraví čtyři místa proti čtyřem různým pozadím ([[feedback_complete_dwarf_repertoire_not_learn]]). | **KAPITOLA NAPSÁNA 18.08.** — spec **ČÁST 14 „ROZPOČET BLITZU“**. ⚠️ Zjištěno při psaní: kapitola nebyla jen nenapsaná, byla **rozdělaná a přerušená** — blitz byl rozsypaný na **dvanácti** místech spec a měl **DVA různé žebříčky** (S2.10 a 9.5), plus S7.5, S8.3, S5.6, S6.3, S9.4, Z10 a K10/K11/K32. Žádný neříkal, **podle čeho se řadí**. ⭐ **Kritérium: blitz se kupuje DOSAH, ne násilí** — blok je zadarmo, ale jen pro toho, kdo už sousedí; blitz je jeden a jeho jediná vlastnost navíc je, že tělo někam DOJDE ⇒ **patří výhradně tam, kam se blokem nedosáhne**. Z toho žebříček **B1–B7** (jeden pro útok i obranu), seznam **kdy se NEutrácí**, a nová kontrola **K37 „blitz utracen na dosažitelný cíl“**. K32 odblokována rekonstrukcí bez nového logu. ⛔ **Zbývá změřit:** kolik z 1,04 „neutraceno“ bylo správně · B3 vs B2 se nikdy neměřily proti sobě · jestli je 26,4 % blitzů „jinam“ chyba (polovina cílů stála vedle nosiče). |
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
| **T2.10** | ⭐⭐ **`run_night_ab.sh` — ARM blok hlásí falešný poplach a čte jen 1 shard z 8.** Grepuje `^  ARM `, což je řádek **pouze pro mode 4** (`diag_f1_cage_advance_harness.cpp:528`); v mode 0 neexistuje ⇒ v noci 17.→18.08. se vytiskl fallback „(harness nic netiskl — stará binárka?)“ přesně na řádku, který předregistrace označuje za nejcennější ranní čtení — přitom test proběhl a byl **čistý 8/8**. Navíc `head -1` = jen shard 0, takže leak v shardu 5 by neprobublal. Opravit vzor na `MOVED WITHOUT THE ARM ACTING` (tiskne se v každém režimu) a **sečíst přes všechny shardy**. | **UZAVŘENO** — `056f85ed`; vzor je `MOVED WITHOUT THE ARM ACTING` a čte se **přes všechny shardy** |
| **T2.11** | ⭐⭐⭐ **Nic neslučuje shardy — noc nemá výsledek.** `chain.log` končí `NIGHT DONE` bez sloučené delty; 6 000 párů existuje jen jako 8× ±0,019, tj. **osm jednotlivě neprůkazných čísel**, a skutečnou odpověď musel ráno spočítat člověk. Táž rodina jako audit aparátu: **snímek se vydává za stav** — a je to přesně krok, kde si unavené čtení vybere shard, který se hodí. Chce to `night_summarize`: sloučená delta + sdružená SE + **empirická SE mezi shardy** *(kontrola overdisperze — v noci 0,0053 < 0,0068 ⇒ sloučení legitimní)* + počet záporných shardů + součet leaku + `n_nonzero`. | **UZAVŘENO** — `056f85ed`; `night_summarize.py` — sloučená delta · sdružená SE · empirická SE mezi shardy · počet záporných shardů · součet leaku · `n_nonzero`, **v pořadí čtení z předregistrace**; při leaku se delta NEVYTISKNE |
| **T2.12** | ⛔ **Dva různé prahy v jedné noci.** Harness tiskne natvrdo `[pre-reg: >= +0.03 on dwarf matchups]`, předregistrace na tutéž noc říká **±0,015**. Práh se nikde **strojově nevyhodnotí** ⇒ verdikt zůstává na ranním čtení, což je otevřená branka pro dodatečné doladění. Práh má být **vstup běhu** (proměnná prostředí zapsaná do `chain.log` při startu), ne konstanta ve zdrojáku. | **UZAVŘENO** — `056f85ed`; `THRESHOLD` je vstup běhu, zapsaný do `chain.log` PŘI STARTU, verdikt se vynáší strojově |
| **T2.13** | **Preflight neověří, že kontrola vůbec existuje.** Hlídá mtime binárky i `libbb_engine.so` (T2.7 na úrovni enginu), ale ne to, jestli binárka umí vytisknout `MOVED WITHOUT THE ARM ACTING`. Kdyby neuměla, noc vypadá normálně a **verdikt stojí na kontrole, která neproběhla**. Oprava: sonda 1 páru v cílovém režimu, grep na ten řádek, jinak `exit`. | **UZAVŘENO** — `056f85ed`; sonda na 1 páru v cílovém režimu (47 s); když binárka kontrolu netiskne, noc se **nespustí** (`exit 4`) |
| **T2.14** | **Předregistrované předpovědi se nikde nekonfrontují s výsledkem.** Noc 17.→18.08.: dvě ze šesti (K9a tempo dolů, bloky nahoru) byly **nezodpověditelné**, protože běh měl `CORPUS=0`; a minutá předpověď `n_nonzero` **62,8 % vs čekaných >80 %** je informace o rameni *(brána sahá na méně kol, než jsme mysleli)*, která by bez zápisu propadla. Buď `CORPUS=1`, nebo předpovědi na korpus z předregistrace vyškrtnout — a přidat krok „předpověď vs výsledek“ do shrnutí. | **UZAVŘENO 18.08.** — uživatel: *„nemělo toto být součástí oprav s prioritou? dej tomu prioritu"*. Předregistrace je teď **vstup běhu** (`PREREG=soubor`): ⓵ spouštěč ji zapíše do `chain.log` PŘI STARTU a **odmítne se spustit**, když nějaká předpověď potřebuje korpus a `CORPUS=0` (exit 6) — chytne to za minutu místo za 14 h; ⓶ `night_summarize.py` na konci tiskne **PŘEDPOVĚĎ vs VÝSLEDEK** se štítky **TREFA / MIMO / NEZODPOVĚDITELNÁ**. Ověřeno zpětně na noci 17.→18.08.: vytiskne přesně ty čtyři vady (delta MIMO, `n_nonzero` MIMO, 2× nezodpověditelná). Testy **29/29 → 35/35**. |
| T2.8 | E1/E2 jako K34/K35 | **UZAVŘENO** — `bc9cf17` |
| T2.9 | **K36 `LOCKED`** — zamčená vlastní těla jako chybějící člen tempa | **UZAVŘENO** — `bc9cf17`; potvrzeno na 3000 hrách, monotónní: ≤2 → Δx **+2,18** (n=15314) · 3–5 → **+1,89** (n=2162) · 6–8 → **+1,06** (n=50) |
| K9b | **ODPOR SE MUSÍ POČÍTAT MIMO BRÁNU — zadáno 18.08.**<br>**Co odpor je:** kolik soupeřových **stojících** těl stojí v koridoru před nosičem (`CORRIDOR_DEPTH` vpřed, `CORRIDOR_HALF_WIDTH` do stran). Každé stojí blok nebo obloučkovou cestu navíc, tedy tempo — je to to, co odlišuje „tři pole prázdným polem“ od „tří polí skrz zeď“.<br>**Kde dnes žije:** počítá se **jen uvnitř plánovače klece** (`cage_advance.cpp:479–486`); jinde se už jen přenáší (`turn_plan_record.h:36`, `bb_module.cpp:272`). V produkci je brána vypnutá ⇒ `plán: NOT_CONSULTED` ve **100 %** kol ⇒ **odpor je vždy 0** a K9b nemá z čeho měřit.<br>⛔ **Blokátor se dnes stal TRVALÝM:** K9b čekala na T3.1, ale T3.1 byla 18.08. **zamítnuta** (brána škodí) — takže věc, na kterou čeká, se už nikdy nezapne. Čekat dál znamená položku tiše pohřbít.<br>⇒ **Zadání:** vytáhnout výpočet odporu do samostatné funkce nezávislé na plánovači, aby existoval v každém našem kole bez ohledu na to, jestli brána běží. Je to pár řádků (dnes jedna `forEachOnPitch` lambda).<br>⭐ **Není to volba, je to PŘEDPOKLAD T0.1:** přepis K9 na fáze potřebuje odpor — podlaha fáze „klec“ se bez něj nedá napsat jinak než konstantou, a konstanta je 18.08. zakázaná. | **UZAVŘENO 18.08.** — `corridorResistance()` je volná funkce v `cage_advance.{h,cpp}`; plánovač volá **tutéž** (jediná definice), a `captureTurnSnapshot` ji počítá **každé naše kolo s míčem** do nového pole `TurnLog::corridorResistance` *(vlastní pole, ne `plan.resistance` — simulátor přepisuje `curLog.plan` z thread-local záznamu, takže cokoli tam orazítkované by přepsaly nuly)*; −1 = N/A, ne nula. Export do pythonu `corridor_resistance`. Ověřeno na 3 hrách: `plan: NOT_CONSULTED` ve 100 % kol a **odpor přesto naměřen** (36 kol, ⌀ 1,44, max 5). Testy **544 → 545**. |
| K32 | blitz se v logu nepozná od bloku | **BLOKOVÁNO** na X1 |
| **P32** | ⭐⭐⭐ **KLEC SE POSOUVÁ JEN ROVNĚ KUPŘEDU — SMĚR SE NEVOLÍ** *(uživatel 18.08.: „bod 1 nesmí škodit — musí stavět čistou klec a kdyžtak ne vždy jen přímo rovně kupředu“)*. Ověřeno v kódu 18.08.: cíl posunu vzniká na dvou místech (`cage_advance.cpp:41` v `cageExposure` a v `tryAssign`) vždy jako **`dest{carrier.x + dx*step, carrier.y}`** — **`y` se NIKDY nemění**. Plánovač tedy volí jen **JAK DALEKO**, nikdy **KAM**: všechny zvažované cíle leží na jedné přímce vpřed. Řádek 548 sice vybírá „nejméně exponovaný cíl“, ale jen mezi kroky po té přímce.<br>⭐ **To je pravděpodobný mechanismus, proč brána postavila víc klece a HORŠÍ klec** (rohů 2,22→2,54, čistota 79,4→72,6 %): když rovně vpřed nejdou rohy udržet čisté, plánovač nemá jak uhnout do strany — umí jen zkrátit krok, nebo vetovat.<br>⚠️ **Táž rodina jako P9** (`choosePushSquare` = „rovně dozadu první“, cílové pole se nehodnotí): **geometrie se nevybírá, jen se vykoná.**<br>⇒ Zadání: cílem posunu smí být i pole do strany/šikmo; kritérium výběru je **čistota rohů v cíli** (počet ŠPINAVÝCH rohů, ne počet rohů — σ-tabulka 18.08.), při shodě dál vpřed. | **OTEVŘENO — zadáno 18.08., přímý vstup pro T3.1** |
| **P33** | ⭐⭐ **BLITZ NA NOSIČE — 4,5× ČASTĚJŠÍ SITUACE NEŽ BLOK NA NĚJ, A BEREME SI JI TŘETINOVĚ** *(uživatel 18.08.: „přijde mi zajímavější situace blitz na ballcarriera“)*. Vypadlo jako vedlejší řádek z měření P10a (3 000 her): u nosiče **stojí** naše tělo v **3 733** kolech a udeříme v **81,5 %**; u nosiče **nikdo nestojí** v **16 832** kolech a doblitzujeme **6 143× = 36,5 %**.<br>⛔ **Ten rozdíl se NESMÍ číst jako vada, dokud nemá jmenovatel:** blitz je **jeden za kolo** a v části těch 16 832 kol na nosiče nedosáhneme, nebo je blitz potřeba jinde (**P0.6 roh vs zeď**: blitz na roh dá Δx +1,80 proti +2,52 do zdi). ⇒ **Otázka zní: z kol, kde je nosič V DOSAHU blitzu, kolikrát jsme blitz utratili jinam — a za co?** Rozpad utracených blitzů podle cíle (roh · zeď · nosič · jinam) proti Δx.<br>⭐ **Měřitelné bez běhu**, na `corpus_baseline_20260817_data`. Je to podstatně větší položka než P10a (16 832 kol proti 3 733) a napojuje se na **P0.6** i na **T1.8** (rozpočet jedné akce za kolo). | **ZMĚŘENO 18.08. — a povýšeno na T1.9.** Se správným jmenovatelem: nosič je v dosahu blitzu ve **12 370 kolech = 4,12/zápas**; blitz na něj **5 997 = 48,5 %** *(ne 36,5 %, to bylo bez jmenovatele)*, jinam 3 264 = 26,4 % *(z toho **50,2 % cílů stálo hned vedle nosiče** — eskorta, nejspíš volba, ne chyba)*, **neutracen vůbec 3 109 = 25,1 % = 1,04 kola/zápas**. Na nosiče dosáhnou **4+ naše těla v 60,4 %** kol ⇒ není to otázka dosažitelnosti. ⛔ **Narazilo to na K32**: v logu **žádná událost `BLITZ` neexistuje**, jen `BLOCK` — první běh vrátil 0,0 % a vypadalo to jako nález. ⭐ **Rekonstrukce, která K32 obchází a nepotřebuje nový log: blok, u kterého útočník s cílem na začátku kola NESOUSEDIL, je blitz** (blok vyžaduje sousedství, blitz je 1/kolo). Použitelné i pro ostatní kontroly visící na K32. ⚠️ Dosah je horní odhad (bez dodge z TZ a bez obsazených polí). Doklad `evidence/blitz_on_carrier_20260818.txt`. |
| T0.1 | **K9 SE PŘEPÍŠE CELÉ — ROZHODNUTO uživatelem 18.08.** *(„jsem pro přepsat celé — minimálně na fáze — a ty kroky asi měřit zvlášť“)*. Ověřeno 18.08.: oprava **neproběhla**, `need = ceil(vzdálenost/zbývající kola)` (`diag_rules_checks_20260812.py:401`) je čistá geometrie bez členu odporu ⇒ rovnoměrná podlaha trestá klec za to, že jede pomalu, ačkoli **nesmí jet rychleji než 2 pole/kolo**. Nový tvar: **rozvrh po FÁZÍCH** (sólo Runner → klec → sólo výběh) a **každá fáze se měří zvlášť**, ne jedním číslem. ⛔ **A ŽÁDNÁ KONSTANTA** *(uživatel 18.08.: „je zbytečné uvedení konstanty někam, kde by stačilo — nesmí se rozpadnout, ale má jet co nejrychleji“)*: podlaha fáze „klec“ se nepíše jako **2 pole/kolo** — to je jen to, co dnešní trpasličí roster vydá (7,03 volných těl z 11, 5,5 kandidáta na 4 rohy). Pravidlo zní **„klec jede tak rychle, jak rychle se dokáže znovu složit“**, strop je funkce volných těl a **cíl je maximum, ne ten strop**. Týž zásah jako T1.8; viz [[feedback_implement_the_rule_not_the_outcome]]. ⚠️ Váže na **P3** (fázový plán trasy) — bez fáze v modelu nejde odlišit chybu od záměru. ⚠️ A pozor: K9a je s **20,7σ** nejsilnější prediktor dnešní σ-tabulky, takže se přepisuje kontrola, podle které je seřazená fronta; blízkost k Δx (13,9σ) navíc připouští, že měří spíš „drive šel dobře“ než „dobře jsme rozvrhli“. | **OTEVŘENO — zadáno 18.08.** |
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
| **P11** | ⭐⭐⭐ **ENGINE SKÓRUJE, JAKMILE MŮŽE — a trpaslík nemá.** *(uživatel 14.08.: „kdyby Runner s míčem utekl a skóroval dříve, je to také špatně, protože soupeř dostane čas na re-TD"; a upřesnění: „ujet víc polí není špatně — ale je špatně skórovat dříve".)*<br>**Cíl není dojít co nejdřív, cíl je překročit čáru tak pozdě, aby soupeř neměl čas odpovědět.** Doktrína záporné rezervy tohle říká od 10.08., ale **v enginu není nikde**.<br>**Tři vady:**<br>**①** `greedyMacroRank(SCORE) = 100` (`macro_mcts.cpp:39`) — nejvyšší prior ze všech maker, bezpodmínečně.<br>**②** `scoringBonus += 0.4` za „safe walk-in" (`:702`) a `+0.8` v posledním kole (`:724`) — odměna za *„umím teď dojít do endzony"* nemá podmínku na zbývající kola soupeře. **Chybí zdržovací člen** *„umíš, ale ještě ne"*.<br>**③** `pacing` (`:712`) trestá `dist < idealDist`, tedy **předstih ve vzdálenosti** — a to je špatně: víc ujetých polí je vždy zisk, dá se stát pole před čárou a čekat. Trestat se má **předčasné překročení čáry**, ne pozice.<br>⛔ **Opravuje mou vlastní úvahu z téhož dne:** navrhl jsem „K9a přepsat na koridor" (trestat i předstih) a uživatel to zamítl. **K9a zůstává jednostranné**; místo něj vzniká samostatná kontrola **„v kolikátém kole jsme skórovali a kolik kol zbylo soupeři"**.<br>⇒ Týká se **každého drivu, který dojde k endzoně** — ne okrajové situace. | **OTEVŘENO — VYSOKÁ PRIORITA**, kandidát na víkend |
| **P12** | **Fáze 3 (sólo výběh) má vlastní podmínku puštění — PRONÁSLEDOVÁNÍ, ne vzdálenost** *(uživatel 14.08.: „taky záleží na tom, kolik soupeřů pak Runnera doběhne v poslední fázi")*. Runner MA6 proti Gutter Runner MA9 / wood-elf Catcher MA8 / ork MA5–6.<br>⇒ proti **wood-elfovi a skavenovi** musí být výběh **krátký** (klec donese míč skoro až tam), proti **orkovi a humanovi** může být delší.<br>⭐ **Dává mlácení účel v TRASE, ne jen v attrition:** *každý sražený pronásledovatel prodlužuje fázi 3.* Dosud se bití obhajovalo jen statisticky (+2,7σ na TD) a nikdo neuměl říct, k čemu je v plánu drivu. ⇒ Priorita blitzu má možná mířit na **ty, kdo umí doběhnout Runnera**, ne na rohy klece. Měřitelné na stávajícím korpusu. | **OTEVŘENO** — třetí parametr P3 |
| P3 | ⭐ **Fázový plán trasy** — sólo Runner + kick-off return → klec → sólo výběh u endzone. Rozvrh pozpátku od TD **po fázích**. Vstup pro `classifyTurnGoal` i `K9a`. **Bez fáze v modelu nejde odlišit chybu od záměru.** | **OTEVŘENO** |

# 4. ROZHODOVÁNÍ ENGINU *(živé chyby)*

| ID | co | stav |
|---|---|---|
| **P9c** | ⭐⭐⭐ **ÚČEL BLOKU NA POLLUTERA JE ODKLIDIT HO OD ROHU** *(uživatel 14.08.: „priorita u špinavého rohu je odklidit protihráče pryč od rohu — ne jej nechat u rohu a posunout blíž k balonu")*. **Není to kompromis, je to pořadí:** odsun, po kterém polluter roh **pořád špiní**, není částečný úspěch — je to **selhání akce**, protože roh byl jediný důvod ji dělat. A když ho takový odsun navíc přiblíží k nosiči, je to **záporný obchod**.<br>⇒ Řazení cílových polí při bloku na pollutera: **① přestane sousedit s rohem** *(to je účel)* → **② nepřiblíží se k našemu nosiči** → ③ zbytek.<br>⚠️ **Váže to i výběr blokujícího, ne jen směru:** tři nabízená pole jsou dána vektorem `polluter − blokující`, takže **kdo udeří, určuje, kam se dá odsunout**. Blokující se má vybírat tak, aby pole splňující ① vůbec existovalo.<br>⭐ **Nepotřebuje nové logování** — je to otázka na začátek kola (pozice pollutera, rohu, nosiče a kandidátů na blok), ne na průběh. Jde spočítat na **stávajícím korpusu**. <br><br>✅ **IMPLEMENTOVÁNO 18.08. A JDE NA NOC.** `choosePushSquare` teď cílové pole **skóruje**, v uživatelově pořadí: ① přestane sousedit s rohem naší klece → ② nepřiblíží se k našemu nosiči (REACH0) → ③ rovně dozadu jako tiebreak. **Bez našeho nosiče na hřišti skóruje přesně jako dřív ⇒ na obraně je rameno konstrukčně no-op.** Per SIDE (`setPushGeometryArm`), default OFF, čítač tiká **jen když se pole opravdu přesměrovalo**. Testy 545 → 549. **Strop spočítán PŘED během:** 21,93 odsunu na zápas, **17,34 se skutečnou volbou**, **1,04 na zápas přisunuto BLÍŽ k našemu nosiči** ač šlo dál (0,27 přímo k němu) a **0,24 ponecháno u rohu** ač šlo jinam ⇒ **~1,28 prokazatelně horší volby na zápas** (P10a 0,23 · P8 0,056, obojí zamítnuto). Předregistrace `evidence/night_prereg_20260818.md`. |
| **P9** | ⭐⭐⭐ **SMĚR ODSUNU SE VYBÍRÁ SLEPĚ — a je to společný kořen dvou dnešních nálezů.** CRP FAQ: *„The coach of the moving team decides all pushback directions unless the pushed player has Side Step."* Máme tedy volbu ze **tří polí** (`getPushbackSquares`) a `choosePushSquare` (`block_handler.cpp:113`) ji zahodí: `score = count - i` = **„rovně dozadu první"**, čistě geometricky. Cílové pole se **nikdy nehodnotí** — nedívá se na nosiče, na klec, na endzonu ani na tackle zóny. Heuristiky existují jen pro Side Step a Grab.<br>⇒ **Každý náš odsun je volné přemístění soupeře, a tu volbu zahazujeme.**<br>⭐ **Nejde jen o geometrii, ale o OBSAZENÍ** *(uživatel 14.08.: „je důležité kdo stojí na a — jestli náš nebo soupeř")*. Když prázdné pole není, odsun **řetězí** a druhý článek je ten, kdo tam stojí. Kód rozlišuje jen prázdné/neprázdné (`anyEmpty`) a pak jede straight-back — **komu to tělo patří, neřeší**. Žebříček cílového pole podle obsazení:<br>• **soupeř** → dobré, řetěz posune **dva jejich** ⇒ když je vedle straight-backu pole se soupeřem a straight-back řetězí přes nás, je současná volba **striktně horší** ⇒ patří k **P9a**, ne k doktríně<br>• **prázdné** → výchozí<br>• **naše řadové tělo** → malá cena<br>• **náš roh klece** → vysoká cena *(úder, který měl roh očistit, ho rozebere)*<br>• **náš nosič** → veto<br>⇒ 44,2 % odsunových polí je obsazených, takže tohle není okrajový jev.<br>**Dopad 1 (uživatel 14.08.):** při čištění rohu blokem *„může odsun nechat soupeře nejen jako stojícího souseda rohu, ale nově navíc i souseda ballcarriera"*. Bije to do **27,2 %** bloků, kde polluter zůstane stát (Fable: na zemi je 72,8 %). A míří to na `REACH0`, což je podle E1 rozdíl mezi **1,8 %** a **33 %** ztráty míče.<br>**Dopad 2:** ranní **8 darovaných TD** ve 3000 hrách má týž kořen — ověřeno na `g0289`: pusher (23,8), nosič (24,7), „rovně dozadu" = **(25,6) = endzona**. Nebyla to smůla, byla to ta konstanta. | **OTEVŘENO — VYSOKÁ PRIORITA** *(blokuje bezpečné nasazení P2)* |
| **P10** | ⭐⭐⭐ **HODNOTA BLOKU SE NEODVOZUJE OD CÍLE — a nosič se odměňuje za MARKOVÁNÍ, ne za sražení.** *(uživatel 14.08.: „když je vedle našeho Longbearda možnost block na GR s míčem a navíc jsou kolem naši — co může být lepšího než jej blocknout?")* Odpověď: nic — a engine to neví.<br>**① Prior je plochý:** `greedyMacroRank` (`macro_mcts.cpp:47-48`) dává `BLITZ 20`, `BLOCK 15` — **jedna hodnota pro všechny bloky**. Blok na nosiče s Tackle a 3 kostkami má týž prior jako blok na linemana v protilehlém rohu. Kategorie „udeř na míč" v žebříčku není.<br>**② Tři existující členy o nosiči odměňují jen STÁNÍ VEDLE:** marking `+0,08×min(TZ,3)` (max +0,24, `:776`), sideline trap `+0,10` (`:808`), contain-vs-AG≥4 `+0,06×…` (max +0,12, `:819`). **Sražení nosiče nemá člen žádný.**<br>⚠️ **DOPOČÍTÁNO 14.08. — podezření POTVRZENO a je silnější, než vypadalo.** Všechny tři členy o nosiči visí na `ball.isHeld`, takže sražením **zmizí naráz** (−0,24 −0,10 −0,12), a místo nich naskočí `heuristic -= 0.1  // loose ball is bad` (`:762`) — ⭐ **který nerozlišuje „upustili jsme ho" od „právě jsme ho soupeři vyrazili".**<br>Bilance členů, které se mění (soupeřův nosič, 3 naše TZ, AG4):<br>• **uprostřed hřiště** (12 polí od endzony): **+0,13 → −0,02 = −0,15**<br>• **u lajny** (y=2): **+0,23 → −0,02 = −0,25**<br>• **může skórovat** (8 polí, MA9): −0,31 → −0,02 = **+0,29** ✅<br>⇒ **Čím blíž je soupeř skórování, tím víc heuristika blok chce; uprostřed hřiště se mu aktivně brání.** A S7 boxing-in = 32,4 % kol je právě ten střed.<br>⚠️ Poctivě: je to **listová evaluace**, MCTS to může přebít hledáním (sebráním míče hlouběji ve stromě). Netvrdím „AI nikdy nebije nosiče" — tvrdím, že úspěšný výsledek akce se hodnotí hůř než výchozí stav.<br>⇒ Sedí na starý nález *„trpaslík markuje a bije, nehoní"*: markovat jsme ho naučili, bít nedopsali.<br>⛔ **PODMÍNKA, BEZ KTERÉ JE OPRAVA ŠPATNĚ** *(uživatel 14.08.: „zkontroluj před blitz Wardancera na balon, že máš v záloze druhého pro pickup a třetího pro zablokování cesty k uzmutému balonu")*: **vyražený míč je zisk jen tehdy, když ho posbíráme.** Jinak jsme vyrobili volný míč uprostřed hřiště a dali ho rychlejšímu týmu — a trpaslík je v souboji o volný míč nejhorší možná rasa (MA4, AG2 u většiny těl).<br>⇒ `loose ball is bad` **není nesmysl, je to správné pravidlo se špatnou podmínkou**: platí, když scramble prohrajeme, neplatí, když ho vyhrajeme. ⇒ Rameno **nesmí** znít „bij nosiče", ale **„bij nosiče, když scramble vyhrajeme"**, a to je **rozpočet tří těl**: ① kdo srazí · ② kdo sebere · ③ kdo zavře cestu.<br>⭐ **Pravidlo je BEZPODMÍNEČNÉ a rasa soupeře o něm nerozhoduje.** Uživatel je řekl dvakrát a pokaždé stejně — u Longbearda proti GR jako součást zadání (*„a navíc jsou kolem naši"*), u Wardancera jako kontrolu. Rychlost soupeře neurčuje, **jestli** pravidlo platí, jen **jak těsně se ta trojice počítá**: proti Wardancerovi musí být třetí tělo blíž a cesta zavřenější, protože je na míči dřív. ⛔ *(Zapsal jsem to nejdřív jako dva protikladné případy lámající se podle rychlosti soupeře — to byl můj konstrukt, ne jeho pravidlo. Opraveno.)*<br>⇒ Dnešní člen se ptá **jen na nás** (`nearestDist` našeho nejbližšího, max +0,08) — nikdy na to, **kdo je blíž, my nebo oni**, a tělo zavírající cestu nemodeluje vůbec.<br>⭐⭐ **A musí se ptát i KDO JE RYCHLEJŠÍ** *(uživatel 14.08.)*: **cena ztráty míče není konstanta, je funkcí rychlosti soupeře k míči.** Proti skavenovi (MA9 + Dodge, a s Nerves of Steel i chycení v obklíčení) je upuštěný míč **skoro inkasovaný gól** — proto se jim vyplatí blitz na míč i za cenu ztráty těl, a proto nám dají **198 krádežových TD proti orkovým 31**. Proti orkovi (MA4–5, AG2–3) je to nepříjemnost, kterou často sebereme zpátky. ⇒ Táž jedna oprava, dva vstupy místo jednoho. Podrobně `evidence/matchup_asymmetry_20260814.md`.<br>⇒ Symetricky potvrzuje rozpočet tří těl: **my potřebujeme tři, abychom scramble vyhráli. Oni jedno.** Souvisí s [[project_bloodbowl_loose_ball_denial_doctrine_20260807]].<br>⛔ **POZOR NA ZÁMĚNU SITUACÍ** *(uživatel 14.08.)*: „nosič" a „polluter" **nejsou dva cíle v jednom kole**, jsou to cíle ve **dvou různých situacích**. Roh klece existuje jen v **našem kole s míčem** (S2–S5); soupeřův nosič jen v **obranném kole** (S7/S8). Prior na blok podle cíle proto **musí být indexovaný situací**, ne jeden plochý žebříček — jinak se opakuje táž chyba o patro výš. ⇒ P10 se dělí: **P10a** blok na soupeřova nosiče *(obrana)* · **P10b** blok na pollutera *(útok)*. **P10b NENÍ levnější cesta k P2, je to P2.** <br><br>⛔⛔ **P10a ZAMÍTNUTO JAKO NOČNÍ RAMENO 18.08. — Q1 pojistka ho zastavila.** Oprava napsána (`MCTSConfig::carrierBlockPrior`: blok/blitz na nosiče dostane prior floor 0,20 místo plochých 0,12, **jen když u pole nosiče máme aspoň tolik těl v dosahu co soupeř** — podmínka, ne preference, protože sražení nosiče míč UVOLNÍ a soupeř má v dosahu víc těl v 54,1 %). **Q1 na přeplněné desce:** podmínka funguje (nesporné pole 0,0 pp), ale **sporné pole se nehnulo taky — search si nosiče bere v 98 % už bez ramene**. **Korpus se jmenovatelem:** z **3 733** příležitostí jsme na nosiče udeřili v **81,5 %**, a bez souseda jsme na něj **6 143×** doblitzovali. ⇒ **Strop ~0,23 kola na zápas**, z toho část se udeřit nemá. Táž logika jako P8 (0,056 faulu/zápas). Kód zůstává za vypnutým flagem, testy 549/549. Doklad `evidence/p10a_q1_result_20260818.md`. ⚠️ **Neodpovězeno:** jestli je těch 81,5 % úderů dobře NAČASOVANÝCH a jestli se udeří SPRÁVNÝM tělem (P9). |
| **P13** | ⛔⛔ **ZMĚŘENO Q1 TESTEM 14.08. — OPRAVA NABÍDKY SAMA NESTAČÍ.** Postavená pozice: náš Slayer (ST3, Dauntless+Block) mezi **Black Orkem ST4** a **linemanem ST3**, 120 opakování na rameno, `diag_q1_target_choice_20260814.cpp`.<br><br>| | bloků zvoleno | **z toho Black Orc** |<br>|---|---|---|<br>| Dauntless v nabídce **OFF** | 84 | **0** |<br>| Dauntless v nabídce **ON** | 112 | **0** |<br><br>⇒ **Search si Black Orka nevybere ani jednou, ani když je mu nabízený.** Vybere vždy linemana vedle něj. Nabídka stoupla, volba se nezměnila.<br>⚠️ Počet bloků se zvedl 84 → 112, takže ta nabídka **něco** udělala — jen ne to zamýšlené: rozhýbala prohledávání a skončila u linemana. **Noční A/B by měřilo vedlejší efekt přidané volby, ne bití Black Orků.**<br>⛔ **NOČNÍ BĚH 14.08. PROTO ZASTAVEN** po ~5 minutách, ne po 14 hodinách. *(uživatel to předpověděl: „bojím se, že to toho Black Orka nevybere a měření bude nula")*<br>⇒ **P13 zůstává správnou opravou** (filtr oceňoval jinou akci, než jaká se provede) — **ale sama o sobě nemá co změřit.** Musí jít **spolu s úrovní 2**: cena cíle v prioru (**P15 / P10a**). To je přesně ten pětiúrovňový řetěz.<br>⚠️ **Výhrada:** jedna postavená pozice, 120 vzorků na rameno. 0/112 je průkazné pro **tuhle** pozici, ne důkaz pro všechny.<br>*(původní popis nálezu níž — platí, jen nestačí)*<br>⭐ **NABÍDKA BLOKU NEPOČÍTÁ S DAUNTLESS, PROVEDENÍ ANO** *(uživatel 14.08.: „soustředili jsme se na welfy a přitom bolí proti orkům — tam je Dauntless na ST4 orky úplně super plán")*.<br>`getBlockDiceCount` (`macro_actions.cpp:126`) počítá jen `Horns`, a jen u blitzu; **Dauntless nikde**. ⇒ Troll Slayer ST3 proti Black Orkovi ST4 se ocení jako **do kopce**, `dice` vyjde záporné a filtr `if (dice >= 2 || oneDieWorthOffering)` nabídku **zahodí**. Přitom `block_handler.cpp:386` Dauntless při provedení uplatní **správně** (před asistencemi, CRP, opraveno `9f98070`).<br>⇒ **Slayerovi se blok na Black Orka nikdy nenabídne**, ačkoli by se srovnal na ST4 a s jednou asistencí by z toho byly **dvě kostky pro nás**.<br>⭐ **Dauntless je nejsilnější právě proti ST4:** d6+3 > 4 ⇒ 2+ ⇒ **83 %**; proti ST5 67 %, proti Treemanovi ST6 jen 50 %. **Ork je jediný soupeř se čtyřmi ST4 hráči** — proti wood-elfovi je jeden Treeman, proto si toho nikdo nevšiml. Souvisí s tím, že proti orkovi dáváme **86 TD na 750 zápasů** proti 451 na skavena.<br>⚠️ **Třetí výskyt téhož vzorce za den** (po ceně hand-offu a po směru odsunu): **filtr oceňuje jinou akci, než jaká by se provedla.** ⇒ Projít `macro_actions.cpp` systematicky touž otázkou.<br>*(Zadáno Fablemu 14.08. jako doplněk k analýze orků — změřit, kolik bloků to bere a jestli to vůbec souvisí se ztrátami míče.)* | ✅ **ZMĚŘENO 14.→15.08., PROŠLO** — `evidence/weekend_result_20260817.md`. **dw-orc +4,08 pp** (SE 0,80; rameno vyskočilo ~1 739×/hru) · dw-sk **+2,28 pp** · orc-sk **−1,30 pp** (kontrola uvnitř 2 SE). Práh předregistrace splněn. ⛔ **ALE obě nuly se hnuly, ačkoli tam rameno prokazatelně NEBĚŽELO** (`cand_daunt = 0` v 6 000/6 000) ⇒ poctivý efekt je **dw-orc proti sdružené nule: +3,59 pp, ~3,4σ**, ne +4,08. Fable čekal +1–2 pp. **Zbývá: zapnout v produkci** (`dauntlessInOffer` je dál `default false`). |
| **P15** | ⭐⭐⭐ **PRÁH NABÍDKY BLOKU NEZNÁ CENU CÍLE** *(uživatel 14.08.: „kdyby třeba balon nesl Black Ork bez Blocku a my kvůli dvě proti po něm nešli")*.<br>Brána (`macro_actions.cpp:562`):<br>`bool oneDieWorthOffering = dice == 1 && att.hasSkill(Block) && !(ball.isHeld && ball.carrierId == att.id);`<br>`if (dice >= 2 \|\| oneDieWorthOffering) { ... }`<br>Ptá se, jestli **útočník** není náš nosič. **Nikde se neptá, jestli CÍL není soupeřův nosič.** A `dice` je záporné, když vybírá obránce ⇒ **blok do kopce se nenabídne NIKDY**, bez ohledu na terč.<br>⇒ **ST4 s míčem je pro 9 z 11 našich těl nedotknutelný**, přestože jeho sražení je nejcennější událost na desce. **Filtr tu volbu ani nepředloží — search nemůže zvážit, co nedostane.**<br>**Je to kompromis, ne jasná chyba:** blok do kopce dá soupeři ~**30,6 %** šanci vybrat si „Attacker Down" (naše Both Down kryje Block) = turnover, proti tomu stojí uvolněný míč. **Právě proto to má rozhodovat search, ne konstanta ve filtru.**<br>⚠️ **P13 to pokrývá jen zčásti** — Dauntless srovná Slayera proti ST4 na jednu kostku a s Blockem projde, ale **Slayeři jsou dva**; zbylých devět těl zůstane zablokovaných.<br>⛔⛔ **ZAMÍTNUTO JAKO VYSVĚTLENÍ ORKA (Fable §9.5, 14.08.)** — a je to oprava mého vlastního tvrzení z téhož odpoledne. Zóna „dosáhneme, ale nemáme 2 kostky" je proti orkovi 64,7 % kol, ale **98,2 % z toho je BLZ=1 — a ty akce SE nabízejí** (blitz na nosiče má prior +10 i do kopce; 1k sousední blok mají všichni naši přes Block). **Skutečná ne-nabídka (blok do kopce) je 1,8 % zóny**, konverze 1k na Throwera 0,109. ⇒ **Limit je fyzika, ne filtr.** P15 zůstává platnou opravou rozhodovací vrstvy, ale **na orka nemíří** a strop má malý.<br>⚠️ **Ponecháno jako záznam, co jsem tvrdil a čím to bylo vyvráceno** — původní (chybné) zdůvodnění níž.<br>⭐⭐ ~~ZOSTŘENO 14.08. odpoledne — není to okrajový případ, je to hlavní kanál.~~ Změřeno: **Guard v rozích klece vs. náš dostupný 2k blitz na jejich nosiče** — skaven 6,7 % / **54,4 %** · wood-elf 5,6 % / 43,4 % · human 20,6 % / 19,0 % · **ork 50,9 % / 7,5 %**. Monotónní a téměř dokonale inverzní: Guard v rohu znamená, že jejich obranná asistence **nejde zrušit značkováním**, takže proti orkovi jsme **do kopce skoro vždycky**.<br>⇒ **Těch 7,5 % není měření naší volby — je to měření toho, co nám filtr vůbec dovolí zvážit.** Proti orkovi se náš útok na jejich nosiče **prakticky nikdy nedostane do nabídky**, a to je přesně ten matchup, kde dáváme 86 TD proti 451 na skavena. **P15 tedy nemíří na okrajový případ, ale na hlavní příčinu našeho nejhoršího matchupu.**<br>⇒ Jejich nosič je přitom **nejměkčí cíl v týmu**: ork Thrower 78,5 %, human 74,4 %, skaven 48,1 % — ST3, AV7–8. Stojí uprostřed nejtvrdší ochrany, jakou mají.<br>⭐ **Pátý a nejobecnější výskyt vzorce dne:** u ostatních čtyř filtr neznal dovednost; tady **nezná cenu cíle vůbec**. Souvisí s **P10a** (plochý prior `BLOCK 15` pro všechny bloky) — je to týž defekt na dvou místech. | **OTEVŘENO — VYSOKÁ PRIORITA** |
| **P19** | ⭐⭐ **HODNOTA CÍLE JE VZÁCNOST × ROLE — a řetěz „srazit → faulovat → odstranit"** *(uživatel 14.08.: „někam tam bude patřit zničení Black Orka — a třeba i následný faul je hodnotnější než block na linemana, na kterého nemá cenu plýtvat faul")*.<br>Black Orků mají **4**, jsou **ST4 + Guard** a tvoří **50,9 % jejich rohů klece**. Linemanů mají **11** a jsou zaměnitelní. ⇒ Odstranit Black Orka degraduje jejich klec **natrvalo**; sražený lineman se nahradí.<br>**Faul to dotahuje:** je **1 na kolo** a riskuje vyloučení ⇒ patří na nejcennější ležící tělo. **P8** říká, že dnes bere **prvního ležícího v pořadí sousedních polí a nehodnotí nic** — takže se plýtvá na linemany.<br>⛔⛔ **HÁČEK, KTERÝ TU HODNOTU V NAŠEM MĚŘENÍ NIČÍ: T5.3 — zranění nepřetrvávají přes drive.** Po každém TD se staví 11 čerstvých. ⇒ **Odstraněný Black Orc je pryč do konce DRIVU, ne zápasu**, a celý řetěz je **systematicky podhodnocený ve všech našich A/B**, včetně dnešního Dauntless běhu.<br>⇒ **Pořadí oprav: T5.3 PŘED P19 a P8.** Dokud zranění nepřetrvávají, měří se attrition proti pravidlům, která ji zlevňují — a každý výsledek o bití je dolní odhad. | **OTEVŘENO** — Q2; blokováno na **T5.3** |
| **P16** | ⭐⭐ **ROH KLECE SE VYBÍRÁ BEZ OHLEDU NA VHODNOST TĚLA** *(uživatel 14.08.: „zakaž skavenům GR do rohu, jestli to umíš zobecnit")*.<br>Změřeno: **skaven staví 36,4 % rohů z Gutter Runnerů (ST2, AV7)** — nejhorší možné tělo do klece: neuassistuje, neudrží pole, a je to zároveň hráč, který je jinde nejcennější.<br>**Kód pravidlo má, ale ptá se na špatnou věc.** `eligibleCornerPlayer` (`cage_advance.cpp:59`) filtruje jen **spolehlivost aktivace** (Bone-head, Really Stupid, Wild Animal, Take Root, Secret Weapon, Ball & Chain). **Na vhodnost ani na cenu jinde se neptá.** A žije to jen se zapnutou branou — **v produkci vypnutou** ⇒ dnes rohy vznikají samovolně přes heuristiku (`macro_mcts.cpp:686`), která **počítá každé stojící tělo do 4 polí stejně**.<br>⭐ **ZOBECNĚNÉ PRAVIDLO** *(ne „zakázat GR", ale proč)*: **roh klece je tělo, které (a) pole udrží pod blokem a (b) je jinde méně cenné než to pole.** Preferovat **Guard → ST → AV**; vyhnout se **nejrychlejšímu volnému tělu** a **určenému nosiči** (Sure Hands / Catch / vysoké AG) — ti jsou cennější v pohybu.<br>Vyjde správně pro všechny rostery: skaven → linemani místo GR · ork → Black Orci · wood-elf → linemani místo Wardancerů · my → Longbeardi a Slayeři s Guardem *(co už děláme)*. ([[feedback_implement_the_rule_not_the_outcome]])<br>**Dvě místa k opravě:** ① `eligibleCornerPlayer` — doplnit vhodnost, ne jen spolehlivost *(účinné až se zapnutou branou)*; ② **escort člen heuristiky — vážit vhodností místo počítání těl** *(účinné hned, a na OBOU stranách)*.<br>⭐ **Zase „jedna oprava, obě strany"** — táž vlastnost jako u P10a: heuristika je slepá k tomu, KDO to tělo je. | **OTEVŘENO** | <br>⭐ **ZMĚŘENO 17.08. — longbeardy tam UŽ dáváme, ale AG3 těla tam pořád stojí.** Podíl kol, kdy hráč stojí v rohu klece: **Longbeard +Guard 20,2 %** · Troll Slayer +Guard 16,7 % · **Blitzer +Guard+Tackle 12,6 %** · **Runner +Block 11,5 %** · Longbeard 8,0 %. ⇒ Guard těla rohy drží nejvíc *(to je správně)*, ale **~čtvrtina kol našich dvou AG3 pozic se stráví jako nábytek klece** — a AG3 těla jsou jediní použitelní příjemci hand-offu (67 % vs 50 %). ⇒ **Pravidlo: roh patří Guard tělu se špatnýma rukama (Longbeard +Guard, Troll Slayer). Runner a Blitzer nejsou nábytek — jsou to příjemci** *(viz **P29**)*.
| **P17** | ⚠️ **HRÁČ S BLOCK+WRESTLE WRESTLE PŘI ÚTOKU NIKDY NEPOUŽIJE — natvrdo, ne volbou.** CRP: *„This player **may** use Wrestle when he blocks or is blocked … Both players are Placed Prone **even if one or both have the Block skill**."* Engine (`block_handler.cpp:494`): `attWrestle = hasWrestle && !hasBlock`.<br>⇒ Je to **volba, kterou kód udělal za nás** — a udělal ji špatně přesně tam, kde na ní záleží: **blok na soupeře s Blockem**, kde se „Both Down" jinak vzájemně vyruší a **nestane se nic**. S Wrestle by šli oba k zemi.<br>⇒ Navíc **vnitřně nekonzistentní**: u obránce (`defWrestle`) žádné takové omezení není.<br>**Proč to teď vyplavalo:** uživatel 14.08. — *„proti orkovi musíme mít někoho s Wrestle."* Má pravdu (Black Orci i Blitzeři mají Block ⇒ naše Both Down se vyruší; Wrestle to přebije a složí jejich ST4 tělo), ale **v dnešním enginu by to nefungovalo**, protože Longbeard má Block. **Bez P17 je ta rosterová změna k ničemu.**<br>⭐ **Šestý výskyt vzorce dne** — rozhodnutí zadrátované do resolveru místo vyhodnocení. | **OTEVŘENO — blokuje rosterovou změnu** |
| **P20** | ⭐⭐⭐ **PÁR NENÍ TÁŽ HRA S JEDNÍM PŘEHOZENÝM BITEM — a náš práh to nezná.** 15.08.: na `dw-sk` a `orc-sk` se rameno **ani jednou nespustilo** (`cand_daunt = 0` v 6 000/6 000 her, větev `macro_actions.cpp:156` je jediné místo rozdílu) — obě ramena tam běžela na **stejném kódu** — a přesto vyšlo **+2,28 pp (+2,3 SE)** resp. **−1,30 pp**. ⇒ **Předregistrovaný práh „PROŠLO" splnilo rameno, které nedělá nic.**<br>Příčina: orientace 0 hraje `seed*2`, orientace 1 `seed*2+1`, MCTS seedy se liší o 1 ⇒ pár jsou **dvě různé hry na spřízněných seedech**, ne tatáž hra. Redukce rozptylu je mnohem slabší, než na čem stály všechny naše prahy — **včetně „brána klece ZAMÍTNUTA −0,0297 (−2,0σ)"**.<br>⇒ **Každé A/B musí od teď obsahovat matchup s expozicí NULA a efekt se čte PROTI NĚMU**, ne proti nule. *(Souvisí se šumovým dnem ±5,3 pp a s tvrzením „harness je DETERMINISTICKÝ" — to platí o hře při daném seedu, ne o páru.)*<br>⚠️ **Přehodnotit ZAMÍTNUTÍ brány klece** touž optikou: běželo bez nulového ramene. | **OTEVŘENO — VYSOKÁ PRIORITA** |
| **P21** | ⛔⛔ **HAND-OFF: 0 VÝSKYTŮ VE 3 000 HRÁCH.** Fronta si 14.08. vymínila *„nejdřív ověřit hand-off kritérium na doběhlém korpusu — kdyby nefungovalo, nesmí být v baseline"*. **Neověřilo se**, a korpus, který v baseline **je**, má `HAND_OFF` **nula** (`BLOCK` 44,7/hru · `PASS` 0,30 · `CATCH` 1,97 · **`HAND_OFF` 0,00**) — přestože logování bylo přidáno právě proto (`3b11d33b`) a Q1 sweep dává na postavených pozicích **18,3 %**.<br>Dvě čtení, obě levná: **(a)** situace „Longbeard nese a vedle je volný Runner" je v reálné hře tak vzácná, že za 3 000 her nenastala ⇒ P5 je správná, ale bezcenná oprava; **(b)** makro cesta hand-offu se resolvuje jinudy než `pass_handler.cpp` a log ji nepokrývá ⇒ nevíme nic.<br>⇒ **Rozhodne to spočítání VÝSKYTU té situace ve snímcích korpusu**, bez dalšího běhu. | ✅ **ZODPOVĚZENO 17.08.** — `evidence/handoff_offered_never_chosen_20260817.md`. ⛔ **PŮVODNÍ ZÁVĚR (c) ZRUŠEN TÝŽ DEN.** Nula byla **vada exportu logu**: `bb_module.cpp:325` měl stráž `typeIdx < 21` proti 22 jménům ⇒ `HAND_OFF` se ukládal jako `UNKNOWN` (commit `3b11d33b` přidal jméno a stráž nezvedl). **Naše strana zahrála 130 hand-offů ve 3 000 hrách** (349 událostí celkem, 51 vedlo k TD v témž kole) ⇒ **hand-off ŽIJE**. Stráž opravena. ⚠️ Ověřoval jsem emisi eventu i poziční mapu jmen — obojí správně — a **stráž o řádek výš ne**. Původně jsem psal: ~~nabízí se 10,4× za zápas a nevybere se ani jednou~~ (počítadlo `takeHandOffOfferCount`, 50 her: 519 nabídek, 0 zahrání). Situace nastává v **329 kolech ze 3 000 her** (0,69 % kol, 7,3 % zápasů; podlaha — snímek je začátek kola), ověřeno i na čerstvé binárce. Log je v pořádku (`pass_handler.cpp:428` + poziční mapa v `bb_module.cpp` sedí). ⛔ Cestou nalezena vada nástroje: **Q1 sweep počítal `HAND_OFF` a `PASS` do jednoho čísla** — po rozdělení je těch 18,3 % opravdu předání (PASS 0). ⇒ **Vada se přesunula z NABÍDKY do VOLBY** (týž vzorec jako P13). **Patří k P15 / P10a**, ne k P5. ⚠️ Strop je malý (~0,1 akce na zápas) — spočítat napřed, než na to půjde rameno. |
| **P22** | ⛔ **KORPUS A JEHO BASELINE BĚŽELY NA JINÉM ENGINU.** Korpus 14.08. `engine@e273a369`, baseline `night_big_20260813/` `engine@9f98070c` — **šest commitů do `engine/`, z toho čtyři mění chování** (2× hand-off `38dcad6d`/`4f2c658d`, odmítnutí darovaného TD `eb231c5c`, čítače). Rozklad drivů navíc jel s opravou atribuce TD (`14c7d035`), kterou baseline neměla (proto tam je sekce ANOMÁLIE a v novém korpusu ne). ⇒ **A 17 % → 21 % a C 41 % → 37 % NELZE připsat Dauntlessu.** Předregistrované předpovědi vyšly (K33 76,6 → 78,9 % · C proti orkovi 59 → 57 % · REACH0 41,0 → 41,5 %), ale **nejsou přiřaditelné**.<br>✅ **Opraveno systémově:** `night_stamp_head` / `night_check_baseline` v `run_night_lib.sh`; otisk `ENGINE_HEAD` **doplněn zpětně** do obou korpusů. | **UZAVŘENO jako vada aparátu** *(nález o měně drivů zůstává NEZMĚŘENÝ)* |
| **P23** | ⭐⭐ **SPOUŠTĚČ NEPŘEŽIJE DRUHÉ SPUŠTĚNÍ — a 14.08. se spouštělo dvakrát.** Zabití po 5 minutách a znovuspuštění v 15:15 bylo **úsudkově správné**; vadný byl aparát: **①** zámek `mkdir` + `trap EXIT` nepřežije `kill -9` ⇒ druhé spuštění tiše skončí; **②** 12 shardů na `&` — zabití rodiče **nezabije děti**, sirotci píšou do týchž adresářů jako nový běh *(prošlo jen proto, že se shardy nestihly rozjet)*; **③** `chain.log` **nemá první spuštění**, noc vypadá jako jedno čisté — *snímek se vydává za stav*; **④** řádky se otevíraly `fopen(…,"a")` ⇒ znovuspuštění **přidá druhou sadu** a po deduplikaci podle seedu to i vypadá správně. Táž křehká kopie zámku byla v **devíti** spouštěčích. | ✅ **UZAVŘENO 17.08.** — `run_night_lib.sh` (PID zámek se sebráním starého · úklid dětí na EXIT/INT/TERM · číslované POKUS n · `fopen "w"`); **každý nový noční spouštěč ho musí sourcovat** |
| **P24** | ⭐ **VYHODNOCENÍ SE NESHODLO SAMO SE SEBOU.** Binárka tiskla práh `>= +0.03`, předregistrace říkala **±0,02**; hlavička attrition v mode 4 tvrdila `cageAdvance on/off` (v pondělí by se četlo rameno brány místo Dauntlessu); a řádek *„spustilo se to rameno vůbec?"* — **nejcennější řádek celého čtení** — **netiskl nikdo** a musel se 17.08. dolovat skriptem. | ✅ **UZAVŘENO 17.08.** — harness tiskne práh i s názvem dokumentu, `Dauntless ON/off`, a nově `ARM Dauntless: N offers … NEVER FIRED => TRUE NULL` |
| **P25** | ⭐⭐⭐ **AUDIT MĚŘICÍCH NÁSTROJŮ — „počítá tenhle čítač to, co tvrdí jeho jméno?"** *(uživatel 17.08.: „zase jsem se dočetl, že jsme měřili jinak, než jsme chtěli")*.<br>**Je to DRUHÉ KOLO auditu z 13.08.** To první mířilo na **kontroly plnění** (chybějící jmenovatel, prázdná množina = „splněno") a dalo `Check(ok,n,deg)`+N/A. Tohle míří na **diagnostické nástroje**: čítače, printy a A/B harness. Za jediný den (17.08.) se jich našlo **sedm**, všechny bez hledání:<br>**①** pár není tatáž hra ⇒ práh splnilo mrtvé rameno *(**P20**, opraveno CRN)*<br>**②** korpus a baseline na jiném enginu *(**P22**)*<br>**③** binárka tiskla práh `+0.03`, dokument `±0.02` *(**P24**)*<br>**④** hlavička attrition v mode 4 tvrdila `cageAdvance on/off` *(**P24**)*<br>**⑤** řádek „vyskočilo to rameno vůbec?" **netiskl nikdo** *(**P24**)*<br>**⑥** Q1 sweep počítal `HAND_OFF` a `PASS` **do jednoho čísla** *(**P21**, opraveno)*<br>**⑦** „12 nezávislých nullů" z 13.08. bylo **8 unikátních** — zbytek bit-identické duplikáty *(Fable 17.08.)*<br>⭐ **ZBÝVÁ PROJÍT** *(to je ta práce, ①–⑦ jsou hotové)*:<br>&nbsp;&nbsp;• **čítače měří vnitřek prohledávání, ne hranou hru** — `cand_daunt` 1 739/hru **nejsou bloky**, jsou to evaluace v MCTS. Kdekoli se takové číslo cituje jako „kolikrát jsme to udělali", je to špatně. **Projít všechny `take*Count()` a k jménu doplnit, CO počítají.**<br>&nbsp;&nbsp;• **`diag_rules_checks_*.py`** — druhé kolo po opravě z 13.08.: má každá kontrola jmenovatele a je N/A odlišené od „splněno"?<br>&nbsp;&nbsp;• **`diag_drive_failure_*.py`** — definice kategorií A/B/C/D1/D2: co přesně je „A"? Nikde to není napsané a čte se to jako „skórovali jsme".<br>&nbsp;&nbsp;• **ostatní `diag_*` binárky a skripty** — jedinou otázkou: *počítá to, co tvrdí jeho jméno, a na jakém jmenovateli?*<br>⚠️ **Pravidlo, které z toho plyne pro každý nový nástroj:** k číslu patří **jmenovatel** a **věta, co se počítá** — jinak se za měsíc přečte jako něco jiného. | ⏳ **Z VĚTŠÍ ČÁSTI HOTOVO 17.08.** — `evidence/instrument_audit_20260817.md`. ⭐ **NÁLEZ: čítač přeceňoval 186×** — `takeDauntlessRollCount` hlásil 349/zápas, skutečně odehraných Dauntless hodů je **1,88/zápas** (5 651 celkem, srovnalo 73,8 %, aspoň jeden v 60,3 % her — čísla, která šla citovat celou dobu). Padá i tvrzení, že *rozdíl nabídka vs. odebrání* říká, jestli si to search vzal — obě čísla jsou z vnitřku prohledávání. ⇒ **přejmenováno na `take*EvalsInSearch`** (jméno je dokumentace, kterou lidé opravdu čtou). Dál: **legenda kategorií A/B/C/D1/D2 se tiskne do `drives.txt`** (definice byly jen v `.py`, čte se ale výstup) a **legenda jednotek `n`** do kontrol (K33 = kola, K30 = příležitosti k dodgi — lišily se 5× a četly se jako srovnatelné). ⚠️ **ZBÝVÁ:** ostatních 20+ `diag_*` souborů + hlídání rebuildu za běhu. | **ČÁSTEČNĚ OTEVŘENO — vysoká priorita** |
| **P26** | ⛔ **`CHAIN_SCORE` JE POTVRZENĚ MRTVÝ — a je to vada PROVEDENÍ, ne volby** *(Fable 17.08., `evidence/fable_offered_not_chosen_20260817.md`)*. Makro se nabídne v **270 kolech** ze 48 000 a **řetěz se nezahraje ani jednou**: krok 1 (pass) spálí `passUsedThisTurn` a krok 2 (hand-off) pak `rules_engine.cpp:99` odmítne. Potvrzuje **P4** jako živý bug. ⚠️ Strop malý (270 kol / 3 000 her), ale oprava je v pravidlové vrstvě, ne v doktríně. | ✅ **OPRAVENO 17.08.** — a je to **PRAVIDLOVÁ CHYBA, ne doktrína**, což mění její váhu proti stropu. CRP, HANDING-OFF: *„The Hand-Off Action is added to the list of Actions like Move, Block, Blitz and Pass. A coach may only declare one Hand-Off Action per turn."* ⇒ hand-off má **vlastní limit na kolo**. Sdílel ale `passUsedThisTurn`, a to **v OBOU směrech**: přihrávka blokovala hand-off (`rules_engine.cpp:99`) a hand-off spálil přihrávku (`pass_handler.cpp:408`). ⇒ `CHAIN_SCORE` byl **nesplnitelný z definice**. Přidán `handOffUsedThisTurn`; každé makro hlídá svůj limit, `CHAIN_SCORE` oba. ⚠️ Existující test `NoPassActionsWhenPassUsed` tvrdil staré chování a **procházel jen proto, že spoluhráč nebyl soused** — opraven, doplněny 3 testy na obojí směr. **544/544 zelených.** |
| **P27** | **`BLITZ_AND_SCORE` se nabízí a nekonvertuje** *(Fable 17.08.)*. Nabídnuto v **1 123 kolech** (789 bez současné nabídky SCORE), nosič blitzoval 38×, **TD 2×**. Vada ve **VOLBĚ**. ⚠️ **Strop 0,26 kol/zápas**, a před psaním kódu se musí změřit, **kolik z těch kol je při vedení** — tam je cap prioru 0,02 na celou SCORE-rodinu **záměr** (doktrína stall), ne chyba. | **OTEVŘENO — napřed rozdělit podle skóre** |
| **P28** | ⛔ **OPRAVA P10a: PRIOR NENÍ PLOCHÝ** *(Fable 17.08.)*. Tvrzení o plochém prioru `BLOCK 15` **neplatí** — je to `greedyMacroRank`, používaný jen při leaf-lookahead. Skutečné priory mají floory **0,08–0,90** a capy, mj. **cap 0,02 na celou SCORE-rodinu při vedení** *(doktrína stall, tedy záměr)*. Jediné útočné makro bez flooru je `PASS_ACTION` (~0,05 po renormalizaci). ⇒ **Plochý prior mrtvá makra nevysvětluje; hlavní vrah je listová evaluace (Q).** Přeformulovat P10a i P15 tím směrem. | **OTEVŘENO — přeformulovat P10a/P15** |
| **P29** | ⭐⭐⭐ **PŘÍPRAVA PŘÍJEMCE NA POSLEDNÍ TAH — „elfí catcher" pro trpaslíky** *(uživatel 17.08.: „přidat specificky pro poslední tah, kdy je druhý Runner nachystaný vepředu, ať prodlouží dosah… a čím víc cílů, tím lepší šance, že pak jeden bude volný")*.<br>**Polovina toho už v enginu JE:** větev `emergency` v nabídce přihrávky **obchází bránu `swap` úplně** (`worthIt \|\| emergency`), takže **Runner → Runner se v kolech 7–8 nabídne**, když nosič na endzonu nedosáhne a příjemce ano. Pohyb před předáním taky funguje — **49,9 % hand-offů v korpusu předává až po pohybu** (průměrně 3,8 pole).<br>⛔ **Chybí ta DRUHÁ polovina: nikdo příjemce nepřipraví.** Změřeno na 3 000 hrách: kol 7–8 s naším nosičem **5 625** (1,88/zápas), z toho nosič sám nedosáhne v **78,5 %** — a **v 73,8 % těch kol NENÍ KOMU PŘEDAT**. Příjemce existuje jen ve **4,8 %** (volný ve 3,2 %), průměrně **1,22 kandidáta**. ⇒ Emergency větev je napsaná dobře a **nemá na koho střílet**.<br>⭐⭐ **STROP — a je to nejvyšší, jaký jsme dosud spočítali.** Ze 4 136 kol bez příjemce chybí nejlepšímu kandidátovi: **1 pole 6,5 % · ≤2 14,8 % · ≤3 27,1 % · ≤6 74,2 %**. Runner urazí za kolo 6 (s GFI 8). ⇒ konzervativně **≤3 pole = 1 121 kol = 0,37 na zápas** šancí navíc, volněji ≤6 polí = **1,02/zápas**. Při konverzi ~50 % je to **~0,18 TD/zápas proti dnešním 0,75 = +25 %**.<br>⚠️ **Háček v kvalitě příjemce:** nejbližším kandidátem bývá **Troll Slayer** (340×) a **Blitzer** (278×), Runner jen 252×. Slayer je **AG2** ⇒ chytá na 4+ (50 %). Příprava tedy musí tlačit dopředu **Runnera**, ne libovolné tělo — jinak se strop propadne.<br>⇒ **Úkol: doktrína „v kole 6–7 pošli druhého Runnera dopředu do dosahu endzony a nech ho volného."** Souvisí s ⑤ „čím víc cílů, tím větší šance, že jeden bude volný" — redundance proti markování. <br><br>⭐⭐ **ZPŘESNĚNO 17.08. — PŘIPRAVOVANÝ PŘÍJEMCE JE BLITZER, NE DRUHÝ RUNNER** *(uživatel: „druhý Runner je jediný, kdo může vyrazit dopředu — a být sražen?")*. Změřeno na 3 000 hrách, kola 6–7 (n = 5 729):<br><br>| | roh klece | v kontaktu | **VOLNÝ** | **LEŽÍ** | volný a doběhl by k EZ |<br>|---|---|---|---|---|---|<br>| Troll Slayer | 15,9 % | 12,9 % | **50,5 %** | 20,6 % | 0,47/kolo |<br>| **Blitzer** | 14,5 % | 14,7 % | **48,4 %** | 22,5 % | **0,47/kolo** |<br>| Longbeard | 12,0 % | 11,9 % | 43,5 % | 32,7 % | 0,60/kolo |<br>| **Runner** | 9,6 % | 11,3 % | **33,7 %** | **45,4 %** | **0,24/kolo** |<br><br>⇒ **Obava se potvrdila: druhý Runner LEŽÍ v 45,4 % kol 6–7** — je nejrychlejší (MA6) a má nejlepší ruce, ale **AV8** je nejměkčí brnění v týmu. **Není jediný kandidát, je ten nejhůř dostupný.**<br>⇒ **Blitzer je lepší připravovaný příjemce:** AG3 stejně jako Runner (**chytá na 3+, 67 %**), o pole kratší dosah (MA5), ale **AV9** a **volný dvakrát častěji**. Runner je první volba jen tehdy, když zrovna stojí.<br>⇒ Redundance („čím víc cílů, tím větší šance, že jeden bude volný") má konkrétní tvar: **4 těla s AG3** = 2 Runneři + 2 Blitzeři. Longbeard a Slayer jsou **AG2 ⇒ chytají na 50 %** a na tuhle roli nepatří. <br><br>⭐⭐ **CENA DOPOČÍTÁNA 17.08. — a je malá.** Pravidlo ze 14.08. žádá vedle výnosu i rozpočet, z něhož se platí. Kola 6–7 s **volným AG3 kandidátem na dosah endzony do dvou kol: 3 212 = 1,07 na zápas.** Z toho:<br>• **60,7 % — kandidát u nosiče NESTOJÍ ⇒ jeho odchod nestojí NIC** *(neubere roh ani zeď)*<br>• 39,3 % — kandidát je součástí těla u nosiče, **ale v 19,4 % všech těch kol je k dispozici JINÝ kandidát mimo klec**<br>⇒ **v ~80 % případů jde příjemce poslat dopředu bez dotyku na ochranu nosiče**; jen ~20 % vyžaduje volbu „roh, nebo příjemce".<br>Hustota v těch kolech: **2,21 našich sousedů nosiče z 8** a jen **0,48 soupeře** u nosiče ⇒ nosič v kolech 6–7 **není pod tlakem**, takže se tam neplatí ani expozicí.<br>⚠️ **Poctivá výhrada:** měřil jsem **sousedství**, ne `REACH0` (BFS „dosáhnou bez dodge"). Sousedství je proxy; přímý přepočet REACH0 s odebraným tělem zbývá.<br>⇒ **Strop 0,37–1,02 šance na zápas, cena ~0 ve 4 z 5 případů. Jediná dnešní položka, která prošla oběma testy** *(P8 padlo na stropu, P26 na stropu ale prošlo jako parita)*. | **OTEVŘENO — PRVNÍ V POŘADÍ, strop i cena změřeny** |
| **P30** | ⭐⭐ **CENA ODSTRANĚNÍ KONKRÉTNÍHO TĚLA — spočítat ZVLÁŠŤ pro Gutter Runnera a pro Black Orka** *(uživatel 17.08.: „pak by to chtělo zvlášť později vyčlenit výpočet na sražení a faul GRunnera nebo silnýho orka")*.<br>Dosud se hodnota cíle řeší **obecně** (P19 „vzácnost × role", P15 cena cíle) a obecné číslo nikoho nepřesvědčí. Tyhle dva cíle jsou **kvalitativně různé** a mají se počítat každý po svém:<br>• **Gutter Runner** *(skaven, MA9 AG4 AV7 Dodge)* — je to **stroj na TD**, ne článek formace. Odstranit ho znamená sebrat soupeři **schopnost skórovat**; AV7 je nejměkčí terč, ale Dodge ho drží mimo dosah. Starý odhad z P8: **4,4× lepší cíl** než lineman *(číslo je nutné přeměřit, pochází z doby před dnešními opravami)*.<br>• **Black Orc** *(ork, ST4 Guard, 4 v týmu, 50,9 % jejich rohů klece)* — je to **nosná konstrukce**, ne skórer. Odstranit ho znamená **degradovat klec**, a to trvale.<br>⭐ **Co se změnilo dnes a proč to má teď smysl:** **T5.3 je uzavřené** — zranění PŘETRVÁVAJÍ (0 návratů ze 648 casualty). Odstranění tedy platí **do konce zápasu, ne do konce drivu**, což hodnotu obou cílů **zvyšuje** a dělá z výpočtu smysluplnou věc. Do dneška se to počítalo proti pravidlům, která ji zlevňovala *(a ten předpoklad byl mimochodem taky chybný, viz rámec o násobiteli)*.<br>**Co má výpočet obsahovat:** (a) pravděpodobnost odstranění na pokus, rozpadlá na blok → armor → injury → casualty, pro každý terč zvlášť *(AV7 vs AV9, Thick Skull, Mighty Blow)*; (b) **kolik drivů v zápase ještě zbývá** v okamžiku odstranění — tím se výnos násobí; (c) co soupeři reálně ubude *(skaven: podíl jeho TD nesených GR; ork: čistota a rychlost jeho klece)*; (d) **cena pokusu**: faul je 1/kolo a riskuje vyloučení na dvojici, blok/blitz se platí z rozpočtu.<br>⏰ **SPOUŠTĚČ:** až (1) bude přepočítaná σ-tabulka na `corpus_baseline_20260817` *(dnešní tabulka je pozastavená, viz P25)* a (2) bude hotové **P29**. Do té doby se nepočítá — priority by se řadily podle metru, který se právě přeměřuje. | **ODLOŽENO — spouštěč uveden** |
| **P31** | ⚠️ **BLITZER SE PO BLOKU NEMŮŽE STÁHNOUT — `hasActed` se nastavuje nepodmíněně** *(nalezeno 22.07., ztraceno na 26 dní, znovuobjeveno 17.08. při úklidu paměti)*. `block_handler.cpp` staví `hasActed = true` na **osmi místech bez výjimky** (ř. 301, 314, 327, 348, 355, 368, 521, 548). U **Block Action je to správně** — blok pohyb neobsahuje. Vada je u **BLITZU**: CRP dovoluje hýbat se **před i po** bloku ze zbývající MA. ⇒ Blitzer, který cíl nesloží, **se nemůže stáhnout do bezpečí** a zůstane stát v jeho tackle zóně. ⚠️ **Napřed strop:** jak často má blitzer po bloku vůbec zbývající MA a jak často by se měl kam stáhnout. Bez toho se nesahá — pravidlo ze 14.08. | **OTEVŘENO — parita, čeká na strop** |
| **P18** | ⭐⭐ **DOVEDNOSTI JSOU V ENGINU POVINNÉ, ALE V PRAVIDLECH VOLITELNÉ** *(uživatel to psal už dřív; tehdejší odpověď byla „to bude složitější změna" a **nikde se to nezapsalo** — čtvrtý takový případ 14.08.)*.<br>CRP u řady dovedností říká **„may"**, tedy je to **volba kouče**, ne automatika: **Stand Firm** *(„may choose to not be pushed back")* · **Fend** · **Side Step** · **Wrestle** *(„may use Wrestle when he blocks or is blocked")*. Náš engine je vyhodnocuje **natvrdo** — `holdsGround()` rozhodne za Stand Firm, `attWrestle` za Wrestle *(viz P17)*.<br>⛔ **Bez toho nefunguje kombinace Block + Wrestle** *(uživatel 14.08.: „u toho, když máme i Wrestle i Block, to bez té úpravy fungovat nebude")* — a tím padá i balík proti Black Orkovi.<br>⚠️ **Ta tehdejší odpověď byla věcně správná:** udělat z každé dovednosti rozhodovací bod nafoukne akční prostor MCTS a je to projekt, ne úkol.<br>⭐ **ALE P17 na to čekat nemusí.** Nepotřebuje obecnou volitelnost, jen **jedno pravidlo**: použij Wrestle tehdy, když by se „Both Down" jinak vyrušilo — tedy když má obránce Block. To je pár řádků, ne framework.<br>⇒ **Rozděleno: P17 = cílená oprava (teď), P18 = obecná volitelnost (projekt).** | **OTEVŘENO — P18 projekt, P17 hned** |
| P4 | **`CHAIN_SCORE` je aktivní bug** — krok 1 (pass) spálí `passUsedThisTurn`, krok 2 (hand-off) se pak nenabídne ⇒ přihrávka se provede, předání selže, **tah je pryč**. Opravit nebo odstranit. | **PŘÍČINA ODSTRANĚNA `f5998575`** *(ověřeno 18.08.)* — pass a hand-off mají od 17.08. vlastní povolenky (`handOffUsedThisTurn`), takže krok 2 řetězu se **už nenabídne prázdný**. ⚠️ **Zbývá ověřit sám řetěz:** `expandChainScore` (`macro_actions.cpp:1670`) se od té opravy nikdy neproměřil a `CHAIN_SCORE` se v korpusu 3 000 her **nezahrál ani jednou** (jako hand-off, P21) ⇒ stav je **„příčina pryč, účinek nezměřen"**, ne UZAVŘENO. |
| P5 | **Hand-off pro výměnu nosiče** — filtr váží předání cenou přihrávky (33 %), i když by ho provedl jako hand-off (83 %); práh 0,5 zahodí i Runner→Runner (44 %) ⇒ nenabízí se žádné předání. Kritérium: **„nosič je špatný"** (AG≤2 bez Sure Hands a nedoběhne), ne „příjemce je lepší". Patch: `scratchpad/handoff_fix_plan.md` | **ZASTAVENO 18.08. — CHYBÍ KORPUS.** Doktrína napsána (spec **ČÁST 16**), ale ⛔ **`corpus_baseline_20260817_data` exportuje `HAND_OFF` jako `UNKNOWN`**: sběr začal 17.08. v **10:15**, oprava exportu `c943e8b8` je z **11:59** téhož dne (ověřeno: není předkem `5e5ab352`, na kterém korpus běžel). ⇒ **Jakékoli měření hand-offu nad dnešní baseline neplatí.** Podle pravidla 18.08. *(spravit kontroly vždy povýšit před měření)* se P5 nedělá, dokud nevznikne korpus na opraveném exportu. ⚠️ Ostatní dnešní měření to NEzasahuje (σ-tabulka, odsuny, blitzy, rohy stojí na `BLOCK`/`PUSH`/pozicích). ⭐ Co platí i tak: nabídka 10,4/zápas proti **130 zahraným** za 3 000 her (0,043/zápas) = **1 : 240** ⇒ vada je ve VOLBĚ, jen nestojí na nule. *(Předchozí stav: OTEVŘENO — POVÝŠENO uživatelem 14.08.)* |
| P6 | **Zobecnit item 14 na výběr cíle a na pickup** — BLITZ vybírá cíl podle surových kostek (blitzera podle kostek + cesty); PICKUP váží cenu sebrání, ne cestu k míči (připouští 2 GFI = 30 % pád). Nástroj existuje: `estimateApproachFailChance` (`macro_actions.cpp:206`), použitý jen 2× a oba u blitzu. Porušené vlastní pravidlo z 03.08. | **OTEVŘENO** |
| P8 | **Výběr cíle faulu** bere prvního ležícího v pořadí sousedních polí, nehodnotí nic. Přitom Gutter Runner je 4,4× lepší cíl a Thick Skull se nefauluje. | **OTEVŘENO** |  <br>⛔⛔ **STROP SPOČÍTÁN 17.08. — VÝBĚR CÍLE SE DĚLAT NEMÁ** (`diag_foul_choice_20260817.py`, 3 000 her, 11 175 našich faulů). Kód je horší, než se psalo: `macro_actions.cpp:805` po prvním nalezeném cíli **`return`** ⇒ na faulujícího se nabídne **právě jeden** faul a search **nedostane na výběr** (u bloků alternativu vidí a Black Orka si bere v 76,8–83,3 %). **Jenže vybírat skoro není z čeho:** v **74,8 %** faulů leží u faulujícího **jediný** cíl, průměr výběru je **1,32**. Black Ork je mezi dostupnými jen v **12,1 %** faulů a **už teď ho v 87,6 % z nich faulujeme**. ⇒ **Dokonalá oprava přesměruje 168 faulů za 3 000 her = 0,056 na zápas**, z nichž jen část někoho odstraní — a to se ještě dělí článkem 7 (**T5.3**). ⚠️ Netýká se to širší otázky *kdo má faulovat a kam se kvůli tomu postavit*, která zůstává nezměřená a je to jiný úkol. | **ZAMÍTNUTO — strop 0,056 faulu na zápas** |
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
| P7 | **Sdílený limit pass / hand-off** — CRP má dva nezávislé limity, engine jeden (`passUsedThisTurn`). ⚠️ **Jediný nález, který po opravě hraje PROTI nám** ⇒ měřit zvlášť. | **UZAVŘENO — a fronta to 26 dní nevěděla.** Opraveno **`f5998575`** *(„the hand-off has its own action allowance, and always did", 17.08.)*: `rules_engine.cpp:102` se ptá na vlastní `handOffUsedThisTurn`. ⚠️ **Ověřeno až 18.08. při psaní kapitol** — položka stála OTEVŘENO ještě den po opravě. Táž rodina jako T0.2: **fronta věří svému snímku víc než kódu.** ⇒ Před plánováním podle položky se ověřuje kód, ne řádek. Uzavírá i příčinu **P4**. |
| T5.2 | [6] **Kick-Off Return** — 3 vady | **OTEVŘENO — POVÝŠENO** *(fáze 1 fázového plánu na něm stojí, přestává být okrajové)* |
| T5.1 | [7] Tabulka výkopu — 5 z 11 výsledků vadných; 22 % výkopů zahazuje volné tempo | **ODLOŽENO uživatelem** — **spouštěč:** až se bude řešit fáze 1 / kick-off (tedy spolu s T5.2) |
| T5.3 | Zranění nepřetrvávají přes drive — každý TD staví 11 čerstvých ⇒ attrition čísla měří jen poslední drive | ✅ **UZAVŘENO — BYLO OPRAVENO UŽ 10.08.**, commit `918fc589` *(balík G, casualties survive the drive)*, a kniha to od té doby vedla jako otevřené. **Ověřeno empiricky 17.08., ne podle komentáře:** z **648 hráčů s událostí `CASUALTY` se za 600 her nevrátil na hřiště ANI JEDEN**. Návrat soupisky po TD (8→11) je **legální návrat KO** (CRP: 4+ na kick-offu). ⇒ **Tvrzení o 11 čerstvých po TD NEPLATÍ.** |
| T5.8 | Frenzy druhý blok se v hodnocení příležitostí nikde nemodeluje — týká se Slayerů | **OTEVŘENO** |
| **T5.14** | ⚠️ **MIGHTY BLOW SE PŘIČÍTÁ K OBĚMA HODŮM — ŽIVÁ CHYBA, HRAJE PROTI NÁM.** CRP: *„you only modify **one** of the dice rolls, so if you decide to use Mighty Blow to modify the Armour roll, you may not modify the Injury roll as well."* Engine (`block_handler.cpp:673`) nastaví **oba**: `defCtx.armourModifier += 1; defCtx.injuryModifier += 1;`. Má to být **volba kouče**, ne součet.<br>⭐ **SPECIFIKACE OPRAVY** *(uživatel 14.08.: „na zranění MB může, pokud se nevyužila aktivně na brnění")* — není to „vyhoď `injuryModifier`", je to **rozhodnout, kam ten jediný bonus dát, podle výsledku hodu na brnění**:<br>• brnění prorazí **i bez** `+1` ⇒ **nespotřebovat**, nechat na zranění<br>• neprorazí, ale s `+1` ano ⇒ **spotřebovat na brnění**, na zranění už ne<br>• neprorazí ani s `+1` ⇒ nespotřebováno, nerozhoduje<br>Totéž platí s Claw: `+1` smí posunout 7 na 8, kterou Claw vyžaduje — to je legitimní spotřebování.<br>⚠️ **Naivní varianta „vždycky na brnění" by soupeře oslabila víc, než pravidla chtějí.** Opravou se odebírá jen dvojí započtení, ne bonus sám.<br>**Claw je naopak správně** (`injury.cpp:181`: `armourRoll` už obsahuje modifikátory ⇒ „8+ po modifikacích" sedí) — a právě proto se chyba znásobuje: `+1` legitimně pomáhá dosáhnout osmičky pro Claw **a navíc** neoprávněně zvedá zranění.<br>**Dopad:** MB má v TV1200 **ork** (Blitzer), **human** (Blitzer + Ogre), **wood-elf** (Treeman); **skaven ani trpaslík ani jednoho** ⇒ **chyba jen bije nás**, ve 3 ze 4 matchupů. Táž rodina jako T5.7 Dauntless, jen opačným směrem.<br>⚠️ **OPRAVA NEOSLABUJE CLAW+MB** *(uživatel 14.08.: „u Skaven Blitzera s Block MB Claw je to síla i bez Piling On … my zůstáváme teď u silné kombinace podle 2016")*. Stacking na **brnění** — `+1` z MB pomáhá dosáhnout osmičky, kterou Claw vyžaduje, takže na AV9 stačí **7+** — je podle 2016 **správně a zůstává**. Odebírá se **jen** druhý dip na hod na **zranění** v témž bloku. Pozdější edice nerfly i Claw; **my u 2016 zůstáváme.** | **OTEVŘENO — snadná oprava, měřit zvlášť** |
| **T5.15** | **Piling On není implementovaný** — `SkillName::PilingOn` je v `enums.h:115`, ale v celém `engine/src/` se nevyskytuje. Mrtvá hodnota enumu. Dnes neškodí (nikdo v TV1200 ho nemá), ale je to druhá polovina **CLAWPOMB** a ožije s **T5.13**. | **ODLOŽENO** — **SPOUŠTĚČ: spolu s T5.13** |
| **T5.16** | ⭐⭐ **KTEROU EDICI VLASTNĚ MODELUJEME? — nikde to není napsané.** Náš zdroj `rules_crp2016.txt` se sám představuje jako *„BLOOD BOWL **COMPETITION RULES** … Competition Rules pack"*; řetězce `BB2016`, `Death Zone`, `Living Rulebook` v něm **nejsou ani jednou**. ⇒ **Je to CRP/LRB6 a „2016" v názvu je matoucí** — i paměť ho vede jako „CRP/LRB6 (BB2016)", což slučuje dvě různé edice.<br>**Kde se to rozchází** *(uživatel 14.08.)*: v **CRP je Piling On zdarma** (přehodí brnění nebo zranění, hráč jde prone); v **BB2016 stojí týmový reroll**, což CLAWPOMB prakticky zabilo. Kombinace Claw+MB je v obou stejná — na AV9 stačí 7+, protože `+1` z MB pomáhá k osmičce, kterou Claw vyžaduje.<br>⛔ **ROZHODNUTO — a bylo rozhodnuto UŽ DŘÍV.** Uživatel 14.08.: *„já jsem minule už hlásil, že chci pravidla 2016."* **Cílová edice je BB2016.** Rozhodnutí padlo dřív a **nikde se nezapsalo** — paměť vede zdroj jako „CRP/LRB6 (BB2016)", takže se ty dvě edice slily a stáhl se CRP. ⇒ **Všechny pravidlové audity od 07.08. běžely proti ŠPATNÉ edici.** Táž rodina chyby jako ztracených ~20 položek fronty: rozhodnutí bez zápisu. | **UZAVŘENO rozhodnutím** → viz **T5.17** |
| **T5.17** | ⭐⭐⭐ **OPATŘIT TEXT BB2016 A PŘEAUDITOVAT ROZDÍL.** Cílová edice je **BB2016** (T5.16), zdroj `rules_crp2016.txt` je **CRP/LRB6**. Kroky:<br>**①** sehnat autoritativní text BB2016 (Death Zone Season 1/2 + BB2016 rulebook) týmž postupem jako 07.08. — stáhnout PDF, rozebrat `pypdf`, grepovat; **nespoléhat na AI, ta edice míchá** (to je přesně, co se stalo);<br>**②** přejmenovat současný soubor na `rules_crp_lrb6.txt`, ať název nelže;<br>**③** projít text na změny **uvnitř zápasu**, které v seznamu nejsou;<br>**④** ověřit body 1–5 z `evidence/rules_edition_crp_vs_bb2016.md`.<br><br>⭐ **PŘEŠKÁLOVÁNO 14.08. — poplach z velké části odvolán.** Uživatel dodal celý přehled rozdílů (zapsán do `evidence/rules_edition_crp_vs_bb2016.md`; **posílal ho už dříve a nezapsal se** — třetí případ téhož dne). Po triáži proti tomu, co engine modeluje: **v zápase se edice skoro neliší.** Drtivá většina změn BB2016 je **liga a ekonomika po zápase** (MVP, Spiralling Expenses, Expensive Mistakes, Redrafting, Wizards, karty, měřítko 32 mm) — z toho **nemodelujeme nic** (ověřeno grepem: `inducement`, `SPP`, `MVP`, `treasury` nejsou v `engine/src/`).<br>⇒ **Všech ~15 dosud auditovaných pravidel je mezi edicemi beze změny.** Sahá na nás jen: **Piling On** (T5.15, odloženo) · **Argue the Call** (T5.18, nové) · Weeping Dagger a Human Catcher 60k (jen rostery, T5.13) · **Timmm-ber! wood-elfí Treemani NEDOSTALI** ⇒ náš je správně bez ní.<br>⚠️ Zbývá jediné skutečné riziko: **seznam je z AI a jeho úplnost není zaručená** ⇒ krok ③.<br>*(Fable na to netřeba — uživatel 14.08.)* | **OTEVŘENO — STŘEDNÍ**, malá ověřovací práce |
| **T5.18** | **Argue the Call není implementované.** BB2016: kouč smí zkusit zvrátit vyloučení za faul / Secret Weapon — `1` = kouč vykázán a −1 na Brilliant Coaching · `2–5` = platí · `6` = hráč jen na střídačku (tah **stále končí turnoverem**). Vyloučení za faul **modelujeme** (`foul_handler.cpp:64`, dvojice → ejected, Sneaky Git brání), odvolání ne. ⇒ Dnes jsme **přísnější**, než 2016 káže — ztrácíme hráče, které bychom na 6 udrželi. Týká se obou stran. | **OTEVŘENO — NÍZKÁ** *(faulujeme málo; P8 „výběr cíle faulu" jde napřed)* |
| T5.4 | [13] M1 přeběhnout — smazat falešný `M1_DONE`, přestavět `diag_m1` | **OTEVŘENO** |
| T5.5 | O1 **kopat, nebo přijímat** — volbu vůbec nemodelujeme; potenciálně větší páka než cokoli uvnitř kola | **OTEVŘENO** |
| T5.6 | O7 Underworld | **OTEVŘENO** |
| **T5.13** | **Přestavba testovacích rosterů — OTT skaven** *(uživatel 14.08.)*. Dnešní TV1200 skaven má GR s Dodge+Sure Feet (**11 polí/kolo**) a žádného Rat Ogra. Jednokolová hrozba potřebuje **`+MA` + Sprint**: MA10 + 3 GFI = **13 polí**. ⭐ **Nepotřebuje ani jeden double** — Sprint i Sure Feet jsou Agility a GR má A v normálním přístupu (CRP: `Gutter Runners 80,000 9 2 4 7 Dodge · GA · SPM`), `+MA` je zvýšení statu. ⇒ **Není to exotický roster, je to nejběžnější cesta vývoje GR.**<br>Rat Ogra **ne**: 160k, mutace jen na double, a bash-skaven se překrývá s orkem, kterého už měříme.<br>⭐⭐ **TŘETÍ a podle rozboru NEJNEBEZPEČNĚJŠÍ varianta: `Gutter Runner + Wrestle`** *(uživatel 14.08.)*. Dnešní skaven má **nástroj bez dosahu a dosah bez nástroje**: Lineman +Wrestle (MA7) ke kleci nedojde, Gutter Runner (MA9) dojde vždycky, ale ST2 proti našemu ST3 je do kopce a kostku vybíráme my. **Wrestle na GR ty dvě věci spojí** — a obchází sílu úplně: nepotřebuje ST4, **neruší ho Sure Hands** (to je jen proti Strip Ballu) a **nechrání před ním Block** (`block_handler.cpp:492`: na „Both Down" jdou oba k zemi, míč padá, a **není to jejich turnover**). Stačí mu s asistencemi na jednu kostku. Podrobně `evidence/matchup_asymmetry_20260814.md`.<br>⭐ **Čtvrtá varianta — jiný KANÁL, ne jiný nástroj: `Gutter Runner + Nerves of Steel`** *(uživatel 14.08.)*. Mechanika je **implementovaná a důkladně** — zohledňuje se na všech čtyřech místech, kde ji CRP zmiňuje: chytání (`helpers.cpp:146`), přihrávka a intercept (`pass_handler.cpp:64, 83, 250`), bomby, Throw Team-Mate. V rosterech je ale **jen na Pro Elf Catcherovi** (`roster.cpp:377`); **skavení GR ji nemá ani v základu, ani v TV1200**.<br>⚠️ **Je to na DOUBLE** — Nerves of Steel je *Passing*, a GR má normální přístup jen ke **G+A** (CRP: `Gutter Runners … GA · SPM`). Tedy vzácnější build než OTT Runner (Sprint i Sure Feet jsou Agility = obyčejný hod).<br>⭐ **Proč je nebezpečná jinak než ostatní tři:** není o odebrání míče, je o tom, že **nemusí projít naší zdí** — GR s NoS chytá v našich tackle zónách bez postihu, takže si míč **přihrají za klec** místo aby ho protlačili skrz. **Celá naše doktrína (klec, rohy, REACH0, značkování) řeší kontakt a proti přihrávce nedělá nic.**<br>⇒ **Samostatná otevřená otázka, kterou dnes neumíme zodpovědět:** jak často nám kdokoli přihrává přes klec a co s tím děláme? **Pasovací hrozba není v žádné z našich kontrol.**<br>⭐ **Druhá varianta: Blitzer `Block + Mighty Blow + Claw`** *(uživatel 14.08.: „je to síla i bez Piling On")*. Proti našemu AV9 stačí **7+** na proražení. Dnešní skavení Blitzeři mají Guard a Strip Ball+Tackle, tedy **žádnou** bash hrozbu. ⇒ **Dvě varianty skavena k otestování: OTT Gutter Runner (rychlost) a bash Blitzer (attrition).** Claw je Mutation ⇒ **na double**, MB je Strength ⇒ Blitzer má S v normálním přístupu.<br>Engine je připravený — `Sprint` funguje (`pathfinder.cpp:34,113` dává `maxGfi=3`), změna je **jeden řádek v rosteru**.<br>⚠️ **Proč to nejde teď:** změna soupeře rozbije srovnatelnost s korpusem 3000 her i se všemi dosavadními A/B.<br>⛔⛔ **VŠECHNY TŘI ÚPRAVY MÍŘÍ JEDNÍM SMĚREM** *(uživatel 14.08.: „všechny tři moje dnešní úpravy skavenů oslabují trpaslíky")* — OTT Runner, bash Blitzer i CLAWPOMB dělají skavena silnějším proti nám. ⇒ **Jejich přidáním spadne chess a bude to vypadat jako zhoršení naší AI, přestože se změnil jen metr.**<br>⚠️ **A není to dorovnání asymetrie, je to zavedení nové:** náš trpasličí TV1200 je vyvinutý stejně jako soupeřovy — 2× Longbeard +Guard, 2× Blitzer a 2× Slayer **+Guard+Tackle** (rohy klece), 2× Runner +Block (`roster.cpp:544`). Podvybavení nejsme.<br>⇒ **Každá změna rosteru vyžaduje NOVÝ baseline**; výsledky přes tu hranici se neporovnávají. Zapsat jako éru, ne jako pokračování.<br>⭐⭐ **BALÍK PROTI BLACK ORKOVI: DAUNTLESS + WRESTLE NA TÉMŽ TĚLE** *(uživatel 14.08.: „Black Orci mají i sílu i dost pohybu na klec — musíme na ně něco vymyslet, třeba Dauntless a Wrestle")*. Black Orc: **MA4 ST4 AG2 AV9, Guard+Block** — MA4 je stejně jako náš Longbeard, takže na klec mu to stačí, na honičku ne ⇒ **bít ho v kleci, ne v běhu**.<br>Řetěz: **Dauntless** srovná ST3 na ST4 (**83 %**) → s asistencemi 1–2 kostky → **Wrestle** přebije jejich **Block** na „Both Down" ⇒ **oba k zemi**. **Bez Wrestle se Both Down vzájemně vyruší a nestane se nic** (obě strany mají Block).<br>⭐ **Troll Slayer už Dauntless má** (Block+Frenzy+Dauntless+ThickSkull) ⇒ je to přirozený nositel obojího; stačí doplnit Wrestle.<br>⛔ **BLOKOVÁNO NA P17** — Slayer má Block, a engine hráči s Blockem Wrestle při útoku nedovolí. **Bez P17 je celý balík mrtvý.**<br>⭐ **PROTI ORKOVI POTŘEBUJEME WRESTLE** *(uživatel 14.08.: „proti orkovi musíme mít někoho s Wrestle — připomínka ke stavbě týmů později")*. Black Orci i Blitzeři mají **Block** ⇒ na „Both Down" se to s naším Blockem vyruší a **nestane se nic**. **Wrestle to přebije a složí jejich ST4 tělo** — a my za to vyměníme AV9 tělo. Wrestle je **General**, takže Longbeard ho vezme obyčejným hodem. A hlavně: **náš Tackle je proti orkovi mrtvý** (nemají Dodge) ⇒ je to slot, kterým stejně plýtváme.<br>⛔ **BLOKOVÁNO NA P17** — dnešní engine hráči s Block+Wrestle Wrestle při útoku vůbec nedovolí, takže by ta změna byla k ničemu.<br>⭐ **NAŠE STRANA PŘESTAVBY** *(uživatel 14.08.)*: **Mighty Blow má dostat co nejvíc trpaslíků** — dnes ho nemá **ani jeden**, zatímco ork, human i wood-elf ho mají. To dorovnává tři ze čtyř matchupů a je to jediná změna, která míří v NÁŠ prospěch. **TV smí vylézt až na ~1500** *(„když nám TV vyleze na 1500, tak OK")*, takže rozpočet na to je.<br>⇒ Sem patří i **T5.14 Mighty Blow (jedna kostka místo obou)** — uživatel ho 14.08. odsunul z dnešní noci sem: *„s MB si budeme pak hrát při změnách rosterů"*. Dnes je dopad malý (Treeman + Ogre, pár případů), po přestavbě bude velký na obou stranách. Oprava je **napsaná včetně testů**, čeká na větvi.<br>⛔ **ODLOŽENO uživatelem 14.08.** *(„to přestavování týmů máme v plánu na později"; „korigování a doladění rosterů odloženo až poté, co doběhne toto vše")*. **SPOUŠTĚČ: až doběhnou víkendové běhy a bude se smět měnit baseline.** Otázka, kterou to má zodpovědět: *„obstojí naše doktrína proti soupeři, který umí odpovědět v jednom kole?"* — protože **proti OTT buildu není bezpečné ani kolo 8**, a tím se bortí dostatečnost P11. | **ODLOŽENO** |

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
*(jediný oddíl, který se přepisuje — stav k 18.08.2026 ráno)*

## ⭐ Nález noci, podle kterého se řadí zbytek

**Brána klece ŠKODÍ — a poprvé to stojí na kontrole, ne na víře.**
`gate_crn_20260817/`, 6 000 párů, čteno v pre-registrovaném pořadí:
leak `MOVED WITHOUT THE ARM ACTING` **0 v 8/8 shardech** ⇒ delta se smí číst ·
`arm acted` **6000/6000** · `n_nonzero` **62,8 %** ·
**delta −0,0248 ± 0,0068 SE (−3,7σ), 8/8 shardů záporných**.
Pre-reg práh −0,015 ⇒ **ŠKODÍ**. ⇒ **T3.1 uzavřeno.**
⛔ **Není to replikace 13.08.** (3 commity enginu mezi tím) a ⚠️ výhrada platí:
**chybí jí plán trasy, ne schopnost** — kód se nezahazuje, vrací se přes **P3**.

⚠️ **Pre-registrovaná předpověď byla FALZIFIKOVÁNA** (čekalo se „kolem nuly“).
To je zdravé: předregistrace poprvé zabránila tomu, aby se výsledek dočetl zpětně.

## ⭐⭐ Druhý nález noci: spravili jsme SPUŠTĚNÍ noci, ne její ČTENÍ

17.08. se opravil spouštěč a přibyl per-pair leak test — obojí v noci fungovalo.
**Čtecí strana ale selhala na pěti místech naráz** (T2.10–T2.14). Nejhorší dvě:

* `chain.log` vytiskl **„(harness nic netiskl — stará binárka?)“** přesně na tom
  řádku, který předregistrace označuje za nejcennější ranní čtení — a byl to
  **falešný poplach**: test proběhl a byl čistý 8/8. Grep hledá mode-4 vzor.
* **Nic neslučuje shardy.** Noc skončila bez výsledku; 6 000 párů leželo jako
  osm jednotlivě neprůkazných čísel a součet musel udělat člověk ráno.

⇒ *Kontrola, kterou nikdo nepřečte, a výsledek, který nikdo nespočítá,
se od chybějící kontroly a chybějícího výsledku neliší.*

## Dnes — pořadí

| | co | proč teď | stroj? |
|---|---|---|---|
| ~~1.~~ | ✅ **ČTECÍ STRANA NOCI SPRAVENA — `056f85ed`.** T2.10 (ARM grep) · **T2.11 `night_summarize.py`** *(sloučená delta · sdružená SE · empirická SE mezi shardy · počet záporných shardů · součet leaku · `n_nonzero`, **v pořadí čtení z předregistrace**; při leaku deltu NEVYTISKNE)* · T2.12 (`THRESHOLD` jako vstup běhu) · T2.13 (preflight sonda na 1 páru, 47 s). `test_night_lib.sh` **14/14 → 29/29 zelených**. Ověřeno zpětně: na `gate_crn_20260817` vrátí přesně to, co ráno spočítal člověk; na `gate_measure_20260813` (starý harness) správně odmítne vynést verdikt. **Zbývá T2.14** — předpovědi se stále nekonfrontují s výsledkem. | hotovo |
| **2.** | **P30 — přepočet σ-tabulky** na `corpus_baseline_20260817_data` (3 000 her, COLLECT_DONE, otisk enginu 5e5ab352) | ⏰ **spouštěč splněn dnes v noci**; σ-tabulka je od 17.08. POZASTAVENÁ a **nesmí řadit frontu**, dokud se nepřepočítá | ne |
| **3.** | **T2.12** — práh jako vstup běhu, ne konstanta ve zdrojáku | dvě různá čísla (+0,03 vs ±0,015) v jedné noci | ne |
| **4.** | **P21** — výskyt „nosič ≠ Runner a vedle volný Runner“ ve snímcích korpusu | rozhodne (a)/(b) bez běhu a odblokuje čtení P5; teď už je na čem (nová baseline) | ne |
| **5.** | **T5.3** — zranění nepřetrvávají přes drive | ⭐ násobitel celého attrition programu | ne |
| **6.** | vybrat a spustit **noční A/B** *(kandidáti: P2+P9c · blitz roh vs zeď · P10a; **P17 Wrestle vyřazen** — je to parita, ne zlepšení)* | tři okna už propadla | **ano, 14 h** |

⛔ **Podmínka na bod 6 se ROZŠIŘUJE.** Dosud: nulová kontrola + `run_night_lib.sh`.
Nově navíc: **běh se nespouští, dokud neumí sám vytisknout sloučený výsledek**
(T2.11) — jinak vyrobíme další noc, kterou nikdo nepřečte.
⚠️ A do předregistrace patří **jen předpovědi, na které ten běh umí odpovědět**:
noc 17.→18.08. měla `CORPUS=0`, takže dvě ze šesti byly od začátku nezodpověditelné (T2.14).

## Rozpočet párů — POZOR, změna pravidla

**CRN u všudypřítomného ramene skoro nepomáhá: redukce SE ~6 %**, ne slíbených
15–25 %. Identických párů bylo **37 %** *(víc, než se čekalo)*, a přesto se SE
skoro nehnula — ⭐ **páry, které se hnou, se hnou víc** (podmíněné SD páru 0,66
vs nepárová 0,56). ⇒ **Rozpočet párů se NESMÍ počítat z podílu identických párů**;
plánuje se podle šumového dna BEZ CRN (2 pp ⇒ ~5 300 párů) a CRN se bere jako bonus.
U **vzácného** ramene platí dál, že CRN je zásadní (mrtvé rameno dá exaktní nulu).

## Stolní práce *(nesoupeří o stroj, tady je úzké hrdlo)*

| | co | proč |
|---|---|---|
| **0.** | ⭐⭐⭐ **PRAVIDLO 18.08.: OPRAVA KONTROLY SE VŽDY POVYŠUJE PŘED MĚŘENÍ** *(uživatel: „spravit kontroly vždy povýšit před měření“)*. Neplatí jen dnes — je to **pořadí, které platí pokaždé**. Měření vadnou kontrolou se platí dvakrát a **umí dát radu s obráceným znaménkem** (K33). ⇒ Než se spustí jakýkoli běh: *umí ta kontrola zapsat, co potřebuju číst?* | řadí vše pod sebou |
| ~~0a.~~ | ✅ **P1 HOTOVO 18.08.** — K33/K34/K35 zapisují i počet (`← metr`); ⌀ 1,62 bloku · 1,84 REACH0 · 0,95 FB2. | hotovo |
| ~~0b.~~ | ✅ **T2.14 HOTOVO 18.08.** — předregistrace je vstup běhu; spouštěč odmítne předpověď, na kterou běh neumí odpovědět, a shrnutí tiskne TREFA/MIMO/NEZODPOVĚDITELNÁ. Testy 35/35. **Čtecí strana noci je tím kompletní.** | hotovo |
| ~~0c.~~ | ✅ **K9b HOTOVO 18.08.** — `corridorResistance()` mimo plánovač, počítá se každé kolo (⌀ 1,44 při vypnuté bráně). T0.1 tím má svůj předpoklad. | hotovo |
| **1.** | **audit filtr vs. resolver** | pět nálezů za den bez hledání |
| **2.** | **P10a** cena volného míče podle toho, kdo je blíž a kdo rychlejší | spraví naše rozhodování i to, že soupeř nehraje svou rasu |
| **3.** | **P15** práh nabídky podle ceny cíle | ST4 s míčem je dnes pro 9 z 11 těl nedotknutelný |
| **4.** | **P14** Wrestle do výběru kostky a do rerollu | bereme „Both Down" v přesvědčení, že nás Block chrání |
| **5.** | **P1** K33/K34 na spojité | levné, opravuje metr |
| **6.** | **P3** fázový plán trasy | bez fáze nejde odlišit chybu od záměru |

## Zadání pro Fable — **kolik našich verdiktů přežije nulové rameno** *(SPUŠTĚNO 17.08. ráno)*

⏳ **Běží.** Výstup půjde do `evidence/fable_null_arm_audit_20260817.md`.

**Otázka:** *Které z dosavadních A/B tohoto projektu by při čtení proti
nulovému rameni dopadly jinak?* Vstup: uložená ramena (brána klece, balík G,
M1, era, Dauntless) + `dauntless_ab_20260814/*_s*/diag_dauntless_rows.jsonl`,
kde jsou **dvě nezávislé nuly na 6 000 hrách** — dosud jediné, co o podlaze
aparátu víme napřímo.

Co má vrátit: **(1)** empirickou podlahu párové delty per shard i sdruženě;
**(2)** přepočet každého historického verdiktu proti té podlaze, jmenovitě
**zamítnutí brány klece**; **(3)** kolik párů je doopravdy potřeba na 2 pp;
**(4)** jestli jde pár utáhnout (sdílený herní seed, shodné MCTS seedy) tak,
aby se rozptyl skutečně redukoval — to by zlevnilo **každý** další běh.

**Proč tohle a ne attrition:** nepotřebuje stroj, a dokud neplatí metr, je
každý další 14hodinový běh sázka. Audit měřicího aparátu byl nejcennějším
nálezem 13.08.; **P20 je jeho druhé kolo.**

## Odložené se spouštěčem *(nesmí se ztratit — proto i tady, ne jen v tabulce)*

* **FABLE 18.08. — VYNECHÁNO ZÁMĚRNĚ** *(uživatel: „na dnes asi pro fable nic — když už běží noc a zadání máme frontu“)*. Není to zapomenutí: den vyrobil víc nálezů, než se stihlo strávit, a Fable analýza by soutěžila o CPU s běžící nocí.
  ⏰ **Spouštěč:** po ranním čtení noci 19.08. **Tři kandidáti, seřazení podle toho, co dnešek otevřel:**
  1. ⭐ **„Kolik rohů klece je vlastně optimum?“** — σ-tabulka 18.08.: počet rohů **−2,1σ**, počet ČISTÝCH rohů **−0,2σ** a v půlkách mění znaménko, zatímco počet ŠPINAVÝCH je **−6,8σ**. ⇒ **Možná jsou 4 rohy špatný cíl** a celá klec se má stavět menší. Zapsáno jako otevřená otázka ve spec **15.8**.
  2. **B3 vs B2** — odmarkovat vlastního nosiče proti prorazit zeď. Obojí je patro „vpřed“ v žebříčku blitzu (**ČÁST 14**) a **nikdy se neměřily proti sobě**.
  3. **Je 26,4 % blitzů „jinam“ chyba?** Polovina těch cílů stála **hned vedle nosiče** (eskorta), což může být záměr, ne únik.

* **P30 — cena odstranění Gutter Runnera a Black Orka, každý zvlášť** *(uživatel 17.08.)*.
  ⏰ **Spouštěč:** přepočítaná σ-tabulka na `corpus_baseline_20260817` **+** hotové P29.
  Teď se nepočítá, protože metr se právě přeměřuje.
* **T5.13 přestavba rosterů** — ⏰ po doběhnutí víkendových běhů.
* **T5.15 Piling On · T5.18 Argue the Call** — ⏰ s přestavbou rosterů.
* **P27 `BLITZ_AND_SCORE`** — ⏰ napřed rozdělit 789 kol podle skóre; cap 0,02 při vedení je záměr.

## Projekty (ne úkoly)

* **T5.13 přestavba rosterů** — MB na co nejvíc trpaslíků, TV až 1500.
  Skavení varianty: OTT Runner · bash Blitzer · **GR s Wrestle** · GR s Nerves of Steel.
* **Soupeř nehraje svou rasu** — část **(A)** je levná a splývá s P10a;
  část **(B)** je vlastní projekt a **začíná skavenem**.
* **T6 učení** — až po repertoáru, s úplnou procedurou jako nulovou hypotézou.

## Co se uzavřelo o víkendu

**P13 PROŠLO** *(čeká na zapnutí)* · **P23** spouštěč *(`run_night_lib.sh`)* ·
**P24** vyhodnocení *(práh, popisky ramen, řádek „vyskočilo to vůbec?")* ·
**P22** otisk enginu u korpusů *(zpětně doplněn)*.

⚠️ **T1 (repertoár) se nehnul** od 14.08. — S7 boxing-in, O6, S5.3/S5.4
a chybějící situace z T1.2 čekají dál.
