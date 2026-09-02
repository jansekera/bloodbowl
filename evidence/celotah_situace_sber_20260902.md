# SBĚR: HERNÍ SITUACE, KTERÉ UŽ PADLY — PODKLAD PRO CELOTAH
*(02.09.2026 · vysbíráno z `evidence/task_queue.md`, `evidence/dwarf_turn_procedure_spec_20260811.md`,
ostatních reportů v `evidence/` a z paměti)*

> **Zadání uživatele 02.09.:** *„projdi všechny dříve zmíněné situace a zkontroluj, že jsou
> připravené pro celotah — probírali jsme jich za měsíce hodně."*

⛔ **CO TENHLE SOUBOR JE:** inventura. Sbírá **jen to, co v projektu skutečně padlo**,
každou položku s doložením. **Nic není vymyšleno z pravidel Blood Bowlu.**
⛔ **CO NENÍ:** není to plán, není to pořadí a **není to úprava
`evidence/celotah_situace.md`** — ten se plní jinde. Duplicity s jeho `C1`-`C8` jsou
vypsané na konci, ne rozepsané.

## ⛔⛔ PROSTŘEDNÍ SLOUPEC A PROČ SMÍ BÝT „NIC"

> **Uživatel 02.09.:** *„celotah ještě model neumí — dej pozor, ať nepočítáš něco, co model neumí."*

U každé situace se ptám **na cenu její nepřítomnosti** *(důsledek, který nastává už dnes)*,
ne na to, jestli ji engine umí zahrát. Kde se dnes nedá změřit nic, je to **napsané
výslovně jako `⛔ NIC`** — a je to plnohodnotná odpověď, ne mezera.
[[feedback_measure_what_the_change_does]] · [[feedback_na_bucket_is_the_finding]]

**Legenda zdrojů:** `FRONTA ř. N` = `evidence/task_queue.md` · `SPEC ř. N` =
`evidence/dwarf_turn_procedure_spec_20260811.md` · jinak plná cesta.


## ⭐ UPŘESNĚNÍ Z `celotah_situace.md` (02.09.) — SITUACE JE **SEKVENCE**

> **Uživatel 02.09.:** *„šlo mi o příklady jako blitz na toho, kdo nám markuje
> nosiče, a pak nosičem pohyb a TD."*

⇒ Tvar, který celotah opravdu potřebuje, je **`akce A jednoho hráče → umožní
akci B jiného hráče → výsledek`** *(vzor `C9` tamtéž)*.

⛔ **Tenhle sběr je proto ŠIRŠÍ než to, co patří do `celotah_situace.md`.**
Vysbíral jsem **všechno, co za měsíce padlo jako herní situace**, protože
zadání znělo *„ať se nic neztratí"* — ale velká část z toho je **doktrína
o JEDNÉ akci** *(kam se smí postavit roh, kdy se nesmí skórovat)*, ne sekvence.
⭐ **Oddíl `N` na konci vypisuje TY, které tvar `A → B` mají** — to je ta
podmnožina, která se má přepsat do `celotah_situace.md`. Zbytek je **vstupní
materiál** *(podmínky a ceny, se kterými sekvence počítá)*, ne kandidát na `C`.

---

# A. ÚTOK S MÍČEM — ZAČÁTEK DRIVU A ROZESTAVENÍ

