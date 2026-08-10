# Post-promotion dynamika + stash forenzika — report (05.08.2026)

Zadání: evidence/fable_postpromotion_dynamics_20260805.md. Hypotéza uživatele:
„po skocích ve zlepšení se výsledek vrátí do šumu okolo 50 % a budeme zpátky"
(plateau se schody).

## ZÁVĚRY (napřed)

1. **Úkol 1 — verdikt: NELZE ROZHODNOUT (n=1 post-promotion bod), ale podružná
   vyhodnocení hypotézu KVALIFIKUJÍ mechanicky.** Skok 03.08. (55,8 %) vznikl
   v ASYMETRICKÉM gate: kandidát hrál s policy_blend=0,2, frozen šampion s 0,0
   (gate_history: `gate_policy_blend: 0.2, frozen_policy_blend: 0.0`). Měřil
   tedy jednorázový přínos ZAPNUTÍ policy hlavy (~+4–6 pp, shodné s fairtestem
   01.08.: 54,26 %, také policy-on vs policy-off), ne přírůstek učení. 04.08.
   bylo PRVNÍ symetrické měření (oba 0,2) → 50,3 %. „Návrat k šumu" je zatím
   plně vysvětlitelný ztrátou strukturální výhody — nevyžaduje (ale ani
   nevylučuje) stagnaci učení.
2. **Per-epoch metriky policy jsou PLOCHÉ napříč všemi 5 iteracemi**
   (policy_loss ~1,99, top1 ~42 %, mcts_H ~0,88; žádný trend uvnitř iterace
   ani mezi iteracemi). Nezávislý signál: policy hlava se v současném režimu
   dál NEZLEPŠUJE na úrovni fitu — konzistentní s plateau diagnózou 17.07.
   Bez změny učení (featury/kapacita) se „schod" nezopakuje; power analýza
   30.07. žádá ~+5 pp reálného zisku pro 80% šanci projít gate.
3. **Úkol 2 — mazací mechanismus NENALEZEN; ztráta NENÍ systemická.** Všichni
   ověřitelní podezřelí vyloučeni přímou evidencí (git reset/merge/cherry-pick
   ignorované soubory nemažou + `git log --all -- weights_policy.json` prázdný;
   reflog bez `git clean`; pytest suite píše jen do tmp_path/tempfile; engine
   C++ na `weights_policy` nemá jedinou referenci; pipeline kód žádnou mazací
   cestu na stash nemá). Historicky stash PŘEŽÍVAL: „Policy carry-over" logline
   je přítomna ve VŠECH očekávaných iteracích (22.07. 4/4, 20.07. 4/4, 03.08. a
   04.08. po 1/1 vč. přežití přes noc/2 dny). Akumulace přes rejecty tedy
   NEBYLA v historii rozbitá — ztráta 04.08. je první a jediná pozorovaná.
   Zbývá neauditovatelný ad-hoc shell příkaz interaktivní session v okně
   ~09:12–13:42 (disk po něm nenese stopu).
4. **Obnova stashe je prokazatelně korektní**: extrakce policy klíčů ze
   snap e16 (postup `_stash_policy`) dává md5 fa7698b8… == obnovený root
   soubor (obsahová identita ověřena). `weights_az_train.json` na disku je po
   push-resetu committed verze z Colab commitu 353ebcd — BEZ policy klíčů
   (md5 extrakce = md5 prázdného dictu), takže snap e16 byl jediný a správný
   zdroj. Guard ee353c8 review: OK, drobné poznámky níže.
5. **Úkol 3 — policy-vs-policy H2H (šampionova promotnutá cd72ed6b vs stash
   s rejected deltou fa7698b8, obě strany value=weights_best, blend 0,2,
   300 side-swapped párů = 600 her): BĚŽÍ, výsledek se doplní po doběhnutí.**

## Úkol 1 — podružná vyhodnocení

### 1a. gate_history.jsonl — celá série (chess score HtH vs frozen)

```
datum       chess   CI95           prah   tier                 pblend c/f  vysledek
28.07. #1   45.9%   34.0-58.3      56.4%  bm zlepsen (1.0s)    -           REJ
28.07. #2   48.1%   43.1-53.2      52.6%  bm zlepsen (1.0s)    -           REJ
29.07.      50.7%   39.3-62.0      55.9%  bm zlepsen (1.0s)    -           REJ  (mcts 400)
30.07.      50.8%   38.4-63.2      56.5%  bm zlepsen (1.0s)    -           REJ
31.07. #1   45.0%   39.7-50.4      52.8%  bm zlepsen (1.0s)    -           REJ
31.07. #2   49.5%   44.4-54.5      52.6%  bm zlepsen (1.0s)    -           REJ
01.08. #1   48.5%   43.3-53.6      52.7%  bm zlepsen (1.0s)    -           REJ
01.08. #2   46.8%   41.6-52.0      52.7%  bm zlepsen (1.0s)    -           REJ
03.08.      55.8%   50.7-60.8      53.9%  bm ~stejny (1.5s)    0.2 / 0.0   PROMOTE
04.08.      50.3%   45.2-55.4      52.6%  bm zlepsen (1.0s)    0.2 / 0.2   REJ
```

