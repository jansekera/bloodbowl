# Kde navázat — Blood Bowl odložen 17.10.2026 ve 22:10

Uživatel: „zastav pak práci na bb a odlož na další dny“. Všechno je commitnuté a pushnuté (poslední `de49e2f4`).

## Stav

- **P159 odklad riskantních akcí** — přepsán `220095ce`, zapnutý pro **všechny** týmy.
  - Riziko akce se **počítá přímo z hodů** (`macroDirectRisk` v `engine/src/macro_actions.cpp`): úhyby, GFI a kostky bloku dohromady. Žádné zkušební hry.
  - Stupně podle celkové pravděpodobnosti selhání (uživatel 17.10.):

    | selže | příklad | kdy hrát |
    |---|---|---|
    | do 1/6 (hod 2+) | AG4 úhyb do prázdna, GFI | kdykoli |
    | do 1/3 (hod 3+) | AG3 do prázdna, AG4 do jedné zóny, úhyb 2+ s GFI | na konci tahu |
    | víc (4+ a hůř) | AG3 do zóny, AG2 | jen když není méně riskantní možnost |

  - Zvednutí míče se **nepočítá**, jen cesta k němu („i Khemri musí zkusit zvednout míč a jít s ním do TD zóny“).
  - Vypnout pro měření: `risk_deferral=False` v `ai_plan_turn`.
- **Měření P159 běží** v `evidence/p159c_20261017/` (180 párů, worktree `../bb-p159` na `220095ce`). Konec zhruba v 01:00–02:00 18.10.; v `beh.log` bude `HOTOVO`.

## Pořadí po návratu

1. ✅ (18.10.: čas +9 %, propadlé aktivace −25 %, 3,6σ — zapsáno v knize) **Vyhodnotit P159:** `cd evidence/p159c_20261017 && /usr/local/bin/python3.8 vyhodnoceni.py`.
   - Rozhoduje **čas na tah** (má být zhruba jako bez odkladu; dřív skaveni 12 → 23 s) a **propadlé aktivace** po turnoveru (víkend: −24 %).
   - TD jen s výhradou (180 párů je na TD málo).
   - Výsledek zapsat do knihy `evidence/task_queue.md`, řádek P159.
2. **P213 útěk nosiče:** útěk smí použít i GFI a úhyb, spočítaný podle dosahu soupeřů. Pak změřit verzi `2bfe769a` (útěk před TD předávkou) proti noci 16.10. (`evidence/p213_20261016/`).
3. **P216:** připravený hráč nesmí stát v obklíčení; slabší tým se připravuje vždy („slabší tým musí být rychlejší“).
4. **Strop úhybu P149** (`macro_actions.cpp:45`, úhyb na 4+ se nehraje): ověřit, že AG4 do dvou zón pustí jen tehdy, když není méně riskantní možnost.
5. **Pondělní kontroly 19.10.:** audit parity PHP × C++ od 16.10., `composer audit` + `npm audit`, `make check-all`.

## Poučení z 17.10.

- Před novým pravidlem napřed hledat, jestli už v enginu není (pravidlo úhybu podle AG jsem začal psát znovu, ač ho dělá P149; rozpracovaný patch je zálohovaný v `evidence/p159_20261016/ag_uhyb_rozpracovano_20261017.patch`).
- Riziko a nejhorší odpověď soupeře se **počítají hned**, ne zkušebními hrami.
- Uživateli psát **krátce**.
