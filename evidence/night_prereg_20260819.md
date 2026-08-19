# PŘEDREGISTRACE NOCI 19.→20.08.2026 — P38

**Rameno:** `setCageAwareAdvanceArm`, mode 6. Cílové pole nosiče v `expandAdvance`
se vybírá podle klece, která z něj vyjde *(4 rohy ∧ všechny čisté ∧ nosič bez
dalších sousedů ∧ čtyři volná těla na ty rohy dosáhnou)*, místo aritmetiky
„x plus počet kroků, y o jedno ke středu".
Kandidáti jsou omezeni na pole do **jednoho pole** od nejlepšího dostupného
postupu — **tempo se neprodává** (K9a je 20,7σ).

**Práh:** ±0,015 párové delty chess. **Párů:** 8×850 = 6 800, matchup dw-we.
**Nulová kontrola:** `CONTROL_MODE2=1` + vnitřní leak test *(rameno jednalo vs
nejednalo, v jeho vlastním běhu)*.

## Předpovědi *(zapsané PŘED během)*

| veličina | předpověď | proč |
|---|---|---|
| `delta` | **0,005 – 0,040** | cena kroku je změřena jako **záporná**: REACH0 se zlepší ve 30,3 % kol a zhorší ve 3,5 %, FB2 beze změny ve 100 % |
| `n_nonzero` | **0,60 – 0,95** | rameno sahá na ADVANCE, tedy skoro na každé kolo s míčem — na rozdíl od P9c, kde šlo jen o odsuny |
| `leak` | **0** | jinak se delta nečte |
| `arm_acted` | **≥ 0,95** | nosič postupuje skoro v každém zápase |

## ⚠️ Co může předpověď o deltě shodit — zapsáno předem

* **σ pravidla je 0,0.** Fable 19.08.: jako prediktor skórujícího drivu má
  pravidlo na dnešním korpusu **nulu** — ale je to **vyhladovělý metr**
  *(korpus ho hraje ve 2,2 % kol)*. **Nemáme tedy žádný důkaz, že plnění
  pravidla vede k výhře**; máme uživatelovu doktrínu a změřenou zápornou cenu
  kroku. Tahle noc je první test toho spojení.
* **Tempo.** Omezení „do jednoho pole od nejlepšího postupu" má tempo uhájit,
  ale je to konstrukce, ne měření. Když delta vyjde záporná, **první podezřelý
  je K9a**, ne klec.
* **Jednosměrnost.** Rameno umí nosiče poslat do boku; při vyšším `n_nonzero`
  než 0,95 by šlo o to, že sahá i na kola, kde se nic nemělo měnit.

## Jak se to čte ráno

`chain.log` tiskne sám: (1) leak → (2) arm acted → (3) `n_nonzero` →
(4) sloučená delta se sdruženou SE, empirickou SE mezi shardy a **strojovým
verdiktem**, který od 19.08. rozlišuje **EKVIVALENCI** *(celé CI uvnitř prahu
⇒ „ZASTAV")* od **NEROZHODNUTO — MÁLO SÍLY** *(CI přes práh ⇒ dopočte chybějící
páry)*. Plus PŘEDPOVĚĎ vs VÝSLEDEK.
**Když se ráno něco dopočítává ručně, je to vada aparátu.**
