# TRPASLIČÍ PROCEDURA — ÚPLNÁ SPECIFIKACE ÚKOLŮ NA KAŽDÉ KOLO
### verze 1, 11.08.2026 · zadání: „úkoly na jakékoliv kolo + jak to zkontrolovat + čím to provést a zalogovat“

Navazuje na: `project_bloodbowl_dwarf_procedure_20260810` ·
`..._turn_purpose_schedule_20260810` · `..._slack_switch_design_20260810` ·
`..._plan_compliance_measurement_20260810` · `..._dwarf_boxing_in_doctrine_20260810` ·
`..._dwarf_cage_corner_doctrine_20260807` · `..._loose_ball_denial_doctrine_20260807`.

Doplněno rešerší otevřeného webu 11.08. (zdroje na konci). **Věci z webu jsou
označené `[web]` a nejsou verdikt** — jsou to hypotézy k ověření proti CRP
a proti enginu, přesně podle chyby z rána 10.08.

---

## ČÁST 0 — RÁMEC

### 0.1 Zásada
**Nezapisujeme TAHY, zapisujeme POVINNOSTI.** Plán říká, *co má být na konci
kola pravda*; `search()` vybírá *jak*. Prior má sílu `f(rezerva)`.

### 0.2 Neměnné pořadí uvnitř kola (univerzální, ne trpasličí)
```
1. MÍČ      → 2. BLITZ  → 3. TVAR    → 4. MLÁCENÍ
   zajistit    utratit ho   klec/past    zbylá kapacita
```
Trpaslík se liší jen tím, že mu na patra 3–4 skoro nikdy nezbude volba.

### 0.3 Dvě kotvy, které rámují všechno ostatní
1. **Kotva je POSLEDNÍ kolo půle, ne „co nejdřív“.** TD v 6. kole daruje
   soupeři drive navíc.
2. **`[web]` Kotva je celý ZÁPAS, ne drive.** Cíl trpaslíka není „skórovat
   při každém držení“, ale **skórovat přesně dvakrát, pokaždé v posledním
   kole půle** → 2:1. To je „the 2-1 grind“.
   ⚠️ **Nové a námi vůbec nemodelované:** zdroj tvrdí, že mlátící tým chce
   spíš **KOPAT první** — nechá rychlého soupeře skórovat rychle, sám melne
   kola 3–8, a ve druhé půli melne dalších 8. To dává *„efektivně 15 kol
   klece“* místo 8. My **volbu při hodu mincí vůbec nemodelujeme** ⇒ viz
   Část 6, otevřená položka O1.

### 0.4 ⭐ REZERVA — jediný přepínač (rozšířeno oproti 10.08.)
```
turnsLeft   = 9 − turnNumber
paceAch     = achievablePace          (cage_advance.h:100 — už se počítá)
turnsToBall = 0, držíme-li míč
            = ceil(dist(nejbližší schopný hráč, míč) / MA)      jinak
turnsNeeded = turnsToBall + ceil(dist(míč, endzone) / paceAch)
REZERVA     = turnsLeft − turnsNeeded
```
**Rozšíření o `turnsToBall` je to podstatné.** Dosud byla rezerva definovaná
jen s míčem v ruce. S tímhle členem **„seber míč v 1. kole“ přestává být
zvláštní pravidlo a stává se aritmetikou:**

| | |
|---|---|
| pochod od výkopu | **medián 22 polí** (72 % drivů delších než 20,9) |
| kol na pochod | ~7 (osmé je TD) |
| ⇒ **nutné tempo** | **3,14 pole/kolo** |
| doktrinální pásmo mletí | 2–3 |
| **dosahujeme** | **1,73** |
| první kolo, kdy držíme míč | **medián 2** |

⚠️ **OPRAVENO 11.08. odpoledne.** Původní verze uváděla pochod 20,9 pole,
nutné tempo 2,61 a *„první držení míče v kole 4,1"*. **Všechna tři čísla
byla špatně:** pochod je delší, tempo tedy vyšší, a „kolo 4,1" byl artefakt
drivů startujících uprostřed půle — v plných drivech držíme míč vždy
v kole 1–2. ⇒ **„Seber míč v prvním kole" NENÍ naše slabina. Tempo je.**
Zdroj: [[project-bloodbowl-drive-failure-decomposition-20260811]].

⭐ **Závěr:** trpaslík musí jet **3,14 pole každé kolo** a dělá **1,73**.
Rozdíl není v tom, že by vyrážel pozdě — vyráží včas. **Nedojde.**
⇒ Jediná páka je **tempo**, a to znamená **čistit koridor** (bod [9]),
protože při odporu 5–7 nosič stojí 4–7 kol na místě.

### 0.5 Tři režimy podle rezervy
| rezerva | režim | účel tahu | „hotovo“ |
|---|---|---|---|
| ≈ 0 | **ROZVRH** | splnit kvótu postupu | kvóta splněna |
| > 0 | **OPCE** | míč v bezpečí, možnost zachována | míč krytý a nemarkovaný |
| < 0 | **NOUZE** | prorazit s rizikem **nebo** držet hodiny | viz S4 |

---

## ČÁST 1 — KLASIFIKACE SITUACE (rozhodovací strom)

Vyhodnotit na **začátku každého kola**, ze `GameState`. Právě jedna větev.

```
┌ fáze == SETUP ────────────────────────────────────────────┐
│   přijímáme?  ──ano──► S0  ROZESTAVENÍ NA PŘÍJEM          │
│               ──ne───► S1  ROZESTAVENÍ NA VÝKOP           │
└───────────────────────────────────────────────────────────┘
┌ fáze == PLAY ─────────────────────────────────────────────┐
│ míč drží NÁŠ hráč?                                        │
│   ├ ano                                                   │
│   │   dist(nosič, endzone) ≤ MA+2  ?                      │
│   │     ├ ano ── je to POSLEDNÍ kolo půle nebo rezerva<0? │
│   │     │         ├ ano ► S10 SKÓRUJ TEĎ                  │
│   │     │         └ ne  ► S9  DRŽ SE NA PRAHU (stall)     │
│   │     └ ne ── rezerva > 0 ► S3  OPCE                    │
│   │             rezerva ≈ 0 ► S2  ROZVRH (jádro grindu)   │
│   │             rezerva < 0 ► S4  NOUZE                   │
│   ├ míč drží SOUPEŘ                                       │
│   │     zbývají jim ≤ 2 kola půle? ► S8 ZABRAŇ SKÓRE      │
│   │     jinak                      ► S7 BOXING-IN         │
│   └ míč VOLNÝ (na zemi / po odrazu)                       │
│         dosáhneme na něj a umíme ho ZAJISTIT? ► S5 SEBER  │
│         nedosáhneme, ale soupeř ano?          ► S6 ODEPŘI │
│         nedosáhne nikdo                       ► S6' TVAR  │
└───────────────────────────────────────────────────────────┘
```

⚠️ **S5 vs S6 je trojí volba, ne dvojí** (upřesnění 10.08.): *vzít* jen když
umím i *zajistit*; jinak *odepřít*; „nedělat nic“ je skoro vždy nejhorší.
**Do rozhodnutí vstupuje CENA FIGURKY**, ne jen riziko tahu.

---

## ČÁST 2 — KATALOG POVINNOSTÍ

Notace: **[P]** povinné (nesplnění = kolo neplní plán) · **[S]** podmíněné ·
**[V]** volitelné, když zbude kapacita.

---

### S0 — ROZESTAVENÍ NA PŘÍJEM

| # | povinnost | zdroj |
|---|---|---|
| S0.1 **[P]** | přesně 3 hráči na LOS, ani jeden Runner | CRP ř. 8 (min. 3) |
| S0.2 **[P]** | ≤ 2 hráči v každé wide zone | CRP ř. 8 |
| S0.3 **[P]** | `[web]` v wide zone **nikdy na LOS**, ale **2 pole vzad** | Creative Twilight |
| S0.4 **[P]** | **budoucí nosič (Runner) stojí tak, aby v 1. kole dosáhl na míč** — tj. `dist(Runner, očekávaný dopad) ≤ MA` | 0.4 |
| S0.5 **[P]** | **záloha vedle míče PŘED hodem** — druhý hráč v dosahu pro případ fumble | item 11 |
| S0.6 **[P]** | `[web]` rozestavení musí **přežít výsledek BLITZ na tabulce výkopu** — žádný osamocený hráč v zadním poli | Creative Twilight |
| S0.7 **[S]** | 4 budoucí rohy klece (Longbeardy) do bloku kolem očekávaného dopadu | doktrína rohů |
| S0.8 **[V]** | Kick-Off Return (dnes ho v rosteru TV1200 **nemá nikdo**) | balík E |

---

### S1 — ROZESTAVENÍ NA VÝKOP (obrana)

| # | povinnost | zdroj |
|---|---|---|
| S1.1 **[P]** | 3 na LOS, ≤ 2 na wide zone | CRP |
| S1.2 **[P]** | `[web]` **nedat soupeři víc než 4 bloky v jeho 1. kole** (3 LOS + 1 blitz) ⇒ na LOS jen AV9 s Block | Steam GBU |
| S1.3 **[P]** | **krýt celou šířku**, nepřevažovat na jeden kraj | doktrína |
| S1.4 **[P]** | `[web]` **nikdo na LOS ve wide zone**; wide-zone hráči 2 pole vzad | Creative Twilight |
| S1.5 **[P]** | žádný náš hráč nesmí být po výkopu **osamocený a obklopitelný** | GBU „potato play“ |
| S1.6 **[S]** | proti rychlému týmu spíš plošné krytí (typ 3-4-4), ne hloubka | `[web]` |
| S1.7 **[V]** | hráč s Kick (halving scatter) — v našem rosteru není | CRP |

---

### S2 — ROZVRH (jádro trpasličího útoku, rezerva ≈ 0)

**To je situace, ve které se hraje většina trpasličích kol.**

| # | patro | povinnost | „hotovo“ |
|---|---|---|---|
| S2.1 **[P]** | 1 míč | **nosič je Runner** (nejvyšší MA se Sure Hands), ne Longbeard | `carrier.role == RUNNER` |
| S2.2 **[P]** | 1 míč | **nosič není markovaný** na konci kola | 0 soupeřových TZ na nosiči |
| S2.3 **[P]** | 1 míč | **nosič nedodgoval** `[web]` „never dodge with the carrier“ | 0 DODGE událostí nosiče |
| S2.4 **[P]** | 2 blitz | **blitz použit** — cíl podle žebříčku (S2.10) | MOVE+BLOCK týmž hráčem |
| S2.5 **[P]** | 3 tvar | **klec: 4 rohy obsazené** (diagonály nosiče) | 4/4 obsazeno |
| S2.6 **[P]** | 3 tvar | ⭐ `[web]` **ŽÁDNÝ z pěti hráčů klece nekončí v soupeřově TZ** | ∀ 5: 0 TZ |
| S2.7 **[P]** | 3 postup | ⭐ **kvóta postupu splněna — a kvóta JE FUNKCÍ ODPORU, ne konstanta** (revize 1): odpor 0 ⇒ rychlost klece (rohy MA5); odpor ≥ 2 ⇒ doktrinální 2–3 | Δx nosiče ≥ kvóta(odpor) |
| S2.8 **[S]** | 3 postup | ⭐ `[web]` **posun do strany se počítá jako splnění**, pokud přitom jiní hráči postoupili a připravili příští pozici klece | viz K7 |
| S2.9 **[S]** | 3 koridor | **prorazit zeď**: `plan.resistance` má koho ⇒ blitz míří tam | cíl blitzu ∈ resistance |
| S2.10 **[P]** | — | **žebříček cílů blitzu**: 1) prorvat koridor · 2) odmarkovat nosiče · 3) nejšťavnatější cíl | první splnitelné patro |
| S2.11 **[S]** | 3 koridor | ⭐ `[web]` **proti sloupcové obraně nestačí jeden blitz** — soupeř stojí *2 pole od sebe a 2 do hloubky*; prorazit první řadu bez obsazení druhé je past. ⇒ **blitz + druhé tělo do mezery v témže kole** | ≥ 2 naši hráči v prolomeném koridoru |
| S2.12 **[V]** | 4 mlácení | zbylé aktivace na cíle, které slouží plánu | — |
| S2.13 **[P]** | — | **žádný hráč nezůstane bez aktivace**, pokud má bezpečný užitečný tah | 0 nevyužitých |
| S2.14 **[P]** | — | `[web]` **pořadí: bezpečné akce první, rizikové poslední** | index rizika neklesá |

**Šťavnatost cíle** (vše dopočitatelné, žádné vzorkování): nízké AV
(AV7 se probije v 41,7 %, AV9 v 16,7 %) · nemá Block · má Dodge a my Tackle
(33 % → 50 %) · stojí sám · je u čáry (surf) · je pro soupeře drahý
× kolik kostek si na něj umíme postavit.

---

### S3 — OPCE (rezerva > 0)

Trpaslík se sem dostane zřídka (např. 3 pole od endzone se 4 koly), ale
**musí to být platný stav, jinak mu rámec vnutí kvótu a rozbije ho.**

| # | povinnost |
|---|---|
| S3.1 **[P]** | míč krytý (klec nebo screen), nosič nemarkovaný |
| S3.2 **[P]** | blitz použit (patro 2 platí vždy) |
| S3.3 **[P]** | **nesnižovat rezervu zbytečně** — nepostupovat tak, aby se z OPCE stal ROZVRH |
| S3.4 **[P]** | **nepodstupovat žádný hod, který plán nevyžaduje** |
| S3.5 **[S]** | spouštěč skórování: *soupeř tě už nemůže zastavit* NEBO *čekání stojí víc než skórování* |

---

### S4 — NOUZE (rezerva < 0) — ⭐ jediné místo, kde má trpaslík co se učit

Podle měření 10.08. **to není okrajový případ, ale NORMÁL** (míč poprvé
v kole 4,1). Dvě větve, uzavřený vzorec neexistuje:

| větev | co to znamená | kdy |
|---|---|---|
| **PRORAZIT** | zahodit klec (je pomalá), GFI řetěz, předat Runnerovi, riskovat | vedeme-li o TD to nemá cenu; za stavu 0:0 ke konci půle ano |
| **DRŽET HODINY** | míč do hromady, nepostupovat, ubírat soupeři kola | 0:0 v poločase je pro trpaslíka dobrý výsledek |

| # | povinnost |
|---|---|
| S4.1 **[P]** | **rozhodnout se pro jednu větev a zapsat ji** — kolo bez zvolené větve je nesplněné |
| S4.2 **[P]** | při DRŽET: nosič v maximálním obklopení, 0 pohybu vpřed, blitz na attrition |
| S4.3 **[P]** | při PRORAZIT: riziko akceptováno **v pořadí od nejmenšího**, nosič poslední |
| S4.4 **[P]** | ⚠️ **do rozhodnutí vstupuje skóre, kdo kope další drive a kolik soupeř stihne** — ne jen geometrie |

---

### S5 — VOLNÝ MÍČ, DOSÁHNEME (dnes největší díra: 96 % dosažitelnost, 53 % pokus)

| # | povinnost |
|---|---|
| S5.1 **[P]** | **pokus o sběr proběhl** — nebo je zdůvodněný přechod na S6 |
| S5.2 **[P]** | sbírá **Runner** (Sure Hands ruší postih za TZ), ne kdokoli |
| S5.3 **[P]** | ⭐ **záloha stojí vedle míče PŘED hodem** (fumble → odraz do našeho TZ / na našeho hráče) |
| S5.4 **[P]** | ⭐ **zajištění, ne sebrání**: po sebrání je nosič krytý a nedosažitelný soupeřovým blitzem |
| S5.5 **[S]** | pokud S5.4 nejde splnit ⇒ **přepni na S6**, míč neber |
| S5.6 **[P]** | pořadí: **nejdřív odmarkovat pole kolem míče, pak sbírat** (blitz→pickup, doktrína 07.08.) |

---

### S6 — VOLNÝ MÍČ, ODEPŘENÍ (dnes naše nejlepší disciplína, 70 %)

| # | povinnost |
|---|---|
| S6.1 **[P]** | **žádné soupeřovo pole vedle míče není volné a bez našeho TZ** |
| S6.2 **[P]** | soupeřův nejlepší sběrač je markovaný nebo sražený |
| S6.3 **[P]** | blitz utracen na toho, kdo by míč sebral |
| S6.4 **[S]** | **nepřibližovat vlastní drahé hráče na dostřel**, když míč stejně nebereme |

---

### S7 — BOXING-IN (soupeř drží míč, není konec půle)

**Cíl NENÍ zabránit TD. Cíl je vynutit ho v 7.–8. kole, nebo vůbec** —
a mezitím sbírat jejich hráče.

| # | povinnost | pozn. |
|---|---|---|
| S7.1 **[P]** | **nehonit, nevybíhat, nepřevažovat** — krýt šířku | pasivní část |
| S7.2 **[P]** | ⭐ **odebírat únikové pole**: počet volných polí, kam může nosič odejít, musí každé kolo **klesat** | nová metrika |
| S7.3 **[P]** | **vytlačit k lajně** — lajna je zadní stěna zdarma | |
| S7.4 **[P]** | **uzavřít do L/U**: diagonální jistící hráči na krajích, tlačící zepředu | tvar |
| S7.5 **[P]** | blitz na nosiče, když je pravděpodobnost sražení dost velká; jinak na jeho eskortu | patro 2 |
| S7.6 **[S]** | **Slayeři s Frenzy čistí okraj** — 1–2 pole od lajny = surf dvěma ranami | `[web]` potvrzuje 1–2 pole |
| S7.7 **[P]** | ⚠️ `[web]` **nemarkovat bezúčelně** — marker sám vystavuje našeho hráče | GBU „aimless marking“ |
| S7.8 **[P]** | ⚠️ **nedat se surfovat** — na konci kola nikdo náš zbytečně 1 pole od lajny | GBU |
| S7.9 **[V]** | **foul** na klíčového ležícího soupeře, když je blitz utracen a kapacita zbývá | `[web]` foul je legitimní nástroj |

⚠️ **Rozpor k rozhodnutí (poctivě):** jeden webový zdroj radí obraně
**svádět soupeře do STŘEDU** hřiště, naše doktrína ho **tlačí k LAJNĚ**.
Nejsou to protiklady na téže vrstvě — *formací* se zavírá běh kolem křídla,
*kontaktem* se tlačí k lajně. Ale **v kódu se to potká** a musí to rozsoudit
jedno pravidlo. → otevřená položka O3.

---

### S8 — ZABRÁNIT SKÓRE (soupeři zbývají ≤ 2 kola)

| # | povinnost |
|---|---|
| S8.1 **[P]** | **žádná volná cesta do endzone** — pokrýt pásmo `MA+2` před nosičem |
| S8.2 **[P]** | přepnout z attrition na blokádu: teď už se nesbírá, teď se zavírá |
| S8.3 **[P]** | blitz na nosiče **vždy**, i za horších kostek — turnover teď stojí soupeře celý drive |
| S8.4 **[S]** | hlídat i **příjemce přihrávky** (ne jen nosiče) |

---

### S9 — DRŽ SE NA PRAHU (v dosahu endzone, ale skórovat je brzy)

| # | povinnost |
|---|---|
| S9.1 **[P]** | **neskórovat** |
| S9.2 **[P]** | nosič v dosahu endzone, **krytý a nemarkovaný** |
| S9.3 **[P]** | zbytek týmu **mlátí a odklízí**, ne postupuje |
| S9.4 **[P]** | blitz utracen |
| S9.5 **[P]** | ⚠️ spočítat, že **skórování bude proveditelné i po soupeřově kole** (rezerva na jeho blitz) |

---

### S10 — SKÓRUJ TEĎ

| # | povinnost |
|---|---|
| S10.1 **[P]** | **TD padne v tomto kole** |
| S10.2 **[P]** | cesta nosiče **bez zbytečného hodu** — dodge/GFI jen když jinak nelze |
| S10.3 **[P]** | ⭐ `[web]` **pořadí: nejdřív odklidit překážky ostatními hráči, nosič běží POSLEDNÍ** |
| S10.4 **[S]** | pokud TD nevyjde jistě, raději S9 — pokud zbývá kolo |

---

## ČÁST 3 — ZÁKAZY (anti-pravidla, kontrolují se stejně tvrdě)

`[web]` z „Good, Bad and Ugly Habits“ + naše nálezy:

