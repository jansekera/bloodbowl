# Návrh: cílené měření first-possession hypotézy (2026-07-14, Fable 5)

Backlog item 3. Doprovodný skript: `diag_first_possession.py` (hotový, read-only
napsaný, syntax + offline logika ověřeny; **zatím nespuštěn** — čeká na dnešní
baseline-reset po H2 kickoff fixu).

---

## 1. Hypotéza a co už víme

**Hypotéza (uživatel, 2026-07-13, potvrzeno čísly):** výhoda prvního držení míče
(HOME přijímá první — engine nemá los, `game_simulator.cpp:409/:521` natvrdo
`kickingTeam = AWAY`) je latentní a vynořuje se úměrně tomu, jak ofenzivní fixy
lámou remízovou plošinu. Dokud hry končily 0:0, neměla se jak projevit.

Evidence ze stall-guard paired A/B (N=400 párů, `diag_stall_guard_ab_20260713.log`):

| metrika | baseline | candidate | verdikt |
|---|---|---|---|
| TD/hru | — | +0.007 | INCONCLUSIVE |
| draws | 48.2 % | 45.8 % (−2.5pp) | INCONCLUSIVE |
| **home_win** | 25.5 %W / 26.3 %L (edge −0.8pp) | 32.5 %W / 21.8 %L (edge **+10.7pp**) | **+7.0pp [+1.3,+12.7], p=0.020 CONFIRMED** |

V mirror zápase s identickými vahami obou stran je +7pp home_win čistý
strukturní signál, ne signál o vahách.

