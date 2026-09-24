# PŘEDREGISTRACE — tabulka následků D68 má zapnout smrt

**Zapsáno 24.09.2026 PŘED spuštěním měření.** Důvod: aby se předpověď nedala přizpůsobit výsledku.

## Co se změnilo
Do 24.09. byl každý hod 10+ na tabulce zranění plošně `INJURED` ⇒ **DEAD nemohl nastat vůbec**.
Nově se hází na tabulce následků (D68): **8 ze 48 polí je DEAD, tedy přesně šestina.**

## Základna (běh z téhož dne, PŘED změnou, seed 20260924, 40 her)
| veličina | hodnota |
|---|---|
| DEAD/hru | **0.0000** (celkem 0) |
| INJURED na konci/hru | 1.950 (celkem **78**) |
| surfy | 32 (0,8/hru), z toho do rezerv 23 (71,9 %) |

## Předpověď pro týž seed a týž počet her
* **DEAD celkem: ~13** (78 casualty × 1/6)
* **DEAD/hru: ~0,33**
* rozptyl při n=78 je ±3,3 ⇒ **pásmo 7 až 19 sedí**, mimo něj je nález

⛔ **Co by předpověď VYVRÁTILO:**
* DEAD = 0 ⇒ tabulka se nevolá (stejná vada jako před opravou, jen jinde)
* DEAD > 25 ⇒ špatně spočítané rozložení (např. `tens >= 5` místo `== 6`)
* INJURED + DEAD ≠ ~78 ⇒ změnil se počet casualty, což změna dělat neměla

## Co se tím NEMĚŘÍ
⚠️ Ostatní následky (Badly Hurt, Miss Next Game, Niggling, ztráty vlastností) jsou **v jednom
zápase nerozlišitelné** — hráč je venku tak jako tak. Projeví se až v lize. Měří se proto
jen DEAD, a zbytek se ověřuje **testy tabulky**, ne během.
