# CRP (LRB6) → BB2016: rozdíly a co z nich sahá na náš engine

**Cílová edice projektu je BB2016** (rozhodnutí uživatele, zopakované 14.08.).
Náš stažený text `rules_crp2016.txt` je ve skutečnosti **CRP** — viz T5.16.

## ✅ ZDROJE — obě edice máme jako text (14.08.)

| soubor | co to je |
|---|---|
| `rules_crp_lrb6.txt` | CRP / LRB6, 243 kB *(dřív matoucí název `rules_crp2016.txt`)* |
| `rules_bb2016.txt` | **BB2016**, 107 stran / 432 kB — viz rozpis níž |

### Co `rules_bb2016.txt` obsahuje (podle vlastního obsahu)

Rulebook 11/2016 · **Death Zone Season 1** 11/2016 · **Death Zone Season 2**
5/2017 · Bugman's Star Players 5/2016 · ⭐ **Errata and FAQ (pdf, 5/2017)** ·
Referees (White Dwarf 1/2017) · Winter Weather Table · Blitzmania Kick-off
table · Special Play Variant rules v3 · team-specific ball rules · Match Events.
**Kompilace k říjnu 2017 — errata jsou už zapracovaná v textu.**

⚠️ **Není to oficiální dokument GW, je to komunitní kompilace**
(*„Author: Baxx @ dakkadakka"*). Kompilátor dělal redakční rozhodnutí
(*„Incorporated White Dwarf Goblin & Halfling Referees to the Death Zone
Season 2 Referee table"*) a sám uvádí nedodělky (*„Todo: -Skaven Vermin
Pestilent match events (missing details for 1)…"*).
⇒ **Dobrý pracovní referenční text; u sporného bodu dohledat oficiální zdroj.**

✅ **Ověřeno, že to NENÍ BB2020:** „Second Season" 0× · „Passing Ability" 0× ·
„Team Draft" 0× · „Agility" 51× (statline 2016). Nic z 2018/2019.

Postup byl týž jako 07.08.: `WebFetch` uloží PDF na disk i když ho neumí
přečíst → `venv/bin/python3` + `pypdf` → grep. Zdroj PDF:
`cdn.1j1ju.com/medias/f8/fd/4b-blood-bowl-rulebook.pdf`.
Oba soubory jsou **mimo git** (`.gitignore`).

⚠️ Seznam níž přišel od uživatele a jeho zdrojem byla AI. **Body ověřené proti
textu jsou označené ✅ / ⛔.** Neověřené zůstávají hypotézou.

### ⛔ Dva body z webových shrnutí jsou NEPRAVDIVÉ

Web tvrdil, že BB2016 *„removed the '(except for Stakes)' exception from the
Stab skill and completely removed Stakes as a skill"*. **Obojí je špatně** —
v textu BB2016 je `Stakes (Extraordinary)` i výjimka `(except for Stakes)`
u `Stab`, a znění je **slovo od slova totožné s CRP**.
⇒ Přesně proto platí pravidlo *stáhnout text, ne věřit souhrnu* — ať je ten
souhrn od AI nebo z fóra.

> **Proč tenhle soubor vznikl:** uživatel tenhle přehled poslal **už dříve**
> a nikde se nezapsal — třetí případ téhož dne (po rozhodnutí o edici a po
> ~20 ztracených položkách fronty). Rozhodnutí a fakta od uživatele patří do
> repozitáře, ne do konverzace.

---

## ⭐ Hlavní závěr triáže: v ZÁPASE se ty edice skoro neliší

Náš engine modeluje **jediný zápas**. Nemodeluje ligu, SPP, inducements,
pokladnu ani karty (ověřeno grepem: `inducement`, `SPP`, `MVP`, `treasury`
nejsou v `engine/src/` nikde). Drtivá většina změn BB2016 je **poliga
a ekonomika po zápase** ⇒ na nás nedopadá vůbec.

⇒ **Poplach „všechny audity běžely proti špatné edici" se tím z velké části
ruší.** Dosud auditovaná pravidla (dodge, leap, Take Root, Wild Animal, pickup,
catch, GFI, Dauntless, Stand Firm, řetěz odsunu, TD v soupeřově kole, Tackle,
Sprint, Claw, Mighty Blow) se mezi CRP a BB2016 **nemění**.

## Co na nás sahá

| # | změna | dopad na nás |
|---|---|---|
| **1** ✅ | **Piling On** — v CRP zdarma; v BB2016 aktivace stojí **Team Re-roll**. **OVĚŘENO v textu:** BB2016 *„**You can use a team re-roll** to re-roll the Armour roll or Injury roll"* vs CRP *„You may re-roll the Armour roll or Injury roll"* | **Sahá.** U nás **není implementovaný vůbec** (mrtvá hodnota enumu). Až se bude dělat, musí vzniknout **verze 2016**. → **T5.15**, odloženo uživatelem |
| **2** | **Argue the Call** — kouč smí zkusit zvrátit vyloučení za faul / Secret Weapon. `1` = kouč vykázán a −1 na Brilliant Coaching · `2–5` = platí · `6` = hráč jen na střídačku (tah stále končí turnoverem) | **Sahá.** Vyloučení za faul **modelujeme** (`foul_handler.cpp:64`, dvojice → ejected, Sneaky Git tomu brání), ale odvolání nemáme. → **nová položka** |
| **3** | **Weeping Dagger** — nová dovednost skavenních Gutter Runnerů; Badly Hurt se na **4+** mění na Miss Next Game | Sahá jen přes rostery. „Miss Next Game" je **poliga**, kterou nemodelujeme ⇒ prakticky bez efektu na jeden zápas. → k **T5.13** |
| **4** | **Timmm-ber!** — nová dovednost **halflingských** Treemanů (+1 na postavení za každého nemarkovaného souseda). **Wood-elfí Treemani ji NEDOSTALI** | **Užitečný negativní nález:** náš wood-elfí Treeman je **správně bez ní**. Nic neměnit. |
| **5** | **Human Catcher 70k → 60k** | Jen skladba TV. Naše TV1200 rostery jsou ručně psané odhady ⇒ zanedbatelné. |

## Co na nás nesahá vůbec

* **MVP** — v CRP náhodně ze všech, v BB2016 kouč vybere 3 a losuje mezi nimi
* **Spiralling Expenses** — v BB2016 volitelné
* **Expensive Mistakes** — nová tabulka, riziko při pokladně > 100k
* **Redrafting** — pravidla mezisezóny, hráči chtějí vyšší platy za skilly
* **Wizards** — v Death Zone 1 vyřazeni z běžných inducements
* **Special Play Cards** — větší důraz, 100k za kartu
* **Přejmenování** — Chaos Pact → Chaos Renegades, Slann vyřazen
* **Měřítko 28 mm → 32 mm**, deska 29 → 34 mm, nové pravítko na přihrávky

*(Vše výše je liga, ekonomika po zápase, nebo fyzické komponenty.)*

## ✅ Křížová kontrola nezávislým zdrojem (14.08.)

Oficiální samostatné errata/FAQ z 5/2017 se na otevřeném webu **nepodařilo
sehnat** — GW downloads je JS SPA bez obsahu, `thenaf.net` vrací 403, a všechny
nalezené odkazy vedou zpět na tutéž kompilaci. Cross-check tedy proběhl proti
komunitnímu soupisu změn *(Talk Fantasy Football, „Blood Bowl 2016 — the full
list of changes")*.

**Shoduje se s uživatelovým seznamem doslova** (Piling On volitelné + team
reroll · Argue the Call · Weeping Dagger · Timmm-ber! pro **halflingské**
Treemany · MVP ze tří vybraných) a **žádnou další změnu uvnitř zápasu
nepřidal**.

### ⭐ Detail, který vysvětluje shodu u Claws

> *„**Claw and Grab skills returned to CRP versions**" per the errata*

Posloupnost byla **CRP → rulebook 11/2016 Claw a Grab změnil → errata 5/2017
je vrátila na verzi CRP.** Naše kompilace errata obsahuje, proto v ní Claws
sedí na CRP slovo od slova.
⚠️ **Kdybychom vzali holý rulebook z 11/2016, měli bychom jiný Claw** — to je
konkrétní důvod, proč se u téhle edice nesmí pracovat s textem bez errat.

## ✅ Co se ověřilo jako BEZE ZMĚNY mezi edicemi

Znění je v obou textech totožné, takže naše dosavadní audity **platí**:

* **Mighty Blow** — *„you only modify **one** of the dice rolls"* v obou
  ⇒ **T5.14 je chyba proti oběma edicím**, ne důsledek špatné edice.
* **Claws** — *„any Armour roll of **8 or more after modifications**"* v obou
  ⇒ stacking Claw+MB na brnění (7+ prorazí AV9) **zůstává**, jak si uživatel přál.
* **Stakes**, **Stab**, **Tackle** — beze změny.

## ⚠️ Nová parita, která z ověření vypadla

**`Stakes` u nás dělá něco jiného, než pravidlo říká.** Obě edice shodně:
*„may add **1 to the Armour roll** when they make a **Stab attack** against any
player playing for a Khemri, Necromantic, Undead or Vampire team."*
Náš engine s ním místo toho **blokuje Regeneration** (`injury.cpp:145`:
`hasSkill(Regeneration) && !ctx.hasStakes`).
**Neškodné** — Stakes má jediný Star Player proti nemrtvým a my nehrajeme
ani Star Playery, ani nemrtvé *(uživatel 14.08.)*. → **T5.19**, nízká priorita.

## Co zbývá ověřit (T5.17, přeškálováno)

Původní zadání znělo „přeauditovat všech ~15 pravidel proti nové edici".
Po téhle triáži je to **mnohem menší práce**:

1. Sehnat autoritativní text **BB2016 / Death Zone Season 1** týmž postupem
   jako 07.08. (stáhnout PDF → `pypdf` → grep).
2. Ověřit **body 1–5** výše proti textu.
3. **Projít text na změny UVNITŘ ZÁPASU, které v tomhle seznamu nejsou** —
   seznam je z AI a jeho úplnost není zaručená. Tohle je jediná část, kde
   pořád hrozí překvapení.
4. Přejmenovat `rules_crp2016.txt` → `rules_crp_lrb6.txt`, ať název nelže.

## Pozdější edice (mimo rozsah)

**BB2020** systém předělalo radikálněji (např. oddělení statu Passing od
Agility) a nerflo i Claw. ⛔ **Nejdeme tam** — uživatel 14.08.:
*„my zůstáváme teď u silné kombinace podle 2016."*
