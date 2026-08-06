# ZADÁNÍ pro Fable agenta: Tempo doktrína cage advance (příští týden, ~10.08.2026)

## Proč (uživatel, 05.08. — ZÁVAZNÉ RÁMOVÁNÍ)

„To, že se spočítá, že trpasličí klec nedojde, nám nemůže znamenat, že
trpaslíci zahodí pokus o skóre TD hned v prvním kole zápasu."

A k riziku (05.08.): **„bez rizika není TD — a pád k 0:0 zpátky."** Hra
vyhýbající se všem hodům končí remízou; doktrína, která neumí vědomě
riskovat, systematicky produkuje 0:0. Data: dwarf mirror v A/B 04.08.
59 % remíz; dlouhodobě vysoká remízovost (draw-collapse historie, šumové
dno draw-rate ±8–11 pp). Úspěch tohoto úkolu se pozná i na POKLESU
remízovosti dwarf her, ne jen na WR.

Dnešní stav: CageAdvancePlanner při requiredPace > achievablePace vrací
TEMPO_INSUFFICIENT a tah spadne na search() = staré chování (solo runy,
klec zůstává stát). Sonda 05.08. (8 her dwarf-skaven, aktuální kód po
přestavbě 04.08.): z 78 relevantních turnů s nositelem míče je
**TEMPO_INSUFFICIENT 53 (68 %), DICEY 17 (22 %), PLAN_READY jen 8 (10 %)**.
Tempo veto je hlavní brzda adoption (A/B 04.08.: 0,34–0,66 plánů/hru).
Plánovač tedy většinou rezignuje na doktrinální postup dřív, než začal —
přesně to, co uživatel odmítá.

## Jádro úkolu: CO má plánovač dělat, když rozvrh nevychází?

ZÁVAZNÝ DESIGN VSTUP (uživatel, 05.08.): **dnešní fallback je nepřijatelný.**
„Návrat ke starému chování, kde nositel utíká z klece sám" NENÍ záložní
plán — je to přesně vada, kterou F1 léčí. Minimální chování tahu, když
posun nevyjde: **DOPLNIT NEÚPLNOU KLEC VŠÍM, CO NA NI DOSÁHNE** (obsadit
volné rohy dostupnými hráči, zpevnit formaci, držet míč). Hierarchie:
posun klece → nejde-li, doplnit/zpevnit klec → nikdy solo útěk nositele.
Poznámka k proveditelnosti: přiřazování hráčů na sloty (tryAssign) i
dice-free REPOSITION exekuce už existují — „cage-fill tah" je jejich
podmnožina bez posunu; rozpracuj jako součást opcí níže.

Design opce z 04.08. (rozpracovat VŠECHNY, s čísly, bez implementace):
(a) **grind**: posouvat klec max dosažitelným tempem i při nesplnitelném
    rozvrhu (soupeř dělá chyby, attrition otevírá cestu; držení míče má
    obrannou hodnotu);
(b) **veto jen při achievable=0** (žádný feasible corner assignment ani
    na step 1) — a i pak platí cage-fill minimum výše;
(c) **eskalace signálu výš**: „tempo nestačí" jako vstup do rozhodnutí
    vyšší úrovně (pass / blitz průlom / committed grind) — ta úroveň zatím
    neexistuje; navázat na blitz taxonomii 4 účelů (project memory
    blitz_review_plan_20260803) a PASS seed (zatím NEROZPRACOVÁVAT sérii).

ZÁVAZNÁ DEFINICE (uživatel, 05.08.): **konec poločasu = konec možnosti dát
TD v tomto drivu.** O přestávce se hra staví ZNOVU (nový výkop) — pozice se
do druhé půle NEPŘENÁŠÍ, vše se přepočítává od nuly. Žádná „dvoupůlová
doktrína" přenosu pozice neexistuje (Claudova chybná hypotéza, škrtnuta).
Největší šance trpaslíků na TD: PŘIJÍMAJÍ výkop na začátku poločasu a mají
celých 8 kol na přesun míče — plán s tím musí počítat.

