# ZADÁNÍ — PONDĚLNÍ AUDIT PARITY PHP × C++ (první, 29.09.2026)

## 0. Proč

Uživatel 25.09.: *„kontrolu, jestli PHP a C++ jsou stejné — naplánuj pravidelné kontroly.“*
Máme DVA enginy: **PHP** (`src/`, v něm běží práce na pravidlech) a **C++** (`engine/src/`, v něm běží měření a trénink).
Od posledního auditu parity (24.08.) se pravidla opravovala **skoro jen v PHP** — 25.09. se takhle našly
dvě mezery, které měl uživatel za vyřešené: `holdsGround` (E18–E19) a **pohyb po bloku v Blitzu**
(C++ `771d1ecc` 07.09., PHP nikdy — E28).

## 1. Úkol

Pro **KAŽDÝ** commit v tabulce níž zjisti: **má druhý engine totéž chování?**
- commit v **PHP** ⇒ najdi odpovídající místo v `engine/src/` (C++) a rozhodni.
- commit v **C++** ⇒ najdi odpovídající místo v `src/` (PHP) a rozhodni.
- K tomu **C++ commity bez slova PRAVIDLA od 24.08.**, které mění chování hry (`git log --since=2026-08-24 -- engine/src/`) — projdi je stejně, jen stručně.

Postup u každého: `git show <hash>` → co přesně se změnilo a proč → najdi totéž v druhém enginu (`grep`, čtení) →
verdikt. **Nečti jen název commitu** — ten bývá zkratka.

Verdikt je jeden z:
- **SHODNÉ** — druhý engine se chová stejně (uveď soubor:řádek, kde to vidíš).
- **CHYBÍ** — druhý engine má staré / jiné chování (uveď soubor:řádek a v čem se liší).
- **NEMÁ SMYSL** — druhý engine danou věc vůbec nemá (skill, akce, událost neexistuje) — i to je nález, napiš co chybí.
- **NEJDE ROZHODNOUT ČTENÍM** — napiš, jaký krátký test by to rozhodl.

Pravidlo, když je potřeba: `rules_bb2016.txt` (hrajeme **BB2016**, ne CRP) — cituj číslo řádku.

## 2. Commity k projití

