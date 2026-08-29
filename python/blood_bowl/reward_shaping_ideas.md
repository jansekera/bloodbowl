<!-- SLOUČENO 29.08.2026: existovaly DVĚ rozešlé kopie tohoto souboru,
     `reward_shaping_ideas.md` v kořeni (28.03.) a tahle (30.03.), a ani jedna
     nebyla v gitu. Kořenová byla STARŠÍ a tvrdila, že turn-urgency je
     "Implementováno" -- přitom byl revertovaný, což ví jen tahle. Kořenová
     kopie je zazálohovaná v /home/jenda/zal/claude2/blood-bowl/, její jediný
     unikátní obsah (motivace k návrhům A/B/C) je vepsaný níž kurzívou.
     ⛔ CELÝ SOUBOR JE PŘEKONANÝ: reward SSOT je od 26.06.2026
     `python/blood_bowl/rewards.py` (win +1.0 > draw > loss, TD-in-loss má
     hodnotu), plus Lever B per-TD step reward C=0.2 z 29.06. Drží se jako
     ZÁZNAM, CO SE ZKOUŠELO A CO NEVYŠLO, ne jako plán. -->

# Reward Shaping — nápady pro budoucí iterace

Vše níže jsou změny v `_compute_potential` v `trainer.py` (bez nových features, bez změny dimenzí).

## Implementováno

- **stall_incentive bug fix** (commit 6c0dc33) — `>= 0.0` → `> 0.0` v features.py
  Stall se aktivuje jen při VEDENÍ, ne při remíze. Váha zůstala 0.5.
  Výsledek: PROMOTED 86.7% po 4 předchozích REJECTech.

## Vráceno (nefungovalo)

- **Turn-urgency multiplier** (commit a53e748, revertováno) — urgency = 1.0 + (1.0 - turns_remaining) * 3.0
  Výsledky: 83.3% REJECTED, 76.7% REJECTED — model se naučil čekat na "ideální moment"
- **stall_incentive váha 1.5** (revertováno) — příliš silný signál, 83.3% REJECTED

## Nápady k vyzkoušení (po stabilizaci na 90%+)

*(motivace z kořenové kopie k A: v posledních tazích držet míč bez pohybu = prohrává čas)*

### A) Penalizace za stání s míčem na vlastní půlce v závěru
- Přidat záporný příspěvek pro `having_ball` (feature 12) když `turns_remaining < 0.3`
  a carrier je daleko od endzone (carrier_dist_to_td > threshold)

### B) Exponenciální urgency místo lineární (jemnější verze)
*(motivace z kořenové kopie: lineární možná příliš tlačí na skórování i v polovině hry)*
- `urgency = 1.0 + 2.0 * (1.0 - turns_remaining) ** 2` (max 3× místo 4×)
- Pouze pokud lineární způsobovala problémy kvůli síle, ne konceptu

### C) Záporná urgency pro obranu
- Zvýšit váhu penalizace za `carrier_dist_to_td` (feature 15) soupeře v závěru
*(motivace z kořenové kopie: když soupeř má míč blízko naší endzóny a zbývá málo tahů, bránit stejnou logikou jako útok)*

## Pravidla pro nasazení
- Vždy jen jedna změna naráz
- Nejdřív 2–3 tréninky bez změn (ověřit stabilitu)
- Benchmark ≥ 90% po 2× po sobě → teprve uvažovat o dalším experimentu
