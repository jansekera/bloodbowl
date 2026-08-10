# A2-2: Táhne únorový replay buffer value drift červencových kandidátů? (30.07.2026, Fable)

Hypotéza: konzistentní směr value driftu 10 červencových e16 kandidátů (crossera A.3,
medián párového cos 0,89) = směr klesání value loss na zastaralých únorových datech
z replay_buffer.pkl (pipeline audit N3).

Skript/data: `diag_drift_alignment_20260730.py` → `diag_drift_alignment_20260730.json`.
Čistě výpočetní, žádný trénink, produkční soubory nedotčeny.

## Verdikt: **VYVRÁCENO**

Drift NEmíří do únorové distribuce. Míří výrazně víc do směru klesání na **čerstvých**
datech: cos(drift, −∇únor) = **0,17** vs cos(drift, −∇čerstvé) = **0,64**
(šumové dno v 4801-dim prostoru: σ ≈ 0,014). Oba gradienty jsou navzájem téměř
ortogonální (cos = 0,083) — únorová a současná éra chtějí od value sítě jiné věci,
a trénink jde za tou současnou.

## Metodika

1. **Drift vektor**: value část (value_W1 73×64, b1, W2 64×1, b2 → 4801 dim, pořadí
   flatten W1,b1,W2,b2) 10 červencových e16 snapshotů (mtime 08.–28.07., stejná
   množina jako crossera A.3) minus šampion `weights_best.json`; průměr = D,
   ‖D‖ = 0,296 (‖šampion‖ = 8,88, tj. ~3,3 % normy — sedí s A.3 „~4 %").
2. **Rozdělení bufferu**: živý `replay_buffer.pkl` (6286 transakcí) rozdělen na
   únor = 5000 × 70 featur, `mc_return=None` (winner-only reward ±1/0) a
   čerstvé = 1286 × 73 featur s mc_return (nová reward sémantika: hodnoty jako
   −0,88/−0,5/−0,35). Rozdělení 70 vs 73 je exaktní a kryje se 1:1 s mc_return
   (assert v skriptu). Padding 70→73 nulami replikován dle `trainer.py:416-425`.
3. **Gradient**: přesná replika produkční replay cesty pro `mc_shaped`
   (potvrzeno v logu `smoke_item2_mcts400_20260729.log` „Method: mc_shaped";
   default `run_iteration.py:114`): target = r + γ·Φ(s′) − Φ(s), terminál r − Φ(s)
   (`training_loop.py:819-821` → `trainer.py:620-645`), Φ = DEFAULT_SHAPING_WEIGHTS,
   backprop loss 0,5·(target − tanh(…))² dle `trainer.py:515-541`. Průměrný gradient
   1. řádu na šampionových vahách, plný subset (5000 resp. 1286 vzorků).
4. **Zarovnání**: kosinus D vs −gradient obou subsetů, per-kandidát detail, projekce.

## Čísla

| metrika | únor | čerstvé |
|---|---|---|
| cos(mean drift, −∇) | **0,174** | **0,637** |
| % normy driftu vysvětlené projekcí | 17,4 % | 63,7 % |
| ‖mean ∇‖ / vzorek | 0,624 | 0,147 |
| koeficient ve společném least-squares fitu | 0,058 | 1,261 |

- Společný 2-směrový fit vysvětlí 42,0 % variance driftu; prakticky celý příspěvek
  nese čerstvý směr (samotný čerstvý: cos² = 40,6 %; únor přidává ~1,4 p.b.).
- Per-kandidát: 9/10 kandidátů má cos k čerstvému směru 0,51–0,76 a k únorovému
  jen 0,15–0,20. Outlier `e16_93pct_+1.6` (13.07.) je proti-směrný k oběma
  (−0,31 / −0,06) — tentýž kandidát je i jediný proti-směrný k mean driftu
  (cos −0,32; ostatních 9 má 0,93–0,98, což reprodukuje A.3).
- Únorový per-vzorek gradient je ~4× SILNĚJŠÍ než čerstvý (0,62 vs 0,15 —
  winner-only ±1 targety jsou od dnešních predikcí dál), přesto se drift jeho
  směrem nevydal: 64 replay vzorků/epochu je přehlušeno tisíci full-log updaty
  z čerstvých her (konzistentně s odhadem dopadu ~2–3 % v N3).
- Kontrola nezávislá na gradientech: řádky W1 pro featury 70–72 (únorová data na ně
  mají po zero-paddingu gradient PŘESNĚ 0) nesou ‖drift‖ = 0,083 z celkových 0,296
  (~7,8 % kvadrátu normy) — i tahle část driftu existuje, tedy z čerstvých dat.

## Limity

- „Čerstvý" subset (1286 transakcí) pochází z item2 smoke běhu 29.07. — je to proxy
  celé červencové éry, ne přesná data jednotlivých kandidátských běhů. Vzhledem
  k velikosti rozdílu (0,17 vs 0,64) to závěr neohrožuje.
- Gradient 1. řádu v šampionovi ≠ celá 16-epochová trajektorie; test je směrový.
- cos(drift, −∇únor) = 0,17 je ~12σ nad šumem — únorová data drift MÍRNĚ
  přikrmují stejným směrem, ale vysvětlí jen ~17 % normy vs ~64 % čerstvými.

## Co to znamená pro post-fix běhy

1. **Fix 3df1a6d (untrack pkl) je hygiena, ne game-changer pro drift.** Po vyčištění
   živého pkl od 5000 únorových transakcí se směr driftu prakticky nezmění —
   nebyl to únor, kdo ho určoval. Neočekávat skokovou změnu chování kandidátů.
2. **Posiluje hypotézu „zahazujeme malé reálné zisky" (29.07.).** Konzistentní drift
   je autentický směr klesání na SOUČASNÝCH datech — trénink opakovaně táhne
   kandidáty k optimu dnešní distribuce a gate je opakovaně resetuje. Není to
   artefakt časové kapsle. (Pozor: „klesání na self-play datech" stále může být
   self-play bias, ne nutně herní zlepšení — to rozhoduje jen HtH/turnaj, viz
   crossera část B.)
3. Živý pkl pořád obsahuje 5000 únorových transakcí — před dalším dlouhým během
   je vyčistit (nebo smazat pkl a nechat buffer nabíhat čerstvý) zůstává správný
   krok dle N3, jen s vědomím, že jde o odstranění ~17% kontaminace směru, ne
   hlavní příčiny čehokoli.
