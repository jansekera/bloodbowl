# FABLE AUDIT — RULES-PARITY: ODVOZUJEME VŠECHNO OD BLOKU? (21.08.2026)

Zadání: `evidence/fable_brief_rules_parity_20260821.md`. Edice **BB2016**
(`rules_bb2016.txt`, čísla řádků odtud). Čísla z korpusu jsou ze
`corpus_baseline_20260819_data` (400 her, **starý engine bez P45** — ilustrace
mechanismu, ne platné hodnoty).

---

## 1. ⏰ ODPOVĚĎ NA PRVNÍ OTÁZKU: ANO — našel jsem vady, které běžící korpus znehodnocují

Tři pravidlové vady (mění chování, ne jen čtení), všechny živé v binárce, na
které právě běží sběr. Řazeno podle odhadu dopadu:

### F1 ⛔⛔ OMRÁČENÍ SE PROBÍRÁ O CELÉ KOLO DŘÍV (nová, největší)

* **Pravidlo (ř. 703-710):** *„Stunned – … All face-down players are turned
  face up **at the end of their team's next turn**, even if a turnover takes
  place. Note that a player may not turn face up on the turn they are
  Stunned."* — omráčený leží obličejem dolů CELÉ své příští kolo, otočí se
  (= PRONE) až na jeho KONCI a vstávat smí až kolo poté.
* **Náš kód:** `game_state.cpp:58-62` (`resetPlayersForNewTurn`): STUNNED →
  PRONE **na ZAČÁTKU vlastního kola** (volá `turn_handler.cpp:24` při každém
  střídání). Hráč se tedy může postavit a jednat **o jedno celé vlastní kolo
  dřív**, než pravidla dovolují. Žádný `stunnedThisTurn` mechanismus v kódu
  neexistuje (ověřeno grepem).
* **Dopad:** starý korpus: **6,2 omráčení/hru** (INJURY 2-7; 75 % hodů na
  zranění) — a po opravě P45 jich bude víc (víc bloků). Každé омráčení daruje
  oběti jednu aktivaci navíc. Je to **táž měna jako P45** (těla na nohou),
  a P45 pohnulo TD o 40 %. Oprava = nová základní čára ⇒ **korpus sbíraný teď
  umře, až se to opraví**.
* Pozn.: souvisí s F1 i formulace ř. 390-391 (po turnoveru se otáčí na konci
  kola) — konzistentní s „konec kola", ne „začátek".

### F2 ⛔⛔ FAUL: dvě vady, jen jedna z nich je známé P54

* **F2a (= P54, známé, NEOPRAVENÉ):** `helpers.cpp:173` — `countAssists` dává
  Guardovi asistenci vždy; volá ji `foul_handler.cpp:19-22`.
  **Ř. 8160-8161:** *„This skill may not be used to assist a foul."* a
  **ř. 1849-1852:** *„No player from either side may assist a foul if they are
  in the tackle zone of an opposing player, do not have their tackle zones, or
  are not standing."* Guard má trpaslík 6 a ork 6 (human 2, skaven 1, WE 1 —
  Treeman). Fauly: **6,96/hru, v každé hře** ⇒ soustavně nadhodnocené armour
  hody u faulů Guard-těžkých ras, oběma směry (útočné i obranné asistence).
  * ⚠️ **interpretační otázka k rozhodnutí:** text faulu NEMÁ výjimku „kromě
    blokovaného" jako blok. Čteno doslova nemůže obrannou asistenci u faulu
    dát NIKDY nikdo (obránce vedle faulujícího je z definice v jeho TZ).
    Komunitní čtení vyjímá faulujícího/oběť. Rozhodnout před opravou —
    doslovné čtení = obranné asistence u faulu prostě smazat.
* **F2b (NOVÁ): vyloučení se nekontroluje na hodu na zranění.**
  **Ř. 1878-1882:** *„if the **Armour and/or Injury roll** is a doubles …
  the player taking the Foul Action is sent off … his team suffers a turnover."*
  Náš kód (`foul_handler.cpp:34` + `:65`) kontroluje dublety **jen na armour
  kostkách**; hod na zranění je delegován do sdíleného `resolveInjuryRoll`
  (`foul_handler.cpp:59`), který o faulovém pravidlu dublet **neví** a kostky
  nevrací. Starý korpus: 36,3 % faulů prorazí zbroj ⇒ chybí
  **≈ 0,42 vyloučení + turnoverů na hru** (dnes 1,09/hru, mělo by být ~1,5).
  Faulování je u nás o ~28 % levnější než v pravidlech — a je to přesně ten
  šev, kde faul přebírá blokovou mašinérii (injury path) a faul-specifické
  pravidlo se na něm ztratí.

### F3 ⛔ FRENZY: druhý blok i po BOTH DOWN, kde nikdo nespadl

