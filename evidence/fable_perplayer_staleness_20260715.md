# Staleness audit: per-player brief/Opus results vs HEAD (2026-07-15)

Delta memo k `team1_brief_per_player.md` (06-09) a `team1_results_opus.md` (06-10)
proti HEAD a9dcaaf (post c7b1ed6 + side-swap d90227b). Jen změny; co tu není,
platí beze změny. Autor: Fable 5.

## 1. 70 vs 73 featur — SKUTEČNÁ ZMĚNA KÓDU, ne chyba v dnešních reportech

Commit **8658768 (2026-06-25)**, „fix #3: +3 loose-ball field-position features
(70-72)": `feature_extractor.h:8` i `features.py:9` dnes `NUM_FEATURES = 73`.
Nové featury: f70 `loose_dist_to_td`, f71 `my_nearest_to_ball`, f72
`pickup_clear`. **73 je správné číslo v HEAD; „70" v obou červnových docích je
STALE** (byly napsané 15 dní před změnou). PHP extractor dojel paritu až
30539d6 (07-13). Kosmetika: stale komentář `policy_trainer.py:19` „70 + 23 = 93"
(skutečnost 73+23=96; kód počítá dynamicky, jen komentář lže). Důsledek pro
Fázi A: baseline fit je **G ~ f73**, a kandidátní loose-ball featury z briefu se
částečně překrývají s f70-72 — baseline fit to zaúčtuje automaticky, jen
nečekat plný ΔR² z loose-ball kandidátů.

## 2. `_align_features` / warm start — mechanismus beze změny, čísla řádků STALE

Pad-zeros/truncate logika beze změny; posunula se na `trainer.py:416`
(LinearTrainer) + **nová kopie v NeuralTrainer na :896**; `load_weights`
zero-pad na :433-441 (Opus psal :236 a ř. 532-541). Warm-start claim
(inkrementální 73→~150 zachová váhy, nové init 0) **platí a je už produkčně
prověřený** — přesně tudy proběhla expanze 70→73 v 8658768. Opusova „past"
(tiché zarovnání staré sítě místo failu) platí dál a nově na DVOU místech.

## 3. Value-target předpoklady — Opusův bod 1 pořadí je MRTVÝ, bod 2 posílen

Opusovo Konsolidované doporučení řadilo **před** featury: TD(λ=0.9) + draw
penalty + osekat stallIncentive. STALE jako sekvence: target-shaping větev byla
mezitím dvakrát testována a REFUTED (mc_td_mix Stage 1; drive-target
`evidence/fable_drive_target_prefilter_20260715.md`) — 73-dim lineární hlava
within-episode strukturu nevyjádří ani s bohatšími labely. To **posiluje**
kanál (a) přesně dle roadmapy 07-14 §4; plán featur samotný se nemění. Dílčí
stav: default target pořád `mc_shaped` (`run_iteration.py:94`), stallIncentive
NEosekán (`feature_extractor.cpp:236-240` beze změny — otevřená položka mimo
tento plán; pasivitu mezitím řešil stall-guard a88f5e2 jinou cestou).

## 4. vf_blend gate — NOVÁ podmínka, v červnových docích NEEXISTUJE

Ani brief ani Opus doc slovo vf_blend neobsahují (předchází draw-collapse
nálezu 06-24 i null-testu 07-02). Podmínka Fáze B „(ii) vf_blend bring-up
prokázal, že V dosahuje do hry" (roadmap §4.3) je přidaná POZDĚJI a Opusovo
pořadí kroků (inkrementál → plný 492) s ní nepočítá. **Akce: do
`team1_results_opus.md` u Konsolidovaného doporučení bodu 2 přidat jednu
poznámku** (mezi pozitivní Fázi A a C++ Fázi B stojí vf_blend gate — jinak se
zlepšuje hlava, kterou hra nečte). Žádný rewrite.

## 5. C++↔Python parita — bez driftu; Fáze A ji navíc obchází

Obě implementace změněny lockstep v jediném commitu 8658768 (jediná změna obou
souborů od 06-10); drift měl jen PHP (spraveno 30539d6). Opusův paritní gate
platí dál, ale **až od Fáze B**: Fáze A dle roadmapy §4.2 je offline
(snapshot-persist v `cpp_runner.py` + nový offline skript), produkční
`features.py` ani C++ se nedotýká → červnový předpoklad „obě strany měnit
spolu" se na Fázi A nevztahuje. Konzistentní, jen to v červnových docích není
řečeno explicitně.

## 6. Engine změny od 06-10 vs mechaniky briefu — cílové cesty NETKNUTÉ, +1 nové call-site

`pathfinder.cpp`: **nula commitů** — `PathNode::dodged` pořád mrtvý kód (:11),
`carrierBlitzable` pořád čistý Chebyshev (`feature_extractor.cpp:359-364`, f63)
→ Opusův nález #3 platí beze změny, zůstává Fáze-B TODO. Fixy od června šly do
`game_simulator.cpp` (halfclock, kicking-team, kickoff-clock, H2 směr a79d164),
`mcts.cpp` (negamax/backprop/FPU/CAGE), `macro_actions.cpp` (expandScore hang,
PICKUP cap, stall guard, intercept lane), `action_resolver.cpp` (BLITZ hang) —
žádný nesahá na BFS reachability / net_st / TZ cesty, kolem kterých plán staví.
Dvě nové skutečnosti: (1) stall guard **znovupoužívá Chebyshev blitzable
aproximaci** (`macro_actions.cpp:827-838`, komentář „same approximation as
feature f63") → upgrade na BFS+TZ má nově DVĚ call-sites, ne jednu; (2)
H2-kickoff/side-swap fixy mění orientační sémantiku → Fáze A mining POUZE
z post-fix logů (roadmap to už požaduje).

## Verdikt

Červnová dvojice docích je **pořád zdravý základ pro Fázi A** — žádný nález
neinvaliduje architekturu ani feature-katalog. Stačí tento delta memo + jedna
poznámka o vf_blend gate do Opus docu. Klíčové korekce: baseline je 73 (ne 70),
sekvence „nejdřív value-target fixy" je mrtvá (REFUTED ×2, featury povýšeny),
blitzable-upgrade má 2 call-sites.
