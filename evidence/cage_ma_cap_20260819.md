# STROP KLECE Z NEJPOMALEJŠÍHO ROHU — a co z toho vypadlo (19.08.2026)

*(uživatel: „dá se nějaký max klece odvodit z nejpomalejšího MA rohu? pomůže
tato hodnota něčemu?" a „první a poslední část mají max MA ballcarriera")*

## Kapacita fází je tím kompletní — a nepotřebuje korpus

| fáze | strop |
|---|---|
| **SÓLO** | MA nosiče |
| **KLEC** | min(MA čtyř rohů, MA nosiče) |
| **VÝBĚH** | MA nosiče |

**Mechanika:** popojede-li nosič o Δx, musí o Δx popojet i každý roh, jinak
klec zůstane vzadu ⇒ klec jede rychlostí nejpomalejšího rohu.
⭐ **Řeší to námitku „záplava starých čísel":** kapacita se odvozuje z rosteru
a pravidel, ne z korpusu, kde se pravidlo klece hraje ve 2,7 % kol.
Roster: Longbeard **MA4** · Blitzer/Troll Slayer **MA5** · Runner **MA6**.

## Odpověď: pomůže — ale ne jako brzda. Mez NENÍ svazující.

7 703 kol s postavenou klecí (≥2 rohy):

| | |
|---|---:|
| ⌀ strop z nejpomalejšího rohu | **4,14 pole/kolo** |
| ⌀ skutečné Δx | **1,98** |
| **využití stropu** | **48 %** |
| Δx dosáhlo stropu | 28,6 % kol |
| ⛔ **Δx = 0 (klec stála)** | **42,0 % kol** |

Kdo stojí v rozích: Longbeard 47,8 % · Troll Slayer 24,2 % · Blitzer 17,1 % ·
**Runner 10,9 %**.

⇒ ⛔ **Vyměňovat rychlejší těla do rohů se dnes NEVYPLATÍ.** Strop 4 využíváme
na 50 %, strop 5 na 39 % — zvednout mez z 4 na 5 nekupuje nic, dokud
nevyužíváme ani tu čtyřku. **Tím padá celá jedna linie práce**, aniž se musela
zkoušet.

## ⭐⭐ Zpětný rozvrh z mechanických stropů obrací diagnózu

Rozvrh se skládá od 8. kola zpět: `M(8) = MA nosiče`, `M(t) = strop(fáze) + M(t+1)`.
17 728 kol:

| | |
|---|---:|
| ⌀ kapacita do konce půle | **21,7 pole** |
| ⌀ zbývající vzdálenost | 16,5 pole |
| **rozvrh mechanicky NESPLNITELNÝ** | **27,6 %** |
| ze splnitelných: kvótu splnil | 30,9 % |
| ⛔ **ze splnitelných: nehnuli jsme se VŮBEC** | **37,7 %** |

S historickým tempem vycházelo „nesplnitelné" v **95,8 %** kol, mechanicky ve
**27,6 %**. ⇒ **Drivy neprohráváme ROZVRHEM, ale NEVYUŽITÍM.**
Přehodnocuje to i D1 *(„pozdní start", 42 % drivů)*: konstanta 2,61 pole/kolo
v `diag_drive_failure` popisuje, co děláme, ne co jde udělat.

## ⛔⛔ P39 — NOSIČ SE V KOLE VŮBEC NEAKTIVUJE

Z 1 641 kol s Δx = 0 *(vzorek 800 her)*:

| | | |
|---|---:|---:|
| **nosič NEJEDNAL VŮBEC** (žádná událost) | 1 375 | **83,8 %** |
| z toho byl přitom **úplně volný** (žádná soupeřova TZ) | 957 | 58,3 % |
| nosič byl v soupeřově TZ (dodge by stál hod) | 684 | 41,7 % |
| nosič v kole jednal | 266 | 16,2 % |

⇒ **Nosič nestojí proto, že by nemohl. Neaktivuje se.** Není to tempo,
je to chybějící akce.

Kandidáti na příčinu, neověřeno:
* `carrierStallAwareSteps` drží pohyb **schválně** v záloze (`maxSafe = MA/2`)
  a **P37** ukazuje, že jeho test bezpečí (`carrierIsBlitzable`) **nezná GFI**
  ⇒ považuje se za bezpečného neprávem v 6,3 % kol;
* makro ADVANCE se v search vůbec nevybere;
* `expandAdvance` skončí na `if (steps <= 0) return result;`.