| # | zákaz | proč |
|---|---|---|
| Z1 | **nosič nedodgeuje** | zbytečný hod na nejdražší figurce |
| Z2 | **nosič nekončí v TZ** | dá soupeři blitz zdarma |
| Z3 | **roh klece nekončí v TZ** | soupeř ho odblokuje a klec se otevře |
| Z4 | ⚠️ **PŘEPSÁNO 11.08.:** 1kostkový blok je zakázaný **jen hráči BEZ Block** (33 % turnover). S Block je to 16,7 % a u trpaslíka legitimní nástroj (příručka: Block a Tackle dělají 1D bloky „s klidem“). **Ale je to FALLBACK:** nejdřív přivést asistenci (nejlépe Guard) a uděřit na dvě kostky = 2,8 %. Nikdy nosičem. | |
| Z5 | **žádný „červený“ (uphill) blok** bez důvodu | |
| Z6 | **žádné bezúčelné GFI** | stejná chybovost jako riskantní blok |
| Z7 | **nikdo zbytečně 1 pole od lajny** (surf) | |
| Z8 | **nemarkovat bezúčelně** | marker se vystavuje |
| Z9 | **nespotřebovat aktivace na mlácení dřív, než jsou patra 1–3 hotová** | past, před kterou uživatel varoval |
| Z10 | **neztratit blitz** — kolo bez blitzu je promarněný zdroj | 1× za kolo, ~16× za zápas |
| Z11 | **neskórovat brzy** | daruje soupeři drive |
| Z12 | **nepřevažovat na jeden kraj** | |
| Z13 | **nerozbít klec kvůli mlácení** | |
| Z14 | **neriskovat, když je účel kola splněný** | risk-last jako důsledek, ne heuristika |

---

## ČÁST 4 — KONTROLY

### 4.1 ⭐ Klíčový nález o datech: konec kola JE k dispozici
`captureTurnSnapshot()` (`game_simulator.cpp:519`) se volá **jen na začátku
kola**. Skript z 10.08. proto rekonstruoval konec kola z `to_x/to_y`
událostí — to je nepřesné (hráč, který se nehnul, nemá událost).

**Nemusí se to tak dělat.** `turnLogs[i+1]` je snímek pořízený hned po konci
mého kola ⇒ **je to přesně end-of-turn stav.** Výjimka: kolo, po kterém
následuje TD nebo poločas (`setupDrive` / `setupHalf` mezi tím přestaví
hřiště). ⇒ **oprava rozhodčího, ne enginu.**

### 4.2 Predikáty — co se dá spočítat DNES

Vstup: `S = turnLogs[i]` (start), `E = turnLogs[i+1]` (konec), `V = S.events`.

