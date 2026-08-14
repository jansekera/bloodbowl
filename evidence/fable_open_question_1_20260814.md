# Otevřená otázka č. 1: Proč zlepšení procesu nevede k výsledku?

**14.08.2026 · Fable analýza.** Korpus `diag_replay_mine_20260813_big_data`
(3000 her, brána OFF, HEAD e4b99ee; 7930 drivů = 4113 přijímacích + 3817
obranných; 2641 plných přijímacích drivů ≥7 kol). Dělení drivů a kategorie
**importovány** z `diag_drive_failure_20260811.py`, kontroly z
`diag_rules_checks_20260812.py` + `diag_exposure_scan_20260812.py` (jediná
definice). SD párové delty spočteno z reálných řádků
`gate_measure_20260813/*_s*/diag_f1_cage_advance_rows.jsonl`, ne z přepočtu.
Očekávání pre-registrována PŘED výpočtem
(`scratchpad/open_q1_20260814/prereg.md`). Skripty a mezidata:
`scratchpad/open_q1_20260814/` (extract_games.py, extract_drive_checks.py,
analyze_ceiling.py, analyze_checks.py, games.jsonl, drive_checks.jsonl).

---

## 0) Verdikt jednou větou

**Proces se nepropsal do výsledku, protože brána zlepšila kontroly, jejichž
korelace s TD je z většiny SELEKCE (proveditelnost drivu), zatímco veličiny,
které kontrolu proveditelnosti přežijí — expozice nosiče (REACH0 počet) a
bloky/kolo — s tempem přímo soupeří (r = +0,21, resp. brána doloženě měnila
bití za tempo); a celá poctivá cílová populace brány (D2, 283 drivů z 3000
her) unese nejvýš +3,9 pp chess.** Výsledek dw-we −2,97 ± 1,45 pp navíc
kladný efekt na 95 % VYLUČUJE — u brány tedy nejde jen o málo párů: brána
skutečně nekonvertovala, a znaménko je konzistentní s tím, že platila
expozicí nosiče v 5× větší populaci C drivů (mechanismus neověřen, viz §6).

---

## 1) Strop účinku (hlavní požadované číslo)

Chess teď: **0,4660** (W/D/L = 661/1474/865). Marže (naše−jejich TD):

| marže | −3 | −2 | −1 | 0 | +1 | +2 | +3 |
|---|---|---|---|---|---|---|---|
| her | 7 | 83 | **775** | **1474** | 594 | 66 | 1 |

**Her „na jeden gól" (remíza nebo prohra o 1): 2249 z 3000 = 75,0 %.**
Jeden konvertovaný drive je proto extrémně drahý: **+0,41 až +0,53 chess
v té hře** (podle scénáře). Strop není v ceně gólu, ale ve VELIKOSTI
populace, kterou daná změna umí zasáhnout:

| scénář (bootstrap po hrách, 2000×) | konvert. drivů | drivů/hru | Δchess [95% CI] |
|---|---|---|---|
| S+1TD: +1 náš TD každou hru (absolutní strop útoku) | 3000 | 1,00 | **+0,375** [+0,367,+0,382] |
| S_full: skóruje každý plný přijímací drive bez TD | 2196 | 0,73 | +0,312 [+0,304,+0,321] |
| S_C: žádná ztráta míče (C→TD, ruší se soupeřův TD z drivu) | 1445 | 0,48 | +0,257 [+0,245,+0,269] |
| S_D1: každý pozdní start skóruje (vyžaduje kickoff-return) | 1715 | 0,57 | +0,250 [+0,240,+0,262] |
| S_Cnoswing: C→TD bez rušení soupeřova TD (konzervativní) | 1445 | 0,48 | +0,208 [+0,198,+0,218] |
| S_def: soupeř neskóruje v žádném obranném drivu | 765 | 0,26 | +0,113 [+0,106,+0,121] |
| **S_D2: každá pomalá klec skóruje = STROP BRÁNY KLECE** | **283** | **0,09** | **+0,039** [+0,034,+0,044] |

