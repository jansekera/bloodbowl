# Rozklad selhání trpasličích přijímacích drivů (Fable, 11.08.2026)

**Data:** čerstvý korpus `diag_replay_mine_20260811_data/` (120 her, COLLECT_DONE,
v každé hře trpaslík, obě orientace, TV1200 / MCTS-100 / vf_blend=0.0).
**Nástroj:** `diag_drive_failure_20260811.py` (odladěn na korpusu z 30.07., čísla níže
jsou výhradně z čerstvého korpusu). Vazba na motivaci sedí: trpaslík dal 0 TD
v **77 %** her tohoto korpusu (orc 100 %, human 87 %, wood-elf 77 %, skaven 43 %).

Celkem 175 přijímacích drivů trpaslíka, 149 obranných, 1 drive vyřazen jako bug
enginu (viz §6).

---

## 1) Tabulka A/B/C/D1/D2

### Všechny přijímací drivy (n = 175)

| soupeř | A skórovali | B míč nezískán | C ztratili | D1 pozdní start | D2 pomalá klec | celkem |
|---|---|---|---|---|---|---|
| human | 3 (6 %) | 5 (10 %) | 17 (35 %) | 20 (42 %) | 3 (6 %) | 48 |
| orc | 0 (0 %) | 3 (7 %) | 12 (27 %) | 28 (64 %) | 1 (2 %) | 44 |
| skaven | 9 (23 %) | 2 (5 %) | 9 (23 %) | 15 (38 %) | 4 (10 %) | 39 |
| wood-elf | 4 (9 %) | 3 (7 %) | 16 (36 %) | 18 (41 %) | 3 (7 %) | 44 |
| **VŠE** | **16 (9 %)** | **13 (7 %)** | **54 (31 %)** | **81 (46 %)** | **11 (6 %)** | **175** |

### Jen plné drivy (≥ 7 našich kol — to jsou půle her 0:0, kvůli kterým měření vzniklo; n = 106)

| soupeř | A | B | C | D1 | D2 | celkem |
|---|---|---|---|---|---|---|
| human | 3 (12 %) | 0 | 11 (46 %) | 7 (29 %) | 3 (12 %) | 24 |
| orc | 0 (0 %) | 0 | 11 (38 %) | 17 (59 %) | 1 (3 %) | 29 |
| skaven | 7 (27 %) | 0 | 6 (23 %) | 9 (35 %) | 4 (15 %) | 26 |
| wood-elf | 4 (15 %) | 0 | 11 (41 %) | 9 (33 %) | 3 (11 %) | 27 |
| **VŠE** | **14 (13 %)** | **0 (0 %)** | **39 (37 %)** | **42 (40 %)** | **11 (10 %)** | **106** |

Doplňkové průměry (vše-soupeři, všechny přijímací drivy):

| | A | C | D1 | D2 |
|---|---|---|---|---|
| 1. držení (kolo) | 1,3 | 1,5 | 3,8¹ | 1,0 |
| vzdálenost při 1. držení (polí) | 18,8 | 20,2 | 21,7 | 16,6 |
| dosažené tempo (polí/kolo) | 2,57 | 2,16² | 2,08 | 1,50 |
| odpor v koridoru (stojící soupeři, nosič ± 2 řady) | 3,1 | 5,1 | 4,9 | 3,9 |
| nosič = Runner (podíl kol s míčem) | 89 % | 84 % | 79 % | 66 % |

¹ průměr zkreslují drivy startující pozdě v půli — viz §2. ² tempo do okamžiku ztráty.

Kategorie C: ztraceno průměrně v **kole 6,0**, **13 polí** od endzone; příčina ztráty
**43× z 54 (80 %) = soupeřův blitz/blok srazil nosiče**; dále 4× neúspěšný dodge
nosiče, 3× nechycená přihrávka/handoff, 3× sražení v našem tahu (both down),
1× GFI. Fumble na pickupu je zanedbatelný (0,05–0,07 na drive).

---

## 2) Verdikt k předpovědím: **ani jedna, tak jak byla zapsána** — ale mechanismus je blíž uživatelově

**Uživatel předpověděl D2 (pomalá klec).** Předregistrovaný test dává D2 jen
6 % (10 % v plných drivech) → podle litery testu NEPOTVRZENO.

