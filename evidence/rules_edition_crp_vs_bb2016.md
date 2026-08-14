# CRP (LRB6) → BB2016: rozdíly a co z nich sahá na náš engine

**Cílová edice projektu je BB2016** (rozhodnutí uživatele, zopakované 14.08.).
Náš stažený text `rules_crp2016.txt` je ve skutečnosti **CRP** — viz T5.16.

⚠️ **Původ tohoto seznamu: od uživatele, zdrojem je AI.** Naše vlastní pravidlo
z 07.08. zní *stáhnout text a grepovat, ne se ptát modelu* — právě takhle se ty
dvě edice slily. **Bereme to jako seznam hypotéz k ověření**, ne jako doklad.
Ověření vede **T5.17**.

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
| **1** | **Piling On** — v CRP zdarma; v BB2016 **volitelné pravidlo** a aktivace stojí **Team Re-roll** | **Sahá.** U nás **není implementovaný vůbec** (mrtvá hodnota enumu). Až se bude dělat, musí vzniknout **verze 2016**. → **T5.15**, odloženo uživatelem |
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
