# Fable 5 research queue — 2026-07-09

Spuštěno na pozadí (postupně, ne paralelně), zatímco běží trénink
(`training_post_expandscore_fix_20260709.log`, PID 147037/147038, start 09:23 UTC).
Všechny tři úkoly jsou READ-ONLY (agenti nesměli nic v repu upravovat) — bezpečné
souběžně s běžícím tréninkem. Kontrolovat zítra.

Status: **VŠECH 7 ÚKOLŮ HOTOVO** (2026-07-09, poslední ~15:45 UTC).

---

## 1. Gating null-test — je dnešní gate verdikt vůbec informativní?

**Stav: HOTOVO (2026-07-09 ~14:21 UTC)**

**Verdikt: nález z 2026-07-02 STÁLE PLATÍ.** Dnešní gate (PROMOTE/REJECT o vahách)
nebude informativní — candidate a frozen jsou pořád mechanisticky identičtí agenti.
Sledovat z dnešního běhu má smysl jen watchdog-skip% a draw-rate telemetrii (to je
skutečný účel tohoto běhu), NE promote/reject výrok samotný.

**Proč (kód, HEAD `39f689a`):**
- Value váhy se sice per-side předávají (`run_iteration.py:216` →
  `bb_module.cpp:416-427`), ale `valueFn_` se použije jen když
  `config_.vfBlend > 0.0f` (`macro_mcts.cpp:680`) — a gate pořád jede s
  `VF_BLEND=0.0` (`run_iteration.py:38`, žádný env override). Větev je mrtvá.
- Policy je mrtvá dvakrát: (a) `_gate_game` nikdy nepředává `policy_blend`, takže
  padá na default `0.0` (`bb_module.cpp:519`), učený prior se blendne jen při
  `policyBlend > 0` (`macro_mcts.cpp:266,402`); (b) `bb_module.cpp` má jen JEDEN
  `policy_weights_path` parametr — stejná síť jde do OBOU stran (`:454-457`), gate
  navíc schválně používá jeden sdílený `gate_policy_path` (`run_iteration.py:407`).
  I kdyby blend byl >0, obě strany by dostaly identický prior.
- Benchmark noha (`_benchmark_game`) jede na stejném `VF_BLEND` → i
  az_train-vs-train_best selekce (`:429-445`) je fakticky hod mincí mezi
  identickými agenty.
- Kvantitativní důsledek: s identickými agenty je `chess_score` ~Binomial(0.5),
  práh promote `0.5+k·σ` → P(promote) ≈ 16%/6.7%/2.3% pro k=1.0/1.5/2.0 — gate je
  strukturálně vychýlený k REJECT; jakýkoli historický PROMOTE (mimo baseline_reset)
  byl šum.

**Gating redesign (2026-06-15 plán) — implementován, ale řeší jiný problém.**
Commit `ead2a21` (2026-06-16, dual-signal σ-rule) + `46c5691` je živý v
`run_iteration.py:71-76,500-546`. Opravil STATISTIKU verdiktu (práh mimo šum),
ne CO se měří — oba vstupní signály (HtH i benchmark) jsou pořád weight-blind.
Redesign predchází null-test nálezu o ~2.5 týdne, nemohl ho předjímat.

**Minimální oprava (Python-only, BEZ rebuildu enginu):**
1. `run_iteration.py:53` přidat `GATE_VF_BLEND = float(os.environ.get('BB_GATE_VF_BLEND', 1.0))`
2. V gate task tuple `run_iteration.py:452` nahradit `VF_BLEND` → `GATE_VF_BLEND`

Všechno propojení už existuje (per-side value váhy se předávají,
`macro_mcts.cpp:680-687` je umí blendnout při vfBlend>0) — jde jen o to gate
fázi přepnout na nenulový blend, benchmark nechat na 0.0 (zachovat `BM_FLOOR=0.77`
srovnatelnost). Per-side POLICY citlivost by vyžadovala C++ změnu
(`away_policy_weights_path` param) + rebuild — mimo rozsah tohoto minimálního fixu.

**Bezpečnost aplikace PŘI běžícím tréninku: bezpečné, ale neúčinné dnes.**
`run_iteration.py` nedělá `git pull` (to je jen na remote serveru); běžící proces
má modul načtený v paměti od 09:23, gate `Pool` workery forkují (žádný
`set_start_method`), takže editace/commit souboru dnes je pro živý běh inertní —
projeví se až při příštím spuštění. **Jediné, čemu se dnes vyhnout: nerebuildovat
engine** před dokončením gate fáze (gate workery importují `bb_engine` z disku po
forku — vyměněné `.so` by se načetlo mid-run). Navržený fix rebuild nepotřebuje.

