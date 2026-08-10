# Balík D, bod 9a: tabulka cílových čísel Z PRAVIDEL (ne z mé hlavy)

Datum: 10.08.2026. Zdroj: `rules_crp2016.txt` (lokální text CRP 2016).
Účel: engine dosud stál na mém nezkontrolovaném čtení pravidel. Tahle
tabulka je vyplněná **citacemi z pravidel**, ne z paměti, a ke každému
řádku je dohledaný řádek v kódu. **Určeno ke SPOLEČNÉMU PROJITÍ s
uživatelem PŘED jakoukoli opravou.**

Formát: co říkají pravidla → co dělá engine → verdikt.

---

## 1. Agility tabulka (společný základ)

> „AGILITY 1 2 3 4 5 6+ / DODGING ROLL 6+ 5+ 4+ 3+ 2+ 1+"
> „**IMPORTANT:** The Agility table is used to work out (…) dodging,
> picking up the ball, and throwing or catching (…). Each Action has its
> own set of modifiers, and **it is only these modifiers which apply**."

Základ = `7 − AG`. Engine to tak má všude. ✅
Klíčové je, že **každá akce má vlastní sadu modifikátorů** a nesmí se
míchat. Přesně tady jsme udělali chybu (řádky 2 a 3).

---

## 2. ⛔ DODGE — CHYBÍ ZÁKLADNÍ +1 (bod 11d)

> „DODGING MODIFIERS — **Making a dodge roll +1** / Per enemy tackle zone
> on the square that the player is dodging to −1"

Kontrolní příklad přímo z pravidel (Slow-Motion Replay):
> „Dieter has an Agility of 3, which means that he needs to roll a basic
> 4 or more. **He gets a +1 to the roll for making a dodge**, but has to
> subtract 2 because there are two Orc tackle zones on the square he is
> moving to, for a final modifier of −1. (…) the D6 roll is a **5**, which
> means that Dieter successfully dodges."

**Engine** (`engine/src/helpers.cpp:61`):
```cpp
int target = 7 - ag;                                   // ← chybí −1 za dodge
target += countTacklezones(state, dest, player.teamSide);
```

| situace | pravidla | engine | rozdíl |
|---|---|---|---|
| AG3 → volné pole | **3+** | 4+ | o stupeň těžší |
| AG3 → pole s 1 TZ | **4+** | 5+ | o stupeň těžší |
| AG3 → pole s 2 TZ | **5+** | 6+ | o stupeň těžší |
| AG4 → volné pole | **2+** | 3+ | o stupeň těžší |

⇒ **Každý dodge ve hře je u nás o stupeň těžší, než má být.** Je to
nejčastější hod ve hře. A bolí to asymetricky: elfové mají skill Dodge
(reroll), takže jim engine část chyby vrátí; trpaslíci ne.
**Pozor při opravě:** zkontrolovat, že se to propíše i do
`macro_actions.cpp:190` (plánovač počítá `dodgeFail` z téže funkce ✓)
a do všech pravděpodobnostních odhadů.

---

## 3. ⛔ LEAP — nesmí mít ŽÁDNÉ modifikátory (bod 10)

> „In order to make the leap, move the player to any empty square 1 to 2
> squares from his current square and then make an Agility roll.
> **No modifiers apply to this D6 roll unless he has Very Long Legs.**"

**Engine** (`engine/src/move_handler.cpp:237-240`):
```cpp
int target = 7 - player.stats.agility;
target += countTacklezones(state, to, player.teamSide);   // ← nemá tam být
if (player.hasSkill(SkillName::VeryLongLegs)) target -= 1; // ✅ správně
```

| situace | pravidla | engine |
|---|---|---|
| AG4 leap na hlídané pole (2 TZ) | **3+** | 5+ |
| AG4 leap na volné pole | 3+ | 3+ ✅ |

⇒ Leap do klece je u nás výrazně dražší, než má být. **Tohle hraje
V NÁŠ PROSPĚCH** (wardancer se hůř dostane do klece), takže po opravě
čekat zhoršení proti elfům — a je to důvod, proč se D má měřit jako
jeden balík s pojistkou, ne jako experiment s hypotézou o zlepšení.

---

## 4. ⛔ FOLLOW-UP JE VOLITELNÝ (bod 11b)

> „PUSHED: (…) The attacking player **may** follow up the defender."
> „Follow Up Moves: A player who has made a block **is allowed** to make a
> special follow up move (…). The player's coach **must decide whether to
> follow up before any other dice rolls are made**."

Jediná výjimka — Frenzy:
> „When making a block, a player with this skill **must always follow up**
> if he can."