**Fable předpověděl B + D1 (míč pozdě nebo vůbec).** Podle litery B+D1 = 53 % →
zdánlivě POTVRZENO. Jenže předpovězený *mechanismus* („míč získáváme pozdě
nebo vůbec") data vyvracejí:

* **B je v plných drivech NULA** (0 ze 106). Všech 13 B drivů jsou mikro-drivy
  o 1–2 kolech (výkop v kole 7–9, nebo okamžitý steal soupeře → jeho TD).
* **Ve všech 42 plných D1 drivech držíme míč už v kole 1–2** (38× kolo 1,
  4× kolo 2). Fumbly před prvním držením: 0,07/drive. Zpoždění prvního držení
  proti startu drivu je průměrně 0,2 kola. Dřívější číslo „míč poprvé v kole 4,1"
  bylo artefaktem míchání drivů, které začínají uprostřed půle.

**Proč tedy test hlásí D1 a ne D2?** Předregistrovaná hranice je aritmeticky
téměř nesplnitelná: výkop dopadá na medián **22 polí** od cílové endzone (74 %
výkopů > 20,9 — doktrinální pochod je tedy kratší než realita) a snímek prvního
držení nechává nosiči už jen ~7 kol → 7 × 2,61 = 18,3 < 22. **D1 se proto
rozsvítí, i když je míč v ruce v kole 1.** Test v této podobě neměří „pozdní
míč", měří „vzdálenost > doktrinální dosah".

**Co data skutečně říkají o tempu (a to je jádro):** dosažené tempo v plných
D1 drivech je **1,73 pole/kolo** proti potřebným ~3,1 (22,3 polí / 7 kol);
38 ze 42 plných D1 je pod doktrinálních 2,61 (překryv „pozdě i pomalu" přes
všechna D1: 51 z 81). D2 drivy mají tempo 1,50. A i kategorie C je tichým
svědkem pomalosti: nosič je sražen průměrně až v kole 6 a stále 13 polí od
endzone — tak dlouho je vystaven jen proto, že postup vázne (tempo do ztráty
2,14). **Mechanismus „pomalý pohyb klece" tedy data potvrzují — jen se
neprojeví v kolonce D2, protože předregistrovaná hranice ho schovala do D1.**

Shrnutí jednou větou: **míč máme brzy (kolo ~1), ale neseme ho ~1,7–2,2
pole/kolo proti potřebným ~3, takže půle končí v průměru 10,5 pole od endzone
(D), nebo je nosič po 6 kolech vystavení sražen (C).**

---

## 3) Tři nejčastější konkrétní situace v dominantní kategorii (D1, 81 drivů)

**Situace 1 — „klec zaparkuje za zamčenou linií" (typicky vs orc; nejpomalejší
třetina plných D1, tempo < 1,5).** Příklady `g0005:2`, `g0013:2` (oba vs orc, tempo 0,57 a 0,43): trpaslík (away,
útočí na x=0) přijímá, Runner sebere míč v kole 1 na x=23 a popojde na x=21.
Longbeardi se zakousnou do orčí linie na x=16–19, vznikne statický scrum
(v koridoru nosiče stojí **7 soupeřů**), a nosič **stojí 4–7 kol na stejném
poli** za zády vlastní linie. Za celou půli míč urazí 4 pole (23→19). Nikdo
neotvírá pruh, rohy klece nepostupují do kontaktu, nosič čeká na díru, která
nikdy nevznikne. Konec: 13 ze 42 plných D1 skončí s míčem **ještě na vlastní
půlce** (≥ 15 polí od endzone), 23 uvázne ve středu pole (6–14 polí).

**Situace 2 — „pochod dojde, čas ne" (těsné D1, konec ≤ 5 polí; 6 ze 42).**
Příklad `g0006:0` (vs human): pickup kolo 1, 20 polí před sebou, klec jde
středem ~2,4–2,7 pole/kolo skrz scrum na střední čáře. V kole 8 zoufalý
handoff a sprint — konec půle **3 pole od endzone**. Tady by stačilo o ~1 pole
za kolo víc v prvních třech kolech (nebo neztratit kolo přestavováním klece
na místě).

