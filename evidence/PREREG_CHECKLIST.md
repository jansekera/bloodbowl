# PŘEDREGISTRACE — POVINNÝ KROK PŘED PŘEDPOVĚĎMI

⛔ **Nepsat predikce delty/chess, dokud tohle není vyplněné.** Účel:
zaručit, že se měří to, co změna DĚLÁ, ne co se zrovna měřilo po ruce —
[[feedback_measure_what_the_change_does]], [[feedback_measure_the_cause_not_the_symptom]].
Vzniklo 09.09.2026 po M14b (viz níž), potřetí zapsaná stejná připomínka od
uživatele — paměť sama nestačí, checklist musí ležet **vedle** souboru,
který se stejně píše při každé sondě/noci.

## Pět kroků, v pořadí

1. **Jedna věta, co se v kódu změnilo** — s odkazem na commit/soubor:řádek.
2. **Přímý mechanismus** — co ta změna dělá NA MÍSTĚ, KDE SE ROZHODUJE, ne
   kde se to projeví o tři vrstvy dál (výsledek celé hry je vždycky moc
   daleko). Zformulovat jako větu *„tahle změna dělá X"* a metriku odvodit
   z X, ne z toho, co už existuje po ruce.
3. **Existuje čítač, který X měří?** Pokud ne, napsat ho TEĎ — před
   spuštěním sondy, ne po přečtení výsledku, který se pak nedá vysvětlit.
4. **Sanity-test čítače na 1-2 párech** — dá smysluplné, nenulové číslo pro
   OBĚ strany/politiky srovnání? (Nula z rozbitého čítače se čte jako nález,
   viz [[feedback_wrong_result_looks_normal]].)
5. **Teprve TEĎ napsat předpověď mechanismové metriky** do `.preds`.
   Souhrnná herní metrika (chess/win-rate) se píše taky, ale jako
   DRUHOTNÁ kontrola dopadu na výhry — nikdy se nečte jako první a nikdy
   sama o sobě neuzavírá otázku, co změna dělá.

## Doklad, proč to nejde nechat jen na paměti (09.09.2026, M14b)

Sonda `ab_m14b_20260909_probe/` byla předregistrována a přečtena JEN podle
`chess` delty (win-rate) — −0,0250 ± 0,0235 SE, nerozhodnuto. Uživatel
upozornil, že tohle měří důsledek o tři vrstvy dál, ne to, co M14b mění
(bezpečnost přiblížení blitzujícího). Teprve dodatečně přidán čítač
`takeBlitzOutcome` (`engine/src/action_resolver.cpp`, commit `98db65bf`):
kolik blitzů dojde k bloku a kolik z nich srazí cíl, rozdělené podle
POLITIKY chůze (BFS/M14b vs hladová) — přesně věc, kterou M14b mění.
Sanity test na 2 párech dal smysluplné nenulové číslo pro obě politiky
hned napoprvé (48,7 % vs 47,8 %) — čítač šel napsat DŘÍV, jen se nenapsal.
