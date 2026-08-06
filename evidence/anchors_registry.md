# Registr kotev — živý dokument (doplňovat při každé promoci / novém měření)

Kotva = zmrazená verze AI jako pevný metr. Pamatuje se podle toho, CO SE
NAUČILA PROTI PŘEDCHOZÍ, ne podle data. Absolutní řada: každá verze se měří
proti kotvě „POZIČNÍ" (harness diag_a3_2_anchor_20260805.py, 300 side-swapped
párů, kontrolní rameno musí dát ~50 %).

## KOTVY (pevné metry — zmrazené navždy, každá se jménem = co se naučila)

| Kotva | Soubor / md5 | Co se naučila proti předchozí | Změřená síla |
|---|---|---|---|
| **„POZIČNÍ"** (jul28) | weights_anchor_jul28.json / b426c64d… (= weights_best @ commit 99a0d1c, 22.07. 13:57) | Hraje čistě z ohodnocení pozice, bez našeptávače tahů (policy blend 0, prior floors ano). Produkt červencového value učení; éra 8 zamítnutých iterací po sobě. | **Nula absolutní řady (50 %).** Benchmark éry ~96 %. |
| **„ELFÍ INTUICE"** (promoce 03.08.) | weights_best.json / 17578260… + weights_best_policy.json / cd72ed6b… (blend 0,2) | **První promotnutý našeptávač. Naučil se hru rychlých ras — wood-elf 74 %, skaven 65 %, human 52 % — a trpaslíky AKTIVNĚ KAZÍ (33 %, −17 pp).** Přínos +4–6 pp celkově = celý současný náskok nad Poziční. | **54,0 % [48,9–59,1] vs Poziční** (A3-2, 05.08.); fairtest 54,26 % |

## MĚŘENÉ A KONTROLY (nejsou kotvy — mění se, nebo slouží jako negativní kontrola)

| Položka | Soubor / md5 | Role | Stav |
|---|---|---|---|
| **„ŽIVÁ"** (pracovní stash) | weights_policy.json / 7e962a41… po iteraci 05.→06.08. (md5 se mění každou iterací!) | NENÍ kotva — jediná věc, co se mění každou iterací: živý našeptávač nesoucí učení přes zamítnuté brány. **Kandidát na příští kotvu.** Ranní otázka po tréninku: „posunula se Živá před Elfí intuici?" | **FINÁL 05.08. (verze fa7698b8, 600 her): 51,4 % [46,5–56,2] vs Elfí intuice = celkově ŠUM — zatím ne.** Per-race ale ne-nula: wood-elf 73,8 % (z=4,25, lepší!), dwarf 37,8 % (z=−2,21, horší) — Živá se dál elfizuje, dwarf regresi prohlubuje. |
| **„STAGNACE"** (noreset iter1–4) | weights_noreset_iter1-4.json | Zmrazený experiment „střádá se učení bez resetů?" — **dokázal, že NE** (plochá řada). Negativní kontrola kotevní metody; zdokumentovaná slepá ulička, nevracet se. | 53,6 / 49,6 / 49,6 / 52,5 % vs Poziční (A3-2, 05.08.) |

## Pravidlo přijetí nové kotvy (05.08., z prověrky Stagnace)
**Kotvou se smí stát jen verze MĚŘITELNĚ ODLIŠNÁ od předchozí kotvy**
(párové HtH s CI mimo 50 %, typicky skrze promoci bránou). Prověrka
Stagnace: obsahem se od Poziční liší (4 iterace tréninku; její policy
hlava je předek Elfí intuice), ale herní silou NE (49,6–53,6 %, vše CI
přes 0,5) → jako kotva by nic neměřila, jen zdvojila cenu měření.
Zůstává negativní kontrolou.

## Stálý režim měření (od 05.08.; SCHVÁLENO uživatelem 06.08. — závazné)
1. Po každé promoci: nový šampion vs PŘEDCHOZÍ šampion → „co se naučil" řádek sem.
2. Periodicky: šampion vs „POZIČNÍ" → absolutní řada (první bod 54,0 %, 05.08.).
3. Po tréninku intuice: nová vs zmrazená „ELFÍ INTUICE" (žije učení?).
4. Zpětně kdykoli: policy_backups/ (30 verzí, md5 v gate_history) → intuice(N) vs intuice(M).

## Cíl příští kotvy (zadání příštího týdne)
Řádek „co se naučila proti Elfí intuici" má znít: **„přestala kazit trpaslíky"**
(learning_mechanism páka 1) a/nebo „umí dovézt klec k TD" (tempo doktrína).