| commit | datum | engine | název |
|---|---|---|---|
| `9e980cac` | 2026-09-25 | PHP | PRAVIDLA: Blood Lust -- krmeni az na konci akce, Block -> Move (balik E, E25) |
| `966a871c` | 2026-09-25 | PHP | PRAVIDLA: Regeneration az PO lekarnikovi a jen jednou (balik E, E26) |
| `60ba5961` | 2026-09-25 | PHP | PRAVIDLA: Hypnotic Gaze -- Agility roll a konec efektu dalsi akci (balik E, E21+E22) |
| `aba4e078` | 2026-09-25 | PHP | REFAKTOR+PRAVIDLA: odtlaceni vc. retezu na jednom miste (BlockHandler::odtlacit) |
| `3cec72e2` | 2026-09-25 | PHP | PRAVIDLA: kdo drzi pole -- Take Root a Stand Firm (balik E, E18, E19, E20b) |
| `04e4946c` | 2026-09-25 | PHP | PRAVIDLA: Ball & Chain -- blok podle sily, lezici odtlacit + brneni, follow-up (balik E, E14+E15) |
| `e373bf23` | 2026-09-25 | PHP | PRAVIDLA: Ball & Chain -- sablona vhazovani, dav, zraneni bez brneni (balik E, E12+E13+E16) |
| `d9910a96` | 2026-09-25 | PHP | PRAVIDLA: Bombardier -- sedm rozporu s pravidly (balik E, E10) |
| `ced20b99` | 2026-09-25 | PHP | PRAVIDLA: Always Hungry -- dva hody, smrt, rozptyl z pole obeti, fumble (balik E, E9) |
| `26508221` | 2026-09-25 | PHP | PRAVIDLA: No Hands nezachytava prihravky (balik E, E8) |
| `926b4a7a` | 2026-09-25 | PHP | PRAVIDLA: Secret Weapon -- vylouceni kazdeho, kdo v drivu hral (balik E, E7) |
| `fa2422ad` | 2026-09-25 | PHP | PRAVIDLA: pila nejde s Multiple Block (balik E, E3) |
| `78bf7601` | 2026-09-25 | PHP | PRAVIDLA: faul pilou -- +3 k brneni a hod na zpetny raz (balik E, E4) |
| `9e02a834` | 2026-09-25 | PHP | PRAVIDLA: Chainsaw -- nositel pily srazeny => souper +3 k brneni (balik E, E5) |
| `0003267e` | 2026-09-25 | PHP | PRAVIDLA: Chainsaw -- +3 k brneni zasazeneho a turnover pri zpetnem razu (balik E) |
| `04a5d0f7` | 2026-09-24 | PHP | PRAVIDLA: Sweltering Heat -- D6 za kazdeho hrace na konci drivu (balik G, 4/4) |
| `5f9cb83a` | 2026-09-24 | PHP | PRAVIDLA: na hriste smi jen 11 hracu, zbytek je lavicka (balik G, 3/4) |
| `266d7ae4` | 2026-09-24 | PHP | PRAVIDLA: tabulka nasledku D68 -- smrt uz muze nastat (balik G, 2/4) |
| `51e99c83` | 2026-09-24 | PHP | PRAVIDLA: surf -- "Stunned" jde do REZERV, ne na hriste (balik G, 1/4) |
| `01bdfd77` | 2026-09-21 | PHP | PRAVIDLA: TTM -- -1 k hodu, jen quick/short a fumble na PUVODNI pole |
| `ebee6362` | 2026-09-21 |  C++ | PRAVIDLA (C++): po neuspesnem Pro jde tymovy prehoz na HOD PRO, ne na kostky bloku |
| `6a6537ef` | 2026-09-21 | PHP | PRAVIDLA: Dirty Player je VOLBA zbroj/zraneni; Break Tackle jen 1x za kolo a jen kdyz pomuze |
| `a7603e8f` | 2026-09-21 | PHP | PRAVIDLA: tymovy prehoz u chytani po odskoku (vcetne prehozu HODU PRO) |
| `1657a19e` | 2026-09-21 | PHP | PRAVIDLA: TTM -- presny i nepresny hod rozptyluje hrace TRIKRAT |
| `196813c8` | 2026-09-21 | PHP | PRAVIDLA: faul -- dublet se cte i na hodu na ZRANENI, nejen na brneni |
| `c4d082e1` | 2026-09-21 | PHP | PRAVIDLA: Pro u chytani po odskoku a vhazeni; prehoz z Catch uz neni potichu |
| `ec3feb2c` | 2026-09-21 | PHP | PRAVIDLA: hozeny NOSIC v davu vrati mic vhazenim, misto aby mic zmizel |
| `29d421c4` | 2026-09-21 | PHP | PRAVIDLA: Dodge prehodi jen JEDEN neuspesny uhyb za kolo |
| `cebf9703` | 2026-09-18 | PHP | PRAVIDLA: throw-in sablonou + 2D6, chytani/odskok, znovu od posledniho pole; nosic v davu a odskok na leziciho |
| `5fabdd12` | 2026-09-18 | PHP | PRAVIDLA: interaktivni prehoz jen z nabidky; Hail Mary zapise tymovy prehoz |
| `84f7dc46` | 2026-09-18 | PHP | PRAVIDLA: Pro 1x za kolo a kostka nejvys 1x prehozena; Sure Feet 1x za kolo |
| `cd2f72fe` | 2026-09-18 | PHP | PRAVIDLA: nepresna prihravka se rozptyluje 3x po jednom poli, ne 1 smer x min(3, D6) |
| `d0c6e1d7` | 2026-09-16 | PHP | PRAVIDLA: faul -- asistence misto pausalniho +1, a vylouceni je turnover |
| `f58eecb1` | 2026-09-16 | PHP | PRAVIDLA: tymove prehozy se o polocase vraci na vychozi pocet |
| `e278c6a2` | 2026-09-16 | PHP | PRAVIDLA: hod na navrat KO hracu i po touchdownu, nejen o polocase |
| `d9c91cc3` | 2026-09-16 | PHP | PRAVIDLA: vytlaceni do publika nema zadny modifikator zraneni (+1 odstraneno) |
| `8623a6b5` | 2026-09-15 | PHP | PRAVIDLA: tri kostky bloku az pri VICE nez dvojnasobne sile, ne pri dvojnasobku |
| `a02b6ec9` | 2026-09-15 | PHP | PRAVIDLA: Thick Skull byl podle jine edice -- v BB2016 je to "modifikovana 8 = Stunned", bez kostky navic |
| `d674d080` | 2026-09-15 | PHP | PRAVIDLA: tabulka vykopu -- vysledky 7 a 8 byly prohozene (Changing Weather / Brilliant Coaching) |
| `354a7dda` | 2026-09-15 | PHP | PRAVIDLA: tabulka pocasi -- pekne pocasi padalo v 16/36 hodu misto 30/36, vanice 3x casteji |
| `d852dcd6` | 2026-09-15 | PHP | PRAVIDLA: ve vanici jen quick a short prihravky (a Hail Mary vubec) |
| `9c60e468` | 2026-09-15 | PHP | PRAVIDLA: Disturbing Presence pusobi i lezici a omraceny |
| `e81bb57d` | 2026-09-15 | PHP | PRAVIDLA: fumble prihravky je "1 nebo mene PO modifikaci", ne jen prirozena 1 -- a Safe Throw si mic udrzi |
| `faef31f3` | 2026-09-15 | PHP | PRAVIDLA: Safe Throw -- hazi HAZEC na svou AG, ne zachycujici znovu; Very Long Legs ho vypina |
| `534c9e40` | 2026-09-15 | PHP | PRAVIDLA: intercepce -- chybely zony, dest, Disturbing Presence, Extra Arms, Nerves of Steel a podminka zony |
| `4c0e5240` | 2026-09-15 | PHP | PRAVIDLA: zvedani mice -- Nerves of Steel tam nepatri, Big Hand ignoruje i dest |
| `8cf8e316` | 2026-09-15 | PHP | PRAVIDLA: pocasi davalo postih tam, kam nepatri -- dest a vanice u prihravky, vanice u chytani a zvedani |
| `afc015c3` | 2026-09-14 | PHP | PHP37 (1. krok) + PRAVIDLA: skill Pass se pocital dvakrat |
| `dea6d70c` | 2026-09-14 | PHP | PRAVIDLA: Mighty Blow je VOLBA, ne pevny bonus na brneni -- a tenhle SEPNE |
| `93043345` | 2026-09-14 | PHP | PRAVIDLA: dve latentni vady -- Stunty a Mighty Blow u Stab/Chainsaw |
| `2f930279` | 2026-09-11 | PHP | OPRAVA MOJI OPRAVY PHP17 + zakotveno pravidlo "lezici nechyta mic" |
| `a77941d8` | 2026-09-11 | PHP | PRAVIDLA/PHP: hand-off a TTM vyhlasovaly turnover, kdyz ho katalog nezna |
| `8460a2ff` | 2026-09-11 | PHP | PRAVIDLA/PHP: neuspesny gaze NENI turnover + obet se nepocita do modifikatoru |
| `80852863` | 2026-09-11 | PHP | PHP/PRAVIDLA: Wild Animal podle textu + blitz mimo dosah uz nehazi vyjimku |
| `496f5a03` | 2026-09-10 |  C++ | PRAVIDLA: dodge se hazi pri VYSTUPU z tacklezony, ne pri vstupu |
| `82673bb8` | 2026-09-10 |  | A1 Leap: zmereny PRILEZITOSTI -- premisa "preskoceni zdi" se nepotvrdila |