Šikmé otázky, které MUSÍŠ zodpovědět daty/analýzou (ne názorem):
1. **Rozpad TEMPO vet podle kontextu drivu:** kolik tempo-vet padá v drivu
   od začátku poločasu (8 kol k dispozici — tam by rozvrh vycházet MĚL;
   pokud i tam veto, je chyba ve výpočtu required/achievable, ne v situaci)
   vs. v drivu začínajícím uprostřed poločasu (málo kol — tempo reálně
   nevychází). Pro druhý případ zvaž doktrínu DRŽENÍ MÍČE: mele-li klec
   míč, soupeř nemůže skórovat — grind má obrannou hodnotu i bez šance na
   TD. Kvantifikuj z korpusu: co se dnes děje s drivem, když plán rezignuje
   (ztráta míče? soupeřovo TD?).
2. **Je achievablePace fér?** Rozpad TEMPO případů: kolik jich má
   achievable ≥ 2 (grind by reálně jel) vs achievable ≤ 1 (skutečně
   zaseknuto)? Sonda detailní TEMPO řádky neloguje — PRVNÍ KROK: rozšířit
   diag_f1_adoption_probe.cpp o dump TEMPO turnů (pole v CageAdvancePlan
   už existují: requiredPace/rawAchievableStep/achievablePace/resistance),
   přeměřit na více matchupech (i orc — makro je rasově agnostické).
3. **Kolik by grind reálně dovezl?** Simulační experiment: opce (a) vs
   dnešní fallback na search — párové A/B na dwarf matchupech (vzor
   ab_run_20260804, klidně menší N na první čtení). Metriky: TD, chess
   delta, jak daleko se klec reálně dostane, attrition.
4. **Interakce s DICEY — ZÁVAZNÝ VSTUP (uživatel, 05.08.): v DICEY
   případech je potřeba PŘIDAT VÍCE RISKANTNÍCH TAHŮ pro dokončení TD.**
   „Bylo by moc jednoduché, kdyby se klec jen posouvala dopředu a na konci
   byl TD" — soupeř se brání, dokončení drivu VYŽADUJE vědomé riziko
   (dodge přes TZ, block/blitz na uvolnění cesty/rohu, GFI). Bezrizikový
   plán (SAFE_PTO=0.02) je tedy neúplná doktrína. Navrhni RISK BUDGET
   plánu: které nohy smí být riskantní, s jakými stropy pravděpodobnosti
   selhání, a jak stropy ŠKÁLOVAT s fází drivu (blízko endzóny/konec
   poločasu = vyšší povolené riziko).
   **POŽADAVEK UNIVERZÁLNOSTI (uživatel 05.08.): „bez rizika není TD"
   platí obecně, ne jen pro trpasličí klec.** Risk budget navrhni jako
   JEDEN mechanismus enginu (vstupy: skóre, zbývající kola, fáze drivu,
   hodnota cíle), ze kterého plátky čerpají — klec je jen PRVNÍ zákazník,
   blitz a pass budou další. Dnešní roztroušené konstanty (SAFE_PTO 0.02
   v item10/item13/F1, GFI výjimky) mají skončit pod jednou střechou.
   Vzor: zobecňovací pravidlo blitzových změn z 03.08. — stejná zásada,
   tady pro riziko. Existující stavební kameny — NEDUBLOVAT:
   carrier GFI výjimka (SAFE_PTO_GFI1=0.25/GFI2=0.40) už tenhle princip
   dělá pro jeden typ nohy; item10 risk-deferral řadí riskantní akce na
   konec tahu (Q-guard); corner-release groundwork (evidence/
   fable_corner_release_report_20260804.md) už spočítal block-release a
   blitz-release odhady s reálnými kostkami pro 62 situací; diskuze C
   (clear+shift) v paměti f1_cage_questions_20260804. Koridor ve stínu TZ
   je 11/17 DICEY případů — grind bez řešení koridoru bude jen víc narážet.

## Deliverable

