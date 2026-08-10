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

### 4b. ⛔ FEND — ZÁKAZ follow-upu (doplnil uživatel 10.08.)

Follow-up tedy není dvoustavový (volitelný / povinný u Frenzy), ale
**třístavový**, a Fend je ten třetí stav — ZÁKAZ:

> **Fend (General):** „Opposing players **may not follow-up** blocks made
> against this player **even if the Fend player is Knocked Down**. The
> opposing player may still continue moving after blocking if he had
> declared a Blitz Action."

A Fend má dvě výjimky, obě potvrzené textem:

> **Juggernaut (Strength):** „If this player takes a Blitz Action, the
> opposing player **may not use his Fend, Stand Firm or Wrestle skills**
> against the Juggernaut player's blocks."
>
> **Ball & Chain (Extraordinary):** „**The player must follow up** if he
> will push back another player, and will then carry on with his move."

**Výsledná hierarchie follow-upu:**
1. základ = **volitelný** (kouč útočníka volí před hodem kostkami)
2. útočník má **Frenzy** → **povinný**
3. obránce má **Fend** → **zakázaný** (i když Fend hráč padne)
4. výjimky z bodu 3: útočník **Juggernaut při Blitz Action**, nebo
   útočník **Ball & Chain** (ten musí)

**Co dělá engine — TŘI nálezy:**