- Pre-promotion serie 8 mereni: 45,0–50,8 %, vse v pasmu sumu kolem 50 %
  (side audity 03.08. i 04.08. symetricke: home-slot 48,8 % resp. 49,2 %
  decisive — zadny side bias).
- Jediny „skok" je 03.08. — a je to JEDINE asymetricke mereni cele serie
  (frozen bez policy). 04.08. (prvni fer policy-vs-policy bod) = 50,3 %.
- Benchmark vs random je na stropu (99–100 %) → nulova rozlisovaci schopnost;
  pritom PRAH gate (k=1,0 vs 1,5 → 52,6 % vs 53,9 %) ridi prave sum tohoto
  saturovaneho signalu (99 % vs 100 % prehazuje tier). Doporuceni nize.

### 1b. epoch_metrics — trendy uvnitř a napříč iteracemi

```
iterace       loss_mean  loss_e1->e16   top1_mean  top1_e1->e16   mctsH   ramp
iter1 31.07.  1.9940     1.995->1.999   42.2%      41.7->43.3%    0.879   +0.71
iter2 31.07.  1.9961     2.004->1.984   41.6%      41.5->41.8%    0.879   +0.68
iter3 31.07.  1.9926     2.016->1.999   42.0%      42.2->41.4%    0.878   +0.63
iter4 01.08.  1.9903     1.987->1.990   42.4%      43.1->41.7%    0.878   +0.62
it#2  04.08.  1.9889     1.998->2.011   42.0%      41.7->40.9%    0.879   +0.67
```

- policy_loss NEklesa (rozptyl epoch ~±0,02 = sum), top1 NEroste, mcts_H
  konstantni na desetinu procenta. Ani PO promoci (04.08., poprve trenink
  proti sampionovi s policy) se nic nezmenilo.
- Interpretace: policy hlava je na svem kapacitnim/feature stropu (plateau
  17.07. potvrzeno popate); dalsi „schod" z pouheho opakovani iteraci
  necekat. To NENI dukaz hypotezy o navratu k sumu — je to dukaz, ze neni
  co promovat, dokud se nezmeni ucici mechanismus.
- Pozn.: metrics_archive/ (4c34638) zatim prazdny — plni se az od pristi
  iterace; per-iteracni archiv byl dosud jen u noreset serie.

### 1c. Fairtest 31.07. (policy-on vs policy-off) — per-race rozpad

Decisive WR kandidata s blendem 0,2 (N=1600, sanity flagy zadne):

```
celkem     54.26%  CI 51.1-57.4  (prah 52.63% -> PROSLO)
wood-elf   74.4%   CI 68.0-79.9
skaven     65.1%   CI 58.4-71.2
human      52.4%   CI 45.3-59.4
orc        44.3%   CI 37.0-51.9
dwarf      32.7%   CI 26.6-39.4   <- -17pp regrese
```

Promotnuta policy je silne rasove asymetricka: pomaha hbitym rasam, dwarfa
zhorsuje. Souvisi s trvalym cilem „zlepsit AI za trpasliky" — pripadny dalsi
skok pres gate muze prijit i z odstraneni dwarf regrese, ne jen z pridani
sily jinde.

### 1d. Kvalitativni vzorky — nocni A/B (ab_run_20260804, 3200 her)

Rows nesou per-game agregaty (cand_plans = pocet CAGE_ADVANCE planu, attr =
attrition). Podminena analyza her S aspon jednim planem vs BEZ planu
(POZOR: observacni, selection bias — plany se zapaluji ve stavech s drzenim
mice; neni to kauzalni dukaz):

```
matchup            plans>0: n   WR_dec  attr inf/suf | plans=0: n   WR_dec  attr inf/suf
dwarf-skaven         266       61.4%    0.91/0.42    |   534       52.5%   0.65/0.86
dwarf-woodelf        324       55.0%    0.83/0.70    |   476       49.3%   0.77/0.71
dwarf-mirror         440       61.5%    0.52/0.61    |   360       36.1%   0.58/0.60
orc-skaven (kontr.)  217       75.9%    0.98/0.40    |   583       44.2%   0.59/0.85
```

- Hry, kde klec vystrelila, maji konzistentne vyssi decisive WR a otoceny
  pomer attrition (vice rozdano nez utrzeno) — plan se zapaluje ve „spravnych"
  stavech a koreluje s jejich konverzi. Adopce zustava nizka (61 % her bez
  jedineho planu; distribuce 0/1/2/3/4 planu: 1953/996/220/26/5).