`evidence/fable_tempo_doctrine_report_20260810.md`: (1) datový rozpad TEMPO
verdiktů (otázka 2), (2) simulační srovnání grind vs fallback (otázka 3),
(3) doporučená doktrína s opcemi a/b/c vyhodnocenými proti datům a proti
závazným constraintům (tempo = výpočet, ne konstanta; klec dutá; carrier
GFI jen v nouzi s reálnými hody), (4) formulace finálního rozhodnutí PRO
UŽIVATELE (podklad, ne hotové rozhodnutí). ŽÁDNÁ implementace do enginu
(diag sonda rozšířit smíš — vzor 149b7cc, no behavior change).

## DOPLNĚNO 06.08. (dispatch PŘEDSUNUT na dnešek — tokeny uvolněny, GO uživatele)

- **Deliverable přejmenován: `evidence/fable_tempo_doctrine_report_20260806.md`** (dispatch dnes).
- **Nový kontext PROMOTION FREEZE (06.08., commit 5658f83):** promoce šampiona jsou zmrazeny, dokud trpasličí styl není spraven a zkontrolován — TENTO úkol je první polovina odblokování. Brána nově loguje per-race audit (log + gate_history `per_race_cand/frozen`) — použij jako průběžný datový zdroj.
- **Retro per-race data (diag_race_guard_retro_20260806.py na branách 03.–05.08.):** kdo hraje dwarf, prohrává (decisive WR 18–38 % obě strany, každá brána) — strukturální slabost, kterou doktrína léčí; čísla v denním pointeru 06.08.
- **Item13 KROK 2 má hotový design (evidence/item13_krok2_design_20260806.md, implementace 07.08.):** cage-fill po sebrání (pasivní hlídač → pickup+poposun → rohy přes tryAssign+reservedPlayerIds). „Cage-fill tah" z hierarchie výše tedy NENAVRHUJ od nuly — navaž na tento design (stejná mašinérie, jiný trigger: nevyšlý posun místo sebrání).
- **⚠️ BĚŽÍ trénink (run_iteration.py, verdikt ~13:30 UTC):** žádná těžká měření, dokud běží — ověř `ps -eo args | grep run_iteration`; poté měř s nice -19. Živá (weights_policy.json) se NEMĚNÍ, šampion NEMĚNIT.
- **⚠️ Incident 05.08.: background procesy agenta NEPŘEŽIJÍ jeho ukončení.** Všechna měření dokonči ve své session (výsledky průběžně do souborů); pokud musíš něco nechat běžet, spusť setsid+nohup+disown a do reportu napiš, kde přistane výsledek a jak ho ověřit.

## Omezení

- ~~Dispatch až PŘÍŠTÍ TÝDEN~~ → předsunuto 06.08. (viz výše).
- Měření nice -19; neinterferovat s tréninkem/gate (ověřit, že nic neběží).
- Rozpočet ≤150k tokenů. Šampiona a root weights NEMĚNIT. Žádný git push
  bez zelených testů; sondu commitovat jako diag (vzor 51c1aa0/149b7cc).

## Datová příloha (stav 05.08.)

- Sonda: evidence/diag_f1_adoption_probe_full_20260805.log (8 her dw-sk,
  256 team-turnů; DICEY dumpy + souhrn). Binárka: g++ příkaz v hlavičce
  diag_f1_adoption_probe.cpp.
- A/B: ab_run_20260804/diag_f1_m{0-3}_20260804.log + rows jsonl (3200 ř.);
  adoption distribuce: 27–55 % her s ≥1 plánem dle matchupu; deskriptivně
  WR s plánem >> bez plánu (selekční bias!).
- Kód: engine/src/cage_advance.cpp (from-scratch stavba klece od 04.08. —
  builtCorners jen diagnostika; POZOR hlavička cage_advance.h má zastaralý
  komentář o trigger ≥2 rohů), engine/include/bb/cage_advance.h.
- Kontext: evidence/fable_dwarf_playstyle_gap_20260803.md (g0018 zmrzlá
  klec), evidence/fable_corner_release_report_20260804.md (diskuze A),
  paměť f1_cage_questions_20260804 (fronta diskuzí A/B/C).
