# ZADÁNÍ pro Fable agenta: AlphaZero metodika vs. naše pipeline (06.08.2026)

## AKTUALIZACE 05.08. (dispatch předsunut na dnešní odpoledne, pokyn uživatele)

- **Report post-promotion dynamiky UŽ EXISTUJE: evidence/fable_postpromotion_report_20260805.md — POVINNÉ čtení.** Klíčové: skok promoce 03.08. vznikl v asymetrickém gate (kandidát policy 0,2 vs frozen 0,0) = jednorázový přínos zapnutí policy, ne přírůstek učení; per-epoch policy metriky ploché napříč 5 iteracemi; stash ztráta není systemická. Otázku 5 formuluj S TÍMTO nálezem.
- **A3-2 kotevní měření běží DNES V NOCI** (výsledky až po tvém reportu): kotva jul28 == před-promoční šampion == md5 b426c64d (TENTÝŽ soubor — weights_best se 22.07.→03.08. neměnil, vše REJECTED). Harness diag_a3_2_anchor_20260805.py: 6 ramen (control/noreset1-4/champion s policy 0,2) × 300 side-swapped párů vs kotva, pre-reg v docstringu. U doporučení ke kotvám/lize proto řekni, KTERÁ tvá doporučení zítřejší čísla potvrdí/vyvrátí a jak je číst.
- Stav fronty se od sepsání posunul: item13 wiring SHIPPED (3d6e456, env BB_STAGED_PICKUP, trénink čeká na GO); F1 cage advance A/B smíšený (adoption 0,34–0,66/g; jen dwarf-skaven +6,75pp prošel pre-reg; default zůstává OFF, adoption brzdí TEMPO_INSUFFICIENT 68 % + DICEY koridor).

**SROVNÁVACÍ BOD (ujasněno uživatelem): DeepMindí AlphaZero 2017 — to, co
vyhrává šachy proti všem a trénovalo čistě hraním obrovského množství her
samo proti sobě.** Ostatní systémy (AlphaGo Zero, AlphaStar liga…) uváděj
jen jako kontext tam, kde vysvětlují, PROČ AlphaZero něco dělá jinak než
my. Otázka zní: „ono to funguje — v čem přesně se náš postup liší a co
z jejich receptu nám chybí nebo u nás nemůže fungovat kvůli měřítku."
POVINNÁ část odpovědi: poctivé srovnání MĚŘÍTKA (AlphaZero: ~44M partií
šachu, obří sítě a compute; my: ~640 her/iteraci, malé sítě, MCTS-100) —
u každého doporučení řekni, jestli přenositelnost na naše měřítko stojí,
padá, nebo se musí přizpůsobit.

## Otázka uživatele (04.08. večer, doslova témata):

1. **Jak se AlphaZero učí?** (signál, cíl, co přesně se optimalizuje)
2. **Jak pozná, že postupuje?** (metriky pokroku — Elo, evaluace, kotvy)
3. **Jak si vybírá protivníky?** (self-play režimy, pooly, checkpointy)
4. **Co z toho nám chybí?** (gap analýza proti naší pipeline)
5. **Jak řeší problém „proti nedávnému sobě mám jen malý pokrok"?**
   (přesně naše situace: po promoci 03.08. je iterace #2 na 50,3 % vs nový
   šampion — [[hypotéza uživatele: návrat k šumu po každém skoku]])

## Formát práce

Nejdřív LITERATURA (ze znalostí; WebSearch jen na doplnění konkrét):
AlphaGo Zero vs AlphaZero rozdíly (POZOR na klíčový fakt: AlphaGo Zero MĚLA
evaluator/gating 55% práh proti šampionovi — jako my; AlphaZero ho ZRUŠILA
a hraje vždy nejnovější sítí — proč to funguje a co to vyžaduje), Elo
měření přes pool checkpointů, AlphaStar league training (main agents +
exploiters + past checkpoints — odpověď na opponent diversity), OpenAI Five
past-self pool, population-based training. Ke každému: JAK měří pokrok
nezávisle na „50 % proti sobě".

Pak MAPPING na naši pipeline (čti kód run_iteration.py + paměť přes
evidence/ a gate_history):
- my = AlphaGo Zero vzor (gating proti šampionovi, reset-on-reject) + naše
  odchylky (policy carry-over přes rejecty — akumulace potvrzena 01.-03.08.;
  vf_blend heuristika+síť; MCTS-100 rozpočet; benchmark vs random na stropu).
- co už MÁME rozpracované a kam to zapadá: A3-1 league patch (worktree
  agent-ae897a8fe179d2dc3, NEMERGOVANÝ — frozen-anchor league, jul28 kotva,
  rozhodnutí uživatele 03.08. platí!), A3-2 přeměření archivů vs kotvy,
  policy_backups/ + gate_history.policy_stash_md5 (retro policy-vs-policy
  měření), metrics_archive/ learning křivky, weights_noreset_iter1-4 archivy.

## Deliverable

`evidence/fable_alphazero_methodology_report_20260806.md`, stručně:
1. Odpovědi na 5 otázek (každá ≤1 odstavec jádra + odkaz na zdroj vzoru).
2. **Gap tabulka**: co kanonické systémy dělají vs. co děláme my vs. co
   z toho už čeká ve frontě (A3-1/A3-2/kotvy) vs. co chybí úplně.
3. **Doporučení seřazená proti EXISTUJÍCÍ frontě** (nezakládej novou
   paralelní frontu; řekni, co povýšit/doplnit/zrušit a proč) — zvlášť:
   má smysl náš gating (AlphaGo Zero vzor) vs. přechod na AlphaZero režim
   bez gate vs. league (AlphaStar vzor)? Za jakých podmínek který?
4. Návaznost na včerejší report post-promotion dynamiky (05.08.) — čti ho.

## Terminologie (závazná v celém reportu)

„**naše AI**" = náš Blood Bowl systém (i když ho kód/commity historicky
nazývají „AlphaZero" — to je interní identifikátor, v textu ho nepoužívej).
„**AlphaZero**" = výhradně DeepMind 2017. Žádné míchání.

## Omezení

Žádné běhy/měření (čistě literatura + kód + logy), žádné změny kódu,
žádný git push. Rozpočet ≤120k tokenů. Závěry formuluj jako podklad
k rozhodnutí uživatele, ne jako hotová rozhodnutí.
