# FABLE 17.08. — NABÍDNUTO vs ZAHRÁNO: KTERÁ MAKRA JSOU MRTVÁ, A KDE

*(Fable 5, 17.08.2026; korpus `diag_replay_mine_20260814_dauntless_data/`,
3 000 her, engine e273a369, DAUNTLESS=1, CAGE_GATE=0; oba týmy `macro_mcts`,
100 iterací, policy net načtená, `policy_blend=0.0` ⇒ heuristické priory.
Skripty: `diag_fable_offered_played_20260817.py`,
`diag_fable_deadmacro_detail_20260817.py`.)*

## ⛔ NÁLEZ Č. 0, KTERÝ MĚNÍ ZADÁNÍ: „0 hand-offů za 3 000 her" JE ARTEFAKT LOGU

`bb_module.cpp:325` serializuje event stráží `typeIdx < 21`, ale pole
`eventNames` má **22 položek** — `HAND_OFF` na indexu 21. Commit `3b11d33b`
(14.08., „a hand-off leaves a trace of its own") přidal jméno do pole, **ale
nezvedl stráž** (`499b8508` ji předtím zvedal z 20 na 21 kvůli EJECTED).
⇒ **Každý hand-off se do python logu zapisuje jako `"UNKNOWN"`.**

V korpusu je **349 UNKNOWN událostí / 3 000 her** a všech 349 má vzdálenost
dárce→příjemce ≤ 1 pole, cíl je spoluhráč a následuje CATCH (příp. SKILL =
reroll chytání). Jsou to hand-offy:

| | počet | jmenovatel |
|---|---|---|
| hand-offy **naše** (trpaslík) | **130** (0,043/zápas) | 3 000 her, 48 000 našich kol |
| … z toho chycené | 109 ze 130 | |
| … z toho příjemce v témž kole skóruje | **51 ze 130** | |
| hand-offy soupeřů | 219 (0,073/zápas) | 3 000 her |

⇒ **Ústřední tvrzení P21 („nabídne se 10,4×/zápas a za 3 000 her se nezahraje
ANI JEDNOU") je vyvrácené: hand-off se hraje, jen ho log přejmenoval.**
Evidence `handoff_offered_never_chosen_20260817.md` §(b) kontrolovala poziční
mapu (`bb_module.cpp:233` — správně), ale stráž o 8 řádků níž ne. Závěr §(c)
„vada se přesunula z NABÍDKY do VOLBY" tím **částečně padá** — viz §4.

*(Sedmý výskyt vzorce „nástroj měří něco jiného, než tvrdí" — tentokrát ne
filtr vs resolver, ale jméno vs stráž v serializátoru.)*

## 1. Tabulka: nabídnuto × zahráno, všech 14 typů

**Nabídnuto** = rekonstrukce brány `getAvailableMacros`
(`macro_actions.cpp:285–1036`, s `dauntlessInOffer=true`) nad snímkem ZAČÁTKU
každého z **48 000 našich kol** (3 000 her × 16 kol). ⚠️ Snímek je začátek
kola ⇒ všechna „nabídnuto" jsou **PODLAHA**: stav vzniklý až během kola
(soupeř sražen → FOUL, míč upuštěn → PICKUP, nosič dojde k příjemci →
HAND_OFF) se nezapočítá. Proto smí být poměr zahráno/nabídnuto i > 1.

**Zahráno** = klasifikace `turn_logs[].events[]` týchž kol (BLOCK event bez
předchozího pohybu útočníka = BLOCK; s pohybem = BLITZ; TOUCHDOWN přiřazen
rodině podle toho, jak střelec přišel k míči).

| makro | kol s nabídkou (z 48 000) | % kol | kandidátů/kolo | zahráno (identifikovatelně) | verdikt |
|---|---|---|---|---|---|
| END_TURN | 48 000 | 100 % | 1,00 | 48 000 (každé kolo končí) | — |
| REPOSITION | 47 024 | 97,97 % | 5,42 | z eventů NEROZLIŠITELNÉ (samé MOVE) | žije (nepřímo) |
| BLITZ | 46 365 | 96,59 % | 1,38 | 27 442 bloků-po-pohybu (57,2 % kol) | **žije** |
| BLOCK | 35 263 | 73,46 % | 2,05 | 47 331 bloků na místě (0,99/kolo) | **žije** |
| FOUL | 31 541 | 65,71 % | 1,95 | 11 175 (23,3 % kol) | **žije** |
| CAGE | 19 885 | 41,43 % | 0,41 | nerozlišitelné (MOVE) | ? (nepřímo žije) |
| ADVANCE | 18 291 | 38,11 % | 0,38 | nerozlišitelné (MOVE) | ? (nepřímo žije) |
| PICKUP | 7 189 | 14,98 % | 0,24 | 8 812 pickup hodů (18,4 % kol; >nabídka, protože míč padá i BĚHEM kola) | **žije** |
| SCORE | 1 614 | 3,36 % | 0,034 | 1 067 TD došlapem (66 % nabídkových kol) | **žije** |
| BLITZ_AND_SCORE | 1 123 | 2,34 % | 0,023 | **2 TD** přes vlastní blitz nosiče | **☠ MRTVÉ — VOLBA** |
| PASS_ACTION | 547 | 1,14 % | 0,013 (401 hand-off + 246 throw kand.) | 41 přihrávek + 130 hand-offů | throw: skomírá; hand-off: **žije** (viz §4) |
| PASS_SCORE | 472 | 0,98 % | 0,010 | **0 TD po přihrávce** (celkem jen 41 hodů, viz níže) | **☠ MRTVÉ — VOLBA** |
| CHAIN_SCORE | 270 | 0,56 % | 0,006 | **0** řetězů (0 kol s PASS i HAND_OFF zároveň) | **☠ MRTVÉ — PROVEDENÍ (P4)** |
| HAND_OFF_SCORE | 256 | 0,53 % | 0,007 | 51 TD po hand-offu | **žije** (konverze ~20 % nabídkových kol, a nabídka je podlaha) |

Křížová kontrola: nabídka hand-off swapu (PASS_ACTION, dist=1) vyšla
**329 kol / 218 her** — přesně čísla z `diag_handoff_situation_20260817.py`,
rekonstrukce brány tedy sedí na dřívější nezávislý výpočet.

**Co jsem ze snímku nedokázal spočítat** (piš u každé rekonstrukce):
`lostTacklezones` (TakeRoot — má jen soupeřův Treeman, naše nabídky
neovlivní); `movementRemaining` (vzato = MA, na začátku kola pravda);
`hasActed/hasMoved/passUsed/blitzUsed/foulUsed` (= false, na začátku kola
pravda — makra generovaná PO jiném makru v témž kole nevidím, další důvod,
proč je „nabídnuto" podlaha); přesné pořadí kandidátů BLITZ (top-1/top-2 —
počítám počet, ne identitu cíle). „Zahráno": blitz z už-sousedního pole se
klasifikuje jako BLOCK (podhodnocuje BLITZ); PICKUP event nerozliší makro
od šlápnutí na míč; Frenzy druhý blok se počítá dvakrát.

## 2. Mrtvá makra — a KDE je vada (NABÍDKA vs VOLBA vs PROVEDENÍ)

### ☠ CHAIN_SCORE — vada v PROVEDENÍ, bug P4 ŽIJE
Nabídnuto 270 kol z 48 000 (0,09 kola/zápas). Kol, kde tentýž tým v jednom
kole hodil PASS **i** HAND_OFF: **0 z 48 000** — přesně, jak P4 predikuje.
Řetěz je v kódu **nemožný**: `pass_handler.cpp:144` nastaví
`passUsedThisTurn=true` při kroku 1 a `rules_engine.cpp:99` generuje HAND_OFF
jen při `!passUsedThisTurn` ⇒ `expandChainScore` (macro_actions.cpp:1621–1630)
v kroku 2 hand-off akci nikdy nenajde a makro tiše degraduje na „přihraj
relayi a dojdi k scorerovi". Vada NENÍ v nabídce ani ve volbě — **i kdyby si
ho search vybral, řetěz se nestane.** (Vedle toho je to i parity chyba: CRP
povoluje 1 Pass akci A 1 Hand-off akci za kolo — engine je slévá do jednoho
flagu.)

### ☠ BLITZ_AND_SCORE — vada ve VOLBĚ
Nabídnuto 1 123 kol z 48 000; z toho 334 kol se současnou nabídkou SCORE
(tam se právem vybírá došlap: 100 TD ze 334 kol). Zbývá **789 kol, kde je
B&S jedinou přímou cestou na TD** (0,26 kola/zápas): nosič v nich blitzoval
jen **38× (4,8 %)** a TD padl **7× (0,9 %)**. Podpis makra (nosič se pohne a
sám hodí blok) se skoro nikdy neobjeví ⇒ makro se nevybírá; kde se vybere,
konvertuje ~18 % (7/38). Nabídka funguje, provedení existuje — **search ho
nebere.**

### ☠ PASS_SCORE (a throw-větev PASS_ACTION) — vada ve VOLBĚ, možná RACIONÁLNÍ
Nabídnuto 472 kol z 48 000; TD po přihrávce **0**. Celkem naše strana hodila
za 3 000 her jen **41 přihrávek** (24 se success flagem), z toho 34 v kolech
5–8 půle (profil `emergency` klauzule brány) a 24 od Runnera. Pozor na
interpretaci: trpasličí AG3→AG3 přihrávka ≈ 0,50×0,67 = 33 % kompletace —
listová evaluace ji oceňuje reálnými kostkami a **odmítat ji může být správná
hra**. Mrtvost PASS_SCORE je fakt; že je to chyba, fakt není. (Náprava, pokud
vůbec, patří do doktríny „trpaslík hází jen v nouzi" — a tu už brána vynucuje.)

### ✔ HAND-OFF (PASS_ACTION dist=1 i HAND_OFF_SCORE) — NENÍ mrtvý (oprava P21)
130 zahraných hand-offů; kumulace v kolech 6–8 obou půlí (79 ze 130) a 51×
z nich příjemce hned skóruje. **Search si hand-off bere tehdy, když je zisk
viditelný v horizontu** — TD teď, nebo `carrierMA`/`scoringThreat` feature
(feature_extractor.cpp:195–197: nosičovo MA a „MA ≥ dist" JSOU featury).
**„Swap kvůli lepším rukám" se nebere skoro nikdy** — a mechanismus je vidět
v listové evaluaci: **AG nosiče featurou není** (jen týmový průměr AG, který
se hand-offem nezmění). Hand-off „na lepší ruce" tedy z pohledu VF nezmění
stav (míč ±1 pole), ale v expanzi přidá reálný hod na chycení (17–33 % fail)
⇒ Q(hand-off) < Q(nedělat nic) vždy, když nehrozí okamžitý TD. To je
zpřesněný závěr místo P21: **ne „nikdy se nevezme", ale „vezme se právě a jen
tehdy, když ho vidí hodnotová funkce".**

## 3. CHAIN_SCORE / P4 — zvláštní pohled (viz §2)

Potvrzeno dvojitě: (a) kódem — flag `passUsedThisTurn` sdílený mezi PASS a
HAND_OFF generací (`rules_engine.cpp:75` vs `:99`), (b) korpusem — 0 kol
z 48 000 s oběma eventy. Strop opravy: 270 nabídkových kol / 3 000 her =
**0,09 kola/zápas** (podlaha; + legální kombinace pass+handoff mimo
CHAIN_SCORE, které dnes flag zakazuje taky). I s velkorysou konverzí ~30 %
je to ≤ 0,03 TD/zápas. **P4 nechat v knize, ale s nízkou prioritou — strop
si na velkou opravu nevydělá.**

## 4. Prior: JE plochý? (P10a)

Ne — a P10a míchá dvě různé tabulky. „BLOCK 15" je `greedyMacroRank`
(`macro_mcts.cpp:37–54`), který se používá **jen v leaf-lookahead bonusu**,
ne při výběru. Skutečné priory (`expand()`, macro_mcts.cpp:408–547; aktivní,
protože policy net je načtená a floors block běží při `config_.policy != null`)
ploché nejsou:

| typ | floor/cap |
|---|---|
| SCORE-rodina (vč. B&S, HOS, PASS_SCORE, CHAIN) | floor 0,70–0,90 (poslední kolo) / 0,50 (ztráta 2+) / 0,35 (≤2 kola) / 0,20 (≤4) / 0,08 jinak; **cap 0,02 při vedení a >2 kolech do konce**, cap 0,05 v 1. kole |
| ADVANCE / CAGE / BLOCK (za kandidáta) | floor 0,12 |
| BLITZ | floor 0,20 obrana / 0,12 útok |
| PICKUP | floor 0,20–0,35 (druhý picker ×0,5) |
| REPOSITION | floor 0,08 jen obrana |
| FOUL | cap 0,08 jen obrana |
| END_TURN | cap 0,10 |
| **PASS_ACTION** | **nic — jediné útočné makro bez flooru** |

Drží plochý/nízký prior mrtvá makra pod vodou? **Pro každé jinak:**

* **PASS_ACTION (hand-off):** jediný kandidát bez flooru; na typickém
  útočném uzlu (n≈13, ~6 floored dětí) vyjde po renormalizaci ~0,05–0,06.
  Se 100 iteracemi a Dirichletem (w=0,25) to je ~5 návštěv — **málo na
  objevení odloženého zisku, dost na výběr, kdyby Q bylo lepší**. Jenže Q
  lepší není (chybějící AG featura, §2) ⇒ **primární brzda je listová
  evaluace, prior je až druhá**. Samotný floor by hand-off nevzkřísil.
* **B&S / PASS_SCORE / CHAIN:** floory MAJÍ (0,08–0,35 podle času). Mrtvost
  B&S tedy prior nevysvětluje — a navíc **cap 0,02 při vedení** trefuje
  přesně trpasličí plán 1‑0: jakmile vedeme, každé skórovací makro má prior
  2 % — což je záměrná doktrína (negativní rezerva), ne bug; ale znamená to,
  že B&S je potlačené dvakrát: capem při vedení a listovou evaluací jinak.
* **Závěr:** „plochý prior" jako univerzální vysvětlení NEPLATÍ. Jediné
  floor-less makro je PASS_ACTION a i u něj je hlavní vrah Q, ne prior.

## 5. Doporučení (JEDNO) + strop

**Opravit `bb_module.cpp:325`: stráž `typeIdx < 21` → odvodit z velikosti
`eventNames` (a přidat statický assert proti enum).** Jednořádková oprava.

* **Strop v herním výsledku: 0** — je to čistě log. Ale strop je špatná
  metrika pro měřicí nástroj: tahle jedna stráž už **vyrobila nepravdivý
  doktrinální závěr** (P21 „hand-off se nikdy nevezme" → „vada je ve VOLBĚ,
  patří k P15/P10a") a znehodnocuje **každé** budoucí měření hand-offů —
  včetně vyhodnocení všech tří commitů ze 14.08., které teď v baseline sedí
  jako „neúčinné", ačkoli 130 hand-offů/3 000 her proběhlo. Projeví se
  0,12×/zápas (349 událostí / 3 000 her) v každém dalším korpusu.
* Druhá v pořadí (první VĚCNÁ): **B&S volba** — strop **0,26 kola/zápas**
  (789/3 000 kol, kde je B&S jediná přímá cesta na TD, dnes konverze 0,9 %);
  při střízlivé konverzi ~25 % je to až ~+0,06 TD/zápas, největší strop mezi
  mrtvými makry. Před psaním kódu ale změřit, kolik z těch 789 kol je „při
  vedení" (cap 0,02 = doktrína, ne bug) — to snímek umí, jeden skript.
* P4 (CHAIN): strop 0,09 kola/zápas — do knihy, neprioritní.
* Hand-off swap: po opravě logu přeměřit; teprve pak rozhodovat o AG featuře.

## Nálezy bez dopadu (taky nálezy)

* FOUL není mrtvý (11 175 faulů / 3 000 her; 23,3 % kol) — poměr k nabídce
  0,35, zdravý; nabídka je navíc podlaha (soupeř padá až během kola).
* PICKUP zahráno (18,4 % kol) > nabídka ze snímku (15,0 %) — očekávaný
  projev „snímek je začátek kola", ne chyba.
* Rekonstrukce brány reprodukovala nezávislé číslo 329/218 z 14.08. přesně —
  metoda „přepsat bránu krok po kroku" se dá považovat za kalibrovanou.
* Vícenásobné srovnání: v reportu nejsou žádné korelace ani hvězdičky,
  jen počty s jmenovateli — korekce netřeba.
