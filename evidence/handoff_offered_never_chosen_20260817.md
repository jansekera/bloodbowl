# ⛔⛔ TENTO ZÁVĚR JE CHYBNÝ — OPRAVENO TÝŽ DEN VEČER

**Nadpis níž tvrdí, že se hand-off nikdy nezahraje. NEPLATÍ.** Fable 17.08.
našel příčinu té nuly: **vada exportu logu**, ne chování enginu.
`bb_module.cpp:325` měl stráž `typeIdx < 21`, zatímco `eventNames` má **22**
položek ⇒ `HAND_OFF` (index 21) se zapisoval jako `"UNKNOWN"`. Commit
`3b11d33b` přidal jméno do tabulky a **stráž nezvedl**.

**Skutečnost:** v korpusu je **349 událostí `UNKNOWN` na 3 000 her** a všech 349
jsou hand-offy; **naše strana jich zahrála 130** (109 chyceno, 51 vedlo k TD
v témž kole). ⇒ **Hand-off žije.**

⚠️ **Jak se mi to stalo:** ověřoval jsem, že se event *emituje*
(`pass_handler.cpp:428`) a že *poziční mapa jmen sedí* (index 21 v obou) —
obojí platí. **Stráž o řádek výš jsem nezkontroloval.** A měl jsem to přímo
před sebou: ve vlastní tabulce typů událostí stálo `UNKNOWN 349 0,12/hru`
a šel jsem kolem. **Ověřil jsem dvě správné věci a tu třetí ne.**

✅ Stráž opravena (velikost se bere z pole, ne konstantou).
⚠️ `corpus_baseline_20260817` se sbíral **před** opravou ⇒ i v něm je hand-off
uložený jako `UNKNOWN`.

**Co z původního textu PLATÍ dál:** trychtýř brány (329 kol ze 3 000 her),
příčiny, proč se nabídka zahazuje, i rozdělení Q1 sweepu na `HAND_OFF`/`PASS`.
**Neplatí závěr (c) „nabízí se, ale nevybere se".**

---

# P21 — HAND-OFF SE NABÍZÍ 10,4× ZA ZÁPAS A NEVYBERE SE ANI JEDNOU
*(17.08.2026; navazuje na `evidence/weekend_result_20260817.md` §3)*

## Otázka

Ve 3 000 hrách korpusu 14.08. je `HAND_OFF` událostí **nula**, přestože
logování bylo přidáno právě proto (`3b11d33b`) a Q1 sweep dával na postavených
pozicích 18,3 %. Fronta si vymínila ověření *„kdyby nefungovalo, nesmí být
v baseline"* — neověřilo se. Byla dvě čtení, **obě padla**, a odpověď je třetí.

## (a) „Situace nenastává" — PADÁ

`diag_handoff_situation_20260817.py` počítá **přesně tu bránu, která se
provádí** (`macro_actions.cpp:673+`), krok po kroku, na snímcích začátku
našeho kola:

| krok | kol | % kol s míčem |
|---|---|---|
| držíme míč, nosič může jednat | 19 905 | 100 % |
| nosič má **špatné ruce** (AG ≤ 2, bez Sure Hands) | 1 489 | 7,48 % |
| stojí vedle něj spoluhráč | 1 049 | 5,27 % |
| soused má **lepší ruce** | 577 | 2,90 % |
| a není dál od endzony | 362 | 1,82 % |
| chytí to aspoň na 50 % ⇒ **NABÍDKA PROJDE** | **329** | **1,65 %** |

**329 kol ve 3 000 hrách** — 0,69 % všech našich kol, **7,3 % zápasů** má
aspoň jedno. Vzácné, ale ne nulové. ⚠️ **Je to PODLAHA:** snímek je začátek
kola, situace vzniklá až během kola se nezapočítá.

Ověřeno i na **čerstvě přeložené binárce** (120 nových her): 16 kol, 2,12 %
kol s míčem — tedy to není artefakt staré `.so`.

