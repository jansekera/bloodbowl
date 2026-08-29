# Audit shody s pravidly BB2016 — dovednosti bez citace + mrtvé dovednosti

Datum: 2026-08-29
Zdroj pravidel: `rules_bb2016.txt` (9518 řádků) — **jediný platný zdroj**, každé tvrzení níže má číslo řádku a doslovnou citaci.
Nástroj: `python3 check_rules_citations.py` (230 citací celkem, 135 v `src/`, 95 v testech).
Rozsah kódu: `engine/src/*.cpp`, `engine/include/bb/*.h`. **Nic v `engine/` nebylo změněno.**

---

## Shrnutí

**20 nálezů, z toho 7 ŽIVÝCH.**

Všech 12 auditovaných dovedností (7 z Priority 1 + 5 z Priority 2) je vůči korpusu **LATENTNÍCH** — ověřeno čtením `engine/src/roster.cpp:494-635`, tedy pěti sestav `getOrcRoster1200 / getHumanRoster1200 / getDwarfRoster1200 / getSkavenRoster1200 / getWoodElfRoster1200`. Ty obsahují dohromady jen: Block, Guard, Tackle, MightyBlow, StripBall, SureHands, Pass, Wrestle, Catch, Dodge, Sprint, SureFeet, Leap, SideStep, ThickSkull, Frenzy, Dauntless, StandFirm, TakeRoot, Loner, BoneHead, ThrowTeamMate. **Ani jedna z auditovaných dvanácti tam není.** To potvrzuje i výstup `check_rules_citations.py` („HRAJE V KORPUSU (0)“).

Všech 7 živých nálezů pochází z **kick-off tabulky** (`engine/src/kickoff_handler.cpp`) — narazil jsem na ně při ověřování `KickOffReturn`, který v tomtéž souboru bydlí. Kick-off se hraje na začátku každého drive, takže tyhle vady se platí v každé hře korpusu.

| | Živé | Latentní |
|---|---|---|
| Priorita 1 (7 dovedností bez citace) | 0 | 6 |
| Priorita 2 (5 dovedností mrtvých v `src/`) | 0 | 5 |
| Priorita 3 (ostatní) | 7 | 2 |

---

# ČÁST A — ŽIVÉ NÁLEZY (hrají se v korpusu TV1200 každou hru)

Všechny se týkají výsledku hodu 2D6 na kick-off tabulku, který v `resolveKickoff()` padá při každém rozehrání (`engine/src/kickoff_handler.cpp:275-277`).

---

## Ž1 — Kick-off `Blitz!`: místo bonusového tahu jen posun o jedno pole

**1. Kód:** `engine/src/kickoff_handler.cpp:132-142` (case `KickoffEvent::BLITZ`)

**2. Pravidla:** ř. 1335-1341
> „10 Blitz! The defence start their drive a fraction before the offence is ready, catching the receiving team flat-footed. The kicking team receives a free ‘bonus’ turn: however, players that are in an enemy tackle zone at the beginning of this free turn may not perform an Action. The kicking team may use team re-rolls during a Blitz. If any player suffers a turnover then the bonus turn ends immediately.“

**3. Co kód dělá místo toho:** projde stojící hráče kopajícího týmu a každému zavolá `movePlayerToward(state, p.id, losTarget)`, tedy posune ho **o jedno pole směrem k linii scrimmage** (a jen pokud existuje volné sousední pole striktně blíž). Žádný bonusový tah se neuděluje — kopající tým nedostane ani jednu akci, nemůže blokovat, blitzovat, sebrat míč ani skórovat, a pravidlo „hráči v soupeřově tackle zóně nesmí hrát akci“ nemá co omezovat.

**4. ŽIVÉ** — `Blitz!` je hod 10 na 2D6, ≈8,3 % kick-offů, a nezávisí na žádné dovednosti.

**5. Komu to prospívá / škodí:** symetricky se to týká obou stran (každý tým kope zhruba polovinu drive), ale **hodnota bonusového tahu není symetrická**. Volný tah pro kopající tým využije nejlíp ten, kdo se za jeden tah dostane k míči nebo k nosiči — tedy rychlé sestavy (skaven MA9 Gutter Runner, wood-elf MA8 Wardancer). Trpaslík s MA4-5 by z bonusového tahu vytěžil nejmíň. **Oprava tedy pravděpodobně posílí soupeře, ne nás** — a to se nesmí číst jako regrese AI.

**6. Odhad velikosti opravy:** nová vrstva. Bonusový tah znamená pustit celý turn-loop pro kopající tým uvnitř kick-off fáze, s příznakem „hráči v enemy TZ nesmí akci“ a s ukončením při turnoveru. Není to změna v jednom souboru — vyžaduje háček do `turn_handler` / `game_simulator` a rozmyšlení, jak se to promítne do MCTS horizontu.

---

## Ž2 — Kick-off `Throw a Rock`: chybí souboj D6, a místo zranění jen stun

**1. Kód:** `engine/src/kickoff_handler.cpp:143-167` (case `KickoffEvent::THROW_A_ROCK`)

**2. Pravidla:** ř. 1342-1350
> „11 Throw a Rock: An enraged fan hurls a large rock at one of the players on the opposing team. Each coach rolls a D6 and adds their FAME to the roll. The fans of the team that rolls higher are the ones that threw the rock. In the case of a tie a rock is thrown at each team! Decide randomly which player in the other team was hit (only players on the pitch are eligible) and roll for the effects of the injury straight away. No Armour roll is required.“

**3. Co kód dělá místo toho:** tři odchylky naráz.
   - (a) Žádný souboj D6 mezi trenéry — kód **vždy** zasáhne po jednom hráči **v obou týmech**. Podle pravidel dostane kámen jen tým, jehož fanoušci prohráli hod (tj. jeden tým), a oba týmy jen při remíze (1/6 případů). Kód tedy zasahuje dvakrát tolik hráčů, kolik má.
   - (b) Zasažený hráč dostane `PlayerState::STUNNED` (`kickoff_handler.cpp:159`). Pravidla říkají `roll for the effects of the injury straight away. No Armour roll is required` — tedy **hod na tabulku zranění bez hodu na brnění**, který může skončit KO nebo casualty. Kámen v našem enginu nikdy nikoho nevyřadí ze hry.
   - (c) Cíl se vybírá jen mezi stojícími hráči (`if (p.state != PlayerState::STANDING) return;`, ř. 156). Pravidla říkají `only players on the pitch are eligible`, tedy i ležící a omráčení.
   - Navíc `int target = dice.rollD6() % count;` (ř. 153) je zkreslený výběr, pokud `count` nedělí 6 — komentář to sám přiznává („simplified random selection“).

**4. ŽIVÉ** — hod 11 na 2D6, ≈5,6 % kick-offů.

**5. Komu to prospívá / škodí:** bod (b) **prospívá soupeřům**. Kámen je jediný kanál na kick-offu, který dokáže někoho trvale vyřadit; tím, že se z něj stal garantovaný stun, se maže náš strukturální náskok — trpaslík má AV9 + Thick Skull a v hodu na zranění by rockem trpěl výrazně míň než skaven (AV7) nebo wood-elf (AV7). Bod (a) je proti tomu zhruba symetrický. Netto: současný stav **stírá naši odolnostní výhodu**, oprava by ji vrátila.

**6. Odhad velikosti opravy:** ~25 řádků v jednom souboru (`kickoff_handler.cpp`). Injury roll bez armour rollu už existuje jako `resolveInjuryRoll` v `engine/src/injury.cpp`, takže stačí zavolat ho a doplnit D6 souboj.

---

## Ž3 — Kick-off `Riot`: chybí hod D6 a kopající tým se nikdy nehýbe

**1. Kód:** `engine/src/kickoff_handler.cpp:63-71` (case `KickoffEvent::RIOT`)

**2. Pravidla:** ř. 1285-1297
> „3 Riot: The trash talk between two opposing players explodes and rapidly degenerates, involving the rest of the players. If the receiving team’s turn marker is on turn 7 for the half, both teams move their turn marker back one space as the referee resets the clock back to before the fight started. If the receiving team has not yet taken a turn this half the referee lets the clock run on during the fight and both teams’ turn markers are moved forward one space. Otherwise roll a D6. On a 1-3, both teams’ turn markers are moved forward one space. On a 4-6, both team’s turn markers are moved back one space.“

