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

### ✅ VYŘEŠENO 10.08. — Stunty a Decay u surfu (odpověď uživatele + text + web)

**Obojí u divácké odplaty PLATÍ** — jsou to vlastnosti vyhozeného hráče,
ne davu. Ruší se tedy **jen náš `injuryModifier = 1`**. Ale při ověřování
vyšly najevo dvě věci navíc:

**1. ⛔ DECAY MÁME ZÁSADNĚ ŠPATNĚ (nová chyba, nesouvisí se surfem).**
> „**When this player suffers a Casualty result on the Injury table**,
> roll twice on **the Casualty table** and apply both results. The player
> will only ever miss one future match (…). A successful Regeneration roll
> will heal both results."

Engine (`injury.cpp:17-26`) hází **hod na ZRANĚNÍ dvakrát a bere horší**
⇒ dělá hráče s Decay **snáz vyřaditelným ze zápasu**. Pravidlo přitom do
hodu na zranění **vůbec nesahá**; spustí se až PO casualty a zdvojí hod
na *následky*. ⇒ **Správně implementovaný Decay má v jednom zápase NULOVÝ
efekt.** Oprava = vyndat ho z hodu na zranění úplně.

**2. ⚠️ STUNTY není modifikátor, je to PŘEMAPOVÁNÍ — ale u nás na tom
nesejde.**
> „treats a roll of **7 and 9** on the Injury table after any modifiers
> have been applied as a **KO'd** and **Badly Hurt** result respectively."

Engine dává ploché `+1` (`injury.cpp:29-31`). Liší se to **jen u hodu 9**:
pravidla dávají zaručeně Badly Hurt, +1 udělá náhodnou casualty.
**Jenže náš engine žádnou tabulku následků nemá** (`injuryRoll >= 10`
→ `PlayerState::INJURED`, konec, `injury.cpp:64-66`), takže Badly Hurt
a Casualty jsou pro nás totéž. ⇒ **v modelu jednoho zápasu je ploché +1
chováním totožné**; rozešlo by se to až v lize. **Nesahat.**

**3. ⭐ VEDLEJŠÍ NÁLEZ — proč je `DEAD/hru = 0,00`:** tabulka trvalých
následků v enginu **neexistuje**, takže smrt nemůže nastat. Ranní
pozorování ze 3200 her je tím vysvětlené. Patří k **balíku G** (spolu
s persistencí), ne do D.

**Rozsah 1-3:** Decay ani Stunty nemá nikdo z měřené pětky
(ověřeno v `roster.cpp`) ⇒ latentní.

**⚑ Vazba na balík G (třetí výskyt téhož):** „rezervy do konce půle /
do TD" je **týž chybějící mechanismus** jako přetrvávající zranění
(bod 16) a jako Sweltering Heat (5b/b) — stav, který musí přežít konec
drivu. **Dnes je rozdíl KO vs rezervy prakticky nulový**, protože se po
každém TD staví 11 čerstvých hráčů, takže se vrátí obojí. ⇒ nález b
opravit **až v G**; v D udělat jen nález a (jeden řádek, `injuryModifier`).

---

## 4c. ⛔ STAND FIRM A TAKE ROOT (doplnil uživatel 10.08.) — ŽIVÉ, NE LATENTNÍ

**Pokyn uživatele: „ball and chain nehne ani se stand firm ani s take root
— stand firm je volitelné jako většina aktivních schopností."**
Obojí text potvrzuje, a **na rozdíl od §4b to NENÍ latentní.**

### Pravidla

> **Stand Firm (Strength):** „A player with this skill **may choose** to
> not be pushed back as the result of a block. He may choose to ignore
> being pushed by 'Pushed' results, and to have 'Knock-down' results knock
> the player down in the square where he started. **If a player is pushed
> back into a player using Stand Firm then neither player moves.**"
>
> **Take Root (Extraordinary):** „**Immediately after declaring an Action**
> with this player, roll a D6. On a 2 or more, the player may take his
> Action as normal. On a 1, the player 'takes root', and **his MA is
> considered 0 until a drive ends**, or he is Knocked Down or Placed Prone.
> A player that has taken root may not Go For It, **be pushed back for any
> reason**, or use any skill that would allow him to move out of his
> current square (…). **The player may block adjacent players without
> following-up as part of a Block Action**, however if a player fails his
> Take Root roll as part of a Blitz Action he may not block that turn."

Uživatelův bod o Ball & Chain z těchto dvou skillů **plyne** — není to
zvláštní pravidlo B&C. B&C „musí následovat, **pokud odstrčí**"; proti
Stand Firm / zakořeněnému se odstrčení nekoná, takže se nekoná ani
follow-up. (U nás je to zatím bezpředmětné, protože náš B&C neodstrkuje
nikoho — §4b/d.)

### ⚑⚑ ROZSAH: TOHLE JE ŽIVÉ V dw-we

`getWoodElfRoster1200()` obsahuje **Treeman +Guard** se skilly
`Loner, TakeRoot, StandFirm, MightyBlow, ThickSkull, Guard`
(`roster.cpp:590-591`). ⇒ **Obě schopnosti byly aktivní ve víkendovém
dw-we běhu (400 párů).** Ostatní výskyty (`:89` Deathroller, `:105`,
`:181`, `:260`) jsou v plných rosterech, ne v měřené pětce.

### Nálezy — Stand Firm

| # | nález | kód | verdikt |
|---|---|---|---|
| a | **Stand Firm je vynucený, ne volitelný.** `if (defender.hasSkill(StandFirm)) { pushDest = defender.position; return false; }` — obránce nikdy nedostane volbu. Pravidla: „**may choose**". | `block_handler.cpp:62-69` | ⛔ **BUG** |
| b | **Chain push se o Stand Firm nezajímá.** Když je na cílovém poli hráč se Stand Firm, pravidla říkají „**neither player moves**" — tedy zastaví se i původní odstrčení. Náš chain push jen hledá volné pole a tlačí dál. | `block_handler.cpp:122-145` | ⛔ **BUG** |
| c | Knock-down výsledek srazí hráče na jeho původním poli | `block_handler.cpp:65-67` (`pushDest = defender.position`) | ✅ správně |
| d | Juggernaut při blitzu Stand Firm ruší | `block_handler.cpp:64` | ✅ správně |

