# P180 — návrh „připravený hráč pro TD předávkou / přihrávkou“ (08.10.2026)

Zdroj: rozbor čtením kódu (oddělený agent, nic neměněno a nespuštěno). Stav: **návrh k představení uživateli, nepsáno.**
Zadání uživatele: *„rychlejší tým by měl být pouze ve stavech — klec v pořádku — a — nosič doběhne, případně předá nebo hodí někomu nachystanému dát TD“*; *„pokud má skavení nosič utéct sám — hodí se GR — nebo případně Thrower handoff/pass na GR, který pak uteče“*.

## Co je dnes (ověřeno v kódu)

- TD přes spoluhráče v tahu s platným plánem klece prakticky nenastane: řadič pohne nosičem dřív, než se hledání dostane ke slovu (makra chtějí `carrier->canAct()`), a příjemce nikdo nestaví — kde stojí, je náhoda (`planLaggards` hýbe jen hráči za nosičem, nejvýš 3 sloupce před něj).
- `HAND_OFF_SCORE` / `PASS_SCORE` / `CHAIN_SCORE` se nabízejí jen „zaseknutému“ nosiči (nedosáhne, nebo stojí ve 2+ zónách), bez výpočtu šance; `PASS_SCORE` nezná hod na přihrávku, zóny ani intercepci a vzdálenost nebere podle pravítka.
- `expandHandOffScore`: příjemce běží na `{zóna, vlastní řádek}` bez výběru pole a bez kontroly obsazenosti — stejná třída vady jako u `SCORE` (opraveno 08.10.).
- `forbidsCarrierMove` zakazuje TD přes spoluhráče jen při `stalling_`, a ten se nastaví jen, když nosič dojde SÁM bez hodu ⇒ **díra v zdržování:** míč v bezpečí, nosič sám nedojde, hledání smí zahrát předávku na TD.
- `stillValid` řadiče makro předávky nezná (zahodil by ho).

## Návrh — dvě úpravy, každá s vypínačem

**A. Příprava (`kFeatReadyMate`, bit 1024).** Po tahu klece se nejvýš jeden volný hráč postaví na pole `r`:
1. dojde tam tento tah bez hodu; 2. pole není v zóně soupeře a je ≥ 2 pole od postranní čáry; 3. příští tah z něj dojde do zóny bez hodu (`dist ≤ MA příjemce`); 4. nosič k němu příští tah dojde bez hodu; 5. soupeř na něj nedosáhne dobrou ranou (`blitzThreat` ≤ 0,15); 6. klec má po tomto tahu čtyři rohy, nebo je nosič mimo dosah soupeře.
Pořadí: nejméně soupeřů v dosahu sousedního pole → vyšší šance zachycení (AG, Catch) → co nejblíž nosiči.
Okno: `MA nosiče < vzdálenost nosiče do zóny ≤ MA nosiče + MA příjemce + 1`.

| tým (TV1500) | nosič → příjemce | okno do zóny | zachycení předávky |
|---|---|---|---|
| skaven | Thrower MA7 → Gutter Runner MA9/10 | do 17 / 18 | 83 % |
| wood-elf | Thrower MA7 → Catcher MA8 | do 16 | 97 % |
| člověk | Thrower MA6 → Catcher MA8 | do 15 | 89 % |
| ork | Thrower MA5 → Blitzer MA6 | do 12 | 67 % |
| trpaslík | Runner MA6 → Blitzer MA5 / Runner MA6 | do 12 / 13 | 67 % (AG2: 50 %) |

**B. Skórování přes něj (`kFeatScoreViaMate`, bit 262144).** V `planStart` vedle „TD teď nosičem“ i „TD teď přes spoluhráče“; stejné porovnání jako `kFeatScoreEarly`: šance TD teď ≥ šance, že míč přežije soupeřův tah (1 − `blitzThreat` po našem nejlepším plánu klece) ⇒ řadič TD přikáže lepší z obou cest. Šance předávky = (nosič dojde na pole vedle příjemce) × (zachycení, +1, −1 za zónu, Catch = přehoz) × (příjemce dojde do zóny); makro nese obě pole.
První krok jen PŘEDÁVKA (pravidla ř. 1676–1692, 845–846). Přihrávka chce nový odhad intercepce sdílený s `pass_handler` — druhý krok.