**Co bránu zabíjí** *(z kol, kde má nosič špatné ruce)*: soused má taky špatné
ruce 31,7 % · vedle nikdo nestojí 29,6 % · lepší ruce jsou jen dozadu 14,4 % ·
cíl v tackle zónách 2,2 %.

**Kdo nese špatně:** Longbeard +Guard 38,5 % · Troll Slayer 33,6 % ·
Longbeard 27,9 %. *(Runner nese 83,6 % kol, Blitzer 8,9 %.)*

## (b) „Log tu cestu nepokrývá" — PADÁ

`resolveHandOff` (`pass_handler.cpp:428`) event **emituje**, `GameEvent::Type`
ho má a python export `bb_module.cpp:233` má poziční mapu **správně**
(`HAND_OFF` na indexu 21 v obou). Log je v pořádku.

## ⛔ A jedna má vlastní vada nástroje, kterou jsem našel cestou

Q1 sweep počítal `act.type == HAND_OFF **||** PASS` **do jednoho čísla** —
těch „18,3 % předání", kvůli kterým se hand-off vrátil na živý seznam, byl
součet dvou různých akcí. Rozdělil jsem to (`diag_q1_sweep_20260814.cpp`):

| | HAND_OFF | PASS |
|---|---|---|
| 432 pozic, 36 geometrií | **79 (18,3 %)** | **0 (0,0 %)** |

⇒ **Těch 18,3 % je opravdu předání.** Moje podezření na vadný nástroj bylo
mylné — ale nástroj to číslo dosud tvrdit nemohl.

## ⭐ (c) ODPOVĚĎ: NABÍZÍ SE, ALE SEARCH SI HO NEVYBERE

Přidáno počítadlo `takeHandOffOfferCount()` (týž nástroj, který rozhodl P13) —
zvyšuje se tam, kde se `PASS_ACTION` se sousedním cílem vkládá do nabídky.
**50 skutečných her `dwarf–skaven`:**

| | |
|---|---|
| hand-off **nabídnut** | **519× = 10,4 na zápas** |
| hand-off **zahrán** | **0** |

*(Počítadlo počítá nabídky uvnitř prohledávání, ne jednou za kolo — proto je
10,4 víc než 0,11 kola/zápas ze snímků. Na otázku „dostane se to na menu?"
to odpovídá bez výhrad.)*

⇒ **Šestý výskyt vzorce, ale posunutý o úroveň.** U P5, P9, P13, P14, P15 šlo
o *„filtr oceňuje jinou akci, než jakou resolver provede"*. Tady je filtr
**opravený a funkční** — a přesto se akce nehraje. **Vada se přesunula
z NABÍDKY do VOLBY**, přesně jako u P13 (*„nabídka stoupla, volba se
nezměnila"*).

## Co z toho plyne

* **P5 je opravené na úrovni nabídky a mrtvé na úrovni volby.** Tři commity
  z 14.08. (`38dcad6d`, `4f2c658d`, `3b11d33b`) udělaly svou práci; **zisk
  z nich je zatím nula** a v baseline sedí jako neúčinná změna.
* **Rozdíl proti postavené pozici je ta informace.** Na planted pozici, kde
  Longbeard nemá nic lepšího na práci, se předání vybere v 18,3 %. V reálném
  kole má search 11 těl a ~60 povinností — a předání prohraje **vždy**.
  ⇒ Chyba nebude v bráně, ale v **listové evaluaci / prioru**: nic v hodnocení
  neříká, že *míč v rukou AG2 těla je sám o sobě drahý*.
* ⇒ **Patří to k P15 / P10a** (plochý prior, cena cíle), ne k P5. **P5 se
  uzavírá jako hotové; nový úkol je „proč se to nevybere".**
* ⚠️ **Nezapínat na to další rameno naslepo:** strop je malý — 329 kol ve
  3 000 hrách. I kdyby se předání vybíralo pokaždé, je to ~0,1 akce na zápas.
  **Spočítat strop napřed**, jak žádá pravidlo z 14.08.
