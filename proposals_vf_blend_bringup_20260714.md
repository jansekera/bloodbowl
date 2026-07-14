# Návrh: vf_blend=0.15 bring-up jako samostatný jednovariablový experiment

**Datum:** 2026-07-14 · **Autor:** Fable 5 (read-only agent, nic nespuštěno)
**Zdroj zadání:** `evidence/fable_ramp_postmortem_20260714.md` doporučení #1 —
mc_td_mix páka je mrtvá (hlavy corr 0.9998 při změně α), vf_blend bring-up se
ODPOJUJE od mc_td_mix a běží se stávající mc_return hlavou (`weights_best.json`,
champion 30.06., benchmark 91.5 %/89 % dle škály).

> Kontext při psaní: multi-arm chain A/B (H2 kickoff + screen 2a/2b) právě
> běží (`diag_h2_screens_chain_20260714.log`, start ~11:33 UTC, arm `pre`
> 10/400). Vše níže je navrženo tak, aby (a) běželo AŽ PO doběhnutí chainu,
> (b) recyklovalo jeho rameno `arm_chain_lane2b.json` jako kontrolu.

---

## 0. Proč je to samostatná páka a co přesně testujeme

Tři nezávislé linie evidence se protínají v jednom bodě:

1. **Strukturální disconnect** (syntéza 07-09): value hlava se ~3 měsíce
   trénuje, ale při `vf_blend=0` NIKDY nevstoupila do searche. Žádný
   reward/value/feature experiment proto nebyl férově testován.
2. **Gating null-test** (07-02, POTVRZENO měřením): při `vf_blend=0` gate
   váhy neměří vůbec (kandidát s NULOVÝMI váhami ≈ self-mirror). S
   `vf_blend>0` gate POPRVÉ začne být na váhy citlivý — bring-up je zároveň
   oprava citlivosti měřidla.
3. **Protisíla — draw-collapse riziko** (06-24): `vf_blend` ředí heuristické
   forward-pull termy → pasivita. Historie: `vf_blend=0.3` (caa99da)
   benchmark 89→76 % + VF inversion; `vf_blend=0.5` (mc_return_shaped)
   REJECTED 89→80, 87.5 % remíz.

Klíčová změna od té doby: **fix #1 (524a39f) přesunul `scoringBonus` až ZA
blend** — ověřeno v worktree (budoucí HEAD)
`bloodbowl-fixes-20260714/engine/src/macro_mcts.cpp:675-692`:
`leaf = (1−b)·heuristic + b·vf; return clamp(leaf + scoringBonus)`. Hlavní
identifikovaný kanál diluce (skórovací tah) je tedy strukturálně mimo blend.
Prior floors (SCORE-family, PICKUP…) jsou prior-side a blend se jich nedotýká.
Ředí se „jen" poziční termy (marking/contain/player-count) — a přesně tam VF
z cross-game kalibrace (corr(V,G)=0.81) něco umí.

**Co testujeme:** ne „hlava je chytrá" (post-mortem ukazuje, že within-game
signál skoro nemá), ale (i) **neškodí injekce hlavy hře** (non-inferiorita
vůči čisté heuristice), (ii) **začne gate měřit váhy** (reverse null-test),
(iii) uzavře se training↔play smyčka, aby BUDOUCÍ value experimenty měly
vůbec kauzální cestu do hry. Kritérium pro postup do tréninku je „neškodí",
ne „pomáhá".

## 1. Hodnota: 0.15, bez rampy v search-only fázi, s rampou v tréninku

- **0.15, ne 0.3/0.5:** obě vyšší hodnoty historicky selhaly, ale PŘED
  fixem #1 a před 13 engine fixy — jejich verdikty nejsou platná evidence
  proti mechanismu (syntéza 07-09), zato jsou platná evidence o velikosti
  dávky. 0.15 drží perturbaci leafu omezenou: |Δleaf| ≤ 0.15·|vf−h| ≤ 0.30
  worst-case, typicky ~0.15·0.6 (mean_abs_vf ≈ 0.6 z post-mortemu) ≈ 0.09 —
  řádově 3 jednotky player-count termu (0.03/hráč), tj. znatelné, ne
  dominantní.