Čtení: dokonalá útočná změna má strop +37,5 pp — strop obecně vázající
není. Ale **brána klece cílí na D2 („míč máme, jsme pomalí") a tam je strop
+3,9 pp i při 100% konverzi**. Realistická konverze (brána zvedla plnění
K9a o 7,8 pp posuzovaných kol; převod compliance→konverze drivu neznáme,
což je samo o sobě nález) leží hluboko pod tím. Druhá strana téže mince:
**v 68,9 % her (−1 náš TD) chess ZHORŠÍ** — kanál ceny je stejně velký jako
kanál zisku a vede přes 5× větší populaci C (1445 drivů, ztráty z 84,5 %
„soupeř srazil nosiče").

Po soupeřích: hry na-jeden-gól skaven 57,6 % · wood-elf 73,3 % · human
78,7 % · **ork 90,3 %** — proti orkovi je páka na drive největší a náš
chess nejnižší mimo skavena.

## 2) H5 SELEKCE: prediktory TD z 13.08. jsou z většiny artefakt — replikace na 13× větším n

Plné přijímací drivy, n = 2641 (TD 452); týž kód veličin jako
`diag_drive_predictors_20260813.py`. Tři stupně očištění:

| veličina | celý drive | bez posledního (TD) kola | jen první 3 kola s míčem |
|---|---|---|---|
| tempo (Δx/kolo) | +0,67 (16,2σ) | +0,28 (5,8σ) | +0,39 (6,5σ) |
| bloky/kolo | +0,32 (10,7σ) | **+0,48 (13,9σ)** | **+0,47 (11,0σ)** |
| čisté rohy | −0,05 (−1,5σ) | +0,18 (4,7σ) | +0,06 (1,3σ) |
| REACH0 (počet) | −0,66 (−11,8σ) | **−0,55 (−9,3σ)** | **−0,29 (−4,2σ)** |
| idle těl | −0,20 (−4,0σ) | −0,15 (−2,6σ) | −0,35 (−4,9σ) |

**58 % tempo-rozdílu vyrábí samotné skórovací kolo** (0,67→0,28).
Rozhodující je ale kontrola PROVEDITELNOSTI: need0 = vzdálenost při
1. držení / zbývající kola (co bylo dáno výkopem, ne hrou). První 3 kola
s míčem, bez posledního kola TD:

| koš need0 | n (TD/bez) | tempo | bloky | čisté rohy | REACH0 | FB2 |
|---|---|---|---|---|---|---|
| ≤2,61 (stihnutelné) | 139/523 | **+0,16 (+1,7σ)** | **+0,21 (+2,8σ)** | +0,00 (0,0σ) | **−0,39 (−2,7σ)** | −0,13 (−2,2σ) |
| 2,61–3,5 | 252/1333 | +0,52 (+6,8σ) | +0,54 (+9,6σ) | +0,07 (1,1σ) | −0,35 (−4,1σ) | −0,13 (−2,8σ) |
| >3,5 (nestihnutelné) | 58/333 | +0,74 (+6,0σ) | +0,49 (+4,3σ) | +0,10 (1,0σ) | −0,11 (−0,7σ) | −0,10 (−0,9σ) |

Čtení: **tam, kde byl drive stihnutelný, tempo TD nepředpovídá (+1,7σ).**
Tempo „předpovídá" jen v drivech, které potřebovaly heroické tempo — tam je
korelace mechanická (skórovat mohl jen ten, kdo náhodou letěl). To je přesně
hypotéza 5 (selekce). **Kontrolu proveditelnosti přežívají jen: bloky/kolo,
REACH0 (počet) a slabě FB2.** Čistota rohů nepřežívá vůbec (0σ ve všech
koších) — její 2,6σ z 13.08. bylo zprostředkované (r(čisté, REACH0) = −0,39;
čistý roh je jen proxy ochrany nosiče). Směr u REACH0 podpírá i časová
posloupnost z 12.08. (REACH0=0 na konci našeho kola → ztráta míče
v soupeřově kole 1,8 % vs 8,3 %) a příčiny ztrát C (84,5 % sražený nosič);
společnou příčinu typu „dominance na desce" úplně vyloučit neumím (viz §6).

**Tím je mezera proces→výsledek vysvětlena bez paradoxu: brána zlepšila
K9a a čistotu rohů — dvě veličiny, jejichž prediktivní síla je selekce —
a directional cena šla do bití (doloženo z A/B) a expozice (r-konflikt §3),
tedy do těch dvou veličin, které kauzální filtr přežily.**

## 3) H3 KONFLIKTY: kontroly se navzájem vylučují (n=2638 drivů, bootstrap po hrách, 2000×)

| pár | r | 95% CI | čtení |
|---|---|---|---|
| tempo × čisté rohy | **−0,215** | [−0,257,−0,174] | rychlý postup rozbíjí čistou klec |
| tempo × REACH0 | **+0,205** | [+0,159,+0,256] | rychlý postup EXPONUJE nosiče |
| čisté rohy × REACH0 | **−0,390** | [−0,425,−0,354] | čistý roh = ochrana (proto byl proxy) |
| idle × bloky | **−0,400** | [−0,434,−0,364] | těla buď bijí, nebo stojí |
| čisté rohy × bloky | **−0,101** | [−0,143,−0,060] | na úrovni drivu se bití a čistota NEsčítají (potvrzuje dnešní A2 z fable_dirty_corner_chain: obecné bloky čistotu nezvedají) |
| tempo × bloky | +0,151 | [+0,111,+0,190] | pre-reg čekala zápornou — VYVRÁCENO (viz §7) |
| tempo × FB2 | +0,051 | [−0,009,+0,111] | nic |

Per-turn (tentýž tah, 13 285 kol): r(Δx, čisté)=−0,199 · r(Δx, REACH0)=+0,219
· r(Δx, idle)=−0,149. **K9a (tempo) stojí přímo proti K34/K29 — vynucením
tempa se kupuje expozice nosiče a špinavější klec.** Zbytné/odporující si:

* **K9a jako vynucovaná podlaha — vyřadit z doktríny** (prediktivně selekce,
  konflikt s oběma přeživšími veličinami). Jako *deskriptor* fáze smí zůstat.
* **K29-čistota — zbytná jako cíl**, je to proxy REACH0; měřit rovnou REACH0.
* **K33 (ano/ne blok) — zbytné**, nositelem signálu je POČET bloků (známé).
* **K31 idle — z poloviny duplikát bloků** (r=−0,40); ve stihnutelných
  drivech TD nepředpovídá (−0,3σ).
* Přežívají: **REACH0 (počet), bloky/kolo, FB2 (slabě)**.

## 4) H4 POWER: kolik párů co rozliší (SD z reálných řádků brány)

SD párové delty: dw-sk 0,537 · dw-we 0,561 (n=1500 párů; SE 0,0139/0,0145).

| skutečný efekt | párů na 2σ detekci |
|---|---|
| +0,5 pp | ~46 000–50 000 |
| +1 pp | ~11 500–12 600 |
| +2 pp | ~2 900–3 100 |
| +3,9 pp (= strop brány) | ~760–830 |

**Power ale NENÍ hlavní vysvětlení výsledku brány:** kdyby brána dosáhla
svého stropu (+3,9 pp), 1500 párů by ji vidělo. dw-we −0,0297 ± 0,0145 kladný
efekt na 95 % vylučuje (horní mez CI −0,1 pp) — na wood-elfovi brána
nekonvertovala vůbec, případně platila. U dw-sk (+0,8 ± 1,4 pp, horní mez
+3,6 pp) naopak nejde vyloučit skoro nic — tam je to čistě málo párů. Power
je zato zábranou pro VŠECHNY budoucí změny této třídy: realistický efekt
útočné procesní změny (konverze části D2 ≈ +0,5 až +2 pp) potřebuje
3 000–50 000 párů, které nemáme; proto „měřit v měně drivů", §5.

## 5) H2 POKRYTÍ + co měřit místo toho

* Ofenzivní kontroly se posuzují ve **39,7 % kol** (K9a 17 531/44 177);
  míč na konci kola máme v 55,7 % kol.
* Z přijímacích drivů je **43,9 % rozhodnuto už výkopem** (D1 krátký 1700 +
  B 105 z 4113) — žádná kontrola uvnitř drivu je nezachrání.
* Obranná polovina hry (48 % drivů, soupeřových 763 TD, strop +11,3 pp =
  3× strop brány) nemá kontrolu žádnou.
* Kategorie ex post (plné drivy, bez posl. kola TD): C má REACH0 2,60 vs
  A 1,64 — ztrátové drivy jsou přesně ty s exponovaným nosičem. D2 má
  tempo 1,61, ale REACH0 1,77 v normě — pomalá klec nosiče chrání.

**Co měřit místo dosavadních kontrol (konkrétní veličiny):**

1. **Expozice nosiče: REACH0 jako POČET na konci každého našeho kola
   s míčem** (průměr/drive; A-úroveň ≈ 1,6, C ≈ 2,6). Jediný prediktor,
   který přežil kontrolu proveditelnosti, má časovou posloupnost k ztrátě
   a míří na největší dosažitelnou populaci (S_C strop +21 až +26 pp).
2. **Bloky/kolo jako počet** (přežily: +2,8σ i ve stihnutelných drivech).
3. **A/B měřit v měně drivů, ne kontrol:** per rameno logovat podíl
   kategorií (C-rate, D2-rate, D1-rate, A-rate) a příčinu ztráty. Převod je
   pak přímý: Δchess ≈ 0,42 × Δ(konvertované drivy/hru) — posun o 0,05
   drivu/hru = +2,1 pp. Compliance kontrol tenhle převod nemá, což je
   root-cause dnešní otázky.

## 6) Co jsem NEZMĚŘIL a proč (povinné)

1. **Mechanismus záporného dw-we** (brána → vyšší expozice/C-ztráty):
   `gate_measure_20260813` nese jen chess řádky, žádné replaye. Rozhodl by
   sběr ~500 her s bránou ON a tímto diag nad nimi (REACH0, C-rate per
   rameno) — sběr teď spouštět nesmím (běží korpus) a
   `diag_replay_mine_20260814_handoff2_data` je zakázané číst. Odhad
   velikosti: na −3 pp stačí +0,07 ztraceného drivu/hru ≈ +14 % C-ztrát.
2. **Společná příčina u REACH0→TD** (dominance na desce může vyrábět nízké
   REACH0 i TD zároveň). Stratifikoval jsem jen need0; plná kontrola
   (odpor koridoru, počet soupeřů na nohou) by chtěla regresi — dnešní
   analýza špinavých rohů (A1: adresný blok → −22,9σ) ale ukazuje, že
   ochranné mechanismy jdou manipulovat cíleně, takže test zásahem je
   proveditelný.
3. **Obranné prediktory** (co předpovídá soupeřův TD v našich obranných
   drivech) — mimo rozsah dne; strop +11,3 pp říká, že je to třetí
   největší páka po S_C a S_D1.
4. **Clusterové σ u need0 košů** — drive-level řádky jsou ~1–2 plné
   drivy/hru, závislost uvnitř hry je malá; bootstrap po hrách jsem dal
   jen korelacím. U hraničních hodnot (±2σ v koši ≤2,61) ber ±0,5σ rezervu.
5. **9 TD-flag anomálií** z drives.txt (0,3 %) — ponechány, na agregáty
   nemají vliv.

## 7) Pre-registrace vs výsledek (`scratchpad/open_q1_20260814/prereg.md`)

| očekávání | dopadlo |
|---|---|
| E1 hrubý strop ≈ +0,32 (proher o 1 ~480) | +0,375; proher o 1 bylo 775 — podceněno, směr správně |
| E2 realistický strop brány <2 pp, hlavní vysvětlení | strop je +3,9 pp (víc, než jsem čekal) a **plný strop BY 1500 párů vidělo** — power sám o sobě bránu nevysvětluje, dw-we kladný efekt aktivně vylučuje. Hlavní vysvětlení se posunulo na E6+E4-konflikty |
| E3 kontrolami ovlivnitelných <25 % přijímacích drivů | ŠPATNĚ: s míčem je ~56 % drivů; výkopem rozhodnuto „jen" 43,9 % |
| E4 tempo×bloky záporná | **ŠPATNĚ: +0,15** (rychlé drivy i bijí — obě stojí na iniciativě); tempo×expozice a idle×bloky správně; čisté×bloky čekáno kladně, je −0,10 |
| E5 power ~11k párů na 1 pp | potvrzeno (11,5–12,6k) |
| E6 tempo v prvních 3 kolech spadne pod 2σ | surové nespadlo (6,5σ — špatně odhadnutý mechanismus: confounder je proveditelnost, ne pozdnost); PO kontrole need0 spadlo na +1,7σ — jádro hypotézy potvrzeno |
| E7 verdikt „malá populace × šum" | zpřesněno: primárně SELEKCE u optimalizovaných kontrol + konflikt s přeživšími veličinami; populace (+3,9 pp strop) a šum jsou zesilovače, ne příčina |

---
*Navazuje na: gate_result_and_paradox (13.08.), what_predicts_td (13.08.),
exposure_reach0 (12.08.), fable_dirty_corner_chain (14.08. — nezávisle
potvrzuje, že obecné bloky čistotu rohů nezvedají). Definiční zdroje beze
změn; engine, tréninky ani sběr nedotčeny.*