**3. Co kód dělá místo toho:**
```cpp
TeamState& recvTeam = state.getTeamState(receiving);
if (recvTeam.turnNumber <= 1) {
    recvTeam.turnNumber++;   // Receiving team loses a turn
} else {
    recvTeam.turnNumber--;   // Extra turn
}
```
   - (a) Mění **jen přijímající tým**. Pravidla třikrát opakují „**both teams** move their turn marker“. Kopajícímu týmu se ukazatel tahu nikdy nepohne, takže Riot v enginu vždycky mění poměr zbývajících tahů mezi týmy — což pravidla nikdy nedělají.
   - (b) Chybí hod D6. Mimo případ „ještě nehrál tah“ kód **vždy** posune ukazatel zpět (tj. tah navíc). Pravidla mají 50/50 D6: na 1-3 dopředu (tah se ztrácí), na 4-6 zpět. Engine tedy z Riotu udělal deterministický bonus.
   - (c) Zvláštní případ „turn marker on turn 7“ se s (b) shodou okolností potkává na správném výsledku, ale jen protože větev `else` vždy couvá.

**4. ŽIVÉ** — hod 3 na 2D6, ≈5,6 % kick-offů.

**5. Komu to prospívá / škodí:** **prospívá přijímajícímu týmu**, tedy vždy tomu, kdo právě dostal kop — a to je po každém našem TD soupeř. Tah navíc pro útočící stranu má vysokou hodnotu (jedno rozehrání navíc na doskórování). Vzhledem k tomu, že to nezávisí na rase, je to zhruba symetrické přes celý zápas, ale **systematicky prodlužuje drive útočníka**, což tlačí na remízové výsledky — a to je hlavní otevřené téma projektu (0-0 draw collapse).

**6. Odhad velikosti opravy:** ~12 řádků v jednom souboru. Pozor: `recvTeam.turnNumber` je na ř. 217 už jednou inkrementováno, takže „has not yet taken a turn“ odpovídá `turnNumber == 1`, ne `0`.

---

## Ž4 — Kick-off `Cheering Fans` / `Brilliant Coaching`: při remíze nedostane reroll nikdo (mají oba)

**1. Kód:** `engine/src/kickoff_handler.cpp:90-100` (CHEERING) a `102-112` (BRILLIANT_COACHING); reroll se přičítá na ř. 95, 97, 106, 108.

**2. Pravidla:**
   - Cheering Fans, ř. 1309-1314: „6 Cheering Fans: Each coach rolls a **D3** and adds their team’s FAME and the number of cheerleaders on their team to the score. The team with the highest score is inspired by their fans' cheering and gets an extra re-roll this half. **If both teams have the same score, then both teams get a re-roll.**“
   - Brilliant Coaching, ř. 1323-1326: „8 Brilliant Coaching: Each coach rolls a **D3** and adds their FAME and the number of assistant coaches on their team to the score. The team with the highest total gets an extra team re-roll this half thanks to the brilliant instruction provided by the coaching staff. **In case of a tie both teams get an extra team re-roll.**“

**3. Co kód dělá místo toho:** hodí `dice.rollD6()` za každý tým; při `homeRoll > awayRoll` dá reroll domácím, při `awayRoll > homeRoll` hostům, **a při shodě neudělá nic** (komentář na ř. 91 to přiznává: „ties: no effect“). Dvě odchylky: (a) **D6 místo D3**, což mění pravděpodobnost remízy z 1/3 na 1/6, a (b) při remíze **mají dostat reroll oba**, ne nikdo.

**4. ŽIVÉ** — hody 6 a 8 na 2D6, dohromady ≈27,8 % kick-offů. Je to zdaleka nejčastější z živých nálezů.

**5. Komu to prospívá / škodí:** snižuje to celkovou zásobu rerollů obou stran. Reroll má vyšší mezní hodnotu pro tým, který dělá víc rizikových hodů — dodge, pickup, catch, pass. To jsou skaven a wood-elf, ne trpaslík (Block+Tackle sestava hází míň agility hodů). **Současný stav tedy mírně pomáhá nám a oprava mírně pomůže soupeřům.** Kombinovaně (D3 místo D6 + oba při remíze) by opravou vzrostl očekávaný počet rozdaných rerollů na tyhle dvě události z ~0,83 na ~1,33 na výskyt.

**6. Odhad velikosti opravy:** ~10 řádků, jeden soubor. `rollD3` v `engine/src/dice.cpp` možná bude potřeba doplnit — pak plus 3 řádky tam.

---

## Ž5 — Kick-off `High Kick`: chybí podmínka „mimo soupeřovu tackle zónu“ a volba hráče

**1. Kód:** `engine/src/kickoff_handler.cpp:77-88` (case `KickoffEvent::HIGH_KICK`), používá `findClosestPlayer` z `kickoff_handler.cpp:12-23`

**2. Pravidla:** ř. 1302-1308
> „5 High Kick: The ball is kicked very high, allowing a player on the receiving team time to move into the perfect position to catch it. **Any one player on the receiving team who is not in an opposing player’s tackle zone** may be moved into the square where the ball will land no matter what their MA may be, as long as the square is unoccupied.“

**3. Co kód dělá místo toho:**
   - (a) Vybere **nejbližšího** stojícího hráče přijímajícího týmu (`findClosestPlayer`) a přesune ho na míč. Pravidla dávají trenérovi volbu „any one player“ — v enginu se nikdy nevybere lepší chytač (Catch/SureHands/vyšší AG), pokud zrovna nestojí nejblíž. Vzhledem k tomu, že hráč po přesunu **musí** chytat (`resolveCatch` na ř. 316), je vynucená volba nejhoršího dostupného chytače reálná ztráta.
   - (b) **Chybí filtr `not in an opposing player’s tackle zone`.** Kód přesune i hráče, který je zamarkovaný soupeřem. To je v přijímající polovině po kick-offu vzácné, ale ne nemožné (Blitz! posune kopající tým dopředu, viz Ž1).
   - Správně kód ověřuje, že cílové pole je prázdné (ř. 83) a ignoruje MA ✓.

**4. ŽIVÉ** — hod 5 na 2D6, ≈11,1 % kick-offů.

**5. Komu to prospívá / škodí:** škodí **přijímajícímu týmu**, symetricky přes obě strany. Ale ne stejně silně: rozdíl mezi „nejbližší“ a „nejlepší“ chytačem je největší tam, kde sestava má vyhraněné specialisty — wood-elf Catcher AG4+Catch vs Lineman AG4, skaven Gutter Runner AG4 vs Lineman AG3. U trpaslíka jsou všichni AG2-3 a rozdíl je malý. **Oprava tedy pomůže agilním soupeřům víc než nám.**

**6. Odhad velikosti opravy:** ~15 řádků v jednom souboru — buď parametrizovat `findClosestPlayer` o filtr `countTacklezones(...) == 0` a preferenci `calculateCatchTarget`, nebo napsat vedle malou vlastní volbu.

---

## Ž6 — Kick-off `Quick Snap`: volný pohyb nahrazen posunem k LOS

**1. Kód:** `engine/src/kickoff_handler.cpp:121-130` (case `KickoffEvent::QUICK_SNAP`)

**2. Pravidla:** ř. 1329-1334
> „9 Quick Snap! The offence start their drive a fraction before the defence is ready, catching the kicking team flat-footed. All of the players on the receiving team are allowed to move one square. This is a free move and **may be made into any adjacent empty square, ignoring tackle zones**. It may be used to enter the opposing half of the pitch.“

**3. Co kód dělá místo toho:** pro každého stojícího hráče přijímajícího týmu zavolá `movePlayerToward(state, p.id, losTarget)` s cílem na linii scrimmage uprostřed hřiště (`{losX, 7}`). To znamená:
   - směr je pevně **k LOS a ke středu**, ne volba trenéra;
   - `movePlayerToward` (ř. 27-50) se posune jen na pole **striktně blíž** k cíli, takže hráč, který už je na LOS nebo od cíle nemá bližší volné pole, **se nepohne vůbec**;
   - hráč, který by chtěl jít pro míč (typický smysl Quick Snapu), jít nemůže.
   Podmínky „prázdné pole“ (ř. 37) a „ignoruje tackle zóny“ (žádný dodge se nehází) jsou splněné ✓.

**4. ŽIVÉ** — hod 9 na 2D6, ≈11,1 % kick-offů.