### Nálezy — Take Root

| # | nález | kód | verdikt |
|---|---|---|---|
| e | **Hází se jen u MOVE a BLITZ.** Pravidla: „immediately after declaring **an Action**" — tedy i Block, Pass, Hand-off, Foul. | `big_guy_handler.cpp:81` | ⛔ **BUG** (= známý bod 11 „TakeRoot u bloku") |
| f | **⭐ Zakořenění vůbec NEPŘETRVÁVÁ.** Při hodu 1 engine jen zruší tu jednu akci (`actionBlocked`, `hasActed = true`). Pravidla: **MA = 0 až do konce drivu** (nebo do sražení). Žádný příznak „zakořeněn" v enginu neexistuje (grep `rooted/takenRoot` = 0). | `big_guy_handler.cpp:85-91` | ⛔ **BUG, největší z téhle sekce** |
| g | **„Nesmí být odstrčen z jakéhokoli důvodu" — neimplementováno** (a nemá se ani o co opřít, dokud chybí f). Přesně bod uživatele. | `block_handler.cpp:62` testuje jen StandFirm | ⛔ **BUG** |
| h | **Zakořeněný smí blokovat sousedy bez follow-upu** — my mu akci zamítneme celou. | `big_guy_handler.cpp:85-91` | ⛔ **BUG** |
| i | Při neúspěchu Take Root v rámci Blitz Action nesmí ten tah blokovat | tamtéž | ✅ správně (shodou okolností) |

### ✅ STAND FIRM NEZASTAVÍ DRUHÝ ÚDER FRENZY — a engine to má SPRÁVNĚ

**Pokyn uživatele 10.08.: „V BB2016 (i BB2) platí klíčové pravidlo:
Stand Firm nezastaví druhý úder z dovednosti Frenzy."** Ověřeno proti
textu i kódu — **tady nic neopravovat.**

Proč to tak vychází z pravidel: Frenzy vyžaduje druhý blok, „*so long as
they are both still standing and adjacent*". Stand Firm odstrčení zruší,
takže se **nikdo nepohne** — oba tedy stojí a sousedí. Podmínka je
splněná, druhý blok se hází. Stand Firm ho nejenže nezastaví, on ho
**zaručí** (sousednost je zachovaná z definice).

**Engine** (`block_handler.cpp:559-562`):
```cpp
if (!frenzySecondBlock && att.hasSkill(SkillName::Frenzy) &&
    canAct(att.state) && canAct(def.state) &&
    att.position.distanceTo(def.position) == 1)
```
`canAct` = pouze `STANDING` (`enums.h:23-25`) ⇒ podmínka je doslova
„oba stojí a sousedí". Engine netestuje, která stěna padla, ale vychází
to nastejno: Attacker Down / Both Down / jakékoli sražení obránce →
někdo neSTOJÍ → druhý blok nepadá ✓; Pushed i Defender Stumbles s Dodge →
oba stojí → padá ✓. **Shoda s pravidly ve všech větvích.**

**Kontrast s Fendem (§4b):** Fend druhý úder naopak **zastaví** — zakáže
follow-up, takže odstrčený obránce skončí 2 pole daleko a sousednost
zmizí. ⇒ **Proti Frenzy je Fend obrana, Stand Firm ne.**

**⚑⚑ DOKTRINÁLNÍ DŮSLEDEK PRO ROHY KLECE (nový, plyne z toho přímo):**
platí obráceně, než zní obecná obava „Frenzy roh se nechá vytáhnout
z klece". Proti obránci se Stand Firm **se roh nikam neposune** — není
push, tedy není follow-up — a přesto dostane druhý blok.
⇒ **Pro Frenzy roh (Slayera) je blok na Stand Firm obránce pozičně
ZDARMA.** Elfí Treeman je přesně takový cíl a v dw-we stojí na hřišti.
Zařadit k bodu 5 fronty (feasibility rohů) jako výjimku z pravidla
„roh se nesmí nechat vytáhnout".

**⚠️ Co z toho plyne pro opravu §4c/a:** jakmile bude Stand Firm
**volitelný**, rozhodovací vrstva musí vědět, že **zvolit Stand Firm
proti Frenzy útočníkovi si kupuje druhý blok**. Není to tedy jen flag
„nechat se odstrčit ano/ne" — je to obchod: zůstat na pozici výměnou za
další blok proti sobě. Přesně ten typ ocenění, který už máme rozepsaný
pro Frenzy sekvence (fronta 6b, `evidence/frenzy_trap_defence_20260807.md`).

### ⚑⚑ DRUHÁ STRANA MINCE (uživatel 10.08.): NEPOUŽÍT Stand Firm = NÁSTRAHA

**Pozorování uživatele: „asi v AI není moc možností vyhodnotit, že se
aktuálně nehodí použít Stand Firm a vlákat dwarfa do pasti frenzy trap —
i když on asi často na silný strom neútočí."** Obojí platí a dá se
vyčíslit.

**Proč je odmítnutí Stand Firm zbraň:** když se Treeman nechá odstrčit,
Slayer **musí** následovat (Frenzy) a **musí** hodit druhý blok — jenže
už **z jiného pole**. Elf tím Slayera fyzicky vytáhne z pozice a druhý
blok se počítá z geometrie, kterou si elf mohl připravit.