* **Pravidlo (ř. 8138-8140):** *„If a **'Pushed' or 'Defender Stumbles'**
  result was chosen, the player must immediately throw a second block…"* —
  druhý blok JEN po odsunu.
* **Náš kód:** `block_handler.cpp:789-791` — podmínka je jen „oba stojí a jsou
  sousedé" ⇒ po BOTH_DOWN, kde oba mají Block (nic se nestalo), hodíme
  **ilegální druhý blok zdarma**. Nositel: trpasličí Troll Slayer
  (Block+Frenzy+Dauntless, 2 ks) — vada **nadržuje trpaslíkovi**, tedy
  subjektu většiny našich měření. Četnost malá (BD zvolen ~1/36 u 2 kostek),
  ale směr je systematický.

**Menší pravidlové vady rovněž živé v korpusu** (každá malá, dohromady měřitelné):

* **F4 Dodge reroll bez limitu 1/kolo** — ř. 8089-8090: *„may only re-roll
  **one** failed Dodge roll per turn."* `attemptRoll` (`helpers.cpp`) žádné
  per-turn počítadlo nemá (existuje jen `proUsedThisTurn` — postavené pro
  Pro). 3,9 % kol má hráče se 2+ dodge ⇒ nadržuje obratným (WE/human/skaven).
* **F5 Sure Feet bez limitu 1/kolo** — ř. 8541-8542. Totéž místo v kódu.
  5,3 % kol má hráče se 2+ GFI; nositelé: 4 gutter runneři skavena.
* **F11 Wrestle: chybí turnover, když je sražen VLASTNÍ nosič** —
  ř. 8677-8678: *„Use of this skill does not cause a turnover **unless the
  active player was holding the ball**."* `block_handler.cpp:602` vrací
  `ok()` bezpodmínečně. Vzácné (blok nosičem je doktrinálně zákaz), ale je to
  pravidlo zadarmo.

**Doporučení:** máte-li restartovat jen jednou, **zabít sběr a opravit F1 +
F2a + F2b + F3 + F4 + F5 + F11 najednou** (všechno malé, lokalizované opravy)
— a vzít s sebou i F6-F10 níže, ať příští baseline nepadne na pass ekonomice.

---

## 2. VERDIKT K HYPOTÉZE: je to TŘÍDA, ne dvě náhody — a má tři mechanismy

Hypotéza „všechno se odvozuje od bloku" je směrově správná; přesnější tvar:
**tvary postavené pro první implementované akce (blok, binární hod) se
recyklují i tam, kde pravidla mají jiný tvar — a schopnosti mimo jádro
MOVE/BLOCK/BLITZ jsou druhořadé na všech vrstvách** (nabídka, plánovač, log).

**Mechanismus A — sdílený pomocník nese pravidlo, které u jiné akce neplatí:**
1. `countAssists` + Guard u faulu (F2a) — doložený případ z briefu.
2. `resolveInjuryRoll` u faulu (F2b) — injury path je bloková; faulová dubleta
   se na švu ztratí. **Nový člen téže rodiny.**
