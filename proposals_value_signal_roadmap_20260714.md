# Strategická roadmapa: odkud vzít within-game value signál

**Datum:** 2026-07-14 · **Autor:** Fable 5 (read-only agent)
**Zdroj zadání:** závěr #3 post-mortemu `evidence/fable_ramp_postmortem_20260714.md` —
within-game strukturu V dnes dodávají FEATURY (within-episode V std 0.24), ne labely
(within-episode std MC targetu 0.028); re-mix estimátoru téhož rewardu (mc_td_mix)
nepřidal informaci (corr hlav 0.9998 při α=0.7, 0.997 i při α=0.1). Má-li within-game
signál vzniknout, musí přijít **nový informační kanál**: (a) bohatší featury,
(b) externí teacher, (c) jiný reward.

> ⚠ Nic z tohoto dokumentu nebylo spuštěno ani implementováno. Na stroji běží A/B
> (paired-seed stall-guard) — všechny navržené testy jsou specifikace k pozdějšímu
> spuštění, ne provedené experimenty. Kotvy kódu ověřeny čtením k 2026-07-14.

---

## 0. TL;DR — doporučené pořadí

| Pořadí | Kanál | Co | Náklad na falzifikaci | Status doporučení |
|---|---|---|---|---|
| **1** | (c1) | **Drive-level value target** — per-drive outcome místo/vedle per-game broadcastu | **0 nových dat, 0 C++** — offline §E test na existujícím `replay_buffer.pkl`, ~hodina práce + minuty CPU | UDĚLAT PRVNÍ (nejlevnější falzifikace v celé frontě) |
| **2** | (b1+b2) | **Teacher logging**: heuristický leaf eval H(s) + MCTS root-Q do StateLogu, pak offline distilační §E test | Malý C++ diff (+2 floaty v StateLog) + rebuild + jedny nové self-play logy — **piggyback na už plánovaný H2-kickoff baseline reset** | UDĚLAT DRUHÉ (nejsilnější mechanismus, skoro zadarmo při rebuildu, který stejně přijde) |
| **3** | (a) | **Per-player featury, Fáze A**: Python-only persistování board snapshotů + offline ridge test kandidátních featur | Python-only změna `cpp_runner.py` (žádný rebuild) + offline fit | Fáze A ano; **plná C++ implementace (73→150) jen PODMÍNĚNĚ** — po pozitivní Fázi A A ZÁROVEŇ po důkazu, že V vůbec dosáhne do hry (vf_blend bring-up) |
| — | (c2, c3…) | PBRS/potential shaping, vyšší Lever-B C, další re-mixy G, MCTS800 offline teacher | — | **ZAHODIT / ODLOŽIT** (viz §5) |

Klíčové pravidlo (lekce mc_td_mix): **žádný kandidát nedostane trénovací běh, dokud si
offline nevydělá netriviální divergenci hlavy** — vzor `diag_td_mix_target_diff.py §E`,
práh mean|ΔV| > ~0.1 při nezhoršené MSE vs G (post-mortem §4 bod 2).

---

## 1. Společný rámec: co znamená „nová informace" a jak ji levně testovat

### 1.1 Test nové informace (proč mc_td_mix umřel)

Post-mortem §3.2 strukturálně: bootstrap člen `r+γV(s′)` je sebereferenční — V je
naučené ze stejného G na stejných featurách, takže gradient mixu ≈ gradient MC.
Obecné kritérium pro každý kandidát:

> **Label smí záviset jen na (stav, budoucnost, externí znalost) — nikdy na aktuální
> hlavě V.** Pokud target_t = f(V_θ, featury_t), je to re-mix a nepřidá nic.
> Pokud target_t obsahuje veličinu, kterou z 73 featur + G nelze spočítat
> (drive outcome, plný GameState, lookahead searche), je to kandidát na nový kanál.

Tři kanály tímto testem prochází různě:
- **(a) featury**: informace vstupuje vstupem, ne labelem — prochází triviálně.
  Otázka není „je to nové", ale „zlepší to within-game generalizaci a dosáhne to do hry".
- **(b) teacher**: H(s) se počítá z plného GameState (geometrie cage, walk-in logika,
  MA/GFI), tj. z informace, kterou 73-dim projekce nenese → labely nesou novou
  informaci. NENÍ sebereferenční (H nezávisí na V) → argument §3.2 neplatí.
