# Oracle judge: kvalita divergentních rozhodnutí kandidát vs šampion (A2-3, 30.07)

STAV: HOTOVO — 96/96 divergencí ohodnoceno (0 přeskočeno), K=48 rolloutů/větev.
Data: `diag_oracle_judge_20260730_results.json` (raw per-event),
`diag_oracle_judge_20260730_report.json` (agregace), nástroj
`diag_oracle_judge_20260730.py`, log `diag_oracle_judge_20260730.log`.

Navazuje na `evidence/fable_decision_churn_20260730.md` (churn ~26 %/rozhodnutí,
změny laterální). Otázka: jsou divergentní volby kandidátů LEPŠÍ, HORŠÍ nebo
EKVIVALENTNÍ? Zvlášť třída `safer_same_scoring`. Sekundárně R4: je REJECTED
verdikt item1 konzistentní s kvalitou jeho divergencí?

## Proveditelnost rekonstrukce stavu (zjištění)

- Bindingy (`engine/python/bb_module.cpp`) NEMAJÍ „spusť macro-MCTS z libovolného
  stavu" — AI běží jen přes `simulate_game*` od výkopu. Oracle „MCTS400 na
  rekonstruovaném stavu" tedy bez rebuildu enginu NEJDE (rebuild zakázán).
- Rekonstrukce GameState ALE jde: `GameState()` + `setup_half(gs, roster, roster,
  kicking)` naplní 22 hráčů (ID 1–11 home, 12–22 away, pořadí z rosteru vč.
  skillů — stejné mapování jako v původních hrách), pak přepis
  position/state/movement_remaining per hráč (`get_player` vrací zapisovatelnou
  referenci, `reference_internal`), míč přes `BallState`, `active_team`,
  `phase=PLAY`, half/turn_number/score ručně; hráči mimo snapshot → OFF_PITCH
  (snapshot obsahuje jen hráče na hřišti; stavy 0/1/2 = STANDING/PRONE/STUNNED).
- Herní smyčku řídí `execute_action` (přepíná tahy, fáze; skóre se inkrementuje
  při TD v action_resolver.cpp) — Python driver ji replikuje.

## Metodika oracle (párové rollouty z pozice PO každé volbě)

Divergence na indexu ci: stav ci je identický v obou větvích (common random
numbers), stav ci+1 je důsledek šampionovy volby (REF hra) resp. kandidátovy
volby (CH/CA hra) — board snapshoty obou post-stavů jsou v uložených hrách
(`diag_decision_churn_data/`). Makra nejsou přes bindingy spustitelná, proto se
hodnotí právě tyto zaznamenané post-stavy.

Pro každou divergenci a každou větev:
1. rekonstrukce post-stavu ci+1; imposed clock half=2, aktivní strana turn=5
   (~4 tahy/strana horizont), skóre 0:0, fresh-turn reset — STEJNĚ pro obě
   větve (párový rozdíl sdílené neznámé neutralizuje);
2. K=48 párových rolloutů se STEJNÝMI dice seedy v obou větvích (CRN);
   rollout politika = přesná Python replika `greedyPolicy` (policies.cpp) pro
   obě strany obou větví — fixní, na šampionovi NEZÁVISLÝ playout (pilot s
   VF-greedy politikou v horizontu nikdy neskóroval → z bez signálu; greedy
   skóruje); rollout končí prvním TD / GAME_OVER / cap 700 kroků;