**Situace 3 — „matematicky mrtvý drive po inkasovaném TD" (39 z 81 D1 je
z drivů kratších než 7 kol; start průměrně v kole 6,4, k dispozici 2,6 kola).**
Příklad `g0011:2` (vs wood-elf): elf dá TD, my přijímáme v kole 6 druhé půle,
míč dopadne 22 polí od endzone. Runner sebere hned, ale 3 kola × MA6 na 22
polí nejde stihnout ani teoreticky (potřeba 7,3 pole/kolo). Tyhle drivy
nejsou selhání útoku — jsou to důsledky dřívějšího inkasování; 14 z 81 D1
navíc drží míč poprvé až v úplně posledním tahu půle. Pro hodnocení útočné
procedury je správné je oddělovat (proto druhá tabulka „jen plné drivy").

---

## 4) Co z toho plyne pro pořadí prací

Řazeno podle páky na dominantní vadu (pomalý postup s míčem v ruce + sražený nosič):

1. **[2] Tempo jako funkce odporu — NEJVĚTŠÍ dopad.** Přesně cílí na změřený
   deficit: potřebných ~3 pole/kolo při odporu ~5 soupeřů v koridoru, dosaženo
   1,7. Zabíjí Situaci 1 (parkování za linií) a nepřímo i C (nosič vystavený
   6 kol). Pozor: cílové tempo nesmí být kalibrované na 2,61 — reálný výkop
   (medián 22 polí, ~7 efektivních kol) žádá ~3,1.
2. **[1] Roh klece v TZ — druhý, a je to podmínka pro [2].** Situace 1 je
   přesně stav „39 % kol nula rohů klece": rohy odmítají vstoupit do TZ,
   linie se zamkne a tempo spadne na nulu. Bez ochoty rohů postupovat do
   kontaktu se tempo zvednout nedá. [1]+[2] jsou fakticky jeden balík.
3. **[3] Oprava rozestavení — třetí, menší a nepřímá páka.** Rozestavení
   nezmění, kam dopadne výkop (medián 22 polí, to je dané kickerem); může
   ušetřit ~1 kolo formování klece na startu a pomoci proti mikro-drivům B.
   Ale ani ideální rozestavení nevyřeší tempo 1,7 vs 3,1.

Kategorie C (31–37 %) navíc říká: až tempo vzroste, je nutné držet ochranu
nosiče (Runner nese 79–89 % kol) — 80 % ztrát je blitz na nosiče. To mluví
pro doktrínu „roh klece v TZ" spíš než pro rychlé běhy mimo klec.

---

## 5) Čeho jsem si všiml, na co ses neptal

* **Vs orc jsme za 30 her nedali ANI JEDEN TD** (0 % A ze 44 přijímacích
  drivů; D1 64 %). Zrcadlový pomalý soupeř nás zamyká nejlíp. Naopak vs
  skaven A = 23 % — proti měkké AV7 obraně procedura občas projde.
* **Polovina našich TD je z obrany.** 16 TD z příjmu vs 15 TD ze stealu
  v obranných drivech (STEAL+TD = 10 % obranných drivů; míč v obraně získáme
  ve 43 % drivů). Útok je tak slabý, že obrana nese půlku skóre.
* **Nosičská disciplína vypadá opravená:** Runner nese míč 79–89 % kol
  (10.08. bylo naměřeno „Longbeard nese 44 % kol"). Buď to spravila D-vlna 1,
  nebo se metriky liší — stojí za rychlé ověření, ale směr je dobrý.
* **Fumbly nejsou problém** (0,05–0,07/drive před prvním držením). Sure Hands
  Runner míč prostě sebere. „Míč poprvé v kole 4,1" z 10.08. je artefakt.
* **🐛 BUG: hra bez míče.** `g0040`, celá 2. půle (16 snímků): míč je na
  (−1,−1), nikdo ho nedrží, žádný BALL_BOUNCE ho nevrací — týmy se 8 kol jen
  mlátí. Zamýšlený příjemce byl soupeř. Četnost 1/252 drivů — stejná rodina
  jako ball-stuck z 23.07. (fix 27.07.), zřejmě nová varianta na cestě
  kickoff-out-of-bounds/touchback. Zaslouží trasovací metodu ze
  `project_bloodbowl_ball_stuck_fix_shipped_20260727`.
* **Pravidlová parita: existuje „kolo 9".** Po TD v kole 8 engine rozehraje
  výkop a dá přijímajícímu ještě tah T9 (v čerstvém korpusu 5 takových
  mikro-drivů, v korpusu z 30.07. tři hry). Podle CRP půle končí po 8 kolech
  obou týmů — tohle nafukuje kategorii B a dává kolo zdarma navíc.
* **Předregistrovaný práh 2,61 je špatně ukotvený k datům** (píšu to sem,
  neobcházel jsem ho — tabulky výše jsou spočítané přesně podle zadání):
  20,9 polí / 8 kol neodpovídá realitě „22 polí / ~7 kol od snímku prvního
  držení". Příště bych testoval D1 proti vzdálenosti z místa výkopu a počtu
  kol celého drivu, a „pomalost" proti potřebnému tempu konkrétního drivu,
  ne proti konstantě.

---

## 6) Metodika (pro reprodukci)

* **Dělení na drivy** (spolehlivější než reset čísla kola): hranice = první
  snímek hry, změna `half`, a snímek následující po tahu s `touchdown=True`
  (mezi nimi proběhl kickoff — snímek nového drivu je už po výkopu). Reset
  čísla kola hranice po TD uprostřed půle vůbec nevidí (číslo kola po TD
  pokračuje). Kontrola: součet TD podle vlajek == finální skóre ve všech hrách.
* **Přijímající tým:** držitel míče na startovním snímku; jinak strana, kde
  míč leží (HOME x ≤ 12); míč mimo hřiště → aktivní tým (přijímající hraje
  první — v celém korpusu bez jediné neshody).
* **První držení, zbývalo_kol, zbývalo_polí, tempo, odpor:** definice jsou
  v docstringu skriptu; všechny byly zafixované před spočítáním čerstvého
  korpusu, nic nebylo laděno zpětně.
* Konec posledního tahu půle není ve snímcích — držení a pozice po něm se
  dopočítávají z událostí toho tahu (PICKUP/CATCH/KNOCKED_DOWN/MOVE nosiče).
* ASCII prohlížeč situací: `python3 diag_drive_failure_20260811.py <dir> --dump gNNNN:K`.