Známé otevřené mezery (neověřovat znovu do hloubky, jen potvrdit stav): **E28** pohyb po bloku v Blitzu (PHP chybí) ·
**E18/E19** holdsGround / Take Root (dnes opraveno v PHP — má to C++?).

## 3. Tvrdá omezení

⛔ **NEOPRAVUJ KÓD. Nic neměň v `src/` ani `engine/`. Nekompiluj, nespouštěj hry ani testy.** Je to audit ČTENÍM.
⛔ Nepiš do `evidence/task_queue.md` — řádky do knihy jen navrhni ve výstupu, zapíše je hlavní sezení.
Smíš psát **jen** do souboru `evidence/parity_audit_20260929.md`.

## 4. Výstup — `evidence/parity_audit_20260929.md`

Založ ho jako **první akci** (kostra + seznam commitů s `[ ]`) a po **každém** commitu dopiš výsledek a odškrtni `[x]`.
1. **Nahoře souhrn:** kolik SHODNÉ / CHYBÍ / NEMÁ SMYSL / NEJDE ROZHODNOUT.
2. **Tabulka:** `commit | co se měnilo (1 věta) | druhý engine: soubor:řádek | verdikt | dopad na měření (vysoký/střední/nízký)`.
3. **Navržené řádky do knihy** pro každé CHYBÍ ve tvaru: `| **P??** | PORT do C++/PHP: <co> | commit <hash> | OTEVŘENO |`.
4. **Pořadí** CHYBÍ podle toho, jak moc mění výsledky her v C++ (tam se měří).
5. Poslední řádek souboru, až budeš úplně hotov: **HOTOVO**
