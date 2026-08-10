# Balík G, krok (a): ROZSAH CHYBY OVĚŘENÝ ČTENÍM KÓDU

Datum: 10.08.2026. Čistá diagnostika, **žádný zásah do produkce.**
Zadání z fronty: „(a) ověření v kódu · (a2) kolik bloků vlastně házíme."

## Nález: nejde o „reset stavu", jde o to, že REZERVY NEEXISTUJÍ

Chyba je hlubší, než zněla formulace „po TD se vracejí i mrtví". Nejsou
tam dvě věci — je tam **jedna, a je strukturální.**

### 1. Soupiska je natvrdo 11 hráčů na tým
```cpp
std::array<Player, 22> players{};  // 0-10 = home (IDs 1-11), 11-21 = away
```
`game_state.h:19`. A `forEachPlayer` iteruje pevně `start .. start+11`
(`:47-52`). **Žádné rezervy, žádný dugout** — grep na
`reserve|dugout|squad` v `game_state.h` i `player.h` nevrací nic.
⇒ Není KAM odloženého hráče dát a není KÝM ho nahradit. Persistence
zranění by dnes znamenala jen „hrajeme v deseti", ne „nastoupí náhradník".

### 2. Každý drive se všem hráčům přepíše stav
`setupHalfOrDrive` (`game_simulator.cpp:258-268`) na začátku **bezpodmínečně**:
```cpp
for (auto& p : state.players) {
    p.state = PlayerState::OFF_PITCH;
    p.position = {-1, -1};
    ...
}
```
Žádná větev nekontroluje `KO / INJURED / DEAD`. Volá se z `setupDrive`
(`:314`), tedy **po každém touchdownu**.

### 3. A `buildTeam` je pak postaví jako zdravé
`buildTeam` (`:162+`) přiřazuje sloty 0-10 a nastavuje
`player.state = PlayerState::STANDING` **bez jakékoli podmínky**
(`:196`). Nekouká na předchozí stav.

⇒ **Každý drive začíná 11 zdravými hráči na každé straně, vždy.**
Sedí to na naměřené `DEAD/hru = 0,00` ve všech 3200 hrách (10.08.) —
smrt nemá jak přetrvat, a navíc ani neexistuje tabulka trvalých následků
(`injury.cpp`: 10+ = rovnou `INJURED`, viz rules-parity §5e).

## Co z toho plyne pro návrh G

**Není to bugfix na pár řádků. Jsou to tři vrstvy, a musí se udělat
v tomhle pořadí:**

1. **Soupiska > 11** — rozšířit `players` a iterátory. Dotkne se všeho,
   co předpokládá „11 na tým" (`baseId`, `forEachPlayer`, feature
   extractor, PHP parita). **Největší kus.**
2. **Nemazat stav při `setupDrive`** — `KO / INJURED / DEAD` musí přežít;
   `isNewHalf` už dnes rozlišuje half od drivu (`:258`), takže je kam
   podmínku pověsit.
3. **Výběr nastupující jedenáctky z dostupných** — dnes `buildTeam` bere
   sloty 0-10; musí brát prvních 11 **způsobilých**.

**A teprve pak dávají smysl přibalené položky:**
* zotavení z KO na začátku drivu (hod, jinak zůstává mimo),
* Sweltering Heat (D6 na hráče na konci drivu),
* surf `Stunned` → **rezervy**, ne KO,
* tabulka trvalých následků (bez ní neexistuje smrt).

## Krok (a2) — kolik bloků házíme: ZATÍM NEZMĚŘENO
Engine počet bloků nikde nepočítá. Potřebuje to malý čítač
v diagnostickém buildu (ne v produkci). **Neděláno dnes**, aby to
nezasahovalo do běžícího měření D-vlny 1; zůstává jako první krok, až
se na G sáhne.

## Odhad pracnosti
Vrstva 1 je zdaleka největší a dotkne se i **PHP parity** a **feature
extractoru** (ten dnes počítá s 11). Doporučení: začít vrstvou 2+3 nad
současnou jedenáctkou (tedy „hrajeme v deseti, když někdo odejde"), to
už samo o sobě zapne attrition jako mechaniku a dá se změřit — a teprve
pak řešit rezervy. **Rozdělit, jak bylo řečeno u P1.**
