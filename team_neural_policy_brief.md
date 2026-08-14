# Brief pro tým: Má smysl ZNOVU zapnout neural policy? (historie zapnutí/vypnutí + aktuální kód)

**Datum:** 2026-06-17
**Stav:** podklad k vyjádření týmu — kombinuje git-historii (policy už BYLA zapnutá a opuštěná) s aktuálním stavem kódu
**Kontext:** [[project-bloodbowl]], navazuje na `team_alphazero_results.md` a `team_alphazero_brief.md`

---

## Centrální otázka pro tým

Plán AZ bring-up došel ke kroku, kde lineární imitační policy plató (top1≈38 %, loss≈2.24)
a zvažujeme přechod na **neural policy**. PŘED tím chce uživatel vědět:

**Neural policy už byla v minulosti zapnutá a opuštěná. Byl to prokazatelný neúspěch
(slepá ulička), nebo jen pivot za jiných podmínek? A je důvod čekat, že DNES to dopadne
jinak — zejména proto, že dnes máme mnohem výkonnější model než tehdy?**

Tým má posoudit NEJEN aktuální kód, ALE I tu historii zapnutí/vypnutí, a dát doporučení:
(A) jít na neural policy, (B) nejít, (C) jít, ale jinak/za jiných podmínek.

---

## ČÁST 1 — Historie: neural policy BYLA zapnutá (únor–březen 2026)