- **(c) reward**: musí měnit CO se odměňuje (jiná definice úspěchu), ne JAK se
  estimuje totéž G. Drive outcome je budoucnostní informace jiné granularity než
  game outcome → prochází. PBRS/potential z featur NEprochází (funkce featur;
  navíc policy-invariantní — viz §5).

### 1.2 Společný offline pre-filtr (rozšířený §E)

Pro každý kandidátní target T_t (drive-level, teacher-mix, …) spustit §E proceduru
z `diag_td_mix_target_diff.py` (stejný init `weights_best.json`, stejný seed/pořadí,
3 průchody bufferem, lr=3e-4) a reportovat:

| metrika | práh „jdi dál" | proč |
|---|---|---|
| mean\|V_T − V_ref\| | **> ~0.1** (mean\|V\|≈0.6) | mc_td_mix mělo 0.012–0.048 → mrtvé |
| corr(V_T, V_ref) | < ~0.99 | 0.997+ = zaměnitelné hlavy |
| within-episode V std | ↑ proti 0.24 ref | přímo měřená chybějící veličina |
| MSE vs G (outcome kalibrace) | nezhoršená o > ~10 % rel. | V nesmí přestat predikovat výsledek (pojistka proti mc_return_shaped 89→80 vzoru) |
| outcome-controlled ramp (§1c post-mortemu: pre-TD vs ostatní JEN uvnitř TD-epizod) | méně záporný / kladný proti −0.17 init | jediná ramp varianta, která není slepá na selekci |

Publikovanou (uncontrolled) `pre_td_value_ramp` **nepoužívat na verdikty** — ukáže
+0.6 každé outcome-kalibrované hlavě (post-mortem §1c).

### 1.3 Dvě tvrdé závislosti, které platí pro všechny kanály

1. **vf_blend=0 ⇒ jakékoli zlepšení V je v produkci herně inertní** (gating null-test,
   potvrzeno měřením 07-02). Kanály (b)/(c) zlepšují V; jestli V pomůže HŘE, se pozná
   až s vf_blend>0 — bring-up běží jako paralelní jednovariablový experiment
   (post-mortem doporučení 1; návrh `proposals_vf_blend_bringup_20260714.md`).
   Roadmapa proto odděluje **offline verdikt o hlavě** (lze hned) od **herního
   verdiktu** (až po bring-upu).
2. **Behaviorální verdikty jen paired-seed** (`diag_utils.py`), nikdy jeden N=150:
   šumové dno delta remíz je ±8–11 pp ([[feedback_draw_rate_noise_floor]]).
   A pozor na baseline resety ve frontě: H2-kickoff fix (bug 07-13) resetuje
   baseline — nové self-play logy pro (b)/(a) sbírat **až po něm**, jinak drive
   boundaries obsahují vadné výkopy.

---

## 2. Kanál (c): jiný reward — konkrétně **drive-level target (c1)**

### 2.1 Mechanismus

Dnešní G_t = γ^(T−t)·terminal_value broadcast na celou hru: 63 % stavů (bezgólové
hry) sedí na ~−0.72, TD-epizody na ~+0.85 (post-mortem §1a). Within-episode std
labelu 0.028 — label je uvnitř hry skoro konstanta.

**Návrh:** target po drivech, ne po hře:

```
drive_outcome(d) = +D   pokud drive d končí vlastním TD
                   −D   pokud soupeřovým TD
                   −d0  pokud vyšumí (konec poločasu/hry bez TD; d0 malé, „nevyužitý drive")
T_t = clip( λ·G_t  +  (1−λ)·γ^(k_d−t)·drive_outcome(drive obsahující t), −1, 1 )
```

s λ≈0.5–0.7, D≈0.6, k_d = index konce drivu. Proč to dodává within-game signál,
který dnešní setup nemá:
- **V labelech**: hranice drivů leží UVNITŘ hry → label skáče na hranicích drivů
  a diskontuje se k místnímu (blízkému) horizontu, ne k−15 tahů vzdálenému konci
  hry (γ^15≈0.86 dnes srovnává celou hru na jednu hodnotu). Within-episode std
  targetu poroste řádově, ne o +0.072 jako Lever-B bump.
