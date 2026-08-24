# ZADÁNÍ PRO FABLE — AUDIT METRŮ, ČÁST 2 (24.08.2026)

## 0. Proč tohle zadání existuje

21.08. běžel audit metrů podle `evidence/fable_brief_metric_audit_20260821.md`.
**Byl useknut session limitem po ~18 minutách**, ve zhruba třetině.
Jeho výstup **`evidence/fable_metric_audit_20260821.md` je PLATNÝ a je TVŮJ
VSTUP — přečti ho celý jako první a NEOPAKUJ ho.**

Hotové je: oddíl 0 *(strukturní fakt o jedné perspektivě)* a **celá úloha (a)**
*(všech 73 rysů `feature_extractor.cpp`, tabulka, bilance, generativní
mechanismus)*.

**Tvoje práce = ZBYTEK původního zadání**, nic víc:
* **(b)** diagnostické metry v `TurnLog` a v `diag_*.py` / σ-tabulka
* **(c)** kontroly `K*`
* **druhá osa** — počet/kvalita, stav/změna, dosah/vzdálenost
* **souhrnný verdikt + tabulka + pořadí oprav + co potřebuje měření**

⇒ Původní zadání `fable_brief_metric_audit_20260821.md` si přečti taky
*(oddíly 2b, 2c, 3, 4 jsou závazné doslova)*; tenhle soubor je jen jeho
oříznutí a aktualizace omezení.

## 1. Hypotéza, kterou máš dokončit

Část (a) už rozhodla, že jednostrannost **NENÍ tři náhody, ale TŘÍDA**, a našla
mechanismus: *v základním bloku rysů [0-55] je zrcadlení normou (16 ze 17 párů),
v „novém strategickém" bloku [56-69] je ze 14 rysů zrcadlený JEDINÝ.*
⇒ **Nový metr se přidá z perspektivy problému, který se zrovna řešil, a zrcadlo
se nepřidá nikdy.**

**Otázka pro tebe: platí týž mechanismus i mimo hodnotovou funkci** — v
diagnostice, v σ-tabulce a v kontrolách `K*`? Nebo je hodnotová funkce zvláštní
případ? **Obě odpovědi jsou nález**, ale musí být doložená.

## 2. Konkrétní záchytné body (nejsou vyčerpávající, jsou startovní)

* ⚠️ **N/A a nuly jsou první podezřelí.** Veličina, která je nula, protože se
  **nepočítá**, se u nás opakovaně četla jako fakt: `plan.*` bylo
  `NOT_CONSULTED` ve **100 %** kol **celý rok** a všechna pole byla nula.
  ⭐ Pravidlo domu: **„když je koš N/A větší než n, nález je v tom N/A."**
* **`corridorResistance` / `corridorStrength` (P53, NEOPRAVENO):** iterují
  **jen soupeře** ⇒ po faulu ukážou zlepšení, i když jsme koridor ucpali
  **vlastními** asistenty. Chybí sesterský metr „naše těla v koridoru".
  Tohle je známý kus třídy — hledej **zbytek**.
* **`required_pace` / `achievable_pace`** *(razítkuje se od 21.08.)*:
  ⚠️ **NEPRŮMĚROVAT** — v posledních dvou kolech se dělitel usekne na 1 a
  veličina se **změní ve zbývající vzdálenost** *(3,46 / 3,60 / 4,11 / 5,06 /
  7,09 / 13,20 / 11,72 podle zbývajících kol)*. Je to metr, který **mění
  význam podle stavu**; ptej se, **kolik dalších to dělá**.
* **dosah vs. vzdálenost** *(uživatelova oprava 18.08.)*: u bloku na nosiče
  rozhoduje, **kdo tam DOJDE**, ne kdo tam stojí — první verze měřila stojící
  a vyrobila **falešný závěr**. **Kolik dalších metrů měří vzdálenost tam, kde
  má měřit dosah?**
