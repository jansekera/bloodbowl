# ZADÁNÍ pro Fable: OFFLINE FEATUROVÝ A/B (P0-b), 10.08.2026

Navazuje na **tvůj vlastní report** `fable_learning_mechanism_report_20260811.md`
— jsi jeho autor, tohle je jeho §3 „doporučený PRVNÍ krok", ale
**s jednou zásadní opravou brány** (viz §2 níže).

## 1. Cíl

Offline ověřit, jestli **cílené přírůstky akčních featur** odstraní dvě
změřené slepoty policy. **Bez zásahu do enginu** — celé v Pythonu nad už
nasbíranými rozhodnutími. Implementace do C++ až po GO uživatele.

## 2. ⛔ BRÁNA NENÍ top-1. TOHLE JE HLAVNÍ ZMĚNA PROTI TVÉMU NÁVRHU.

Tvůj report navrhoval „GO práh: top1 +5 pp offline". **To se ruší**, ze dvou
důvodů:

1. **top-1 neměří sílu hry.** Policy dnes má 42 % top-1 a přitom trpaslíky
   prokazatelně kazí (−17 pp, race_guard N=600). „Častěji se trefit do
   stejného tahu jako search" ≠ „hrát líp".
2. **Těch „+5 pp" bylo přeneseno z jiné veličiny.** Pochází z power analýzy
   30.07., která se týkala **winrate**. Vazba mezi top-1 a winrate nebyla
   nikdy doložena.

**Místo toho platí metodika uživatele (10.08.):**
> „Když řešíme nějakou situaci, přidat k tomu **specifický log** a **na něm
> zkoumat posun v konkrétní oblasti**. Logů ať není moc, starší se ručně
> smaže."

⇒ **Bránou je CÍLENÝ LOG tří rozbitých míst, která jsi sám našel.**
Agregát (top-1, CE) smí zůstat jako **levný předfiltr BEZ práva veta** —
když se nepohne ani on, nemá cenu pokračovat; ale schválit sám nesmí.
**Prahy předregistruj i pro ten cílený log**, než se cokoli měří.

## 3. Tři rozbitá místa = obsah cíleného logu

Čísla jsou z tvého reportu §1.4; ber je jako výchozí stav, proti kterému se
měří posun.

**(A) „Nikdy nekonči tah" — akční bias.**
Ve všech **9** trpasličích rozhodnutích, kde search jednoznačně (≥50 %
visitů) volí `END_TURN`, ho policy **nikdy** nemá jako top-1
(medián: 4. z 5 kandidátů). Chování: `END_TURN` 10,4 % → 5,5 %,
rozhodnutí/hru 78,6 → 85,9, průměrná riskovost zvoleného makra +4,9 % rel.
*Metrika:* pořadí `END_TURN` v těch situacích + podíl, kde je top-1.

**(B) REPOSITION slepota — největší třída rozhodnutí.**
V **946** trpasličích rozhodnutích s ≥3 REPOS kandidáty je spread priorů
policy **0,000** (search visity **0,109**). Na ~40-50 % rozhodnutí policy
nenese žádnou informaci a blend 0,2 tam **ředí** funkční heuristické priory
(`macro_mcts.cpp:507-534`) k uniformě.
*Metrika:* spread priorů mezi REPOS kandidáty; cíl = posun od 0,000 k 0,109.

**(C) PICKUP bez ohledu na AG.**
Prior masa na PICKUP vs search: dwarf **+2,7 pp**, wood-elf **−10,3 pp**,
skaven **−3,2 pp**. Jedno číslo pro všechny ⇒ moc pro AG2-3, málo pro AG4.
*Metrika:* odchylka od searche, a jestli začne záviset na AG aktéra.

## 4. Featury k dopočítání (tvůj §189-197)

* **identita cíle u REPOSITION** — featury [15-22] v `action_features.cpp`
  už existují, jde je přenést;
* **skutečné p(fail) místo konstanty** — z AG aktéra, tackle zón na trase
  i cíli, počtu kostek; engine to všechno umí spočítat;
* **AG a MA aktéra + MA-normalizovaná vzdálenost k cíli.**

**⛔ ZÁVAZNÝ CONSTRAINT UŽIVATELE: žádné rasové labely ani per-race hlavy.**
(„rasově oddělená je pro gobliny") Vše se musí odvodit ze **schopností
a situace**, aby klec/grind vyšly emergentně pro každý pomalý silný tým.
Tohle **není** čtvrtý per-player pokus (3× NO-GO) — jde o akční featury
policy, ne per-player vstupy do value.

## 5. Provozní pravidla — ČTI POZORNĚ

* **VŠECHNO DLOUHÉ SPOUŠTĚJ ODPOJENĚ** (`setsid nohup … & disown`), s logem
  do souboru. Pokyn uživatele: *„ať vše spustí odpojeně — kdyby to nestihl,
  tak si logy přečteme zítra."* Nesmí to spadnout s koncem tvé session.
* **CPU JE OBSAZENÉ.** Do ~17:00 UTC běží 6 procesů srovnání ér
  (`era_measure_20260810/`), pak na ně naváže M1 (`m1_measure_20260810/`)
  přes noc. **Nezabíjej je, nečekej na ně, ale nesoutěž s nimi** — trénink
  malé policy hlavy nad ~5 866 rozhodnutími je minutová záležitost, to je
  v pořádku. Cokoli většího pusť s `nice -19`.
* **Před spuštěním čehokoli náročného** ověř `ps aux | grep -E
  "run_iteration|diag_era_pre|diag_f1_cage_advance|diag_m1"`.
* **NEMĚŇ produkční kód.** Jsi analytik. Skripty do `diag_*` nebo
  `scratchpad/`, nic nepromovat.
* **Každý launcher, který napíšeš, musí být IDEMPOTENTNÍ** — kontrola
  markeru „hotovo" i vlastní běžící instance. Dnes se ukázalo, že páteční
  launcher tuhle pojistku neměl a pustil běh dvakrát (~2 h CPU nazmar).
* **Nevymýšlej si čísla.** Každé tvrzení dolož souborem/řádkem nebo daty,
  jinak ho označ jako hypotézu.
* Anglické názvy skillů a herních pojmů, **nepřekládat** do češtiny.

## 6. Deliverable

`evidence/fable_offline_feature_ab_report_20260810.md`:
1. **předregistrované prahy** pro cílený log (A/B/C) — zapsat DŘÍV než
   výsledky, ať je vidět, že nejsou vybrané po měření;
2. výsledek staré vs nové sady featur na těch třech metrikách;
3. agregát (top-1, CE) jen jako doplněk, výslovně bez práva veta;
4. **doporučení GO/NO-GO** pro implementaci do enginu + odhad práce;
5. co se nepodařilo a proč.

Rozpočet: tokenů je dost (dnes se z Fable limitu vyčerpala jen malá část),
ale **hlásit, když se blíží strop**. Kdyby to nestihlo, **logy si přečteme
zítra** — proto ty odpojené běhy.
