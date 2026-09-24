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

---

# ✅ DRUHÉ, NEZÁVISLÉ POTVRZENÍ — 20 her, 24.09. večer

Kouřový běh po dokončení balíku G *(bod 4/4)*, tedy **jiné kostky, jiný běh**:

| | naměřeno | očekáváno |
|---|---|---|
| casualty *(30 INJURED + 6 DEAD)* | 36 | — |
| **DEAD** | **6 = 16,67 %** | **16,67 %** |

⇒ ⭐ **Přesně na hodnotě.** Tabulka následků D68 je tím ověřená dvakrát nezávisle
*(21,6 % při n = 74 a 16,67 % při n = 36)*.

## ⏰⏰ ZATO SURF SE PŘITVRDIL — otevřená otázka je teď silnější

| běh | surf → rezervy | pozn. |
|---|---|---|
| **PŘED** změnou D68 | 23/32 = **71,9 %** | sedí na teoretických **72,2 %** |
| **PO**, běh 1 *(40 her)* | 16/30 = 53,3 % | |
| **PO**, běh 2 *(20 her)* | 7/11 = 63,6 % | |
| **PO, sloučeno** | **23/41 = 56,1 %** | **−2,3 σ proti teorii** |

⭐ **Sloučit ty dva běhy po změně SE SMÍ** — je v nich **týž kód**, takže to není
pohyblivá báze *(viz [[feedback_moving_baseline_only_paired_ab]])*.

⚠️ **Jako rozdíl proti základně to významné NENÍ** *(dvouvýběrový test: 1,4 σ)*.
**Proti teoretické hodnotě 72,2 % ale ano** — a už ze dvou nezávislých vzorků
týmž směrem.

⛔ **Mechanismus NEODHADOVAT.** Větev Stunned se změnou nedotkla a Regeneration
funguje dál, takže vysvětlení nevychází — a právě proto se to má **změřit**.

⭐⭐ **DALŠÍ KROK, a má přednost:** `php cli/diag_package_g_20260914.php --matches=40 2>surf.err`
a sečíst řádky `SURF hrac=… zraneni=…` podle výsledku hodu. Tím se měří **příčina
na vrstvě, kde se rozhoduje** *(jaký výsledek padl na tabulce zranění)*, ne následek
*(kde hráč skončil)*. **Nepotřebuje to víc her než jeden běh.**

## ⛔ POZOR NA VOLÁNÍ MĚŘIDLA

Diag bere **`--matches=N`**, ne poziční argument. `php … 5` a `php … 2` obojí pustí
**výchozích 20 her** a tiše — 24.09. jsem si tím zabil běh vlastním `timeout 180`
v domnění, že pouštím pět her.

---

# ✅✅ SURF — UZAVŘENO 24.09. VEČER, VADA NEEXISTUJE

Změřeno **přímo na příčině**: běh 40 her se zachyceným STDERR, kde diag u každého
surfu tiskne i výsledek hodu na zranění. **26 surfů:**

| hod | výsledek | kam hráč šel |
|---|---|---|
| 3 ×3 · 4 ×1 · 5 ×2 · 6 ×4 · 7 ×8 | **stunned** | **18× rezervy (OFF_PITCH)** |
| **8** ×4 · **9** ×1 | **ko** | 5× KO box |
| **10** ×1 · 11 ×2 | **casualty** | 2× injured, 1× **dead** |

⭐ **Převod „omráčen → rezervy" sedí 18 z 18.** Kontrolní dotaz *„existuje omráčený,
který nešel do rezerv?"* vrací **prázdno**. Oprava z bodu 1/4 funguje bez výjimky.

## ⛔⛔⛔ PODEZŘENÍ BYLO MOJE CHYBA, NE VADA ENGINU

Očekávaných **72,2 %** jsem vyrobil tím, že jsem si v `InjuryResolver.php` přečetl
komentář citující *„a roll of 8 … as a Stunned result rather than a KO'd result"*
jako obecné pravidlo. ⛔ **Je to text dovednosti THICK SKULL, tedy VÝJIMKA.**
Obecně platí **≤ 7 = omráčen ⇒ 21/36 = 58,3 %**.

Proti správné hodnotě: **18/26 = 69,2 % = +1,13 σ** ⇒ ✅ sedí.
A dřívější „základna" 71,9 % byla **odlehlý vzorek**, ne norma.

⚠️ **Konzistentní odchylka od nesprávné báze vypadá k nerozeznání od nálezu** —
zapsáno jako [[feedback_expected_value_needs_its_own_check]].

## ⭐ HRANICE 9 a 10 — na dotaz uživatele

Uživatel si všiml, že v prvním výpisu **chyběly hody 9 a 10**, tedy přesně obě
hranice prahů *(≤7 omráčen · 8-9 KO · ≥10 casualty)* — místo, kde by se schovala
chyba o jedničku.

✅ **Obě jsou pokryté testy, a ty rozlišují** *(ověřeno negativní kontrolou)*:

| posun prahu | co spadne |
|---|---|
| `<= 7` → `<= 8` | `testArmourBrokenKO`, `testCrowdSurfCanCauseKO` |
| `<= 9` → `<= 10` | `testArmourBrokenCasualty`, `testInjuryModifierUpgradesSeverity`, `testCrowdSurfCanCauseCasualty` |

✅ **A v úplném běhu nakonec padly obě** — 9 → KO, 10 → casualty. Nebyla to mezera
v ověření, jen vzorkování.

# ⏰ CO SLEDOVAT PŘÍŠTĚ: podíl smrtí je 2,0 σ nad očekáváním

| běh | casualty | DEAD | podíl |
|---|---|---|---|
| 1 *(40 her)* | 74 | 16 | 21,6 % |
| 2 *(20 her)* | 36 | 6 | 16,7 % |
| 3 *(40 her)* | 75 | 19 | 25,3 % |
| **sloučeno** | **185** | **41** | **22,2 %** vs 16,67 % ⇒ **+2,0 σ** |

⚠️ **NEPROHLAŠUJI ZA NÁLEZ**, a záměrně:
* **pro:** očekávaná hodnota **je** ověřená — test `CELA TABULKA NARAZ` projde všech
  48 kombinací D6×D8, takže posunutá hranice by se poznala hned;
* **proti:** dvě σ nastanou u jednoho z dvaceti měření samy; a tohle je **jedno
  měření**, ne tři nezávislá potvrzení — slučuji běhy téhož kódu.

⇒ ⏰ **Při dalším měření je to první číslo ke čtení.** Drží-li se nad 20 % i na
větším n, pak teprve hledat příčinu *(a začít tím, jestli se DEAD nepřiřazuje i jinde
než z tabulky)*.
