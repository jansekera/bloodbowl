# AlphaZero metodika vs. naše pipeline — report (06.08.2026, dispatch 05.08.)

Zadání: evidence/fable_alphazero_methodology_20260806.md. Povinné čtení
splněno: evidence/fable_postpromotion_report_20260805.md. Terminologie:
„AlphaZero" = výhradně DeepMind 2017 (Silver et al., arXiv 1712.01815 /
Science 2018); náš systém = „naše AI".

## ZÁVĚRY (napřed)

1. **Otázka 5 (jádro): AlphaZero problém „proti nedávnému sobě mám jen ~50 %"
   NEŘEŠÍ — ROZPOUŠTÍ ho.** Nikdy se neptá „porazil kandidát šampiona?";
   ~50 % proti nedávnému sobě je OČEKÁVANÝ ustálený stav (malá změna vah na
   krok → H2H vs nedávné já je z principu mince). Pokrok čte jinde: monotónní
   Elo křivka proti FIXNÍM kotvám — poolu starších checkpointů a externímu
   měřítku (Stockfish/Elmo). Hypotéza uživatele „po každém skoku návrat
   k šumu" je s tímto pohledem plně konzistentní; navíc náš skok 03.08. byl
   strukturální (asymetrie policy 0,2 vs 0,0, report 05.08.), ne učení, takže
   symetrický bod 04.08. (50,3 %) je přesně predikované chování. Přenositelný
   recept: gate vs aktuální šampion = POUZE anti-regrese; pokrok měřit kotvami
   — A3-2 (běží) je první bod přesně této řady.
2. **Náš vzor je AlphaGo Zero (gating 55 %/400 her, self-play šampionem),
   ne AlphaZero.** AlphaZero gate ZRUŠILA a hraje vždy nejnovější sítí —
   funguje to díky měřítku (šachy: ~44M partií, 700k kroků, batch 4096,
   ~5000 TPU na self-play; regrese se statisticky samoopraví v proudu dat).
   U nás (~640 her/iterace, MCTS-100, malé sítě na CPU) by špatná promoce
   otrávila self-play data na dny → **gate si na našem měřítku obhájíme**;
   bezgatový režim máme beztak už empiricky rozehraný (noreset větev) a
   dnešní A3-2 změří, jestli driftuje, nebo sbírá zisky.
3. **Náš nejtvrdší rozpor s receptem AlphaZero není opponent selection, ale
   učicí výtěžek:** per-epoch policy metriky jsou ploché (loss ~1,99, top1
   ~42 %, 5 iterací; report 05.08. §1b). AlphaZero učení = fit na (π, z);
   když fit neroste, víc self-play/lepší soupeři nepomohou. Priorita fronty
   (item13 trénink GO, dwarf regrese −17 pp, featury/kapacita) tím zůstává
   NAD ligou/opponent-diversity — league (AlphaStar) řeší cyklení/exploity,
   na které zatím nemáme jedinou evidenci.
4. **Nejlevnější okamžitý přenos: benchmark tier.** Saturovaný benchmark vs
   random (99–100 %) dnes šumem přepíná práh gate 52,6/53,9 % — AlphaZero
   žádný takový nízkosignální vstup nemá. Nahradit vstup tieru HtH vs kotva
   (A3-2 harness ho už umí) = doporučení 05.08. §1e-3, zde potvrzeno
   literaturou.

## 1. Odpovědi na 5 otázek uživatele

**1. Jak se AlphaZero učí?** Jedna síť f_θ(s)→(p, v); ztráta
(z−v)² − π·log p + L2. Cíle: π = distribuce návštěv MCTS (800 simulací/tah,
u nás 100) — tj. „destilace vyhledávání do sítě", z = výsledek partie.
Self-play VŽDY nejnovějšími vahami, exploraci zajišťuje Dirichlet šum
v kořeni (šachy α=0,3, ε=0,25 — my máme, 2ff47ca) + teplotní vzorkování
úvodních tahů. Trénink asynchronně z bufferu nejnovějších partií. My:
stejný princip, ale vf_blend míchá síť s heuristikou a policy_blend=0,2
míchá policy s heuristickými priory — AlphaZero žádnou heuristickou
oporu nemá (a nepotřebuje, protože má 44M partií na to, aby se ji naučila).

**2. Jak pozná, že postupuje?** DVĚ nezávislé roviny, obě mimo „50 % proti
sobě": (a) průběžné Elo z turnajů checkpointů proti poolu STARŠÍCH
checkpointů (relativní Elo křivka — monotónní růst = učení), (b) periodické
zápasy proti fixnímu externímu měřítku (Stockfish/Elmo, 1 s/tah). Ani jedno
neslouží k selekci vah — jen k monitoringu. Třetí signál je loss/accuracy
křivka tréninku. My máme (c) — a je plochá; (a) právě vzniká (A3-2, kotva
b426c64d); (b) nemáme (benchmark vs random je saturovaný ekvivalent).