- Toto je novy typ dukazniho materialu dle metodiky 29.07. (situace >
  agregaty) — doporucuji z rows vytahnout konkretni seedy her s 2+ plany
  pro galerie diskuzi A/C.

### 1e. Rozhodovací rámec pro hypotézu (co by ji rozhodlo)

Hypoteza „kazda promoce jen zvedne latku a vratime se k sumu" potrebuje >=3
post-promotion body, ktere zatim nejsou. Konkretne:

1. **Absolutni kotvy misto pohyblive latky**: diky 4c34638 (policy_stash_md5
   v gate_history + policy_backups/) lze kdykoli parove zmerit policy(iter N)
   vs policy(iter M). Doporucena instrumentace: po kazde promoci H2H
   novy_sampion vs PREDCHOZI sampion (ne frozen tehoze dne) a 1x tydne vs
   fixni kotva (napr. dnesni cd72ed6b). Roste-li rada kotevnich WR, ucime se
   i pri gate ~50 %; stoji-li, plateau je realne. Dnesni ukol 3 je prvni bod
   teto rady (vysledek: SUM).
2. **Power kontext (30.07.)**: prah gate = 50 % + 1,0–1,5 s pri s~2,6 pp na
   N=600 → kandidat s realnym +2,6 pp ma ~50% sanci projit, pro ~80% sanci
   potrebuje ~+5 pp. Per-epoch metriky (1b) nenaznacuji ani zlomky pp za
   iteraci → dalsi promoce z pouheho opakovani je loterie na sumu, ne trend.
   Duesledek: bud zvetsit vytezek iterace (featury/kapacita policy, dwarf
   regrese, item13 planovac), nebo akceptovat pomalou akumulaci pres rejecty
   a merit ji KOTVAMI (bod 1), ne gate binarkou.
3. **Benchmark tier vymenit**: saturovany benchmark (99–100 %) dnes nahodne
   prepina prah 52,6/53,9 % — o prisnosti gate rozhoduje sum stropu. Nahradit
   tier vstup necim informativnim (napr. HtH vs kotva z bodu 1), nebo tier
   zafixovat.
4. **Kvalitativni archiv** (29.07.): rows z A/B + replay logy uz umoznuji
   galerii konkretnich situaci nove AI — doplnovat pri kazdem mereni.

## Úkol 2 — stash forenzika

### 2.1 Mazací mechanismus + systemičnost

Casova osa 04.08. (vse UTC, z mtimes/reflogu/logu):

```
~04:34        start treninku iterace #2
09:12:29      konec epochy 16; zapis weights_snap_e16_99pct_+2.6.json;
              hned pote _stash_policy -> weights_policy.json (log r. 120-121)
09:12-13:33   gate (600 her; stash gate pouziva jen ke CTENI)
13:33:54      _git_push: git reset --hard origin/main + commit ef9ad82 + push
13:37:12      git merge f1-cage-fix (3d01756, ort) + cherry-pick 149b7cc
~13:38-40     cmake build, bb_tests (engine/build), pytest (root),
              git worktree remove x3  [interaktivni session]
13:42:14      setup ab_run_20260804/: kopie weights_best.json +
              weights_policy.json := SNAPSHOT sampiona (md5 cd72ed6b, NE
              stash) — dle pameti 04.08. stash v rootu v tu chvili UZ CHYBEL
13:54:05      mtime dnesniho root weights_policy.json = okamzik OBNOVY
              (extrakce ze snap e16; cerstvy zapis, ne cp -p)
14:18/14:50   guard ee353c8 + metrics/md5 4c34638
```

Vylouceni podezrelych (primou evidenci):

- `git reset --hard` / `merge` / `cherry-pick`: weights_policy.json je
  IGNOROVANY (`/weights*.json` v .gitignore) a nikdy nebyl tracked
  (`git log --all -- weights_policy.json` prazdny) → git ho nemaze ani
  neprepisuje; merge 3d01756 + cherry-pick 47bbc18 menily jen engine/diag
  soubory (git show --stat).
- `git clean`: v reflogu/okoli zadna stopa; nikdo ho nevola ani ze skriptu.
- pytest: vsechny testy pisi vyhradne do tmp_path/tempfile (proverena cela
  python/tests/); zadny test nesaha na PROJECT_ROOT soubory.
- bb_tests / cmake: engine/ nema JEDINOU referenci na `weights_policy`
  (grep *.cpp *.h) a bezi v engine/build.
- pipeline: run_iteration.py nema zadnou mazaci cestu na stash (jedine
  unlink je retence policy_backups/); training_loop.py snap glob jen cte.
- watchery: watch_iter_then_f1ab_20260803.sh stash necte ani nepise (log
  potvrzuje); nocni A/B bezi izolovane v ab_run_20260804/.