- **Ne méně (0.05):** pod úrovní pozičních termů by efekt zapadl do šumu a
  bring-up by nic nerozhodl; 0.15 je nejmenší hodnota s reálnou šancí na
  detekci v paired-seed designu.
- **Search-only fáze: fixní 0.15.** Ramp je koncept tréninkových epoch;
  v čistém search A/B nemá co rampovat.
- **Tréninková fáze: defaultní ramp zůstává.** `run_iteration.py:39`
  `VF_RAMP_EPOCHS=10` + `training_loop.py:330-333` → self-play jede
  0.015→0.15 lineárně přes epochy 1-10 (gate/benchmark jedou na plných 0.15
  od začátku). To je žádoucí: mírný náběh v self-play datech, plná citlivost
  měřidla.

## 2. Fáze 0 — search-only paired A/B (PŘED jakýmkoli tréninkem)

**Nulová cena na kód: `vf_blend` je runtime parametr** (pozice 6 v 9-tuple
`_gate_game`; `bb_module.cpp:451` `cfg.vfBlend = vfBlend`). Žádný rebuild,
žádný commit — jen diag běhy na stávajícím post-merge bináru. Tím je fáze 0
levnější než kterýkoli dosavadní engine A/B (odpadá cross-rebuild tanec
`run_stall_guard_ab.sh`).

**Zásadní omezení bindingu (ověřeno):** `vf_blend` je JEDEN skalár pro obě
strany (`simulate_game_logged` nemá per-side blend) a prázdný
`away_weights_path` PADÁ ZPĚT na home váhy (`bb_module.cpp:424`). Důsledky:

- asymetrický duel „0.15 vs 0.0 blend" NENÍ vyjádřitelný bez úpravy bindingu
  → neděláme ho; asymetrii vyjadřujeme přítomností/nepřítomností VAH,
- „null váhy na AWAY" NENÍ vyjádřitelné (fallback) → null jde jen na HOME
  slot; slot-bias korigujeme kalibrací proti mirror ramenu (viz 0b). Platí
  to i po případném merge side-swap patche (jeho 10-tuple prohazuje CESTY,
  prázdná cesta na away zas jen spadne na fallback) → design 0b je stejný
  s i bez side-swapu.

### 0a — symetrický mirror: „co udělá injekce hlavy s HROU" (draw-collapse tripwire)

Obě strany `weights_best.json`, obě `vf_blend=0.15`, paired seedy
`base_seed=20260714` — **identické s chain ramenem `arm_chain_lane2b.json`**,
které slouží jako kontrola (vf=0). Nové rameno = jediných N=400 her.

- Podmínka reuse: binár musí být tentýž jako u lane2b ramene (chain skript
  končí na lane2b buildu = dnešní HEAD engine). Před spuštěním ověřit
  `git rev-parse HEAD` == commit chainu (aktuálně 4af3616 / jeho merge) a že
  se mezitím nerebuildilo nic jiného. Pokud ne → přeběhnout i kontrolní
  rameno `vf0` (dalších 400 her, stejné seedy).
- Poznámka k seed panelu: reuse panelu 20260714 je tu záměrný (sdílí
  kontrolu s chainem). Pokud verdikt vyjde hraniční, replikace na čerstvém
  panelu `base_seed=20260715` před závěrem.

### 0b — reverse null-test / hlava vs čistá heuristika (nejcennější měření)

`diag_null_weights.py` vzor, ale při `vf_blend=0.15`: HOME `weights_path=""`
(žádná VF → čistá heuristika, `bb_module.cpp:418` skip +
`macro_mcts.cpp:680` guard), AWAY `weights_best.json` (0.85·h + 0.15·v).
N=300, stejný seed panel.