- **Nová informace**: drive outcome je budoucnostní veličina jiné granularity —
  není funkcí V ani featur, není odvoditelná z per-game G (hra 1:1 může mít driv
  vyhraný i prohraný). Prochází testem §1.1.
- **Cílí na potvrzenou slabinu**: synteza 07-09 vrstva 4 — „offense, not defense,
  is the real gap" (0.71 TD/g, 0.02 obdržených). Odměna za KAŽDÝ konvertovaný
  drive je přesně tlak na konverzi, ne na výsledek zápasu, který je stejně skoro
  vždy remíza/těsná výhra.

### 2.2 Náklady

- **Falzifikace: nulové nové náklady.** `replay_buffer.pkl` Transition už nese
  všechno potřebné: `reward_step` (znaménko = čí TD → hranice drivů),
  `is_terminal` (konec epizody), `mc_return` (λ-mix referenční složka). Nový
  `diag_drive_target_diff.py` = kopie §E s jinou konstrukcí targetu; minuty CPU
  (stejná zátěž jako 07-14 post-mortem skript). **Jediný kandidát v celé frontě
  falzifikovatelný ještě dnes, bez čekání na cokoliv.**
- Implementace (jen pokud pre-filtr projde): Python-only — `rewards.py` nová
  funkce `drive_level_returns()` (SSOT vedle `episode_returns`), napojení ve
  stejných 4 cestách jako mc_td_mix (Linear/Neural × full-log/replay), env knob
  `BB_TRAINING_METHOD=drive_mix` + `BB_DRIVE_LAMBDA`. Struktura patche zrcadlí
  hotový mc_td_mix diff (master list item 11) — většina wiring práce je
  opsatelná. Žádný C++, žádný rebuild.

### 2.3 Rizika

- **Mění optimum — záměrně a přiznaně** (stejná třída jako terminal_value
  re-pricing, který je přijatá konvence; NENÍ to PBRS a nemá být). Konkrétní
  pasti: (i) *TD trading* — hlava může přestat penalizovat obdržený rychlý TD,
  pokud za něj tým dostane míč a šanci na vlastní drive → držet λ·G_t složku
  dost velkou, aby výhra zápasu dominovala; hlídat MSE vs G v pre-filtru.
  (ii) *Stall devaluace* — 8-tahový grind na 1 TD je legitimní strategie;
  d0 (penalizace vyšumělého drivu) držet malé.
- **Interakce s Lever-B**: reward_step fold-in (C=0.2) už v G_t je; drive složka
  ho částečně duplikuje. V implementaci definovat čistě (drive_outcome NAHRAZUJE
  bump uvnitř své složky, ne sčítá se s ním) — jinak dvojí odměna za TD.
- **H2-kickoff bug**: hranice drivů v DNEŠNÍM bufferu jsou po posledním fixu
  kickoffů v zásadě validní (TD eventy jsou správně), ale směr výkopu H2 je vadný
  → offline pre-filtr na dnešním bufferu je OK pro verdikt o divergenci hlavy,
  finální trénink až na post-fix datech.
- Šance na neúspěch, kterou test odhalí levně: pokud i drive-level target
  přemapuje hlavně stavy PODLE výsledku drivu stejně plošně (tj. hlava na 73
  featurách nemá kapacitu odlišit stavy uvnitř drivu), mean|ΔV| bude vysoké, ale
  within-episode V std se nehne → to by byl silný argument, že binding constraint
  jsou featury (kanál a), a (c) se odloží.

### 2.4 Falzifikační plán (offline, před implementací)

`diag_drive_target_diff.py`: (1) rekonstrukce epizod a drivů z bufferu
(`reward_step`≠0 = hranice, `is_terminal` = konec); (2) konstrukce T_t pro mřížku
(λ∈{0.7,0.5,0.3}, D∈{0.4,0.6}, d0∈{0,0.1}); (3) §E A/B retrain vs mc_return;
(4) report metrik z §1.2. Verdikt: aspoň jedna kombinace nad prahy → implementovat
`drive_mix`; jinak zapsat REFUTED a jít na (b).

---

## 3. Kanál (b): externí teacher

