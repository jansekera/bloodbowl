# L vs SLOUPCE — CO KORPUS OBSAHUJE A CO Z NĚJ JDE ROZHODNOUT (Fable, 20.08.2026)

*(zadání `fable_brief_basing_vs_columns_20260820.md`; spec ČÁST 17; skript
`diag_basing_vs_columns_20260820.py`, příklad `diag_bvc_find_example_20260820.py`)*

**Rámec se během práce změnil:** uživatel 20.08. rozhodl, že kontakt vs
sloupce **není volba, ale FÁZE** (spec 17.4: D1 zdržuj = sloupce bez
kontaktu → D2 zavři past = půlkruh L, přechod řídí převaha). A audit kódu
20.08. ukázal, že **engine nehraje ani jedno** — obrana je pět rolí po
jednotlivcích (cage tag · intercept · safety · marker · endzone guard ·
jednořadý screen s pevnými Y {3,5,7,9,11}), žádný koordinovaný tvar.
⇒ **Tohle měření tedy neporovnává dvě doktríny, ale popisuje VÝCHOZÍ STAV
před stavbou fázové obrany** — a říká, co z korpusu jde a nejde rozhodnout.

Korpusy: baseline 3 000 her (soupeři skaven · orc · human · wood-elf),
zrcadlo 750 her dwarf–dwarf (za „nás" domácí). Obranné kolo = snímek na
ZAČÁTKU soupeřova kola, kde jeho stojící nosič drží míč — tedy výsledek
našeho předchozího kola. Baseline 15 675 klasifikovaných kol, zrcadlo 4 344.

---

## (1) HLAVNÍ NÁLEZ: SLOUPCE V KORPUSU PRAKTICKY NEJSOU — otázka se z něj rozhodnout NEDÁ

| kategorie (baseline) | n | podíl z 15 675 |
|---|---:|---:|
| KONTAKT (≥2 naše těla v base) | 8 720 | **55,6 %** |
| 1 TĚLO v kontaktu (marker) | 4 561 | 29,1 % |
| ANI JEDNO | 1 709 | 10,9 % |
| ODSTUP-SLOUPEC (≥2 screeneři, hloubka 2) | **357** | **2,3 %** |
| ODSTUP-SCREEN jednořadý | 328 | 2,1 % |

Zrcadlo (4 344 kol): KONTAKT 51,6 % · 1 TĚLO 33,1 % · ANI JEDNO 9,9 % ·
SLOUPEC 3,1 % · SCREEN 2,3 %.

⛔ **Kategorie D1 (sloupce) je pod hranicí vyhladovělého metru (~2 %)** —
totéž, na čem 19.08. pohořela σ pravidla klece. Jejích 357 kol vzniklo
náhodou (engine tvar neumí), takže **srovnání „co je lepší" z korpusu
neplyne a čísla kategorie SLOUPEC níže jsou popis náhody, ne doktríny.**

⚠️ A **KONTAKT je sice hojný (55,6 %), ale taky NENÍ doktrína L**: histogram
těl v base 0/1/2/3/4/5+ = 2 394 / 4 561 / 4 731 / 2 727 / 963 / 299 —
většinou 1–2 těla, žádný půlkruh. Je to vedlejší produkt bitky, ne past.

## (2) METRIKA 17.6 POSTAVENA: „kolik má nosič levných únikových polí"

Definice: volná sousední pole nosiče (8-okolí, na hřišti, neobsazená kýmkoli
včetně ležících); LEVNÉ = P(selhání dodge) < 0,20 (logika `dodge_cost`:
7−AG−1 + naše TZ na cílovém poli; Dodge reroll ruší náš Tackle u nosiče).

* baseline: ⌀ volných polí **4,28**, z toho levných **3,04** (n=15 675);
  nosič **BEZ jediného levného pole ve 32,7 %** kol (5 131 z 15 675);
  zrcadlo: 40,0 % (1 738 z 4 344).
* **Prediktivita na výsledek soupeřova kola** (baseline):

| levných polí | ⌀ postup míče | ztráta míče | TD drivu |
|---|---|---|---|
| 0 | **+1,41** (n=4 749) | **3,4 %** (n=4 914) | 21,0 % (n=5 131) |
| 1 | +1,67 (n=698) | 2,6 % (n=717) | 31,9 % (n=737) |
| 2 | +2,32 (n=1 059) | 2,2 % (n=1 083) | 29,4 % (n=1 119) |
| 3+ | **+2,86** (n=8 304) | 1,8 % (n=8 454) | 26,0 % (n=8 688) |

* Starý metr rozhodčího („stojí u nosiče někdo náš"): postup +1,63 vs +2,79.
  ⇒ nová metrika má **širší gradient (1,45 pole vs 1,16)** a je spojitá;
  na úrovni KOLA funguje monotónně. ⚠️ Na úrovni DRIVU monotónní není
  (21,0 % při 0 vs 26,0 % při 3+, ale 31,9 % při 1) — zamčení nosiče na
  jedno kolo se do TD drivu nepřepisuje přímočaře; tohle je korelace přes
  stav partie, ne kauzální řetěz.

## (3) TEST OBOU TVRZENÍ (⚠️ vše korelace — kategorie vznikají náhodou, nedrží se doktrínou)

**(3a) Tvrzení L — kontakt zpomaluje: ANO (koreluje), ale nezabíjí.**
Kontakt≥2 vs zbytek, stratifikováno převahou stojících těl (konfundér
iniciativy), baseline, ⌀ postup míče:

| převaha (my − oni, stojící) | kontakt≤1 | kontakt≥2 | Δ |
|---|---|---|---|
| ZÁPORNÁ | +2,87 (n=4 384) | +2,25 (n=4 140) | −0,62 |
| NULA | +2,18 (n=797) | +2,05 (n=1 457) | −0,13 |
| KLADNÁ | +2,40 (n=1 370) | +1,56 (n=2 662) | **−0,84** |

Znaménko se **v žádném pásmu nepřeklápí** ⇒ hypotéza iniciativy jako
DĚLICÍ ČÁRY („bez převahy je kontakt škodlivý") tímhle metrem **nežije**.
Ale největší přínos kontaktu je při KLADNÉ převaze — což s fázovým modelem
D1→D2 („past zavírej s převahou") je konzistentní. Sliby o pádech se
nekonají: nosič padá na dodge při kontaktu jen v ⌀ 0,009 případu/kolo —
**past nezabíjí, jen zdržuje** (AG4 s Dodge má 2+ i proti Tackle).

**(3b) Protitvrzení webu — „kontakt dovolí vyrobit díru": v baseline se
NEUKAZUJE, v zrcadle ANO.** Kola s ≥1 kontaktním tělem:

* baseline: po soupeřově bloku na naše kontaktní tělo postup +2,13
  (n=9 657) vs +2,29 bez bloku (n=2 865); průnik ≥3 pole 36,4 vs 36,2 %.
  Ztráta míče po bloku 1,6 % vs **5,2 %** bez bloku — blok soupeři
  nahrazuje riskantní dodge, rychlé rasy díru nepotřebují.
* zrcadlo (soupeř dwarf): po bloku postup **+2,09 vs +1,55**, průnik
  **41,8 % (n=2 870) vs 32,7 % (n=655)** ⇒ proti pomalému bashery je
  probití jediná cesta — a funguje. ⚠️ Blok je soupeřova volba, ne náš
  experiment: i tohle je korelace.

**(3c) Rasa soupeře — U1 SEDÍ.** Gradient kontaktu na postupu
(kontakt≤1 → kontakt≥2) a TD drivu:

| soupeř | ⌀ postup ≤1 → ≥2 | Δ | TD drivu ≤1 → ≥2 |
|---|---|---|---|
| skaven (max MA 9) | +3,06 → +1,66 | **−1,40** | 42,8 % → 25,7 % (**−17,1 pp**) |
| wood-elf (MA 8–9) | +3,76 → +2,56 | −1,20 | 38,9 % → 35,2 % |
| human (MA 8) | +2,93 → +2,07 | −0,86 | 28,4 % → 22,2 % |
| orc (MA 5–6) | +2,06 → +1,62 | −0,44 | 12,8 % → **15,1 %** (obráceně) |

⇒ přesně předpověď U1: **kontakt/L je nejcennější proti rychlým a slabým**
(skaven, wood-elf), proti orkům skoro nic nedává a TD drivu je s kontaktem
dokonce mírně horší. (n v tabulce: human 1 593/2 560 · orc 3 039/1 868 ·
skaven 916/1 941 · wood-elf 1 167/2 084 pro ztrátu; postupová n v runu.)

## (4) DÍRA PEVNÝCH Y {3,5,7,9,11} — reálná, ale tenhle soupeř ji nezneužívá

Nosič na křídle (y 0–1 / 13–14): jen **429 kol = 2,7 %** z 15 675 (soupeřovy
enginy taky běhají středem). Když tam ale je: v jeho pruhu před ním stojí
někdo náš jen v **55,5 %** (vs 76,3 % ve středu) a jeho ⌀ postup je
**+3,56 vs +2,27**. ⇒ díra existuje a lidský soupeř s elfy by ji našel;
v korpusu je vzácná, tak z ní nic silnějšího netvrdit.

## (5) GEOMETRIE D1 Z WEBU (1 od soupeře · 2 do šířky · 2 do hloubky)

*(doplňuje se — měření běží, viz konec souboru)*

## (6) ODEHRANÁ SITUACE — g0248, 2. půle, 3. kolo, soupeř skaven

*(baseline `g0248.json.gz`, snímek idx 20; my = domácí, bráníme endzonu x=0)*

Gutter Runner +Sure Feet (AG4, Dodge) nese míč na (13,5). **Kontakt držíme
učebnicově:** dva Longbeardi s Tackle ho basují z (14,5) a (14,6); třetí
kontaktní tělo, Runner +Block, stojí na (11,4) u jeho doprovodu. Nosič má
podle metriky 17.6 jediné volné sousední pole. Naše převaha stojících těl
je kladná.

```
    0         1         2
    01234567890123456789012345
  2 ............DD............
  3 ......D...................
  4 ..........DDO.O...........
  5 ...........dO*D...........   * = nosič (13,5), D(14,5)+D(14,6) = Tackle
  6 .......D....oODO..........
  7 ...........dOOO...........
  8 ...........d..............
```

* **Co říká L:** správně — drž kontakt, každý jeho krok je dodge proti
  Tackle (bez rerollu).
* **Co říkají sloupce:** nebasuj — postav dvojice do hloubky v jeho pruhu
  směrem k endzoně; blitz první vrstvu přežije druhá.
* **Co se stalo:** skaven bloknul našeho Runnera na (11,4) — srazil ho na
  (10,3) a **dokročil do uvolněného pole**: díra otevřená přesně podle
  protitvrzení. Nosič hodil JEDEN dodge 2+ (i proti Tackle je to 83 %,
  reroll nepotřeboval), prošel na (12,5) a běžel **10 polí** — (11,6),
  (10,6), (9,6)… až (3,5), se dvěma GFI. Za naším kontaktním prstencem
  nestál v jeho dráze **nikdo**. O dvě kola později skaven **dal TD**
  (míč se u endzony ještě jednou vysypal, ale sebrali ho dřív než my).

**Poučení jde proti oběma doktrínám naráz:** kontakt proti AG4+Dodge
nezdrží ani s Tackle (2+), a škoda nevznikla z kontaktu samotného, ale
z **chybějící HLOUBKY za ním** — tedy přesně z toho, co dává D1. L bez
zadní stěny je nejhorší z obou světů. Fázový model uživatele (sloupce,
dokud past nejde zavřít CELÁ) tenhle případ pokrývá.

## (7) MEZE A OTEVŘENÉ OTÁZKY

* Všechno výše je **korelace na korpusu bez doktríny** — kategorie vznikají
  náhodou a kontakt koreluje se stavem desky; stratifikace převahou je
  jediná kontrola konfundéru, kterou tohle umí.
* **Spouštěč přechodu D1→D2 měření nerozhodne** a obecné zdroje ho neváží
  na převahu (dělí podle typu týmu; spouštěč mají ve stavu zápasu) — a
  jediný zdroj o načasování říká „vyber si kolo a jdi all-in", tedy
  **jednorázové překlopení, ne postupné svírání**. Otevřená otázka na
  uživatele (spec 17.4b).
* Po implementaci D1/D2 **přeměřit** — dnešní čísla kategorie SLOUPEC jsou
  z 357 náhodných kol a σ na vyhladovělém metru nic neuvidí (lekce 19.08.).