**5. Komu to prospívá / škodí:** okrádá to **přijímající tým**, symetricky. Ale opět nerovnoměrně: pole navíc směrem k míči je cenné pro tým, který plánuje rychlé rozehrání, kdežto trpaslík typicky staví klec a posun k LOS mu zhruba vyhovuje. **Oprava mírně pomůže soupeřům.** Poznámka: stejná deformace je i v `Blitz!` (Ž1), kde `movePlayerToward` supluje celý bonusový tah.

**6. Odhad velikosti opravy:** malá jako mechanika (~10 řádků), ale nesnadná jako **rozhodnutí** — „volný pohyb o 1 pole pro 11 hráčů“ je 11 nezávislých voleb, což se do stávající akční vrstvy nevejde. Buď nový `MacroType`, nebo heuristika lepší než „k LOS“ (např. „k míči, pokud je blíž než X, jinak k LOS“).

---

## Ž7 — Kick-off `Pitch Invasion`: ležící a omráčení hráči se přeskakují

**1. Kód:** `engine/src/kickoff_handler.cpp:169-181` (case `KickoffEvent::PITCH_INVASION`)

**2. Pravidla:** ř. 1350-1356
> „12 Pitch Invasion: Both coaches roll a D6 for **each opposing player on the pitch** and add their FAME to the roll. If a roll is 6 or more after modification then the player is Stunned (players with the Ball & Chain skill are KO'd). A roll of 1 before adding FAME will always have no effect.“

**3. Co kód dělá místo toho:** `if (p.state != PlayerState::STANDING || !p.isOnPitch()) continue;` — hází jen za **stojící** hráče. Pravidla říkají „each opposing player **on the pitch**“, a `isOnPitch()` v `engine/include/bb/enums.h:19-21` správně zahrnuje i `PRONE` a `STUNNED`. Ležící hráč, kterému fanoušci uštědří 6, má být Stunned (což je horší stav než Prone — nemůže hned vstát); v enginu zůstane Prone. Chybí i výjimka pro Ball & Chain (KO místo Stun). Práh `roll == 6` odpovídá pravidlu „6 or more after modification“ při FAME 0 ✓.

**4. ŽIVÉ** — hod 12 na 2D6, ≈2,8 % kick-offů. Nejmenší z živých.

**5. Komu to prospívá / škodí:** prospívá tomu, kdo má na kick-offu víc ležících hráčů. Na začátku drive stojí obvykle všichni, takže dopad je velmi malý — uvádím to pro úplnost, ne jako prioritu.

**6. Odhad velikosti opravy:** 2 řádky v jednom souboru (změnit podmínku, přidat Ball & Chain větev).

---

# ČÁST B — LATENTNÍ NÁLEZY, PRIORITA 1
*(7 implementovaných dovedností bez jediné citace; žádná není v korpusových sestavách TV1200)*

---

## L1 — `Regeneration`: `Stakes` blokuje regeneraci, což v textu není

**1. Kód:** `engine/src/injury.cpp:150`, deklarace `engine/include/bb/injury.h:20`
```cpp
if (player.hasSkill(SkillName::Regeneration) && !ctx.hasStakes) {
```
```cpp
bool hasStakes = false;   // blocks Regeneration
```
Nastavuje se na `engine/src/block_handler.cpp:559`, `engine/src/block_handler.cpp:975`, `engine/src/foul_handler.cpp:58`.

**2. Pravidla:**
   - `Stakes`, ř. 8504-8509 — **celý text dovednosti**:
     > „Stakes (Extraordinary) This player is armed with special stakes that are blessed to cause extra damage to the Undead and those that work with them. This player may add 1 to the Armour roll when they make a Stab attack against any player playing for a Khemri, Necromantic, Undead or Vampire team.“
   - `Stab`, ř. 8495-8496:
     > „Make an unmodified Armour roll (except for Stakes) for the victim.“
   - `Regeneration`, ř. 8406-8416:
     > „If the player suffers a Casualty result on the Injury table, then roll a D6 for Regeneration after the roll on the Casualty table and after any Apothecary roll if allowed. On a result of 1-3, the player suffers the result of this injury. On a 4-6, the player will heal the injury after a short period of time to 're-organise' himself, and is placed in the Reserves box instead. Regeneration rolls may not be re-rolled.“

   **V textu pravidel jsem nenašel žádnou zmínku o tom, že by Stakes bránil Regeneraci.** Prohledal jsem všechny čtyři výskyty slova „Stakes“ (ř. 7710, 7918, 8496, 8504) i celý odstavec Regeneration.

**3. Co kód dělá místo toho:** dvě chyby, které se navzájem doplňují.
   - (a) `Stakes` je v enginu implementován **výhradně** jako „vypni oběti hod na Regeneraci“ — a to při **jakémkoli** bloku, stabu i faulu, bez ohledu na to, jestli šlo o Stab a jestli je oběť z Khemri / Necromantic / Undead / Vampire týmu.
   - (b) **Skutečný efekt Stakes (+1 k hodu na brnění při Stab útoku proti undead týmům) není implementován nikde.** `ctx.hasStakes` se v `resolveArmourAndInjury` (`injury.cpp:178-200`) do `armourModifier` nikdy nepromítne.
   Jinými slovy: dovednost dělá výlučně to, co pravidla neříkají, a nedělá nic z toho, co říkají.

   Zbytek Regeneration je v pořádku, viz sekce „Prověřeno a v pořádku“.

**4. LATENTNÍ** — `Regeneration` má v `roster.cpp` nositele (Orc Troll ř. 172, Undead, Khemri, Necromantic, Nurgle, Vampire), ale **žádnou z těchto sestav korpus TV1200 nepoužívá**; orkská sestava `getOrcRoster1200` (ř. 503-528) Trolla vůbec nemá. `Stakes` nemá v `roster.cpp` nositele vůbec žádného.

**5. Komu to prospívá / škodí:** dnes nikomu — ani jedna z obou dovedností se v korpusu nevyskytne. Ožije to ve chvíli, kdy se sáhne na rostery: nasazení Undead / Necromantic / Khemri soupeře by okamžitě znamenalo, že jejich Regeneration funguje (dobře), ale libovolný náš hráč se `Stakes` by jim ji vypnul jednou ranou pěstí (špatně, a ve **náš** prospěch).

**6. Odhad velikosti opravy:** ~6 řádků ve dvou souborech. Odstranit `&& !ctx.hasStakes` z `injury.cpp:150`, přejmenovat/opravit komentář v `injury.h:20`, a `hasStakes` napojit na `armourModifier += 1` pouze ve Stab větvi `block_handler.cpp:554-563` a pouze proti undead rasám (což znamená přidat do `TeamState` nebo `GameState` informaci o rase soupeře — dnes ji `InjuryContext` nemá, takže **nová malá vrstva**). Alternativa levnější a bezpečnější: `Stakes` úplně odpojit, dokud se undead týmy nedostanou do korpusu.

---

## L2 — `DisturbingPresence`: nepůsobí na intercepci

**1. Kód:** `engine/src/pass_handler.cpp:62-68` (výběr interceptora) a `engine/src/pass_handler.cpp:78-92` (samotný hod). Funkce `countDisturbingPresence` je v `engine/src/helpers.cpp:21-30`, volá se z `engine/src/pass_handler.cpp:263` (přihrávka) a `engine/src/helpers.cpp:168` (chycení).

**2. Pravidla:** ř. 8050-8058
> „Disturbing Presence (Mutation) This player's presence is very disturbing… Regardless of the nature of this mutation, any player must subtract 1 from the D6 when they **pass, intercept or catch** for each opposing player with Disturbing Presence that is within three squares of them, even if the Disturbing Presence player is Prone or Stunned.“

**3. Co kód dělá místo toho:** pravidlo jmenuje tři hody — pass, intercept, catch. Engine ho aplikuje na **pass** (`pass_handler.cpp:263`) a na **catch** (`helpers.cpp:168`), ale **na intercept vůbec ne**. V `checkInterception` se cíl skládá takto:
```cpp
int intTarget = 7 - interceptor->stats.agility + 2;
if (interceptor->hasSkill(SkillName::VeryLongLegs)) intTarget -= 1;
if (interceptor->hasSkill(SkillName::ExtraArms)) intTarget -= 1;
if (!interceptor->hasSkill(SkillName::NervesOfSteel)) { intTarget += countTacklezones(...); }
if (state.weather == Weather::POURING_RAIN) intTarget += 1;
```
— `countDisturbingPresence` tam nefiguruje, a to ani ve výběrové smyčce na ř. 62-68, která hledá „nejpravděpodobnějšího“ interceptora. Interceptor stojící tři pole od dvou soupeřů s Disturbing Presence chytá stejně snadno, jako by tam nikdo nebyl.

**4. LATENTNÍ** — `DisturbingPresence` mají v `roster.cpp` Norse Yhetee (ř. 205), Nurgle Warrior/Beast (ř. 360, 364) a Star Playeři; v pěti sestavách TV1200 není.

**5. Komu to prospívá / škodí:** prospívá **tomu, kdo intercepuje**, tj. bránícímu týmu proti přihrávkám. Kdyby se do korpusu dostal Nurgle nebo Norse, chyba by hrála **proti** jejich nositeli — Disturbing Presence by mu na intercepci nepracovala. Pro nás (trpaslík) je to dnes irelevantní; trpaslík má sestavu bez DP a intercepce hraje minimální roli, protože přihrávek je málo.

**6. Odhad velikosti opravy:** **2 řádky v jednom souboru** — přidat `intTarget += countDisturbingPresence(state, interceptor->position, interceptor->teamSide);` a totéž do výběrové smyčky, aby výběr „nejlepšího“ interceptora zůstal konzistentní. `helpers.h` je už v `pass_handler.cpp` includovaný.

---

## L3 — `KickOffReturn`: špatné pořadí vůči kick-off tabulce + chybí tři podmínky

**1. Kód:** `engine/src/kickoff_handler.cpp:284-311`

**2. Pravidla:** ř. 8249-8256
> „Kick-Off Return (General) A player on the receiving team **that is not on the Line of Scrimmage or in an opposing tackle zone** may use this skill when the ball has been kicked. It allows the player to move up to 3 squares **after the ball has been scattered but before rolling on the Kick-Off table**. Only one player may use this skill each kick-off. This skill may not be used for a touchback kick-off and **does not allow the player to cross into the opponent’s half of the pitch**.“

**3. Co kód dělá místo toho:** čtyři odchylky.
   - (a) **Pořadí.** Hod 2D6 na kick-off tabulku a jeho vyhodnocení běží na `kickoff_handler.cpp:275-277`, hod na počasí na ř. 280-282, a teprve **potom** na ř. 284 začíná blok Kick-Off Return. Pravidla to mají obráceně: „after the ball has been scattered but **before** rolling on the Kick-Off table“. Konkrétní dopad: `High Kick` (ř. 77-88) vybírá nejbližšího hráče k míči **z pozic před** KOR pohybem, takže KOR hráč se nikdy nemůže High Kickem posunout dál; `Blitz!` (ř. 132-142) posune kopající tým dřív, než KOR hráč vyjde, takže mu může zavřít cestu.
   - (b) **Chybí podmínka „not on the Line of Scrimmage or in an opposing tackle zone“.** Kód testuje jen `p.state != PlayerState::STANDING` a `hasSkill(KickOffReturn)` (ř. 286-288). Zamarkovaný hráč nebo hráč na LOS smí v enginu KOR použít.
   - (c) **Chybí zákaz přechodu do soupeřovy poloviny.** `movePlayerToward` (ř. 27-50) testuje jen `pos.isOnPitch()` a obsazenost pole; nic mu nebrání překročit střed hřiště.
   - (d) **Pohyb je vynucená greedy trasa k míči, ne volba 3 polí.** Smyčka `for (int step = 0; step < 3; step++) movePlayerToward(state, p.id, state.ball.position);` posune hráče jen na pole **striktně blíž** k míči; je-li nejlepší pole obsazené, hráč se zastaví (obcházení neexistuje) a zbylé kroky propadnou. Pravidla dávají „move up to 3 squares“ bez omezení směru.
   - Správně kód řeší: vyloučení touchbacku (`if (!touchback)`, ř. 284) ✓ a „only one player may use this skill each kick-off“ (vnitřní smyčka ř. 292-300 vybere jediného, nejbližšího) ✓.

**4. LATENTNÍ** — `KickOffReturn` nemá v `engine/src/roster.cpp` **žádného** nositele (grep na `KickOffReturn` vrací pouze `kickoff_handler.cpp` a `enums.h`). Je to jediná z Priority 1, kterou nemá ani žádná ze základních 26 sestav.

**5. Komu to prospívá / škodí:** dnes nikomu. Po nasazení by mechanika jako celek prospívala **přijímajícímu týmu** a nejvíc rychlým sestavám. Body (b) a (c) jsou příliš velkorysé (dovolují víc, než pravidla), body (a) a (d) příliš skoupé. Netto by po opravě byla dovednost o poznání slabší, než je dnes v kódu.

**6. Odhad velikosti opravy:** střední, jeden soubor + jedna nová pomocná funkce. Přesun bloku KOR z ř. 284 nad ř. 275 je triviální, ale mění pořadí hodů kostkou ⇒ **rozbije reprodukovatelnost všech uložených korpusů a CRN párování** — nedělat mimo plánovaný break. Podmínky (b)/(c) jsou ~8 řádků. Bod (d) chce buď skutečný 3-krokový pathfinding (`engine/src/pathfinder.cpp` už existuje), nebo se s greedy variantou smířit a poznamenat to.

---

## L4 — `DivingCatch`: bonus se dává na každé chycení, a druhá polovina dovednosti chybí

**1. Kód:** `engine/src/helpers.cpp:171` — jediný výskyt `DivingCatch` v celém `engine/src/`:
```cpp
if (catcher.hasSkill(SkillName::DivingCatch)) target -= 1;
```
uvnitř `calculateCatchTarget`.

**2. Pravidla:** ř. 8061-8071
> „Diving Catch (Agility) The player is superb at diving to catch balls others cannot reach and jumping to more easily catch perfect passes. The player may add 1 to any catch roll **from an accurate pass targeted to his square**. In addition, the player can attempt to catch any pass, kick off or crowd throw-in, **but not bouncing ball**, that would land in an empty square in one of his tackle zones as if it had landed in his own square without leaving his current square. A failed catch will bounce from the Diving Catch player's square. If there are two or more players attempting to use this skill then they get in each other’s way and neither can use it.“

Doplňkově tabulka modifikátorů chycení, ř. 828-830 a ř. 891-893:
> „Catching an accurate pass +1 / Catching a scattered pass, Bouncing ball or throw-in“
> „Catching an accurate pass…..………………………….. +1 / Catching a missed pass, kick-off, Bouncing ball or throw-in……………………………….. +0“

**3. Co kód dělá místo toho:** tři odchylky.
   - (a) **Bonus je bezpodmínečný.** `calculateCatchTarget` neví, odkud míč přiletěl; volá se ze všech chytacích cest a Diving Catch −1 se přičte pokaždé. Konkrétně: odraz míče (`engine/src/ball_handler.cpp:89` a `:247`, obojí s modifikátorem 0), chycení kopu (`engine/src/kickoff_handler.cpp:316`, modifikátor 0), chycení vhozu od davu (`ball_handler.cpp:247`), hand-off (`engine/src/pass_handler.cpp:422`, modifikátor 1) i nepřesná/rozptýlená přihrávka (`pass_handler.cpp:367`, modifikátor 0). Pravidla dávají bonus **výhradně** u přesné přihrávky mířené na jeho pole — tedy jen `pass_handler.cpp:344` (`resolveCatch(..., 1, ...)` ve větvi `if (accurate)`). Engine tedy dovednost aplikuje na přinejmenším pět situací navíc, včetně odrazu, který ji pravidla výslovně **zakazují** („but not bouncing ball“).
   - (b) **Druhá polovina dovednosti není implementována vůbec.** Schopnost chytit míč, který dopadá do **prázdného sousedního pole v jeho tackle zóně**, jako by dopadl na jeho vlastní pole, v kódu neexistuje — jediný výskyt `DivingCatch` je onen jeden řádek modifikátoru. To je zdaleka větší polovina dovednosti (je to jediný způsob, jak zachytit kop nebo přihrávku mimo své pole).
   - (c) **Chybí i pravidlo o kolizi** („If there are two or more players attempting to use this skill then they get in each other’s way and neither can use it“) — což je logický důsledek (b).

**4. LATENTNÍ** — nositelé v `roster.cpp`: Pro Elf Catcher a Star Playeři (ř. 7376, 7533, 7538, 7606 v pravidlech). V korpusu TV1200 nikdo.

**5. Komu to prospívá / škodí:** bod (a) prospívá nositeli, bod (b) mu škodí, a to výrazně víc. Netto je dnešní Diving Catch **slabší**, než má být. Protože jde o dovednost elfích a skavenních chytačů, oprava by po nasazení do rosterů **pomohla soupeřům**, ne nám.

**6. Odhad velikosti opravy:** bod (a) je ~4 řádky, ale vyžaduje protáhnout do `calculateCatchTarget` informaci „jde o přesnou přihrávku mířenou sem“ — buď novým parametrem, nebo tím, že se −1 přesune z `helpers.cpp` do volajícího `pass_handler.cpp:344`. To druhé je čistší a je to změna ve dvou souborech o ~5 řádcích. Bod (b) je **nová funkce plus vrstva**: musí zasáhnout do `resolveBounce`/`resolveThrowIn`/kick-off doskoku a nabídnout „chycení do sousedního pole“, což dnes žádná cesta v `ball_handler.cpp` neumí.

---

## L5 — `NurglesRot`: implementace je mrtvá větev, příznak se nikdy nenastaví

**1. Kód:** `engine/src/injury.cpp:168-171`
```cpp
if (ctx.hasNurglesRot) {
    emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                      static_cast<int>(SkillName::NurglesRot), true});
}
```
Deklarace `engine/include/bb/injury.h:22`: `bool hasNurglesRot = false;`

**2. Pravidla:** ř. 8324-8334
> „Nurgle’s Rot (Extraordinary) This player has a horrible infectious disease which spreads when he **kills** an opponent during a **Block, Blitz or Foul Action**. Instead of truly dying, the infected opponent becomes a new rookie Rotter. To do so, the opponent must have been removed from the roster during step 2.1 of the Post-game sequence, his Strength cannot exceed 4, and he cannot have the Decay, Regeneration or Stunty skills. The new Rotter can be added to the Nurgle team for free during step 5 of Updating Your Team Roster if the team has an open Roster slot. This new Rotter still counts at full value towards the total value of the Nurgle team.“

**3. Co kód dělá místo toho:**
   - (a) **`ctx.hasNurglesRot` se v celém `engine/src/` nikde nenastavuje na `true`.** Grep na `hasNurglesRot` vrací pouze deklaraci v `injury.h:22` a čtení v `injury.cpp:168`. Blok je tedy nedosažitelný a žádná událost se nikdy neemituje. Pro srovnání: sourozenecké příznaky `hasStakes`, `hasDecay`, `hasClaw`, `mightyBlow` se nastavují v `block_handler.cpp:972-976` a `foul_handler.cpp:57-58` — `hasNurglesRot` tam prostě chybí.
   - (b) I kdyby se nastavoval, větev je uvnitř `if (isCasualty)` a spustila by se při **jakékoli** casualty, ne jen při `cas == CasualtyResult::DEAD`, jak žádá slovo „kills“. Nekontrolovala by ani ST ≤ 4, ani nepřítomnost Decay/Regeneration/Stunty.
   - (c) Vlastní efekt (nový Rotter v soupisce) je čistě **poligová/post-game mechanika**; engine hraje jednotlivý zápas a soupisky mezi zápasy nevede. Že tohle není implementované, **nepovažuji za vadu** — vadou je, že v kódu leží prázdná skořápka, která vypadá, jako by se něco dělo.

**4. LATENTNÍ** — nositelé jen v `getNurgleRoster()` (`roster.cpp:357, 359, 363`). Nurgle není v korpusu TV1200.

**5. Komu to prospívá / škodí:** nikomu — mechanika je inertní v obou směrech. Riziko je jiné: `check_rules_citations.py` počítá `NurglesRot` mezi „implementovanou mechaniku“, takže dovednost figuruje jako hotová, ačkoli neběží ani jeden její řádek. Doporučuji buď příznak napojit (a doplnit podmínky), nebo mrtvou větev smazat a `NurglesRot` explicitně vést jako neimplementovaný.

**6. Odhad velikosti opravy:** varianta „smazat“ = 4 řádky v jednom souboru. Varianta „dopojit“ = ~10 řádků ve třech souborech (`block_handler.cpp`, `foul_handler.cpp`, `injury.cpp`) + podmínky, ale bez post-game vrstvy zůstane efekt stejně jen informativní.

---

## L6 — `MultipleBlock`: hráči s Frenzy je zakázán, ačkoli si pravidla výslovně dovolují vybrat

**1. Kód:** `engine/src/rules_engine.cpp:221-243` (nabídka akce) a `engine/src/block_handler.cpp:1038-1113` (`resolveMultipleBlock`)

**2. Pravidla:** ř. 8298-8306
> „Multiple Block (Strength) At the start of a Block Action a player who is adjacent to at least two opponents may choose to throw blocks against two of them. Make each block in turn as normal except that each defender's strength is increased by 2. The player cannot follow up either block when using this skill, **so Multiple Block can be used instead of Frenzy, but both skills cannot be used together.** To have the option to throw the second block the player must still be on his feet after the first block.“

FAQ, ř. 9226-9228:
> „Q. When do I declare the second opponent for a Multiple Block? A. You may declare the second opponent **after the first block has been completed**.“

**3. Co kód dělá místo toho:**
   - (a) `rules_engine.cpp:222`:
     ```cpp
     if (p.hasSkill(SkillName::MultipleBlock) && !p.hasSkill(SkillName::Frenzy)) {
     ```
     Hráč, který má obě dovednosti, **nedostane akci `MULTIPLE_BLOCK` nikdy nabídnutou**. Pravidla ale říkají „Multiple Block **can be used instead of** Frenzy“ — tedy že trenér si mezi nimi vybírá pro daný blok. Zákaz zní „both skills cannot be used **together**“ (v jednom bloku), ne „hráč s Frenzy Multiple Block nemá“. Prakticky: každý hráč, který by obě dovednosti měl (v pravidlech např. Star Player na ř. 7450 „Loner, Block, Dauntless, Frenzy, Multiple Block, Thick Skull“), přijde v enginu o Multiple Block úplně.
   - (b) Druhý cíl se deklaruje **předem**: akce se generuje jako dvojice `(adjEnemies[i], adjEnemies[j])` zakódovaná do `targetId` a `target.x` (`rules_engine.cpp:236-238`), a `resolveMultipleBlock` oba cíle dostane najednou. FAQ dovoluje druhého soupeře vybrat **až po vyhodnocení prvního bloku**. V hledání to znamená, že MCTS nemůže reagovat na výsledek prvního bloku — musí se rozhodnout naslepo.

   Zbytek `resolveMultipleBlock` je v pořádku, viz sekce „Prověřeno a v pořádku“.

**4. LATENTNÍ** — `MultipleBlock` nemá v `engine/src/roster.cpp` žádného nositele (v pravidlech ji mají jen Star Playeři, ř. 7450 a 7500). V korpusu TV1200 tedy nehraje.

**5. Komu to prospívá / škodí:** dnes nikomu. Bod (a) by po nasazení škodil **nositeli** — a nositelem by v korpusu byl s největší pravděpodobností silový hráč, tedy spíš náš typ sestavy než elfí. Bod (b) je omezení hledání, nikoli asymetrie mezi stranami.

**6. Odhad velikosti opravy:** bod (a) = **1 řádek** v `rules_engine.cpp` (odstranit `&& !p.hasSkill(SkillName::Frenzy)`), plus je potřeba zkontrolovat, že `resolveBlock` volaný s `noFollowUp = true` opravdu Frenzy druhý blok nespustí (dnes ho spouští blok na `block_handler.cpp:1013-1033` řízený `params.isBlitz`/`frenzySecondBlock` — chce to ověřit). Bod (b) vyžaduje **dvoufázovou akci** (deklarace prvního cíle, vyhodnocení, deklarace druhého), což dnešní plochý `Action` model neumí ⇒ nový `MacroType` nebo rozdělení na dvě akce.

---

# ČÁST C — LATENTNÍ NÁLEZY, PRIORITA 2
*(dovednosti v `enums.h`, které v `engine/src/` nepoužívá nikdo)*

Ověřeno gepem: pro každou z nich vrací `grep -rn <Skill> engine/src/ engine/include/` pouze definici v `enums.h` a případné výskyty v `roster.cpp`. Žádná herní mechanika.

---

## L7 — `Leader`: neimplementován; a rerollová ekonomika je pevně 3, nezávisle na týmu

**1. Kód:** `engine/include/bb/enums.h` (`Leader` v enumu) — jediný výskyt. Rerollová ekonomika: `engine/src/game_simulator.cpp:156` (`ts.rerolls = 3;  // Standard starting rerolls`) a `engine/src/game_simulator.cpp:326` (`ts.rerolls = 3;` při `resetHalfState`). Struktura `engine/include/bb/team_state.h:11-12, 28-30`.

**2. Pravidla:**
   - `Leader`, ř. 8257-8269:
     > „Leader (Passing) The player is a natural leader and commands the rest of the team from the back-field as he prepares to throw the ball. A team with one or more players with the Leader skill may take a single Leader Re-roll counter and add it to their team re-rolls at the start of the game and at half time after any Master Chef rolls. The Leader re-roll is used exactly the same in every way as a normal Team re-roll with all the same restrictions. In addition, the Leader re-roll may only be used so long as at least one player with the Leader skill is on the pitch - even if they are Prone or Stunned! Re-rolls from Leader may be carried over into Overtime if not used, but the team does not receive a new Leader re-roll at the start of Overtime.“
   - Obnova o poločase, ř. 941-943:
     > „At half time the two teams get a chance to rest and recuperate, and so their team re-rolls are restored to their starting level.“
   - Nákup rerollů, ř. 1145-1149:
     > „When you create a team you do not get any team re-rolls or Fan… Each re-roll costs the number of gold pieces shown on the team list for the team that you have chosen“
   - Ceny pro korpusové rasy: Dwarf ř. 8778 „0-8 Re-rolls: 50'000 gold pieces each“ · Human ř. 8885 „50'000“ · Skaven ř. 9044 „60'000“ · Orc ř. 9077 „60'000“ · Wood Elf ř. 9109 „50'000“.

**3. Co kód dělá místo toho:**
   - (a) `Leader` nemá **žádnou** mechaniku. Tým s Leaderem nedostane reroll navíc, a chybí i navazující pravidlo „jen dokud je aspoň jeden Leader na hřišti“. Zároveň `Leader` nemá v `roster.cpp` **žádného nositele**, takže dnes není ani zakoupitelný.
   - (b) **Celá rerollová ekonomika je konstanta 3 pro všechny týmy.** `rerollCost` v `engine/include/bb/roster.h:24` je sice u každé sestavy vyplněný, ale v `engine/src/` ho **nečte nikdo** (jediné čtení je `engine/python/bb_module.cpp:384`, které ho jen vystavuje do Pythonu, a `engine/tests/test_game_simulator.cpp:190`, který jen kontroluje, že je > 0). Pravidla staví počet rerollů na nákupu za cenu z týmové listiny ⇒ při stejném TV má levnější tým reálně mít víc rerollů. V korpusu je Dwarf/Human/WoodElf 50k a Orc/Skaven 60k, takže po zavedení nákupu by při TV1200 vznikl rozdíl mezi rasami.
   - (c) Obnova o poločase (`ts.rerolls = 3` při `resetHalfState`) **je správně** — odpovídá ř. 941-943 „restored to their starting level“, včetně toho, že se tím zahodí rerolly získané z kick-off tabulky. To je v souladu.

**4. LATENTNÍ pro `Leader`** (nikdo ho nemá) / **ŽIVÉ jako modelovací rozhodnutí pro (b)** — konstanta 3 platí v každé hře korpusu. Neuvádím (b) mezi živými nálezy části A, protože pravidla samotné číslo nepředepisují: je to důsledek nákupu, ne konstanta. Ale je to reálná odchylka od toho, jak se rerolly v pravidlech berou.

**5. Komu to prospívá / škodí:** (b) **prospívá orkovi a skavenovi** — mají v pravidlech dražší reroll (60k) než trpaslík, člověk a wood-elf (50k), a přesto dostanou stejné tři. Při zavedení nákupu by trpaslík měl relativně víc rerollů než teď. Zároveň platí obecná úvaha z nálezu Ž4: reroll je cennější pro agilní sestavu, takže výsledný netto efekt opravy je nejistý a je nutné ho **změřit**, ne odhadnout.

**6. Odhad velikosti opravy:** `Leader` = nová funkce (~20 řádků v `game_simulator.cpp` + kontrola „aspoň jeden Leader na hřišti“ v `team_state.h::canUseReroll`, což znamená předat `GameState` — dnes `TeamState` na hráče nevidí ⇒ malá refaktorizace). Rerollová ekonomika = rozhodnutí, ne kód: pár řádků, ale **rozbije baseline** a musí projít gate.

---

## L8 — `DumpOff`: neimplementován, přestože ho jedna sestava rozdává

**1. Kód:** `engine/include/bb/enums.h` — jediný výskyt v enginu. Nositel v `engine/src/roster.cpp:165` (Dark Elf Runner, `makeSkills({SkillName::DumpOff})`).

**2. Pravidla:** ř. 8093-8103
> „Dump-Off (Passing) This skill allows the player to make a Quick Pass when an opponent declares that he will throw a block at him, allowing the player to get rid of the ball before he is hit. Work out the Dump-Off pass before the opponent makes his block. The normal throwing rules apply, except that neither team’s turn ends as a result of the throw, whatever it may be. After the throw is worked out your opponent completes the block, and then carries on with his turn. Dump-Off may not be used on the second block from an opponent with the Frenzy skill or in conjunction with the Bombardier or Throw Team-Mate skills.“

FAQ, ř. 9246-9247: „Q. Can a player attempt to intercept a pass that is made using the Dump-Off skill? A. Yes.“

**3. Co kód dělá místo toho:** nic. `resolveBlock` (`engine/src/block_handler.cpp`) nemá žádný háček před vyhodnocením bloku, který by nositeli dal šanci přihrát. Dark Elf Runner tuto dovednost v enginu má a nikdy ji nepoužije.

**4. LATENTNÍ** — Dark Elf není v korpusu TV1200.

**5. Komu to prospívá / škodí:** dnes nikomu. Po nasazení Dark Elfů by chyba škodila **jim** (přišli by o obranu nosiče proti bloku). Pro nás je Dump-Off nepříjemný — trpaslík vyhrává tím, že nosiče uzavře a odblokuje; Dump-Off je přesně protilék. **Oprava by tedy pomohla soupeři.**

**6. Odhad velikosti opravy:** nová vrstva. Vyžaduje **přerušení bloku o rozhodnutí soupeře**, které dnešní jednosměrný `resolveBlock` neumožňuje — potřebuje reakční bod v akční vrstvě (podobně jako by ho potřeboval `PassBlock`, viz L10). Netriviální i pro MCTS: strom by musel modelovat volbu soupeře uvnitř naší akce.

---

## L9 — `Animosity`: neimplementována, přestože ji dvě sestavy rozdávají plošně

**1. Kód:** `engine/include/bb/enums.h` — jediný výskyt v enginu. Nositelé v `engine/src/roster.cpp:409, 411, 412, 414` (Underworld) a `452, 453, 455, 456, 459, 462` (Chaos Pact) — u Chaos Pactu ji má prakticky celá soupiska.

**2. Pravidla:** ř. 7796-7808
> „Animosity (Extraordinary) A player with this skill does not like players from his team that are a different race than he is and will often refuse to play with them despite the coach's orders. If this player at the end of his Hand-off or Pass Action attempts to hand-off or pass the ball to a team-mate that is not the same race as the Animosity player, roll a D6. On a 2+, the pass/hand-off is carried out as normal. On a 1, the player refuses to try to give the ball to any team-mate except one of his own race. The coach may choose to change the target of the pass/hand-off to another team-mate of the same race as the Animosity player, however no more movement is allowed for the Animosity player, so the current Action may be lost for the turn.“

**3. Co kód dělá místo toho:** nic. `resolvePass` a hand-off v `engine/src/pass_handler.cpp` žádný hod na Animosity nedělají. Navíc engine **nemá pojem „rasa hráče“** — `PlayerTemplate` (`engine/include/bb/roster.h:10-17`) nese jen `stats`, `skills`, `quantity` a textový `name`. Bez toho nejde ani rozhodnout, kdo je „same race“.

**4. LATENTNÍ** — Underworld ani Chaos Pact nejsou v korpusu TV1200.

**5. Komu to prospívá / škodí:** dnes nikomu. Po nasazení by chyba prospívala **nositeli** (Animosity je čistá nevýhoda), tj. Underworld/Chaos Pact by byli silnější, než mají být.

**6. Odhad velikosti opravy:** potřebuje **nové datové pole** (rasa/identita positional v `PlayerTemplate` a `Player`), pak ~15 řádků v `pass_handler.cpp`. Dokud tyto rasy nejsou v korpusu, doporučuji to nechat a jen zaznamenat.

---

## L10 — `PassBlock`: neimplementován, žádný nositel

**1. Kód:** `engine/include/bb/enums.h` — jediný výskyt v enginu. V `engine/src/roster.cpp` **žádný nositel**.

**2. Pravidla:** ř. 8338-8360
> „Pass Block (General) A player with this skill is allowed to move up to three squares when the opposing coach announces that one of his players is going to pass the ball (but not a bomb). The opposing coach may not change his mind about passing once Pass Block’s use is declared. The move is made out of sequence, after the range has been measured, but before any interception attempts have been made. A player may not make the move unless able to reach a legal destination and may not follow a route that would not allow them to reach a legal destination. A legal destination puts the player in a position to attempt an interception, an empty square that is the target of the pass, or with his tackle zone on the thrower or catcher. The player may not stop moving until he has reached a legal destination, has been held fast by Tentacles or has been Knocked Down. The special move is free, and in no way affects the player’s ability to move in a subsequent action. The move is made using all of the normal rules and skills and the player does have to dodge in order to leave opposing players’ tackle zones. Players with Pass Block may use this skill against a Dump Off pass. If a player performing a Pass Block in their own turn is Knocked Down then this is a turnover, no other players may perform Pass Block moves, and your turn ends as soon as the results of the pass and the block are resolved.“

FAQ, ř. 9230-9247, doplňuje mj.: „A player cannot GFI during a Pass Block“; „since Pass Block is NOT an Action, they cannot stand up or use Jump Up“; a „Q. Can a player use Pass Block when an opposing player tries to throw a team-mate holding the ball? A. No“.

**3. Co kód dělá místo toho:** nic — `resolvePass` (`engine/src/pass_handler.cpp:230+`) jde přímo z výpočtu dosahu na `checkInterception` bez jakéhokoli reakčního okna pro soupeře.

**4. LATENTNÍ** — nemá nositele ani v jedné z 26 sestav v `roster.cpp`.

**5. Komu to prospívá / škodí:** dnes nikomu. Po nasazení by šlo o obrannou dovednost proti přihrávce — v profilu trpaslíka (nízké AG, málo přihrávek) bychom ji chtěli spíš **my proti soupeřům**, protože my přihráváme málo a soupeři často. Oprava by tedy potenciálně pomohla nám.

**6. Odhad velikosti opravy:** stejná třída jako `DumpOff` (L8) — **nová reakční vrstva** uprostřed soupeřovy akce, plus pathfinding s omezením na „legal destination“. Nejdražší z celého seznamu.

---

## L11 — `PilingOn`: neimplementován, žádný nositel

**1. Kód:** `engine/include/bb/enums.h` — jediný výskyt v enginu. V `engine/src/roster.cpp` **žádný nositel**.

**2. Pravidla:** ř. 8361-8372 (a duplicitně jako volitelné pravidlo ř. 3050-3061)
> „Piling On (Strength) The player may use this skill after they have made a block as part a Block or Blitz action, but only if they are currently standing adjacent to the victim and the victim was Knocked Down. You can use a team re-roll to re-roll the Armour roll or Injury roll. Then the Piling On player is Placed Prone in their own square - no Armour roll is made for them, and this does not cause a turnover unless the Piling On player is carrying the ball. Piling On cannot be used with the Stab or Chainsaw skills. If a player with the Loner skill wishes to use Piling On, roll a D6. On a 1-3, the team re-roll is spent, but they remain standing and cannot re-roll the Armour or Injury roll.“

Poznámka: v seznamu dovedností je na ř. 7875 vedena jako **„Piling On (Optional)“**, tedy jako volitelné pravidlo.

**3. Co kód dělá místo toho:** nic. Po vyhodnocení `resolveArmourAndInjury` (`engine/src/block_handler.cpp:978`) žádná možnost přehodit armour/injury roll za cenu položení se neexistuje.

**4. LATENTNÍ** — bez nositele.

**5. Komu to prospívá / škodí:** dnes nikomu. Po nasazení by to byla čistě **silová/attrition** dovednost, tedy typicky náš nástroj (trpaslík vyhrává vyřazováním). Oprava by pravděpodobně pomohla nám — což je opačný směr než u většiny ostatních nálezů.

**6. Odhad velikosti opravy:** střední. Mechanicky je to ~20 řádků v `block_handler.cpp` + `injury.cpp`, ale je to **volitelná dovednost** ⇒ potřebuje rozhodovací bod (kdy se vyplatí položit se a utratit reroll), tedy zásah do akční/makro vrstvy, ne jen do resolveru. Vzhledem k tomu, že to není v korpusu, doporučuji zaznamenat a nechat.

---

# ČÁST D — PRIORITA 3, DALŠÍ NÁLEZY

## P1 — `rerollCost` trpaslíka je 40, pravidla říkají 50

**1. Kód:** `engine/src/roster.cpp:91` (základní `getDwarfRoster`, `5, 40, true`) a `engine/src/roster.cpp:587` (`getDwarfRoster1200`, `6, 40, true`). Pole je `rerollCost` (`engine/include/bb/roster.h:24`, „in thousands“).

**2. Pravidla:** ř. 8778 (v bloku `DWARF TEAMS`, který začíná na ř. 8764)
> „0-8 Re-rolls: 50'000 gold pieces each“

**3. Co kód dělá místo toho:** vede trpaslíkův reroll za 40k místo 50k. Ostatní čtyři korpusové rasy mám ověřené a **sedí**: Human 50 (ř. 8885) ✓, Skaven 60 (ř. 9044) ✓, Orc 60 (ř. 9077) ✓, Wood Elf 50 (ř. 9109) ✓. Chybná je jen ta jedna hodnota, a je to hodnota **našeho** týmu — chyba ukazuje naši sestavu jako levnější, než je.

**4. LATENTNÍ v enginu, ale ŽIVÉ jinde.** `rerollCost` v `engine/src/` nikdo nečte (viz L7b), takže na výsledek zápasu to dnes nemá vliv. Vystavuje se ale do Pythonu (`engine/python/bb_module.cpp:384`), takže každý TV výpočet nebo skript, který si soupisku ocení, počítá s chybnou cenou — a to je přesně ta situace, kdy se z tiché datové chyby stane systematický posun v TV1200 kalibraci.

**5. Komu to prospívá / škodí:** kdyby se TV počítalo z těchto čísel, trpaslík by při 3 rerollech „ušetřil“ 30k a mohl by je utratit za hráče ⇒ **prospívá nám**.

**6. Odhad velikosti opravy:** **2 znaky ve dvou řádcích** jednoho souboru. Ale pozor: pokud se `rerollCost` používá v Pythonu ke kalibraci TV1200 sestav, změna posune TV a spustí baseline auto-reset (viz commit f07c005). Ověřit v `python/` před sáhnutím.

---

## P2 — 68 citací stále odkazuje na CRP / LRB6 místo na `rules_bb2016.txt`

**1. Kód:** rozloženo přes 15 souborů v `engine/src/` (nejvíc `block_handler.cpp` = 12, `pass_handler.cpp` = 8, `injury.cpp` = 7) a 8 testových souborů. Výstup `check_rules_citations.py`, sekce (2).

**2. Pravidla:** není to nález o pravidlech, ale o **auditní stopě**. `rules_bb2016.txt` je jediný platný zdroj; komentář, který se odvolává na „CRP“, není ověřitelný proti ničemu v repozitáři.

**3. Co kód dělá místo toho:** cituje sekundární zdroj. Několik z nich je přitom **věcně správných** (např. `pass_handler.cpp:100-102` „CRP: A -1 modifier applies to all catch, intercept, or pick-up rolls“ odpovídá tabulce na ř. 891-893), takže nejde o chybnou mechaniku — jde o to, že je nelze zkontrolovat.

**4. Stav:** už evidováno jako nález z 20.08.2026. Uvádím pro úplnost, neotvírám znovu.

**5./6.** Mechanická oprava (dohledat a přepsat na `l. A-B`), ~68 komentářů. Není pravidlová změna, jde ji dělat kdykoli bez dopadu na baseline.

---

# ČÁST E — PROVĚŘENO A V POŘÁDKU

Tohle jsem proti textu ověřil a **shodu našel**. Uvádím to explicitně, protože to tyto otázky zavírá.

### `ExtraArms` — kompletně správně (jediná z Priority 1 bez nálezu)
Pravidla ř. 8104-8106: „A player with one or more extra arms may add 1 to any attempt to **pick up, catch or intercept**.“ Všechny tři cesty jsou pokryté a všechny se správným znaménkem (kód pracuje s cílovým číslem, takže „+1 k hodu“ = `−1` k cíli):
- pick up — `engine/src/helpers.cpp:152` v `calculatePickupTargetAt`
- catch — `engine/src/helpers.cpp:170` v `calculateCatchTarget`
- intercept — `engine/src/pass_handler.cpp:81` (samotný hod) i `engine/src/pass_handler.cpp:63` (výběr nejlepšího interceptora, konzistentně)
Žádná další cesta v pravidlech není a žádnou navíc kód nepřidává.

### `DisturbingPresence` — vše kromě intercepce (viz L2)
- **Vzdálenost „within three squares“**: `countDisturbingPresence` používá `p.position.distanceTo(pos) <= 3`, a `distanceTo` je Chebyshevova vzdálenost (`engine/include/bb/position.h:29-32`, komentář „Chebyshev distance“). To je pro čtvercovou mřížku s diagonálami správné „pole“. ✓
- **„even if the Disturbing Presence player is Prone or Stunned“**: iteruje se `forEachOnPitch` (`engine/include/bb/game_state.h:99-105`), který filtruje `p.isOnPitch()`, a to je v `engine/include/bb/enums.h:19-21` definováno jako `STANDING || PRONE || STUNNED`. Ležící i omráčení se tedy počítají ✓ — a nefiltruje se `lostTacklezones`, což je taky správně, protože pravidlo o tackle zóně nemluví.
- **„each opposing player“**: `TeamSide enemySide = opponent(friendlySide);` a volající předává vlastní stranu házejícího (`passer.teamSide` na `pass_handler.cpp:263`, `catcher.teamSide` na `helpers.cpp:168`). ✓
- **Modifikátor je −1 za každého** (`count++` a odečet celého počtu), ne kapnutý na 1. ✓

### `Regeneration` — všechno kromě `Stakes` (viz L1)
Proti ř. 8406-8416:
- „**after the roll on the Casualty table and after any Apothecary roll**“ — hod je v `engine/src/injury.cpp:150`, tj. až za blokem apotékáře (`injury.cpp:130-144`) a za `rollCasualty`. ✓
- „**On a 4-6 … is placed in the Reserves box instead**“ — `injury.cpp:154-158` nastavuje `PlayerState::OFF_PITCH` a `position = {-1,-1}`. Komentář na ř. 147-149 sám dokládá, že to je opravená chyba („not left standing stunned on the pitch as we used to have it“). ✓
- „**On a result of 1-3, the player suffers the result of this injury**“ — propadne do `injury.cpp:161-163`, kde se nastaví `DEAD` nebo `INJURED`. ✓
- „**Regeneration rolls may not be re-rolled**“ — v kódu žádná reroll cesta pro tento hod není. ✓ (Igor z ř. 5453 / 2898 je poligová položka, engine ji nemá — mimo rozsah zápasového enginu.)

### `MultipleBlock` — jádro resolveru odpovídá textu
Proti ř. 8298-8306, kód `engine/src/block_handler.cpp:1038-1113`:
- „**each defender's strength is increased by 2**“ — `def1.stats.strength += 2` (ř. 1061) a `def2.stats.strength += 2` (ř. 1100), obojí se po bloku vrací na původní hodnotu. ✓
- „**The player cannot follow up either block**“ — oba bloky volají `resolveBlock(..., false, true)` (ř. 1068 a 1107), kde poslední parametr je `noFollowUp` (`engine/include/bb/block_handler.h:18-21`). ✓
- „**To have the option to throw the second block the player must still be on his feet after the first block**“ — `if (result.turnover || att.state != PlayerState::STANDING) return ActionResult::turnovr();` (ř. 1073-1075). ✓
- „**At the start of a Block Action**“ — akce se generuje v hlavní smyčce nabídky, `params.isBlitz = false` u obou bloků (ř. 1066, 1105), takže se nedá zahrát jako blitz. ✓
- **Cíle musí být stojící**: `rules_engine.cpp:230` požaduje `enemy->state == PlayerState::STANDING`, což odpovídá obecnému blokovacímu pravidlu, ř. 539-541: „You may only make a block **against a standing player** – you may not block a player who has already been Knocked Down.“ ✓
- **Foul Appearance zvlášť za každý blok** (ř. 1048-1056 a 1088-1096) odpovídá ř. 8127-8133 („any opposing player that wants to block the player … must first roll a D6“, tedy per blok) i FAQ ř. 9205-9209 („you must make a second roll for both of these skills“ u Frenzy). ✓

### `KickOffReturn` — dvě podmínky ze čtyř sedí
- „**This skill may not be used for a touchback kick-off**“ — celý blok je pod `if (!touchback)` (`engine/src/kickoff_handler.cpp:284`). ✓
- „**Only one player may use this skill each kick-off**“ — vnitřní smyčka (ř. 292-300) vybere jediného hráče (nejbližšího k míči) a jen tomu se pohyb provede (`if (closestKorId == p.id)`, ř. 302). ✓

### Rerolly — obnova o poločase
Ř. 941-943: „At half time the two teams get a chance to rest and recuperate, and so their team re-rolls are restored to **their starting level**.“ `engine/src/game_simulator.cpp:324-328` nastavuje `ts.rerolls = 3` právě a jen při `resetHalfState`, a při restartu drive po TD nikoli (komentář na ř. 313-315 to výslovně rozlišuje). Rerolly získané z kick-off tabulky se tím o poločase správně zahazují. ✓

### Kick-off `Pitch Invasion` — práh
Ř. 1352-1354: „If a roll is **6 or more after modification** then the player is Stunned… A roll of 1 before adding FAME will always have no effect.“ Kód `if (roll == 6)` (`kickoff_handler.cpp:175`) je při FAME = 0 ekvivalentní, protože D6 nedá víc než 6. ✓ (Odchylka je jen ve výběru hráčů, viz Ž7.)

### Kick-off `Get the Ref` a `Perfect Defence` — vědomá zjednodušení, ne chyby
- `Get the Ref` (ř. 1275-1283) uděluje úplatky proti vyloučení za faul. Engine je no-op (`kickoff_handler.cpp:59-61`) a komentář to přiznává. Vzhledem k tomu, že úplatek jen ruší vyloučení, je no-op konzervativní zjednodušení.
- `Perfect Defence` (ř. 1298-1301: „The kicking team’s coach may reorganize his players“) je no-op (`kickoff_handler.cpp:73-75`). Engine nemá volnou fázi rozestavení, takže „nechat sestavu, jak je“ je jediná dostupná interpretace.
Obojí uvádím jako **prověřená zjednodušení**, ne jako nálezy.

---

# Poznámka k metodě

Každé tvrzení o pravidlech v tomto dokumentu má číslo řádku a doslovnou citaci z `rules_bb2016.txt`. Kde jsem oporu nenašel, je to napsané explicitně — nejvýrazněji u **L1**, kde jsem prohledal všechny čtyři výskyty slova „Stakes“ (ř. 7710, 7918, 8496, 8504) i celý odstavec `Regeneration` (ř. 8406-8416) a **vazbu Stakes → blokuje Regeneraci jsem v textu nenašel**. Kód ji přitom má zapsanou jako fakt v komentáři (`engine/include/bb/injury.h:20`: „blocks Regeneration“). To je přesně vzorec z nálezu 24.08.2026: v kódu stojí *naše staré chování*, ne rulebook.

Tři místa, kde moje vlastní intuice s kódem souhlasila a text ji **nepotvrdil**, jsou:
1. `Stakes` vs `Regeneration` (L1) — v textu není.
2. `MultipleBlock` zakázaný hráči s Frenzy (L6) — text říká pravý opak („can be used instead of Frenzy“).
3. `DivingCatch` jako paušální bonus na chytání (L4) — text ho váže výhradně na přesnou přihrávku na jeho pole a odraz míče výslovně vylučuje.

Vše ostatní, co bylo v souladu, je vypsané v části E.