**Engine:** follow-up je vynucený pro všechny (`noFollowUp` nikdo
nenastaví, viz `block_handler.cpp:478`). ⇒ parity bug.
Dopad na doktrínu: zadní Longbeard dnes nemůže odstrčit markera a zůstat
stát — sám se vytáhne z pozice zálohy (fronta, bod 5, varianta 2).

---

## 5. ⛔ VYSURFOVANÝ NOSITEL — míč mají vhodit DIVÁCI (bod 11c)

> „**If the player who is holding the ball is pushed out of bounds, then
> he is beaten up by the fans, who are more than happy to throw the ball
> back into play!** The Throw-in template is centred on the last square
> the player was in before he was pushed off the pitch."
>
> (throw-in = 2d6 polí, viz THROW-INS)

**Engine** (`block_handler.cpp:470`): volá se
`handleBallOnPlayerDown(state, def.id, …)`, což míč **upustí na poslední
pole na hřišti a odrazí ho o JEDNO pole** (`ball_handler.cpp:211-215`).
`resolveThrowIn` v enginu **existuje** (`ball_handler.cpp:180`, volá se
z pass/kick cest), jen se sem nezavolá.

| | pravidla | engine |
|---|---|---|
| míč po surfu nositele | vhozen 2d6 polí od místa vypadnutí | spadne u čáry a odrazí se o 1 pole |

⇒ **Surf je u nás výrazně přeplacený**: dostaneme odstranění hráče
*a zároveň* míč hned vedle sebe. Ovlivňuje to sideline doktrínu (L-pin,
bod 12) a nejspíš i ocenění pushů k čáře v plánovači.

---

## 6. ✅ CO JE SPRÁVNĚ (ověřeno, nesahat)

| pravidlo | citace | engine |
|---|---|---|
| **Pickup** | „Picking up the ball **+1** / Per opposing tackle zone on the player −1" | `helpers.cpp:115` `6 - AG` + TZ ✅ (bod 9 byl falešný poplach) |
| **Catch** | „Catching an accurate pass +1 / missed pass, kick-off, bouncing ball, throw-in **+0** / per TZ −1" | `helpers.cpp:135` `7 - AG - modifier` + TZ ✅ |
| **GFI** | „On a roll of 1 the player trips up" | target 2+ ✅ |
| **GFI v Blizzardu** | „will slip and be Knocked Down on a roll of **1-2**" | target 3+ ✅ |
| **Pouring Rain** | „−1 modifier applies to all catch, intercept, or pick-up rolls" | +1 k targetu u pickup i catch ✅ |
| **Surf zranění** | „no Armour roll (…) automatically injured", „one roll on the Injury table" | `injury.cpp:114-120` bez zbroje, injuryModifier=1 ⚠️ (viz otázka níže) |

---

## 7. OTEVŘENÉ OTÁZKY K PROJITÍ

1. **Surf = +1 k Injury?** Pravidla říkají „receives one roll on the
   Injury table" a „The crowd does not have any injury modifying skills" —
   o modifikátoru +1 tam nic není. Engine dává `injuryModifier = 1`
   (`injury.cpp:116`). **Vypadá to jako náš přídavek navíc** — potvrdit
   nebo vyvrátit.
2. **Interception target** `7 − AG + 2` (`pass_handler.cpp:74`) — základní
   modifikátor +2 jsem v textu CRP zatím neověřil, dohledat.
3. **Volba pole pro push** (bod 11b, druhá půlka) — pravidla dávají volbu
   koučovi útočníka mezi eligible poli; ověřit, jak to dělá
   `getPushbackSquares` a kdo volí.

## 8. NÁVRH POŘADÍ OPRAV V RÁMCI BALÍKU D

Sbalit do JEDNÉ změny „engine hraje podle pravidel" a změřit JEDNOU
(pojistková pre-registrace: **trpaslíci nesmí regredovat; neutrální =
úspěch**; žádná hypotéza o zlepšení).

1. dodge +1 (řádek 2) — největší dopad, jedna řádka
2. leap bez modifikátorů (řádek 3) — jedna řádka, hraje proti nám
3. follow-up volitelný mimo Frenzy (řádek 4) — potřebuje rozhodovací
   vrstvu „následovat, nebo zůstat", ne jen flag
4. throw-in u vysurfovaného nositele (řádek 5) — přesměrovat na
   `resolveThrowIn`, funkce existuje
5. nega-traity (TakeRoot u bloku, ReallyStupid soused) — z fronty, dosud
   neověřeno proti textu; **doplnit do téhle tabulky před opravou**

⚠️ Body 2 a 5 hrají spíš proti nám (usnadní elfům leap do klece, zpřísní
naše big guye), body 1 a 4 jsou obousměrné. Proto pojistka, ne hypotéza.
