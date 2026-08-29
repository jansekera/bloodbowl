# Ruční testování — Block Reroll Dialog

## Prerekvizity

1. PHP dev server běží: `php -S localhost:8080 -t public/`
2. Vite dev server: `cd frontend && npm run dev`
3. DB migrovaná + seednutá
4. Existují 2 týmy s 11+ hráči (pro vytvoření zápasu)

## A. Základní block dialog

### A1. Block zobrazí modal s kostkami
1. Vytvoř nový zápas (Human vs AI)
2. Dokonči setup fázi (rozmísti hráče, End Setup)
3. Vyber hráče vedle soupeře → klikni "Block" → klikni na soupeře
4. **Očekávání**: Zobrazí se modal s block kostkami (1-3 dle síly)
5. Kostky mají barvy: AD=červená, BD=fialová, Push=modrá, DS=oranžová, DD=zelená
6. Zobrazí se jméno útočníka a obránce

### A2. Výběr kostky kliknutím
1. V block modalu klikni na jednu z kostek
2. **Očekávání**: Modal zmizí, blok se vyhodnotí (push/knockdown/turnover)
3. Game log ukazuje výsledek bloku

### A3. Attacker Down = turnover
1. Proveď blok kde soupeř má vyšší sílu (1-die nebo 2-die defender chooses)
2. Pokud padne Attacker Down, vyber ji (nebo ji vybere obránce)
3. **Očekávání**: Turnover, tvůj hráč padne, tah přejde na soupeře

### A4. Push výsledek
1. Proveď blok, vyber "Push" kostku
2. **Očekávání**: Obránce je odtlačen, žádný pád, útočník může follow-up

### A5. Defender Stumbles vs Dodge
1. Blokni hráče s Dodge skillem, vyber DS (Defender Stumbles)
2. **Očekávání**: Dodge brání pádu (jen push), pokud útočník nemá Tackle

## B. Reroll tlačítka

### B1. Team Reroll na bloku
1. Proveď blok, dostaneš špatný výsledek (AD nebo BD)
2. Klikni "Team Reroll" (pokud je dostupný)
3. **Očekávání**: Kostky se přehodí (nový set), team reroll se spotřebuje
4. Scoreboard ukazuje snížený počet rerollů

### B2. Team Reroll nelze použít dvakrát
1. Po použití Team Rerollu — tlačítko zmizí
2. **Očekávání**: Nelze rerollovat znovu (rerollUsed=true)

### B3. Team Reroll spotřebuje reroll
1. Zkontroluj počet rerollů před blokem
2. Použij Team Reroll
3. **Očekávání**: Po rerollu má tým o 1 reroll méně

## C. Brawler a Pro (vyžadují level-up)

Brawler a Pro nejsou na žádné rase jako starting skill — pro otestování:

### Příprava přes DB
```sql
-- Najdi player_id tvého blitzera
SELECT p.id, p.name, pt.name as position
FROM players p JOIN positional_templates pt ON p.positional_template_id = pt.id
WHERE p.team_id = <tvůj_team_id>;

-- Přidej Brawler
INSERT INTO player_skills (player_id, skill_id)
SELECT <player_id>, id FROM skills WHERE name = 'Brawler';

-- Přidej Pro
INSERT INTO player_skills (player_id, skill_id)
SELECT <player_id>, id FROM skills WHERE name = 'Pro';
```

### C1. Brawler reroll (jen Both Down kostka)
1. Hráč s Brawler provede blok
2. Pokud v kostce je Both Down (BD) → tlačítko "Brawler" je viditelné
3. Klikni "Brawler"
4. **Očekávání**: Přehodí se POUZE ta jedna kostka s Both Down, ostatní zůstanou
5. Brawler je zdarma (nespotřebuje team reroll)

### C2. Brawler nedostupný bez Both Down
1. Hráč s Brawler provede blok
2. Pokud žádná kostka není Both Down
3. **Očekávání**: Tlačítko "Brawler" se NEZOBRAZÍ

