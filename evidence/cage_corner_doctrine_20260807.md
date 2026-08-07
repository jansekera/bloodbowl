# Doktrína rohů klece — zobecněná (uživatel, 2026-08-07)

Závazný design vstup uživatele (expert na Blood Bowl). Zdroj pro planner-side
implementaci (feasibility slotů klece — fix nálezu 2 technické cage review,
probed exposure) a pro TV1200 roster úpravy (commity f7aa61c dwarf, 2e5b7b7 orc).

## Obecná pravidla (přes skilly/staty, NE přes rasu — F5 princip)

1. **Rohy klece musí mít POHYB (MA).** Roh, který neutáhne plánovaný krok
   klece, stropuje tempo celé formace (viz tempo bilance CAGE_ADVANCE).
2. **Každý v DRŽÍCÍ roli má mít BLOCK** — nejen rohy: zeď, screen, pomalá
   klec. (Držení JE obrana; tělo bez Blocku v kontaktu je snadný knockdown.)
3. **Rohy klece mají mít TACKLE.** Markery klece jsou typicky Dodge kusy
   (gutter runner, wardancer) — Tackle ruší jejich Dodge při bloku i útěku,
   roh si markera ošetří sám.
4. **Rohy POMALÉ klece mají mít GUARD.** Pomalou klec soupeř obestoupí a
   markuje; Guard drží obranné asisty (anti-leap past, asistenční geometrie)
   i markovaný — soupeř ho neutralizuje jen sražením (= platí blok navíc).
5. **Elfové (horší přístup ke Guard — S-skill na double): stačí DVA
   PROTILEHLÉ rohy s Guard.**
6. **Orkové spec.: pokud tempo stačí, rohy mohou mít NAVÍC vyšší ST (ST4).**

## Trpasličí instance (interim)

- Rohy pohyblivé klece = 2× Troll Slayer +Guard+Tackle, 2× Blitzer
  +Guard+Tackle (MA5). Carrier = Runner (MA6, Sure Hands, Block) uprostřed.
  Druhý Runner = záloha V BEZPEČÍ (mimo kontakt, Guard nepotřebuje) —
  substituce rohů + nouzový carrier.
- Základní (vrozené) skilly se nepřerozdělují — jen dokoupené.
- **CÍLOVÝ STAV: klasická klec s Longbeardy na rozích, jak ji hrají lidé.**
  Rychlé rohy jsou přechodová berlička, dokud AI neumí obsluhu rohů (release
  markovaných rohů, clear+shift, substituce). Přechod je z tohoto zobecnění
  záměrně vynechán — řeší se samostatně.

## Stav TV1200 rosterů vůči doktríně (k 07.08.)

| Rasa | Stav |
|---|---|
| dwarf | ✅ f7aa61c: rohy Guard+Tackle; ball-hunter vypadl; Guard jen 2 Longbeardům; ~TV1240 |
| orc | ✅ 2e5b7b7: Black Orc +Block (držící zeď/pomalá klec; MA4 = ne rohy pohyblivé klece); ~TV1280 |
| human | OK bez úprav: 2× Blitzer +Guard (MA7, Block) |
| skaven | 1× Guard (Blitzer); klec není skaven styl — bez úprav |
| wood-elf | ⏳ ODLOŽENO, VE FRONTĚ: 2 pohybliví nositelé Guardu pro protilehlé rohy (pravidlo 5); dnes Guard jen Treeman (MA2, TakeRoot) a Tackle nemá nikdo; s kompenzací, až po dwarf pracích |

## Poznámky k měření

Roster změny f7aa61c/2e5b7b7 = **nová měřicí éra** dwarf/orc matchupů: šampion,
kotvy A3-2 i grind400 data (06.08. večer) vznikly na starých rosterech —
per-race čísla před/po nesrovnávat naslepo; GO rozhodnutí (grind default)
re-validovat na nových rosterech.