### 3.1 Varianta b1 — heuristický leaf eval jako distilační teacher (hlavní)

**Mechanismus.** `macro_mcts.cpp::simulate()` (:483–693) počítá `heuristic +
scoringBonus` z plného GameState: cage geometrii, walk-in/one-turn-TD logiku s MA
a GFI, pacing, screen — informace, které 73-dim featury nenesou (přesně ty, kvůli
kterým se navrhoval per-player plán). Tahle heuristika je zároveň to, co reálně
hraje na 91.5 % benchmarku — je to jediný „expert" v projektu, který prokazatelně
umí within-game rozlišovat stavy (jinak by nevyhrával). Distilace:

```
T_t = clip( α·G_t + (1−α)·H̃(s_t), −1, 1 ),   H̃ = clamp(heuristic+scoringBonus)  (už v [−1,1])
```

- Labely nesou novou informaci (H je funkce stavu bohatší než featury) — §1.1 OK.
- NENÍ sebereferenční: H nezávisí na V → gradient se od MC skutečně liší
  (na rozdíl od r+γV(s′)).
- **Strategický bonus, který žádný jiný kanál nemá:** V ≈ (outcome kalibrace + H)
  je přesně ta hlava, se kterou je vf_blend bring-up bezpečný. Historická regrese
  „vf_blend ředí scoring-pull heuristiky → pasivita" (draw-collapse 06-24) zmizí
  z konstrukce: blendovat H s V≈H nic neředí. b1 a vf_blend bring-up se vzájemně
  odblokovávají.

**Náklady.**
- Logging: přidat do `StateLog` (`include/bb/game_simulator.h:45`) pole
  `float heuristicValue` (a rovnou i `float rootValue`, viz b2), naplnit v místě,
  kde `simulateGameLogged` pushuje stavy, propsat přes `bb_module.cpp::get_states`
  a `cpp_runner.py` jsonl writer (ten už dnes zipuje states×turn_logs). Odhad:
  desítky řádků, jeden rebuild. Per-state cena = 1 leaf eval navíc — zanedbatelné
  proti 100 MCTS iteracím na tah.
- Data: potřeba JEDNY nové self-play logy. **Nepálí nic navíc: H2-kickoff fix
  stejně vynutí rebuild + baseline reset + čerstvý běh — bundlovat logging do
  téhož rebuildu** (čistě aditivní pole, nemění chování, žádné interakční riziko
  se screen fixy 8+9 ani s ničím z master listu).
- Trénink (po pre-filtru): Python wiring identický vzor jako mc_td_mix/c1.

**Rizika.**
- **Teacher ceiling**: V distilované k H nemůže být lepší než H; dědí známé
  patologie heuristiky (stall sklony — částečně fixnuté stall-guardem a88f5e2;
  SCORE-availability díra z re-miningu 07-14 — pozor, H tu díru MÁ, viz
  `proposals_score_availability_20260714.md`). Mitigace: α-mix (ne čistá
  distilace) — outcome složka drží V ukotvenou na výsledku; ceiling řešit až to
  bude binding (nejdřív ať V vůbec něco within-game umí).