**Zaver: mechanismus se z perzistentni evidence urcit NEDA.** Jedina
neauditovatelna trida operaci v okne je ad-hoc shell prikaz interaktivni
session (napr. preklep pri priprave ab_run kopii ~13:42; obsah ab_run
weights_policy.json je vsak snapshot cd72ed6b, ne stash, takze stash tam
presunut nebyl — po jeho obsahu neni na disku zadna stopa krome snap e16).

**Systemicnost: NE.** „Policy carry-over" logline (tiskne se JEN kdyz stash
existuje a nese policy_W1) je pritomna ve vsech ocekavanych iteracich:

```
training_gatefix_20260722.log      carry 4/4 (loop 4)
training_postitem7_20260720.log    carry 4/4 (loop 4)
training_gateblend_20260803.log    carry 1/1 (stash z 01.08. prezil 2 dny)
training_gateblend_20260804.log    carry 1/1 (stash z 03.08. prezil noc)
training_noreset_20260731.log      carry 1/4 — KOREKTNI: NO_RESET vetev
                                   carry-over zamerne preskakuje (az_train
                                   uz hlavu nese; run_iteration.py:656-659)
```

Akumulace pres rejecty tedy NEBYLA v zadne overitelne casti historie
prerusena; hypoteza „plateau kvuli systemickemu mazani stashe" PADA.
Ztrata 04.08. je prvni a jedina, a diky obnove tehoz dne + guardu nikdy
nevstoupila do treninku.

### 2.2 Korektnost obnovy — OVĚŘENA

- Extrakce policy klicu ze `weights_snap_e16_99pct_+2.6.json` postupem
  `_stash_policy` (7 klicu policy_*) → md5 **fa7698b8**0a006ecc033e01baedb4e0e7
  == md5 obnoveneho root souboru; obsahova rovnost `snap == root` True.
- `weights_az_train.json` na disku NENI pouzitelny zdroj: mtime 13:33:54 =
  okamzik `git reset --hard` v _git_push, ktery ho vratil na COMMITTED verzi
  (353ebcd „Update weights from Colab", value-only) — extrakce z nej dava
  prazdny dict. Snap e16 (mtime 09:12:29 = finalni epocha) je jediny artefakt
  nesouci dnesni vytrenovanou hlavu → obnova byla korektni i jedina mozna.

### 2.3 Review guardu (ee353c8 + 4c34638) — stačí, s poznámkami

Silne stranky: zaloha kazde verze s casem+md5 (retence 30), self-healing
`_restore_policy_from_backup` je HLASITY, md5 stashe nove v gate_history
(absolutni stopa pro retro mereni), OSError pri hashovani osetren, testy
kryji backup, restore, retenci i archiv metrik.

Poznamky (zadna nevyzaduje okamzitou akci):
1. policy_backups/ je untracked a NEKRYTY .gitignore vyjimkou — pripadny
   budouci `git clean -fdx` (nikde se dnes nevola) by smazal stash I zalohy
   najednou. Levna pojistka: pridat `policy_backups/` do .gitignore
   (explicitni ignore = clean bez -x ho nevezme) nebo zalohy pushovat.
2. Vyber nejnovejsi zalohy je lexikograficky dle nazvu (UTC timestamp) —
   korektni; kolize v sekunde resi md5 suffix, poradi pak neni garantovane
   (prakticky irelevantni).
3. Self-heal bere nejnovejsi zalohu i kdyby _stash_policy predchozi iterace
   selhal PRED zapisem zalohy → obnovi se starsi delta (ztrata 1 iterace,
   ale hlasita a viditelna v md5 gate_history). Prijatelne.
4. Okno stash->gate: kdyby stash zmizel BEHEM gate, kandidatovy gate hry by
   bezely bez policy bez varovani (gate cte soubor per-hra pres cesty).
   Zachyti se az post-hoc pres policy_stash_md5=None v gate_history. Staci.

## Úkol 3 — párový H2H policy hlav (pre-registrováno před spuštěním)

Design: kandidat = weights_policy.json (stash s rejected deltou 04.08., md5
fa7698b8), baseline = weights_best_policy.json (promotnuta, md5 cd72ed6b);
obe strany value = weights_best.json (md5 17578260, guard pred/po), blend
0,2 obe strany, 300 side-swapped paru = 600 her, seedy 35M+ (disjunktni),
MCTS 100, TV 1200, nice 19, Pool(2). Pre-registrovane cteni: decisive WR +
Wilson CI; 0,5 uvnitr CI = SUM. Skript + rows:
diag_policy_vs_policy_20260805/ (run_h2h.py, results.json, run_h2h.log).

```
VYSLEDEK: [MERENI BEZI — doplni se po dobehnuti 600 her]
```