**Dnes je to nemožné DVAKRÁT:**
1. elf **nemá volbu** — Stand Firm je u nás vynucený (§4c/a);
2. i kdyby ji měl, **náš Slayer past neuvidí** — plánovač ocení jen první
   blok z aktuálních pozic a druhý vůbec neuvažuje (fronta 6b).

**Proč „na silný strom stejně neútočí" — čísla:**
Pravidla: shodná ST = 1 kostka · silnější = 2 kostky pro silnějšího ·
více než dvojnásobek = 3. Slayer ST3 vs Treeman ST6 ⇒ **2 kostky PROTI
nám** (6 není *víc* než 2×3, takže ne 3). S asistencemi
(`macro_actions.cpp:131-135` sčítá `ST + asistence` na obou stranách):

| asistence u Treemana | poměr | kostky |
|---|---|---|
| 0 | 3 vs 6 | 2 proti nám |
| 3 | 6 vs 6 | 1 (shoda) |
| **4** | 7 vs 6 | **2 pro nás** |

A generace tahů nabídne BLOCK **jen při `dice >= 2` v náš prospěch**
(`macro_actions.cpp:525-528`) ⇒ **Slayerovi se blok na Treemana vůbec
nenabídne, dokud vedle stromu nestojí 4 trpaslíci.** Spouštěč pasti je
tedy úzký — přesně jak uživatel odhadl.