* **počet vs. kvalita**: odpor koridoru počítal **těla místo síly** — čtyřem
  soupeřům dával 1,78/1,93/1,89/1,89, přestože se skórování mezi nimi liší
  **4,3×**.
* **σ-tabulka**: ptej se nejen „je ta veličina jednostranná", ale i
  **„má jmenovatel, a je ten jmenovatel ten správný?"** *(13.08.: kontrola bez
  jmenovatele + prázdná množina = „splněno")*.

## 3. Výstup

Piš **PRŮBĚŽNĚ** do `evidence/fable_metric_audit_part2_20260824.md`
*(ne až na konci — předchozí pokus zemřel a zachránilo ho jen to, že psal
průběžně)*. Struktura:

1. **Pracovní deník** nahoře — odškrtávej `[x]`, ať je po useknutí vidět, kde jsi.
2. **(b)** tabulka `metr | strana/osa | protějšek existuje? | záměr / díra | dopad`.
3. **(c)** totéž pro kontroly `K*`.
4. **Druhá osa** — tři rodiny zvlášť, u každé „je to táž třída, nebo jiná?".
5. **VERDIKT** — platí mechanismus z (a) i mimo hodnotovou funkci?
6. **POŘADÍ OPRAV podle dopadu.** ⚠️ U každé řekni, jestli je to
   **POZOROVACÍ** změna *(smí se batchovat, nemění hru)* nebo **ZÁSAH DO
   HODNOCENÍ** *(mění `NUM_FEATURES` = 73 ⇒ znehodnotí natrénované váhy ⇒
   drahé)*. Tohle rozdělení je pro nás to nejcennější z celého auditu.
7. **Co nejde rozhodnout čtením kódu** — jmenuj jako **zadání na měření**,
   ne jako závěr.

**Poslední řádek souboru, až budeš hotov, musí být `HOTOVO`.** Když ho tam
nenajdeme, čteme dokument jako useknutý — a to, co v něm je, bereme jako platné.

## 4. Tvrdá omezení

⛔ **NEPŘESTAVUJ engine. NEOPRAVUJ kód. Neměň nic v `engine/`.**
Je to **audit čtením**. Zrovna teď je na disku **čerstvý křížový korpus, který
ještě nikdo nepřečetl**, a jakýkoli zásah do enginu z něj udělá „předchozí
verzi". Tohle je tvrdé.

⛔ **Nespouštěj dlouhé běhy.** Když se bez čísla neobejdeš, čti **hotová data**
*(`crosses_20260821_data/`, `corpus_baseline_20260819_data/`)* jednovláknově
a s `nice -n 19`.

⚠️ **Čísla z `corpus_baseline_20260819_data` jsou z JINÉHO ENGINU**
*(21.08. dvě opravy: vstávání P45 + sedm pravidlových)* ⇒ **ilustrace
mechanismu, ne platné hodnoty.** Totéž řekni u každého čísla, které použiješ.

⚠️ **Křížový korpus `crosses_20260821_data` má ZNÁMOU VADU (P57/TA1):**
`attemptRoll` řetězí až **tři** rerolly jednoho hodu ⇒ **nadhodnocené rerolly**,
nejvíc u týmů s Dodge a Sure Feet. **Nepřipisuj rozdíly mezi rasami doktríně.**

## 5. Formální

* ⛔ **Žádná čísla v kroužcích** (①②③). Piš `(1) (2) (3)`.
* Názvy dovedností **anglicky** (Guard, Stand Firm, Sure Hands…).
* Text česky.
* **Když si nejsi jistý pravidlem, ověř ho v `rules_bb2016.txt`** — hrajeme
  **BB2016**, ne CRP/LRB6 — a **cituj číslo řádku**. Nikdy necituj pravidlo
  z hlavy: 21.08. přinesli agenti dvakrát správný nález se špatným zdůvodněním.