| kód | kontrola | výpočet |
|---|---|---|
| K1 | pokus o sběr | ∃ `PICKUP` s naším `player_id` |
| K2 | sběr úspěšný | týž event, `success == true` |
| K3 | nosič je Runner | `E.ballCarrierId` → `name` obsahuje „Runner“ |
| K4 | nosič nemarkovaný | 0 soupeřů v Chebyshev-1 od nosiče v `E` |
| K5 | nosič nedodgoval | ¬∃ `DODGE` s `player_id == carrier` |
| K6 | rohy klece | počet obsazených diagonál nosiče v `E` (0–4) |
| K7 | ⭐ **rohy ČISTÉ** | ∀ 5 hráčů klece: 0 soupeřů v Chebyshev-1 |
| K8 | postup nosiče | `Δx` mezi `S` a `E` |
| ~~K9~~ | ⚠️ **VADNÁ — neměří to, co S2.7 přikazuje** (audit 12.08., nález N5). Počítala konstantu, kdežto S2.7 po revizi 1 říká, že **kvóta je FUNKCÍ ODPORU**. Kdyby rozhodčí běžel, měřil by špatně a nikdo by to nepoznal, protože obojí je v dokumentu a vypadá to konzistentně. Rozděleno na K9a/K9b. | — |
| **K9a** | **rozvrhová PODLAHA** — pod tohle se nesmí spadnout, jinak se nedoskóruje | `Δx ≥ ceil(zbývá / (9−turn))` |
| **K9b** | **kvóta podle odporu** (S2.7) | `Δx ≥ kvóta(odpor)`; odpor **0** ⇒ rychlost klece = `min(MA)` přes pětici (dnes 4, nosič je MA4) · odpor **≥ 2** ⇒ doktrinální 2–3 · odpor **1** ⇒ ⚠️ **NEROZHODNUTO, viz O9** |
| K10 | blitz použit | ∃ hráč s `PLAYER_MOVE` i `BLOCK` (⚠️ dolní odhad, viz 4.3) |
| K11 | blitz na tělo v cestě | cíl blíž endzone než nosič a ≤ 4 pole |
| K12 | odepření | blokujeme nosiče **nebo** u něj na konci stojí náš hráč |
| K13 | ⭐ **volná pole nosiče soupeře** | počet polí kolem soupeřova nosiče bez naší TZ a bez těla — a jeho **pokles** proti minulému kolu |
| K14 | vytlačení k lajně | `min(y, 14−y)` soupeřova nosiče klesá |
| K15 | nevyužité aktivace | hráči stojící v `S` bez jediné události |
| K16 | nosič v TZ (Z2) | negace K4 |
| K17 | roh v TZ (Z3) | negace K7 |
| K18 | surf-riziko (Z7) | naši hráči v `E` s `min(y,14−y) ≤ 1` bez důvodu |
| K19 | turnover | `S.turnover` |
| K20 | TD v kole | `S.touchdown` |
| K21 | rozestavení: LOS | počet našich na `x == LOS` v prvním snímku drivu |
| K22 | rozestavení: wide zone | počet našich v `y ≤ 3` a `y ≥ 11` |
| K23 | Runner v dosahu míče při výkopu | `dist(Runner, dopad) ≤ MA` |
| K24 | záloha u míče před hodem | ∃ náš hráč v Chebyshev-1 od míče |
| K25 | skórovali jsme brzy (Z11) | `touchdown && turnNumber < 8 && rezerva > 0` |
| K26 | ⭐ **rezerva** | vzorec 0.4, na začátku i na konci kola |
| K27 | režim | ROZVRH / OPCE / NOUZE z K26 |
| K28 | situace | S0–S10 podle stromu Části 1 |
| **K29** | ⭐ **R1 — roh klece nesmí markovat** | obsazené diagonály nosiče v `E`, každá proti soupeřům splňujícím `threatens()` (stojící **+ ležící s Jump Up**) |
| **K30** | **R3 — značkovat tam, kde dodge NĚCO STOJÍ** *(přepsáno 13.08. z „AG3": elfové jsou celí AG4, na AG3 se proti nim nedá řadit)* | podíl stojících soupeřů, kterým únik stojí ≥ 20 % turnoveru a mají vedle sebe našeho stojícího, který **není roh klece ani nosič** (R1 > R3). Cena = P(selhání dodge) a **závisí na dvojici**: náš `Tackle` ruší soupeři `Dodge` reroll |
| **K31** | **R4 — tělo bez úkolu** | náš stojící hráč bez jediné události, který není nosič, roh ani soused soupeře |
| **K32** | pořadí blitzu (zeď → odmarkovat nosiče → příležitost) | ⚠️ **blokováno na X1** — blitz se v logu nepozná od bloku |
| **K33** | **kolo bez jediného bloku** | 0 událostí `BLOCK` s naším `player_id` |

Implementováno 12.08. v `diag_rules_checks_20260812.py` (K29, K30, K31, K33,
K9a). K32 čeká na X1.

⚠️ **TVAR KAŽDÉ KONTROLY JE ZÁVAZNÝ (13.08.).** Kontrola se vydává jako
povinná trojice `Check(ok, n, deg)` — **splněno / posuzováno / degenerovaných**
— a predikát nad **prázdnou množinou hlásí `N/A`, ne úspěch**. Kontrola, která
trojici nevyplní, se nedá vytisknout.

Není to formalita. Šest kontrol měřilo něco jiného, než čím se hlásily, a obě
příčiny byly tyhle dvě: **jediné procento bez viditelného jmenovatele** (K31
dělila čitatel z kol s míčem počtem všech kol — 0,86 místo 1,80) a **prázdná
množina počítaná jako splnění** („žádný roh není markovaný" je pravda i bez
klece, a to byla třetina kol). Platí i pro kontroly, které teprve vzniknou.

### 4.3 Predikáty, které DNES spočítat NELZE (a co pro ně dodělat)

| kód | kontrola | co chybí | oprava |
|---|---|---|---|
| **X1** | **blitz z místa** (deklaruji blitz, nehnu se, udeřím) | v logu k nerozeznání od bloku | `GameEvent`: `bool isBlitz` na BLOCK |
| **X2** | **kolik kostek měl blok** (Z4, Z5) | `roll/die1/die2` jsou výsledky, ne počet kostek | `GameEvent`: `int8_t blockDice` (+/−), `bool chooserWasUs` |
| **X3** | **záměr vs. výsledek** — proč se nesebralo | nelogujeme deklarovaná makra | `TurnLog`: `vector<MacroRecord>{type, playerId, order}` |
| **X4** | **pořadí rizika** (S2.14) | žádný index aktivace | plyne z X3 (`order`) |
| **X5** | **byl sběr vůbec legální?** | neznáme množinu legálních maker | `TurnLog`: bitmaska dostupných typů maker na začátku kola |
| ~~**X6**~~ | ✅ **Z ~80 % HOTOVO** (audit 12.08.) — commit `31efa93` přidal `TurnPlanRecord` (`engine/include/bb/turn_plan_record.h`), navěšený na `game_simulator.h:78`: `goal`, `verdict`, `adopted`, `distToEndzone`, `turnsLeft`, `requiredPace`, `achievablePace`, `step`, **`resistance`**, `filledCorners`, `openCorners`, `carrierGfi`, `exposure`. ⇒ **vstupy pro K9b už jsou k dispozici.** Chybí jen `resistanceIds[]` — máme počet, ne kdo. | doplnit jen ID |
| **X7** | **GFI bez důvodu** (Z6) | GFI eventy jsou, ale ne „bylo to nutné“ | plyne z X6 (kvóta) |

⭐ **X6 je nejlevnější a nejcennější:** ta čísla se **už počítají**, jen se
nikam nezapisují. Tím se rozhodčí přestane dohadovat, co engine chtěl.

---

## ČÁST 5 — NÁSTROJE (co postavit, v pořadí)

### N1 — patch logování (engine) · malý, mechanický
`game_event.h` · `game_simulator.h` · `cage_advance.cpp` → viz X1, X2, X3, X5, X6.
Serializace do `diag_replay_*` korpusů.
**Pozor:** mění se struktury sdílené s tréninkem ⇒ **jen aditivně**, žádné
přejmenování, ať staré korpusy zůstanou čitelné.

### N2 — `diag_turn_referee_20260811.py` · rozhodčí zvenčí, v2
Nástupce `diag_plan_compliance_20260810.py`. Tři změny proti v1:
1. **konec kola z `turnLogs[i+1]`** místo rekonstrukce z událostí (4.1);
2. **klasifikace S0–S10** místo čtyř cílů (dnes umí jen PICKUP/ADVANCE/SCORE/DENY);
3. výstup = **karta kola**, ne jen agregát.

**Karta kola** (jeden řádek JSONL na kolo):
```json
{"match":12,"half":1,"turn":3,"side":"home","race":"dwarf",
 "situace":"S2","rezerva":-1,"kvota":3,"postup":2,
 "P":{"S2.1":true,"S2.2":false,"S2.4":true,"S2.5":false,"S2.6":false,...},
 "Z":{"Z2":true,"Z10":false,...},
 "splneno":false,"proc":["S2.5 rohy 1/4","S2.2 nosič markován"]}
```
⭐ **Pole `proc` je to hlavní** — bez něj je z agregátu zase jen číslo.

### N3 — `diag_referee_report.py` · sestavy
* **na kolo** — karta výše;
* **na drive** — kolik z 8 kol splněno, kdy poprvé míč, průměrné tempo, kde se to zlomilo;
* **na zápas** — 2-1 grind vyšel / nevyšel;
* **na rasu** — kontrolní pětice (dwarf/orc/skaven/wood-elf/human) touž metrikou.

### N4 — ⭐ kalibrace rozhodčího proti člověku · **než se čemukoli uvěří**
Podle `feedback_present_situations_not_just_math` a metodiky „důkaz učení“:
vzít **20 náhodných trpasličích kol**, vykreslit hřiště, uživatel je posoudí
ručně („splněno / nesplněno a proč“), a **teprve když se rozhodčí shodne
v ≥ 18 z 20**, smí se jeho agregát citovat.
⇒ Zapadá do už zapsaného úkolu **hra v terminálu s koučováním**
(`project_bloodbowl_terminal_coaching_game_20260810`) — je to týž nástroj:
kreslení hřiště + výpis povinností a kontrol.

### N5 — vstřelení plánu do enginu (až po N1–N4)
Prior se sílou `f(rezerva)` podle `slack_switch_design`. **Až naposled** —
dokud neumíme změřit plnění, nemáme podle čeho ladit.

**Pořadí je záměrné:** N2+N3 měří **dnešní** engine bez jediné změny chování,
takže vzniká baseline. N1 je jen zpřesnění. N5 je jediná změna chování.

---

## ČÁST 6 — OTEVŘENÉ POLOŽKY

| # | otázka | proč teď nejde uzavřít |
|---|---|---|
| **O1** | ⭐ **kopat, nebo přijímat?** `[web]` říká, že mlátící tým chce kopat první (2-1 grind, „15 kol klece“). **My volbu vůbec nemodelujeme.** | vyžaduje modelovat celý zápas, ne drive; ale je to potenciálně větší páka než cokoli uvnitř kola |
| **O2** | ⚠️ **FAKTICKY ZODPOVĚZENO, zbývá zapsat jako povinnost.** 11.08.: odpověď je **„skoro nikdy"** — předávat až když nutné tempo přeroste ~3 pole/kolo; do té doby je pomalý nosič uvnitř celé klece lepší než rychlý s dírou v rohu. 12.08. doplněno: **záloha u míče musí být potenciální NOSIČ** (Blitzer AG3 MA5). A `41c3570` mezitím zvedl podíl kol s Runnerem na 79–89 % ⇒ premisa „ve 44 % nese Longbeard" už neplatí. | zbývá formulace, ne rozhodnutí |
| ~~**O3**~~ | ✅ **UZAVŘENO 11.08.** rozhodnutím uživatele: **tlačíme k LAJNĚ** (lajna nahrazuje těla, Slayeři s Frenzy trestají neopatrnost). Viz REVIZE 1 / R2. | — |
| ~~**O4**~~ | ✅ **UZAVŘENO 11.08.**: tvar zůstává stejný, **rohy = LONGBEARDI**, jakmile jsou kolem markeři; Slayer je špatný roh (AV8 + povinný Frenzy ho z rohu vytáhne), Blitzer je blitzovací rameno. Identita rohu je funkcí fáze. | — |
| ~~**O5**~~ | ✅ **UZAVŘENO 11.08.** (REVIZE 1 / R5b): není to doktrína, je to **výpočet** `hodnota(T) = díra(T) × P(sražení T)`; díra se měří v tackle zónách a průchodem, prahy 3 a 5 polí, jednopolová díra je past. | — |
| **O6** | **rozhodovací pravidlo NOUZE: prorazit vs. držet** (S4) | závisí na skóre a na tom, kdo kope další drive |
| **O7** | **Underworld** — uživatel zmínil, že i tam jde plán vyjádřit jasně; důvod neprobrán | doptat se, až bude aktuální |
| **O9** | ⭐ **NOVÁ (12.08., z opravy K9): jaká je kvóta postupu při ODPORU 1?** S2.7 určuje jen krajní body — odpor 0 ⇒ rychlost klece, odpor ≥ 2 ⇒ 2–3. Prostředek chybí, a přitom je to nejčastější případ. **Rozhodnutí uživatele, ne výpočet** — jde o to, jestli se při jediném tělu v cestě ještě jede naplno, nebo už se mele. | doktrinální mezera, odhalená až tím, že se K9 srovnávala s S2.7 |

---

## ČÁST 7 — CO Z TOHO PLYNE JAKO POŘADÍ PRÁCE

1. **N2 + N3** (rozhodčí v2 + sestavy) — měří dnešní engine, nemění nic.
2. **N4** (kalibrace proti uživateli, 20 kol) — bez ní jsou čísla nedůvěryhodná.
3. **N1** (logovací patch, hlavně **X6** a **X1**) — zpřesní, co rozhodčí vidí.
4. **N5** (vstřelení plánu) — jediná změna chování, A/B-ovatelná.

**Nejlevnější velká věc:** **K7 (rohy ČISTÉ)**. Dnes měříme, že rohy jsou
v 39 % kol prázdné. `[web]` říká, že roh v soupeřově TZ je **stejně špatný
jako prázdný** — a to jsme nikdy neměřili. Reálné číslo klece tedy může být
výrazně horší než 8 % se čtyřmi rohy.

---

## REVIZE 1 (11.08. odpoledne) — čtyři změny proti verzi výše

### R1. ⭐ ODPOVĚĎ: trpaslík potřebuje na TD **SEDM kol**, ne deset
Změřeno (`diag_pace_vs_contact_20260811.py`, `evidence/pace_vs_contact_20260811.log`):

| nosič | MA | kol | postup/kolo | kol na 20,9 pole |
|---|---|---|---|---|
| **Runner** | 6 | 22 | **3,41** | **6,1** |
| **Longbeard** | 4 | 24 | **1,50** | **14** |

Nutné tempo je 2,61–2,79. **S Runnerem se rozvrh vejde s rezervou.**
Longbeard nese v **49 % kol** — a to je ten rozdíl mezi 7 a 14 koly.

**A druhý nález, který ruší předpoklad celé části 0.4:** při **nulovém
odporu** dělá trpaslík jen **2,69 pole** — člověk 4,31, ork 3,56.
**Predikce byla, že při odporu 0 musí být ≥ 4, je-li 2,4 doktrína.
Vyvráceno ⇒ je to VADA.** Klec se plazí i tam, kde jí nic nebrání.

⇒ **Tempo 2–3 je tempo MLETÍ, ne tempo drivu.** Rozpočet kol:

| kolo | co se má stát | x nosiče |
|---|---|---|
| 1 | zajistit míč (Runner) | 4 → 6 |
| 2–3 | **volný pochod rychlostí klece MA5** | 6 → 11 → 15 |
| 4–6 | mletí 2–3 | 15 → 18 → 20 → 22 |
| 7 | **TD** | 22 → 25 |
| 8 | **rezerva na jeden turnover** | |

### R2. ⭐ O3 UZAVŘENO — tlačíme k LAJNĚ (rozhodnutí uživatele)
Důvody: (a) **lajna nahrazuje těla** — na obklíčení u čáry stačí míň hráčů;
(b) **Troll Slayeři s Frenzy** promění neopatrnost soupeře v surf.
Webová rada „sváděj do středu" **neplatí pro nás** — ta je pro obranu, jejímž
cílem je *zabránit skóre*. Naše obrana bere *hráče*. ⇒ S7.3 potvrzeno,
rozpor smazán. Zůstává: pravidlo o krytí šířky (S1.3) platí pro **formaci**,
tlak k lajně pro **kontakt**.

### R3. Tabulka výkopu — 5 vadných výsledků z 11
Audit proti textu CRP (ř. 91–92) → `project_bloodbowl_kickoff_gaps_20260811`.
Pro proceduru podstatné:
* **Blitz! (10) fakticky není implementované** — CRP dává kopajícímu *volný
  bonusový tah*, u nás se každý posune o 1 pole. ⇒ **povinnost S0.6 je dnes
  neověřitelná**, protože ten výsledek nic nedělá.
* **High Kick (5)** má koučem *vybraný* hráč mimo TZ dojít na pole dopadu
  a **chytit** — u nás jde nejbližší a nechytá. ⇒ zahazujeme volné kolo.
* **Quick Snap (9)** je *volný pohyb libovolným směrem*, u nás sunutí ke
  středu LOS. ⇒ nedá se použít na míč ani na klec.
* ⇒ **22 % výkopů (High Kick + Quick Snap) dnes zahazuje volné tempo** —
  přímo ta veličina, které se v 0.4 nedostává.

### R4. O1 upřesněno — 2-1 grind nám v téhle podobě nevychází
Námitka uživatele: *„dám míč rychlému, ten skóruje, a já pak nemám dost kol
na vyrovnání — když mám plán stěží na 8 kol."* **Aritmeticky má pravdu:**

| | přijímáme první | kopeme první |
|---|---|---|
| H1 | grind 1.–8., TD → 1:0 | soupeř skóruje ~3., my dostaneme 5–6 kol ⇒ **nestačí** → 0:1 |
| H2 | soupeř přijímá, skóruje → 1:1, nám zbude zbytek | grind 9.–16., TD → **1:1** |
| výsledek | **1:1** | **1:1** |

⇒ **Pro tým, jehož plán potřebuje celou půli, se 2-1 grind degeneruje na
1-1.** Druhý TD nemůže přijít z druhého drivu — **musí přijít z KRÁDEŽE**
(turnover v obraně). To je přesně to, v čem jsme dnes nejlepší (odepření
70 %) a co dělá boxing-in.
⇒ **2-1 vyžaduje kladnou rezervu aspoň na jednom drivu.** Dokud je plán na
8 kol, je náš strop remíza — a při dnešním chess 0,46 (dw-sk) a 0,37 (dw-we)
by **spolehlivá remíza byla zlepšení**.
⇒ **Volbu mince netestovat teď.** Nejdřív zkrátit plán pod 8 kol (R1) —
teprve to vyrobí rezervu, ve které 2-1 vůbec dává smysl.

### R5b. ⭐ [9] VÝBĚR CÍLE VE ZDI — UZAVŘENO: je to VÝPOČET, ne doktrína
Rozhodnutí uživatele 11.08.: *„výběr cíle ve zdi — ten, co otevře největší
díru — to dopočítáš."* ⇒ Není to díra ve znalostech, je to **kritérium
k naprogramování.** Definice, aby byla jednoznačná:

**Díra se neměří v TĚLECH, ale v TACKLE ZÓNÁCH — a měří se PRŮCHODEM, ne
mezerou.** Sloupcová obrana proti nám stojí *2 pole od sebe a 2 do hloubky*
právě proto, aby sražení jednoho těla nic neotevřelo: druhá řada díru
pokryje TZ. Zdroj „How to beat Dwarfs" to říká přímo — *„blitznou prvního,
ale přes druhého se nedostanou."*

```
pro každý cíl T ∈ plan.resistance:
    stav' = stav bez T (sražený nepůsobí TZ; u pushe se T přesune)
    díra(T) = počet polí ve směru postupu, která jsou ve stavu'
              PRÁZDNÁ a BEZ soupeřovy TZ, a jsou spojitě dosažitelná
              z pozice nosiče  (pathfinder.cpp, bez dodge)
    hodnota(T) = díra(T) × P(sražení T)      ← ne samotná díra
vyber argmax hodnota(T)
```

**Tři upřesnění, bez kterých to bude špatně:**
1. **Díra musí být široká pro KLEC, ne pro nosiče.** Prahy: **3 pole** =
   nosič + dva rohy projdou · **5 polí** = projde celá čelní stěna klece.
   Jednopolová díra je past — nosič projde, klec zůstane vzadu (přesně vzor
   „vlastní těla si překážejí", 07.08.).
2. **Násobit pravděpodobností sražení.** Obrovská díra za jednokostkový blok
   je horší než skromná za tři kostky. Proto `díra × P`, ne `díra`.
3. **Push se počítá jinak než knockdown.** Sražený mizí z TZ na místě;
   odstrčený TZ **přesune** — někdy díru otevře, jindy ji jen posune
   o pole dál. Obojí se dá spočítat, ale nesmí se to zaměnit.

⚑ **Vstup už existuje a dnes se zahazuje:** `cage_advance.cpp` seznam těl
v koridoru **spočítá** a udělá z něj jediné číslo (postih k tempu). Tohle
kritérium je přesně to, k čemu ten seznam měl sloužit (viz X6).

### R5. Nová otevřená položka
**O8 — proč Longbeard vůbec bere míč?** Sbírá ho jako první, kdo je u něj?
Pak je oprava v **prioritizaci sběru**, ne v předávání (O2).

---

## ZDROJE (otevřený web, rešerše 11.08.2026)

* [Blood Bowl Tactics — Cage Basics](https://bbtactics.com/cage-basics/) — tvar klece, „žádný z pěti hráčů nekončí v TZ“, posun do strany
* [Blood Bowl Tactics — The 2-1 Grind](https://bbtactics.com/2-1-grind/) — skórovat v 8. a 16. kole, kopat první, „15 kol klece“
* [Blood Bowl Tactics — How to beat Dwarfs](https://bbtactics.com/how-to-beat-dwarfs/) — sloupce po dvou, „blitznou prvního, ale přes druhého se nedostanou“
* [Steam — The Good, the Bad, and the Ugly Habits of Blood Bowl](https://steamcommunity.com/sharedfiles/filedetails?id=827210711) — bezpečné akce první, nedodgovat nosičem, 1D bloky, surf, foul, stalling
* [Creative Twilight — 12 Blood Bowl Tips](https://creativetwilight.com/blood-bowl-tips/) — pravidlo dvou polí, sloupcová obrana, wide zone 2 pole vzad, „vždy čekej blitz“
* [grumbbl — A Lean, Mean, Screening Machine](https://grumbbl.co.uk/screening-in-blood-bowl/) — screen 2 pole od sebe, proč to zastaví blitz, proč pomalé týmy trpí
* [Exit 23 Games — Blood Bowl Defense 101](https://exit23.games/blogs/blood-bowl/defense-101) — obrana mlátícího týmu: plná šířka, síla ve středu, zeď z více těl
* [Un-Gri Games — Defending in Blood Bowl](https://www.un-grigames.com/blog-2-1/blog-post-title-one-7mhds) — formace, markování klíčových hráčů, svádění do středu (rozpor O3)
* [Blood Bowl Tactics — Strategy index](https://bbtactics.com/strategy/) · [Goonhammer Dwarf Team Guide](https://www.goonhammer.com/blood-bowl-dwarf-team-guide) *(nešlo stáhnout — obsah jen ze snippetu: „úspěch stojí na poziční hře“, max. dva MA6, 1:0 a 2:1 jsou nejlepší scénáře)*
* CRP 2016 lokálně: `rules_crp2016.txt` ř. 8–9 (rozestavení: ≥3 na LOS, ≤2 na wide zone), ř. 226 (Kick-Off Return, Kick)

---
---

# ČÁST 8 — KONSOLIDACE 11.08. (verze 2)

Všechno, co bylo 11.08. rozhodnuto a dosud leželo jen v paměti.
**Části 0–7 výše zůstávají v platnosti, kde je tato část nepřepisuje.**

---

## 8.1 SESTAVA JEDENÁCTKY — kdo co dělá

| role | kdo | proč |
|---|---|---|
| **nosič** | Runner #1 | MA6, Sure Hands, Block |
| **rohy, diagonála A** | 2× Longbeard **+Guard** | Guard na **protilehlých** — spolu pokryjí všechny čtyři strany, ze kterých lze udeřit |
| **rohy, diagonála B** | **Blitzer** (outlet) + Longbeard | Blitzer AV9/Guard/Tackle/AG3 = „AG4 s Guardem" v trpasličí verzi |
| **eskorta ×3** | 1× Blitzer, 2× Troll Slayer | **MA5** — musí dopředu odklidit **a stihnout zpět** |
| **hluboká pojistka** | **Runner #2** | vzadu, ball-hawk. **Nikdy vepředu.** |
| **volná těla ×2** | 2× Longbeard | markování, zavazování, faulování |

**Proč outlet a ne druhý Runner:** nosič bude na konci drivu odkrytý tak
jako tak (8.3), takže druhý Runner je **pojistka**. Kdyby stál v kleci,
byli by v jednom poločase vystavení oba — a tým bez obou Runnerů nemá nosiče.

**Proč rychlí nejsou v kleci:** kdo předbíhá a vrací se, musí být rychlý;
kdo se loudá s nosičem, může být pomalý. Longbeard MA4 klec nebrzdí,
protože klec jede tempem mletí.

---

## 8.2 KLEC NENÍ ZAHAJOVACÍ FORMACE
**První kola: BĚH.** Nosič na plné MA, **jeden** doprovod, screen za ním.
Klec v prvních kolech stojí pět aktivací a nechrání nic — nikdo tam není.

**Spouštěč přechodu na klec, o kolo NAPŘED:**
```
ohrožen ⇔ ∃ soupeř S: dist(S, nosič) ≤ S.MA + 2 + 1
                    ∧ vede k němu cesta BEZ našich tackle zón
```
**Klec se staví v kole, kdy se ohrožení objeví — ne když dorazí.**
⚠️ Gutter Runner dosáhne 12 polí, Wardancer 11 ⇒ **proti rychlým drží běh
jedině screen**, ne vzdálenost.

---

## 8.3 DRIVE KONČÍ SÓLOVÝM BĚHEM — a je to záměr
Nosič MA6, rohy MA4, klec jede 2–3 pole. **V posledních kolech musí nosič
urazit zbytek sám.** Není to chyba provedení, je to důsledek pomalosti.

⇒ **„Skóruj až v posledním kole" má DVA nezávislé důvody:**
1. **hodiny** — brzký TD daruje soupeři drive navíc
2. **bezpečnost** — sólový výběh **nelze potrestat, když po něm soupeř
   už nemá tah**

---

## 8.4 PROŽENÍ ZDÍ — vše v jednom tahu
**Průchod přes dvě kola neexistuje.** Buď se v jednom tahu podaří
**rozbít zeď + protáhnout celou pětici včetně nosiče**, nebo je tah
zaseknutý a řeší se jinak (doplnit, držet, mlátit).

```
jeden tah = blitz (+ bloky s asistencemi) na tělo/těla zdi
          + všechny čtyři rohy za díru
          + nosič za díru        v rámci 11 aktivací a JEDNOHO blitzu
```
**Feasibility se testuje na celém balíku najednou.** Poloviční verze se
nezkouší.

**⭐ GFI je pro průchod povolené** — výjimka z „tempo se nekupuje kostkami".
Rush je legitimní přesně tehdy, když je rozdílem mezi *projít* a *zůstat
zaseknutý*. Ne pro bankování, ne pro rutinní tempo.
⚠️ V **blizzardu** to neplatí (GFI padá na 1–2).

### Díra se neměří šířkou, ale CENOU PRŮCHODU V DODGÍCH
```
díra(T) = po odstranění T nejlevnější cesta pro JEDNOHO hráče,
          ocenĕná počtem dodgů (výstupů ze soupeřovy TZ),
          × kolik našich ji zvládne
```
| tým | tolerovaná cena | proč |
|---|---|---|
| **trpaslík** | **0 dodgů** | AG2/AG3 a **Dodge skill nemá nikdo** |
| elf, skaven | 1 dodge na hráče | AG4 + Dodge u všech procházejících |

**Pro nás je díra za jediný dodge bezcenná.** Tvrdý filtr, ne preference.
A **hodnotí se prostor ZA dírou** — díra, za kterou je další zeď, není díra.

⚠️ Sloupcová obrana stojí 2 pole od sebe **právě proto**, aby po odstranění
jednoho těla žádná bezdodgová cesta nevznikla. ⇒ **Jeden blitz díru
neotevře**; výběr cíle musí umět **kombinaci dvou těl**.

### Osamělé tělo se NEBLITZUJE
Jeden marker není zeď. **Posunout se dál a obejít ho**, a zavázat ho
linemanem — **postaveným MEZI něj a klec**, aby se nemohl vrátit.
Blitz si najde lepší cíl.
⚠️ **Obejít platí na jedno tělo, ne na zeď.** Proti rychlé zdi neplatí
vůbec — přeskupí se dřív, než klec doputuje do strany.

---

## 8.5 ZÁSADA POŘADÍ AKCÍ — přepsáno
**Řadit akce podle toho, CO PO SOBĚ ZANECHÁ SELHÁNÍ — ne podle toho, jak
je pravděpodobné.**

„Bezpečné akce první" byla jen **aproximace**, platná dokud jsou akce
nezávislé. Jakmile jedna akce **odemyká** ostatní, se pořadí obrací samo:
* blitz první, klec ještě celá ⇒ selhání zanechá **kompletní klec**
* klec první, blitz poslední ⇒ selhání zanechá klec **rozpracovanou**
  a nosiče odkrytého — táž pravděpodobnost, **mnohem dražší následek**

---

## 8.6 REZERVA < 0 — držet neznamená stát
**Pomalý bezpečný postup PRYČ od vlastní endzone.**
Logika je riziková, ne skórovací: turnover u vlastní endzone dá soupeři
krátké pole, turnover uprostřed stojí jen míč.

| # | co |
|---|---|
| 1 | udržet míč do konce půle |
| 2 | žádný hod, který plán nevyžaduje |
| 3 | sekundárně vzdálenost od **vlastní** endzone |
| 4 | zbylé aktivace na attrition |

**„Míň zběsile"** = zákaz toho, co v ROZVRHU povolujeme: GFI řetěz,
dodge nosičem, předání na rychlejšího.

---

## 8.7 TÝMOVÉ REROLLY (3 na půli, CRP dovoluje jeden za tah)
1. **sebrání míče** · 2. **pád nosiče** · 3. **blok, na němž stojí
proražení zdi** · 4. **nikdy:** push→knockdown, blok bez následku, GFI
hráče na kterém nic nezávisí.

---

## 8.8 KOLO PO TURNOVERU
1. **Přepočítat rezervu** — turnover typicky překlápí ROZVRH → NOUZE
2. **Míč BRÁT, ne dostavovat klec** — 80 % ztrát je sražení nosiče, míč
   leží uvnitř našich těl a máme všech 11 aktivací
3. míč leží ⇒ **S5** (sbírá Runner, i když je dál) · sebral ho soupeř ⇒ **S7**
⚠️ **Výjimka:** ležící nosič platí 3 pole za vstávání ⇒ nedosáhne-li,
sbírá ten, kdo **stojí**. Při volbě náhradního sběrače rozhoduje **AG,
pak MA**.

---

## 8.9 TABULKA VÝKOPU
| 2D6 | co děláme |
|---|---|
| 3 Riot | **přepočítat rezervu** — mění počet kol |
| 4 Perfect Defence | při výkopu **přestavět proti tomu, co vidíme** |
| **5 High Kick** | **vždy RUNNER** na pole dopadu a chytit (jediné omezení: nesmí být v soupeřově TZ) |
| **9 Quick Snap** | **1. Runner k míči · 2. rohy do klece · 3. zbytek vpřed** |
| **10 Blitz!** | příjem: rozestavení to musí přežít · výkop: **jít po MÍČI** |
| 11 / 12 Rock, Invasion | mění rozpočet těl na klec |

---

## 8.10 POČASÍ
| | dopad |
|---|---|
| **Sweltering Heat** | ~1,8 hráče **každý drive** ⇒ počítat s devíti, obětuje se **eskorta** |
| Very Sunny | netýká se nás |
| **Pouring Rain** | sbírá **jedině Runner** (75 % vs Longbeardových 33 %); nedosáhne-li ⇒ **odepřít** |
| **Blizzard** | **ruší povolené GFI pro průchod** |

Nice je 91,7 % ⇒ celá sekce se týká ~8 % drivů. Úplnost, ne páka.

---

## 8.11 FAULOVÁNÍ
Faul se 3 asistencemi proti AV8: **12 % casualty**, ale **~27 % vyloučení**.
Sražení blokem dá casualty ve 4,6 %.
⇒ Faul je ~2,6× účinnější na použití, ale **v jednom případě ze čtyř nás
stojí hráče**. **Výhodný jen jako výměna nahraditelného za nenahraditelné.**

1. **Faulovat POSLEDNÍ v tahu** (vyloučení = turnover)
2. **Faulovat KE KONCI DRIVU** — náhradník dorazí až na příští výkop,
   takže vyloučený chybí **celý zbytek drivu**
3. **Faulovat smí jen ten, koho lavička nahradí** — u nás obyčejný Longbeard
4. **Faulovat jen jejich nenahraditelného**

---

## 8.12 HRA V OSLABENÍ
| kolik nás je | co odpadne |
|---|---|
| 10 | jedno volné tělo |
| 9 | druhé volné tělo |
| 8 | eskorta ze 3 na 2 |
| 7 | **Runner #2 jde do hry** |

**Klec si drží ČTYŘI ROHY vždy.** Nikdy se nezmenšuje na tři.
⚠️ **Ale oslabení je z devíti desetin téma JEJICH:** soupeř končí v průměru
o **2,82** hráče dole, my o **0,81**.

---

## 8.13 APOTHECARY
**U trpaslíka na RUNNERA** — je klíčový.
Obecně: **nenahraditelný = nositel schopnosti, kterou nikdo jiný nemá
a na které stojí plán týmu.** (Orkský thrower má Sure Hands jako náš Runner,
ale ork na něm plán nestaví ⇒ nahraditelný.)
⚠️ **Aritmeticky se to odvodit nedá** — je to doktrinální vstup per tým.

V kódu platí obecné pravidlo: **stojí za apothecary, jen když v rezervách
nesedí identický spoluhráč.**

---

## 8.14 PO NAŠEM TD A O POLOČASE
**Nechat soupeře rychle skórovat, aby nám vrátil míč? PRO NÁS NIKDY.**
Potřebujeme sedm kol; míč se čtyřmi koly nás vrací rovnou do NOUZE.
⇒ **Obrana je vždy maximální odepření a SKÓRE JI NEMĚNÍ.**

**Poločas:** resetují se kola, rerolly na 3, KO hází na návrat 4+.
1. přepočítat rezervu od nuly · 2. spočítat těla · 3. přijímáme-li 2. půli,
je to **náš jediný plný osmikolový drive** a plán 1-0 stojí na něm.

---

## 8.15 ⭐ CÍL JE 1:0, NE 2:1
Trpaslík dá 2 TD ve **2 %** zápasů. Plán na dva touchdowny nemá smysl.
**Skórovat jednou z vlastního přijímacího drivu a zbytek ubránit.**

**Krádež je příležitost, ne položka rozvrhu:** početní převaha nepomůže
dohnat někoho rychlejšího. Attrition se vyplácí dvěma způsoby a ani jeden
není krádež — (1) míň jejich hráčů = míň jejich skóre, (2) míň jejich
hráčů = slabší odpor proti **našemu** drivu.

---

## 8.16 ⚑ TEST NA PŘEBÍRANÉ RADY
**Skoro každý klecový manévr v literatuře předpokládá, že se tým UMÍ
PŘESKUPIT. Trpaslík se přeskupit neumí — co nechá vzadu, to ztratil.**

| rada | proč u nás neplatí |
|---|---|
| přelévání klece (zadní rohy dopředu) | ergonomie u stolu, ne optimum |
| „posun do strany se počítá" | nemáme kola nazbyt, fail je fail |
| „zeď se dá obejít" | rychlý soupeř se přeskupí dřív |
| „klec slož z jiných hráčů" | kdo zůstane vzadu, je z drivu venku |
| „neboj se faulovat" | darovat hráče je pro nás dražší než pro kohokoli |

⇒ **U každé přebírané rady se ptát: kolik pohybu předpokládá, a máme ho?**

---

# ČÁST 9 — KONSOLIDACE 12.08. (verze 3): ALOKACE TĚL

> **Čím se tahle část liší od všeho výše.** Dosud katalog říkal, CO se má
> v které situaci stát. Neříkal, **KTERÉ TĚLO to udělá** — a když se v S2
> sešly klec, značkování a zbytek, rozhodovalo se od oka. 12.08. dodal
> uživatel čtyři pravidla s **pořadím**, takže je alokace poprvé rozhodnutelná
> a hlavně **zkontrolovatelná po jednotlivých hráčích**.

## 9.1 Hierarchie R1 > R3 > R2, plus zbytková R4

| | pravidlo | síla |
|---|---|---|
| **R1** | **klec (rohy) NESMÍ markovat** — roh jedině na pole s nulou TZ | nejvyšší, nepřebíjí se |
| R2 | nikdo nemarkuje / zůstat volný | **nízké** |
| **R3** | **všichni mimo klec MAJÍ markovat** | přebíjí R2, **nikdy R1** |
| **R4** | **kdo nemá úkol, jde blíž k akci** | zbytková, až když R1 i R3 nejdou |

## 9.2 ⭐ Kdo se počítá do R1 — a osa, ze které to plyne

> **R1 hlídá jen údery, které soupeře NESTOJÍ BLITZ.**

| stav soupeře | do R1 | proč |
|---|---|---|
| stojící | ANO | Block Action ho nestojí pohyb ani blitz |
| ležící s **Jump Up** | ANO | smí Block vyhlásit vleže (AG +2), vstane zadarmo ⇒ blitz jim zůstane |
| ležící bez Jump Up | ne | vstát + dojít = celá aktivace, blok letos nebude |
| omráčený | ne | příští kolo se teprve obrací lícem nahoru |

Tohle je **obecná formulace**, ne „koukni, kdo stojí" — platí i na rostery,
které jsme nikdy nehráli. Jump Up mají jen `roster.cpp` ř. 168/198/395;
ani jeden není dwarf, skaven ani wood elf ⇒ prakticky R1 = „stojící".

## 9.3 Koho značkovat (upřesnění S7 boxing-in) — KRITÉRIUM, ne seznam ras
⚠️ **Přeformulováno 12.08. večer.** Znění *„jen AG3"* bylo odvozené proti
skavenům a **na wood elfy se nepřenáší**: ti jsou skoro celí AG4 (11 Linemanů,
2 Wardanceři, 2 Catcheři, Thrower — **ani jeden AG3**), takže „jen AG3" by
znamenalo **neznačkovat nikoho**.

**Kritérium: značkuj tam, kde dodge NĚCO STOJÍ** — funkce tří vstupů:

| vstup | efekt |
|---|---|
| **AG** | cíl hodu `7 − AG`, **+1 za samotný dodge** |
| **naše TZ na CÍLOVÉM poli** | **−1 za každou** — jediná páka, kterou řídíme my |
| **Dodge** | reroll při selhání — **náš Tackle ho ruší** |

Odchod do čistého pole: AG4 **2+ (83 %)** · AG3 3+ (67 %) · AG2 4+ (50 %).
S jednou naší zónou navíc na cílovém poli: AG4 3+ · AG3 4+. Se dvěma: AG4 4+ ·
AG3 5+.
⇒ **Osamocená značka na AG4 nedrží skoro nic.** Boxing-in není „stoupni si
vedle něj", ale **„uber mu čistá CÍLOVÁ pole"**.

⚠️ **DRUHÁ OPRAVA (12.08. večer): „Gutter Runner uteče v ~97 %" platí jen
u markera BEZ Tackle.** Tackle ruší Dodge při odchodu **z tackle zóny toho
hráče** — a Tackle mají Longbeardi, Blitzeři i Slayeři, **nemají ho jen
Runneři (9 z 11 ho má)**. Označkovaný AG4 tedy hází **bez rerollu**, ať je to
elf nebo Gutter Runner: **16,7 % selhání za pokus** u obou.

### ⭐ Proti hbitým rasám se hraje na POČET VYNUCENÝCH HODŮ, ne na sražení
Selhaný dodge v jejich kole **není sražený hráč — je to TURNOVER a konec
jejich tahu.**

| kolik jich donutíme hodit | aspoň jeden selže |
|---|---|
| 1 | 16,7 % |
| 2 | 30,6 % |
| **3** | **42,1 %** |
| 4 | 51,8 % |
| 5 | 59,8 % |
| 6 | 66,5 % |

Při třech označkovaných je to skoro coin flip, že jim kolo skončí předčasně.
S pokrytým cílovým polem je každý hod **33,3 %** místo 16,7 % ⇒ tři markeři
dají **70 %**.
**Výhrada:** hází jen ten, kdo **chce odejít**. Marker na hráči, který nikam
nemusí, nevyrobí nic — u elfů a skavenů ale musí obíhat skoro všichni.

### Dvě meze, které to drží při zemi (uživatel 12.08.)
| # | pravidlo |
|---|---|
| **S7.10 [P]** | **Za Gutter Runnerem se NEHONÍ.** Jediná výjimka je **příležitostný otevřený blitz**, když se sám nabídne — a i ten je až za blitzem do zdi kupředu (ČÁST 9.5). |
| **S7.11 [P]** | ⭐ **Značka nesmí soupeři dát levnou příležitost přijít s asistencí a udeřit na dvě kostky.** Shoduje se s **E2** změřeným týž den (ČÁST 13.2): povolený bezplatný ≥2kostkový blok ≈ **+0,2–0,35 sraženého na kolo**, mez `FB2 ≤ 1`. |

⇒ Ty dvě povinnosti se **nevylučují** s „markuj hodně": měření ukázalo, že
škodí **kostky, ne kontakt sám** (prostý počet dotyků predikuje hůř).
**Markuj široce, ale žádného markera nenech stát tak, aby na něj levně
postavili dvě kostky.**

## 9.4 Cíl R4 se odvozuje z ROLE, ne z pevného bodu
Uživatel: *„blíž k akci nemusí být nutně vždy k míči — může pomoct např. při
stavění zdi."*

| role | kam ho R4 posílá |
|---|---|
| Longbeard / Blitzer / Troll Slayer (AV9-8, Guard, Block) | **zeď nebo roh klece** — nárazová těla |
| Runner (Sure Hands, AV8, jediný náhradní nosič) | **za klec, na dosah míče** — do zdi nepatří |

**⭐ R4 se hraje PRVNÍ v pořadí kola** (uživatel 12.08.): *„to je hráč, co se má
pohnout první bez rizika a ne čekat na vlastní TD zóně — jestli bude turnover,
nebo se na něj zase zapomene."* Doplňuje to zásadu „bezpečné první, rizikové
nakonec" o **důvod**: turnover ukončí kolo, takže o akci přijdou ti, kdo ještě
nehráli. Hráč, jehož tah je čistý zisk s nulovým rizikem, je přesně ten, kdo se
při jakémkoli zaváhání ztratí.

**Kritérium pořadí je ale ZÁVISLOST, ne riziko:**
> **Napřed volné tahy, jejichž hodnota NEZÁVISÍ na zbytku kola.** Volný tah,
> který na něm závisí — třeba obsazení rohu klece — se **odkládá**, dokud není
> jasné, kde klec skončí.

Obecná úvaha *„akce bez rizika odlož, informace se zlepší"* tedy u zapomenutého
Runnera neplatí: je dvě kola od použitelnosti, takže žádný blok ani posun klece
jeho cíl nezmění. **Empiricky `K31` = 1,80 těla na kolo bez úkolu** *(přepočteno
13.08.; dřív se uvádělo 0,86 — čitatel se počítal jen z kol s míčem, jmenovatel
bral všechna kola).*

## 9.5 Priorita blitzu (doplňuje S2 i S7)
1. **Blitz do zdi kupředu** — vždy nejvyšší
2. **Odmarkování nosiče**, je-li označkovaný — nosič s AG2 platí za odchod ze
   zóny 4+, tedy **50 % turnover**; blitz je levnější než ten hod
3. Příležitostný blitz na Gutter Runnera — **jen když se nabídne** a nic výše
   nečeká

## 9.6 Záloha u míče musí být potenciální NOSIČ
Uživatel: *„jako pomocník vedle míče měl přijít Blitzer — ať když Runner
nezvedne, je on s AG3 a MA5 potenciální nosič."*
Doplňuje **S0** a bod O8: dnes se záloha vybírá podle vzdálenosti, ne podle
toho, jestli ten hráč umí míč donést. ⇒ zařazeno jako **P0** ve
`fix_queue_20260812.md`.

## 9.7 Nová KONTROLA
**`K-noblock`: kolo bez jediného bloku je VAROVNÝ SIGNÁL, ne neutrální
výsledek.** Proti rase, kterou porážíme mlácením, je kolo zdarma pro soupeře
drahé. Změřeno na živé pozici: blitz padl dřív, bezpečný blok neexistoval,
a tah nevyrobil žádnou attrition.

## 9.8 ⭐ CO TAHLE ČÁST ZAKRYLA A CO NE — účetnictví pokrytí

**Zakryto (S2 rozvrh, S7 boxing-in):** alokace těl je poprvé úplná — na
měřené pozici spadlo **všech 11 hráčů** pod některé pravidlo a žádná dvě
si neodporovala, protože mají pořadí. To je ten kvalitativní skok.

**NEZAKRYTO — sem míří příští review:**
* **S5 volný míč, dosáhneme** — pořád největší měřená díra (96 % dosažitelnost, **53 % pokus**). R1–R4 se jí vůbec nedotkly.
* **S0 / S1 rozestavení** — 9.6 přidala jedno pravidlo, zbytek slotů je pořád podle pořadí v rosteru.
* **S4 nouze**, **S8 zabránit skóre**, **S9 práh**, **S10 skóruj teď** — dnešek se jich netkl vůbec.
* **Rozpočet pro R3** — dnes se R3 splnila **náhodou** (dvě zaseknutá těla shodou okolností stála u AG3 soupeřů). R1 spolyká všechna pohyblivá těla; kdyby ta dvě stála jinde, R3 by neměla čím. Pravidlo zatím **nemá vlastní rozpočet**.

---

# ČÁST 10 — BIG GUY SOUPEŘE (12.08.)

> ⭐ **Osa celé části: cenu Big Guye určuje INICIATIVA.**
> **Big Guy, který přijde k nám, je CÍL. Big Guy, ke kterému jdeme my, je NÁKLAD.**
> Je to zrcadlo strategie, kterou soupeři hrají proti nám: *potrestat toho,
> kdo vyleze z řady.*

## 10.1 Proč se to musí vážit, a ne jen zakázat

**Nechat ho být není zadarmo.** Ogre ST5 s Mighty Blow blitzuje náš Longbeard
(AV9, Thick Skull), bez asistencí ⇒ dvě kostky vybírá **on**:

| | |
|---|---|
| srazí kohokoliv | **55,6 % za kolo** |
| prolomí AV | 15,4 % |
| **odstraní nám hráče** | **4,3 %** |
| Bone-head mu kolo sebere | 16,7 % |

⇒ Reálná škoda je **tvarová, ne attriční**: jednou za dvě kola nám otevře
formaci. Ale **je to JEHO blitz** — utratil by ho tak jako tak, takže
marginální cena je *„vzal si naše tělo místo našeho nosiče"*, což je pro nás
často **lepší** varianta.

## 10.2 Náš blok na něj — cena je funkcí asistencí, ne kostek

Troll Slayer proti ST5 (Ogre má u nás **Block**, takže Both Down je nula):

| asistence | bez Dauntless | s Dauntless |
|---|---|---|
| 0 | 11,1 % / turnover 30,6 % | 25,9 % / 21,3 % |
| 1 | 11,1 % / 30,6 % | 40,7 % / 12,0 % |
| 2 | 33,3 % / 16,7 % | 48,1 % / 7,4 % |
| **3** | **55,6 % / 2,8 %** | **55,6 % / 2,8 %** |

⭐ **Při třech asistencích je Dauntless zbytečný.** Jeho role je **záchranná
síť**, když tři asistence nesehnáme (při jedné dělá 11 % → 41 % a sráží
turnover z 31 % na 12 %).
⭐ **Náš skutečný nástroj proti ST5 je GUARD — má ho 6 z 11** (oba Longbeardy
s Guard, oba Blitzery, oba Slayery) a **asistuje i když je náš hráč sám
markovaný**. Tam, kde jiné týmy asistence ztrácejí, my je držíme.

## 10.3 POVINNOSTI

| # | povinnost |
|---|---|
| **S-BG.1 [P]** | **Na Big Guye se NECHODÍ. Čeká se, až přijde.** Když blitzne, stojí uprostřed našich těl ⇒ asistence máme zadarmo a blok je levný. Jdeme-li k němu my, platíme za asistence pohybem a rozbíjíme tvar. |
| **S-BG.2 [P]** | **Proti ST5 sháněj ASISTENCE dřív než kostky — cíl 3.** Rozdíl 0→3 je 26 % → 56 % sražení a **21 % → 2,8 % turnover**. |
| **S-BG.3 [P]** | **Srazit stačí, odstraňovat netřeba.** Ležící Ogre nemá tackle zónu, vstávání ho stojí 3 z MA5, Bone-head mu kolo ze šesti sebere stejně. Zisk je v **TEMPU**, patří k S2.9 „prorazit zeď". |
| **S-BG.4 [S]** | **Treemanovi se UTÍKÁ.** MA2 + Take Root ⇒ nedohoní nikoho. Nikdy s ním nenavazovat kontakt, obejít. |
| **S-BG.5 [S]** | **Rat Ogre: jediná páka je potrestat expozici.** Wild Animal ho nutí buď se schovat, nebo se odvážit. **Odváží-li se blitznout, sesypat se na něj** — a on je AV8 **bez Thick Skull**, takže faul na něj funguje (24,3 % odstranění při 2 asistencích proti 6,9 % u Ogra). |

## 10.4 ZÁKAZY

| # | zákaz | proč |
|---|---|---|
| **Z15** | **nefaulovat cíle s Thick Skull**, dokud nemáme Mighty Blow nebo Dirty Player | AV9+Thick Skull: 6,9 % že ho odstraníme proti **22,2 %** že přijdeme o faulujícího **a o zbytek kola** ⇒ třikrát pravděpodobněji prodělek |
| **Z16** | **nechodit k Big Guyovi kvůli attrition** | viz S-BG.1; jde se k němu jedině, když stojí v koridoru (`plan.resistance`) |

## 10.5 Žebříček — koho SRAZIT ≠ koho FAULOVAT

**Srazit** se má ten, kdo překáží. **Faulovat** ten, jehož armour povolí:

| cíl | AV | pryč z drivu (2 asistence) |
|---|---|---|
| Ogre *(Thick Skull)* | 9 | **6,9 %** |
| Lineman, Blitzer | 8 | 24,3 % |
| **Gutter Runner · Catcher · Wardancer** | **7** | **30,1 %** |

⇒ *„Nejsilnější"* je systematicky *nejhůř odstranitelný* — síla koreluje s AV
a Thick Skull. **Gutter Runner je 4,4× lepší cíl faulu než Ogre**, a je to
navíc ten, kdo jim skóruje.

## 10.6 Kde to vůbec platí + jedna ironie

Big Guye mají v TV1200 **jen Human (Ogre) a Wood Elf (Treeman)**. Dwarf,
Skaven ani Orc žádného nemají ⇒ tahle část je dnes živá jen ve dvou dvojicích.

⭐ **Trpaslík Big Guye v TV1200 NEMÁ VŮBEC.** Generický `"Dwarf"` roster
Deathrollera má (ST7, AV10, Juggernaut, **Mighty Blow**, **Dirty Player**,
Stand Firm), ale je to **Secret Weapon** — po drivu odchází.
⇒ **Ten nepoužitelný Big Guy nese přesně ty dvě dovednosti, jejichž absence
dělá naše fauly prodělečné.** Není to špatný výběr cíle, je to chybějící
nářadí. Proto je Troll Slayer s Dauntless jediná odpověď na ST5, kterou máme.

## 10.7 ⚠️ Cena, kterou jsme oba přehlédli: Slayer je AV8

`[web]` *„They have average AV8 compared to AV9 like most teammates, which
makes them **a target** for opponents to hit, and their damage-causing
abilities make this even more desirable for the opposing team."*

**Troll Slayer je jediný náš hráč s AV8 mezi samými AV9.** Posílat ho na Ogra
znamená dát Ogrovi s Mighty Blow měkčí cíl:

| koho Ogre srazí a rozbije | prolomí AV | odstraní |
|---|---|---|
| Longbeard AV9 | 15,4 % | 4,3 % |
| **Troll Slayer AV8** | 23,2 % | **6,4 %** |

⇒ **Naše jediná odpověď na ST5 je zároveň naše nejkřehčí přední tělo.**

| # | povinnost |
|---|---|
| **S-BG.6 [P]** | **Slayer se k Big Guyovi nepřibližuje dřív, než kolem něj stojí asistence.** Nechat Big Guye přijít neznamená jen ušetřit aktivace — znamená to **nevystavovat Slayera dobrovolně**. V kontaktu má být jen tak dlouho, jak musí. |

## 10.8 Zdroje `[web]` (rešerše 12.08.) — potvrzení, ne inspirace

Doktrína 10.1–10.6 vznikla **z výpočtu**, web se dohledával až potom.
Shoduje se, a na jednom místě doslova:

* *„Failing Dauntless can leave you doing a block at a disadvantage, so it pays
  to cover this by **having assists to provide a one die block if you fail**."*
  — [bbtactics: Dwarf Troll Slayers](https://bbtactics.com/dwarf-troll-slayers/)
  ⇒ nezávislé potvrzení **S-BG.2**: dvě asistence nekupují lepší kostky při
  úspěchu, kupují **přepsání větve selhání** (ST3+2 = 5 proti 5 je jedna
  kostka místo dvou pro soupeře).
* *„For a bashing team like Dwarves that has **no ST4 or ST5 players**, Troll
  Slayers make up for this with their Dauntless ability."* — tamtéž.
  ⇒ „jediná odpověď na ST5" je charakteristika rasy, ne nouzovka.
* *„When a big guy is tying up players or occupying a key position, it's often
  best to **just leave them**"* · *„disengage from the Ogres, hit the
  [low-armour supporters] instead"* · *„wait for Bonehead rolls"*
  — [bbtactics: big guy](https://bbtactics.com/tag/big-guy/),
  [Ogres!! how to beat them](https://bbtactics.com/forum/threads/ogres-how-to-beat-them-if-possible.14483/)
  ⇒ potvrzuje **S-BG.1**, **Z16** a žebříček z 10.5.
* [Goonhammer: Dwarf Team Guide](https://www.goonhammer.com/blood-bowl-dwarf-team-guide) ·
  [bbtactics: Human Ogres](https://bbtactics.com/human-ogres/)

**Nikde se nedoporučuje faulovat big guye** — cílem má být nízkoarmourový
doprovod. Sedí to na Z15 i na čísla z 10.5.


---

# ČÁST 11 — PRVNÍ MĚŘENÍ KONTROL (12.08.)

Nástroj `diag_rules_checks_20260812.py`, korpus `diag_replay_mine_20260811b_data`
(120 her), **1761 našich kol**, 104 kol vyřazeno kvůli TD/poločasu mezi snímky.

⚠️ **Je to BASELINE, ne hodnocení.** Korpus vznikl **před** opravou klece
`c085331` a s **vypnutou bránou** `cageAdvance`. Slouží k pozdějšímu srovnání.

⚠️⚠️ **ČÍSLA PŘEPOČÍTÁNA 13.08.** Audit měřicího aparátu našel, že šest
kontrol měřilo něco jiného, než čím se hlásily: kontrola vydávala jediné
procento **bez jmenovatele** a **predikát nad prázdnou množinou se počítal
jako splněný**. Původní hodnoty jsou v závorkách a **neplatí**. Oprava
`dd295e5` (povinná trojice `Check(ok, n, deg)` + `N/A`).

| kontrola | výsledek | *(bylo)* |
|---|---|---|
| **K29 (R1)** — žádný roh není markovaný | **79,4 %** (n = 680) | *(86,3 %)* |
| **K29** — plná **ČISTÁ** klec 4/4 | **14,0 %** (n = 680) | *(9,3 %)* |
| kol s míčem, kdy klec **vůbec nestojí** | **341 z 1021** — dřív se počítala jako „čistá" | — |
| obsazených rohů průměrně *(jen když klec stojí)* | **2,22 ze 4** (z toho čistých 1,97) | *(1,48 / 1,31)* |
| **K9a** — rozvrhová podlaha splněna | **20,6 %** (n = 671) | *(32,5 %)* |
| průměrný schodek proti podlaze | **−3,19 pole/kolo** | *(−2,27)* |
| **K30 (R3)** — držení, kde **dodge něco stojí** | **24,8 %** (n = 8669) | *(23,3 % AG3)* |
| **K30b** — držení, kde dodge **nestojí nic** | **24,1 %** (n = 3639) | — |
| **K31 (R4)** — těla bez úkolu | **1,80 na kolo**; jen 40,8 % kol je bez | *(0,86)* |
| **K33** — kolo s aspoň jedním blokem | **76,1 %** | *(23,9 % bez)* |
| plán enginu | **`NOT_CONSULTED` ve 100 % kol** | — |

## Co z toho plyne

**⭐ 11.1 Prázdný roh je mnohem větší díra než markovaný.** R1 platí, ale dnes
**není co špinit**: ve **třetině kol s míčem klec vůbec nestojí** (341 z 1021),
a kde stojí, jsou obsazené 2,22 rohu ze čtyř. Upřesňuje to nález z 11.08. —
číslo „markovaných je 11 % obsazených rohů" jsme četli jako údaj o *kvalitě*
klece; je to údaj o tom, že **klec skoro není**.
⇒ Pořadí oprav: **nejdřív rohy obsadit** (cage-fill), teprve pak řešit čistotu.
⇒ A přesně tuhle třetinu si stará kontrola připisovala k dobru jako „čistou".

**11.2 K9a je tempo v jednom čísle.** Podlahu — absolutní minimum, pod kterým
se nedoskóruje — plníme v **pětině kol** a jsme v průměru **3,19 pole POD ní**.
Ne pod ideálem, pod podlahou. *(Původní −2,27 vzniklo tím, že se požadavek
počítal ze vzdálenosti PO pohybu, tedy z toho, kam jsme došli.)*

**11.3 R3 a R4 nejsou okrajová pravidla.** **1,80 těla z jedenácti** na kolo
bez úkolu; jen 40,8 % kol nemá ani jedno. R4 byla navržena jako zbytková
povinnost pro jeden zapomenutý případ — týká se prakticky každého kola.

**⭐⭐ 11.3b ZNAČKOVÁNÍ JE SLEPÉ K CENĚ DODGE — nový nález 13.08.** Držíme cíle,
kterým dodge něco stojí, ve **24,8 %**, a cíle, kterým nestojí nic, ve
**24,1 %**. Rozdíl **0,7 pp**: R3 se v praxi neuplatňuje vůbec, značkujeme
naslepo. Stará kontrola to vidět nemohla — měřila `AG3`, což je predikát,
který proti wood-elfům (celý tým AG4) nevybere nikoho.
**Cena je vlastnost DVOJICE, ne soupeře:** náš `Tackle` ruší soupeři `Dodge`
reroll a má ho **deset z jedenácti** (všichni krom Runnera), takže tentýž
elf AG4+Dodge stojí 2,8 % proti Runnerovi a 16,7 % proti Longbeardovi.
⇒ **Povinnost do repertoáru:** kdo značkuje, není jedno — Runner je na
značkování nejhorší hráč v týmu a je to zároveň jediný, kdo má nést míč.

**⭐ 11.4 T2 a T3 nejsou nezávislé — nový nález.** `NOT_CONSULTED` ve **100 %**
kol znamená, že plánovač se nezeptal ani jednou, takže `resistance` je všude 0
a **K9b se na existujících datech spočítat NEDÁ**. Není to díra v logování
(X6 přistálo), je to **vypnutá brána klece**.
⇒ **Dokud se brána nezapne, část rozhodčího nemá co měřit.** Plán „T2 celé,
pak T3" tímhle padá; K9b se musí měřit až po T3.1.

---

# ČÁST 12 — ROZLOŽENÍ SITUACÍ A SMRT ČÍSLA „53 %" (12.08.)

Fable scan, `diag_situation_scan_20260812.py`, report
`evidence/situation_scan_20260812.md`. **280 her / 4 488 trpasličích kol.**

## 12.1 Rozložení S0–S10

| situace | podíl kol | |
|---|---|---|
| **S7 boxing-in** | **32,4 %** | ⭐ nejčastější — je to OBRANA |
| **S4 nouze** | **27,7 %** | ⚠️ a je to **jediná trpasličí učební úloha (O6), NEROZHODNUTÁ** |
| S5 volný míč, dosáhneme | 15,6 % | |
| S8 zabránit skóre | 8,0 % | |
| **S2 ROZVRH** | **6,8 %** | ⚠️ spec o něm říká *„situace, ve které se hraje většina trpasličích kol"* |
| S3 opce | 5,3 % | |
| S10 · S9 · S6 | 1,9 · 1,1 · 0,9 % | |
| **mimo strom** | **0,2 %** (8 kol) | ✅ **katalog díru NEMÁ** |

⭐ **S2 dostalo 14 povinností a je to 6,8 % kol. S7 jich má 9 a je to 32,4 %.**
Detail procedury sedí na nejřidší situaci.

⚠️ **ALE: hranice S2/S3/S4 NENÍ z logu rozhodnutelná.** Stojí na `paceAch`,
který se loguje jako **0.0** — protože plánovač se nezeptal (viz ČÁST 11:
`NOT_CONSULTED` ve 100 % kol). Podle volby konstanty se **S2 hýbe mezi 6 a 354
koly**. ⇒ **Robustní je jen S7 (32,4 %)**; podíly S2/S3/S4 jsou orientační,
dokud se nezapne brána klece. **K28 se bez X6 nedá přibít.**
S0/S1 se nesnímkují vůbec.

## 12.2 ⛔ Číslo „96 % dosažitelnost / 53 % pokus" JE MRTVÉ

Pocházelo z korpusu **20260730**, `n = 53 kol`, engine o **šest oprav** starší
(reprodukováno: 96 % / 51 %). **Na dnešních korpusech je pokus o sběr v 89 %
kol S5.** Zbylých 77 kol:

| | |
|---|---|
| turnover skončil kolo dřív, než na sběr došlo | **42** — je to vada **pořadí rizika**, ne S5 |
| sběr vyžadoval dodge, neber je obhajitelné (S5.5) | **24** |
| **čistá vada volby** | **19 kol = 2,7 % S5** |

**Verdikt volba vs. generátor: generátor je v pořádku.** `macro_actions.cpp`
(ř. 551–635) emituje PICKUP pro všechny volné hráče s dosahem ≤ MA+2 — tatáž
podmínka jako definice S5; vrstva „nedosažitelný po cestě" je prázdná.
⇒ **Předregistrovaná varianta „S5 patří do generátoru" NENASTALA.**
*(Dostupnost je rekonstrukce z pozic — X5 se neloguje. Přiznáno.)*

## 12.3 Nové největší díry S5 — poprvé změřené

| povinnost | plnění |
|---|---|
| **S5.3** záloha u míče | **22,2 %** |
| **S5.4** nosič po sebrání krytý před blitzem | **28,8 %** *(Chebyshev, dolní odhad)* |

Po zániku „53 %" jsou tohle **nové největší díry S5** — a jsou to obě
povinnosti o **zajištění**, ne o sebrání. Sbírat už umíme; **pojistit sběr ne.**

## 12.4 Co to dělá s prioritami

1. **S7 je nejčastější situace a je to obrana.** Naše pozornost šla celý týden
   do útoku.
2. **S4 je druhá nejčastější — a její doktrína (O6) je NEROZHODNUTÁ.** Přestává
   to být „jediná učební úloha na okraji" a stává se z toho **díra uprostřed**.
3. **Odehraná situace mířená na S5 ztratila důvod.** Cíl se přesouvá na
   **S5.3/S5.4** (zajištění) nebo rovnou na **S7**.
4. **Dvě nezávislá měření dnes ukázala na týž kořen:** vypnutá brána klece
   znemožňuje jak K9b (ČÁST 11), tak hranici S2/S3/S4 (ČÁST 12).

---

# ČÁST 13 — BILANCE SOUPEŘOVA KOLA (12.08.)

**Doplňuje díru, kterou našel audit (N6): všech jedenáct situací popisovalo
NAŠE kolo. Tohle je první povinnost tvaru „na konci našeho kola smí mít
soupeř nejvýš X".**

Fable scan, `diag_exposure_scan_20260812.py`, report
`evidence/exposure_scan_20260812.md`. 1603 vzorků, 320 kol vyloučeno kvůli
TD/poločasu. Konec kola z `turn_logs[i+1]`, výsledek soupeřova kola z diffu
`[i+1] → [i+2]` po id. **Predikce, ne kauzalita** — a jeden korpus.

## 13.1 ⭐ REACH0 — kolik jich dosáhne na nosiče BEZ JEDINÉHO DODGE

Nejsilnější prediktor ztráty míče (r = 0,34, uvnitř každé rasy 0,31–0,35):

| REACH0 | ztratíme míč v jejich kole |
|---|---|
| **0** | **1,8 %** |
| 1 | 8,3 % |
| 2–3 | 22 % |
| 4+ | 33 % |

⭐ **Koleno je ostře na NULE.** Není to plynulá škála — mezi „nikdo" a „jeden"
je čtyřapůlnásobek. Rozhoduje **bez dodge**: prostá dosažitelnost bez toho
rozlišení predikuje hůř a byla zahozena.

**BLZ** (nejlepší kostky jejich blitzu na nosiče) přidává nezávisle:
1 kostka → 9 % · 2 kostky → 23 % · **3 kostky → 69 %**. Kombinace
REACH0 ≥ 3 **a** dvoukostkový blitz = **40 %**.

## 13.2 FB2 — bezplatné bloky se dvěma a víc kostkami

Předpovídá sražené (r = 0,40): **FB2 ≤ 1 → 0,41 sraženého/kolo · FB2 ≥ 2 →
1,00.** Každý povolený 2kostkový blok zdarma ≈ **+0,2–0,35 sraženého**.

⭐ **Škodí KOSTKY, ne kontakt sám.** Prostý počet dotyků (MARKED) je slabší
prediktor v každém sloupci. ⇒ **Mez nekoliduje s boxing-in doktrínou** —
můžeme dál stát vedle nich, jen jim nesmíme nechat výhodné kostky zdarma.

### ⚠️ VÝHRADA K E2 — a jak ji zavřít (uživatel 12.08.)
**Tak, jak `FB2` scan počítal, je to DOLNÍ ODHAD.** Počítá asistence z míst,
kde jejich hráči stojí **teď** — jenže se **nejdřív pohnou a teprve pak
udeří**. Práh `FB2 ≤ 1` tedy stojí na čísle, které nebezpečí podceňuje.
*(Táž třída chyby jako u K9: práh vypadá tvrdě, ale měří míň, než tvrdí.)*

**Uživatelova definice, která to zavírá:** *„kdo může přijít na pomoc zadarmo
zjistíme taky — kdokoliv nemarkovaný, kdo dojde bez rizika hodu kostkami."*

⇒ **Operativní definice `FB2`:**
1. **Blokující** = jejich stojící hráč **už sousedící** s naším *(Block Action
   je bez pohybu; kdo musí dojít, utrácí BLITZ — a ten mají jen jeden)*
2. **Asistence** = kdokoli jejich, kdo je **nemarkovaný** (odchod ho nestojí
   dodge) a **dojde k našemu hráči V DOSAHU MA** — tedy **bez GFI a bez
   jediného hodu**. Ne MA+2; GFI je hod a ten se nepočítá.
3. `FB2` = počet takových bloků, které vyjdou na **≥ 2 kostky**

Tím se z dolního odhadu stane skutečná míra: zahrnuje to přesně ty pomocníky,
kterých se soupeř může dovolat **zdarma**, a vylučuje ty, za které by musel
hodit. Cesta se počítá týmž BFS jako dosah bez dodge (`diag_play_session_20260812.py`).

⇒ **Takhle implementovat K35.** Dnešní scan zůstává platný jako dolní odhad
a jako baseline.

### ⭐ JE TO JEDEN VÝPOČET, OBĚMA SMĚRY (uživatel 12.08.)
*„Stejně jako spočítáme naše volby pro asistenty, tak spočítáme i jejich."*

**Volní asistenti** = nemarkovaní hráči dané strany, kteří dojdou k cílovému
poli **v dosahu MA**, bez dodge a bez GFI. Táž funkce, jen jiná strana:

| směr | k čemu slouží |
|---|---|
| **naše** | **S-BG.2** — proti ST5 sháněj asistence dřív než kostky, cíl 3 (rozdíl 0→3 je 26 % → 56 % sražení a **21 % → 2,8 % turnover**) |
| **jejich** | **E2 / `FB2`** — kolik ≥2kostkových bloků zdarma jim deska dovolí |

⇒ **Neimplementovat dvě metriky, ale JEDNU primitivu** `volniAsistenti(strana,
pole)` a obě povinnosti z ní odvodit. Je to zásada „implementovat PRAVIDLO,
ne jeho výsledek": okrajové případy (Guard, markovaný asistent, cesta zónou)
pak vypadnou zadarmo na obou stranách naráz.

## 13.3 Obrana před chain pushem

Řetěz vznikne **jedině tehdy, když jsou všechna tři pole odsunu obsazená**
(CRP: *„must be pushed back into an empty square if possible"*). ⇒ Obrana je
prostá a je celá v našich rukou:

| # | povinnost |
|---|---|
| **E3 [P]** | **Za každým naším tělem, které je (nebo příští kolo bude) v kontaktu, musí zůstat aspoň JEDNO ze tří polí odsunu VOLNÉ.** Pak odsun musí jít tam a řetěz nevznikne. |

**Proč to patří sem:** je to další věc, kterou naše pozice **dává soupeři** —
a na rozdíl od E1/E2 ji dostane **úplně zadarmo**, jedním blokem bez hodu
navíc. Vlastními těly mu vyrobíme páku přímo na nosiče.

**Příklad z živé pozice (12.08.), kvůli kterému to vzniklo:**
Nosič stojí na (12,8), čelní tělo **DLG10 na (13,8)**, a **SBZ18 leží na
(14,8)** — až vstane, může DLG10 blokovat přímo na západ. Tři pole odsunu jsou
`(12,8)` *(náš nosič)*, `(12,7)` a `(12,9)`. Dokud jsou ty dvě **prázdné**,
odsun musí tam a je to neškodné. **Kdybychom je obsadili, blok na DLG10 by
nám vlastním tělem vystrčil nosiče z klece** — bez jediného hodu.
⇒ Kontrola potvrdila **0 řetězových rizik** v té pozici, ale jen podmíněně:
bezpečnost stojí na tom, že ta dvě pole zůstanou volná.

### ⭐ E4 — EskORTA JE PŘESNĚ ČTYŘI ROHY. Víc těl u nosiče = chain push.
**Uživatel 12.08.: *„našich má být 4 — 4 rohy — víc je vždy chainpush."***

| # | povinnost |
|---|---|
| **E4 [P]** | **U nosiče stojí PŘESNĚ čtyři těla, a to na DIAGONÁLÁCH. Ortogonální pole kolem nosiče zůstávají PRÁZDNÁ.** |

**Mechanismus, proč „víc" škodí:** odsun rohu jde vždy do trojice polí, ve
které je jedno ortogonální. Dokud jsou ortogonály volné, **odsun tam MUSÍ jít
a řetěz nevznikne**. Jakmile je zaplníme vlastními těly, ta úniková pole
zmizí a blok na roh **prostrčí naše tělo do nosiče** — zadarmo, bez hodu.

**Ověřeno na živé pozici (12.08.):** nosič na (14,6), kolem něj **pět** našich.
SBZ17 blokne roh DBZ9 na (13,5) ⇒ pole odsunu `(14,6) nosič`, `(13,6) DR5`,
`(14,5) volné`. **Zachránilo nás jediné volné pole.** Kdyby na (14,5) někdo
stál, řetěz jde do nosiče.

⭐ **A druhý důvod, stejně vážný — sami si pomáháme s obklíčením.** Kolem
nosiče je osm polí. S pěti našimi zbyla **tři volná** — a soupeř na ně dojde
**právě třemi** hráči bez jediného hodu. Pak nosič **nemá kam šlápnout,
nehne se ani s dodgem**, a na otevření pole musíme utratit blitz.
⇒ **Každé naše tělo navíc u nosiče je jedno pole, které za soupeře zazdíváme
my.**

⚠️ **Tím se ruší dřívější napětí:** cage-fill **neznamená „nacpat kolem
nosiče, co dojde"** — znamená **obsadit ROHY**. Čtyři, a dost.

### ⭐ Proč PROTILEHLÉ — dokázáno geometricky (12.08.)
Pravidlo „Guard na protilehlých rozích" máme od 11.08., ale **bez důvodu**.
Tady je: rozhoduje, kolik **ortogonálních** polí u nosiče je sousedem aspoň
jednoho našeho rohu — tedy kam si soupeř nemůže stoupnout, aniž ho markujeme
a aniž na něj máme asistenci.

| dvojice rohů | pokrytí ortogonál |
|---|---|
| **SZ+JV** nebo **SV+JZ** *(protilehlé)* | **4 ze 4** ✅ |
| kterákoli **sousední** dvojice | 3 ze 4 — jedno pole zůstane volné |
| jeden roh sám | 2 ze 4 |

⇒ **Jen protilehlá dvojice uzavře nosiče ze všech čtyř stran.** U sousední
dvojice existuje pole, kam si soupeř stoupne úplně volně — a je to zrovna to
pole, ze kterého pak blitzuje.

### ⭐⭐⭐ E4c — VOLNÉ POLE U NOSIČE JE ASISTENCE PRO SOUPEŘE
**Uživatel 12.08.: *„pokud pošleme nosiče s jakýmkoliv volným rohem, GR si to
obejde a praští nás na 2 proti — to je vždy špatně."*** A dodává klíčové
odstupňování: *„musíme obsadit 4 rohy — obsadit VÍC je menší chyba —
ale NEOBSADIT jediný roh je průšvih."*

⚠️ **Toto opravuje můj vlastní výpočet E5.** Počítal jsem sílu, se kterou
blitzující **přijde sám**. Jenže soupeř má celé kolo pohybu: **napřed doplní
volná pole kolem nosiče a teprve pak blitzuje** — a každé to tělo je
**ofenzivní asistence**.

**Přepočítáno na živé pozici** (obsadit jsme uměli **jediný** roh, zbytek
zamčený v zónách):

| cíl nosiče | zisk | jejich těl k nosiči | nejlepší jejich blitz |
|---|---|---|---|
| (18,7) · (18,6) · (17,7) | +3 až +4 | **8 z 8 polí** | **3 kostky ONI** |
| (16,9) | +2 | 7 ze 7 | **3 kostky ONI** |
| (15,8) · (15,7) | +1 | 5–6 polí | **2 kostky ONI** |

⇒ **Se všemi rohy volnými nedostaneme kostky ani jednou.** Vzdálenost s tím
nic nedělá — rozhoduje **počet volných polí**, ne kam nosič dojde.

| # | povinnost |
|---|---|
| **E4c [P]** | **Nosič se smí pohnout jen na pole, jehož rohy DOKÁŽEME OBSADIT VOLNÝMI TĚLY.** Ne „kam dojde" a ne „kde jsou rohy čisté" — **kolik jich reálně zavřeme**. **Volné pole u nosiče je slot pro soupeřovu asistenci.** |

### ⭐ Odstupňování chyby (uživatel 12.08.) — NENÍ symetrické
| stav | hodnocení |
|---|---|
| **4 rohy** | správně |
| **víc než 4** (těla i na ortogonálách) | **menší chyba** — porušuje E4 (chain push, zazdění), ale hmota nosiče chrání |
| **chybí byť JEDINÝ roh** | **PRŮŠVIH** — tou dírou si soupeř přivede asistenci a blitz se otočí |

⇒ **Chybovat se smí jen směrem nahoru.** „Pět těl u nosiče" je vada, kterou
soupeř trestá **tempem**; „tři rohy místo čtyř" je vada, kterou trestá
**míčem**. To jsou nesouměřitelné ceny a proto E4c přebíjí E4.

### ⭐ Důsledek pro měření eskorty
**Eskorta se neměří tím, kolik rohů je DOSAŽITELNÝCH, ale kolika VOLNÝMI těly
je umíme obsadit.** To jsou dvě různá čísla: v měřené pozici bylo dosažitelných
4 ze 4, ale **volná těla byla dvě** (a jedno z nich devět polí vzadu) —
protože devět z jedenácti stálo v soupeřových zónách po dvou kolech mlácení.
⇒ **Cenou mlácení není jen attrition, ale ZTRÁTA MOŽNOSTI POHNOUT MÍČEM.**

### ⭐⭐ E4b — a ty čtyři rohy mají mít GUARD
**Uživatel 12.08.: *„guard je důležitý."*** Změřeno na třech blocích téhož kola
— přepočítáno, jak by dopadly bez něj:

| blok | s Guardem | **bez Guardu** |
|---|---|---|
| DR5 → SGR19 | ST5:2 = **3 kostky my** | ST3:2 = 2 kostky my |
| DL2 → SBZ18 | ST4:3 = **2 kostky my** | ST3:3 = **1 kostka** |
| DL3 → STH16 | ST6:4 = **2 kostky my** | ST3:4 = **2 kostky ONI** |

⭐ **Ten třetí se bez Guardu OTOČÍ** — kostky by nevybírali my, ale oni.

**Důvod je pokaždé týž: všichni asistenti byli OZNAČKOVANÍ.** DLG10, DLG11,
DBZ8 i DTS6 stáli v jejich tackle zónách a **bez Guardu by neasistoval ani
jeden**.
⇒ **Guard je jediná dovednost, která funguje právě tehdy, když jsme
obklíčení** — tedy v situaci, do které se trpaslík dostane každé kolo.
Máme ho na **6 z 11**, takže „čtyři rohy s Guardem" je splnitelné.

| # | povinnost |
|---|---|
| **E4a [P]** | ⭐ **MINIMUM jsou DVA PROTILEHLÉ rohy** (uživatel 12.08.: *„2 protilehlé rohy je minimum — za pokus i pro elfy"*). Čtyři je cíl, dva protilehlé je podlaha — a je to podlaha, na kterou dosáhne i tým, který si nemůže dovolit obětovat čtyři těla. |
| **E4b [P]** | **Čtyři rohy eskorty obsazovat přednostně hráči s Guard.** Longbeard +Guard ×2, Blitzer ×2, Troll Slayer ×2 — z nich se čtyři vyberou vždy. Prostý Longbeard patří do rohu až jako poslední možnost. |

## 13.4 POVINNOSTI

| # | povinnost |
|---|---|
| **E1 [P]** | **Na konci našeho kola nesmí na nosiče dosáhnout BEZ DODGE nikdo — `REACH0 = 0`.** ⚠️ **ALE viz 13.4a — samotný REACH0 je zavádějící, potřebuje druhý vstup.** 1,8 % ztráty proti 14 % základní míře. **Dnes plněno ve 42 % kol** ⇒ je to dosažitelné, ne zbožné přání. **Fallback**, když to nejde: aspoň žádný ≥2kostkový blitz na nosiče. |
| **E2 [P]** | **Soupeř smí mít nejvýš JEDEN bezplatný ≥2kostkový blok (`FB2 ≤ 1`), cíl 0.** Koleno je mezi 1 a 2. |

## 13.4a ⚠️ REACH0 SÁM NESTAČÍ — chybí mu počet NAŠICH u nosiče

**Nález z odehrané situace 12.08.** Nosič skončil na (14,6) s **REACH0 = 3**,
což podle korpusové korelace znamená ~22 % ztráty míče. **V té pozici to
neplatilo** — a je vidět proč:

Kolem nosiče stálo **PĚT našich těl**. Kdokoli jejich k němu přijde, dostane
proti sobě **tři obranné asistence** ⇒ nosič je efektivně **ST6**:

| kdo by udeřil | jejich ST | proti ST3+3=6 | výsledek |
|---|---|---|---|
| Gutter Runner ×3 | 2 | 6 > 2×2 | **3 kostky — vybíráme MY** |
| Lineman / Thrower / ball-hunter | 3 | 6 > 3 | **2 kostky — vybíráme MY** |

**Ani jedna jejich možnost jim nedá kostky.** Blitz na nosiče je pro ně čistě
ztrátový.

⭐ **Proč korelace neplatila:** REACH0 měřený přes korpus obvykle znamená, že
nosič stojí **sám** — a to je ta skutečná příčina ztrát. Kde je obklopený,
je mechanismus zablokovaný a číslo z korpusu se na tu pozici **nedá
aplikovat**.

⇒ **E1 potřebuje DVA vstupy, ne jeden:**
> **`REACH0` (kolik jejich k nosiči dojde bez dodge) a `ESCORT` (kolik našich
> u něj stojí).** Nebezpečný je až **vysoký REACH0 při nízkém ESCORT**.
> Vysoký REACH0 s pěti těly kolem znamená, že se k němu **dostanou, ale
> neublíží mu** — zbytkové riziko pak není ztráta míče, ale **ZAMRZNUTÍ**:
> označkovaný AG2 nosič potřebuje na odchod **4+ (50 %)**, a to je ztráta
> tempa, ne turnover.

**⇒ K35 implementovat jako dvojici, ne jako jedno číslo.** A `ESCORT` se počítá
touž primitivou `volniAsistenti` — jen z naší strany.

**Obecné poučení o metodě:** korelace změřená přes korpus se nesmí aplikovat
na pozici, kde je její **příčinný mechanismus** prokazatelně zablokovaný.
Práh bez mechanismu je pověra.

## 13.4b ⭐⭐ TVAR versus HMOTA — co se stalo, když tempo dostalo přednost

**Odehráno 12.08.** Uživatel dal přednost pohybu klece: nosič **+3 pole**,
dva KO, řada y=6 uklizená — a za to **nula rohů**, `REACH0` = 3 a **pět** našich
těl namačkaných u nosiče (porušené E4). *„Skaven mě nepotrestal."*

⚠️ **Nezobecňovat výsledek, zobecnit MECHANISMUS.** Nepotrestal ho z důvodu,
který se dá spočítat dopředu — a jindy platit nemusí.

### Eskorta má DVĚ různé funkce a ty se navzájem vyměňují
| funkce | co kupuje | čím se platí |
|---|---|---|
| **TVAR** (rohy na diagonálách) | soupeř se k nosiči **NEDOSTANE**; nosič zůstává **pohyblivý** | těla musí stát na konkrétních polích |
| **HMOTA** (těla vedle nosiče) | soupeř se dostane, ale **NEUBLÍŽÍ MU** — každý útočník proti sobě dostane naše obranné asistence | ubírá nosiči volná pole ⇒ **zamrznutí** |

**Uživatel obětoval TVAR a nechal si HMOTU.** Výsledek přesně odpovídá:
soupeř nosiče **obklíčil a zamrazil** *(zaplnil všechna tři zbylá volná pole)*,
ale **neudeřil ani jednou** — každý jeho blitz by nám dal kostky do ruky.

⇒ ⭐ **Obětování tvaru MĚNÍ RIZIKO ZTRÁTY MÍČE NA RIZIKO ZTRÁTY TEMPA.**
Zaplatili jsme příštím kolem: blitz plus tři bloky padly jen na to, aby se
nosič zase mohl hnout. Míč jsme neriskovali ani jednou.

**A pro trpaslíka je to často DOBRÝ obchod** — 0:0 je náš nejčastější výsledek
a neprohráváme tím, že ztratíme míč, ale tím, že nikdy nedojdeme.

### Podmínka, za které to platí — a je to KONTROLA, ne odhad
| # | kontrola |
|---|---|
| **E5** | **Spočítat nejlepší kostky, které soupeř dokáže na nosiče postavit.** Dokud **žádná** jeho možnost nedává kostky JEMU, hmota svou práci dělá a **tvar se smí vyměnit za tempo**. Jakmile mu aspoň jedna možnost dá kostky, hmota nestačí a tvar je povinný. |

**Změřeno v té pozici:** všech 7 jejich stojících, nosič ST3 + **3 obranné
asistence** = ST6 ⇒ Gutter Runner ST2 → **3 kostky pro NÁS**, jejich ST3 →
**2 kostky pro NÁS**. Ani jedna možnost jim kostky nedala.

⚠️ **Kdy to NEPLATÍ** (a v té pozici to jen shodou okolností nenastalo):
* soupeř má **ST4+** — pak asistence nestačí na převahu
* soupeř má **Guard** na hráči, který k nosiči dosáhne *(SBZ18 ho má — jen
  zrovna ležel)*
* naši asistenti jsou **označkovaní a nemají Guard** ⇒ neasistují (viz E4b)
* soupeři zbývá dost těl, aby zaplnil volná pole **i udeřil** — tady už měl
  dva KO a na obojí nestačil

**⇒ E5 se počítá TOUŽ primitivou jako E1/E2** — stačí ji pustit na nosiče
z jejich strany.

## 13.5 ⭐ Co to dělá s R1 (a je to oprava našeho výkladu)

Roh klece v soupeřově tackle zóně **nemá samostatný signál** — v cross-tabu
zmizí. **Škodí jen tím, že otevírá cestu k nosiči.**

⇒ **R1 je správné pravidlo ze špatného důvodu.** Nechrání roh; **drží
REACH0 na nule.** Praktický důsledek: kdyby někdy stálo proti sobě „čistý
roh" a „nulový REACH0", **vyhrává REACH0** — je to ta veličina, o které víme,
že předpovídá.

## 13.6 Zahozeno podle předregistrace
**SURF** (r ≈ 0, výskyt 2,6 % kol — neměřitelné) · **MARKED** · **ESC** ·
**CCBAD** (roh v TZ, viz 13.4) · REACH bez rozlišení dodge. Odchody ze hřiště
nepředpovídá pořádně nic — základní míra 0,066/kolo je pod rozlišením korpusu.

**Podle ras:** proti skavenům je všechno mírnější (ztráta míče 9,5 %,
BLZ 0,87); **nejtvrdší kontakt je proti orkům a lidem.**

**Nezměřeno:** turnover v našem vlastním následujícím kole (měřena ztráta
během jejich kola — kauzálně čistší), kauzalita, replikace na dalších dvou
korpusech.

---

# ČÁST 14 — ROZPOČET BLITZU (18.08.2026)

> *Uživatel 18.08.: „je ten blitz na nosiče někde v dlouhém plánu blitz akce?"*
> Nebyl. Kapitola byla rozdělaná a přerušená — proto tahle část.

## 14.0 Proč to musí být kapitola, a ne další povinnost

Blitz je **jediná akce s tvrdým limitem 1× za kolo**. Všechno ostatní v téhle
specifikaci je seznam povinností, které se dají splnit vedle sebe; blitz je
**rozpočet** a jeho uchazeči se **vylučují**.

⛔ **Dosud byl rozsypaný na dvanácti místech a měl DVA různé žebříčky:**

| kde | co říká |
|---|---|
| **S2.10** | 1) prorvat koridor · 2) odmarkovat nosiče · 3) nejšťavnatější cíl |
| **9.5** | 1) blitz do zdi kupředu · 2) odmarkování nosiče · 3) příležitostně GR |
| **S7.5** | blitz na nosiče, když je šance sražení dost velká; jinak na eskortu |
| **S8.3** | blitz na nosiče **vždy**, i za horších kostek |
| **S5.6** | nejdřív odmarkovat pole kolem míče, pak sbírat |
| **S6.3** | blitz na toho, kdo by míč sebral |
| **Z10** | kolo bez blitzu je promarněný zdroj |

Ty seznamy si neodporují náhodou — **každý řeší jinou situaci a žádný neříká,
podle čeho se řadí.** Proto se nedaly sečíst a proto se šest nálezů o blitzu
(P0.6 · P2 · P6 · P15 · K32 · P33) měřilo proti šesti různým pozadím.

## 14.1 ⭐ JEDINÉ KRITÉRIUM: BLITZ SE KUPUJE DOSAH, NE NÁSILÍ

**Blok je zadarmo a bez limitu — ale jen pro toho, kdo už sousedí.
Blitz je jeden — a jeho jediná vlastnost navíc je, že tělo někam DOJDE.**

⇒ **Blitz patří výhradně tam, kam se blokem nedosáhne.**
Všechno ostatní v žebříčku je důsledek téhle jedné věty.

**Doloženo, ne odvozeno** *(3 000 her, 18.08.)*:
* **61 % polluterů** u rohu klece jde srazit **blokem zdarma** — na ně se blitz
  utrácet nemá, a přesto **45,5 % dnešních blitzů na roh padlo v kolech, kdy
  blok zdarma šel**. To je vyhozený rozpočet, ne špatný cíl.
* Blitz na roh dá Δx **+1,80**, blitz do zdi **+2,52** (−6,4σ) — ne proto, že
  je zeď cennější, ale proto, že **na zeď se jinak nedosáhne**, kdežto na roh
  většinou ano.
* Soupeřův nosič **nemá souseda v 5,61 kola na zápas**; blokem je nedostupný
  úplně. Tam je blitz jediný nástroj.

## 14.2 Žebříček — jeden pro útok i obranu

Řadí se **shora dolů, první splnitelné patro vyhrává.** Patra jsou seřazená
podle toho, **co v tomhle kole nejde koupit blokem**:

| # | patro | podmínka | platí v |
|---|---|---|---|
| **B1** | **odemknout skórování** — tělo v cestě mezi nosičem a endzonou, když se dnes skóruje | S10 / S9 | S9.4 · S10 |
| **B2** | **prorazit koridor / zeď** — `resistance` má koho a bez něj se rozvrh nesplní | postup vpřed | S2.9 · S2.11 |
| **B3** | **odmarkovat vlastního nosiče** — nosič AG2 platí za odchod ze zóny 4+, tj. ~50 % turnover; blitz je levnější než ten hod | nosič je markovaný | 9.5 (2) |
| **B4** | **soupeřův nosič, na kterého nikdo nesousedí** — jediný nástroj, jak se k němu vůbec dostat | obrana / boxing-in | S7.5 · S8.3 |
| **B5** | **odemknout sběr volného míče** — odmarkovat pole u míče, nebo srazit toho, kdo by ho sebral | S5 / S6 | S5.6 · S6.3 |
| **B6** | **polluter u rohu klece, na kterého NIKDO nesousedí** | 39 % polluterů | P2 (2) |
| **B7** | zbytek — nejšťavnatější dostupný cíl | nic výše nečeká | S2.10 (3) |

⚠️ **B4 povyšuje na B1 v S8** *(soupeři zbývají ≤ 2 kola)*: tam turnover stojí
soupeře celý drive, takže se bere i za horších kostek. To je jediná výjimka
z řazení a je to výjimka **situační**, ne cílová.

## 14.3 ⛔ KDY SE BLITZ NEUTRÁCÍ

`Z10` říká *„kolo bez blitzu je promarněný zdroj"*. **Platí, ale ne bezvýhradně** —
a rozdíl je právě to kritérium z 14.1:

* **NEUTRÁCET** na cíl, na který **někdo z našich už sousedí**. Blok to udělá
  zadarmo a blitz zůstane. *(Tohle je ten nejčastější dnešní únik.)*
* **NEUTRÁCET** na cíl, jehož sražení nic neodemyká — samotná attrition je
  práce pro **blok**, ne pro blitz.
* **NEUTRÁCET** nosičem. Blitz nosičem ho vystavuje a nosič má jinou práci.
* ⚠️ **Jinak platí Z10 tvrdě:** dnes máme soupeřova nosiče v dosahu blitzu
  **4,12 kola na zápas**, blitz na něj padne v **48,5 %** a v **25,1 %
  (= 1,04 kola na zápas) nespadne blitz vůbec žádný.** Na nosiče přitom
  dosáhnou **4+ naše těla v 60,4 %** těch kol ⇒ **není to otázka
  dosažitelnosti, ale volby.**

## 14.4 Co je změřené a co ne

| | stav |
|---|---|
| nosič v dosahu blitzu 4,12/zápas · blitz na něj 48,5 % · neutracen 1,04/zápas | ✅ 3 000 her |
| na nosiče dosáhnou 4+ těla v 60,4 % | ✅ |
| blitz na roh +1,80 vs do zdi +2,52 (−6,4σ); 45,5 % blitzů na roh zbytečně | ✅ |
| 61 % polluterů jde srazit blokem zdarma | ✅ |
| **kolik z toho 1,04 „neutraceno" bylo správně** *(dosah je horní odhad — bez dodge z TZ a bez obsazených polí)* | ⛔ NEZMĚŘENO |
| **cena patra B3 (odmarkovat nosiče) proti B2 (zeď)** — obojí je „vpřed", ale nikdy se neměřily proti sobě | ⛔ NEZMĚŘENO |
| **jestli je 26,4 % blitzů „jinam" chyba** — polovina těch cílů stála **hned vedle nosiče** (eskorta), což může být záměr | ⛔ NEZMĚŘENO |

## 14.5 Kontroly, které z toho plynou

* **K10** (blitz použit) zůstává, ale je to **dolní odhad** — viz níže.
* **K11** (blitz na tělo v cestě) → přeformulovat na **patro B2**.
* **K32** (pořadí blitzu) byla **BLOKOVANÁ na X1**, protože *„blitz se v logu
  nepozná od bloku"*. ⭐ **18.08. odblokována rekonstrukcí, která nový log
  nepotřebuje:** v korpusu žádná událost `BLITZ` není, jen `BLOCK` — ale
  **blok, u kterého ÚTOČNÍK se svým cílem na začátku kola NESOUSEDIL, je
  blitz** (blok vyžaduje sousedství, blitz je 1/kolo). Tím se K32 dá měřit hned.
* **NOVÁ K37 — „blitz utracen na dosažitelný cíl"**: podíl blitzů, jejichž cíl
  **už sousedil s někým naším**. To je přímé měření úniku z 14.1 a dnes je to
  jediné číslo, které kapitolu vymáhá.

## 14.6 Co tahle část NEZAKRYLA

* **Kdo blitzuje.** Žebříček řadí cíle, ne útočníky. Volba těla je jiná otázka
  (P6: cíl blitzu se dnes vybírá podle surových kostek) a váže na **T1.8**.
* **Blitz v soupeřově kole** neexistuje — celá část je o našem rozpočtu.
* **Frenzy** mění cenu blitzu (vynucený druhý blok), ale trpaslík ho má jen na
  dvou Troll Slayerech ⇒ pro nás okrajové, pro obecné pravidlo ne.

---

# ČÁST 15 — POSUN KLECE (18.08.2026)

> Kapitola byla rozdělaná a přerušená. V spec stálo jen **8.2 „Klec není
> zahajovací formace"** — tedy **kdy se klec staví**. Jak se **pohybuje**,
> žilo jen v paměti z 11.08. a v kódu, který jsme dnes zamítli.

## 15.0 ⭐ KRITÉRIUM: KLEC SE NEMĚŘÍ TVAREM, ALE ROZPOČTEM TĚL

Celá tahle kapitola visí na jednom měření *(3 000 her, 18.08.)*:

| veličina | σ | replikuje? |
|---|---|---|
| **počet rohů klece** | −2,1σ | ⛔ ne |
| **počet ČISTÝCH rohů** | **−0,2σ = nic** | ⛔ mění znaménko |
| **počet ŠPINAVÝCH rohů** | **−6,8σ** | ✅ |
| podíl čistých (K29) | 5,1σ | ✅ |

⇒ **Postavit další roh nekupuje nic. Špinavý roh stojí.** A stojí konkrétně:

| tělo, které stálo v rohu na konci kola N | ŠPINAVÝ roh | ČISTÝ roh |
|---|---|---|
| **nedostupné v kole N+1** | **51,2 %** | 13,5 % |
| znovu poslouží jako čistý roh | **12,4 %** | 33,1 % |

A hrajeme se **7,03 volnými těly z 11** (2,62 leží, 1,02 zamčených), tedy
**5,5 kandidáta na 4 rohy**.

⇒ **Klec je účet, ne útvar.** Každé rozhodnutí o kleci — postavit, jet, jak
daleko, kterým směrem, kým — je **položka v rozpočtu těl**. Špinavý roh není
„horší roh", je to **spálené tělo**: ⭐ *špinavý roh je vada obdobných důsledků
jako žádný roh* (uživatel 18.08.), a navíc dražší.

## 15.0b ⭐⭐⭐ PRAVIDLO KLECE *(uživatel 19.08.)* — a co z něj zbylo v kontrole

> **„Optimum klece jsou ČTYŘI rohy, všechny ČISTÉ, a ŽÁDNÍ další sousedi
> s ballcarrierem."** — *„to je pravidlo"*

**Otevřená otázka z 15.8 („kolik rohů je optimum") je tím ZAVŘENÁ, a ne měřením.**
σ-tabulka ukazovala, že *počet* rohů nepředpovídá nic (−2,1σ) a počet *čistých*
rohů nic (−0,2σ) — a nabízelo se z toho číst *„možná jsou 4 rohy špatný cíl"*.
Bylo to čtení metru, ne hry: **cíl je konjunkce tří klauzulí a rozložený na
sčítance přestane existovat.** Táž rodina jako K33 *(ano/ne −2,5σ vs počet
+10,4σ)* — jenže obráceně: tady se **konjunkce nesmí rozložit**, protože
tři rohy „ze čtyř" nejsou 75 % klece, jsou to **otevřená klec**.

⇒ **Cílový stav (K29⭐⭐), všechny tři klauzule najednou:**

| | klauzule | proč |
|---|---|---|
| **(1)** | **4 rohy** — všechna čtyři diagonální pole nosiče obsazená naším stojícím tělem | tři rohy = díra, kterou se dojde k nosiči bez dodge |
| **(2)** | **všechny čisté** — žádný roh nesousedí se stojícím soupeřem | špinavý roh je spálené tělo (15.0) |
| **(3)** | **žádní další sousedi nosiče** — čtyři ortogonální pole **prázdná**, a v žádném z osmi polí soupeř | soused je buď kontakt na nosiči, nebo naše tělo utracené za nic |

⚠️ **(3) zakazuje i VLASTNÍ tělo navíc.** Není to estetika: ortogonální pole vedle
nosiče je jediné, které klec **nechrání dvěma tackle zónami**, takže tělo tam
nic nedrží — a podle 15.0 je klec **rozpočet**, ne útvar. Tělo, které tam stojí,
chybí v rohu, kterých máme 5,5 kandidáta na 4.

### Jak často to plníme — 3 000 her, 24 692 našich kol s míčem *(19.08.)*

| | | |
|---|---|---|
| (1) 4 rohy | 2 396 | **9,7 %** |
| (2) všechny čisté | 21 425 | 86,8 % |
| (3) nosič bez dalších sousedů | 6 000 | **24,3 %** |
| **PRAVIDLO (1)∧(2)∧(3)** | **675** | **⛔ 2,7 %** |

Rozdělení podle počtu rohů: **0 rohů v 33,1 %** kol · 1 v 24,2 % · 2 v 19,6 % ·
3 v 13,4 % · 4 v 9,7 %.

### ⛔ Nález: dnešní K29⭐ nadhodnocuje **4×**, a to ze dvou nezávislých důvodů

1. **Chybí jí třetí klauzule.** `len(filled) == 4 and not dirty` — o sousedech
   nosiče neví. V **1 326 kolech (5,4 %, tj. dvě třetiny všech jejích „splněno")**
   hlásí *plnou čistou klec*, a nosič přitom má dalšího souseda. Z toho **291 ×
   stojí vedle nosiče SOUPEŘ** — lehlý nebo omráčený, takže rohy zůstaly formálně
   „čisté", jenže příští kolo vstane **v kontaktu s míčem**.
2. **Má useknutý jmenovatel.** Přeskakuje kola, kde nestojí ani jeden roh
   *(63 % N/A)* — jenže *„klec nestojí"* je **porušení pravidla, ne prázdný
   predikát**. Proto čte **12,3 %** tam, kde je pravda **3,1 %** *(týchž 400 her)*.

⇒ **K29⭐⭐ počítá všechny tři klauzule přes VŠECHNA naše kola s míčem.**
Ověřeno dvěma nezávislými implementacemi na týchž 400 hrách (3,1 % obě).

### ⭐ Strop opravy: **0,73 rohu na kolo se dá získat překročením o JEDNO pole**

V **52,4 % kol** platí zároveň: *chybí roh* **a** *naše tělo stojí ortogonálně
u nosiče*, tedy na poli, které (3) zakazuje — **jedno pole od prázdného rohu,
a u nosiče už je**. Průměrně **1,01 našeho těla na kolo** stojí takto, proti
**0,63 soupeře** u nosiče.

⚠️ **Je to STROP, ne plán:** neptá se, jestli to tělo už hrálo, ani jestli
nemarkuje něco dražšího *(hierarchie R1 > R3 > R2 > R4)*. Ale řádově je to
**víc než cokoli, co jsme letos měřili jako strop** (P10a 0,23 · P8 0,056),
a nestojí to ani jeden blitz — jen jiné pole.

### ⛔ A nejtvrdší číslo kapitoly

**V 39,3 % našich kol končí nosič v kontaktu se soupeřem.** Ne během
soupeřova kola — **na konci NAŠEHO**, kdy jsme právě rozhodli, kde bude stát.

## 15.0c ⭐⭐⭐ POLE NOSIČE SE DOPOČÍTÁVÁ ZE ZAMÝŠLENÉ KLECE *(uživatel 19.08.)*

> **„Podle toho, kde bude stát nosič v našem kole, přece dopočítáme vše —
> včetně toho, aby byly rohy čisté."**

Je to pravidlo o **pořadí rozhodování**, a obrací to dnešní pořadí:

| | dnes | podle pravidla |
|---|---|---|
| 1. | nosič popojde (rovně kupředu, `cage_advance.cpp:41`) | vyber **cílové pole nosiče** |
| 2. | klec se dopočítává k místu, kam došel | z něj plynou 4 rohy, jejich čistota i zákaz dalších sousedů |
| 3. | čtvrtý roh se nedostaví | těla se přiřadí na rohy, které tím byly určeny |

⇒ **Pole nosiče je kořen plánu, ne jeho vstup.** Cílové pole se hodnotí podle
klece, která z něj vyjde — ne naopak. Je to táž chybějící dimenze **KAM**
jako P9 (pole odsunu) a P35 (pole, kam blitzující dojde), jen na nejdražším
místě: pole nosiče určuje **všechna čtyři** rohová pole naráz.

### ⛔ Strop: možné v 95,6 %, plníme ve 2,7 %

19 964 kol, kde nosič stojí na začátku našeho kola, ⌀ 7,49 volného stojícího
těla mimo nosiče. Pro každé pole v dosahu nosiče se ptáme, jestli z něj vyjde
**plná čistá klec bez dalších sousedů** a jestli na ty čtyři rohy **dosáhnou
čtyři naše těla** (bipartitní párování):

| | kol | |
|---|---:|---:|
| ✅ **takové pole existuje** | 19 095 | **95,6 %** |
| ⛔ méně než 4 stojící těla — rozpočet to nedovolí | 735 | 3,7 % |
| ⛔ žádné dosažitelné pole to nedá | 134 | **0,7 %** |

A z těch 19 095 kol **nosič ve 25,7 % už na takovém poli STOJÍ** a nemusel by
se hnout vůbec; v dalších 22,6 % je to **jedno pole**.

⇒ **Pravidlo je splnitelné v 95,6 % kol a plníme ho ve 2,7 %.**
Rozdíl **není rozpočet těl** (ten brání ve 3,7 %) ani soupeř (0,7 %) —
je to **volba pole**.

⚠️ **Strop, ne plán.** Dosah se počítá Chebyshevem z `ma` bez TZ, dodge a GFI
(horní mez); ležící těla se nepočítají (konzervativní); neptá se, **co to stojí
na tempu** — a tempo je K9a s 20,7σ, takže *„postav klec kdekoli, hlavně čistou"*
je legální jen tehdy, když se pole vybírá **v rámci** postupu, ne proti němu.

### ⛔ Lajna: co nejde, to nejde — a proto se tam nechodí *(uživatel 19.08.)*

Roh **mimo hřiště** je 2,1 % chybějících rohů. Na postranní čáře jsou dva rohy
geometricky nemožné ⇒ **4 rohy tam mají 0,0 %** (193 kol). ⭐ **Není to vada
k opravě, je to stav, kterému se má vyhnout výběr pole** — a už jedno pole od
lajny jsou všechna čtyři pole na hřišti, a přesto je to jen 1,8 %, takže i tam
zbytek dělá volba, ne geometrie. Posiluje to **15.5** *(vlastní klec k lajně
netlačit; soupeřovu ano)*.
⏰ **Zaslouží samostatný rozebraný příklad k diskuzi** *(uživatel)* — zapsáno
jako **T1.10**.

## 15.1 Rychlost: klec jede tak rychle, jak rychle se dokáže složit

> *Uživatel 18.08.: „je zbytečné uvedení konstanty někam, kde by stačilo —
> nesmí se rozpadnout, ale má jet co nejrychleji."*

**Pravidlo:** krok klece = **největší krok, po kterém se v cíli složí čisté rohy
z těl, která tam dojdou.** Cíl je **maximum**, ne strop.

⛔ **Do kódu ani do kontroly nesmí konstanta „2 pole za kolo".** To je jen to,
co ta podmínka dnes vydá na trpasličím rosteru se 7,03 volnými těly. Jiný roster,
jiný počet ležících, jiné číslo. *(Táž chyba jako u S7.3/T1.8 — implementovat
pravidlo, ne jeho výsledek.)*

⚠️ **Přelévání ZAMÍTNUTO** *(uživatel 11.08.)*: zadní rohy se neodlepují, aby se
staly předními. *„Neaktivací nevidím nic co získat, stále jsou to vázané rohy."*
⇒ **tuhý posun o co nejvíc polí**, ne dvoufázové převalení.

## 15.2 ⛔ SMĚR SE VYBÍRÁ — a dnes se nevybírá vůbec (P32)

> *Uživatel 18.08.: „musí stavět čistou klec a kdyžtak ne vždy jen přímo rovně
> kupředu."*

Ověřeno v kódu: cíl posunu vzniká na dvou místech (`cage_advance.cpp:41`
a v `tryAssign`) vždy jako `dest{carrier.x + dx*step, carrier.y}` — **`y` se
nikdy nemění.** Plánovač volí jen **JAK DALEKO**, nikdy **KAM**; všechny
zvažované cíle leží na jedné přímce vpřed.

**Pravidlo:** cílem posunu smí být i pole **do strany nebo šikmo**.
Kritérium výběru je **počet ŠPINAVÝCH rohů v cíli** *(ne počet rohů — ten
nepředpovídá nic)*, při shodě dál vpřed.

⚠️ **Táž rodina jako P9** (`choosePushSquare` = „rovně dozadu první"):
**geometrie se nevybírá, jen se vykoná.**

## 15.3 Pořadí: eskorta uklidí cestu, teprve pak jede klec

Z bbtactics (souhrn 11.08.) a potvrzeno zásadou **8.5**: hráči **mimo** klec
musí odklidit soupeře v cestě **dřív**, než se klec hne. Klec, která vyrazí do
neuklizeného koridoru, dojede k tělu a rozpadne se na půl kroku.

⭐ **A utrácí se v pořadí od volného k vzácnému:** napřed **bloky zdarma**
(kdo už sousedí), pak **blitz** — a jen tam, kam se blokem nedosáhne
(**ČÁST 14**). **61 % polluterů u rohu jde srazit blokem zdarma**; utratit na ně
blitz je vyhozený rozpočet, ne špatný cíl.

## 15.4 Kdo stojí v rohu

**Roh není odkladiště.** Do rohu patří tělo, které tam **umí stát a nechybí
jinde**. Změřeno: skaven staví **36,4 % rohů z Gutter Runnerů** (ST2, AV7) —
nejhorší možné tělo: neuassistuje, neudrží pole, a je to hráč, který je jinde
nejcennější (**P16**).

⚠️ Engine dnes filtruje jen **spolehlivost aktivace** (Bone-head, Really Stupid,
Wild Animal, Take Root, Secret Weapon, Ball & Chain) — **na vhodnost ani na cenu
jinde se neptá.**

⛔ **Klec nesmí markovat** (hierarchie R1 > R3 > R2 > R4): tělo v rohu má úkol
držet roh, ne stát v soupeřově tackle zóně. Zamčené tělo je podle **K36**
měřitelná ztráta tempa (Δx nosiče +2,24 při ≤2 zamčených proti +1,14 při 6–8).

## 15.5 Lajna

Klec u postranní čáry **ztrácí polovinu rohů**. ⇒ **vlastní klec k lajně
netlačit; soupeřovu ano.** *(Spojuje se s T1.8: tlačit k lajně jen když
`vzdálenost ≤ odsuny, které vyrobíme V JEDNOM KOLE`.)*

### ⛔⛔ PROTI SKAVENŮM STRIKTNĚ *(uživatel 19.08.)*

Doktrína 15.5 byla rasově slepá. Změřeno *(3 000 her, ztráta míče v soupeřově
NÁSLEDUJÍCÍM kole podle vzdálenosti našeho nosiče od lajny na konci NAŠEHO)*:

| soupeř | u lajny (0–1) | střed (4+) | násobek |
|---|---:|---:|---:|
| **skaven** | **20,9 %** | **10,8 %** | **×1,94** |
| orc | 27,1 % | 14,6 % | ×1,86 |
| human | 21,1 % | 14,3 % | ×1,48 |
| wood-elf | 17,6 % | 12,8 % | ×1,38 |

⭐ **Proč zrovna u skavenů „striktně“, a ne „raději ne“:** skaven je **uprostřed
hřiště nejméně nebezpečný soupeř ze všech** (10,8 %) a u lajny je naráz stejně
nebezpečný jako kdokoli jiný. ⇒ **Lajna u nich není horší varianta, lajna je
jediné místo, kde na nás dosáhnou.** U orků je absolutní penalizace vyšší, ale
nebezpečný je i střed — tam je to kompromis, tady zákaz.

Skaven má navíc **monotónní gradient** (10,8 → 14,9 → 20,9 %), orkové ne
(14,6 → 29,4 → 27,1 %) ⇒ skavení číslo je i konzistentnější.
Rozdíl **10,0 pp ± 3,2 ≈ 3,1σ** (n = 163 kol u lajny).

⚠️ **Korelace, ne kauzalita:** nosič u lajny může být *příznak* tlaku, ne jeho
příčina. Neměří to, že by odklon ke středu tu ztrátu odstranil.

⇒ **Z17 (nový zákaz): proti skavenům se vlastní nosič k postranní čáře
nepřibližuje. Není to položka rozpočtu, kterou lze přeplatit postupem.**
Doklad `diag_sideline_by_race_20260819.py`.

### ⭐⭐ UNIVERZÁLNÍ TVAR: lajna je soupeřova asistence zdarma

*(19.08., zobecnění Z17 — uživatel: „nějak univerzálněji?")*

Zapsat to jako pravidlo o **rase** je nejslabší možná forma: platí to jen do
příštího rosteru. Mechanismus je rasově nezávislý a má **dvě části**:

**(1) Geometrie.** Roh mimo hřiště neexistuje. Na postranní čáře jsou **dva ze
čtyř rohů nemožné** ⇒ plná klec má **0,0 %**. To nespraví žádné tělo a žádný hod.

**(2) Kryté směry.** Lajna pokrývá jednu stranu **za soupeře**. Uprostřed musí
hlídat všechny směry úniku, u lajny polovinu — a tu polovinu mu dělá hřiště.
⇒ **Lajna je jeho asistence zdarma, a stojí ho nula rozpočtu.**

⇒ **U1 (univerzálně, každý soupeř):** vlastní klec se k postranní čáře
netlačí; soupeřova ano. To platí bez ohledu na rasu — násobek ztráty míče
u lajny je **×1,38 až ×1,94** proti středu u **všech čtyř** měřených soupeřů.

### ⛔ Kdy je z toho ZÁKAZ, a ne kompromis

**Kritérium není rasa, je to bezpečnost STŘEDU.**

| soupeř | u lajny | **střed** | násobek |
|---|---:|---:|---:|
| **skaven** | 20,9 % | **10,8 %** | ×1,94 |
| orc | 27,1 % | 14,6 % | ×1,86 |
| human | 21,1 % | 14,3 % | ×1,48 |
| wood-elf | 17,6 % | 12,8 % | ×1,38 |

Násobek sám rasy **nerozliší** — ork má skoro týž jako skaven. Rozlišuje je
**střed**:

* **Střed je nejbezpečnější místo na hřišti** *(skaven, 10,8 % = nejnižší číslo
  v tabulce)* ⇒ odklon od lajny nás dostane do nejlepšího dostupného stavu
  a soupeř **nemá kam jinam sáhnout** ⇒ **ZÁKAZ**. Není co vyvažovat.
* **Střed je nebezpečný taky** *(ork, 14,6 %)* ⇒ odklon kupuje jen část
  a zbytek se platí dál ⇒ **KOMPROMIS** proti postupu.

⇒ **Pravidlo:** *čím bezpečnější je proti tomuhle soupeři střed, tím tvrdší je
zákaz lajny.* Skaven je dnes jediný, u koho je absolutní — ale **je to vlastnost
matchupu, ne jména rasy**, a přeměří se u každého nového soupeře týmž řezem.

⚠️ **Hypotéza mechanismu, NEZMĚŘENO:** proč zrovna skaven. Je rychlý, ale slabý
(ST2–3, AV7) — uprostřed **dosáhne, a neublíží**; u lajny se dosah mění v
poškození (odsun do davu bez hodu na armor, a nám mizí strana, kam uniknout).
⇒ *Lajna převádí DOSAH na POŠKOZENÍ, a nejvíc získává ten, kdo má dosah bez síly.*
Rozhodne to rozpad ztrát u lajny podle příčiny — zapsáno, neměřeno.

⚠️ Celá tabulka je **korelace**: nosič u lajny může být příznak tlaku.

### ⭐⭐⭐ ČÍM SE CENA LAJNY ŘÍDÍ: **MA toho, kdo na nosiče dosáhne** *(uživatel 19.08.)*

Rasa je špatný hák — se čtyřmi rasami se dá proložit cokoli (n = 4). Tohle je
na úrovni **kola**, jmenovatel ~20 000: co má nosič **kolem sebe**.
Mezi soupeři, kteří na nosiče **dosáhnou** (Chebyshev ≤ MA+2, tedy i přes GFI),
se bere nejvyšší MA a nejvyšší ST.

| max **MA** | u lajny (0–2) | střed (3+) | násobek |
|---:|---:|---:|---:|
| 6 | 27,5 % | 21,0 % | ×1,31 |
| 7 | 16,8 % | 14,7 % | ×1,15 |
| 8 | 25,2 % | 15,6 % | ×1,62 |
| **9** | 25,7 % | **13,6 %** | **×1,89** |

| max **ST** | u lajny (0–2) | střed (3+) | násobek |
|---:|---:|---:|---:|
| **3** | 21,4 % | **13,4 %** | **×1,60** |
| 4 | 31,1 % | 21,1 % | ×1,48 |
| 5 | 29,1 % | 20,4 % | ×1,43 |

⭐ **MA a ST táhnou proti sobě.** S rychlostí násobek **roste** (1,31 → 1,89)
a střed se **zlepšuje** (21,0 → 13,6 %). Se silou **klesá** (1,60 → 1,43)
a střed se **zhoršuje** (13,4 → 20,4 %).

⇒ **Hypotéza z 15.5b je tím změřená, ne jen vyslovená:**
**lajna převádí DOSAH na POŠKOZENÍ.** Kdo má dosah bez síly, získá u lajny
nejvíc — uprostřed k nám dojde a neublíží, u lajny mu hřiště dodá to, co mu
chybí. Kdo má sílu, je nebezpečný všude a lajna mu přidá nejmíň.

### ⇒ Použitelný tvar pravidla, bez jména rasy

**Tvrdost zákazu lajny se řídí MA nejrychlejšího soupeře, který na nosiče
dosáhne** *(nikoli rasou, nikoli průměrným MA týmu)*:

* **max MA ≥ 8 ⇒ ZÁKAZ.** Násobek 1,6–1,9 a střed je zároveň nejbezpečnější
  dostupný stav ⇒ odklon nemá co vyvažovat.
* **max MA ≤ 7 ⇒ KOMPROMIS** proti postupu. Násobek 1,15–1,31 a střed je
  nebezpečný taky, takže odklon kupuje jen část.

⭐ **Je to čitelné v okamžiku rozhodování** — MA soupeřů v dosahu je na desce
vidět, na rozdíl od „proti komu hrajeme". Zapadá to přímo do **P38**: pole
nosiče se dá vážit vzdáleností od lajny **úměrně max MA v dosahu**.

⚠️ **Výhrady, které to nesmí ztratit:**
* **MA a ST jsou přes roster provázané** (GR = MA9/ST2, Black Orc = MA5/ST4),
  takže obě tabulky nejsou nezávislé; tenhle řez je **nerozplete úplně**.
* **Malé buňky u lajny** (122–432 kol). MA9 drží pohodlně (12,1 pp ± 2,8 ≈ 4,3σ),
  ale **MA6 je jen ~1,8σ** a **MA7 boří monotónnost** (×1,15) — nevyhlazovat.
* Pořád **korelace**: nosič u lajny může být příznak tlaku, ne jeho příčina.

## 15.6 Obrana proti klaci: nemusíš ji rozbít, stačí ji zpomalit

Zpomalit soupeřovu klec na **1–2 pole za kolo** stačí — kolem **6.–8. kola**
musí otevřít, nebo o drive přijde. To je chvíle na protiúder.

⚠️ **A pozor na obrácený mechanismus:** roh soupeřovy klece, který stojí v naší
tackle zóně, jde srazit **obyčejným blokem** — čímž **my ušetříme blitz**.
Symetricky to platí proti nám a je to přesně důvod, proč je markovaný roh drahý.

## 15.7 Co s tím, že brána klece byla zamítnuta

`cage_advance` A/B 18.08., 6 000 párů s CRN: **−0,0248 ± 0,0068 (−3,7σ)** ⇒
**brána ŠKODÍ** a v produkci zůstává vypnutá.

⭐ **Ale to není verdikt o téhle kapitole.** Brána počítá `requiredPace =
vzdálenost / zbývající kola` — **rovnoměrnou podlahu**, tedy přesně model, který
byl 18.08. zamítnut i u K9. Vetovala postup tam, kde rovnoměrný rozvrh nedává
smysl, tj. **ve fázi klece**. Změřila se vada rozvrhu, ne vada schopnosti:
zapnutá brána postavila **víc klece a horší klec** (rohů 2,22 → 2,54, čistota
79,4 → 72,6 %) — což je podle 15.0 přesně obchod špatným směrem.

⇒ **Kód se nezahazuje.** Brána se vrací **až** s (a) rozvrhem po fázích
(**T0.1**, **P3**) a (b) výběrem směru (**15.2 / P32**).

## 15.8 Co je změřené a co ne

| | stav |
|---|---|
| počet rohů 0σ · počet špinavých −6,8σ · podíl čistých 5,1σ | ✅ 3 000 her, replikuje |
| tělo ze špinavého rohu nedostupné v 51,2 % vs 13,5 % | ✅ s kontrolní skupinou |
| 7,03 volných těl z 11; 5,5 kandidáta na 4 rohy | ✅ |
| směr posunu se nevybírá (`y` se nemění) | ✅ ověřeno v kódu |
| brána škodí (−0,0248 ± 0,0068) | ✅ 6 000 párů |
| **jaká je správná rychlost jako FUNKCE volných těl** *(dnes jen „co se složí")* | ⛔ NEZMĚŘENO |
| **kolik stojí posun do strany na tempu** — 15.2 zvedne čistotu, ale prodlouží dráhu | ⛔ NEZMĚŘENO |
| **jestli je pořadí „eskorta → klec" lepší než opačné** | ⛔ NEZMĚŘENO *(z bbtactics, ne z našich dat)* |
| ~~kolik rohů je vlastně optimum~~ | ✅ **ZAVŘENO 19.08. PRAVIDLEM, ne měřením** — 4 rohy, všechny čisté, nosič bez dalších sousedů *(uživatel)*. Viz **15.0b**. Metr se rozložením konjunkce mýlil, ne hra. |
| **plníme pravidlo v 2,7 % kol** (K29⭐ hlásila 12,3 %) | ✅ 3 000 her, 24 692 kol |
| **0,73 rohu/kolo jde získat překročením o 1 pole** | ⚠️ STROP — neptá se na dostupnost těla |
| **nosič v kontaktu na konci NAŠEHO kola v 39,3 %** | ✅ 3 000 her |

## 15.9 Kontroly

* ⭐⭐⭐ **NOVÁ K29⭐⭐ — PRAVIDLO KLECE** *(19.08., viz 15.0b)*: `4 rohy ∧ všechny
  čisté ∧ nosič bez dalších sousedů`, přes **všechna naše kola s míčem**.
  Implementováno v `diag_rules_checks_20260812.py` (`K29rule`) **dřív, než se
  nad novým korpusem cokoli změřilo** — pravidlo 18.08.
* **K29** (žádný roh není markovaný) a **K29⭐** (plná čistá klec) zůstávají
  **jen jako historická linie**: ⚠️ **K29⭐ nadhodnocuje 4×** (chybí jí třetí
  klauzule a má useknutý jmenovatel) a míří na tvar — a tvar podle 15.0
  nic nepředpovídá.
* **NOVÁ K38 — „špinavých rohů na kolo"** jako **POČET**, ne podíl. To je
  veličina, která jediná replikuje (**P1**: povinnost smí být ano/ne, metr si
  musí nechat číslo).
* **NOVÁ K39 — „tělo z rohu je příští kolo volné"**. Přímé měření 15.0:
  dnes 51,2 % vs 13,5 % podle čistoty rohu.

## 15.10 Co tahle část NEZAKRYLA

* **Kdy se klec staví** — to je 8.2 a nemění se.
* **Rozbití soupeřovy klece do detailu** — 15.6 je jen doktrína zpomalení.
* **Hand-off a pass uvnitř klece** — vlastní kapitola, píše se před celokolem.

---

# ČÁST 16 — PŘIHRÁVKA A PŘEDÁNÍ (18.08.2026)

> V celé specifikaci byla o přihrávce **jediná zmínka** — S8.4, a to ještě
> o **soupeřově** příjemci. O naší vlastní přihrávce ani řádka, přestože
> doktrína **rozhodnutá byla** (T1.4: *„rozhodnuto, nezapsáno"*).
> ⚑ Píše se **před** celokolovým plánem: pass a hand-off jsou spolu s blitzem
> **tři vzácné akce po jedné za kolo** a celokolo je právě ta kapitola, která je
> alokuje. Plánovat je s prázdnými poličkami nejde *(uživatel 18.08.)*.

## 16.0 ⭐ KRITÉRIUM: MÍČ SE NEPŘEDÁVÁ PRO POSTUP, ALE PRO VÝMĚNU NOSIČE

Trpaslík nepřihrává na vzdálenost — **AG2 a žádná přihrávací dovednost**.
Hodnota přesunu míče je proto skoro celá v tom, **KDO ho po něm nese**:

| nosič | pole za kolo |
|---|---|
| **Runner** | **3,41** |
| **Longbeard** | **1,50** |

To je **2,3×** a je to zároveň odpověď na *„kolik kol na TD"*. ⇒ **Předání je
nástroj tempa, ne nástroj vzdálenosti.**

⭐ **Kritérium tedy zní „NOSIČ JE ŠPATNÝ", ne „příjemce je lepší"** — a to je
přesně ta věta, kterou dnešní filtr nemá (**P5**).

## 16.1 Co říká pravidlo a co říká engine

* CRP dává **dvě NEZÁVISLÉ povolenky**: jednu na `PASS`, jednu na `HAND_OFF`.
  Obojí se smí v témž kole.
* ⚠️ **Engine je do 17.08.2026 sdílel** (`passUsedThisTurn`) ⇒ přihrávka
  a předání byly vzájemně **výlučné** a `CHAIN_SCORE` se rozbíjel na kroku 2.
  **Opraveno `f5998575`** *(„the hand-off has its own action allowance, and
  always did")*. ⇒ **Doktrína níž smí kombinaci použít; do 17.08. nesměla.**
* Riziko není souměrné: **hand-off ~83 %** *(prostý catch)*, **přihrávka AG2
  na dálku ~33 %**. Selhání obojího je **turnover**, tedy celý drive.

## 16.2 Žebříček — kdy se míč předává

Shora dolů, první splnitelné patro vyhrává.

| # | patro | podmínka |
|---|---|---|
| **H1** | **NOSIČ NEDOBĚHNE** — má míč tělo, které nestihne rozvrh *(Longbeard 1,50 pole/kolo)*, a vedle stojí tělo, které ho stihne | předání **stojícímu sousedovi**, který je potenciální nosič |
| **H2** | **NOSIČ JE ZAMČENÝ** — markovaný nosič AG2 platí za odchod ze zóny hod 4+, tj. **~50 % turnover**; předání sousedovi stojí 17 % | soused mimo TZ |
| **H3** | **SKÓROVÁNÍ TEĎ** — příjemce dosáhne endzony v tomhle kole a nosič ne | jen v S10 / S9 |
| **P1** | **přihrávka** — pro trpaslíka **skoro nikdy** *(uživatel)*; jen jako skórovací akce v posledním kole půle, kdy je alternativou nula | ⚠️ AG2 ⇒ ~33 % |

## 16.3 ⛔ KDY SE MÍČ NEPŘEDÁVÁ

* **Nikdy pro vzdálenost.** Míč se nese, ne posílá — to je celé S2.
* **Nikdy Longbeardovi.** Předání, po kterém nese pomalejší tělo, je záporný
  obchod i při 83% úspěchu.
* **Nikdy, když současný nosič rozvrh plní.** 17 % turnoveru za nic.
* **Nikdy z klece ven.** Nosič vyměněný mimo klec zahazuje celý tvar, který si
  klec vybudovala *(ČÁST 15: rozpočet těl)*.
* ⚠️ **Pozor na dvě povolenky, ne na jednu.** Od 17.08. jde v jednom kole
  přihrávka **i** předání — což je nová možnost i nová past: **dva hody na
  turnover v jednom kole.**

## 16.4 Záloha u míče musí být potenciální NOSIČ

> *Uživatel: „jako pomocník vedle míče měl přijít Blitzer — ať když Runner
> nezvedne, je on s AG3 a MA5 potenciální nosič."*

Doplňuje **S0** a **O8**: záloha se dnes vybírá podle **vzdálenosti**, ne podle
toho, jestli ten hráč umí míč donést. ⇒ **Záloha u míče je H1 předem** — kdo tam
stojí, ten se stane nosičem, když se něco pokazí. Longbeard u míče je proto
chyba i tehdy, když míč zvedne.

## 16.5 ⛔ DNEŠNÍ STAV: NABÍZÍ SE DESETKRÁT, ZAHRAJE SE JEDNOU ZA DVACET ZÁPASŮ

⚠️ **NEJDŘÍV OPRAVA ČÍSLA, KTERÉ SE TÁHNE PROJEKTEM.** První verze téhle části
tvrdila *„0 odehraných hand-offů ve 3 000 hrách"*. **To číslo bylo staženo
17.08.** — byla to **vada exportu logu**, ne vlastnost hry: `bb_module.cpp` měl
stráž `typeIdx < 21` proti 22 jménům, takže `HAND_OFF` (index 21) se ukládal
jako `UNKNOWN`. Opraveno `c943e8b8`.

| | |
|---|---|
| `HAND_OFF` nabídek v searchi | **10,4 na zápas** |
| **naše strana ZAHRÁLA** *(3 000 her)* | **130** *(≈ 0,043 na zápas)*, z toho 109 chyceno a 51 vedlo k TD |
| `PASS` odehraných | 0,30 na zápas |
| `CHAIN_SCORE` odehraných | 0 |

⛔⛔ **A DRUHÁ, HORŠÍ VĚC: NÁŠ DNEŠNÍ KORPUS TU OPRAVU NEMÁ.**
`corpus_baseline_20260817_data` se začal sbírat 17.08. v **10:15**, oprava
exportu je z **11:59** téhož dne (ověřeno 18.08.: `c943e8b8` **není** předkem
`5e5ab352`, na kterém korpus běžel). ⇒ **V dnešní baseline jsou hand-offy pořád
uložené jako `UNKNOWN` a JAKÉKOLI měření hand-offu nad ní neplatí.**
*(Ostatní dnešní měření to nezasahuje — σ-tabulka, odsuny, blitzy i rohy stojí
na `BLOCK`/`PUSH`/pozicích, ne na `HAND_OFF`.)*

⇒ **Práce na hand-offu je ZASTAVENÁ, dokud nevznikne korpus na opraveném
exportu** *(pravidlo 18.08.: [[feedback_fix_checks_before_measuring]] — spravit
kontroly vždy povýšit před měření)*. Tahle kapitola je **doktrína napsaná
dopředu**, ne závěr z dat.

⭐ **Co i tak platí:** nabízí se **10,4×** za zápas a zahraje se **0,043×** —
poměr **1 : 240**. Vada tedy opravdu leží ve **VOLBĚ**, ne v nabídce (**P5**:
filtr oceňuje předání cenou přihrávky 33 %, ačkoli by ho resolver provedl jako
hand-off 83 %; práh 0,5 zahodí i Runner→Runner na 44 %). Závěr se nemění, jen
přestává stát na nule.

## 16.6 Co je změřené a co ne

| | stav |
|---|---|
| Runner 3,41 vs Longbeard 1,50 pole/kolo | ✅ |
| hand-off 10,4 nabídek/zápas vs **130 odehraných** (0,043/zápas) | ⚠️ z korpusu **PŘED** opravou exportu; poměr 1 : 240 platí, absolutní počty se musí přeměřit |
| pass/hand-off mají od `f5998575` vlastní povolenky | ✅ ověřeno v kódu 18.08. |
| **jak často situace H1 vůbec nastane** *(nosič ≠ Runner a vedle volný Runner)* | ⛔ **BLOKOVÁNO na novém korpusu** — dnešní baseline exportuje `HAND_OFF` jako `UNKNOWN` |
| **cena H2 proti prostému dodge** — 17 % vs ~50 %, ale neměřeno na reálných pozicích | ⛔ NEZMĚŘENO |
| **`CHAIN_SCORE` po opravě povolenek** — příčina pryč, účinek nezměřen (P4) | ⛔ NEZMĚŘENO |

## 16.7 Kontroly

* **NOVÁ K40 — „nosič je správné tělo"**: podíl kol, kdy míč nese hráč, který
  **stihne rozvrh**. Přímé měření 16.0; dnešní ekvivalent neexistuje.
* **NOVÁ K41 — „záloha u míče je potenciální nosič"**: dnes 22,2 % kol má
  u míče zálohu vůbec (S5.3), ale nikdo neměří **kdo** to je.
* **K-handoff**: nabídnuto vs zahráno. Rozdíl mezi nulou a desítkou je celý
  obsah P5 — a kdyby se ta kontrola tiskla, P21 by se nemusela vymýšlet.

---

# ČÁST 17 — OBRANA „L": BOXING-IN (20.08.2026)

⛔ **Proč tahle kapitola vzniká až teď.** Uživatel 20.08. mluvil o
*„strategickém posunu zdi"*, který si vede jako **„L"**, a já odpověděl,
že L v naší spec nemáme. **Ve spec opravdu není — ale v paměti leží od
10.08.** Doktrína existovala deset dní a **nikdy se nedostala do
procedury**, takže se podle ní nikdy nic neměřilo ani nekontrolovalo.
Tahle kapitola to napravuje.

⚠️ **Obranná část spec byla dosud jen S1 (rozestavení na výkop) a S8
(zabraň skóre).** Mezi nimi zela díra přesně tam, kde trávíme většinu
soupeřových kol — v **S7 (boxing-in)**, což je podle měření 12.08.
**32,4 % kol** a jediné robustní číslo celého rozložení situací.

## 17.1 Co L je — a co není

**L není rozestavení ani screen. Je to ODEBÍRÁNÍ ÚNIKOVÝCH POLÍ.**

Cílem není odebrat míč, ale **vzít soupeři pohyb** a donutit ho k boji
nablízko, kde trpaslík s AV9, Block a Thick Skull vyhrává. Obklíčenému
zbývají dvě špatné možnosti: **dodge** (proti našemu Tackle často selže
⇒ pád, hod na brnění, turnover), nebo **probít se** (bloky proti AV9).

⇒ **Screen je to, co se hraje PROTI nám. Boxing-in je to, co hrajeme MY.**
Komunitní názvy: *Basing Up* · *Boxing-In* · styl *„Dwarf Meatgrinder"*.

## 17.2 Tvar: půlkruh L/U proti lajně

```
[ AUT ]  SOP1  |  SOP2  |
[ AUT ] -------|--------|-------
[ AUT ]  TRP1  |  TRP2  |  TRP3
[ AUT ]        |  TRP4  |
```
**TRP1/TRP3 diagonálně** jistí úniky do stran · **TRP2/TRP4 tlačí
zepředu** · **lajna dělá zadní stěnu ZADARMO**.

## 17.3 ⭐⭐⭐ L a U1 JSOU TÁŽ VĚC ZE DVOU STRAN

**U1** (ČÁST 15.5, 19.08.): *lajna je soupeřova asistence zdarma — bere
dva ze čtyř rohů geometricky a hlídá mu jednu stranu za nulový rozpočet*
⇒ **vlastní klec k lajně netlačit.**

**To je doslova popis L z druhé strany.** Co U1 zakazuje NÁM v útoku, to
L vnucuje SOUPEŘI v obraně. ⇒ **Není to dvojí doktrína, je to jedno
pravidlo o geometrii lajny**, a platí obousměrně.

⭐ A dědí i **změřený hák U1**: lajna **převádí DOSAH na POŠKOZENÍ**,
násobek podle max MA toho, kdo dosáhne (6→×1,31 · 7→×1,15 · 8→×1,62 ·
9→×1,89) ⇒ **L je nejcennější přesně proti rychlým a slabým, tedy elfům.**

## 17.4 ⭐⭐⭐ ROZHODNUTO UŽIVATELEM 20.08.: NENÍ TO VOLBA, JE TO FÁZE

> *„na začátku musí trpaslík začít s 2 sloupce a postupně to tlačit do L —
> jak mu poroste převaha"* **(uživatel 20.08.)**

**Sloupce a L si neodporují. Je to jeden postup ve dvou fázích, a přechod
mezi nimi řídí PŘEVAHA.**

| fáze | tvar | proč |
|---|---|---|
| **D1 — zdržuj** | **sloupce po dvou** napříč hřištěm, **kontakt NEDRŽET** | dokud nemáme převahu, kontakt soupeři dovolí vyrobit díru a protlačit se; sloupec drží i po blitzu — *„prvního blitznou, na druhého nemají rychlost ani obratnost"* |
| **D2 — zavři past** | **půlkruh L/U proti lajně**, kontakt DRŽET | s převahou už díra nevznikne rychleji, než ji zaceláme; teď se vyplatí brát únikové pole a nutit dodge proti Tackle |

⭐ **Přechod je POSTUPNÝ, ne překlopení** *(„postupně to tlačit do L")* —
sloupce se k soupeři přitlačují a zavírají do půlkruhu, jak převaha roste.
Není to `if (převaha) L else sloupce`.

✔ **Nezávisle to potvrzuje trpasličí příručka** (Goonhammer/King_Ghidra):
*„A good approach is often to play cautiously and hope to get a numbers
advantage, at which point you can be more aggressive."* Tentýž fázový model,
jiný zdroj.

## 17.4b OVĚŘENÍ NA WEBU (20.08.) — dvě třetiny sedí, jedna ne

*(rešerše: grumbbl „A Lean, Mean, Screening Machine" · Goonhammer „The Art of
Game Management" a „Dwarf Team Guide" · Exit 23 „Defense 101" · bbtactics)*

**✔ POTVRZENO — fáze D1 je standardní pojmenovaná formace, ne improvizace.**
*„Two-deep columns of players are the most fundamental screen… v column
defence se staví dvouřadé screeny napříč šířkou hřiště, **a nikdo nebasuje**."*
⇒ „sloupce po dvou bez kontaktu" je **column defence**, věc s vlastním jménem.

⭐ **A přišla GEOMETRIE, kterou jsme neměli** — a je implementovatelná:
* **1 pole od soupeře** ⇒ nutí ho **blitzovat**, ne jen blokovat *(blok chce
  sousedství; screen si ho schválně nedovolí)*;
* **2 pole mezi sebou** do šířky;
* **2 do hloubky** — a důvod je přesně ten anti-blitzový:
  *„i když jednoho z předních blitzneš a srazíš, nebo ho i odstraníš,
  stejně neprojdeš — kvůli obránci za ním."*
⇒ Rozpočet: **5 obránců pokryje šířku hřiště** a udrží *„až dvojnásobek
vlastního počtu"*.

**✔ POTVRZENO — přechod do basování existuje jako uznaný krok.**
*„Často si budeš muset vybrat kolo, kdy jdeš all-in — buď přepneš strategii
na **basování více soupeřů**, nebo utratíš rerolly na zlomovou akci."*

**⛔ NEPOTVRZENO — spouštěč „převaha" je DOKTRÍNA TRPASLÍKA, ne obecná.**
Jediný zdroj, který spouštěč váže na početní převahu, je **trpasličí**
příručka: *„hrát opatrně a doufat v početní převahu, a v tu chvíli můžeš být
agresivnější."* **Obecné zdroje mluví jinak:** Exit 23 dělí screen vs basování
podle **typu týmu** *(agility drží soupeře „na délku paže", bashers „basují
tak často, jak to jde")*, a jeho spouštěč je **stav zápasu**: *„když prohráváš
0:2 v poločase, opatrná obrana přestává být možnost."*

⛔ **A JEDNA VĚC SI PŘÍMO ODPORUJE:** uživatel řekl *„postupně to tlačit do
L"*, ale jediný zdroj, který o načasování mluví, říká **„vyber si kolo a jdi
all-in"** — tedy **jednorázové překlopení, ne postupné svírání**. To není
detail: postupné svírání znamená, že v mezifázi držíme **napůl rozbitý tvar**,
který nedělá ani jedno pořádně.
⇒ **Otevřená otázka na uživatele, ne na měření.**

⚠️ **A tenhle rozpor je pro nás ostřejší než pro kohokoli jiného:** trpaslík
je **basher**, takže obecná basher-doktrína („basuj, jak to jde") ho posílá
do kontaktu **hned** — kdežto jeho vlastní rasová příručka ho posílá čekat na
převahu. **Obojí je o téže rase a říká to opak.**

⛔ **CO ZŮSTÁVÁ OTEVŘENÉ — a bez toho se to nedá implementovat:**
**v čem se měří „převaha"?** Kandidáti, žádný zatím nerozhodnutý:
* **stojící těla** (nejlevnější, ale nepočítá kvalitu);
* **attrition** (KO/INJ — ale ta se projeví až se zpožděním);
* **poziční** (kolik jejich těl je za naší linií);
* **iniciativa** (kdo si může dovolit blok zdarma).
⚠️ **A ŽÁDNÁ KONSTANTA** ([[feedback_implement_the_rule_not_the_outcome]]):
prahem nesmí být zadrátované „+2 těla", musí to být pravidlo, ze kterého
trpasličí i jiný případ vypadne sám.

⭐⭐⭐ **A TŘETÍ FÁZOVÉ PRAVIDLO OD UŽIVATELE — engine fáze NEMÁ ŽÁDNÉ.**
Tohle je po **fázovém plánu trasy** (13.08.: sólo Runner → klec → sólo
výběh) a po **fázích útoku** už **třetí** doktrína, která je vnitřně
fázová. **Engine přitom nemá pojem fáze nikde** — ani v útoku, ani
v obraně. ⇒ *Bez fáze nejde odlišit chybu od záměru*
([[project_bloodbowl_phased_route_plan_20260813]]). **To přestává být
detail jedné kapitoly a začíná to být chybějící struktura modelu.**

## 17.5 Co past drží — a proč u nás teče

| skill | role | máme? |
|---|---|---|
| **Guard** | asistence i když sám markovaný | ✔ Longbeardi ×2, Blitzeři, Slayeři |
| **Tackle** | ruší Dodge ⇒ útěk je sebevražda | ✔ všichni Longbeardi |
| **Stand Firm** | ⭐ **klíč k UDRŽENÍ** — obklíčený hodí push back a náš hráč se nehne ⇒ past se neotevře | ⛔ **NIKDO** (jen Deathroller, ten v sestavě není) |
| **Mighty Blow** | až po zámku; převádí kontrolu na zranění | ⛔ **NIKDO** |

⛔ **A i kdybychom Stand Firm přidali, past teče:** chain push u nás
Stand Firmem **PROJDE** (`block_handler.cpp:122`), ač pravidla říkají
*„neither player moves"*; a Stand Firm je u nás **vynucený** místo
volitelného (`:62`). **To je přesně mechanismus, kterým se past otevírá.**

⚠️ **A Frenzy past otevírá sám:** je to **povinný** follow-up + druhý
blok, takže Slayera může vytáhnout z půlkruhu ven — a s AV8 stojí venku
hůř než kdokoli. ⇒ **Frenzy jen tam, kde druhý push končí surfem nebo
zpátky v tvaru.**

## 17.6 Co se má měřit — a co se měří místo toho

⛔ **Náš rozhodčí testoval obranu jako „stojí u nosiče někdo náš" → 70 %.
To je ale úplně jiná veličina než „nemá kam".**

⇒ **Chybějící metrika: počet volných polí, kam může nosič odejít** — a
jestli ho vytlačujeme k lajně. Bez ní si o obraně nic netvrdit.

## 17.7 Co tahle část NEZAKRYLA

* **17.4 je rozhodnutá** *(uživatel 20.08.)*, ale **„převaha" nemá
  definici** — a bez ní se fázový přechod nedá implementovat ani měřit.
* **Kdy se L zahajuje** (v kterém soupeřově kole) není nikde.
* **Rozpočet těl pro L** — kolik ze 7,03 volných těl past spolyká, se
  nikdy nepočítalo. ⚠️ Pozor: je to **obranný** rozpočet, tedy jiná kola
  než útočný (nosič 1 + rohy 4 + blitz 1 = 6 ze 7).
* **L proti Big Guyovi** — půlkruh proti ST5 nemusí držet.
