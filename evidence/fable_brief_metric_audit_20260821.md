# ZADÁNÍ PRO FABLE — AUDIT METRŮ: JSOU NAŠE MĚŘIDLA JEDNOSTRANNÁ?

## 0. Hypotéza, kterou máš rozhodnout

⭐⭐⭐ **Naše metry soustavně počítají jednu stranu a druhou nevidí.**

Za jediný den (21.08.) na to padly **tři nezávislé nálezy**:

| metr | počítá | NEPOČÍTÁ |
|---|---|---|
| `favorable_blocks` *(rys hodnotové funkce [65])* | **naše** hráče, kdo hodí 2+ kostkami | **soupeřovy** proti nám — zrcadlo NEEXISTUJE |
| `corridor_resistance` | **počet** stojících soupeřů v koridoru | jejich **sílu** *(skaven ST 2 = ork ST 4 s Guardem)* |
| `corridor_strength` *(přidáno dnes)* | **soupeře** v koridoru | **nás** v koridoru |

Tři za den vypadají na **TŘÍDU vad, ne na tři náhody**. Máš to potvrdit,
nebo vyvrátit — a hlavně **najít zbytek té třídy, pokud existuje**.

⚠️ **Falzifikace je plnohodnotný výsledek.** Když vyjde, že jsou to tři
izolované případy a zbytek měřidel je symetrický, řekni to.

## 1. Proč to není akademické

Každý z těch tří má doložený dopad:

* **`favorable_blocks`** — engine vidí PŘÍNOS vstání *(přibude stojící tělo)*
  ostře, ale CENU *(stal jsem se cílem na 2+ kostky pro ST 6 s Mighty Blow)*
  **nemá čím vidět** ⇒ systematické zkreslení ve prospěch vstávání. Dnes jsme
  vstávání opravili a **TD spadlo o 40 %** *(0,740 → 0,443, párově, −6,12σ)*.
* **`corridor_resistance`** — dává čtyřem soupeřům **1,78 / 1,93 / 1,89 / 1,89**,
  zatímco trpaslíkovo skórování se mezi nimi liší **4,3×** a je **monotónní**
  v jejich ⌀ ST i ⌀ AV. Metr má v σ-tabulce **−9,6σ** a stojí na něm výklad
  doktríny *„trpaslík zeď PROLOMÍ, elf ji OBĚHNE"*.
* **`corridor_strength`** — po faulu, kterým odstraníme zedníka, metr ukáže
  **zlepšení**, přestože jsme koridor **ucpali vlastními asistenty**
  ⇒ ohodnotí tah jako úspěch přesně tehdy, kdy je to prohra.

## 2. Co konkrétně projít

### (a) 73 rysů hodnotové funkce — `feature_extractor.cpp`
Pro **každý** rys si polož jednu otázku: **měří to symetricky obě strany?**
Kde ne, rozhodni, jestli je asymetrie **záměr** *(např. „my držíme míč" je
z podstaty jednostranné)*, nebo **díra**. Vytiskni tabulku
`rys | strana | zrcadlo existuje? | záměr/díra`.
⭐ Známé zrcadlené dvojice v tom souboru **existují** *(`my_standing` /
`opp_standing`, `scoring_threat` / `opp_scoring_threat`,
`cage_diagonal_quality` / `opp_cage_diagonal_quality`)* ⇒ **symetrie je
v tomhle kódu zavedená konvence, takže její porušení je signál, ne styl.**

### (b) Diagnostické metry v `TurnLog` a v `diag_*.py`
Totéž pro veličiny, které razítkujeme do korpusu a na kterých stojí σ-tabulka.
⚠️ Zvlášť si všímej **N/A a nul**: veličina, která je nula, protože se
nepočítá, se u nás opakovaně četla jako fakt *(`plan.*` je
`NOT_CONSULTED` ve **100 %** kol — všechna pole nula, celý rok)*.

### (c) Kontroly K*
Mají kontroly svůj protějšek pro soupeře? Kde měříme „jak často to děláme my"
bez „jak často to dělá on", nevíme, jestli je číslo dobré nebo špatné.

## 3. Druhá osa, na kterou se dívej

Jednostrannost **my vs. soupeř** je jen jedna. Prověř i tyhle, jestli tvoří
tutéž rodinu:
* **počet vs. kvalita** *(odpor koridoru počítal těla místo síly)*;
* **stav vs. změna** *(měříme, jak deska vypadá, nebo jak se hnula?)*;
* **dosah vs. vzdálenost** — ⭐ **uživatelova oprava z 18.08.**: u bloku na
  nosiče rozhoduje, kdo tam DOJDE, ne kdo tam stojí; první verze měřila
  stojící a vyrobila falešný závěr. **Kolik dalších metrů měří vzdálenost
  tam, kde má měřit dosah?**

## 4. Výstup

1. **Verdikt k hypotéze** — třída, nebo tři náhody. Čím.
2. **Tabulka všech nalezených asymetrií**, u každé **záměr / díra** a
   **doložený nebo odhadovaný dopad**.
3. **Pořadí oprav podle dopadu** — ⚠️ a u každé řekni, jestli je to
   **pozorovací** změna *(smí se batchovat)*, nebo **zásah do hodnocení**
   *(mění `NUM_FEATURES` = 73 ⇒ znehodnotí natrénované váhy ⇒ drahé)*.
4. **Co z toho nejde rozhodnout čtením kódu** a potřebuje měření — jmenuj
   jako zadání, ne jako závěr.

## 5. Tvrdá omezení

⛔ **NEPŘESTAVUJ engine, NESPOUŠTĚJ nic dlouhého, NEOPRAVUJ kód.**
⚠️⚠️ **PRÁVĚ BĚŽÍ VÍKENDOVÝ SBĚR** *(15 dvojic × 1 200 her, do neděle)* —
stroj je vytížený na 10 z 12 jader. Tohle je **čtení kódu**, ne měření.
Když se bez čísla neobejdeš, ber **starý** korpus `corpus_baseline_20260819_data`
a pouštěj to **jednovláknově s `nice -n 19`**.
⚠️ A pamatuj, že **starý korpus je z jiného enginu** *(oprava vstávání 21.08.)*
⇒ čísla z něj jsou ilustrace mechanismu, **ne platné hodnoty**.

Výstup ulož do `evidence/fable_metric_audit_20260821.md`.
