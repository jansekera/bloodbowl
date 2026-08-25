# ZADÁNÍ — T5.21: 68 citací míří na CRP/LRB6, čtou se proti BB2016

**Zadáno 25.08.2026.** Model: **Opus** (ne Fable) — celá úloha stojí na tom
odolat vlastnímu prioru, ne na rychlosti sběru.

## Otázka

Náš kód se na 68 místech odvolává na **CRP/LRB6**. Cílová edice je ale
**BB2016** (rozhodnuto uživatelem, T5.16). Text máme v `rules_bb2016.txt`
(9 519 řádků, pravý BB2016 — hlavička „Blood Bowl 2016, October 2017").

**Pro každou citaci:** chová se kód podle **BB2016**?

## ⛔⛔ TŘI BRZDY — přečti dřív, než napíšeš první verdikt

**(1) NEODPOVÍDEJ Z VLASTNÍ ZNALOSTI BLOOD BOWLU.** Nikdy. Otevři
`rules_bb2016.txt`, najdi pravidlo, **cituj doslova i s číslem řádku**.
⚠️ **Máme tři doklady, že tohle selhává:** 24.08. u upírů, Hypnotic Gaze
i Wrestle popsal AI text **naše STARÉ chování**, ne rulebook — protože náš kód
se psal ze sekundárních zdrojů a model je četl taky.
⇒ **Shoda s populárním výkladem je VAROVNÝ SIGNÁL, ne potvrzení.**

**(2) ROZLIŠUJ DVĚ ÚPLNĚ RŮZNÉ VĚCI.** Většina pravidel je mezi CRP a BB2016
**beze změny**. Verdikt proto musí říct, o který případ jde:
- **A — špatný zdroj, TÝŽ obsah:** BB2016 říká totéž ⇒ levné, jen přepsat odkaz.
- **B — špatný zdroj, JINÝ obsah:** edice se rozcházejí a my hrajeme podle CRP
  ⇒ **to je NÁLEZ** a je to celý důvod téhle úlohy.
- **C — nejde rozhodnout:** v BB2016 se to pravidlo nenašlo, nebo je nejasné.

**(3) NIC NEOPRAVUJ.** Ani kód, ani komentáře, ani čísla řádků.
⛔ Zvlášť **nepřepisuj citace na BB2016** — tím by se jen zamaskovalo, že obsah
nikdo neověřil. Tahle úloha **jen čte a hlásí**.

## Jak hlásit

⚠️ **Piš průběžně do svého výstupního souboru, ne až na konci.** 22.08. usekl
limit metrový audit po ~18 minutách v třetině a **výsledek se ztratil**.
Po každých ~5 citacích ulož, co máš.

Tabulka, jeden řádek na citaci:

| soubor:řádek | téma | verdikt (A/B/C) | BB2016 ř. | doslovná citace | sedí kód? |

A pro **každé B** navíc plný zápis: co dělá kód, co říká BB2016, kdo z toho
těží (my / soupeř / obojí), a v jakých situacích to hraje.

⭐ **Když je něco v koši C, napiš to.** N/A koš je nález, ne ostuda — 20.08. se
hlavní nález roku schovával 17 dní přesně v něm.

## Co NEDĚLAT

- neopravovat kód · nepřepisovat citace · nespouštět testy ani engine
- neodhadovat dopad čísly, které nemáš změřené
- nevymýšlet pravidla, která v textu nejsou

Kontext: `evidence/task_queue.md`, položka **T5.21**. Souvisí s **T5.17**
(projít BB2016 na změny uvnitř zápasu) — tenhle úkol je jeho ohraničená
polovina a má jít první.
