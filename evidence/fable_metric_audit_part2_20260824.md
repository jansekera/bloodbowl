# AUDIT METRŮ — ČÁST 2 (Fable, 24.08.2026)

**Zadání:** `evidence/fable_brief_metric_audit_part2_20260824.md` — dokončit zbytek
auditu z 21.08. (`evidence/fable_metric_audit_20260821.md` je VSTUP, neopakuje se).
Hotovo z části 1: oddíl 0 (jedna perspektiva, žádné duální vyhodnocení) a celá
úloha (a) — 73 rysů, bilance 34 párových / 13 neutrálních / 26 jednostranných,
generativní mechanismus (blok [56–69]: 1 zrcadlo ze 14 rysů).

**Otázka části 2:** platí týž mechanismus („nový metr se přidá z perspektivy
právě řešeného problému a zrcadlo se nepřidá nikdy") i mimo hodnotovou funkci —
v diagnostice TurnLog / diag_*.py / σ-tabulce a v kontrolách K*?

**Stav dokumentu:** DOKONČEN (poslední řádek `HOTOVO`). Psán průběžně;
kdyby `HOTOVO` chybělo, čtěte jako useknutý — co tu je, platí.

**Metoda:** audit čtením kódu. Engine se nemění, dlouhé běhy se nespouští.
Čísla, pokud nějaká, jen z hotových dat; u každého uvedeno, z jakého enginu jsou
(`corpus_baseline_20260819_data` = starý engine, ilustrace mechanismu;
`crosses_20260821_data` = známá vada P57/TA1, nadhodnocené rerolly).

---

## Pracovní deník

- [x] inventura: kde žije TurnLog, diag_*.py, σ-tabulka, kontroly K*
  - TurnLog: `engine/include/bb/game_simulator.h:55`, plnění `engine/src/game_simulator.cpp:660` (captureTurnSnapshot), export `engine/python/bb_module.cpp:225` (get_turn_logs)
  - plán: `engine/include/bb/turn_plan_record.h`
  - koridor + tempo: `engine/src/cage_advance.cpp:60–111`
  - rozhodčí K*: `diag_rules_checks_20260812.py` (K29/K29full/K29rule, K9a/K9c/K9x, K30/K30b, K31, K33, K34, K35, K36, K38/K38b); K32 v `diag_blitz_on_carrier_20260818.py`; K9f v `diag_k9_phase_20260819.py`
  - σ-tabulka: `diag_drive_predictors_20260813.py`; definice E1/E2 (REACH0/FB2): `diag_exposure_scan_20260812.py`
- [x] (b1) TurnLog — pole razítkovaná do korpusu: tabulka strana/protějšek/dopad
- [x] (b2) diag_*.py / σ-tabulka — co se z polí skutečně čte a jak
- [x] (b3) N/A a nuly — koše, které se nepočítají a čtou se jako fakt
- [x] (c) kontroly K* — tabulka
- [x] druhá osa: počet/kvalita
- [x] druhá osa: stav/změna
- [x] druhá osa: dosah/vzdálenost
- [x] VERDIKT — platí mechanismus z (a) i mimo hodnotovou funkci?
- [x] POŘADÍ OPRAV podle dopadu (pozorovací vs. zásah do hodnocení)
- [x] co nejde rozhodnout čtením — zadání na měření

---

## (b) Diagnostické metry: TurnLog a diag_*.py / σ-tabulka

### (b0) Strukturní fakt, který mění čtení celé části (b)

**Producent (TurnLog) je z konstrukce OBOUSTRANNÝ; jednostrannost žije
v KONZUMENTECH.** TurnLog se razítkuje jednou za KAŽDÉ týmové kolo
(`game_simulator.cpp:794`) a veze oba rostery (`homePlayers`/`awayPlayers`),
události obou stran a — což je klíčové — koridor/tempo **z perspektivy právě
aktivního týmu** (`game_simulator.cpp:681–698`: razítkuje se, když nosič patří
aktivnímu týmu). Takže korpus UŽ DNES obsahuje „soupeřův odpor koridoru proti
nám" (v jeho kolech, kde náš tým hraje roli soupeře smyčky) — jen ho žádný
skript nečte: **každý diag skript začíná `if active_team != ours: continue`**
(`diag_rules_checks_20260812.py:370`, `diag_drive_predictors_20260813.py:41`,
`diag_exposure_scan_20260812.py:241`, `diag_blitz_on_carrier_20260818.py:41`).

To je jiný mechanismus než v hodnotové funkci z části (a): tam zrcadlo
**neexistuje v datech** (soupeřův Guard se ani nesbírá), tady zrcadlo v datech
**leží a nikdo se na něj nikdy nepodíval**. Důsledek pro opravy: většina
zrcadel v diagnostice je **POZOROVACÍ** změna (přečíst i soupeřova kola),
ne zásah do enginu.

### (b1) Pole TurnLogu (co veze korpus)

| metr (pole) | strana/osa | protějšek existuje? | záměr / díra | dopad |
|---|---|---|---|---|
| half/turn/active_team/skóre | neutrální | — | ✓ | — |
| home_players/away_players (id, x, y, state, has_ball, name, ma, st, ag, av) | obě | ✓ | ✓, ale **skilly se nevezou** (jen v `name` u části pozic) | K38 nemůže vyhodnotit klauzuli Jump Up (vědomě vynecháno, `diag_rules_checks:584–594`); cena dodge v K30 se rekonstruuje tabulkou `DODGE`/`TACKLE` ručně přepsanou z `roster.cpp` — křehké vůči každé změně rosteru |
| events (obou stran) | obě | ✓ | ✓ | žádný event nemá tým — atribuce přes `player_id`; BLITZ neexistuje jako typ (K32), rekonstruuje se z nesousedství (`diag_blitz_on_carrier:55–66`) |
| plan.* (13 polí) | jen my (plánovač běží jen pro hledajícího) | ✗ | **N/A past č. 1** | verdict=NOT_CONSULTED ve 100 % kol ⇒ všechna čísla nula a rok se četla jako fakt; export `bb_module.cpp:261–277` je vypisuje VŽDY, i když nic neběželo — nulu od hodnoty odliší jen kontrola `verdict` |
| corridorResistance | **jen soupeřova těla** před naším nosičem, jen stojící | ✗ „naše těla v koridoru" (P53, NEOPRAVENO) | **DÍRA** (producent) | po faulu, kterým jsme koridor ucpali vlastními asistenty, metr ukáže zlepšení; navíc **ležící soupeř v koridoru je neviditelný**, ačkoli příští kolo vstane (viz druhá osa: stav/změna) |
| corridorStrength | dtto, součet ST | ✗ dtto | DÍRA (producent), jinak správný krok počet→kvalita | dtto; ST bez Guard/skillů je pořád jen polovina kvality |
| requiredPace | jen náš nosič | ✗ (viz b0 — soupeřova kola v korpusu jsou) | **metr měnící význam podle stavu** | `usable = max(1, turnsLeft − 1)` (`cage_advance.cpp:99`, RESERVE_TURNS=1) ⇒ v posledních 2 kolech je dělitel 1 a veličina = zbývající vzdálenost; NEPRŮMĚROVAT přes kola |
| achievablePace | jen náš nosič | ✗ | díra kvality | = MA nosiče + 2 GFI − penalta, penalta `min(2,(resistance+1)/2)` z POČTU těl (kvalita-slepá) a **ignoruje fázi KLEC** (strop MA nejpomalejšího rohu, pravidlo z 19.08. zná `phase_floor`, engine ne) ⇒ pro klec systematicky nadsazené |
| distToEndzone | jen náš nosič | ✗ | záměr-ish | přímá vzdálenost po x, ne dosah — pro účel (rozvrh) přijatelné |
| turnover/touchdown | naše kolo | ✓ (flag je per kolo, obě strany mají svá kola) | ✓ | — |

**Sentinel −1 vs. nula:** koridor/tempo používají −1 = N/A. σ-tabulka to filtruje
správně (`_cr >= 0`, `diag_drive_predictors:98`). Riziko trvá u každého nového
konzumenta: 0 je legitimní hodnota (prázdný koridor), −1 ne, a `int8` sentinel
se při neopatrném průměrování tiše přimíchá.

### (b2) σ-tabulka (`diag_drive_predictors_20260813.py`)

Jednotka = DRIVE (jen plné, ≥7 našich kol — výběr je přiznán v hlavičce
výstupu). Cílová proměnná `scored` = my jsme skórovali. Všechny prediktory:

| metr | strana/osa | protějšek existuje? | záměr / díra | dopad |
|---|---|---|---|---|
| K33_blok, blokůdo | naše bloky v našem kole | ✗ soupeřovy bloky proti nám nikde | DÍRA (konzument) | „bloků/kolo +10,4σ" nelze číst jako „bijeme dost", dokud nevíme, kolik bere soupeř; čísla ze starého enginu = ilustrace |
| rohů_všech/ČISTÝCH/ŠPINAVÝCH, K29_čisté, PRAVIDLO_klece | naše klec | ✗ soupeřova klec (čistota jeho rohů = naše šance) | DÍRA (konzument) | doktrína „jak bránit klec" nemá metr |
| odpor_koridoru | soupeř před naším nosičem | ✗ (data v korpusu jsou, viz b0) | DÍRA (konzument) | −9,6σ interpretace „zeď prolomíme/obejdeme" stojí na počtu těl; kvalita (corridorStrength) se razítkuje od 21.08., **v σ-tabulce ještě není** |
| K34_reach0, REACH0_počet | jejich dosah na našeho nosiče | ✗ náš dosah na jejich nosiče (σ-tabulka ho nemá; jednorázově P33) | DÍRA (konzument) | obrana bez metru — stejná slepota jako rysy [40]/[63] v hodnotové funkci |
| K35_fb2 | jejich ≥2kostkové bloky na nás | pozoruhodné: tohle JE zrcadlo rysu [65] hodnotové funkce | ✓ jako veličina | viz verdikt: každý systém drží opačnou půlku páru a žádný nemá obě |
| K9a/K9c/K9x, Δx, K9c_rezerva | náš rozvrh | ✗ soupeřovo tempo | DÍRA (konzument) menší | bez soupeřovy reference nevíme, jestli 1,93 pole/kolo je málo obecně, nebo málo pro klec |

⚠️ `K9a_splněno` má touž vlastnost jako requiredPace: `need =
ceil(vzdálenost / turns_left)` — v posledním kole je need = celá vzdálenost.
Pro PODLAHU je to záměr (dluh opravdu roste), ale průměr residuí
`K9c_rezerva` přes kola drivu míchá koly s různým významem need.

### (b3) N/A a nuly (pravidlo domu: „když je koš N/A větší než n, nález je v tom N/A")

1. **plan.*** — pořád exportováno vždy a celé nulové, NOT_CONSULTED 100 %.
   Rozhodčí to dnes přiznává (tiskne rozdělení `plán:`), ale každý budoucí
   konzument má před sebou 13 numerických polí, která vypadají jako data.
2. **corridorResistance = −1** ve všech kolech, kdy nedržíme míč — tj.
   ve VĚTŠINĚ kol (obrana + rozehrávka). Metr „jak těžká je cesta vpřed"
   tedy N/A přesně tam, kde se rozhoduje obrana. To není chyba sentinelu,
   to je hranice definice — ale znamená to, že σ-řádek `odpor_koridoru`
   popisuje jen útočnou podmnožinu drivu a nikdo to u čísla neříká.
3. **K34.skip() bez stojícího nosiče** a **K38.skip() pro ležícího nosiče** —
   správně N/A, ale: kola, kdy je náš nosič NA ZEMI, jsou přesně ta
   nejhorší, a nevozí je ŽÁDNÝ metr expozice (REACH/BLZ/ESC se počítají jen
   pro stojícího). Koš N/A tu je malý, ale koncentruje selhání.
4. **BLZ = None** („nikdo nedosáhne") se v korelacích tiše vynechává
   (`r.get(p) is not None`) — bezpečné pro Pearsona, ale znamená, že
   „nedosažitelný nosič" jako NEJLEPŠÍ stav z metriky vypadl; bucket tabulka
   ho zpět přidává jen u BLZ, ne u korelací.
5. **Eventy, které neexistují, vypadají jako nuly chování** — třída doložená
   třikrát: HAND_OFF exportovaný jako UNKNOWN (P21, `bb_module.cpp:331–340`),
   STAND_UP nelogovaný do 21.08. (`game_event.h:27–35`), BLITZ dodnes
   neexistuje (K32). U všech tří platilo „nula v logu ≠ nula ve hře".

## (c) Kontroly K* (`diag_rules_checks_20260812.py`, + K32, K9f)

Předesláno: rozhodčí je po přepisu 13.08. formálně vzorný — povinná trojice
`ok/n/deg`, prázdná množina = N/A, jednotky jmenovatelů vytištěné u tabulky,
neznámý hráč je chyba, nulová počítadla se přiznávají. **Vady jmenovatele a
prázdné množiny, na které se ptá zadání, jsou tu z velké části OPRAVENÉ a
zdokumentované.** Co opravené není, je perspektiva: **celý rozhodčí soudí
výhradně NAŠE kola** (`analyse():370` — `if S["active_team"] != ours:
continue`) a ani jedna kontrola nemá soupeřův protějšek.

| kontrola | strana/osa | protějšek existuje? | záměr / díra | dopad |
|---|---|---|---|---|
| K29 / K29full / K29rule (klec čistá) | naše klec | ✗ „jak často MY marku­jeme JEHO rohy" | DÍRA | pravidlo R1 vynucujeme na sobě a nevíme, jestli ho soupeři vůči nám porušují beztrestně |
| K9a / K9c_solo/cage/run / K9x (rozvrh) | náš nosič | ✗ soupeřovo tempo jako reference | díra menší (podlaha je z podstaty „naše") | bez reference nelze říct, jestli podlaha je přísná nebo shovívavá |
| K30 / K30b (drahý dodge držený) | my držíme soupeře | ✗ „soupeř drží NÁS drahým dodgem" | DÍRA | polovina výměny: nevíme, kolik nás stojí odchody z jeho TZ |
| K31 (tělo bez úkolu) | naše těla | ✗ jeho idle těla | DÍRA (levná) | „0,x těla/kolo bez úkolu" nemá měřítko dobré/špatné |
| K33 (bloky/kolo) | naše bloky | ✗ jeho bloky | DÍRA | viz (b2) |
| K34 (REACH0), K35 (FB2) | co deska dává JEMU proti nám | ✗ co jeho deska dává NÁM (náš REACH0/FB2 na jeho nosiče na konci JEHO kola) | **DÍRA — největší v (c)** | obrana/pressing nemá ani kontrolu, ani metr; přitom jde o čistě pozorovací doplněk (Board/predictors umí obě strany, stačí je zavolat s prohozeným `us_side` nad koly soupeře) |
| K36 (LOCKED → postup) | naše zamčená těla | ✗ jeho zamčená těla (naše šance) | díra menší | — |
| K38 / K38b (nosič v kontaktu) | náš nosič | ✗ jejich nosič v kontaktu s NÁMI (naše sack-příležitost) | **DÍRA** | zrcadlo K38 je přesně chybějící metr obrany; jednorázově ho měřil jen P33 (`diag_blitz_on_carrier_20260818.py`) a do rozhodčího se nikdy nepřeneslo |
| K32 (blitz nerozeznatelný od bloku) | vada logu, ne strana | — | opraveno rekonstrukcí | rekonstrukce (nesousedící BLOCK = blitz) je použita jen v P33, rozhodčí ji nezná |

Dvě věcné poznámky nad rámec stran:

1. **Dvě definice téže podlahy.** Engine razítkuje `requiredPace =
   dist / max(1, turnsLeft − RESERVE)` (`cage_advance.cpp:96–103`), rozhodčí
   počítá `need = ceil(dist / turns_left)` bez rezervy
   (`diag_rules_checks:619`). Tentýž pojem („kolik dlužím za kolo"), dvě
   aritmetiky, různé hodnoty v týchž kolech. Přesně proti lekci
   `phase_floor` („⭐ JEDINÁ DEFINICE", `diag_rules_checks:120–133`), která
   se uvnitř Pythonu dodržela, ale mezi enginem a Pythonem ne.
2. **Sentinely v `vals` K9a míchají tři významy.** Normální kolo přispívá
   residuem `got − need`, kolo s TD přispívá `0.0`
   (`diag_rules_checks:387`), ztráta míče `−1.0` (`:530`). Průměr „⌀ ← metr"
   pak není residuum rozvrhu, ale směs residuí a sentinelů — týž vzor „metr
   mění význam podle stavu" jako requiredPace. Podíl ok/n to nekazí, jen
   ono ⌀.

## Druhá osa

U každé rodiny je otázka zadání: **je to táž třída (týž generativní
mechanismus jako v (a)), nebo jiná?**

### Počet vs. kvalita

Členové (mimo už doložený `corridorResistance`):

* `achievablePace` — penalta za odpor je `min(2, (počet+1)/2)`
  (`cage_advance.cpp:106`): **kvalitativně slepá i po přidání
  corridorStrength**; síla těl v koridoru na razítkované tempo nemá vliv.
* K33 / `blokůdo` — počet bloků, ne kostky. Kvalita NAŠICH bloků se nedá ani
  zpětně dopočítat z eventů (BLOCK event neveze počet kostek; `die1/die2`
  jen pro armor/injury) — jde ale spočítat z desky (funkce `dice()` v
  `diag_exposure_scan` umí obě strany).
* K31 — počet těl bez úkolu; idle Treeman a idle Runner váží stejně.
* MARKED / SURF / REACH / REACH0 — počty bez ST/skill vah (REACH0=3 skaveny
  není REACH0=3 černoorky).
* Protipříklady, tedy důkaz, že konvence kvality v diagnostice EXISTUJE:
  K30 měří dodge jako pravděpodobnost selhání včetně Tackle/Dodge interakce;
  FB2 měří kostky včetně Guard asistencí obou stran; „rohy → ČISTÉ rohy"
  (uživatelovo pravidlo 04.08.).

**Verdikt rodiny: táž třída.** Vzor je opakovaně: *metr se narodí jako počet;
kvalitativní verze se dodá až po prokázané škodě* (koridor: 1,78/1,93/1,89/1,89
vs. 4,3× skórování — čísla ze starého enginu, ilustrace; bloky ano/ne vs.
počet: obrácené znaménko +10,4σ/−2,5σ, dtto ilustrace). Nikdy proaktivně.

### Stav vs. změna

* Téměř vše, co razítkujeme i co čteme, je **snímek stavu**; změnu měří jen
  místa, kam ji vtlačil doložený omyl: K9a (`got` = Δx nosiče), K36 (LOCKED →
  postup příští kolo), `appr`/`down`/`lost`/`ball_lost` v exposure scanu.
* `corridorResistance/Strength` počítají **jen stojící** soupeře
  (`cage_advance.cpp:65,80`): ležící tělo v koridoru je neviditelné, ačkoli
  vstane hned příští kolo (vstávání stojí 3 MA — `rules_bb2016.txt:670` —
  a od 21.08. se loguje eventem STAND_UP). Odpor
  je tedy snímek, který systematicky podceňuje odpor příštího kola — přesně
  po faulu/bloku, tedy přesně po akcích, jejichž hodnotu chceme číst.
* `requiredPace` mění VÝZNAM podle stavu (dělitel useknutý na 1 v posledních
  2 kolech, `cage_advance.cpp:99` s RESERVE_TURNS=1): průměr přes kola drivu
  míchá „tempo" a „zbývající vzdálenost". Kolik dalších metrů to dělá:
  K9a `need` (poslední kolo = celá vzdálenost, záměr podlahy, ale ⌀ residuí
  to kazí), K9a `vals` (sentinely 0,0/−1,0 vs. residua — viz (c) pozn. 2),
  `BLZ` (None→„nedosáhne" vypadává z korelací). To je podtřída „metr má
  v části stavového prostoru jiný význam a konzument to neví".

**Verdikt rodiny: táž třída, slabší dopad.** Mechanismus týž (forma metru
kopíruje problém dne; dynamická/duální forma se dodá až po omylu — TD-skip
a loss-skip opravované 20.08. dvakrát v jednom dni jsou přesně tenhle vzor),
ale producent (TurnLog: snímky začátků kol + eventy) změnu dopočítat DOVOLÍ,
takže opravy jsou pozorovací.

### Dosah vs. vzdálenost

* Kde uživatelova oprava z 18.08. proběhla, je dosah správně: REACH/REACH0 =
  BFS s MA+2 přes volná pole (`diag_exposure_scan:111–133`), P33 blitz-dosah
  (Čebyšev horní odhad, přiznaný). Dvě různé definice „dosahu" (BFS vs.
  Čebyšev) ale žijí vedle sebe bez společné funkce — táž past jako dvě
  podlahy tempa.
* **`corridorResistance/Strength` jsou vzdálenostní (poziční), ne dosahové**
  — počítají, kdo v koridoru STOJÍ, ne kdo tam DOJDE: rychlý soupeř jedno
  pole vedle koridoru je neviditelný, pomalý na jeho konci se počítá plnou
  vahou. Obě veličiny vznikly PO lekci z 18.08. (K9b 18.08., strength
  21.08.) a přesto se narodily poziční ⇒ lekce se nepřenáší mezi metry.
* `achievablePace` — MA nosiče + 2 GFI − penalta: neptá se, jestli cesta
  vede přes TZ (dodge), tedy vzdálenostní tvar tam, kde rozhoduje
  průchodnost.
* K34/K35 jmenovatel: REACH0 dosahový ✓; FB2 jen sousedství (blok je
  z definice sousedský ✓ — tvar odpovídá pravidlu).

**Verdikt rodiny: táž třída, s dovětkem.** Oprava z 18.08. se ujala v místě
omylu a v přímých potomcích (exposure scan, P33), ale NEgeneralizovala se:
metry přidané později touž chybu zopakovaly. To je silnější tvrzení než
v (a) — nejde jen o „zrcadlo se nepřidá nikdy", ale „ani prodělaná lekce
nemigruje do sousedního metru".

## VERDIKT

**Mechanismus z (a) platí i mimo hodnotovou funkci — je to táž třída — ale
v každé rodině má jinou realizaci, a to rozdělení je důležitější než samotné
ANO:**

1. **Hodnotová funkce (část 1): slepota PRODUCENTA.** Zrcadlo v datech
   neexistuje (soupeřův Guard se ani nesbírá). Oprava = zásah do hodnocení.
2. **Diagnostika (b): slepota KONZUMENTA.** TurnLog je z konstrukce
   oboustranný (razítkuje se za každé týmové kolo, veze oba rostery,
   koridor/tempo z perspektivy aktivního týmu) — ale všechny čtecí skripty
   měří výhradně veličiny NAŠEHO týmu. 52 ze 140 `diag_*.py` se kotví na
   „dwarf = my"; σ-tabulka nemá jediný soupeřův prediktor. Novější skripty
   (20.08.) už čtou i soupeřova kola, ale pořád jen kvůli NAŠIM veličinám
   (naše obrana, náš nosič pod úderem). Oprava = pozorovací.
3. **Kontroly K* (c): asymetrie NORMY, zčásti záměr.** Rozhodčí vynucuje
   NAŠI doktrínu, takže „soudí jen nás" je u pravidel (K29, K38…) záměr.
   Dírou je chybějící REFERENCE: bez soupeřova čísla (jeho bloky/kolo, jeho
   idle těla, jeho kola v kontaktu) se nedá říct, jestli 12,3 % je dobré.
   A zrcadlo K34/K35 — co JEHO deska dává NÁM — není nikde, čímž rozhodčí
   přesně kopíruje největší díru hodnotové funkce ([40]/[63]: obrana).

**Nejsilnější jednotlivý nález: párové veličiny jsou rozpůlené NAPŘÍČ
systémy a žádný systém nedrží obě půlky.**
* výhodné bloky: hodnotová funkce má NAŠE ([65]), exposure scan má JEJICH
  (FB2) — každý systém vidí opačnou polovinu;
* surf: hodnotová funkce má „soupeři surfovatelní námi" ([64]), diagnostika
  má „naši surfovatelní jimi" (SURF) — dtto;
* tlak na nosiče: hodnotová funkce ([40], [63]) i diagnostika (REACH/BLZ)
  měří JEN tlak na NÁS; náš tlak na JEJICH nosiče neměří nikdo nikde
  (jediná výjimka: jednorázový behaviorální P33).
To vylučuje výklad „každý systém si prostě vybral svou konvenci" — výběr
strany je náhodný podle problému, který se zrovna řešil, přesně jak zněla
hypotéza z (a).

**Druhá osa: všechny tři rodiny jsou táž třída s týmž generativním
mechanismem** — metr se narodí ve tvaru, který potřeboval problém dne
(počet, snímek stavu, pozice, naše strana), a komplementární tvar se dodá
až po doloženém falešném závěru, nikdy proaktivně. Doložená časová řada:
rohy→čisté rohy (pravidlo 04.08., měřeno až 13.08.), vzdálenost→dosah
(18.08. po uživatelově opravě), ano/ne→počet (P1 18.08. po obráceném
znaménku), TD/ztráta jako N/A→započtené (20.08., dvakrát v jednom dni),
počet→síla koridoru (21.08. po 4,3×). Dovětek z rodiny dosahu: **ani
prodělaná lekce nemigruje do sousedního metru** (koridorové metry vznikly
po 18.08. a přesto poziční).

## POŘADÍ OPRAV podle dopadu

Legenda: **POZOROVACÍ** = smí se batchovat, nemění hru ani hodnocení;
**ZÁSAH DO HODNOCENÍ** = mění NUM_FEATURES=73 ⇒ znehodnotí váhy ⇒ drahé.
⛔ Žádná z oprav se NEDĚLÁ teď (čerstvý křížový korpus; změna enginu by ho
degradovala na „předchozí verzi") — pořadí je pro plánování.

1. **Zrcadlová četba korpusu** (POZOROVACÍ, čistě Python, žádný engine):
   spustit σ-tabulku a exposure-přediktory i nad soupeřovými koly
   (`predictors(Board(E, us_side=theirs))`, koridorová pole z jeho kol už
   v korpusu jsou). Zavírá největší diagnostickou díru (obrana/tlak na
   jejich nosiče, reference pro K čísla) za cenu jednoho skriptu.
2. **Sesterský metr „naše těla v koridoru" (P53)** (POZOROVACÍ, Python):
   geometrie koridoru je čistá funkce snímku hráčů, dá se dopočítat offline
   z TurnLogu — na uzavření P53 NENÍ potřeba engine. (Razítko v enginu je
   jen pohodlnost — batchovat k příští přestavbě.)
3. **σ-tabulka: doplnit corridorStrength a tempo** (POZOROVACÍ, Python):
   strength se razítkuje od 21.08. a nikdo ho nečte; required/achievable
   pace tamtéž — ⚠️ NEPRŮMĚROVAT přes kola (poslední 2 kola = jiná
   veličina), vykazovat po fázích/kolech nebo bez posledních dvou.
4. **Rozdělit sentinely K9a `vals`** (POZOROVACÍ, Python): tři počítadla
   (residuum normálních kol / TD / ztráta) místo jednoho ⌀ míchajícího
   −1,0 a 0,0 s residui.
5. **Jedna definice podlahy tempa** (dnes POZOROVACÍ jako dokumentace,
   sjednocení v enginu batchovat): requiredPace (s rezervou) vs. K9a need
   (bez rezervy) — dvě aritmetiky téhož pojmu; minimálně pojmenovat, že
   NEJSOU srovnatelné.
6. **Dosahová verze koridoru** (POZOROVACÍ, Python): „kdo do koridoru
   DOJDE příští kolo" (BFS jako REACH) vedle „kdo tam stojí"; zahrnout
   ležící soupeře v koridoru (vstanou za 3 MA, `rules_bb2016.txt:670`).
7. **Skilly do PlayerSnapshotu** (engine, ale POZOROVACÍ povaha — nový
   exportní sloupec, hodnocení nemění; batchovat): odstraní ručně opsané
   tabulky DODGE/TACKLE/guard(), odblokuje Jump Up klauzuli K38 dřív, než
   začne lhát, a FB2 přestane hádat Guard z názvu pozice.
8. **BLITZ v event logu** (engine, POZOROVACÍ; batchovat): flag na BLOCK
   eventu (append-only pole) místo rekonstrukce nesousedstvím; K32.
9. **Penalta achievablePace ze síly, ne počtu + strop fáze KLEC** (engine;
   dnes jen diagnostické razítko, tedy POZOROVACÍ povaha; kdyby se ale
   plánovač klece zapnul, stane se z toho ZÁSAH DO CHOVÁNÍ — opravit dřív,
   než se brána otevře).
10. **Zrcadlové rysy hodnotové funkce** ([40]/[62]/[63]/[52–54], viz část 1)
    (ZÁSAH DO HODNOCENÍ, mění NUM_FEATURES, znehodnotí váhy): největší
    herní dopad, nejdražší cena; pořadí uvnitř této skupiny určí měření M1
    níže, ať se drahý krok udělá jen jednou a pro rysy, které nesou signál.

## Co nejde rozhodnout čtením kódu (zadání na měření)

Formulováno jako zadání, ne závěry. Všechna měření jdou nad HOTOVÝMI daty
(jednovláknově, `nice -n 19`); ⚠️ `crosses_20260821_data` má vadu P57/TA1
(řetězené rerolly, nejvíc Dodge/Sure Feet týmy) — rozdíly mezi rasami
nepřipisovat doktríně; `corpus_baseline_20260819_data` je z jiného enginu —
jen ilustrace mechanismu.

* **M1 — Nese zrcadlo signál?** Spočítat zrcadlené prediktory (soupeřův
  koridor z jeho kol, náš FB2/REACH0 na jeho nosiče) a změřit jejich σ vůči
  `scored`/`ball_lost`. Rozhoduje pořadí uvnitř kroku 10 (které zrcadlo
  stojí za NUM_FEATURES zásah).
* **M2 — Přeceňuje achievablePace klec?** Porovnat razítkované
  achievablePace s realizovaným Δx nosiče po fázích (phase_floor);
  hypotéza z čtení: nadsazené právě ve fázi KLEC.
* **M3 — P53 kvantifikace:** dopočítat „naše těla v koridoru" offline a
  změřit, v kolika kolech po našem faulu/bloku klesl `corridorResistance`
  při současném růstu našich těl v koridoru (= kola, kdy metr lhal).
* **M4 — Velikost N/A koše koridoru:** podíl kol s corridorResistance=−1
  po rasách a fázích hry; říká, jak reprezentativní je σ-řádek
  `odpor_koridoru` (pravidlo domu o N/A).
* **M5 — Ležící v koridoru:** jak často stojí v koridoru ležící soupeř
  (dnes neviditelný) a jak často vstane do příštího kola; rozhodne, jestli
  dosahová/stavová verze odporu (krok 6) něco změní.
* **M6 — Reference pro K čísla:** soupeřovy hodnoty K31/K33/K38-ekvivalentů
  z jeho kol; teprve pak lze říct, která naše čísla jsou vůbec špatná.

---

*Audit dokončen 24.08.2026. Metoda: čtení kódu (engine nezměněn, žádné
běhy). Klíčové zdroje: `engine/include/bb/game_simulator.h`,
`engine/src/game_simulator.cpp`, `engine/src/cage_advance.cpp`,
`engine/python/bb_module.cpp`, `engine/include/bb/turn_plan_record.h`,
`engine/include/bb/game_event.h`, `diag_rules_checks_20260812.py`,
`diag_drive_predictors_20260813.py`, `diag_exposure_scan_20260812.py`,
`diag_blitz_on_carrier_20260818.py`, `diag_k9_phase_20260819.py`,
`diag_basing_vs_columns_20260820.py`, `diag_carrier_contact_20260820.py`,
`rules_bb2016.txt`.*

HOTOVO