**Stav běhu při kontrole (14:21 UTC):** self-play/imitation dohráno (log tichý od
12:43), momentálně vnitřní 400-zápasový benchmark epochy 16 (jednovláknový, proto
ticho v logu — konzistentní s dřívějším pozorováním). Gate fáze (kroky 3-5) přijde
po něm, verdikt možná dřív než odhadovaných 21:31 UTC.

**Doporučení pro zítra:** zvážit nasazení dvouřádkové opravy PŘED příštím
tréninkovým během (ne teď, ne do tohoto běhu). Nová oprava = nová referenční
hodnota draw-rate (současné 42.6-46.6% reference byly měřené při vf_blend=0,
nejsou přímo srovnatelné).

---

## 2. TTM scatter hang-candidate audit (`ttm_handler.cpp:106`)

**Stav: HOTOVO (2026-07-09 ~14:35 UTC)**

**Verdikt: TTM scatter NENÍ hang bug — false alarm ze starého `hang_analysis.md`.**
Ale sweep našel jeden reálný (ohraničený, ne hang) bug jinde.

**Proč TTM scatter není nebezpečný:**
- Smyčka (`ttm_handler.cpp:106-117`) kontroluje `!landPos.isOnPitch()` KAŽDOU
  iteraci → nemůže nikdy akumulovat sentinel/overflow souřadnice (na rozdíl od
  opraveného `expandScore`).
- `resolveThrowTeamMate` je **nedosažitelná z `replayToNode` open-loop replaye**
  — žádná macro-expanze nikdy nevybere `THROW_TEAM_MATE`, takže mechanismus,
  který způsobil expandScore hang (cached macro proti hráči, co mezitím opustil
  hřiště), sem nemá cestu. TTM se provádí jen v generate-a-hned-execute
  kontextech, kde je thrower/projektil garantovaně na hřišti.
- Pravděpodobnostně: smyčka končí s geometrickým ocasem (očekávané řádově
  jednotky až desítky iterací), žádný realistický scénář nedá 100+s hang.
- Starý `hang_analysis.md` ji flagnul čistě proto, že neměla hard cap — směr
  správný (chybí defense-in-depth), ale netrefil skutečnou třídu bugu.

**Nově nalezený reálný bug (ohraničený, ne hang):** `expandBlitzAndScore`
(`macro_actions.cpp:965`) — pokud samotný blitz srazí blockera z hřiště
(crowd surf/KO), následná smyčka honí blockerovu `{-1,-1}` sentinel pozici.
Nehází — ohraničeno na 12 kroků (`:947`) — ale plýtvá celou aktivací blitzera
na nesmyslnou chůzi v každém rolloutu, kde blitz KO'ne blockera → kontaminuje
BLITZ_AND_SCORE value odhady.

**Dvě další reachable-from-replay, ale neškodná místa** (hygiena doporučena,
ne urgentní): `expandAdvance:826` a `expandCage:842` — čtou stale carrier
pozici bez guardu, ale downstream `movePlayerToward` guard (`:706`) hned
zastaví, takže jen plýtvá MCTS iterací, nehangne.

**Zbytek sweepu (macro_mcts.cpp, ostatní expand* funkce):** false positives —
buď guardované jinak (`expandBlitz`, `expandPickup`, `expandReposition`,
`expandHandOffScore`/`expandPassScore`/`expandChainScore`), nebo chráněné
enginovým invariantem "ball.isHeld ⇒ carrier on-pitch" (`simulate()` own-carrier
větev, `macro_mcts.cpp:502-508` — nekonzistence: opponent-carrier větev
`isOnPitch()` kontroluje, own-carrier ne, ale je to jen aritmetika, ne smyčka).
**Žádný další unbounded-loop-on-sentinel bug nenalezen v macro_actions.cpp,
macro_mcts.cpp, ani ttm_handler.cpp po commitu 39f689a.**

**Navržené opravy (priorita, žádná teď — až po dnešním běhu):**
1. **`expandBlitzAndScore` blocker guard** (jediný reálný nález) —
   `macro_actions.cpp:947-968`, přidat na začátek smyčky:
   `if (!blocker.isOnPitch()) break;`
2. TTM scatter defense-in-depth cap (volitelné, nemůže se spustit z replaye) —
   `ttm_handler.cpp:106`, přidat `guard < MAX_SCATTERS` k podmínce smyčky.
3. Hygiena guards `expandAdvance`/`expandCage` (levné, replay-reachable) —
   `if (!carrier.isOnPitch()) return result;` po `:820`/`:841`.
4. Volitelný nit: `simulate()` own-carrier větev pro konzistenci se sourozenci.

---

## 3. Replay-mining zbylé nálezy (stall-guard + screen defense)

