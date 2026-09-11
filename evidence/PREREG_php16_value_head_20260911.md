# PŘEDREGISTRACE — PHP16, krátká sonda místo noci (11.09.2026)

Uživatel: *„ještě jsi zmínil, že by to na něco chtělo noc — napřed zkus
krátký trénink a sepsat mi, co chceš měřit."* ⇒ Tohle je ten sepsaný krok,
**napsaný PŘED spuštěním**.

## 1. Co se má změnit

`weights.json` má `value_weights` = **70 nul**; `LearningAICoach` je čte
(`src/AI/LearningAICoach.php:68-71`), dopadí nulami na 73 a `evaluateState`
vrací vždy 0. Uvažovaná změna = **natrénovat value hlavu** *(noc stroje)*
nebo **číst policy hlavu** *(85 čísel, neznámé pořadí)*.

## 2. Přímý mechanismus — a proč to nejspíš NEBUDE fungovat

⛔ **Na místě, kde se rozhoduje, je `baseScore` KONSTANTA.**
`evaluateState` se volá **jen dvakrát** (`:84` a `:435`) a **pokaždé nad
TÝMŽ, současným stavem** — ne nad stavem PO akci. Každý kandidát dostane
`baseScore + <heuristika>`, takže při výběru maxima se **báze vykrátí**.

⇒ **MECHANISMUS: netrénované váhy nezmění ani jedno rozhodnutí, protože
`baseScore` vstupuje do všech kandidátů stejně a v argmaxu se vykrátí.**

⚠️ A po opravě `PHP24` to platí ještě úplněji: jediné místo, kde báze dřív
rozhodovala, bylo srovnání s `END_TURN` (`baseScore - 0.01`) — a ten
kandidát už neexistuje.

## 3. Čítač

`cli/probe_value_head_20260911.php` — **párové A/B nad TÝMŽ stavem**:
v každém rozhodovacím bodě se zeptá obou koučů *(nulové váhy vs. náhodné
nenulové, pevný seed)* a počítá, kolikrát se liší **akce nebo cíl**.
Jmenovatel se nesmí ztratit: `shodne + rozdilne == rozhodnuti`, tiskne se
i zbytek.

## 4. Sanity-test čítače — obě strany

- **(a) váhy se opravdu načetly:** `evaluateState` kouče B musí být na
  vzorku stavů **nenulové**. Kdyby bylo nulové, měřila by se dvě identická
  nastavení a nula by nic neznamenala *(vada z `PHP7`)*.
- **(b) čítač umí najít jedničku:** táž mašinérie nad dvojicí, která se
  lišit MUSÍ — kouč proti sobě samému s `epsilon = 1.0` *(náhodný výběr)*.
  Rozdílů musí být **výrazně víc než nula**.

SANITY-TEST: doplní se číslem z (a) a (b) před spuštěním ostrého běhu.

## 5. Předpověď — zapsaná PŘED během

| metrika | předpověď |
|---|---|
| rozdílná rozhodnutí (A vs. B) | **přesně 0** |
| `evaluateState` kouče B na vzorku | **nenulové** *(jinak neplatí ani ta nula)* |
| kontrola (b), kouč vs. epsilon=1.0 | **desítky procent rozdílů** |

**Co z toho plyne dopředu, ať se to nečte až podle výsledku:**

- **Vyjde-li 0** ⇒ trénink value hlavy je **k ničemu, dokud skórery
  nehodnotí stav PO akci**. PHP16 tím přestává být úloha na noc stroje
  a stává se úlohou na kód. ⛔ A hlavně: **noc by se v tom případě utratila
  za nic** — proto se pouští tahle sonda, ne noc.
- **Vyjde-li > 0** ⇒ mám v hlavě chybu, existuje cesta, kterou jsem
  nenašel; najít ji **před** utracením výpočetního času.
