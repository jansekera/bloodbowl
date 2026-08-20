# ZADÁNÍ PRO FABLE 20.08.2026 (2. dnes) — L NEBO SLOUPCE PO DVOU?

*(uživatel 20.08.; spec **ČÁST 17.4**, jediná otevřená položka nové obranné kapitoly)*

## Otázka

**Trpasličí obrana: obklíčit a DRŽET KONTAKT (L / boxing-in), nebo kontakt
NEDRŽET a stavět sloupce po dvou?**

Naše doktrína od 10.08. říká obklíčit. Rešerše 20.08. (grumbbl *„To Base,
or not to Base"*, Exit 23 *„Defense 101"*, bbtactics) říká:

> *„Je důležité **nezůstat v base kontaktu**, protože to soupeři obvykle
> dovolí vyrobit díru a začít se skrz ni tlačit."* Alternativa: **sloupce
> po dvou hráčích** napříč hřištěm — *„prvního blitznou, ale na druhého
> nemají rychlost ani obratnost."*

**Hypotéza dělicí čáry (uživatel/Claude, NEOVĚŘENO): INICIATIVA** — bez
početní převahy sloupce, s převahou obklíčení. Týž fázový model, jaký říká
trpasličí příručka. **Tvůj úkol není tu hypotézu potvrdit, ale otestovat.**

## ⛔ ZAČNI TÍMHLE, JINAK JE ZBYTEK BEZCENNÝ

**Napřed zjisti, jestli korpus obě chování vůbec obsahuje.**

19.08. jsme na tom pohořeli: σ pravidla klece vyšla **0,0**, protože korpus
to pravidlo hraje ve **2,2 %** kol — **vyhladovělý metr**, ne důkaz, že
pravidlo nefunguje. Když engine sloupce nikdy nehraje, **tahle otázka se
z korpusu rozhodnout NEDÁ** a tvůj závěr musí znít přesně to, ne slabě
podepřené číslo.

⇒ **Krok 1:** klasifikuj každé NAŠE obranné kolo do: **KONTAKT** *(naše
těla sousedí se soupeřovými)* · **ODSTUP/SLOUPEC** *(hlídáme, ale
nesousedíme; hloubka 2)* · **ani jedno**. Vrať podíly **i absolutní počty**.
Když je některá kategorie pod ~2 %, **řekni to jako hlavní nález a dál se
o ni neopírej.**

## Krok 2 — POSTAV CHYBĚJÍCÍ METRIKU (spec 17.6)

⛔ **Náš rozhodčí dodnes testuje obranu jako „stojí u nosiče někdo náš" →
70 %. To je úplně jiná veličina než „nemá kam".** Doktrína L tvrdí, že
odebírá **únikové pole** — a tu veličinu jsme nikdy nepočítali.

⇒ Postav ji: **počet volných polí, kam může soupeřův nosič odejít**,
a odděleně **kolik z nich stojí hod** (logika `dodge_cost`
v `diag_rules_checks_20260812.py` — u AG2 je odchod z TZ drahý, u AG4 levný).
Tohle je ta metrika, o které celá kapitola je.

## Krok 3 — otestuj OBĚ tvrzení, každé zvlášť

**(1) Tvrzení L:** kontakt odebírá únikové pole ⇒ soupeřův nosič postoupí
míň, častěji fumbluje dodge, drive častěji skončí bez TD.

**(2) Protitvrzení webu:** kontakt soupeři **dovolí vyrobit díru**. Tohle je
testovatelný mechanismus, ne názor: **po našem obklíčení — jak často
soupeřův blok/blitz otevře mezeru, kterou nosič projde?** Porovnej s koly,
kde jsme kontakt nedrželi.

⚠️ **Hlavní konfundér, se kterým se musíš vypořádat:** kontakt držíme spíš
tehdy, když už jsme pozičně/početně napřed — takže kontakt bude korelovat
s dobrými výsledky z důvodů, které s kontaktem nesouvisí. Buď to spárovat
(stejný počet stojících těl na obou stranách, stejná fáze půle), nebo to
**nahlas přiznat jako mez** a nevydávat korelaci za kauzalitu.

**(3) Hypotéza iniciativy:** rozděl výsledky podle **naší početní převahy
ve stojících tělech** (záporná / nula / kladná). Když se znaménko efektu
mezi pásmy překlopí, hypotéza žije; když ne, je mrtvá a řekni to.

## Krok 4 — rasa soupeře

U1 (19.08.) změřilo, že **lajna převádí DOSAH na POŠKOZENÍ** — násobek podle
max MA soupeře (6→×1,31 · 7→×1,15 · 8→×1,62 · 9→×1,89). ⇒ **L má být
nejcennější proti rychlým a slabým.** Rozděl podle rasy soupeře a řekni,
jestli to sedí. V korpusu jsou skaven · orc · human · wood-elf.

## Co odevzdat

`evidence/fable_basing_vs_columns_20260820.md` + shrnutí v závěrečné zprávě.

⭐ **A na konec JEDNU ODEHRANOU SITUACI, ne jen čísla** — konkrétní kolo
z korpusu, kde se ty dvě doktríny rozcházejí: postavení, co říká L, co
říkají sloupce, a čím to dopadlo. **Uživatel rozhoduje doktrínu, ne měření**
— tvoje práce je připravit mu tu volbu, ne ji udělat za něj.

## Co NEdělat

* ⛔ **neopravuj engine**, nic nekompiluj — diagnostické python skripty ano;
* ⛔ **nepiš čísla v kroužcích** (ⓐ ①) — piš (1) (2) (3);
* ⛔ **nevydávej korelaci za kauzalitu** a ke každému podílu tiskni jmenovatel;
* ⛔ nevymýšlej čísla — kde data nestačí, řekni to a napiš, co by to rozhodlo.

## Doklady

`corpus_baseline_20260819_data/*.json.gz` (3 000 her) ·
`corpus_mirror_20260819_data` (750 her dwarf–dwarf) ·
spec **ČÁST 17** (nová, 20.08.) · spec **ČÁST 15.5** (U1) ·
paměť `project_bloodbowl_dwarf_boxing_in_doctrine_20260810` ·
loader `import diag_rules_checks_20260812 as R`, vzor `diag_carrier_idle_20260820.py`.