**Stav: HOTOVO (2026-07-09 ~14:50 UTC)**

**Verdikt: oba nálezy STÁLE PLATÍ na dnešním kódu**, mechanismus beze změny;
plus nový detail k pickup-priority (nález #1) — pořád otevřený, jiné dva
pickup commity opravily něco jiného.

**3a. Stall-guard (blitz-risk-blind pacing) — PLATÍ, mírně ROZŠÍŘENÝ.**
Logika ("dorazit přesně poslední tah") byla přejmenovaná/vytažená do
`carrierStallAwareSteps()` (`macro_actions.cpp:798-815`, commit 2af4252) a teď
se používá na DVOU místech (ADVANCE i post-pickup) — pořád nulová znalost
soupeřovy hrozby (žádný scan TZ/blitzability). Engine PŘITOM signál už počítá
jinam — feature f63 `carrier_blitzable` (`feature_extractor.cpp:356-365`) —
ale macro-expanze ho nikdy nekonzultuje.
- Half-clock fix (676bb50) opravil `turnsRemaining` (dřív se po každém gólu
  falešně nafukoval zpět na ~8) → závažnost stallingu naměřená na 06-30 datech
  je pravděpodobně nadhodnocená pro dnešní build, ale mechanismus je nedotčený.
- Navržený fix (~10 řádků): v `carrierStallAwareSteps` přidat blitzability
  scan (stejná logika jako f63) — pokud hrozí blitz, throttle přeskočit
  (chovat se jako větev `turnsRemaining<=2`, `:810-813`). Update volajících na
  `:824` (`expandAdvance`) a `:1019` (`expandPickup`).

**3b. Screen=0 obranná díra — PLATÍ, NENÍ artefakt měření.**
Koncept screen v kódu existuje (feature f62 `feature_extractor.cpp:345-354` +
REPOSITION "Strategy 4" `macro_actions.cpp:663-671`), ale nic negarantuje, že
se obránce skutečně dostane mezi nosiče a endzónu:
- Zainteresovaní hráči (v TZ) nedostanou žádnou REPOSITION macro (`:548-559`)
  — přesně ti nejblíž nosiči jsou vyloučení.
- `expandReposition` má hard-cap 4 kroky (`:1067-1072`) bez ohledu na MA/
  vzdálenost — stejná třída bugu jako PICKUP step-cap (2899cd5), tady
  neopravená.
- Screen je nejnižší priorita v obranném žebříčku (za cage-tag/safety/marker/
  endzone-guard), fixní Y souřadnice {3,5,7,9,11} — nosič sprintující po
  křídle (y=1-2) je snadno mine.
- Defenzivní REPOSITION prior podfinancovaný (`minPrior=0.05`,
  `macro_mcts.cpp:372`) vs BLITZ 0.20, BLOCK 0.12; FOUL nemá na obraně žádný
  strop — konzistentní s namontovaným ~13% FOUL/END_TURN/BLOCK plýtváním.
- Navržené fixy (2 nezávislé, měřit zvlášť): (1) `expandReposition:1070`
  cap 4→skutečný movement budget hráče; (2) nová "Strategy 0.5" intercept
  targeting na skutečné Y nosiče místo fixních Y, vložit před `:643-649`.
  Volitelně (3) zvednout defenzivní REPOSITION prior + strop na FOUL.

**4. Pickup priority (multi-candidate pickers) — STÁLE OTEVŘENÉ, jiné 2 commity
řešily něco jiného.** Generace pořád vybírá jen JEDNOHO `bestPicker`
(`macro_actions.cpp:434-475`) a emituje přesně jednu PICKUP macro. `2af4252`
opravil throttling PO úspěšném pickupu, `2899cd5` opravil step-cap PŘI cestě
k míči — ani jeden se netýká toho, kolik kandidátů se vůbec generuje. Navržený
fix: sesbírat top-2/3 skórovaných pickerů (místo jednoho `bestPicker`) a
emitovat PICKUP macro pro každého, ať MCTS arbitruje — zrcadlí, jak už BLITZ
generace dělá top-2 kandidáty na obraně (`:368-373`).

**Průřezová výhrada pro zítřek:** všechna čísla z mining reportu (43% loose-ball,
82% pickup miss, 51% threat-conversion) jsou z 06-30 dat, PŘED 5 z 8 mezitím
shipnutých oprav. Mechanismy jsou potvrzené na dnešním kódu, ale jejich
změřený DOPAD na remízy by se měl přeminovat z prvních čistých post-fix logů
(`training_post_expandscore_fix_20260709.log`), než se fixy seřadí podle
očekávaného přínosu.

---

## 4. AlphaZero bring-up audit — je čas znovu zkusit vf_blend/policy_blend?

**Stav: HOTOVO (2026-07-09 ~15:35 UTC)**

**Verdikt: ANO pro `policy_blend` (kvalifikovaně), spíš-později-ANO pro
`vf_blend`.** AZ infrastruktura je reálná a částečně už BĚŽÍ — dnešní trénink
učí neural policy hlavu v imitation módu každou iteraci — ale ani jedna naučená
hlava se nedostane do searche: `vf_blend=0.0` dělá z `--vf-ramp-epochs=10`
naprosté nic (ramp je multiplikativní CÍLOVOU hodnotou, 0×cokoliv=0) a
`policy_blend` nemá žádný ramp mechanismus vůbec — jen skok na konstantu PO
imitation epochách — a navíc je dnes **strukturálně nedosažitelný**, protože
`IMITATION_EPOCHS=16 == EPOCHS=16` (`run_iteration.py:26,69`), takže blend je
vždycky 0 bez ohledu na nastavení `--policy-blend`.

**Bug tabulka ze starého AZ reportu (2026-06-15) — stav dnes:**
| # | Bug | Stav dnes |
|---|---|---|
| 1 | Dirichlet noise hardcoded 0.3 v benchmark/gating (kontaminuje MĚŘENÍ síly) | **STÁLE PŘÍTOMNÝ** — `run_iteration.py:175,212` nikdy nepředává `dirichlet_alpha`, padá na default 0.3 (`bb_module.cpp:522`) |
| 2 | Blend skok 0→plná hodnota, žádný ramp pro policy (na rozdíl od VF) | **STÁLE PŘÍTOMNÝ**, navíc dnes strukturálně nedosažitelný (viz výše) |
| 3 | policy_loss se nelogoval, nešlo poznat že se učí | **OPRAVENO** — `epoch_metrics.csv` má `policy_loss`/`policy_agreement`/visit entropy |
| 4 | Temperature mismatch (blend softmax temp=1.0 vs Python 0.3) | **STÁLE PŘÍTOMNÝ**, nízká závažnost |
| 5 | Lineární trainer ignoroval `passes` | **BEZPŘEDMĚTNÉ** — přešlo se na neural |
| 6 | hidden>64 se tiše ořízne | **STÁLE PŘÍTOMNÝ v C++**, ale neškodí — produkce sedí přesně na capu (64) |

**"VF inversion" historie (2026-05-20, `caa99da`→`06c9283`):** `vf_blend 0→0.3`,
benchmark 88%→76%, both-side-positive VF perspective inversion v epochách
6/7/10 — stalo se to I S aktivním rampem. Mechanismus (scoringBonus ředěný
blendem) byl od té doby **strukturálně odstraněn** (`524a39f`, 2026-06-24:
scoringBonus teď mimo blend). Ale **value-target flatness root cause zůstává
neopravený** (gamma-discounted terminal broadcast, skoro nulový signál na
~45% remízových her) — takže i nový pokus s vf_blend má tlumená očekávání.

**Proč je teď lepší čas než v březnu/květnu/půlce června:** 7 mechanistických
enginových oprav od 07-03 (watchdog-skip 2-5/150→0/150), negamax + turnover-
discard backprop znamenají, že visit-distribuce, které policy imituje, jsou
poprvé SPRÁVNÉ. Ale: dnešní `top1_agree≈38-40%`/`mcts_H≈0.90` ukazuje, že
neural hlava zatím jen DOHNALA starý lineární strop, ne překonala — tlumit
očekávání na velký zisk z blendu.

**Doporučená posloupnost (žádný krok neimplementovat do dnešního běhu):**
- **Krok 0** (předpoklad): nechat dnešní běh doběhnout, přečíst gate verdikt
  jako referenční baseline (viz sekce 1 výhrada).
- **Krok 1** (oprava MĚŘENÍ, PŘED jakýmkoli blendem): předat
  `dirichlet_alpha=0.0` v `_benchmark_game`/`_gate_game` (`run_iteration.py:175,212`)
  — 1 řádek na callsite, žádný C++ rebuild. **Toto byl explicitní precondition
  starého AZ reportu, který se přeskočil.** Souvisí s, ale je nezávislé na,
  nálezu sekce 1 (ten řeší vf_blend/policy_weights v gatingu, tohle řeší
  exploration-noise v měření) — dává smysl opravit obojí najednou.
- **Krok 2** (nejlevnější smoke): `policy_blend` přes env override, žádná
  změna kódu nutná: `BB_IMITATION_EPOCHS=10 BB_POLICY_BLEND=0.15`. Volitelně
  nejdřív přidat malý ramp (~4 řádky, zrcadlí VF vzor). Sledovat `top1_agree`,
  draw-rate, benchmark, `mcts_H`. Smoke = 1 iterace na "nerozbije se", ověření
  = plný ~5h běh + gate.
- **Krok 3** (vf_blend retry, AŽ PO kroku 2): `BB_VF_BLEND=0.1` (ne 0.3 —
  hodnota co selhala), inversion monitor už existuje a auto-aktivuje se
  (`training_loop.py:409-428`). Abort při jediném inversion warningu nebo
  benchmark poklesu >5%.

Plný file:line index a git historie (`caa99da`, `54b9517`, `9f01091`,
`bb198b1`, `524a39f`, `d589c6d`) v transkriptu agenta, k dispozici na dotaz.

---

## 5. Weakness-probe: champion vs `learning` a vs starší frozen checkpoint

**Stav: HOTOVO (2026-07-09 ~15:06 UTC).** Doplnění bodu 2 z
`project_bloodbowl_weakness_probe.md` (2026-07-01 se testovalo jen vs
greedy/random). Zápasy reálně proběhly (`macro_mcts`, 100 MCTS it., vf_blend=0.0,
epsilon=0.0), max 4 workery kvůli běžícímu tréninku, n=48/matchup.

**Provozní poznámka:** Fable agent zadaný na tohle si opakovaně (5×) vymyslel
neexistující "monitor notification" mechanismus a nikdy sám nenahlásil
výsledek, přestože reálný skript (který sám správně napsal a spustil na
pozadí) běžel v pořádku. Agent byl zastaven (`TaskStop`), skript sám (nezávislý
OS proces) doběhl bez přerušení, výsledky níže čtu přímo z jeho log souboru.

**Matchup A — champion vs `learning` (bez searche, "stejné váhy"), n=48:**
- home win 18/48 = **38 %**, draws 30/48 = **62 %**, losses 0/48 = **0 %**
- home TD/g 0.71, conceded TD/g **0.02** (skoro nikdy neinkasuje)

**Matchup B — champion (current) vs FROZEN `champion_backup_91.5pct_20260629`
(taky macro_mcts, 100 it.), n=48:**
- home win 7/48 = **15 %**, draws 28/48 = **58 %**, losses 13/48 = **27 %**
- home TD/g 0.19, conceded TD/g 0.35

**Souhrn 4 datových bodů (weakness-probe matice):**
| soupeř | home win | draws | home loss | home TD/g | conceded TD/g |
|---|---|---|---|---|---|
| random (07-01) | 85 % | — | — | 1.58 | 0.00 |
| greedy (07-01) | 44 % | 37,5 % | 18,7 % | 0.90 | 0.46 |
| learning (dnes) | 38 % | 62 % | 0 % | 0.71 | 0.02 |
| starší frozen (dnes) | **15 %** | 58 % | **27 %** | 0.19 | 0.35 |

**Interpretace — POZOR na přeinterpretaci:** Matchup B ukazuje nápadnou
asymetrii — aktuální champion PROHRÁVÁ víc než VYHRÁVÁ proti svojí starší
91,5% verzi (15 % vs 27 %). Na první pohled to vypadá jako "trénink nic
nezlepšil, možná zhoršil". **Ale s výhradou přímo z dnešního úkolu 1:** oba
zápasy běžely na `vf_blend=0.0`, kde podle potvrzeného null-test nálezu
(sekce 1) natrénované váhy nikdy nevstupují do leaf evaluace — takže
mechanisticky by champion a starší frozen měly být z hlediska value funkce
NEROZLIŠITELNÉ, stejně jako v `diag_null_weights.py`. Navíc při n=48 je
stderr binomického podílu ~7,3 pp — pozorovaný rozdíl 15 % vs 27 % (12 pp) je
cca 1,6 stderr, tedy sugestivní, ale NE průkazný. Možné vysvětlení kromě
"skutečné regrese": šum při malém n, nebo vedlejší efekt dormantních
policy-prior floors (`d589c6d`, zmíněné v sekci 1/4 — ty se aktivují podle
PŘÍTOMNOSTI policy souboru, ne podle blendu, takže by teoreticky mohly
rozlišovat i při vf_blend=0). **Doporučení: neuzavírat, přeměřit na N=150
před jakýmkoli závěrem o regresi.** Matchup A (vs `learning`) je čistší
signál: 0 % proher je dobré, ale 62 % remíz + conceded 0.02 ukazuje týž vzorec
nízkoskórovacích patových zápasů jako mirror self-play — offense pořád
nekonvertuje ani proti neprohledávajícímu soupeři.

---

## 6. Celoprojektová syntéza root-cause

**Stav: HOTOVO (2026-07-09 ~15:30 UTC).** Tentokrát proběhlo přesně podle
zadání — jednorázově, blokujícím způsobem, bez žádného zaseknutí.

**Exekutivní shrnutí (Fable's own words):** "projekt tři měsíce trénoval
hlavy (value/policy), které se nikdy nedostaly do hry, a měřil je bránou,
která je nikdy nečetla." Dva strukturální disconnecty — (1) `vf_blend=0` +
strukturálně nedosažitelný `policy_blend` → veškerá produkce hraje čistě
hand-coded heuristikou, (2) gating/benchmark porovnává dva mechanisticky
identické agenty (null-test, sekce 1) — společně vysvětlují prakticky celou
historii "REJECTED" pák. Draw plošina samotná je vlastnost heuristického
agenta: remízy jsou loose-ball scramble, slabina je ofenzivní konverze, ne
obrana. Mechanistické enginové bugy (half-clock, PASS, negamax,
turnover-discard, hangy…) byly reálné a správně opravené, ale žádný z nich
plošinu neprolomil — jsou to opravy integrity, ne draw-páky.

**Kauzální vrstvy (4):**
- **Vrstva 0 — MĚŘENÍ** (root cause #1, "proč nic nikdy nehnulo gate číslem"):
  gating null-test + neopravený dirichlet noise 0.3 v benchmark/gatingu +
  nekvantifikovaná run-to-run variance (baseline stejné konfigurace se mezi
  běhy hnul 51,0%→62,6% draws — **to znamená, že typický efekt jednotlivých
  fixů ±2-5pp je POD šumovým dnem používané N=150 single-run metodiky**).
- **Vrstva 1 — DISCONNECT UČENÍ↔HRA** (root cause #2, "proč trénink nemohl
  ovlivnit hru"): `vf_blend=0` všude, `policy_blend` strukturálně
  nedosažitelný → každá reward/value/feature páka byla pro chování INERTNÍ
  BY CONSTRUCTION, jejich "REJECTED" verdikty jsou neplatné důkazy.
  Value-target flatness je reálná, ale kauzálně ODPOJENÁ dokud vf_blend=0 —
  je to latentní root cause BUDOUCÍHO pokusu, ne příčina dnešních remíz.
- **Vrstva 2 — MECHANICKÉ BUGY** (reálné, opravené, ale NE draw-páky):
  N=150 řetěz po 13 opravách: 49,0→47,6→49,7→42,6→44,8→45,5→45,2→46,6→~47,3→
  50,7% draws — **plošina se nehnula**. Hodnota oprav je integrita měření
  (watchdog skipy 2-5/150→0/150) a správnost visit-distribucí, ne draw-rate.
- **Vrstva 3 — HERNÍ/STRATEGICKÁ** (root cause #3, "proč heuristický agent
  reálně remizuje"): loose-ball scramble (43% turn-boundaries), pickup miss
  82%, ofenzivní nekonverze i vs `learning` (bez searche!) — 62% draws,
  0,71 TD/g, inkasováno jen 0,02. Search-budget vyčerpaná páka, všech 7
  leaf-eval/reward manipulací selhalo ve FUNKČNÍM kanálu.

**9 vnitřních kontradikcí explicitně vyflagováno** (výběr nejdůležitějších):
- Half-clock fix byl označen za "pravděpodobně hlavní driver remíz" — výsledek
  45,5% draws (uvnitř staré plošiny) **kauzální predikci nepotvrdil**, nikde
  to není napsáno na rovinu.
- **Nejvážnější metodické pnutí:** baseline variance ~11,6pp mezi běhy vs.
  jednotlivé fixy "potvrzované" rozdíly ±1-5pp — většina per-fix verdiktů je
  pod rozlišovací schopností použité metody (nezpochybňuje to bezpečnost
  fixů, jen draw-rate delty mezi nimi jako signál).
- Matchup B z úkolu 5 (15%W/27%L vs starší frozen) protiřečí null-testu —
  buď šum, nebo dormant prior floors (d589c6d) vytváří nekvantifikovaný
  kanál citlivosti i při blendu 0.
- Opus (pickup miss = dominuje kostka) vs. mining data (search aktivně
  downweightuje PICKUP) — nerozhodnutelné bez per-roll logu.

**Prioritizovaný seznam (1=nejvyšší leverage), REVIDUJE pořadí z úkolu 4:**
1. **Opravit CELÝ měřicí stack najednou, PŘED dalším tréninkem**: gate
   vf_blend fix (sekce 1) + dirichlet=0 fix (sekce 4) + **obrácený
   null-test jako verifikace** + **kvantifikovat run-to-run varianci**
   (2× identický baseline běh nebo párované seedy, ~2-4h investice).
2. **AZ bring-up, policy_blend první** — Fable POVYŠUJE tohle NAD další
   enginové audity: "bug-hunting má po 13 opravách bez pohybu plošiny
   prokazatelně klesající výnos."
3. **Tři izolované ofenzivní fixy** (pickup multi-candidate, stall-guard,
   screen) — ale AŽ PO re-miningu z čerstvých post-fix logů, staré mining
   číslo predatuje 5-8 oprav.
4. **Uzavřít matchup-B anomálii** (N=150 vs starší frozen) + levné enginové
   resty (`expandBlitzAndScore` guard, hygiena).
5. **Metodika: přejít z draw-rate na konverzní metriky** (TD/g,
   pickup-conversion, threat-conversion) jako primární signál — draw-rate se
   ukázala necitlivá a šumová.

**Nejdůležitější nezměřené:** (1) rozliší opravený gate skutečně natrénované
váhy? — obrácený null-test je "nejlevnější rozhodující experiment celého
projektu". (2) skutečné šumové dno metodiky — jediný datový bod naznačuje
±10pp, nikdy systematicky nezměřeno. (3) pomůže naučená hlava hře, když se do
ní konečně dostane? — "férový test nikdy neproběhl", platí dodnes. (7) **hra
proti člověku nikdy neproběhla** — deklarovaný cíl projektu nebyl nikdy přímo
měřen, jen proxy (greedy/random/learning/mirror), všechny se známými vadami.

## 7. Redesign reward/value-target shapingu

**Stav: HOTOVO (2026-07-09 ~15:45 UTC).** Taky proběhlo čistě jednorázově,
bez zaseknutí.

**Návrh: `mc_td_mix` — TD-bootstrap/MC mixovaný target.**
```
target(s_t) = clamp( α·G_t + (1−α)·(r_t + γ·V_θ(s_{t+1})), −1, 1 )   α≈0.7
target(s_T) = terminal_value(...)                                     (beze změny)
```
`G_t` = už existující Lever-B discounted return, `V_θ(s_{t+1})` = bootstrap
current hlavou. **Žádné nové reward termy, žádný hand-crafted potenciál** —
čistá změna estimátoru na stejnou veličinu (V^π), tím pádem policy-invariantní
BY CONSTRUCTION (silnější garance než PBRS) a imunní vůči reward-hacking
selháním, kterými si projekt už prošel. Bootstrap term je přesně ten
mechanismus, co převede zdravé cross-game vědění hlavy (spread 0.33–0.58,
korelované s carrier-proximity) na within-game gradient — řeší zdokumentovaný
"paradox" (hlava je zdravá napříč hrami, ale ploché cíle uvnitř jedné hry).

**Proč NE dokončit opuštěný 06-30 pokus (level-shaping):** `Φ = 0.3 +
0.7·proximity` platí 0.3 za pouhé DRŽENÍ míče, bezpodmínečně navždy — jakmile
by se hlava dostala do leaf-evalu (celý smysl opravy), stalo by se to pobídkou
"drž a neriskuj" = přesně ta 0-0 patologie, jen z jiného zdroje. Žádná
invariance garance. **Explicitně odmítnuto** jako řešení, doporučeno zachovat
v git historii (commit-push-revert) podle existujícího rozhodnutí uživatele.

**Proč je bezpečný vůči známé historii selhání:**
- vs. VF inverze (2026-05-20): mění jen CÍL odhadu (stejné V^π, jen nižší
  variance), Stage 1 běží celá při vf_blend=0 kde inverze nemůže projevit v
  hraní.
- vs. `mc_return_shaped` 89→80 selhání: to bylo PBRS-diff na MC targetu, které
  teleskopuje na V−Φ (behavior distortion bez re-přidání Φ v C++). `mc_td_mix`
  nemá žádné Φ, celá tahle třída selhání je uzavřená.
- vs. entropy-floor nález: úspěch se NEBUDE soudit podle MCTS visit entropy
  (známý artefakt), ale podle offline value-ramp/kalibrace metrik.

**Implementace:** ~80 řádků napříč 4 Python soubory (trainer.py,
replay_buffer.py, training_loop.py, train_cli.py), **žádný C++ zásah, žádný
rebuild**. Nová metoda `mc_td_mix` (α jako env/flag), `α=1.0` = přesná
redukce na existující `mc_return` (built-in null test pro A/B).

**Rizika a mitigace:** (1) bootstrap divergence — mitigováno α=0.7 (70 %
kotva k reálnému výsledku), clamp, tanh, malé lr; monitorovat
`mean_abs_vf`/`grad_norm`/`w_norm_Δ` (už se logují). (2) garbage propagation
(chicken-egg) — MC kotva + fallback na α=1.0 při špatném znaménku rampy.
(3) **falešně poplašný inversion monitor** — u `terminal_value` jsou remízy
záporné pro OBĚ strany záměrně (non-zero-sum), takže "obě strany záporné" je
OČEKÁVANÉ, ne inverze; jen "obě strany kladné" zůstává platný detektor —
důležitá poznámka pro čtení monitoru při validaci. (4) interakce se
scoringBonus ve Stage 2 — mírné zdvojení scoring-pull, ohraničeno nízkým
blendem (0.15, ne 0.3) a leaf clampem.

**Validační plán (2 fáze, odděleně, per konvence jedna změna najednou):**
- **Stage 0** (minuty): offline sanity na syntetickém 4-stavovém logu, α=1.0
  musí bit-přesně reprodukovat starý `mc_return`.
- **Stage 1** (~4-5h, vf_blend=0.0 → NULOVÉ behaviorální riziko, gating
  null-test to garantuje): jen změna targetu, sledovat pre-TD value ramp
  (nová primární metrika) + kalibraci + stabilitu. Win/draw čísla očekávaně
  BEZE ZMĚNY (hlava se v hraní nepoužívá) — běh existuje jen pro natrénování
  hlavy + telemetrii.
- **Stage 2** (samostatná změna, AŽ PO úspěšném Stage 1): `vf_blend=0.15`,
  plný běh + standardní N=150 gate. Toto je první bod, kde celé cvičení může
  vůbec něco změnit v chování — a první bod, kde gating vůbec začne měřit
  natrénované váhy (opravuje null-test slepotu jako vedlejší efekt).

**Bookkeeping:** commit+push PŘED každou fází (server pull), běhy odpojené
(setsid+nohup+disown), PID/log/prahy zapsat do paměti při spuštění.

---

## Shrnutí pro zítřek (všech 7 úkolů hotových, poslední ~15:45 UTC)

1. **Gating null-test STÁLE PLATÍ** — dnešní PROMOTE/REJECT nebude o vahách nic
   říkat. Fix navržen (2 řádky, `run_iteration.py:53,452`).
2. **TTM scatter = false alarm**, ale nalezen nový drobný ohraničený bug
   (`expandBlitzAndScore`, `macro_actions.cpp:965`) — 1-řádková oprava.
3. **Stall-guard i screen=0 díra obě STÁLE PLATÍ**, konkrétní fixy navrženy.
   Pickup priority (multi-candidate) taky stále otevřené.
4. **AlphaZero bring-up:** ANO (kvalifikovaně) pro policy_blend, později pro
   vf_blend. Dirichlet noise v gatingu (samostatný bug od #1) taky neopravený.
5. **Weakness-probe:** champion vs `learning` 38%W/62%D/0%L; champion vs
   starší 91.5% frozen 15%W/**27%L** — nápadná asymetrie, ale u n=48 jen
   ~1.6 stderr, přeměřit na N=150 než se z toho dělá závěr.
6. **CELOPROJEKTOVÁ SYNTÉZA** ([[project_bloodbowl_fable_synthesis_20260709]]
   v paměti) — 2 strukturální disconnecty (gating null-test + vf_blend=0)
   vysvětlují skoro celou historii REJECTED pák; ~11.6pp baseline variance
   znamená, že většina starých per-fix draw-rate verdiktů byla pod šumem.
   Nový přeprioritizovaný roadmap — AZ bring-up POVÝŠEN nad další enginové
   audity.
7. **Value-target redesign návrh (`mc_td_mix`)** — TD-bootstrap/MC mix,
   žádný C++ zásah, 2fázový validační plán (Stage 1 @ vf_blend=0 = nulové
   riziko, Stage 2 = teprve reálný test).

**DOPORUČENÉ POŘADÍ PRO PŘÍŠTÍ SESSION (revidováno syntézou v bodě 6,
NAHRAZUJE dřívější pořadí 1-5 z tohoto souboru):**
1. Opravit CELÝ měřicí stack najednou (gate vf_blend fix + dirichlet fix +
   obrácený null-test jako verifikace + kvantifikovat run-to-run varianci)
   — precondition všeho ostatního.
2. AlphaZero bring-up, `policy_blend` první (Stage 1 z bodu 7 sem zapadá —
   value-target redesign PŘI vf_blend=0 je nulové riziko, dá se dělat souběžně).
3. Tři ofenzivní fixy (pickup multi-candidate, stall-guard, screen) — AŽ PO
   re-miningu z čerstvých post-fix logů.
4. Uzavřít matchup-B anomálii (N=150) + levné enginové resty
   (`expandBlitzAndScore` guard).
5. Metodika: draw-rate → konverzní metriky jako primární signál.

Vše navržené (žádný kód neimplementován, žádný fix nasazen) — čeká na
rozhodnutí a implementaci v příští session.
