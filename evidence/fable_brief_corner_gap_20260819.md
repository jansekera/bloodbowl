# ZADÁNÍ PRO FABLE 19.08.2026 — CO BRÁNÍ ČTVRTÉMU ROHU

*(kandidát (4); kandidát (1) „kolik rohů je optimum" odpadl — uživatel 19.08.
odpověděl PRAVIDLEM: „optimum klece je čtyři rohy a žádní další sousedi
s ballcarrierem, vše čisté samozřejmě". Viz spec ČÁST 15.0b.)*

## Otázka

**V 52,4 % našich kol s míčem chybí roh klece a zároveň naše tělo stojí
ortogonálně u nosiče — tedy na poli, které pravidlo zakazuje, jedno pole
od prázdného rohu, a u nosiče už je. PROČ tam nestojí?**

Strop je **0,73 rohu na kolo** a nestojí ani jeden blitz. Je to největší
strop, jaký jsme letos změřili (P10a 0,23 · P8 0,056) — ale je to **strop,
ne plán**: neptá se na dostupnost těla ani na hierarchii povinností.
**Úkolem je ten strop rozebrat na to, co z něj je doopravdy k mání.**

## Co má vrátit — rozklad těch 52,4 % podle PŘÍČINY

Pro každé kolo, kde `chybí roh ∧ naše tělo stojí ortogonálně u nosiče`,
zařaď to tělo do právě jedné kategorie a vrať podíly **i absolutní počty**:

1. **už hrálo** — tělo v tom kole má vlastní událost (pohyb/blok/blitz)
   ⇒ rozpočet kola je pryč, roh nebyl k mání **v tomhle kole**;
2. **markuje soupeře** — sousedí se stojícím soupeřem ⇒ krok do rohu
   povinnost R3 zruší; ⚠️ rozliš, jestli je ten dodge **drahý** *(logika
   `dodge_cost` v `diag_rules_checks_20260812.py` — R3 platí jen tam, kde
   únik něco stojí)* nebo levný, protože levné markování je podle K30b
   povinnost bez ceny a nemá roh přebít;
3. **zamčené** — samo stojí v soupeřově tackle zóně ⇒ krok do rohu stojí dodge
   *(K36: zamčené tělo je měřitelná ztráta tempa)*;
4. **rohové pole není k mání** — obsazené, mimo hřiště, nebo za lajnou
   *(15.5: klec u postranní čáry ztrácí polovinu rohů)*;
5. ⭐ **nic tomu nebránilo** — stojící, nehrálo, nikoho nemarkuje, není
   v TZ, roh je prázdný a dostupný. **Tohle je čistá vada volby a jediná
   část stropu, která je zadarmo.**

Kategorie (5) vrať zvlášť **jako rohů na kolo i na zápas** — to je číslo,
podle kterého se rozhodne, jestli se to implementuje.

## Druhá otázka: patří pravidlo do σ-tabulky?

σ-tabulka 18.08. obsahuje jen **rozložené** kusy — počet rohů (−2,1σ),
počet čistých rohů (−0,2σ), počet špinavých (−6,8σ). **Konjunkce
(K29⭐⭐) v ní není**, a přitom právě ona je cíl. Spočítej **σ pro
K29⭐⭐ jako prediktor skórujícího drivu** stejnou metodikou jako P30
a řekni, kam v tabulce patří.

⚠️ **Nejde o ověřování pravidla** — pravidlo je zadané a platí. Jde o to,
že tabulka dnes obsahuje veličiny, o kterých víme, že jsou zavádějící,
a chybí jí ta správná.

## Třetí otázka: co ten krok stojí

Když tělo přejde z ortogonálního pole do rohu — **co se rozpadne?**
Zajímá nás hlavně, kolik z kategorie (5) zároveň drží něco jiného, co
v kategoriích 2–3 nezachytíme (screen, zálohu na příští kolo, cestu).
Stačí kvalifikovaný odhad s doklady, ne přesné číslo.

## Data a nástroje

* **Korpus:** `corpus_baseline_20260817_data/*.json.gz` — 3 000 her.
  Geometrie rohů je opravou exportu `HAND_OFF` **nedotčená**, takže tenhle
  korpus stačí. *(Čerstvý `corpus_baseline_20260819_data` doběhne ~12:20
  SELČ; ⚠️ je legitimní na něm výsledek zopakovat, ale NEČEKEJ na něj.)*
* **Reprodukce vstupních čísel:** `diag_cage_rule_20260819.py` — pusť ho
  a měl bys dostat pravidlo 2,7 %, (1) 9,7 %, (3) 24,3 %, strop 0,73 rohu/kolo.
  **Když ti to nesedí, hlas to jako první věc** — je to kontrola, že čteme
  týž korpus týmž způsobem.
* **Struktury:** helpers `players`, `adj`, `threatens`, `dodge_cost`,
  `STANDING` v `diag_rules_checks_20260812.py`. Kontrola `K29rule` tamtéž
  je referenční implementace pravidla.
* **Kontext:** spec `evidence/dwarf_turn_procedure_spec_20260811.md`
  ČÁST 15 (celá, hlavně 15.0, 15.0b, 15.4, 15.5), fronta
  `evidence/task_queue.md` (P34, K29⭐⭐).

## Pravidla běhu

* ⛔ **Nespouštěj engine ani žádné A/B.** Do ~12:20 SELČ jede sběr korpusu
  na 10 z 12 jader. Tohle je **čtení korpusu**, python analýzy pouštěj
  s `nice -n 19`.
* ⭐ **Jmenovatel ke každému číslu** a **kontrolní skupina, kde dává smysl**
  — dvě nejčastější vady našich měření (audit 13.08., špinavé rohy 18.08.).
* ⭐ **Prázdná množina není splnění.** Když kategorie vyjde nula, napiš to
  jako nulu s jmenovatelem, ne jako „neaplikuje se".
* **Výstup:** `evidence/fable_corner_gap_20260819.md`. Struktura: nález →
  čísla s jmenovateli → co z toho je k mání → co jsi NEZMĚŘIL a proč.
