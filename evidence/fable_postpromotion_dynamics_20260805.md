# ZADÁNÍ pro Fable agenta: Post-promotion dynamika + stash kontinuita (05.08.2026)

## Kontext a hypotéza uživatele (04.08. večer — VYSOKÁ PRIORITA)

Historie: 01.08. fairtest potvrdil akumulaci policy přes rejecty (54,26 %);
03.08. PRVNÍ PROMOCE (HtH 55,8 % > práh 53,9 %, gate blend 0.2); 04.08.
iterace #2 proti novému šampionovi (poprvé hraje SE svou policy) = REJECTED,
HtH 50,3 % [45,2–55,4].

**Hypotéza uživatele k prověření: „po skocích ve zlepšení se výsledek vrátí
do šumu okolo 50 % a budeme zpátky"** — tj. každá promoce jen zvedne laťku,
kandidát se vůči ní vrátí k šumu, a systém jako celek stagnuje (plateau se
schody, ne růst). Tvůj úkol: potvrdit/vyvrátit/kvalifikovat DATY, a hlavně
z PODRUŽNÝCH vyhodnocení (pokyn uživatele: „zaměřit se na podružná
vyhodnocení — co jsme si nachystali do logů"), ne jen z binárního gate
výsledku.

## Úkol 1 — podružná vyhodnocení z logů (jádro práce)

Projdi a dej do souvislosti VŠECHNU připravenou instrumentaci:
- `epoch_metrics.csv` (+ archivy epoch_metrics_noreset_iter*.csv):
  policy_loss, top1_agreement, mcts_H (entropie), pre_td_ramp, w_norm_Δ,
  grad_norm, nil_nil % — trendy NAPŘÍČ iteracemi/dny, zvlášť před/po
  promoci 03.08. Otázka: učí se policy dál (loss klesá, top1 roste), nebo
  se zastavila?
- `gate_history.jsonl` — celá série: HtH chess skóre, CI, prahy, tiery,
  benchmark; vynes časovou řadu; kde jsou skoky vs. šum. Pozor:
  benchmark je nově na stropu 100 % (ztratil rozlišovací schopnost) a
  頻ozšířená pole gate_policy_blend/frozen_policy_blend existují až od 03.08.
- Per-race rozpady, draw-rate (šumové dno ±8–11 pp na N=150!), side audity
  z logů training_gateblend_20260803/04.log a diag_policy_confirm výsledků.
- Kvalitativní vzorky: replay logy z nočního A/B (ab_run_20260804/
  diag_f1_cage_advance_rows.jsonl — NOVÁ AI s klecí) vs. starší korpus —
  metodika „důkaz učení" 29.07.: konkrétní situace > agregáty.

Výstup úkolu 1: verdikt k hypotéze se sílou evidence (učí se / stagnuje /
nelze rozhodnout + co by rozhodlo), a KONKRÉTNÍ doporučení co měřit dál
(power analýza z 30.07. říkala ~+5pp pro 80% šanci projít gate — dej do
kontextu).

## Úkol 2 — stash kontinuita (mechanická příčina „návratu k šumu"?)

04.08. po iteraci #2 ZMIZEL policy stash `weights_policy.json` z rootu,
přestože log (ř. 120 training_gateblend_20260804.log) potvrzuje zápis
(„Policy stash: vytrénovaná policy hlava uložena"). Soubor je untracked,
git reset ho smazat nemohl; nikdy nebyl tracked (git log --all prázdný).
Claude ho obnovil z weights_snap_e16_99pct_+2.6.json (mtime 09:12 =
finální epocha dnešního tréninku; md5 obnoveného stashe
fa7698b80a006ecc033e01baedb4e0e7).

1. **Forenzně najdi mazací mechanismus** (timeline 12:35 zápis → ~14:10
   nenalezen; mezitím: konec gate 13:36 + git push iterace, poté Claudova
   merge sekvence: git merge f1-cage-fix, cherry-pick 149b7cc, cmake build,
   bb_tests z engine/build, pytest z rootu, git push, worktree remove ×3).
   Prověř i historicky: přežíval stash minulé iterace? (mtime vzory,
   staré logy s „Policy carry-over" vs. „žádná uložená policy".) Pokud je
   mazání SYSTEMICKÉ po každé iteraci od některého commitu, akumulace
   přes rejecty mohla být v části historie rozbitá — přímý kandidát na
   vysvětlení plateau!
2. **Ověř korektnost obnovy**: je policy v snap e16 identická s tím, co by
   _stash_policy extrahoval z finálního az_train? (az_train na disku je
   po push-resetu stará verze — zdůvodni z kódu/časů.)
3. Guard UŽ JE IMPLEMENTOVÁN (04.08. večer, commit ee353c8): _stash_policy
   archivuje každou verzi do policy_backups/ (čas+md5, retence 30) a
   _carry_over_policy se z nejnovější zálohy sám hlasitě obnoví. Tvůj úkol
   k tomu: jen REVIEW (stačí guard? edge-cases?) — neimplementuj nic.

## Úkol 3 (VOLITELNÝ — jen pokud A/B doběhl a CPU je volné)

Nejpřímější test hypotézy, který data dovolují: **párový H2H dvou policy
hlav** — šampionova promotnutá (weights_best_policy.json, md5 cd72ed6b) vs.
obnovený stash s dnešní rejected deltou (weights_policy.json, md5 fa7698b8)
— obě strany stejné value váhy (weights_best.json) + blend 0.2, side-swapped
páry, N≥300 párů, vzor diag_policy_confirm. Odpovídá přímo: „zlepšil dnešní
rejected trénink policy, zhoršil, nebo šum?" Pre-registruj čtení: decisive
WR s Wilson CI; |Δ| uvnitř CI = ŠUM (a to je legitimní výsledek!).

## Statistická poctivost (závazné pro formulaci závěrů)

Hypotéza „návrat k šumu po skoku" má zatím n=1 post-promotion bod — NELZE
ji potvrdit ani vyvrátit; NEPIŠ závěr, který data nedovolují. Deliverable
úkolu 1 je: (a) co říkají podružné metriky o učení policy, (b) ROZHODOVACÍ
RÁMEC do budoucna (kolik iterací / jaké prahy / jaká instrumentace by
hypotézu rozhodly — power analýza ~+5pp pro 80% šanci projít gate je
z 30.07., dohledej v evidence/ nebo gate_history kontextu). K retro-analýze
jsou i archivy weights_noreset_iter1-4.json (víkendová série).

## Omezení

- Převážně analýza logů/kódu — minimum výpočtů; případná měření nice -19,
  max 2 procesy. Šampiona a weights v rootu NEMĚNIT (weights_policy.json
  už je obnoven — jen číst!). Žádný git push.
- Pozor: ráno může ještě dobíhat F1 A/B (PID 80121/80124/80125/80126,
  ab_run_20260804/) — nerušit, jeho jsonl jen číst.
- Výstup: `evidence/fable_postpromotion_report_20260805.md`, stručně,
  závěry napřed; grafy jako ASCII/tabulky. Rozpočet ≤150k tokenů.