3. metriky per rollout: z = skóre-diff z pohledu kandidátovy strany
   (+1/0/−1, „kdo skóruje první v horizontu"), terminální VF (šampionova síť,
   symetricky obě větve), turnovery vlastní strany (validace „safer");
4. verdikt per divergence: Δz = mean(z_cand) − mean(z_champ); LEPŠÍ/HORŠÍ jen
   při |Δz| > práh A/A A zároveň |t| > 2 (párový t-test přes CRN rollouty),
   jinak EKVIVALENTNÍ.

A/A kalibrace: na 16 eventech 2 nezávislé sady rollout seedů na TÉMŽE
(šampionově) stavu → |Δz_AA| mean 0,081, max 0,188 → práh = 0,188.

Limity (přiznané): (a) jeden vzorek kostek provedení divergentního makra —
verdikt je „kvalita realizovaného důsledku", ne čistá kvalita volby; (b)
fresh-turn aproximace (neznámé has_acted/rerolly/skóre — imposed shodně);
(c) greedy playout je slabý hráč — měří „kdo skóruje první při naivní hře",
jemné poziční rozdíly zachytí jen částečně; (d) weather=NICE.

## Výsledky

### Verdikt per pár (96 divergencí, práh 0,188)

| pár | n | kandidát LEPŠÍ | EKVIVALENTNÍ | kandidát HORŠÍ | mean Δz ± SE | mean ΔTO | mean ΔVF |
|---|---|---|---|---|---|---|---|
| pair1_e2 (item2 smoke) | 48 | 5 | 39 | 4 | +0,010 ± 0,040 | +0,15 | +0,014 |
| pair2_e16 (item1 sourozenec) | 48 | 6 | 35 | 7 | +0,007 ± 0,042 | +0,19 | −0,002 |
| pooled | 96 | 11 | 74 | 11 | +0,008 ± 0,029 | | |

Bilance LEPŠÍ:HORŠÍ je 11:11 — dokonale vyrovnaná. Agregátní Δz je nula
s přesností ±0,03. Divergence jsou v úhrnu EKVIVALENTNÍ — potvrzuje
„laterální změny" z churn analýzy, tentokrát na úrovni realizované kvality,
ne jen výsledků her.

### Per třída (klasifikace churn nástroje vs oracle verdikt, L/E/H)

| třída | pair1 | pair2 |
|---|---|---|
| equal_features | 2/9/2 | 1/9/1 |
| sub_feature | 1/17/0 | 1/15/1 |
| **safer_same_scoring** | 2/9/2 | 1/6/5 |
| riskier_no_gain | 0/4/0 | 3/4/0 |
| more_scoring | — | 0/1/0 |

### Třída safer_same_scoring — bezpečnost NENÍ reálná

Pooled n=25: verdikt 3 LEPŠÍ / 15 EKVIVALENTNÍ / 7 HORŠÍ;
mean Δz = −0,093 ± 0,089 (t = −1,04, nesignifikantní, ale směr záporný);
mean Δturnoverů vlastní strany = **+0,58/rollout** (14 eventů s VÍCE
turnovery kandidáta vs 9 s méně); pair2 detail: Δz −0,16, ΔVF −0,11.

Deklarované featury (nižší `risk` při stejném `scoring`) se NEPROMÍTAJÍ do
realizované bezpečnosti — kandidátovy „bezpečnější" větve mají v rolloutech
naopak víc vlastních turnoverů a (u pair2) horší next-score. Vzorec u pair2:
3× BLITZ→CAGE a 1× BLITZ→ADVANCE mezi HORŠÍMI — kandidát vzdává tempo
(blitz) za pasivní krytí, což mu v horizontu ~4 tahů škodí. Naopak pair2
`riskier_no_gain` skončila 3/4/0 ve prospěch kandidáta. Klasifikace podle
akčních featur tedy kvalitu volby nepredikuje — hodnotit je nutné důsledky,
ne deklarované featury (potvrzení metodiky „situace, ne agregáty").

## Závěr pro R4 (item1 re-examinace)

**REJECTED verdikt item1 je konzistentní s kvalitou jeho divergencí; falešná
negativa se nepotvrzují.** Konkrétně pro pair2_e16 (sourozenec item1
kandidáta): (1) bilance 6:7 LEPŠÍ:HORŠÍ, mean Δz ≈ 0; (2) jediný nadějný
kvalitativní signál z churn analýzy — převaha „safer_same_scoring" — se při
hlubším ohodnocení obrací: tato třída je u pair2 1:5 LEPŠÍ:HORŠÍ s reálně
VYŠŠÍ mírou turnoverů. Hypotéza „gate zahazuje malé reálné zisky" (memory
29.07) pro item1 nemá oporu: změny nejsou malé zisky, jsou to laterální
záměny s vyrovnanou bilancí a mírně negativním „safer" chvostem. Zvýšený
benchmark (91,5→99,5 % vs random) při HtH 48 % tak zůstává vysvětlen jako
overfit na slabého soupeře, ne jako neodměněné zlepšení.

Pozn. pro případný akumulační/no-reset experiment (R5): tento oracle
(rekonstrukce + párové greedy rollouty) je znovupoužitelný per-iterační
nástroj; práh šumu z A/A kalibrace je ~0,19 na Δz při K=48.