**Dnešní H2 fix mění strukturu držby:** dosud se H2 výkop obracel proti
poslednímu drivu H1 (`:429/:556`), tj. tým, který dal poslední gól H1, dostal
v H2 míč ZNOVU; ve hře bez gólů v H1 přijímal H2 away (náhodně „správně").
Po fixu platí: H1 přijímá home, H2 away — každá strana dostane právě jeden
opening drive a zbytková asymetrie je už jen **pořadí** (kdo přijímá dřív),
ne **počet** openingů. Všechna měření níže se proto plánují **post-fix** a
skript je navržen tak, aby rozvrh výkopů neověřoval předpokladem, ale **měřil**
(sekce 3, schedule audit) — funguje pre-fix, post-fix i pod případným losem.

## 2. Proč ne další draw-rate běh

Draw-rate slévá dvě různé věci: „ofenziva je lepší" a „first-possession bias je
silnější". Oba efekty snižují remízy a zvyšují home_win; z W/D/L je nelze
oddělit. Navíc šumové dno (feedback_draw_rate_noise_floor): delta <10pp
z jednoho N=150 = INCONCLUSIVE. Potřebujeme metriky definované **per drive**,
kde role (přijímající vs. kopající) je explicitní proměnná.

## 3. Navržené metriky — přesné definice

Vše se čte z existujícího `get_turn_logs()` (`bb_module.cpp:197`): jeden
snapshot na začátek tahu s poli `half, turn, active_team, home_score,
away_score, touchdown, turnover, ...`. **Žádná změna enginu není potřeba.**

**Segmentace drivů** (implementováno v `summarize_drives()`):

- Drive začíná: snapshotem 0; snapshotem následujícím po snapshotu
  s `touchdown=true`; snapshotem, kde se změnilo `half`.
- **Receiver drivu = `active_team` prvního snapshotu drivu.** Opora v kódu:
  `simpleKickoff()` (`game_simulator.cpp:329,341`) nastaví
  `activeTeam = receiving` a inkrementuje jeho `turnNumber`, takže první
  snapshot po výkopu je vždy tah přijímajícího. Nezávisí na tom, kdo kope —
  robustní vůči H2 fixu i případnému losu.
- **Skórující drivu** = strana, jejíž skóre vzroste mezi prvním snapshotem
  drivu a prvním snapshotem drivu následujícího (u posledního drivu vs.
  finální `result`). Snapshoty nesou skóre ze začátku tahu, takže TD daný
  během drivu je poprvé vidět až v následujícím snapshotu.
- Konec drivu: `td_recv` (skóroval receiver), `td_kick` (skóroval kopající —
  counter-TD po turnoveru), `half`, `game`.
- Konzistenční čítač anomálií: skok skóre >1 TD / záporný / nesoulad TD flagu
  s deltou skóre. Očekávání: 0; nenulová hodnota = nález sám o sobě.

**Metriky:**

1. **First-drive conversion** = P(drive 1 končí `td_recv`). Čistá míra
   „přetavení prvního držení v TD"; primární cross-build metrika.
2. **Receiver conversion by side** = P(`td_recv` | receiver=home) vs. totéž
   pro away, pooled přes obě orientace (viz side-swap níže). Klíčový
   separátor: identické váhy ⇒ rozdíl mezi stranami nemá existovat; pokud
   existuje, jde o **stranovou asymetrii enginu**, ne o strukturu držby.
3. **TD dekompozice 2×2 per game**: průměrné TD v buňkách
   {home, away} × {jako receiver, jako kicker} + počty přijatých drivů na
   stranu. Rozkládá home edge na: (a) asymetrii počtu držení,
   (b) asymetrii konverze.
4. **TD timing per drive**: počet tahů přijímajícího do TD (`recv_turns` při
   TD), zvlášť H1/H2. Sleduje, zda fixy zrychlují drive, nebo jen přidávají
   držení.
5. **Slot advantage** = (W−L)/decisive pooled přes obě orientace, exaktní
   binomický test vs. 0,5. Konfirmační test „home SLOT vyhrává".
6. **Race-vs-slot check**: McNemar fwd vs. swp na home_win (párováno seedem).
   CONFIRMED delta ⇒ na výsledku se podílí přiřazení rasy do slotu, ne jen
   slot sám.
7. **Schedule audit** (vedlejší produkt, zdarma): empirické rozdělení
   receiverů drive 1 / H2 openingu / po-TD drivů. Post-fix očekávání:
   drive 1 = home 100 %, H2 opening = away 100 %, po TD přijímá inkasující
   100 %. Jakákoli odchylka = **end-to-end regresní nález k dnešnímu H2
   fixu** na N=600 hrách — přesně ta e2e cesta, kterou paměť u této třídy
   bugů vyžaduje.

**Jak metriky oddělí „lepší ofenzivu" od „silnějšího biasu"** (čtecí klíč,
vytištěný i skriptem v compare módu): ofenzivní fix má zvednout first-drive
conversion a `receiver TDs/game` **symetricky v obou orientacích a pro obě
strany**; fix, který hýbe hlavně home_win / home margin, zatímco konverze
receiverů stojí, bias jen zesiluje.

## 4. Návrh experimentu

**Fáze A — post-fix charakterizace (jednorázově, po dnešním baseline-resetu):**

- `python3 diag_first_possession.py run postfix_20260714 300`
- N=300 seedů (`base_seed=20260714`, `diag_utils.paired_seeds`) × 2 orientace
  (fwd = přesně produkční `_gate_game` matchup races[i]/races[i+1]; swp =
  prohozené rostery) = 600 her, mirror `weights_best.json`, MCTS=100,
  vf_blend=0, TV=1000, eval gate config. Cena ≈ 1,5× jedno rameno
  stall-guard N=400, tj. běžný odpolední/noční detached běh.
- Ramena se persistují (`arm_first_possession_postfix_20260714_{fwd,swp}.json`),
  takže report lze kdykoli přepočítat a ramena poslouží jako baseline pro
  budoucí compare.

**Rozhodovací otázky a prahy (Fáze A):**

- **Q1 (existuje slot advantage i post-fix?):** exaktní binomický test W vs. L
  pooled; p<0,05 ⇒ CONFIRMED. Power: při ~54 % decisive (stall-guard candidate
  rameno) je z 600 her ~325 decisive, SE podílu ~2,8pp; pozorovaná třída
  efektu (podíl home 60 % decisive) dává z≈3,6 — pohodlně detekovatelné;
  spolehlivě zachytíme edge už od ~±11pp z decisive (~6pp per game).
- **Q2 (mechanismus):** rozdíl konverze home−away receiver drivů, 95% CI
  (drives jako jednotky, ±~4–5pp při ~1200 drivech).
  - CI vylučuje 0 ⇒ **stranová asymetrie enginu** — nová bug stopa (featury /
    pathfinding / MCTS perspektiva), řešit jako bugfix, ne losem.
  - CI obsahuje 0 a zároveň Q1 CONFIRMED ⇒ bias je **čistá struktura držby**
    (pořadí/počet possession) ⇒ řešení je plánovací (side-swap v měřeních,
    viz sekce 5), ne engine.
  - Q1 INCONCLUSIVE ⇒ H2 fix bias z velké části srovnal; hlídat dál
    v každém compare (home_win zůstává standardním výstupem).
- **Q3 (H2 fix e2e):** schedule audit musí dát post-fix očekávání 100 %.
  Odchylka = regresní nález s prioritou.

**Fáze B — cross-build A/B pro každý budoucí ofenzivní fix:**

- Stejný vzor jako `run_stall_guard_ab.sh`: `run baseline N` na staré binárce,
  rebuild, `run candidate N`, `compare baseline candidate`. N=300 párů/rameno.
- Primární verdikt: **first-drive conversion** (McNemar, CI mimo 0 =
  CONFIRMED; při ~20 % diskordanci detekuje ~±5pp) + párové delty TD
  dekompozice (`receiver TDs/game` vs. `kicker TDs/game`). home_win a draws
  jen doprovodně (draws dle šumového dna).

## 5. Los vs. side-swap — doporučení

**Doporučení: H2 fix + side-swap. Seed-based los teď NEZAVÁDĚT.**

Proč side-swap stačí a je lepší:

1. **Nulová cena:** čistě measurement-side, žádný C++ zásah, žádný rebuild,
   žádný další baseline-reset (dnes už jeden probíhá — H2 fix + screen fixy
   8+9; nepřidávat třetí třídu resetu později kvůli losu).
2. **Exaktní vyváženost:** side-swap dává přesně 50/50 deterministicky; los má
   binomickou nerovnováhu ~±√N (u N=150 klidně ±6 her na stranu) a u
   side-specifických otázek půlí efektivní N.
3. **Párování:** seed-based los by párovou metodiku technicky přežil (stejný
   seed ⇒ stejný los v obou ramenech), ale home_win metriky by se staly směsí
   a všechny existující harnessy, uložená ramena a interpretace logů by se
   musely přeznačit.
4. **Trénink losem nic nezíská:** featury jsou perspektivně-relativní a
   self-play mirror už dnes generuje trénovací vzorky z obou rolí (home
   perspektiva „přijímám první", away „kopu první"). Los mění jen přiřazení
   rolí ke slotům, ne distribuci stavů, ze kterých se učí value/policy.
5. **Po H2 fixu je struktura skoro symetrická:** každý tým má jeden opening
   drive; zbývá jen efekt pořadí (tempo, koncovka poločasu) — řádově menší a
   side-swap měření ho plně pokrývá.

**Kdy los přehodnotit:** (a) Q2 ukáže stranovou asymetrii konverze — pak je to
ale engine bug k opravě, ne důvod losovat; (b) prokázané přeučení na fixní
rozvrh v tréninku (např. offline nález, že value je systematicky jinak
kalibrovaná pro home vs. away perspektivu).

**Vedlejší důsledek pro gating (návrh nové master-list položky):** produkční
gating hraje candidate=HOME vs. frozen=AWAY, tj. kandidát systematicky
inkasuje first-possession výhodu třídy +7pp home_win — gate verdikt je
nadhodnocený ve prospěch kandidáta. Náprava je opět side-swap (sudé hry
normálně, liché s prohozenými slotry, „výhra kandidáta" = výhra v jeho
slotu), measurement-side, bez C++. Doporučuji zařadit do fronty po
vyhodnocení Fáze A.

## 6. Rizika a limity

- **Clustering:** drivy v jedné hře nejsou nezávislé; CI konverze per-drive je
  mírně optimistické (skript to u výstupu explicitně píše). Konfirmační testy
  (Q1, McNemar) běží na úrovni her/párů, kde problém není.
- **MAX_ACTIONS truncation:** hra useknutá na 5000 akcí skončí drivem `game`
  uprostřed poločasu — segmentace to snese, jen to lehce ředí konverze.
  Post-halfclock jsou hry ~385 akcí, prakticky se to nestane.
- **5 ras × pevné pořadí matchupů:** fwd orientace přesně kopíruje produkční
  `_gate_game`; swp ji zrcadlí. Race-vs-slot McNemar (metrika 6) rozliší
  případný rasový příspěvek od slotového.
- **Skript sdílí engine build:** spustit až po dnešním rebuildu/resetu a mimo
  fáze tréninku, které drží `.so` (stejná disciplína jako u všech diag běhů).

## 7. Artefakty

- `diag_first_possession.py` — módy `run <label> [N]` / `report <label>` /
  `compare <base> <cand>` / `selftest`; paired-seed přes `diag_utils`,
  side-swap aware, per-drive summarizace uvnitř workeru (přes Pool jde jen
  malý dict). Syntax ověřen (`py_compile`), logika ověřena offline selftestem
  + extra testy hraničních případů (TD na posledním snapshotu H1, hra 0:0,
  detekce porušení po-TD rozvrhu) bez importu enginu.
- Ramena: `arm_first_possession_<label>_{fwd,swp}.json`.
- Doporučený první běh: `postfix_20260714`, N=300, detached
  (setsid+nohup+disown), log `diag_first_possession_postfix_20260714.log`.