**Kde past přesto kousne:** těch 6 asistencí je adjacentních ke
**starému** poli Treemana. Po pushi a follow-upu se druhý blok hází
z nové pozice, kde část Guardů už nesousedí ⇒ druhý blok může spadnout
na 1 kostku nebo do kostek proti. To je doslova fronta **7c-II**
(„asistent musí pokrývat i pole PO PUSHI").

**⇒ POŘADOVÉ PRAVIDLO: nešipovat „Stand Firm volitelný" (§4c/a) dřív
než 6b.** Jinak dáme obránci zbraň, kterou náš vlastní útočník neumí
předvídat.
*Poctivá výhrada:* v self-play běží obě strany týmž kódem, takže by
lákadlo **nepoužil nikdo** — chybí táž mašinerie oběma. Na naše A/B to
tedy nemá vliv; je to pravidlo pro **hru člověk vs AI** (položka fronty
z 07.08.) a pro okamžik, kdy budeme mít silnějšího soupeře.

**A obecnější poučení, které z toho plyne:** udělat schopnost volitelnou
znamená **vyrobit nový rozhodovací uzel**. Kdo ho vyhodnotí? MCTS ho
umí navzorkovat, ale u frenzy pasti je ten signál slabý a šumivý
(1 rollout na iteraci, zjištěno 07.08.) — takže odpověď je táž jako
u 6b: **explicitní deterministický výpočet**, ne doufat ve vzorkování.
Je levný: ≤3 push pole z `getPushbackSquares`, follow-up pozice je daná,
kostky z projektovaných pozic jsou uzavřený vzorec.

### ⚑ SMĚR: tohle je první položka balíku D, která hraje PRO NÁS

Náš Take Root je **výrazně mírnější než pravidla**: Treeman u nás ztratí
jednu akci, podle pravidel by měl stát s MA 0 **celý zbytek drivu**.
A náš Stand Firm je **vynucený**, takže Treeman je nepohnutelný vždy,
i když by pro elfa bylo lepší se nechat odstrčit.
⇒ **Obojí dnes nadržuje wood-elfovi.** Oprava by měla trpaslíkům v dw-we
pomoct — na rozdíl od leapu (§3) a přihrávkových nálezů (§5b), které
hrají opačně.
⚠️ **Důsledek pro čtení víkendových dat:** dw-we výsledek (+1,25 pp ± 2,40)
byl naměřen s mírnějším Treemanem, než mají pravidla. Není to důvod ho
přepočítávat (grind byl nula tak jako tak), ale je to důvod **neopírat
o něj budoucí dw-we kalibraci**, dokud D nedoběhne.

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

## 5c. ⛔⛔ NOVÝ NÁLEZ: BLITZER NEMŮŽE POKRAČOVAT V POHYBU PO BLOKU

Vyšlo najevo až při dodatečné kontrole Fendu („*may still continue moving
after blocking if he had declared a Blitz Action*") — ukázalo se, že to
není detail Fendu, ale **obecné pravidlo blitzu, které nemáme vůbec**.

> **BLITZ MOVES:** „A blitz allows the player to move and make a block.
> The block may be made **at any point during the move**, but costs one
> square of movement (…). **The player may carry on moving after the
> effects of the block have been worked out if he has any squares of
> movement left.**"

**Engine:** `ActionType::BLITZ` (`action_resolver.cpp:63-131`) je
atomický: `while (distance > 1) { krok }` → `return resolveBlock(...)`.
A `resolveBlock` na konci nastaví `att.hasActed = true`
(`block_handler.cpp:557`, plus všechny dřívější returny). ⇒ **veškerý
pohyb se spotřebuje PŘED blokem a po bloku už hráč nemůže nic.**
Blok „kdykoli během pohybu" tedy v našem modelu neexistuje — je to vždy
„dojdi až k cíli, praš, konec".

### ⚑ PROČ JE TO VÁŽNÉ: rozbíjí to větve doktríny, které UŽ MÁME navržené

* **Samouvolnění rohu klece** (04.08. i 07.08.): „srazit prvním blokem
  **+ mít MP na návrat na slot**" — tahle větev je dnes **fyzicky
  neproveditelná**. Celá diskuze o tom, jestli Slayer stihne zpátky na
  roh, byla o možnosti, kterou engine nemá.
* **„Otevři a běž" (fronta 6d):** „blitz na nejslabší marker → dodge
  nositele do uvolněného pole → **běh k už stojícím rohům**" — blitzer
  po bloku nikam neběží.
* **Blitz série (fronta 7, 7b):** „úhel příchodu = finální pole podle
  kostek" dostává jiný význam — dnes je finální pole **vynuceně** to,
  ze kterého se blokuje, protože jiné už nebude.
* Fendova věta o pokračování v pohybu je tím pádem bezpředmětná.

### Zařazení — ✅ ROZHODNUTO (uživatel 10.08.)
**Balík C (blitz série), priorita P1, před body 7/7b a 6d.**
Je to pravidlová chyba, ale **NENÍ řádková** — vyžaduje změnu modelu akce
z „přiblížení + blok" na „přiblížení + blok + zbytek pohybu", proto do D
nepatří.
**⚑ Vyhodnocení = SPOLEČNÁ TECHNICKÁ REVIEW nad konkrétní změnou**
(„je to větší oprava, tak na vyhodnocení spolu kouknem, jak koukáme na
technická review"), **ne jen A/B verdikt** — hlavní otázka je
**„jaké to má dopady mimo záměr"**.

---

## 5d. PŘIHRÁVKOVÁ SEKVENCE (uživatel dodal celou 10.08.) — audit

**Většina je SPRÁVNĚ.** Ověřeno proti `pass_handler.cpp`:
* jedna přihrávka za tah (`passUsedThisTurn`) ✅
* rozsahy QP +1 / SP 0 / LP −1 / LB −2 (`passModifier`) ✅
* **Strong Arm**: engine snižuje pásmo o jedno a vynechává QUICK
  (`:197-200`). Je to **přesně ekvivalentní** pravidlu „+1 pro Short,
  Long a Long Bomb" (každý posun pásma = +1) ✅
* Accurate +1 ✅ · −1 za TZ na házeči, rušeno Nerves of Steel ✅
* přirozená 1 = vždy fumble (`:227`) ✅
* nepřesná = 3× rozptyl (`:315-320`) ✅
* **⛔ ale Rain a Blizzard u přihrávky NEMAJÍ co dělat** — viz §5b/d.

### ✅ ZACHYCENÍ: `7 − AG + 2` je SPRÁVNĚ (uzavírá otevřenou otázku 2)
Pravidla dávají −2 k hodu ⇒ +2 k cílovému číslu. **Není to náš přídavek.**

### ⚠️ POŘADÍ: engine má PRAVDU, popis uživatele měl kroky prohozené
Uživatel uvedl „2. hod na přesnost → 3. zachycení (pokud nebyl fumble)".
Text CRP je ale jednoznačný:
> „The coach must declare that one of his players will try to intercept
> **before the thrower rolls to see if he is on target**."

Engine volá `checkInterception` **před** hodem na přesnost
(`pass_handler.cpp:188` vs `:202`) ⇒ **správně, nesahat.** Souhlasí to
i s pořadím v CRP FAQ („4. Check for interceptors … 5. Roll D6 to throw").

### ⛔⛔ PÁSMA PŘIHRÁVKY POČÍTÁME ŠPATNOU METRIKOU (nález 10.08.)
Engine: `passRangeFromDistance(dist)` s prahy **3 / 6 / 10**
(`enums.h:168-173`), kde `dist` je `Position::distanceTo` =
**Chebyshev** `max(|dx|,|dy|)` (`position.h:30-32`).

**Jenže pravítko je fyzická šablona a měří SKUTEČNOU (eukleidovskou)
vzdálenost.** Chebyshev se s ní shoduje jen u přihrávek podél os;
u úhlopříčky je skutečná vzdálenost až **√2 ≈ 1,41×** delší.
⇒ **Úhlopříčné přihrávky jsou u nás systematicky o celé pásmo levnější:**

| Chebyshev (náš práh) | skutečná vzdálenost po úhlopříčce | naše pásmo | mělo by být |
|---|---|---|---|
| 3 | 4,24 | Quick (+1) | **Short (0)** |
| 6 | 8,49 | Short (0) | **Long (−1)** |
| 10 | 14,14 | Long (−1) | **mimo dosah** (LB končí ~13,2) |

⇒ dvojí chyba: **modifikátor o stupeň štědřejší** a **nejdelší
úhlopříčky vůbec povolené**, ačkoli na pravítko nedosáhnou.
**Směr: nadržuje to přihrávající straně, tedy rychlým rasám.**

**⭐ MŘÍŽKU DODAL UŽIVATEL 10.08.** („Regular Throwing Ranges", `~/pass.png`).
Tři kontrolní pole, čitelná bezpečně, potvrzují nález:

| pole | mřížka | náš engine |
|---|---|---|
| (3,3) | **S** | Q |
| (6,6) | **L** | S |
| (10,10) | **mimo dosah** | L |

**⛔⛔ A NAVÍC: NENÍ TO ANI VZDÁLENOSTNÍ VZOREC.**
`(13,0)` je na mřížce **B**, ale `(5,12)` je **šedá** — obě mají
skutečnou vzdálenost **přesně 13,00**. Žádná funkce vzdálenosti to
nerozliší. ⇒ pravítko je **tvarovaná fyzická šablona**, ne kružítko.
**⇒ IMPLEMENTOVAT TABULKOU (14×14 lookup podle |dx|,|dy|), NE VZORCEM.**
Eukleidovská aproximace (hranice 3,49/6,98/10,47/13,22) sedí ve většině
polí, ale na okrajích pásem se rozchází — jako náhrada nestačí.

⚠️ **Tabulku je ještě potřeba PŘESNĚ OPSAT z obrázku.** Krajní pole se
mi z rozlišení nedají číst spolehlivě a **PIL v prostředí není**
(nešlo výřez zvětšit). ⇒ **implementační krok: opsat mřížku a nechat
uživatele zkontrolovat** — je to 196 polí, jednou a navždy.

### ⚠️ Drobná odchylka: kdo SMÍ zachytávat
Pravidla: (a) **pravítko musí přejít aspoň část pole**, na kterém stojí,
(b) má tackle zónu, (c) je blíž házeči, než je házeč k cíli, (d) je blíž
cíli, než je házeč k cíli; **jen jeden hráč**.
Engine jde po Bresenhamově přímce a bere prvního stojícího soupeře **na**
ní. Body (b) a „jen jeden" ✅; (c)+(d) plynou z toho, že leží na úsečce ✅.
**Ale skutečné pravítko má ŠÍŘKU** — přejede i pole, která matematická
přímka mine ⇒ **náš engine připouští MÉNĚ zachytávačů, než pravidla**.

**📐 UPŘESNĚNÍ (dohledáno na webu 10.08. na pokyn uživatele) — dá se to
implementovat přesně:** vede se úsečka **ze středu pole házeče do středu
cílového pole**, a **zachytávat smí hráč, jehož střed pole je od
nejbližšího bodu té úsečky vzdálen ≤ 1 šířku pole**.
⇒ Není to přímka, je to **koridor o poloměru 1**. Náš Bresenham bere
prakticky jen „vzdálenost 0", takže povolujeme řádově **~třetinu**
oprávněných zachytávačů.
Ostatní podmínky (má TZ · je blíž házeči, než je házeč k cíli · je blíž
cíli, než je házeč k cíli · jen jeden hráč) zůstávají.
⇒ **Implementace:** kolmá vzdálenost středu pole od úsečky ≤ 1.
Levné, a odstraní to i dnešní závislost na Bresenhamově diskretizaci.
**Směr: nadržuje to přihrávající straně (rychlým týmům).**

**⚠️ POZOR NA EDICI (uživatel poslal 10.08. odkaz na `bloodbowlbase.ru/bb2025`):
to je BB2025, NE naše 2016 — čísla odtud NEPŘEBÍRAT.** Rozdíly:

| | BB2016 / CRP (naše) | BB2025 |
|---|---|---|
| kdy | zachycení se deklaruje **PŘED** hodem na přesnost | pravítko se pokládá **AŽ PO** určení výsledku |
| kam | pravítko k **cílovému** poli | k poli, **kam míč skutečně dopadne** |
| postih | plochých **−2** | **−3** přesná / **−2** nepřesná |
| markeři | (nemodifikuje) | **−1 za každého** markera zachytávače |

**Jediné, co je z toho přenosné** (a potvrzuje to nezávisle náš závěr):
formulace „*If the Range Ruler **overlaps any squares** containing a
Standing opposition player*" — tedy **překryv PLOCHY, ne průsečík
s přímkou**. Náš Bresenham je tím pádem prokazatelně špatně i podle
novější edice.

### ❌ ODVOLÁNO: „házeč se po přihrávce nemůže hýbat" NENÍ CHYBA
**Původně jsem to 10.08. zapsal jako nález a bylo to ŠPATNĚ.** Vzal jsem
za hotové tvrzení z dodaného popisu („hráč se může pohybovat před hodem
i po něm"), místo abych sáhl do `rules_crp2016.txt`. **Korekce uživatele
(„po pass se může hýbat jen podle nějakých nových skillů a pravidel —
ne v 2016") je správná** a text ji potvrzuje doslova:

> **Pass:** „Pass: The player may move a number of squares equal to his MA.
> **At the end of the move** the player may pass the ball." · „Once you
> have thrown the ball, however, **you may not move the throwing player
> any farther that turn, even if he has spare MA left**."
>
> **Hand-off:** „A player may move before performing the hand-off, but
> **once he attempts to hand-off the ball, he may not move the player
> performing the Hand-Off Action any farther that turn**."
>
> **Foul:** „move a number of squares equal to his MA **and then** make
> a foul."

⇒ `passer.hasActed = true` (`pass_handler.cpp:119`), totéž u hand-offu
(`:362`) i faulu (`foul_handler.cpp:75`) je **SPRÁVNĚ. Nesahat.**

**⚑ A tím padá i zobecnění „chyba modelu akce se čtyřmi výskyty".**
**BLITZ je v pravidlech VÝJIMKA** — jediná akce, kde se jádro provádí
uprostřed pohybu a pohyb pokračuje po něm:
> „The block may be made **at any point during the move** (…) The player
> **may carry on moving after** the effects of the block have been worked
> out if he has any squares of movement left."

⇒ **§5c (blitz) PLATÍ a zůstává samostatnou položkou P1**; přihrávka,
předání ani faul se s ní neslučují.

### 5e. CHYTACÍ A ZACHYTÁVACÍ SKILLY (uživatel dodal 10.08.) — audit

| skill | pravidla (CRP) | engine | verdikt |
|---|---|---|---|
| **Catch** | „re-roll the D6 if he fails a catch roll. It also allows the player to re-roll the D6 if he drops a hand-off or **fails to make an interception**." | `resolveCatch` reroll ✅, ale **zachycení hází holým `dice.rollD6()`** bez rerollu (`pass_handler.cpp:~85`) | ⛔ **BUG, AKTIVNÍ** |
| **Diving Catch** | „+1 to any catch roll **from an accurate pass targeted to his square**" | `helpers.cpp:144` dává −1 k cíli **bezpodmínečně** — tedy i u nepřesných přihrávek, odrazů, vhazování a předání | ⛔ **BUG** (příliš štědré), latentní |
| **Diving Catch, 2. půlka** | může chytit míč padající do **prázdného pole ve své TZ** (ne odražený) jako by padl na něj | **neimplementováno** | ⛔ chybí, latentní |
| **Very Long Legs** | „+1 (…) whenever he attempts to **intercept** or uses the **Leap** skill" | zachycení ✅ (`pass_handler.cpp:75`), leap ✅ (`move_handler.cpp:240`), do **chytání se NEPLETE** ✅ | ✅ **správně** |
| **Extra Arms** | „+1 to any attempt to **pick up, catch or intercept**" | všechny tři ✅ (`helpers.cpp:125`, `:143`, `pass_handler.cpp:76`) | ✅ **správně** |
| **Pass Block** | pohyb až o 3 pole mimo pořadí, po změření vzdálenosti a **před pokusy o zachycení** | v enginu **není vůbec** (grep = 0) | ➕ chybí; žádný roster ho nemá ⇒ latentní, a je to **mimořádová akce** = větší kus |

### ⭐⭐ RODINA MIMOPOŘADOVÝCH SCHOPNOSTÍ — a systematický vzorec
Uživatel 10.08.: „**největší šílenost je tam Pass Block**." Souhlas —
a při kontrole vyšlo najevo, že patří do rodiny tří, u které náš engine
dělá **pokaždé tutéž chybu**.

| skill | pravidla | engine |
|---|---|---|
| **Pass Block** | mimopořadový pohyb až o 3 pole; hází se dodge, hráč může spadnout | **neexistuje** |
| **Shadowing** | při opuštění TZ „**for any reason**"; 2D6 + MA soupeře − MA stínujícího ≤ 7 ⇒ smí následovat | `checkShadowing` **existuje** (`move_handler.cpp:44-71`), ale váže se **jen na dodge** ⇒ únik **leapem** z TZ ho nespustí (leap se nedodgeuje) |
| **Diving Tackle** | „**MAY use this skill**"; −2 soupeři, ale **Diving Tackle hráč je Placed Prone** v poli, které soupeř opustil | `helpers.cpp:99-108`: +2 k cíli **automaticky**, a **nikdy se nepokládá** ⇒ **efekt zdarma a bez volby** |

**⚑ VZOREC, KTERÝ Z TOHO PLYNE (a je doložený 4× nezávisle):**
**náš engine mění VOLITELNÁ obranná rozhodnutí na AUTOMATICKÉ pasivní
modifikátory a zahazuje jejich CENU.**
* Stand Firm — vynucený, bez volby (§4c/a)
* Diving Tackle — automatický, bez ceny (položení na zem)
* follow-up — vynucený všem (§4)
* Fend — špatně podmíněný (§4b)

**Dva důsledky:**
1. **Obranná strana je u nás systematicky levnější, než má být** — dostane
   efekty zadarmo a nikdy za ně neplatí pozicí.
2. **AI se ta rozhodnutí nemá jak naučit**, protože je nikdy nedělá.
   Souvisí s otevřenou otázkou 3 („co bude default u volitelných
   schopností") — tohle je její čtvrtý a pátý výskyt.

**Zařazení:** Diving Tackle (volitelnost + položení na zem) a Shadowing
(spouštěč „any reason") patří **k D-vlně 2** mezi rozhodovací uzly.
**Pass Block je jiná liga** — vyžaduje, aby *pasivní* strana rozhodovala
uprostřed cizí akce, včetně vlastních kostek a vlastního pádu. To není
parity řádek, je to **změna řízení toku**; navíc ho nikdo z rosterů nemá.
⇒ **mimo D i P1**, k evidenci jako známý dluh.

**⚠️ KOREKCE dodaného popisu:** Very Long Legs **nedává +1 k chytání**
(jen k zachycení a leapu) — engine to má správně, popis ne. Opět platí
[[feedback-ask-rules-questions-in-game-terms]] bod 5: dodaný text je
vstup, ne verdikt.

**Rozsah:** `Catch` **MÁ** wood-elf (2× Catcher, `roster.cpp:587`)
⇒ **oprava rerollu zachycení je AKTIVNÍ a hraje PROTI nám** (elfové budou
zachytávat spolehlivěji). `DivingCatch` má jen Slann (`:393`),
`PassBlock` nikdo ⇒ latentní.

### 📌 SPP (Star Player Points) — v enginu NEEXISTUJÍ
`grep -i spp` v `engine/` = **0 výskytů**. Existují jen v PHP ligové
části (`src/Service/SPPService.php`). ⇒ Pro AI **není za úspěšnou
přihrávku žádná odměna** kromě výsledku zápasu; C++ engine je
jednozápasový bez ligové progrese. Relevantní až pro balík G/E (ligový
model), ne pro D.

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

## 5f. VOLBA PUSH POLE + SIDE STEP × GRAB (doplnil uživatel 10.08.)

> **Push Backs:** „**The coach of the player who made the block may decide
> which square** the player is moved to." (u řetězových odsunů rozhoduje
> kouč táhnoucího týmu)
>
> **Side Step:** „his coach may choose which square the player is moved
> to (…) **rather than the opposing coach**. Furthermore, the coach may
> choose to move the player to **ANY adjacent square, not just the three
> squares** shown on the Push Back diagram."
>
> **Grab:** „**only while making a Block Action** (…) he may choose **any
> empty square adjacent to his opponent**. **When making a Block or Blitz
> Action, Grab and Side Step will cancel each other out and the standard
> pushback rules apply.**"

| # | nález | kód | verdikt |
|---|---|---|---|
| a | **Volba útočníka se vůbec nemodeluje** — bere se **první volné** pole z trojice | `block_handler.cpp:113-118` | ⛔ **BUG** (pátý výskyt vzorce „automaticky místo volby") |
| b | **Side Step má jen 3 pole místo 8** — vybírá nejvzdálenější z `pushSquares` | `:88-95` | ⛔ **BUG** (příliš slabý) |
| c | **Grab má taky jen 3 pole místo všech volných sousedních** | `:96-108` | ⛔ **BUG** (příliš slabý) |
| d | **Rušení Grab × Side Step chybí.** Pravidla: při Block **i Blitz** se navzájem **ruší** ⇒ platí standard (volí útočník ze 3). Engine: u **bloku** dá plný Grab útočníkovi, u **blitzu** dá Side Step obránci | `:88-96` | ⛔ **BUG obojím směrem** |
| e | Grab funguje jen mimo blitz (`!isBlitz`) | `:96` | ✅ správně |
| f | **Side Step bez volného pole se NEPOUŽIJE** — „the player **may not use this skill if there are no open squares on the pitch adjacent** to this player" ⇒ vrací se to k **určení útočníkem** (a dál standardní řetězový odsun). Engine podmínku **nekontroluje** a vybírá z trojice bez ohledu na obsazenost | `:88-95` | ⛔ **BUG** (doplnil uživatel 10.08.) |
| g | Side Step platí **i když je hráč po odsunu sražen** („even if the player is Knocked Down after the push back") — engine na výsledek bloku nepodmiňuje | `:88` | ✅ správně |

**⚑ Pořadí vyhodnocení, které z toho plyne** (a je to celé jedna větev,
ne čtyři nezávislé podmínky):
1. má obránce **Side Step** a **existuje volné sousední pole**?
   → ale **má útočník Grab?** (Block i Blitz) → **ruší se** → bod 3
2. ano → **obránce volí z KTERÉHOKOLI volného sousedního pole** (8, ne 3)
3. jinak → **útočník volí ze tří standardních polí**; má-li Grab a jde
   o **Block Action**, volí z **kteréhokoli volného sousedního**
4. není-li volné pole ⇒ řetězový odsun (směr určuje kouč táhnoucího týmu)
   / vytlačení z hřiště

**Rozsah:** Side Step **má** wood-elf Wardancer (`roster.cpp:585-586`)
⇒ body b a d jsou **AKTIVNÍ v dw-we**. Grab v měřené pětce nikdo.

## 5g. ⭐ MŘÍŽKA DOSAHŮ PŘIHRÁVEK — HOTOVÁ
Plná tabulka: **`evidence/pass_range_grid_20260810.txt`**.
Zdroj: obrázek uživatele + jeho oprava řady 7 („0 až 7 je L, pak 3× B")
+ **kontrola symetrie** `band(dx,dy) == band(dy,dx)` (pravítko nezajímá
směr) — prošla, a uživatelovy hodnoty pro řady 8/9/10 (poslední L na
6/4/3) z rekonstrukce **vyšly samy** = nezávislé potvrzení.

**Dopad: 79 ze 196 polí je u nás špatně**, a **všechna v náš neprospěch**:

| máme | má být | počet |
|---|---|---|
| B | mimo dosah | 45 |
| L | B | 19 |
| S | L | 6 |
| L | mimo dosah | 6 |
| Q | S | 3 |

⇒ **Ve 45 polích povolujeme přihrávku, která podle pravidel není možná**,
a ve zbytku je o pásmo levnější. Systematicky to nadržuje přihrávkové
hře, tedy rychlým rasám.

## 5h. ⭐ DIVING TACKLE — deklaruje se AŽ PO HODU (a po rerollech)

Ověřeno v CRP (uživatel na to upozornil 10.08.):
> „Diving Tackle **may be used on a re-rolled dodge if not declared for
> use on the first Dodge roll**. **Once the dodge is resolved** but before
> any armour roll for the opponent, the Diving Tackle Player is **Placed
> Prone in the square vacated by the dodging player**, but do not make an
> Armour or Injury roll for the Diving Tackle player."

**⇒ Pořadí je: hod → (Dodge reroll / týmový reroll) → TEPRVE PAK se
obránce rozhoduje o Diving Tackle → −2 na finální výsledek.**
Útočník už pak nepřehazuje.

**Co z toho plyne prakticky:**
* Obránce **vidí výsledek, než zaplatí**. Nedeklaruje naslepo ⇒
  **nikdy ho neutratí zbytečně**: při přirozené 1 (dodge stejně selhal)
  ani při hodu tak vysokém, že by i s −2 prošel, nemá důvod ho použít.
* Cena se platí **jen když to změní výsledek**.
* **Diving Tackle NERUŠÍ skill Dodge** — to dělá `Tackle`. Je to jen
  plošných −2, takže útočník svůj Dodge reroll použít smí.

**⛔ NÁŠ ENGINE TO MÁ JINAK, A TO DVOJÍ CHYBOU** (`helpers.cpp:99-108`):
1. **aplikuje +2 k cíli PŘED hodem a automaticky** ⇒ rozhoduje naslepo
   tam, kde pravidla dávají informaci;
2. **nikdy nepokládá Diving Tackle hráče na zem** ⇒ efekt je zadarmo.
⇒ Není to „volitelné vs vynucené" jako Stand Firm — je to **jiná
mechanika**: post-hoc deklarace s cenou, ne předhozený modifikátor.
**Správně implementovaný Diving Tackle je pro obránce SILNĚJŠÍ** (nikdy
se neplýtvá) **a zároveň DRAŽŠÍ** (skončí na zemi) než ten náš.

## 5i. BOMBA vs PŘIHRÁVKA vs TTM (uživatel dodal srovnání 10.08.)

Ověřeno proti CRP — **uživatel měl pravdu ve všech třech bodech, které se
týkaly bomb, a náš engine je měl obráceně.**

> **Bombardier:** „throw a bomb instead of taking any other Action with the
> player. **This does not use the team's Pass Action for the turn.** The
> bomb is thrown using the rules for throwing the ball (…) except that
> **the player may not move or stand up before throwing it**. **Intercepted
> bomb passes are not turnovers.** (…) the player catching it **must throw
> it again immediately. This is a special bonus Action that takes place out
> of the normal sequence of play.** (…) The bomb explodes when it lands in
> an empty square or an opportunity to catch it fails or is declined
> (**bombs don't 'bounce'**). If the bomb is fumbled it explodes in the bomb
> thrower's square. If a bomb lands in the crowd, it explodes with no
> effect."

| # | nález | verdikt |
|---|---|---|
| a | **Bomba spotřebovávala týmovou Pass akci** — `bomb_handler.cpp:14` nastavoval `passUsedThisTurn`, a `rules_engine.cpp` bombu navíc **gatoval** na `!passUsedThisTurn` ⇒ tým tiše přišel o přihrávku | ✅ **OPRAVENO 10.08.** |
| b | **Chybělo „nesmí se hnout ani vstát"** — bombu šlo hodit po pohybu i vleže | ✅ **OPRAVENO** (`!p.hasMoved && STANDING`) |
| c | dosah bomby byl Chebyshev ≤13 | ✅ **OPRAVENO** — mřížka pravítka |
| d | **Zachycená bomba: „interceptor ji musí ihned hodit znovu"** — mimopořadová bonusová akce | ⛔ **CHYBÍ** — táž třída jako Pass Block (§4b), řízení toku |
| e | „bomby se neodrážejí" · fumble vybuchne u vrhače · dopad do davu bez efektu | ⚠️ neověřeno v kódu, doplnit |

**Rozsah:** Bombardier má jen **Goblin** (`roster.cpp:305`) ⇒ latentní.

### TTM — ověřeno, většinou správně
> „The pass is worked out exactly the same as if the player with Throw
> Team-Mate was passing a ball, except **the player must subtract 1** from
> the D6 roll, **fumbles are not automatically turnovers**, and **Long Pass
> or Long Bomb range passes are not possible**. (…) accurate passes are
> **treated instead as inaccurate** thus **scattering the thrown player
> three times**. The thrown player **cannot be intercepted**. A fumbled
> team-mate will **land in the square he originally occupied**."

✅ **Max Short Pass DOPLNĚNO 10.08.** (dřív šel TTM na Long Bomb).
To je i ten žlutý „Max. TTM" v legendě mřížky.
⚠️ Zbytek (−1, nikdy accurate, 3× scatter, bez interceptu, fumble na
původní pole, fumble není turnover) **ověřit v `ttm_handler.cpp`** —
zatím neprojito.

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
9. **⭐ Stand Firm volitelný + chain push se o něj zastaví** (§4c a, b) —
   **ŽIVÉ v dw-we**, potřebuje rozhodovací vrstvu „nechat se odstrčit?",
   ne jen flag
10. **⭐⭐ Take Root: hod u KAŽDÉ akce + PŘETRVÁVAJÍCÍ zakořenění
    (MA 0 do konce drivu) + nelze odstrčit + smí blokovat bez follow-upu**
    (§4c e–h) — **ŽIVÉ v dw-we a hraje PRO NÁS**; jediná položka D, která
    potřebuje nový stav na hráči, ne jen řádek
11. **zbytek nega-traitů** (ReallyStupid soused) — dosud neověřeno proti
    textu; **doplnit do téhle tabulky před opravou**

**ODLOŽIT DO BALÍKU G** (všechno jsou to varianty „stav musí přežít konec
drivu", psát jednou, ne třikrát):
* Sweltering Heat (§5b b)
* surf Stunned → rezervy místo KO (§5a b) + stav „rezervy" (§5a c)

**MIMO OBA BALÍKY** (samostatná práce, žádná z našich pěti ras se jich
netýká): Ball & Chain push/follow-up (§4b d), Kick-Off tabulka jako celek
včetně Changing Weather (§5b f, g — dnes neaktivní, `useFullKickoff=false`).

⚠️ **Směry se míchají, proto pojistka a ne hypotéza:**
* **proti nám:** leap bez modifikátorů (§3), přihrávkové nálezy (§5b c–e)
  — zlevní hru rychlým rasám
* **pro nás:** Stand Firm a Take Root (§4c) — dnes obojí nadržuje
  wood-elfovi
* **obousměrné:** dodge +1, follow-up, throw-in po surfu, surf injury

⚠️ **Jediná položka, která NENÍ řádková: Take Root** (§4c/f potřebuje
příznak „zakořeněn" přežívající do konce drivu). Kdyby se D mělo držet
čistě u řádkových oprav, je to jediný kandidát na vyčlenění — ale patří
věcně sem, protože je to pravidlová chyba, ne feature.

---

## 9. ROSTERY: proč tam některé schopnosti NEJSOU (uživatel 10.08.)

**Orkové nemají Trolla, a tím ani Throw Team-Mate — je to VĚDOMÉ
rozhodnutí, ne opomenutí.** Uživatel 10.08.: *„TTM u orků jsme schválně
dali pryč kvůli nespolehlivosti."*
Důvod je dvouvrstvý a v pravidlech doložitelný:
1. **Troll je Really Stupid** ⇒ aktivace selže na 1-3 (bez souseda, který
   sám není Really Stupid) — investice do přípravy hodu se často nekoná;
2. **TTM navíc NIKDY není přesný** — CRP: „accurate passes are treated
   instead as inaccurate thus **scattering the thrown player three times**"
   ⇒ i po úspěšném hodu je dopad náhodný.
⇒ Dvě nespolehlivosti za sebou; roster `getOrcRoster1200()` má místo Trolla
4 Black Orky. **Nevracet Trolla zpět bez nového důvodu.**

### 📌 Rozsah přihrávkových zvláštností v měřené pětce = NULA
Ověřeno `grep` přes `getDwarfRoster1200 / Skaven / WoodElf / Human /
Orc1200`: **nikdo nemá ThrowTeamMate, RightStuff ani Bombardier.**
⇒ Všechna dnešní práce na TTM a bombách je **latentní** — korektnostní,
s nulovým dopadem na jakékoli měření. Zapsáno i jako poznámka
o prioritizaci: udělal jsem ji proto, že přišla v hovoru, ne proto, že
byla na řadě.
⇒ **Vedlejší přínos: `orc-sk` je díky tomu ČISTÁ KONTROLA** pro noční
srovnání ér — není dotčená přihrávkovými zvláštnostmi, jen obecnými
změnami (dodge, leap, počasí, mřížka dosahů, zachycení).