| # | situace — CO MÁ HRÁČ ROZEZNAT | ✅ jde měřit DNES *(cena nepřítomnosti)* | ⏰ až to bude umět | zdroj |
|---|---|---|---|---|
| **01** | **Klec není zahajovací formace.** První kola se **BĚŽÍ** s jedním doprovodem a screenem; klec stojí pět aktivací a v prvních kolech nechrání nic. **Staví se v kole, kdy se ohrožení OBJEVÍ** *(∃ soupeř, který dosáhne `MA+2+1` cestou bez našich TZ)*, ne až dorazí. | ✅ vzdálenost nosiče k EZ po kolech: **20,8 → 18,0 → … → 11,7** ⇒ ujdeme **1,5 pole/kolo** a potřebujeme **3,46 už od prvního** — do skluzu nespadáme během drivu, **vycházíme z něj** | podíl drivů, kde se na klec přepnulo přesně v kole vzniku ohrožení | SPEC ř. 639-651 (8.2) · FRONTA ř. 922 (P50) |
| **02** | **Přepínač fáze je VZTAH RYCHLOSTÍ, ne rasa** *(„klec staví brzo pomalý proti rychlému")*. Operačně: **dokáže nosič skončit kolo mimo dosah všech? ANO ⇒ běž a drž screen · NE ⇒ stav klec.** | ✅ zrcadlo metriky 17.6 *(levná úniková pole nosiče, gradient +1,41 → +2,86)* a rys `carrier_blitzable` **existují** ⇒ podíl kol, kdy nosič mimo dosah skončit umí, se spočítat dá | jestli se podle toho fáze skutečně přepíná | FRONTA ř. 1683-1705 (P48) |
| **03** | **Budoucí nosič stojí tak, aby v 1. kole dosáhl na míč** — a **záloha stojí vedle míče PŘED hodem** *(pro případ fumblu)*. | ✅ `T1.5′`: **záloha u míče 22,2 %**, nosič krytý po sběru 28,8 % — *„sbírat umíme, pojistit sběr ne"* | podíl rozestavení, kde Runner na očekávaný dopad nedosáhne | SPEC S0.4/S0.5 ř. 136-137 · FRONTA ř. 55 |
| **04** | **Záloha u míče musí být potenciální NOSIČ** *(Blitzer AG3 MA5)*, ne nejbližší tělo — ať když Runner nezvedne, donese míč on. | ✅ role těla, které stojí u míče jako druhé | zda se záloha vybírá podle role, ne podle vzdálenosti | SPEC 9.6 ř. 981 · FRONTA ř. 200 (T4.1) |
| **05** | **Rozestavení musí přežít `BLITZ` z tabulky výkopu** — žádný osamocený obklopitelný hráč v zadním poli. | ⛔ **NIC** — korpus jede na `simpleKickoff`, tabulka výkopu je **mrtvá cesta** *(F13)*; spočítat jde jen geometrie „osamocený hráč", což není důsledek | podíl výkopů, které rozestavení nepřežilo | SPEC S0.6/S1.5 ř. 138, 152 · FRONTA ř. 253 (F13) |
| **06** | **Na LOS jen AV9 s Block** — nedat soupeři víc než 4 bloky v jeho prvním kole *(3 LOS + blitz)*. A na lajně má stát tělo, které **není určeným rohem klece**. | ✅ kdo stojí na LOS a kolik bloků soupeř v 1. kole zahraje; `T3.5`: **na lajně stojí 4 hráči místo 3** ⇒ blok navíc zdarma | zda se sloty na LOS přidělují podle AV a role | SPEC S1.2 ř. 149 · FRONTA ř. 166 (T3.5), ř. 56 (T1.6) |
| **07** | **Volba po losu:** křehcí/rychlí volí **ÚTOK**, ostatní včetně trpaslíka **OBRANU** *(2-1 grind: KO mají jen jednu šanci se vrátit)*. | ✅ nasazeno 24.08. `a78470e7`; a doklad, proč to není kosmetika: los se dřív **neházel vůbec** ⇒ host vyhrával o **+4,9σ** v zrcadlech | zda se volba vyplácí po rasách | FRONTA ř. 247 (T5.5) |

# B. ÚTOK S MÍČEM — KLEC: TVAR A ROHY

| # | situace — CO MÁ HRÁČ ROZEZNAT | ✅ jde měřit DNES *(cena nepřítomnosti)* | ⏰ až to bude umět | zdroj |
|---|---|---|---|---|
| **08** | **Eskorta je PŘESNĚ čtyři těla, a to na DIAGONÁLÁCH.** Ortogonály kolem nosiče zůstávají prázdné — jinak *(a)* odsun nemá kam a **chain push prostrčí naše tělo do nosiče zadarmo**, *(b)* volná pole kolem nosiče zazdíme sami a nosič **nemá kam šlápnout**. | ✅ změřeno 26.08.: v tazích s úplně zavřenou cestou stojí **náš hráč přímo před nosičem ve 172 ze 172 = 100 %**, a **149 ze 149** zavřených cest zavírají naši *(soupeř ani jednou)* | zda se ortogonály drží volné záměrně | SPEC E4 ř. 1380-1405 · FRONTA ř. 268 (M11) |
| **09** | **Chybovat se smí jen NAHORU.** Pět těl u nosiče je **menší chyba** *(soupeř to trestá tempem)*; **chybějící byť jediný roh je průšvih** *(trestá to míčem — dírou si přivede asistenci a blitz se otočí)*. | ✅ rozpad kol podle počtu obsazených rohů z **pozic**. ⛔ Ale **ne z `plan`**: `filled_corners` je trvale **0**, protože `NOT_CONSULTED` ve 100 % kol — nula vydávaná za měření | zda se pole nosiče vybírá podle počtu rohů, které opravdu zavřeme | SPEC E4c ř. 1450-1460 · FRONTA ř. 243 (T5.34) |
| **10** | **Protilehlé rohy, ne sousední.** Jen protilehlá dvojice *(SZ+JV nebo SV+JZ)* pokryje **4 ze 4** ortogonál; sousední dvojice nechá volné právě to pole, ze kterého se blitzuje. **Minimum jsou dva protilehlé.** | ✅ z pozic: u dvoutělové klece — jsou protilehlé? | zda se dvojice vybírá geometricky | SPEC ř. 1407-1421, E4a ř. 1487 |
| **11** | **Nosič se smí pohnout jen na pole, jehož rohy DOKÁŽEME OBSADIT VOLNÝMI těly.** Ne „kam dojde" a ne „kde jsou rohy čisté" — **kolik jich reálně zavřeme**. Volné pole u nosiče je **slot pro soupeřovu asistenci**. | ✅ strop `P38`: pole, ze kterého vyjde plná čistá klec, existuje v **95,6 %** kol, plníme **2,7 %**; ve 25,7 % těch kol na něm nosič **už stojí** | zda se pole nosiče dopočítává ze zamýšlené klece | SPEC E4c ř. 1441-1448, 15.0c ř. 2025 · FRONTA ř. 140 (P38) |
| **12** | **Degradovaná klec: dvě těla patří NAD A POD nosiče** *(ortogonály kolmo na směr)*, ne na diagonály — obě varianty nechají soupeři 6 polí, ale nad/pod má **všech 6 ve svých TZ**, diagonály jen 3. Tvar je **pevný**; posouvá se **cíl nosiče** vpřed a do boku, dvojice ho následuje. | ✅ **je to 97 % kol** *(plnou klec plníme 2,7 %)* ⇒ rozmístění dvou těl se dá spočítat z každého kola. ⚠️ Kontrola musí odlišit **„vybrali jsme špatně"** od **„nebylo z čeho"** | zda se degradace volí, nebo jen nastane | FRONTA ř. 1821 (Q14) · SPEC 15.0b‴ ř. 1947 |
| **13** | **Roh klece = tělo, které pole udrží pod blokem a je jinde MÉNĚ CENNÉ** *(Guard → ST → AV)*. Nikdy nejrychlejší volné tělo a nikdy určený nosič — **Runner a Blitzer nejsou nábytek, jsou to příjemci**. | ✅ změřeno 17.08.: v rohu stojí **Runner +Block 11,5 %** kol a **Blitzer 12,6 %** ⇒ ~čtvrtina kol našich dvou AG3 pozic se stráví jako nábytek klece | zda se roh přiděluje podle vhodnosti | FRONTA ř. 181 (P16) |
| **14** | **Ty čtyři rohy mají mít GUARD.** Guard je jediná dovednost, která funguje **právě když jsme obklíčení** — bez něj se blok s označkovanými asistenty **otočí** *(ST6:4 = 2 kostky my → 2 kostky ONI)*. | ✅ podíl rohů obsazených Guard tělem *(máme ho na 6 z 11 ⇒ splnitelné)* | zda se přiděluje přednostně | SPEC E4b ř. 1468-1490 |
| **15** | **Nikdo z pětice klece nekončí kolo vedle STOJÍCÍHO soupeře.** Je to **jedno pravidlo o dvou místech** — nosič (`K38`) i roh (`K29`), táž tvrdost. Ležící se nepočítá *(platí 3 MA za postavení)*; **s Jump Up ano**. | ✅ nosič končí vedle **stojícího** soupeře ve **12,3 %** kol *(vedle jakéhokoli 38,9 %)*; a ⛔ **u nás platí těch 38,9 %**, protože ležící smí blokovat | podíl kol s nulou porušení | SPEC 15.0b″ ř. 1844 · FRONTA ř. 143 (P42), 147 (P45) |
| **16** | **Za každým naším tělem v kontaktu musí zůstat aspoň JEDNO ze tří polí odsunu VOLNÉ** — jinak soupeř vyrobí **chain push jedním blokem bez hodu navíc**, a vlastními těly mu vyrobíme páku přímo na nosiče. | ✅ z pozic na konci našeho kola: kolikrát má soupeřův blok na naše tělo všechna tři pole odsunu obsazená | zda se to hlídá při volbě polí | SPEC E3 ř. 1359-1372 |
| **17** | **Prázdný roh je HORŠÍ než špinavý.** Když pollutera nesrazíme, je lepší nechat u rohu naše tělo než pole vyklidit — soupeř do prázdného vejde zadarmo, přes naše tělo se musí probít. | ✅ **+7,6 pp v držení míče** *(80,1 vs 72,5 %)* na srovnatelných situacích, 3 000 her; drží ve všech koších podle zásoby volných těl *(+9,2 / +6,4 / +6,0 pp)* | — | FRONTA ř. 298 (P0.7) |
| **18** | **Míč patří Runnerovi**, a s pomalým tělem v ruce **fáze VÝBĚH prakticky neexistuje** *(MA4: potřebné 8,57 proti dosažitelnému 4,54)* ⇒ správná odpověď tam není „vyjdi", ale **„PŘEDEJ Runnerovi"**. Kritérium předání je **„nosič je špatný"**, ne „příjemce je lepší". | ✅ v **84,6 %** kol nese Runner *(doktrína drží)*; a **hand-off se nabízí ~14×/hru a zahraje 0,043×/zápas = 1 : 240** ⇒ vada je ve VOLBĚ | zda se předání volí podle kritéria | FRONTA ř. 121-124, 197 (P5) · SPEC 16.0 ř. 2348 |