### C3. Pro reroll (4+ na D6)
1. Hráč s Pro provede blok
2. Klikni "Pro" tlačítko
3. **Očekávání**: Hodí se D6 — na 4+ se přehodí nejhorší kostka, na 1-3 zůstane původní
4. Event log ukazuje "Pro skill: success/fail"

### C4. Priorita rerollů: Brawler → Pro → Team
1. Hráč s Brawler + Pro provede blok s Both Down
2. **Očekávání**: Zobrazí se všechna 3 tlačítka (Brawler, Pro, Team Reroll)
3. Po použití jednoho rerollu — zbylé zmizí (rerollUsed=true)

## D. Frenzy (druhý blok)

### D1. Frenzy dvojitý blok
1. Dwarf Troll Slayer, Norse Berserker, nebo Dark Elf Witch Elf (mají Frenzy)
2. Proveď blok — vyber kostku (push nebo knockdown)
3. **Očekávání**: Pokud obránce stojí a je stále vedle → druhý block modal se zobrazí
4. Vyber kostku pro druhý blok

### D2. Frenzy s rerollem
1. Frenzy hráč blokuje, první blok → špatný výsledek
2. Použij Team Reroll na první blok
3. **Očekávání**: Reroll funguje, po vybrání kostky → druhý blok
4. Na druhý blok už reroll NENÍ dostupný (už použitý tento blok)

## E. Blitz + Block

### E1. Blitz s blokem na konci
1. Vyber hráče, klikni "Blitz" v action panelu
2. Pohni se k soupeři a klikni na sousední pole
3. **Očekávání**: Hráč se pohne, pak se zobrazí block modal
4. Vyber kostku → blok se vyhodnotí

### E2. Blitz použitý jednou za tah
1. Po blitzu zkus znovu kliknou Blitz
2. **Očekávání**: Tlačítko je disabled / nejde vybrat (blitzUsedThisTurn)

## F. Edge cases

### F1. Refresh prohlížeče s pending blokem
1. Proveď blok → modal se zobrazí
2. Refreshni stránku (F5)
3. **Očekávání**: Modal se znovu zobrazí (state má pendingBlock)

### F2. Jiné akce při pending bloku
1. Proveď blok → modal se zobrazí
2. Zkus kliknout na jiného hráče / provést jinou akci
3. **Očekávání**: Nelze — "Must resolve pending block first" (nebo ignorováno)

### F3. AI blokuje (bez modalu)
1. Počkej na AI tah
2. **Očekávání**: AI blokuje automaticky bez zobrazení modalu
3. AI používá Brawler + Pro (pokud má), ale NE team reroll

### F4. Loner + Team Reroll
1. Hráč s Loner (velcí hráči — Troll, Ogre, Minotaur) blokuje
2. Použij Team Reroll
3. **Očekávání**: Hodí se D6 za Loner — na 4+ reroll proběhne, na 1-3 reroll selže (ale spotřebuje se)

## G. Vizuální kontrola

### G1. Modal styling
- Modal je vycentrovaný, backdrop ztmaví pozadí
- Kostky jsou dostatečně velké pro kliknutí
- Barvy odpovídají typu: AD=červená, BD=fialová, Push=modrá, DS=oranžová, DD=zelená
- Reroll tlačítka jsou pod kostkami
- Jméno útočníka a obránce je viditelné

### G2. Scoreboard po rerollu
- Počet rerollů se aktualizuje po použití team rerollu
- "Reroll used" indikátor se zobrazí

### G3. Game log
- Block event se zobrazí v logu s ikonami kostek
- Reroll event se zobrazí (pokud použitý)
- Turnover event se zobrazí při Attacker Down

## Rychlý smoke test (5 minut)

1. Nový zápas Human vs AI
2. Setup → End Setup
3. Vyber linemana vedle soupeře → Block → klikni soupeře
4. ✅ Modal s kostkami se zobrazí
5. Vyber kostku → ✅ Blok se vyhodnotí
6. Další blok → ✅ Team Reroll funguje
7. ✅ AI hraje bez modalu
