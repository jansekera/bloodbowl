# PRE-REGISTRACE — orc scoring gap (zapsáno 14.08. PŘED výpočtem)

Očekávání před spuštěním jakéhokoli měření na `diag_replay_mine_20260813_big_data`:

1. **Příčina ztrát (C drivy) per-race:** podíl „soupeřův blitz/blok srazil nosiče"
   bude proti orkovi VYŠŠÍ než průměr korpusu (>84,5 %), proti skavenovi nižší
   (skaven ST2 → víc ztrát přes dodge/GFI/sebrání volného míče).
2. **Kostky blitzu na nosiče (BLZ na začátku soupeřova kola):** proti orkovi
   častěji ≥2 kostky (ST3/ST4 + asistence), proti skavenovi častěji 1 kostka
   nebo do kopce (Gutter Runner ST2 na Runnera ST3).
3. **REACH0:** proti orkovi podobný nebo mírně vyšší než proti skavenovi
   (skaven má MA9, dosáhne dál — ale přes dodge; REACH bez dodge řídí spíš
   naše značkování). Nejistý směr — proto měřím.
4. **Attrition (stojící těla v čase):** naše stojící těla klesají proti orkovi
   rychleji (Mighty Blow + ST4 bloky), soupeřova klesají pomaleji (AV9 vs AV7
   skavena). Očekávám rozdíl v bilanci stojících ≥1,5 těla v kole 8 mezi
   matchupy orc vs skaven.
5. **Rozdělení zvolených block-die faces:** naše bloky proti orkovi budou mít
   vyšší podíl ATTACKER_DOWN (bloky do kopce / 1kostkové), jejich bloky proti
   nám vyšší podíl DEFENDER_DOWN/STUMBLES než u skavena.
6. **První držení míče:** kolo prvního držení podobné napříč rasami (řídí ho
   výkop, ne soupeř). Ztráta proti orkovi přijde DŘÍV v drivu a DÁL od endzone
   než proti skavenovi.
7. **Tempo s míčem:** proti orkovi nejnižší ze všech ras (víc odporu v koridoru).
8. **Doktrína (hyp. 3):** neočekávám silnou opačnou korelaci kontrol proti
   orkovi; spíš očekávám, že kontroly (čisté rohy, K34) proti orkovi prostě
   nejsou splnitelné (nižší plnění), ne že by korelovaly opačně. Pokud vyjde
   opačná korelace, je to nález proti mému očekávání.
9. **Celkový mechanismus:** rozdíl bude kombinací (a) víc kostek na našeho
   nosiče + (b) rychlejší úbytek našich těl → nechráněný nosič, přičemž (a)
   považuji za primární. NEočekávám, že hlavní příčinou je pozdní zisk míče.

## ADDENDUM (14.08. odpoledne, PŘED plným během s8 — po smoke na 60 hrách,
## po nálezu scoreFace/shouldRerollBlock bez Wrestle)

10. **THREAT_net (pravidlová hrozba přes všechny kanály) NEBUDE monotónní
    s TD** — skavena nafoukne Wrestle člen; pořadí per-race průměrů čekám
    zhruba srovnané (0,40–0,50 u všech kromě welf ~0,38).
11. **Empirická konverze při stejné hrozbě se rozejde:** orc ≈ model,
    skaven výrazně pod modelem — protože rozhodovací vrstva enginu Wrestle
    nezná (scoreFace, shouldRerollBlock, nabídkový filtr) a Strip Ball je
    proti Runnerovi mrtvý právem (Sure Hands).
12. **Wrestle použití** ~0,1–0,2/hru, koncentrované do skaven her; sražení
    nosiče přes BD ~0 %. **Strip úspěchy na nosiče** ~0 (nosič Runner
    88–91 %); Sure Hands negace řádově jednotky/100 her.
13. **Zrcadlo (krádeže):** P(jejich nosič ↓ za naše kolo) skaven ≫ orc
    (čekám ≥2,5×), tažené naším BLZ≥2 na jejich nosiče a jejich AV7;
    to vysvětlí 198 vs 31 bez odvolání na strip/wrestle.
