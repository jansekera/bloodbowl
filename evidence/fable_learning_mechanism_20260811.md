# ZADÁNÍ pro Fable agenta: Proč se policy neučí + jak zvětšit výtěžek iterace (příští týden, ~11.08.2026)

⚠️ DRAFT 05.08. — před dispatchem DOPLNIT (sekce na konci): závěry Fable #2
(AlphaZero metodika, report 05.08. odpoledne) a výsledky A3-2 kotev (noc
05.→06.08.). Schváleno uživatelem 05.08. („polovinu 2 jako další fable úkol
na příští týden") — druhý úkol týdne vedle tempo doktríny (20260810).

## Problém (poloviny „neučíme se")

Policy hlava („intuice" — síť našeptávající tahy vyhledávání, blend 0,2)
se PŘESTALA zlepšovat: per-epoch metriky ploché napříč 5 iteracemi
(policy_loss ~1,99; top1 ~42 %; mcts_H ~0,88 — evidence/
fable_postpromotion_report_20260805.md §1b). Páté potvrzení plateau
diagnózy 17.07. (policy odhaduje ~25 % signálu, strop featur). Power
analýza 30.07.: na 80% šanci projít bránou je třeba ~+5 pp reálného zisku
— plochý mechanismus je nedodá. Verdikt reportu 05.08.: „není co promovat,
dokud se nezmění učicí mechanismus."

## Rámující diagnóza (uživatel, 05.08. — ověř a rozpracuj)

„Elfí intuici jsme dopustili tím, že jsme neměli pro trpaslíky styl hry,
ale pro elfa ano." Přesněji: search má JEDEN univerzální styl (mobilita,
bezpečné kostky), který odpovídá hře hbitých ras; trpasličí styl
(klec/grind/držení) vyžaduje víc-tahovou doktrínu, která v datech nikdy
nebyla → policy neměla z čeho trpasličí hru imitovat a zesílila jediný
existující styl. Důsledek: plátky (cage/blitz/…) = výrobníky trpasličího
stylu v self-play datech — první příležitost, aby se policy učila i
ne-elfí hru. Tvá páka 1 (dwarf regrese) má tuhle hypotézu POTVRDIT/VYVRÁTIT
na konkrétních situacích (jaké tahy policy trpaslíkovi našeptává a proč
jsou špatné).

## Úkol: seřazený plán, JAK zvětšit výtěžek jedné iterace

Kandidátní páky (rozpracovat s odhadem přínosu/nákladu, seřadit):
1. **Odstranění trpasličí regrese intuice** — fairtest 31.07. per-race:
   wood-elf +74 %, skaven +65 %, ale **dwarf 32,7 % = −17 pp**. Promotnutá
   policy trpaslíky AKTIVNĚ kazí. Root-cause analýza (jaké tahy dwarfovi
   našeptává a proč jsou špatné — konkrétní situace, ne agregáty) může být
   nejlevnější skok. Vazba na trvalý cíl zlepšit dwarf AI a na tempo
   doktrínu (zadání 20260810).
2. **Vstupy (featury) policy sítě** — strop featur je diagnostikovaná
   příčina. Existuje spec ~492 per-player featur
   (bloodbowl/team1_brief_per_player.md) — POZOR: per-player pokusy mají
   3× NO-GO (naposledy 20.07., paměť day_20260720); nenavrhovat čtvrtý
   stejný pokus bez NOVÉ evidence, ale menší cílené přírůstky featur
   (např. klec/koridor/tempo signály z F1 světa) jsou otevřené.
   **ZÁVAZNÝ CONSTRAINT (uživatel 05.08.): „rasově oddělená je pro
   gobliny" — ŽÁDNÉ per-race hlavy ani race labely.** Styl se musí
   odvodit ze SCHOPNOSTÍ a situace (profil MA/ST/AG můj vs soupeřův,
   mobilita nositele, tempo drivu) tak, aby se správnou hru naučili
   „i trpaslíci i orci" — pomalý+silný → klec/grind emergentně, pro
   všechny pomalé rasy najednou. Stejná zásada jako u plánovačů
   („generic over skills, never race names") — jen o patro výš.
3. **Kapacita/architektura policy sítě** (hidden size, sdílení s value) —
   levný experiment, ale plateau na 42 % top1 drží už od lineární verze
   (team_neural_policy_brief.md) → samotná kapacita nejspíš nestačí.
4. **Kvalita tréninkových dat**: self-play s BB_STAGED_PICKUP=1 (item13,
   wiring 3d6e456 hotov) a výhledově s cage advance (po tempo doktríně) —
   plánovače generují situace, které dnešní self-play neumí zahrát, tj.
   nový signál pro imitaci.
5. **Rozvrh učení**: imitation-only (16 epoch, blend 0 při tréninku) vs
   učení s blendem; MCTS rozpočet 100 vs 400 (probe 21.07.: 400 iterací
   snižuje entropii cíle — ostřejší imitační target).
NEOTVÍRAT bez nové evidence: mc_td_mix (REFUTED 14.07.), fáze A retest
(NO-GO 16.07.), value akumulace přes rejecty (4/4 REJECTED plochý trend).

## Deliverable

`evidence/fable_learning_mechanism_report_20260811.md`: root-cause dwarf
regrese s konkrétními situacemi + seřazený plán pák 1–5 (přínos/náklad/
riziko/jak změřit) + doporučený PRVNÍ krok. Podklad pro rozhodnutí
uživatele; implementace až po GO. Měření povolena (nice -19, neinterferovat
s tréninkem), rozpočet ≤150k tokenů.

## DOPLNĚNO 05.08. odpoledne:
- [x] **Fable #2 závěry (evidence/fable_alphazero_methodology_report_20260806.md):**
      gating zachovat (no-gate AlphaZero režim padá na propustnosti ~640 her/iter
      vs 44M partií); pořadí potvrzeno = učicí výtěžek PŘED A3-1 ligou (plochý
      loss žádná opponent-diversity nespraví); kotvy povýšit na standing
      instrumentaci; saturovaný benchmark tier nahradit HtH vs kotva.
- [x] **A3-2 kotevní čísla (600 her/rameno vs kotva b426c64d; control 0,500 ✓):**
      noreset1→4 = 53,6/49,6/49,6/52,5 % — PLOCHÁ řada (vše CI přes 0,5) →
      tichá akumulace přes rejecty se NEKONÁ, plateau je reálné; champion
      (value 17578260 + policy 0,2) 53,99 % [48,9–59,1] = kladná tendence
      odpovídající jednorázovému zapnutí policy, ne učení. → Váha úkolu se
      PŘESOUVÁ plně na páky 1–5 (učicí mechanismus), měřicí infrastruktura
      je vyřešená jinde.
- [x] **Re-run policy-vs-policy H2H FINÁL (600 her, doběhl 05.08. 16:01 UTC;
      diag_policy_vs_policy_20260805/results.json):** celkově ŠUM — stash
      („Živá" fa7698b8) vs promotnutá intuice (cd72ed6b) 51,4 % [46,5–56,2],
      z=0,55; šampion nedotčen. ALE per-race rozpad je informativní:
      **wood-elf 73,8 % [63,2–82,1], z=4,25 (Živá výrazně LEPŠÍ elfy) vs
      dwarf 37,8 % [28,1–48,6], z=−2,21 (Živá dál HORŠÍ trpaslíky)** —
      pokračující učení přes rejecty táhne stejným směrem jako Elfí intuice:
      zlepšuje rychlé rasy, prohlubuje dwarf regresi. Přímý vstup pro páku 1
      (dwarf root-cause): učicí signál systematicky preferuje elfí hru;
      pozor — per-race řezy jsou post-hoc, N=120/rasu, ale wood-elf přežije
      i Bonferroniho korekci.
- [x] **První ostrá iterace s BB_STAGED_PICKUP=1 (item13 v bráně, noc
      05.→06.08., training_staged_20260805.log): REJECTED 51,7 % < práh
      52,6 %** [46,7–56,8]; selection az_train 60W–38L; benchmark 99,5 %.
      Wiring 3d6e456 prošel ostrým testem: gate_history má
      gate_staged_pickup=true / frozen_staged_pickup=false, frozen správně
      re-benchmarkován (staleness), stash carry-over proběhl (nová Živá
      7e962a41). → Self-play korpus této iterace = první data se staged
      plánovačem (páka 4 — kvalita dat); k dispozici pro analýzu.
