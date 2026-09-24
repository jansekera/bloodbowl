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

---

# ✅ VÝSLEDEK — 24.09.2026, po běhu

| veličina | předpověď | naměřeno | verdikt |
|---|---|---|---|
| **DEAD celkem** | ~13, pásmo **7–19** | **16** | ✅ **v pásmu** |
| DEAD/hru | ~0,33 | 0,400 | ✅ |
| DEAD = 0 *(tabulka se nevolá)* | vyvrátilo by | ne | ✅ |
| DEAD > 25 *(špatné rozložení)* | vyvrátilo by | ne | ✅ |
| INJURED + DEAD | ~78 | 58 + 16 = **74** | ✅ *(viz níž)* |

⇒ **Předpověď POTVRZENA.** Tabulka následků se volá, šestina casualty končí smrtí.

## ⭐⭐ JAK SE TA KONTROLA POČÍTÁ — A PROČ NEPOTřEBUJE ZÁKLADNU

Uživatel 24.09.: *„taže se nám v AB testu změnila základna? ale spočítáš kontrolu i tak, že?“*

✅ **Ano — a je to tím, že předpověď je POMĚR UVNITŘ JEDNOHO BĚHU**, ne rozdíl proti minulému.

| | |
|---|---|
| casualty celkem *(58 INJURED + 16 DEAD)* | **74** |
| z toho DEAD | **16 = 21,6 %** |
| očekáváno *(8 polí ze 48)* | **16,67 %** |
| σ při n = 74 | √(0,1667×0,8333/74) = **4,33 p. b.** |
| odchylka | **+1,13 σ** ⇒ ✅ šum |

⚠️ **Chyba v zápisu předregistrace:** předpověď byla napísaná jako **absolutní počet** („~13 ze 78“),
ačkoli tvrzení je **poměrové**. Kdyby počet casualty spadl výrazně, vypadalo by to jako vyvrácení,
přestože tabulka pracuje správně. ⇒ ⭐ **Poměrové tvrzení předregistrovat jako poměr**, a absolutní
číslo uvádět jen jako orientaci.

## ⛔⛔ A JEDNO POUČENÍ, KTERÉ PLATÍ NA VŠECHNA DALŠÍ MĚŘENÍ

**Běhy PŘED a PO této změně NEJSOU párové, ani při stejném `BB_DICE_SEED`.**
Důvod: `rollCasualty()` odebere ze zdroje **dvě kostky navíc** (D6 + D8) na každou casualty.
Při deterministickém proudu se tím **od první casualty rozsynchronizuje všechno ostatní**.

**Doklad je přímo v číslech:** surfy 32 → **30**, INJURED+DEAD 78 → **74**, ačkoli změna
neměla ani na jedno sáhnout. Není to nález — je to posun proudu.

⇒ ⭐ **Každá změna, která přidává nebo ubírá hod, ruší párovost.** Kdo chce párové A/B,
musí buď kostky navíc brát z odděleného zdroje, nebo srovnávat jen veličiny, které
na pořadí hodů nezávisí.

## ⏰ OTEVŘENÁ OTÁZKA — podíl surfů do rezerv

| | základna | nový běh | mechanicky očekáváno |
|---|---|---|---|
| surf → rezervy | 23/32 = **71,9 %** | 16/30 = **53,3 %** | **72,2 %** *(2D6 ≤ 8 = 26/36)* |

Nový běh je **2,3 σ pod očekávanou hodnotou**. ⚠️ **Není to důkaz vady** — vzorky jsou nezávislé
(viz výš), n = 30 je málo a očekávaných 72,2 % platí jen **bez modifikátorů** (Stunty +1 posouvá dolů,
Thick Skull na osmičce nahoru).

⭐ **Levná kontrola, až na to přijde řada:** diag už **loguje každý surf na STDERR** i s výsledkem
hodu na zranění (`SURF hrac=… zraneni=…`). Stačí běh pustit se zachycením STDERR a **sečíst
přímo výsledky hodů** — měří se tím příčina, ne následek, a nepotřebuje to víc her.