- **Dvojí započtení při vf_blend>0**: scoringBonus se přičítá POST-blend
  (`macro_mcts.cpp:676-692`, fix #1) → pokud V nasaje scoringBonus přes
  distilaci, efektivní scoring pull se při blendu zdvojí. Mitigace: do H̃ pro
  distilaci logovat **heuristic a scoringBonus ZVLÁŠŤ** (dvě čísla, skoro
  zadarmo) a rozhodnout na datech, jestli distilovat `heuristic`, nebo
  `heuristic+scoringBonus`; při bring-upu hlídat over-aggression metriky.
- Riziko kapacity: linear hlava na 73 featurách nemusí H umět reprezentovat —
  to není protiargument, to je přesně měřená veličina pre-filtru (a most ke
  kanálu (a): R² fitu featury→H kvantifikuje, KOLIK by per-player featury
  přidaly — viz §4.4).

**Falzifikace (offline, po jednom logging běhu).**
`diag_teacher_target_diff.py`: (0) sanity — corr(H, G) a within-episode std H
(očekávám std(H) >> 0.028; kdyby ne, teacher je taky plochý a kanál umírá levně);
(1) přímý fit featury→H (ridge): R² a within-ep std reziduí = kapacita hlavy na
teacher; (2) §E A/B s T_t pro α∈{0.7,0.5,0.3}; (3) metriky §1.2. Verdikt: prahy
z §1.2 + „distilovaná V má within-ep std výrazně > 0.24".

### 3.2 Varianta b2 — MCTS root value jako teacher (logovat hned, použít později)

Root Q vybraného tahu při self-play MCTS100 už je spočítané — logging je zadarmo
(druhý float v StateLogu). Obsahuje lookahead přes kostky = informace nad rámec
statického H i featur. Použití jako target = standardní AlphaZero praxe
(z vs root-Q mix) → přirozený krok 2 distilace, až b1 projde.
**Riziko navíc proti b1**: při vf_blend>0 se root Q stává funkcí V (V je v leaf
evalu) → target je zase sebereferenční, tentokrát přes search — to je legitimní
AZ dynamika, ale vyžaduje drift monitory (w_norm, MSE vs G po epochách), přesně
deadly-triad výhrada z team_alphazero_results.md. Proto: **logovat od prvního dne
(zadarmo), trénovat na tom až po b1 a po stabilním vf_blendu.**

### 3.3 Varianta b3 — vysokorozpočtový search (MCTS800) jako offline teacher

ODLOŽIT: 8× compute na hru jen kvůli targetům, b2 pokrývá většinu hodnoty za
nulový příplatek. Vrátit se k tomu jedině, až b2 prokáže, že root-Q targety fungují,
a ceiling bude měřitelně binding.

---

## 4. Kanál (a): per-player featury

### 4.1 Mechanismus a proč se vrací do hry

Post-mortem: within-game strukturu V dodávají featury (V std 0.24 = 9× std labelu)
— **featury jsou dnes jediný funkční kanál within-game signálu a zároveň jeho
bottleneck**. Bohatší featury = víc within-game rozlišení i pod nezměněným MC
targetem. Konkrétní slepá místa jsou zmapovaná (team1_brief + Opus re-run):
cage-corner ST/Guard, carrier_blitzable přes reálný BFS (dnešní f63 je Chebyshev
nadhodnocený), volný receiver, net_st_for_block.

Historická výhrada „per-player NENÍ fix" (team1_v2 konsensus 06-23) se
reinterpretuje: tehdy šlo o vysvětlení ploché POLICY při odpojených hlavách —
a verdikt vznikl v éře rozbitého měřicího stacku (synteza 07-09 vrstva 1+2).
Post-mortem 07-14 dává per-player featurám novou, užší roli: ne „fix draw-rate",
ale **jediný kanál, jak zvýšit kapacitu V hlavy na within-game strukturu**.
Odklad „do vyřešení root cause" je splněn — root cause je lokalizovaný a říká
„featury/reward, ne tvar targetu".

### 4.2 Fáze A — Python-only prototyp (levná falzifikace PŘED C++)

Board snapshoty (id, x, y, state, has_ball per hráč, ball pozice) UŽ tečou přes
pybind (`bb_module.cpp::get_turn_logs:197-266`), jen se nepersistují —
`cpp_runner.py` z turn logu bere pouze skóre. **Změna: přibalit do game_*.jsonl
state recordu i player snapshoty** (Python-only, žádný rebuild; statické staty
MA/ST/AG/skills se doplní z `get_roster()` podle id). Pak offline:

1. Z nasbíraných logů (opět: piggyback na post-H2-fix běhu) spočítat kandidátní
   per-player featury v Pythonu — startovní sada ~dle Opus plánu (carrier slot +
   ~5 klíčových hráčů ≈ 73→~150): dist_to_ball, in_carrier_diagonal, cage_corner
   eff_st/guard, can_reach_carrier (BFS s TZ — v Pythonu na 26×15 gridu triviální,
   offline nevadí pomalost), is_free_receiver, adjacent_to_sideline.
2. Ridge fit **G ~ f73** vs **G ~ f73+kandidáti** (split po epizodách, ne po
   stavech — jinak leakage) → ΔR², a hlavně within-episode std predikce
   + outcome-controlled ramp obou fitů.
3. Pokud běží i b1: fit **H ~ f73** vs **H ~ f73+kandidáti** — přímo měří, kolik
   teacher-informace nové featury zpřístupní (nejostřejší možný test).

Verdikt Fáze A: kandidáti musí zvednout within-episode strukturu predikce
(analogicky mean|ΔV|>0.1 / std ↑) — jinak plný C++ per-player NEdělat a zapsat
proč (ušetří 11–14 dní práce z Opus odhadu).

### 4.3 Fáze B/C — C++ implementace (podmíněná)

- Fáze B: inkrementální 73→~150 dle Opus plánu (warm start přes `_align_features`;
  u LINEÁRNÍ hlavy jsou nové váhy init 0 → kandidát startuje bit-identický
  s baseline = nulové riziko regrese v okamžiku nasazení). Náklady dle Opus
  auditu: ~12 míst změny, C++↔Python paritní test jako gate, pathfinder TZ/dodge
  (dnes mrtvý kód `PathNode::dodged`), perf precompute (occupancy/tzMap).
- Fáze C: plný ~492 + síťová hlava — až po důkazu z B, beze změny proti Opus plánu.
- **Spouštěcí podmínky B (obě):** (i) Fáze A pozitivní; (ii) vf_blend bring-up
  prokázal, že V dosahuje do hry (jinak zlepšujeme hlavu, kterou nikdo nečte).

### 4.4 Rizika

- Perf: extractFeatures běží v leaf evalu — bez precompute 3–10× zpomalení MCTS
  (Opus audit). Fáze A tomu uniká úplně (offline).
- Parita C++↔Python (`features.py` má vlastní extraktor) — povinný gate.
- Slot-ordering nestabilita (tie-break přes player.id — už specifikováno).
- **Baseline reset**: změna featur zneplatní `weights_best`, `all_time_best_bm`
  i všechny σ-tier reference → načasovat mimo AZ bring-up (viz §6).
- Nejpravděpodobnější mód selhání: featury pomůžou fitu na G/H offline, ale hra
  se nezmění, protože V do hry nedosáhne — proto podmínka (ii) výše.

---

## 5. Co zahodit / neotvírat znovu (a proč)

| Kandidát | Verdikt | Důvod |
|---|---|---|
| Další re-mixy G (mc_td_mix varianty, jiné α schedule, TD(λ) na týchž featurách/rewardu) | **ZAHODIT** | Post-mortem §3.2: sebereferenční bootstrap ≈ no-op pro libovolné α; TD(λ) je tatáž třída (V z téhož G na týchž featurách). Výjimka: bootstrap se vrací legitimně až jako b2/AZ (root-Q má lookahead = nová informace). |
| PBRS / potential shaping (Φ z featur; vč. reverted 06-30 β·Φ diffu) | **ZAHODIT** | Dvojnásobně mrtvé: diff forma telescopuje (policy-invariantní, mc_return_shaped 89→80 historie), LEVEL forma je funkce featur → žádná nová informace (§1.1); 06-30 pokus commit-push-revertnut jako obsoletní. |
| Vyšší Lever-B C (>0.2) | **ZAHODIT** | Změřený within-episode příspěvek C=0.2 je +0.072 (post-mortem §1a) — k std 0.24 by C muselo být obří; leverb historie (89→85.4) ukazuje, že terminal-kalibraci to poškodí dřív, než pomůže. |
| scoringBonus / leaf-eval ladění jako „value signál" | NEPATŘÍ SEM | Je to search-side heuristika (post-blend), ne trénovací signál — žije ve vlastní frontě (`proposals_score_availability_20260714.md`). |
| MCTS800 offline teacher (b3) | ODLOŽIT | 8× compute, b2 zadarmo pokrývá; vrátit se jen s důkazem, že root-Q targety fungují a ceiling binding. |
| Verdikty přes pre_td_ramp (uncontrolled) nebo entropy/policy_loss/top1 | **ZAKÁZÁNO** | Post-mortem §1c (ramp = selekce outcome); [[project_bloodbowl_entropy_is_artifact]]. |

---

## 6. Návaznost: AlphaZero bring-up a during-training gate

### 6.1 AZ policy bring-up (team_alphazero_results.md, sekvence 1–7)

- **(c1) drive-level**: ortogonální k policy bring-upu — mění jen value target,
  nedotýká se priorů, blendu ani policy tréninku. Jediná interakce: AZ Agent 3
  navrhoval shaping A/B (mc_shaped vs mc vs td_lambda) — c1 do té matice vstupuje
  jako čtvrté rameno a **částečně řeší jeho hlavní obavu** („remíza dává z=0 →
  řídký signál"): drive-level dává nenulové labely i remízovým hrám s driv-ději.
  Kompatibilní bez úprav.
- **(b1/b2) teacher**: **synergie, ne konflikt** — AZ krok 5 (postupné zapnutí
  blendu) historicky narážel na „V ředí heuristiku"; V distilovaná k H tento
  regres z konstrukce odstraňuje (§3.1). b2 root-Q target je doslova AZ value
  target — logging teď je příprava na AZ, ne odbočka. Jediné pravidlo: netrénovat
  na root-Q, dokud vf_blend není stabilní (sebereference přes search, §3.2).
- **(a) per-player**: **největší interakce.** AZ sekvence explicitně: „nejdřív
  stabilní AZ na [73] featurách, pak škálovat" — změna NUM_FEATURES mění vstup
  value I policy sítě, ruší warm-start policy hlavy a resetuje benchmark
  reference. Fáze A (Python offline) nekoliduje s ničím a může běžet souběžně
  s bring-upem; **Fáze B (C++) zásadně NEnasazovat uprostřed bring-upu** — buď
  před ním (nepravděpodobné časově), nebo až po jeho stabilizaci, jako
  samostatný baseline reset.

### 6.2 During-training gate

- Při vf_blend=0 je KAŽDÉ zlepšení V pro gate neviditelné (null-test) — gate
  verdikty PROMOTE/REJECT o kanálech (b)/(c) nic neříkají, dokud bring-up
  neproběhne. Proto jsou všechny falzifikace v §2–4 offline: gate v téhle fázi
  není měřidlo.
- Po vf_blendu>0: gate začne měřit chování → platí paired-seed metodika
  (diag_utils, McNemar) a konverzní metriky (TD/g, pickup/threat conversion —
  synteza 07-09 bod 5) místo draw-rate. Pro c1 specificky: očekávaný směr je
  agresivnější offense → draw-rate může klesnout i vzrůst nezávisle na kvalitě
  (soupeř v mirroru zlepšuje totéž) — soudit podle TD/g a drive-conversion,
  ne draws.
- Dirichlet/exploration_c gate fixy (master list Phase 0) jsou prerekvizitou
  každého herního verdiktu zde — beze změny.

---

## 7. Rozhodovací strom (kondenzovaně)

```
1. diag_drive_target_diff.py (offline, hned, existující buffer)
   ├─ projde prahy §1.2 → implementovat drive_mix (Python-only, vzor mc_td_mix),
   │    trénovat až po H2-fix baseline resetu; herní verdikt až s vf_blend>0
   └─ neprojde → REFUTED zápis; pokud ΔV vysoké ale within-ep std stojí
        → silný ukazatel na featury jako binding constraint → posílit prioritu (a)
2. Při H2-kickoff rebuildu přibalit: StateLog {heuristic, scoringBonus, rootQ}
   + snapshot persist v cpp_runner (Python). Z prvního post-fix běhu:
   ├─ diag_teacher_target_diff.py → b1 verdikt (§3.1)
   └─ per-player Fáze A ridge testy (§4.2) → (a) verdikt
3. vf_blend bring-up (paralelní, vlastní návrh) rozhoduje, KTERÝ z prošedších
   kandidátů dostane první plný trénovací běh: preferenčně b1 (odblokovává
   samotný bring-up), pak c1, pak (a) Fáze B.
```

**Souhrn pořadí: (c1) drive-level offline test HNED → (b1+b2) teacher logging
přibalit k H2-fix rebuildu a offline otestovat → (a) Fáze A offline souběžně,
plné per-player C++ jen podmíněně (pozitivní Fáze A + V prokazatelně ve hře),
a nikdy uprostřed AZ bring-upu. Zahodit: re-mixy G, PBRS/potential, vyšší
Lever-B C, MCTS800.**