| # | nález | kód | verdikt |
|---|---|---|---|
| a | `bool fendPrevents = def.hasSkill(Fend) && **!defKnockedDown**;` | `block_handler.cpp:516` | ⛔ **BUG** — pravidla výslovně říkají „even if the Fend player is Knocked Down". Naše podmínka Fend vypne přesně v těch případech, kde se follow-up nejčastěji řeší (Defender Down / Defender Stumbles). Fend u nás fakticky funguje jen na čistý Pushed. |
| b | Juggernaut Fend **neruší** | `block_handler.cpp:516` (chybí test `isBlitz && att.hasSkill(Juggernaut)`) | ⛔ **BUG**. Engine přitom Juggernaut jinde ZNÁ: ruší StandFirm (`:64`) a mění Both Down → Pushed (`:310`). Jen u Fendu chybí. |
| c | Juggernaut **neruší ani Wrestle** | `block_handler.cpp:384` — `defWrestle` se aplikuje bezpodmínečně | ⛔ **BUG**, tatáž věta pravidel („Fend, Stand Firm **or Wrestle**"). |
| d | Ball & Chain vůbec **nikoho neposouvá** — `PUSHED` case jen vrátí `false`, žádný push, žádný follow-up | `ball_and_chain_handler.cpp:60-63` | ⚠️ zjednodušená implementace; pravidlo „B&C musí následovat" v ní nejde ani vyjádřit. Větší kus práce, viz níže. |

**⚑ DŮLEŽITÉ PRO ROZSAH: nic z 4b se dnes nemůže projevit v našich
číslech.** Podle `roster.cpp`:
* **Fend** má jen **Bretonnian** (`:277`),
* **Juggernaut** jen plný **Dwarf** roster (`:88` — Deathroller) a
  **Khorne** (`:434-435`),
* **Ball & Chain** jen **Goblin** (`:308`).

Měřená pětka jede přes `get*Roster1200()` (dwarf/skaven/wood-elf/human/orc)
a **žádný z těch pěti tyhle skilly nemá**. ⇒ Nálezy a–c jsou levné
korektnostní opravy s **nulovým očekávaným dopadem na naše A/B** — což je
dobrá vlastnost, ne špatná: nijak neohrozí trpasličí pojistku balíku D.
Nález d (Ball & Chain) je samostatný kus práce, do balíku D **nepatří**.

**⚑ DOKTRINÁLNÍ DŮSLEDEK (proč to není jen kosmetika): Fend vypíná
Frenzy.** Frenzy vyžaduje druhý blok, jen když oba **stojí a sousedí**.
Proti Fendu se útočník neposune, takže odstrčený obránce je po prvním
bloku o 2 pole daleko ⇒ **žádný druhý blok**. Až budeme mít Fend správně,
je to levná obrana proti Slayerům (2 ze 4 rohů klece mají Frenzy) i proti
Rat Ogrovi — a naopak vodítko pro balík E, kdyby se Fend zvažoval do
některého rosteru.

**K ověření (nedodělek):** Fend říká, že blitzující útočník *smí dál
pokračovat v pohybu* i když nesmí následovat. Zkontrolovat, že mu naše
blitz cesta pohyb po bloku neukončí.

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

### 5a. SURF — HOD NA ZRANĚNÍ (rozhodnuto uživatelem 10.08.)

**Rozhodnutí uživatele: „při vysurfování žádné plus 1 a žádné
modifikátory — diváci hážou na zranění, ne útočník — a jde do rezerv,
když je OK."** Tím se ruší otevřená otázka 1 z první verze téhle tabulky.
Text pravidel to potvrzuje doslova:

> „A player pushed off the pitch, even if Knocked Down, is beaten up
> **only by the crowd** and receives **one roll on the Injury table**.
> **The crowd does not have any injury modifying skills.** Note that
> **no Armour roll is made** for a player that is pushed off the pitch,
> they are automatically injured. **If a 'Stunned' result is rolled on the
> Injury table the player should be placed in the Reserves box** of the
> Dugout, and must remain there until a touchdown is scored or the half
> ends."

| # | nález | kód | verdikt |
|---|---|---|---|
| a | `ctx.injuryModifier = 1` — náš vlastní přídavek, v pravidlech pro něj není opora | `injury.cpp:116` | ⛔ **BUG → nastavit 0** |
| b | **Stunned výsledek se u nás mění na KO** a hráč jde pryč: po hodu `if (isOnPitch(...)) { state = KO; position = {-1,-1}; }` | `injury.cpp:124-127` | ⛔ **BUG** — má jít do REZERV (vrací se po TD / konci půle), ne do KO (to vyžaduje záchranný hod) |
| c | Engine **nemá stav „rezervy"**: `PlayerState` je STANDING/PRONE/STUNNED/KO/INJURED/DEAD/EJECTED/OFF_PITCH | `enums.h:15-17` | ⚠️ chybí nosič stavu; `OFF_PITCH` se dá použít |
| d | Bez hodu na zbroj ✅, jeden hod na Injury ✅ | `injury.cpp:114-120` | ✅ správně |

**Detail k rozhodnutí „žádné modifikátory":** engine kromě `injuryModifier`
pouští do hodu ještě `hasDecay` (`injury.cpp:117`) a Stunty
(`injury.cpp:29-31`). Obojí jsou ale **vlastnosti oběti**, ne útočníka ani
davu — věta „the crowd does not have any injury modifying skills" míří na
útočníkovy skilly typu Mighty Blow. **Čtu tvůj pokyn tak, že se ruší jen
útočníkovy/naše přídavky (`injuryModifier`), a Decay/Stunty zůstávají.**
Kdyby to bylo myšleno jinak, řekni — je to jeden řádek.

**⚑ Vazba na balík G (třetí výskyt téhož):** „rezervy do konce půle /
do TD" je **týž chybějící mechanismus** jako přetrvávající zranění
(bod 16) a jako Sweltering Heat (5b/b) — stav, který musí přežít konec
drivu. **Dnes je rozdíl KO vs rezervy prakticky nulový**, protože se po
každém TD staví 11 čerstvých hráčů, takže se vrátí obojí. ⇒ nález b
opravit **až v G**; v D udělat jen nález a (jeden řádek, `injuryModifier`).

---

## 5b. POČASÍ (na dotaz uživatele 10.08.) — pět nálezů

### Pravidla

> **WEATHER TABLE (2D6, na začátku hry):** 2 Sweltering Heat · 3 Very Sunny
> (−1 na všechny passing rolls) · **4-10 Nice** · 11 Pouring Rain (−1 na
> **catch, intercept, pick-up**) · 12 Blizzard (GFI sráží na **1-2**;
> „only quick or short passes can be attempted")
>
> **Sweltering Heat:** „Roll a D6 for each player on the pitch **at the end
> of a drive**. On a roll of 1 the player collapses and may not be set up
> for the next kick-off."
>
> **Změna počasí ve hře = JEDINĚ Kick-Off table výsledek 7:** „Changing
> Weather: Make a new roll on the Weather table. Apply the new Weather roll.
> If the new Weather roll was a 'Nice' result, then a gentle gust of wind
> makes the ball scatter one extra square in a random direction before
> landing."

### Nálezy

| # | nález | kód | verdikt |
|---|---|---|---|
| a | **Tabulka počasí je posunutá o jedna**: `roll <= 3 → HEAT` a `roll == 4 → VERY_SUNNY`. Podle pravidel je 3 = Very Sunny a 4 = Nice. | `engine/include/bb/enums.h:181-182` | ⛔ **BUG** |
| b | **Sweltering Heat není vůbec implementovaný** — v celém enginu není jediná reference (grep „heat/swelter" = 0 mimo enum). Je to hodnota bez efektu. | — | ⛔ **BUG** (a viz vazba na balík G níže) |
| c | **Pouring Rain chybí u interception.** Pravidla ho jmenují výslovně („catch, intercept, or pick-up"). | `pass_handler.cpp:73-79` — žádný weather člen | ⛔ **BUG** |
| d | **Pouring Rain a Blizzard NEOPRÁVNĚNĚ zdražují PŘIHRÁVKU** o +1. Podle pravidel dává −1 na passing **jen Very Sunny**; Blizzard místo toho omezuje DOSAH, Rain se přihrávky netýká vůbec. | `pass_handler.cpp:213-216` (všechny tři v jedné podmínce) | ⛔ **BUG** |
| e | **Omezení dosahu v Blizzardu („jen quick/short") není implementované.** | — | ⛔ **BUG** |
| f | **Počasí se přelosovává při KAŽDÉM výkopu**, ne jen na výsledku 7. Komentář „Roll weather (if not changed by CHANGING_WEATHER)" je čtení pravidla naruby: `if (koEvent != CHANGING_WEATHER) state.weather = weatherFromRoll(...)`. | `kickoff_handler.cpp:273-276` | ⛔ **BUG, ale LATENTNÍ** — viz níže |
| g | „Extra pole rozptylu, když nové počasí vyjde Nice" (rider u výsledku 7) není implementované. | `kickoff_handler.cpp:113-119` | ⚠️ bezpředmětné, dokud platí „latentní" níže |

### ⚑ Rozsah: co z toho se dnes vůbec projeví

`simulateGame(..., bool useFullKickoff = false)`
(`engine/include/bb/game_simulator.h:43`) a **žádný volající nepředává
`true`** ⇒ naše měřené hry jedou přes `simpleKickoff`, které se počasí
**vůbec nedotýká**. Počasí se tedy losuje **jednou na začátku hry**
(`game_simulator.cpp:391`) — správně.
⇒ **Nálezy f a g jsou latentní** (celá Kick-Off tabulka je v našich bězích
neaktivní — což je samo o sobě fakt, který stojí za zapamatování a
navazuje na nález 21.07. o výkopovém rozestavení). Nálezy **a–e jsou
aktivní**.

### ⚑ Jak moc a–b bolí: míň, než to vypadá — chyby se navzájem ruší

| počasí | CRP | engine | efekt v enginu |
|---|---|---|---|
| Sweltering Heat | 2,8 % | **8,3 %** | žádný (nález b) ⇒ chová se jako Nice |
| Very Sunny | 5,6 % | **8,3 %** | −1 na přihrávky |
| Nice | 83,3 % | **75,0 %** | žádný |
| Pouring Rain | 5,6 % | 5,6 % ✅ | |
| Blizzard | 2,8 % | 2,8 % ✅ | |

Protože Heat nic nedělá, „počasí bez efektu" vyjde 75 + 8,3 = **83,3 %** —
shodou okolností přesně jako v pravidlech. **Jediná reálná odchylka je
Very Sunny navíc (8,3 % vs 5,6 %)**, tedy přihrávky jsou zdražené asi
ve 3 % her navíc. Rain i Blizzard máme ve správné četnosti.
⇒ Nálezy a+b jsou **korektnostní, ne měřicí**: opravovat ano, ale
neočekávat posun v číslech. **Nálezy c–e jsou věcnější** (mění, jak drahá
je přihrávková hra — tedy hru rychlých ras, ne trpaslíků).

### ⚑ Vazba na balík G

Sweltering Heat (nález b) je **týž mechanismus jako bod 16**: hráč
„may not be set up for the next kick-off" = **stav, který musí přežít
konec drivu**. Dnes takový stav neumíme (proto se mažou i zranění).
⇒ **Heat implementovat AŽ v rámci G**, ne v D — jinak by se psal
dvakrát. V D nechat jen opravu tabulky (nález a).

---

## 6. ✅ CO JE SPRÁVNĚ (ověřeno, nesahat)

| pravidlo | citace | engine |
|---|---|---|
| **Pickup** | „Picking up the ball **+1** / Per opposing tackle zone on the player −1" | `helpers.cpp:115` `6 - AG` + TZ ✅ (bod 9 byl falešný poplach) |
| **Catch** | „Catching an accurate pass +1 / missed pass, kick-off, bouncing ball, throw-in **+0** / per TZ −1" | `helpers.cpp:135` `7 - AG - modifier` + TZ ✅ |
| **GFI** | „On a roll of 1 the player trips up" | target 2+ ✅ |
| **GFI v Blizzardu** | „will slip and be Knocked Down on a roll of **1-2**" | target 3+ ✅ |
| **Pouring Rain** | „−1 modifier applies to all catch, intercept, or pick-up rolls" | +1 k targetu u pickup i catch ✅ (⚠️ ale chybí u intercept a navíc se aplikuje na pass — viz 5b/c,d) |
| **Počáteční los počasí** | „At the start of the game each coach should roll a D6. Add the results together" | `game_simulator.cpp:391` jednou na začátku hry ✅ |
| **Surf zranění** | „no Armour roll (…) automatically injured", „one roll on the Injury table" | `injury.cpp:114-120` bez zbroje, injuryModifier=1 ⚠️ (viz otázka níže) |

---

## 7. OTEVŘENÉ OTÁZKY K PROJITÍ

1. ✅ **VYŘEŠENO 10.08. uživatelem** — surf: žádné +1, žádné modifikátory,
   Stunned → rezervy. Viz 5a.
2. **Interception target** `7 − AG + 2` (`pass_handler.cpp:74`) — základní
   modifikátor +2 jsem v textu CRP zatím neověřil, dohledat.
3. **Volba pole pro push** (bod 11b, druhá půlka) — pravidla dávají volbu
   koučovi útočníka mezi eligible poli; ověřit, jak to dělá
   `getPushbackSquares` a kdo volí.
4. **Decay a Stunty u surfu** zůstávají (jsou to vlastnosti oběti, ne davu)
   — potvrdit, viz 5a.

## 8. NÁVRH POŘADÍ OPRAV V RÁMCI BALÍKU D

Sbalit do JEDNÉ změny „engine hraje podle pravidel" a změřit JEDNOU
(pojistková pre-registrace: **trpaslíci nesmí regredovat; neutrální =
úspěch**; žádná hypotéza o zlepšení).

**DO BALÍKU D (levné, řádkové):**
1. **dodge +1** (§2) — největší dopad, jedna řádka
2. **leap bez modifikátorů** (§3) — jedna řádka, hraje proti nám
3. **follow-up volitelný mimo Frenzy** (§4) — potřebuje rozhodovací vrstvu
   „následovat, nebo zůstat", ne jen flag
4. **Fend: zrušit podmínku `!defKnockedDown`** + **Juggernaut ruší Fend,
   StandFirm i Wrestle při blitzu** (§4b a, b, c) — tři řádky, nulový
   očekávaný dopad na naši pětku (nikdo z nich ty skilly nemá)
5. **throw-in u vysurfovaného nositele** (§5) — přesměrovat na
   `resolveThrowIn`, funkce existuje
6. **surf: `injuryModifier = 1` → `0`** (§5a a) — jedna řádka
7. **tabulka počasí posunutá o jedna** (§5b a) — dva řádky v `enums.h`
8. **Pouring Rain u interception; Rain/Blizzard NEsmí zdražovat pass;
   Blizzard omezuje dosah** (§5b c, d, e)
9. **nega-traity** (TakeRoot u bloku, ReallyStupid soused) — z fronty,
   dosud neověřeno proti textu; **doplnit do téhle tabulky před opravou**

**ODLOŽIT DO BALÍKU G** (všechno jsou to varianty „stav musí přežít konec
drivu", psát jednou, ne třikrát):
* Sweltering Heat (§5b b)
* surf Stunned → rezervy místo KO (§5a b) + stav „rezervy" (§5a c)

**MIMO OBA BALÍKY** (samostatná práce, žádná z našich pěti ras se jich
netýká): Ball & Chain push/follow-up (§4b d), Kick-Off tabulka jako celek
včetně Changing Weather (§5b f, g — dnes neaktivní, `useFullKickoff=false`).

⚠️ Body 2, 8 a 9 hrají spíš proti nám (usnadní elfům leap do klece,
zlevní přihrávkovou hru rychlým rasám, zpřísní naše big guye), body 1,
5, 6 jsou obousměrné. **Proto pojistka, ne hypotéza.**