Dvě otázky najednou:

1. **Reverse null-test měřidla** (syntéza 07-09: „nejlevnější rozhodující
   experiment projektu"): liší se výsledek od self-mirroru? Při vf=0 null
   vyšel nerozlišitelný (55.8 % draws, ±1 SE od mirroru). Při 0.15 se MUSÍ
   lišit chování stran — pokud ne, blend do searche reálně neteče a je
   potřeba hledat proč (dřív než jakýkoli trénink).
2. **Unknown #3 ze syntézy** („pomáhá natrénovaná hlava, když se do hry
   dostane? férový test nikdy neproběhl"): znaménko efektu. Slot-bias
   kalibrace: home-edge při 0.15 přečíst z 0a-vf015 ramene (home_win share
   v mirroru) a od výsledku 0b ho odečíst — ne proti staré konstantě +7pp,
   ta je z jiného světa (pre-H2-fix).

### 0c — benchmark guard vs random (replikace caa99da signatury bez tréninku)

Historické selhání vf_blend=0.3 se projevilo benchmarkem (89→76 %). Paired
benchmark: `_benchmark_game` 7-tuple, ramena vf=0.15 vs vf=0.0, N=200,
outcome `win` (`diag_utils.OUTCOMES['win']`). Levné (random soupeř = rychlé
hry) a přímo cílí na historický failure mode.

### Metriky a prahy fáze 0

Vše přes `diag_utils.mcnemar_report` (závazně, `feedback_draw_rate_noise_floor`:
delta <10pp z jednoho N=150 = INCONCLUSIVE; paired-seed je rozhodovací
nástroj).

| měření | metrika | ČERVENÁ (stop, vf zůstává 0) | zelená/neutrální (postup) |
|---|---|---|---|
| 0a | draws (McNemar) | CONFIRMED nárůst remíz (jakýkoli; vzor mc_return_shaped byl ~+30pp) NEBO horní mez CI > +10pp | CI celé pod +10pp |
| 0a | TD/hru (paired delta, vzor `paired_td_delta` z chain skriptu) | CONFIRMED pokles | CI zahrnuje 0 nebo nárůst |
| 0a | podíl výher marží ≥2 a celkové TD v rozhodnutých (engagement proxy ze skóre) | výrazný pokles spolu s draws↑ | — |
| 0b | HtH decisive share hlavy (po slot korekci) | CONFIRMED horší než heuristika → hlava aktivně škodí; trénink odložit, nejdřív value-side práce | ≈0 nebo lepší; OBĚ znaménka jsou informativní |
| 0b | odlišnost od mirroru | nerozlišitelné od 0a-vf015 → blend neteče, debug PŘED tréninkem | rozlišitelné = měřidlo žije |
| 0c | win-rate vs random | CONFIRMED pokles > 3pp | jinak |

Rozhodovací pravidlo do fáze 1: **0a ne-červená AND 0c ne-červená**. 0b
červená první odrážky (hlava škodí) trénink neblokuje absolutně — trénink
při 0.15 je přesně mechanismus, jak se hlava má NAUČIT být v searchi užitečná
(poprvé on-policy) — ale mění očekávání a doporučuji pak start s poloviční
dávkou (BB_VF_BLEND=0.08) jako kompromis.

## 3. Přesné příkazy

### 3.1 Nový skript `diag_vf_blend_bringup.py` (vzor `diag_h2_screens_chain.py`)

```python
#!/usr/bin/env python3
"""Paired-seed A/B: vf_blend=0.15 bring-up, search-only (no training).

Arms (same binary -- vf_blend is a runtime knob, no rebuild):
    vf0     control, both sides weights_best.json, vf_blend=0.0
            -> normally NOT run: reuse arm_chain_lane2b.json (same seeds,
               same binary); run only if the binary/SHA check fails
    vf015   both sides weights_best.json, vf_blend=0.15          (phase 0a)
    null015 HOME weights_path='' (pure heuristic) vs AWAY champion,
            vf_blend=0.15                                        (phase 0b)
    bm0 / bm015   _benchmark_game vs random, paired               (phase 0c)

Proposal: proposals_vf_blend_bringup_20260714.md
"""
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import _benchmark_game, _gate_game

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1000, 100
BASE_SEED = 20260714          # shared with the chain -> lane2b is the control
LANE2B = "arm_chain_lane2b.json"

ARMS = {
    "vf0":     dict(vf=0.0,  home=W,  n=400, fn=_gate_game),
    "vf015":   dict(vf=0.15, home=W,  n=400, fn=_gate_game),
    "null015": dict(vf=0.15, home="", n=300, fn=_gate_game),
    "bm0":     dict(vf=0.0,  n=200, fn=_benchmark_game),
    "bm015":   dict(vf=0.15, n=200, fn=_benchmark_game),
}


def tasks_for(arm: str, seeds):
    a = ARMS[arm]
    if a["fn"] is _benchmark_game:
        return [(s, i, W, MCTS, a["vf"], TV, POLICY_PATH)
                for i, s in enumerate(seeds)]
    return [(s, i, a["home"], W, MCTS, a["vf"], TV, False, POLICY_PATH)
            for i, s in enumerate(seeds)]


def run(arm: str) -> None:
    a = ARMS[arm]
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== vf_blend bring-up: arm={arm} N={a['n']} vf={a['vf']} ===",
          flush=True)
    res = du.run_arm(arm, tasks_for(arm, seeds), game_fn=a["fn"],
                     mcts_iterations=MCTS)
    du.save_arm(f"arm_vfb_{arm}.json", arm, seeds, res)


def compare() -> None:
    ctrl_path = Path("arm_vfb_vf0.json")
    if not ctrl_path.exists():
        ctrl_path = Path(LANE2B)          # reuse the chain baseline arm
    _, s0, r0 = du.load_arm(ctrl_path)
    _, s1, r1 = du.load_arm("arm_vfb_vf015.json")
    assert s0[:len(s1)] == s1[:len(s0)], "seed lists differ -- not paired"
    for outcome in ("draw", "home_win"):
        print(du.mcnemar_report(r1, r0, outcome,
                                label_a="vf015", label_b="vf0"))
        print()
    # TD/game paired delta -- same math as diag_h2_screens_chain.paired_td_delta
    common = sorted(set(r0) & set(r1))
    diffs = [(r1[i][0] + r1[i][1]) - (r0[i][0] + r0[i][1]) for i in common]
    n = len(diffs); mean = sum(diffs) / n
    var = sum((d - mean) ** 2 for d in diffs) / (n - 1)
    se = (var / n) ** 0.5
    print(f"TD/game paired delta vf015-vf0: {mean:+.3f} "
          f"(95% CI [{mean - 1.96 * se:+.3f}, {mean + 1.96 * se:+.3f}])")
    # 0b: null (heuristic) home vs champion away, slot-corrected by the
    # vf015 mirror home_win share
    if Path("arm_vfb_null015.json").exists():
        _, _, rn = du.load_arm("arm_vfb_null015.json")
        dec = [(r[0] > r[1]) for r in rn.values() if r[0] != r[1]]
        mir = [(r[0] > r[1]) for r in r1.values() if r[0] != r[1]]
        print(f"\n0b null@H vs champ@A (vf=0.15): "
              f"{sum(dec)}/{len(dec)} home(heuristic) decisive share "
              f"= {sum(dec)/len(dec):.1%}  |  slot edge from vf015 mirror "
              f"= {sum(mir)/len(mir):.1%}  (difference = head effect)")
        print(f"   draws null015 {100*sum(1 for r in rn.values() if r[0]==r[1])/len(rn):.1f}% "
              f"vs vf015 mirror {100*sum(1 for r in r1.values() if r[0]==r[1])/len(r1):.1f}%")
    if Path("arm_vfb_bm015.json").exists():
        _, _, b0 = du.load_arm("arm_vfb_bm0.json")
        _, _, b1 = du.load_arm("arm_vfb_bm015.json")
        print()
        print(du.mcnemar_report(b1, b0, "win", label_a="bm015", label_b="bm0"))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
```

Pozn.: `_gate_game` si dirichlet/exploration bere z produkčních konstant
(`GATE_DIRICHLET_ALPHA=0.0`, `GATE_EXPLORATION_C=1.0` — viz hlavička chain
logu), tj. plná eval-parita s gatem zadarmo.

### 3.2 Spuštění (AŽ PO doběhnutí chain A/B; detached dle konvence)

```bash
cd /home/jan/claude/bloodbowl && source venv/bin/activate
# 0) předpoklad: chain doběhl, binár = lane2b stav; ověřit:
git rev-parse HEAD          # zapsat k výsledkům (save_arm SHA konvence)
# 1) fáze 0a (kontrola = arm_chain_lane2b.json, jen 1 nové rameno)
setsid nohup python3 diag_vf_blend_bringup.py vf015 \
    > diag_vfb_vf015_20260714.log 2>&1 < /dev/null & disown
# 2) po doběhnutí 0a -> 0b a 0c (sekvenčně, CPU šetřit)
setsid nohup bash -c 'python3 diag_vf_blend_bringup.py null015 && \
    python3 diag_vf_blend_bringup.py bm0 && \
    python3 diag_vf_blend_bringup.py bm015' \
    > diag_vfb_phase0bc_20260714.log 2>&1 < /dev/null & disown
# 3) vyhodnocení
python3 diag_vf_blend_bringup.py compare | tee diag_vfb_verdict_20260714.log
```

Cena: 0a = 1 rameno chainu (N=400, MCTS=100); 0b+0c dohromady ≈ další
~0.9 ramene. Vše read-only vůči enginu i váhám.

### 3.3 Fáze 1 — tréninkový bring-up běh (jen po zeleném verdiktu fáze 0)

```bash
cd /home/jan/claude/bloodbowl && source venv/bin/activate
# commit+push PŘED během (feedback_commit_before_training), pak:
BB_VF_BLEND=0.15 setsid nohup python3 run_iteration.py --loop 1 --no-push \
    > training_vf015_bringup_$(date +%Y%m%d).log 2>&1 < /dev/null & disown
```

- `BB_VF_BLEND=0.15` (`run_iteration.py:52`) nastaví self-play (s rampou
  10 epoch), benchmark I gate na 0.15. Gate tak POPRVÉ porovnává obsah vah —
  pozor, frozen champion tím hraje jinak, než hrál při své promoci; proto
  `--no-push` a verdikt číst ručně, nová škála.
- Jediná proměnná: `BB_POLICY_BLEND` zůstává 0, žádná změna targetu (post-
  mortem: na α nezáleží, jede se mc_return), žádná změna enginu.
- Formát vah: `vf_blend>0` ukládá combined/AZ formát (`training_loop.py:487`)
  — s aktivním policy_trainerem se to děje už dnes, žádná akce.

### Metriky + červené linie fáze 1 (okamžitý stop běhu)

| signál | kde | ČERVENÁ |
|---|---|---|
| VF inversion | log, `VF avg: home=…, away=…` (`training_loop.py:438-441`) | WARNING ve ≥2 epochách po sobě (caa99da vzor: epochy 6,7,10) |
| benchmark | průběžné checkpointy | pokles ≥5pp proti champion referenci (mc_return_shaped vzor 89→80) NEBO pod BM_FLOOR |
| self-play remízy | epoch summary | nil-nil setrvale >60 % |
| gate draws | gate sekce | jen extrém (>15pp od reference) — single-run <10pp je šum |

Úspěch fáze 1 (v tomto pořadí): (1) běh dožije bez červených linií,
(2) gate se chová jinak než self-mirror (váhová citlivost potvrzena i
v produkčním měřítku), (3) benchmark drží, (4) TD/hru v gate neklesá.
PROMOTE verdikt prvního běhu neposlouchat slepě — bez side-swapu je HtH
práh vychýlený (viz §4), s ním je verdikt poprvé čitelný.

## 4. Interakce a ZÁVAZNÉ pořadí

Dva sousedi, oba se dotýkají přesně vrstev, které vf_blend mění:

- **SCORE-availability patch (proposals_score_availability_20260714.md):**
  floor 0.30 opravuje PRIOR vrstvu za předpokladu flat-Q („visits ∝ prior");
  vf_blend>0 mění Q vrstvu a flat-Q režim oslabuje. Nesmí se měřit v jednom
  okně (tamní §6 to už závazně říká z druhé strany).
- **Gating side-swap (proposals_gating_sideswap_20260714.md):** bez něj sedí
  HtH null ~60 % > práh → tréninkový gate verdikt při vf 0.15 by byl
  nečitelný. Fázi 0 NEBLOKUJE (0a je symetrická; 0b kalibruje slot edge
  interně proti 0a-vf015), fázi 1 prakticky ano.

**Doporučené závazné pořadí:**

1. **Doměřit dnešní chain** (běží) → `arm_chain_lane2b.json` = referenční
   rameno post-fix světa.
2. **Side-swap patch** merge + offline stub test (nulová cena) — ideálně
   v jednom balíku s dnešním baseline resetem, jak navrhuje sideswap doc §5.
3. **Fáze 0 vf_blend (tento návrh)** — čisté měření, žádný commit, žádný
   rebuild; proto jde PŘED SCORE patchem (nic nekontaminuje a jeho výsledek
   upřesní očekávání pro patch A: jak moc flat-Q předpoklad platí).
4. **Rozhodnutí dle prahů §2.** Červená → vf_blend zůstává 0, pokračovat
   bodem 5 beze změny plánu (větve jsou nezávislé).
5. **SCORE-availability patch A** (vlastní A/B při vf_blend=0, vlastní
   okno). Engine změna před dlouhým tréninkem = správné pořadí
   (feedback_bugfix_priority_over_speed) a dává fázi 1 lepší substrát.
   POZOR: patch A mění binár → kontrolní ramena fáze 0 po něm NEPLATÍ;
   pokud má jít fáze 1 až po patchi A, přeběhnout levné 0a (2×400) na novém
   bináru pro čistou atribuci.
6. **Fáze 1: tréninkový běh BB_VF_BLEND=0.15** (§3.3) — po side-swapu, po
   patchi A, jako jediná proměnná svého okna.
7. Item 7 (PICKUP top-2/3) a patch B dle pořadí ze score-availability §6;
   `policy_blend` zůstává 0 přes celé toto okno (AZ krok až po vf_blend,
   jedna páka po druhé — master list).

## 5. Co tento experiment neřeší (vědomě)

- **Within-game flatness value targetu** — post-mortem §4.3: nový signál
  musí přijít z nového informačního kanálu (featury/teacher/reward), ne
  z re-mixu G. Bring-up jen otevírá dveře, kterými se budoucí signál do hry
  dostane a bude měřitelný.
- **Asymetrický blend duel (0.15 vs 0.0 na stranách)** — chce per-side
  `vf_blend` v bindingu; zvážit jako drobné rozšíření AŽ pokud 0b vyjde
  těsně (rebuild, vlastní commit, mimo tento návrh).
- **Kvantifikace home-edge post-H2-fix** — vlastní linie (first-possession
  measurement / side-swap Side audit).