## Trpaslíci

Samotné porovnání je nevyřadí (předávka 67 % vyjde proti ráně dvěma kostkami; proti jedné nevyjde o vlas — 0,667 × 0,67, křehké). Brzdí je příprava: okno 12–13 polí, příjemce nejvýš 5–6 polí od zóny, mimo dosah rychlejšího soupeře, jen s dostavěnou klecí. **Neměřeno — pojistka musí být měření, ne předpoklad.**

## Měřidla

1. Jmenovatel: začátek našeho tahu, míč držíme, nosič bez hodu nedojde, hrozba po nejlepším plánu > 0,15; dělit „příjemce s P ≥ 50 / 67 / 83 % existuje / neexistuje“; výsledek: TD · míč držíme · ztracen do konce soupeřova tahu.
2. Příprava: podíl tahů, po kterých připravený hráč stojí, a podíl, kdy je na začátku dalšího tahu ještě použitelný.
3. Stav „ani-ani“ (P178) u rychlých týmů má klesnout.
4. Pojistka trpaslíků: párově bit zapnutý × vypnutý — čistá klec, turnover, soupeř první, TD v 7.–8. kole.
5. Pojistka zdržování: TD přes spoluhráče při hrozbě ≤ 0,05 mimo poslední kolo = 0 (nula jen s pozitivní kontrolou u skavenů).

## Rizika

O muže méně u klece (P143) · osamělý Gutter Runner (AV7) dostane ránu — poločasové měření nevidí úbytek hráčů · soupeřova AI možná příjemce nemarkuje (přínos proti člověku nadhodnocen) · dřívější TD jde proti P177 (číst TD v 7.–8. kole a celé zápasy).

## Otázky k rozhodnutí (uživatel)

1. Má se přes spoluhráče skórovat při jakékoli hrozbě, kdy šance TD ≥ šance přežití míče, nebo až nad určitou mezí hrozby?
2. Předávka a přihrávka naráz, nebo po krocích?
3. Patří sem i „předat rychlejšímu, který s míčem uteče z dosahu“ (bez TD v témž tahu)?
4. Smí se hráč chystat, i když klec po tahu čtyři rohy nemá?
5. Ve fázi vypuštěného nosiče: co má přednost, markování, nebo připravený hráč?
6. Poslední kolo poločasu: vybírá cestu k TD řadič, nebo dál hledání?
7. Kolik hráčů se smí chystat naráz?
8. Míč v bezpečí, nosič sám nedojde, TD přes spoluhráče by šlo: zdržovat, nebo skórovat?
9. Smí připravené pole počítat s GFI příjemce?

Doporučení autora rozboru (zvlášť, ne rozhodnutí): 1 až nad 0,15 · 2 po krocích, napřed předávka · 3 samostatný úkol · 4 ne · 5 markování · 6 řadič, větší z obou šancí · 7 jeden · 8 zdržovat, dokud má tým časovou rezervu · 9 ne.

## Rozsah

`one_cage.cpp` (`planStart`, nový krok přípravy, `forbidsCarrierMove`, `stillValid`), nová funkce šance TD přes spoluhráče, `expandHandOffScore` dodrží pole z makra, dva bity v `CageFeature`, čítače v `mcts_cli`; ~9 testů (hráči zadaní čísly, ne rasou; každý musí s vypnutou úpravou spadnout). PHP se netýká (volba AI, ne pravidlo).
Před plánem ověřit: nastaví přesun řadiče `hasActed` nosiče; počítá `pathFailProb` Sure Feet; reaguje soupeřova AI na volného příjemce.