Framing `team_alphazero_results.md` („infra postavená, nikdy nezapnutá, jen přepni 2 vypínače")
je **historicky nepřesný**. Git ukazuje plnou policy éru:

### Časová osa (vše doložené commity)
| Datum | Commit | Co se stalo |
|-------|--------|-------------|
| 20.2. | `4364fdc` | **86.7 % benchmark** dosaženo (linear value + heuristiky), Human vs Orc |
| 20.2.–9.3. | (notebook) | **Policy-only éra:** `--policy-lr=0.01 --lr=0.0 --model=linear` → value ZMRAŽENÁ + LINEÁRNÍ, trénuje se JEN policy. Denně „Update weights from Colab training". |
| 7.3. | `e0314a5` | **„Restore weights_best.json from 86.7% benchmark"** → rollback na 86.7 % checkpoint (`1c6db16`, 6.3.). Indikace, že policy trénink REGREDOVAL pod 86.7 %. |
| 8.3. | `e2afa6f` | přidán neural policy network |
| 9.3. | `cbf10d3` | „neural policy training" + `POLICY_MODEL='neural'` v Cell 1 |
| 9.3. | `66d4e92` | policy-heuristic blending priorů v MCTS |
| 9.3. | `c636fbf` | imitation learning: konfigurovatelný policyBlend + imitation epochs |
| 9.3. | `c59369f` | mini-batch policy training |
| 9.–10.3. | — | „Update weights from Colab **imitation** training" (`7b53c08`, `f5e85d0`) |
| **10.3.** | **`54b9517`** | **PIVOT: „Enable value function training in MCTS (AlphaZero path)"** — přidán vf_blend, value se začíná TRÉNOVAT. (Co-author Opus 4.6; policy práce byla Sonnet 4.6.) |
| 10.–11.3. | — | „Update weights from Colab **VF** training" (`bb35086`, `79ba337`, `f9f0c2e`) |
| **12.3.** | **`dada2656`** | **Notebook přepnut: `--policy-lr=0 --lr=LR --training-method=mc`** → policy VYPNUTA, value trénink ON. |
| 12.3. | `30685f1` | „AlphaZero: frozen self-play + gating pipeline" (gating threshold 55 %) — pipeline, co běží dodnes |
| 13.–22.3. | (9× `AlphaZero: X% vs frozen`) | value-trénink éra, gating kolísal 10–50 % (UŽ po vypnutí policy) |
| 22.4. | `22ae314` | migrace na `run_iteration.py` (server), `--policy-lr=0`, od té doby OFF |
| kv.–čvn. | — | value-guided MCTS + shaping → 86–87 %, dnes **89 % neural value** |

### Klíčové závěry z historie
1. **Neural policy reálně běžela, ale jen ~3 dny (9.–12.3.)** a za nejhorších podmínek:
   na **ZMRAŽENÉ LINEÁRNÍ value** (`lr=0.0 model=linear`). Policy imitovala MCTS, jehož
   kvalita byla daná slabou lineární value → imitovala slabý search.
2. **Opuštění NEBYL čistý head-to-head verdikt „neural policy je k ničemu".** Byl to
   **pivot**: tým během ~2 dnů zjistil, že produktivní páka je **trénink VALUE funkce**
   (`54b9517`), ne policy, a přepnul tam. Policy + value se nikdy netrénovaly společně
   na silné value.
3. **Policy-only na 86.7 % nedokázala přidat** — naopak regrese a rollback (`e0314a5`, 7.3.).
   Ale 86.7 % už předtím udělaly heuristiky + linear value, takže policy neměla z čeho
   přidávat na slabém základě.
4. **Měření tehdy bylo jiné a horší:** gating threshold ~55 %, pre-σ-rule, „X% vs frozen"
   při vysoké remízovosti sedí ~50 %. Žádná zachovaná benchmark-trajektorie policy éry
   (commity „Update weights" nenesou % v těle).

---

## ČÁST 2 — Proč DNES může být jinak (hypotéza k posouzení)

**Hlavní argument uživatele i dat:** *teď máme výrazně výkonnější model než v době vypnutí.*

| | Policy éra (úno–bře 2026) | Dnes (čvn 2026) |
|---|---|---|
| Value funkce | **zmražená, LINEÁRNÍ** (`lr=0.0 model=linear`) | **trénovaná, NEURAL, 89 %** |
| Co policy imituje | MCTS řízený slabou lin. value | MCTS řízený 89% neural value |
| Gating | threshold 55 %, šumový | σ-pravidlo, decisive-only, GATING_MATCHES=600 |
| Rostery | base TV~1000 | developed TV1200 (heterogenní skilly) |
| Bring-up postup | rovnou policy+blend, krátce | opatrný: logging → imitation-only → … |

**Hypotéza:** policy nikdy nedostala férový test, protože search, který měla imitovat,
byl slabý (lin. value). Na 89% neural value může mít smysluplnější cíl. → Tým ověřit/vyvrátit.

**Protiargument k posouzení:** pokud 89 % value + heuristické priory už dávají dobrý search,
přidá naučená policy vůbec něco? Nebo je nil_nil/remízové plateau dané value+shaping a policy
to neprolomí (per-player by pak bylo relevantnější)?

---

## ČÁST 3 — Aktuální stav kódu (ověřeno 2026-06-17 proti HEAD)

### Dnešní imitation-only běh (commit `878b795`, `--policy-model=linear`)
- iter1: **policy_loss plochý ~2.24** přes 16 epoch; **top1_agreement 27.8 %→38 %** v 1. epoše,
  pak plató ~38 %. Value drží (benchmark 87 %, REJECTED správně). Metrics fix funguje.
- **Příčina plató u linear:** (a) malá kapacita; (b) **bug #5** — lineární `train_on_decisions`
  IGNORUJE `passes`, takže `imitation-epochs=16` (5 passes/epochu) dělal reálně ~1 pass.

### Připravenost neural (ověřeno)
- `NeuralPolicyTrainer` postavený (`policy_trainer.py:139`), `create_policy_trainer('neural')`,
  default `hidden_size=32`. **RESPEKTUJE `passes`** (`policy_trainer.py:175` `for pass_idx in range(passes)`).
- Warm-start automatický: `weights_best.json` legacy `type:neural` (value W1/b1/W2/b2) → value
  z 89 %, policy od nuly (`policy_trainer.py:419-422`).

### Známé bugy/rizika (z `team_alphazero_results.md`, stále neopravené)
| # | Problém | Místo | Dopad |
|---|---------|-------|-------|
| 1 | Dirichlet noise hardcoded 0.3/0.25 i v benchmark/gating | `bb_module.cpp:445-446` | šum v MĚŘENÍ → zkreslený gating |
| 2 | Blend skok 0→plný bez rampu | `training_loop.py:319-322` | slabá raná policy skokem nahradí heuristiku |
| 4 | Temperature mismatch: makro blend ignoruje `temperature_` (eff. 1.0 vs Python 0.3) | `macro_mcts.cpp:206` | priory plošší |
| **6** | **hidden>64 ticho ořízne** (`float hidden[64]` + `min(H,64)`) | `policy_network.cpp:44-45` | **neural hidden MUSÍ ≤64; default 32 OK** |

### Hot path (potvrzeno AZ týmem)
- Blend priorů jede přes **MAKRO MCTS** (`macro_mcts.cpp:188,312`), ne mikro `mcts.cpp`.
  Jakákoli úprava priorů musí cílit na makro.

---

## ČÁST 4 — Co má tým rozhodnout / dodat

1. **Verdikt na option A (neural):** jít / nejít / jinak — s explicitním zdůvodněním VŮČI HISTORII
   (proč teď ne-slepá-ulička), ne jen vůči kódu.
2. **Fér test neural policy:** jaká minimální konfigurace dá neural policy férovou šanci, kterou
   nikdy nedostala (silná value + dost passes + správné měření)? Hidden size (≤64), lr, passes,
   kolik imitačních iterací než se rozhodne.
3. **Metrika úspěchu imitace:** jaké top1_agreement / loss by mělo neural přelézt, aby mělo smysl
   pouštět ji do blendu (krok 5)? Jaký je teoretický strop (počet makro-akcí → uniform loss)?
4. **Rozhodovací brána:** když neural NEpřeleze práh imitace → zastavit AZ větev a jít na
   per-player / jiný směr? (Aby se neopakovala březnová ~3týdenní smyčka bez zisku.)
5. **Pořadí oprav bugů:** dirichlet (#1) a temperature (#4) PŘED měřením blendu? Ověřit proti kódu.

## Provozní omezení
- Jedna změna naráz, ověřit 2–3 iterace ([[feedback-implementation-style]]).
- Commit+push před tréninkem; server po pullu rebuild engine jen při C++ změně ([[feedback-commit-before-training]]).
- HW: 12 CPU, MCTS=100, EPOCHS=16, GAMES=40, hidden_size(value)=64.
- KAŽDÉ tvrzení ověřit proti aktuálnímu kódu (lekce z team1: Sonnet brief měl chyby, Opus opravil).

## Doložení (commit hashe k ověření)
Policy éra: `4364fdc 1c6db16 e0314a5 e2afa6f cbf10d3 66d4e92 c636fbf c59369f`.
Pivot: `54b9517 dada2656 30685f1`. Migrace: `22ae314 ae26d92`. Dnešek: `878b795 61575b9 8ee3816`.