3. `attemptRoll` je binární (práh/úspěch) — pass má TŘI výsledky
   (přesná/nepřesná/fumble) a při skládání do binárního tvaru se ztratily:
   * **F6:** Pass skill reroll na NEPŘESNOU přihrávku (ř. 8335-8337: *„re-roll
     … if he throws an **inaccurate pass** or fumbles"*) — u nás se reroll
     řetěz (skill/Pro/team) spouští **jen na přirozenou 1**
     (`pass_handler.cpp:275`); nepřesná přihrávka se nikdy nererolluje, ani
     týmovým rerollem.
   * **F7:** fumble má být **modifikovaný výsledek ≤ 1** (ř. 1742-1745:
     *„1 or less **before or after modification**"*) — u nás jen přirozená 1
     (modifikátory jsou složené do cíle, čímž fumble práh zmizel).
4. `attemptRoll` nemá per-turn účetnictví skillů (F4 Dodge, F5 Sure Feet) —
   jediný per-turn stav je `proUsedThisTurn`, postavený pro Pro.

**Mechanismus B — akce, kterou plánovač/nabídka nevidí (třída „vstávání"):**
5. **F12 LEAP je mrtvý kód.** `resolveLeap` (`move_handler.cpp:209`) je plně
   implementovaný (a pravidlově správně, ř. 8270-8283 sedí) — ale **nemá
   žádného volajícího**: žádný `ActionType`, nic v `getAvailableActions`,
   žádné makro. Oba wardanceři (WE) mají Leap a **nikdy v žádné hře
   neskočili** — přesně jako vstávání před P45 se to z kódu nepozná a z logu
   taky ne (nula LEAPů nejde odlišit od „neleapuje se").
   * Pozn. do stejné třídy: JumpUp po postavení nastaví `hasMoved` ⇒ blok po
     Jump Up (celý smysl skillu, CRP i BB2016) nejde zahrát — v korpusu ale
     JumpUp nikdo nemá.

**Mechanismus C — recyklovaná ŠABLONA jiné mechaniky:**
6. **F8:** nepřesná přihrávka se rozptylové řeší **kickoffovou šablonou**
   (D8 směr × D6 vzdálenost, `pass_handler.cpp:363-365`) místo **tří
   rozptylů po jednom poli** (ř. 735-741; ř. 269-273 výslovně: D6 vzdálenost
   je JEN pro výkop). Vlastní Hail Mary přitom 3×1 rozptyl má — kód si
   protiřečí. Důsledek: nepřesná přihrávka letí 1-6 polí jedním směrem,
   nikdy se nevrátí k příjemci, častěji opustí hřiště.
7. **F9:** throw-in (`ball_handler.cpp:186-205`): (a) dopad na stojícího
   hráče má být CHYCENÍ (ř. 871-874), u nás se VŽDY hází bounce z dopadového
   pole; (b) míč letící znovu ven se má **znovu vhodit** (ř. 875-877), u nás
   se **klampuje** na okraj hřiště (`:193-197`) — a klamp x i y nezávisle umí
   míč posadit i mimo dráhu hodu.
8. **F10:** výkop dopadající na prázdné pole se má **jednou odrazit**
   (ř. 276-278) — `simpleKickoff` (kterým běží korpusy,
   `game_simulator.cpp:544-552`) i `resolveKickoff` odraz vynechávají; a
   rozptyl výkopu ven z hřiště má být touchback (ř. 279-281), u nás klamp.

**Mimo hypotézu, ale stejné důležitosti:** F1 (stun) není odvozený od bloku —
je to vada HRANICE (stav se překlápí na začátku kola místo na konci). Třída
„špatná hranice přechodu" má jednoho člena, „blokový tvar" jich má sedm.

**Co hypotézu NEPOTVRDILO (prověřeno a je to v pořádku):** Dauntless
(pre-assist, správně), Tackle (dodge i blok), Thick Skull (deterministická 8),
Mighty Blow (jeden hod, chytrá volba), Stand Firm (i v řetězu), Fend,
Juggernaut (SF+Fend+Wrestle na blitzu), Side Step/Grab (vzájemné rušení,
Grab jen na Block), blitz = blok + 1 MP + GFI pravidla, hand-off +1 a vlastní
alokace, pickup/catch/interception modifikátory, injury tabulka, KO recovery,
kazualty perzistence, crowd surf (nosič → throw-in), turnover katalog krom F11.

---

## 3. TABULKA NÁLEZŮ

| # | Co | Pravidlo (rules_bb2016.txt) | Náš kód | Odhad dopadu | Znehodnocuje korpus? |
|---|---|---|---|---|---|
| F1 | Stun se probírá o kolo dřív | ř. 703-710 | `game_state.cpp:58-62`, `turn_handler.cpp:24` | 6,2+ stunů/hru, každý +1 aktivace navíc; měna P45 | ⛔⛔ ANO |
| F2a | Guard asistuje faulu (P54) | ř. 8160-8161, 1849-1852 | `helpers.cpp:173` ← `foul_handler.cpp:19-22` | 6,96 faulů/hru; dw/orc 6 Guardů | ⛔ ANO |
| F2b | Bez vyloučení za dublet na injury | ř. 1878-1882 | `foul_handler.cpp:34,59,65` | ~0,42 chybějících vyloučení+turnoverů/hru | ⛔ ANO |
| F3 | Frenzy 2. blok po BD-nic | ř. 8138-8140 | `block_handler.cpp:789-791` | malé, systematicky pro-trpaslík | ⚠️ přispívá |
| F4 | Dodge reroll bez 1/kolo | ř. 8089-8090 | `helpers.cpp` attemptRoll (bez per-turn stavu) | 3,9 % kol s 2+ dodge | ⚠️ přispívá |
| F5 | Sure Feet bez 1/kolo | ř. 8541-8542 | totéž | 5,3 % kol s 2+ GFI (skaven) | ⚠️ přispívá |
| F11 | Wrestle: chybí turnover nosiče | ř. 8677-8678 | `block_handler.cpp:602` | vzácné | ⚠️ okrajové |
| F6 | Pass reroll jen na přirozenou 1 | ř. 8335-8337 | `pass_handler.cpp:275-346` | pass 0,29/hru dnes; deformuje pass ekonomiku | ne přímo — deformuje CHOVÁNÍ |
| F7 | Fumble jen na přirozenou 1 | ř. 1742-1745 | tamtéž | opačný směr než F6 | ne přímo |
| F8 | Nepřesný pass = kickoff šablona | ř. 735-741, 269-273 | `pass_handler.cpp:363-365` | každý nepřesný pass | ne přímo |
| F9 | Throw-in: bez chycení, klamp | ř. 871-877 | `ball_handler.cpp:186-205` | část z 6,6 bounce/hru | malé |
| F10 | Výkop: bez odrazu, bez touchbacku | ř. 276-281 | `game_simulator.cpp:544-552`, `kickoff_handler.cpp:229-231` | ~3,5 výkopů/hru, ±1 pole | malé, symetrické |
| F12 | Leap mrtvý kód (třída vstávání) | ř. 8270-8283 | `resolveLeap` bez volajícího | WE bez klíčového nástroje | ne — konstantní absence |
| F13 | Kickoff tabulka (Riot jednostranný marker; Blitz! = šoupnutí o pole místo volného kola; Rock jen stun) | ř. 1284-1296, 1330-1345 | `kickoff_handler.cpp:60-180` | **SPÍCÍ** — korpus běží na `simpleKickoff` | ne (mrtvá cesta) |

Pozorovací (batchovatelné kdykoli): BLITZ deklarace se neloguje (blitzy se
rekonstruují heuristicky); asistence u faulu se nelogují (nezměříme skutečnou
velikost F2a po opravě); LEAP nemá event typ.

**Edice:** všech 13 nálezů je shodně vadných proti CRP i BB2016 — texty
Guard/faul/dublety/pass/Frenzy/Dodge/Leap jsou v obou edicích totožné
(ověřeno i v `rules_crp_lrb6.txt`). Žádný z nálezů tedy nestojí na volbě
edice; otevřená otázka P45 (postavení v rámci BLOCK akce) tímto auditem
rozhodnuta není.

---

## 4. POŘADÍ OPRAV

Všechno F1-F11 jsou **pravidlové** změny (mění chování ⇒ nová základní čára).
Když se restartuje jen jednou, patří do JEDNOHO balíku před restart:

1. **F1** stun na konci vlastního kola (potřebuje per-hráč flag „stunnedThisTurn"
   nebo flip při resolveEndTurn odcházejícího týmu) — největší dopad.
2. **F2a + F2b** faul: vyhodit Guard větev pro faul (nový parametr
   `countAssists` nebo faul-specifická funkce) + vrátit kostky z
   `resolveInjuryRoll` volajícímu / předat flag „jsme faul". Před tím
   rozhodnout interpretaci obranných asistencí (viz F2a ⚠️).
3. **F3** Frenzy: podmínku rozšířit o „výsledek byl Pushed/DS" (chosen face).
4. **F4 + F5** per-turn flagy dodgeRerollUsed/sureFeetUsed (reset v
   `resetPlayersForNewTurn`).
5. **F11** Wrestle: turnover, když aktivní strana držela míč.
6. **F6 + F7 + F8** pass ekonomika — dnes skoro nehraje (0,29 passů/hru), ale
   oprava změní i CHOVÁNÍ search (pass zlevní o reroll a zpřesní rozptyl) ⇒
   udělat teď, ať to nerozbije příští baseline.
7. **F9 + F10** throw-in/výkop — malé, symetrické, ale levné; do balíku.
8. **F12 Leap** — NENÍ oprava, je to nová schopnost (nabídka + makro + ocenění
   rizika). Samostatné rozhodnutí a vlastní A/B; nedávat do balíku mlčky.
9. **F13 kickoff tabulka** — spící (korpus ji nespouští); opravit, až se
   někdy zapne `useFullKickoff`.

Pozorovací (kdykoli, bez restartu): logovat BLITZ deklaraci, faul asistence,
LEAP event.

---

## 5. CO NEJDE ROZHODNOUT ČTENÍM (zadání pro měření, ne závěry)

1. **Velikost F1** — párové A/B (oprava vs. baseline) na dw-we, stejný tvar
   jako P45/P38: kolik TD/hru a kolik těl na nohou to pohne. Predikce: směr
   „míň stojících", velikost řádu P45 nejde slíbit.
2. **Velikost F2a** — po opravě přeměřit armour distribuce faulů podle ras;
   dnes to z logu nejde (asistence se nelogují).
3. **Zda F4/F5 stojí za σ** — četnost druhého rerollu v kole je součin malých
   čísel; změřit na novém korpusu počtem SKILL_USED Dodge/SureFeet na kolo.
4. **Pass kaskáda (F6-F8)** — jestli po zlevnění passů search začne házet;
   to je behaviorální otázka pro A/B, ne pro čtení.
5. **Hodnota Leapu (F12)** — vyžaduje implementaci nabídky; teprve pak jde
   měřit, jestli wardancer s Leapem láme klece, jak tvrdí doktrína.
6. **Interpretace obranných asistencí u faulu** (F2a ⚠️) — volba doktríny
   (doslovný text vs. komunitní čtení), patří do fronty rozhovoru.