**3. Jak si vybírá protivníky?** Nijak — vždy nejnovější já, obě strany.
Žádný pool, žádný sampling. Diverzitu dodává šum+teplota, ne soupeři.
Kontext, proč to jde: obrovská propustnost dat; AlphaGo Zero (Nature 2017)
předtím hrála self-play NEJLEPŠÍ (gated) sítí — ablace v AlphaZero ukázala,
že gate je při jejich měřítku zbytečný. Opponent-diversity systémy jsou až
odpověď na netransitivitu v jiných hrách: AlphaStar league (main agents
hrající PFSP proti celé lize + main exploiters na aktuální main + league
exploiters + zamrzlé checkpointy; pokrok = interní Elo + lidský žebříček),
OpenAI Five (80 % nejnovější já / 20 % pool minulých já; TrueSkill).

**4. Co z toho nám chybí?** Gap tabulka níže. Souhrn: nechybí nám mechanika
učení (máme týž (π, z) recept v malém), chybí nám (i) kotevní měření pokroku
nezávislé na pohyblivé laťce — VE FRONTĚ (A3-2 dnes, A3-1 worktree), (ii)
informativní externí měřítko místo saturovaného benchmarku — navrženo
05.08., (iii) učicí výtěžek na iteraci (~0 pp/iter vs potřebných ~+5 pp na
80% průchod gate, power 30.07.) — to žádný AlphaZero trik nespraví, to je
featury/kapacita/item13.

**5. Jak řeší „proti nedávnému sobě jen malý pokrok"?** Viz Závěr 1.
Doplnění mechaniky: při malých krocích je očekávané H2H vs nedávné já
50 % + ε, kde ε je pod rozlišovací schopností stovek her (naše σ ~2,6 pp na
N=600). AlphaZero proto (a) selekci úplně zrušila, (b) pokrok čte z KUMULACE
ε přes kotvy: proti 10 iterací staré kotvě už je signál 10ε a měřitelný.
Přesně to je hypotéza „zahazovaných malých zisků" (29.07.) a přesně to
rozhodne A3-2: rostoucí řada WR noreset1→4 vs jul28 kotva = ε>0 (akumulace
existuje, gate ji zahazuje); plochá řada = ε≈0 (plateau reálné, problém je
výtěžek učení, ne měření). Průběžný stav dnes večer (kotva sama proti sobě
= sanity 0,500 přesně; noreset1 0,536 CI [0,485–0,587]; noreset2 0,496;
noreset3 běží) zatím ukazuje spíš plochou řadu — ale champion rameno
(policy 0,2) teprve přijde a per fairtest by mělo dát ~+4–6 pp.

## 2. Gap tabulka

| Oblast | AlphaZero (2017) | AlphaGo Zero | Naše AI dnes | Ve frontě | Chybí úplně |
|---|---|---|---|---|---|
| Učicí signál | (π, z) z MCTS-800, čistá síť | (π, z) z MCTS-1600 | (π, z) z MCTS-100 + vf_blend heuristika + policy_blend 0,2 priory | item13 staged planner do self-play dat (GO čeká) | — (princip shodný) |
| Metrika pokroku | Elo vs pool checkpointů + externí kotva (Stockfish) | gate 55 %/400 her | gate HtH vs frozen šampion (práh 52,6–53,9 %) + benchmark vs random (saturovaný) | **A3-2 kotvy (běží dnes)**, policy_backups+stash md5 (retro páry), metrics_archive | průběžná Elo křivka přes pool checkpointů jako STANDING instrumentace |
| Výběr soupeřů | vždy nejnovější já | šampion (gated) | šampion (reset-on-reject) + policy carry-over přes rejecty | A3-1 frozen-anchor league patch (worktree agent-ae897a8fe179d2dc3, NEMERGOVANÝ, rozhodnutí 03.08. platí) | PFSP/exploiters (AlphaStar) — bez evidence netransitivity nepotřebné |
| Gating | ŽÁDNÝ | 55 % práh | statistický práh řízený šumem saturovaného benchmarku | výměna tier vstupu (05.08. §1e-3) | — |
| Explorace | Dirichlet + teplota | Dirichlet + teplota | Dirichlet seed diversity (2ff47ca) | — | audit teplotního schedule v self-play (levný check) |
| Měřítko | ~44M partií, 700k kroků, ~5000 TPU | 4,9M partií | ~640 her/iter, CPU, malé sítě | — | nepřenositelné — kompenzujeme gatem a kotvami |
| Fit monitoring | loss klesá (jinak alarm) | dtto | **loss/top1/H ploché 5 iterací** | dwarf regrese −17 pp, featury/kapacita (plateau 17.07.) | tady je skutečný blok — žádná metodika ho neobejde |

## 3. Doporučení (podklad k rozhodnutí, seřazeno proti EXISTUJÍCÍ frontě)

Žádná nová fronta; jde o povýšení/potvrzení stávajících položek:

1. **POVÝŠIT: kotevní instrumentaci (A3-2) z jednorázového měření na
   standing metriku** — po každé promoci H2H vs PŘEDCHOZÍ šampion + periodicky
   vs fixní kotva (cd72ed6b / b426c64d), tj. doporučení 05.08. §1e-1, nyní
   podepřené literaturou (je to přímo AlphaZero recept na otázku 5).
   Přenositelnost: STOJÍ beze změny — je to měření, ne trénink; cena ~1 běh.
   Dnešní A3-2: rostoucí řada ⇒ instrumentace nutná (gate zahazuje reálné
   zisky); plochá řada ⇒ instrumentace stále užitečná, ale těžiště se
   přesouvá na výtěžek učení (bod 3).
2. **POVÝŠIT: výměnu benchmark-tier vstupu** (05.08. §1e-3) — nahradit
   saturovaný benchmark vs random HtH proti kotvě (harness existuje), nebo
   tier zafixovat. Přenositelnost: STOJÍ (AlphaZero analogicky měří proti
   informativnímu externímu měřítku). A3-2 čísla rovnou dají kalibraci
   nového vstupu (rozptyl WR vs kotva na N=600).
3. **POTVRDIT pořadí fronty: učicí výtěžek PŘED opponent-diversity** —
   item13 trénink GO, dwarf −17 pp, featury/kapacita zůstávají nad A3-1
   ligou. Zdůvodnění z literatury: AlphaZero pokrok = klesající (π, z) loss;
   náš je plochý, takže úzké hrdlo není soupeř, ale signál/kapacita. League
   (AlphaStar vzor) povýšit AŽ při evidenci cyklení (např. A3-2/kotvy ukáží
   netransitivitu: A>B, B>C, C>A) — zatím žádná. A3-1 worktree držet
   nemergovaný dle rozhodnutí 03.08.
4. **GATING: ponechat (AlphaGo Zero režim), s podmíněným výhledem.**
   Přechod na AlphaZero režim bez gate PADÁ na měřítku: samoopravnost
   regresí vyžaduje propustnost dat, kterou nemáme; u nás by regrese žila
   dny v self-play datech. Podmínka přehodnocení: pokud A3-2 ukáže, že
   noreset ramena (bezgatová akumulace) jsou ≥ control BEZ degradace a
   ideálně rostou, je bezgatový/hybridní režim (gate jen jako hard-reject
   pojistka proti propadu, ne selekce) reálný kandidát — rozhodnutí
   uživatele. Průběžná čísla (noreset1 0,536, noreset2 0,496) zatím
   neukazují degradaci ani jasný růst.
5. **NEZAKLÁDAT: PFSP/exploiter ligu, zvětšování MCTS budgetu jako cestu
   k pokroku** — MCTS-400 proba (21.07.) snížila entropii, ale bez důkazu
   zlepšení; AlphaZero škálové triky (obří batch, buffer, no-gate)
   nepřenositelné. Drobný levný audit: teplotní schedule v self-play
   (AlphaZero vzorkuje úvodní tahy z π, pak argmax) — jestli náš self-play
   generuje dostatečně diverzní zahájení, souvisí s plochým mcts_H 0,88.

## 4. Návaznost na report post-promotion dynamiky (05.08.)

- Skok 03.08. = strukturální asymetrie (policy 0,2 vs 0,0), ne učení —
  literatura to rámuje přesně: jednorázové zapnutí komponenty je „schod",
  učení by se projevilo SKLONEM kotevní křivky, ne schodem gate binárky.
- Úkol 3 (policy-vs-policy cd72ed6b vs stash fa7698b8): běh ZEMŘEL na
  240/600; částečná data (results_partial_240_20260805.json): decisive WR
  0,482, CI [0,407–0,558] → **PŘEDBĚŽNĚ ŠUM** (0,5 uvnitř CI), tj. rejected
  delta 04.08. se od promotnuté hlavy na 240 hrách neodlišuje — konzistentní
  s plochými per-epoch metrikami. Pozoruhodný předběžný detail: dwarf
  0,286 [0,163–0,451] (stash delta trpaslíkovi dál škodí). Čistý re-run
  600 her poběží dnes večer — čísla odtud NEPOUŽÍVAT jako finální.
- Rozhodovací rámec 05.08. §1e (≥3 post-promotion body, kotvy místo
  pohyblivé laťky, power ~+5 pp) tento report potvrzuje z literatury a
  nemění; A3-2 je jeho první exekuce.

## Zdroje vzorů

Silver et al. 2017 „Mastering Chess and Shogi by Self-Play…" (AlphaZero,
arXiv 1712.01815; Science 362, 2018) — no-gate, 44M partií, Elo vs
checkpointy+Stockfish. Silver et al. 2017 „Mastering the game of Go without
human knowledge" (AlphaGo Zero, Nature 550) — 55% evaluator, self-play
šampionem. Vinyals et al. 2019 (AlphaStar, Nature 575) — league/PFSP/
exploiters. Berner et al. 2019 (OpenAI Five) — 80/20 past-self pool,
TrueSkill. Jaderberg et al. 2017 (PBT) — populace, exploit/explore.
