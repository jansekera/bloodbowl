# ZADÁNÍ PRO AGENTA — 14.09.2026 večer

⚠️ **Tenhle soubor je zadání, ne zápis.** Až se práce udělá, přepiš u každé
položky stav a nechej soubor v repu.

---

## 0. NEŽ ZAČNEŠ — přečti a ověř, nespoléhej na tenhle text

```bash
cd /home/jenda/claude/blood-bowl
git log --oneline -5 && git status -sb
./vendor/bin/phpunit 2>&1 | tail -3      # musí být 1051 zeleně, 0 chyb
grep -rn "CoachHeuristics::" src/        # co je skutečně zapojené
```

⛔ **Stav v zápisech stárne, nález ne.** Když ti čísla nesedí, věř příkazu.

**Kde to stojí k 14.09. odpoledne:**
* `2dde2a76` — oprava rozbitého stromu po 13.09. *(28 chyb; nové patro napsané
  a nezapojené, staré už smazané)*. Zelená **bez změny chování**.
* `24a40d7f` — PHP39 první krok: oba kouči volají `CoachHeuristics::endZoneX()`
  a `::jeNosic()`.
* **Rozpracováno, necommitnuto:** PHP40 — cache pohybových polí na jedno
  rozhodnutí v `LearningAICoach` *(`$poleCache`, metoda `moznaPole()`,
  sebekontrola přes `BB_CACHE_SELFCHECK=1`)*.

---

## PRAVIDLA, KTERÁ PLATÍ NAD VŠÍM

1. ⛔⛔ **NEJDŘÍV ZAPOJIT, PAK MAZAT.** Nové se přidá vedle starého, převedou se
   volající místa, testy zelené — **a teprve pak** se staré odstraní.
   *(13.09. skončilo sezení v opačném pořadí a nechalo repo nespustitelné.)*
2. ⭐ **Každý zelený stav zacommitovat a pushnout**, i když funkce není hotová.
   Do zprávy napsat i **co to NEDĚLÁ a proč**. Commit ani push se neodsouhlasují.
3. ⛔ **Změna → kontrola, po jedné a v pořadí.** Ne dvě změny naráz.
4. ⛔⛔ **CACHE JE PRVNÍ PODEZŘELÝ.** Když se něco chová divně, kontroluj ji
   dřív než heuristiky. Ke každému „0 rozdílů" patří **pozitivní kontrola**, že
   by měřidlo rozdíl našlo, kdyby byl.
5. ⛔ **Běhové artefakty do gitu nepatří** — `corpus_*`, `tempo_measure_*` jsou
   v `.gitignore`. Výsledky se přenášejí přes `evidence/*.md`.
6. ⏱️ **Zápas stojí ~46 s** *(learning vs greedy, jedno jádro)*. 10 zápasů ≈ 8 min,
   100 ≈ 75 min. **Delší běhy pouštěj na pozadí a s timeoutem podle toho**, ne
   podle odhadu.
7. Komentáře a commity **česky**, jako zbytek repa.

---

## ÚKOL 1 — dokončit PHP40 (cache pathfindingu)

**Stav:** kód napsaný, testy 1051 zeleně, **sebekontrola rozběhnutá, výsledek
nedočten**.

**Hotovo znamená:**
1. `BB_CACHE_SELFCHECK=1 php cli/simulate.php --home-ai=learning --away-ai=greedy --matches=10`
   doběhne **bez výjimky** `CACHE POHYBU VRACI JINOU ODPOVED`.
2. ⭐ **Pozitivní kontrola sebekontroly** — dočasně rozbij klíč cache *(např.
   klíčuj konstantou místo `playerId`)* a ukaž, že **výjimka spadne**. Bez toho
   je „0 rozdílů" bezcenné. Pak vrať zpět.
3. **Změř zrychlení**: tytéž zápasy s cache a bez ní *(přidej vypínač
   `BB_CACHE_OFF=1`, ať se nemusí komentovat kód)*. Zapiš **obě čísla**.
4. Commit + push. Do zprávy **obě** čísla a výsledek pozitivní kontroly.

⚠️ **Cache záměrně žije jen po dobu jednoho `decideAction()`** a na jeho začátku
se zahazuje — uvnitř jednoho rozhodnutí se deska nemění. **Nerozšiřuj její
platnost přes tahy**, ani kdyby to bylo rychlejší.

⏰ **A stejnou cache potřebuje i `GreedyAICoach`** — má vlastní volání
`getValidMoveTargets`. Až po bodech 1–4, jako samostatný commit.

---

## ÚKOL 2 — PHP39 zbytek (zapojit, co je připravené a nikdo to nevolá)

V `src/AI/CoachHeuristics.php` leží **nezapojené** metody. Zapoj do **OBOU**
koučů ty, které **nemění chování**:

| metoda | kde je dnes duplicitně |
|---|---|
| `jeRohKlece` | klec v obou koučích |
| `nosicNebojuje` | zákaz blok/blitz/faul pro nosiče |
| `zonyZachyceni` | počítání TZ |
| `jeVedleSoupere` | „nosič nesmí skončit vedle soupeře" |
| `znackujici` | seznam značkujících |
| `pVyhozeniZaFaul` | faul |

**Po každé jedné metodě: testy zelené → commit.** Ne všechny naráz.

⛔ **NEZAPOJUJ** `pTurnoverBloku`, `pTurnoverCesty`, `pTurnoverCelkem`, `jeNouze`
ani vrstvy `VRSTVA_*` — ty **mění chování** a patří do PHP38, který si uživatel
nechal na sebe.
⛔ **Nesahej na `dosahPrihravky`** *(klíče `'short_pass' => 10`)* — ceny jsou
herní rozhodnutí uživatele, řeší se s PHP37.

---

## ÚKOL 3 — sladit frontu

`evidence/task_queue.md`, oddíl **„CO JE TEĎ PRVNÍ"** je z **11.09.** a vede
histogram událostí, PHP28, PHP29, PHP20, PHP18. **Neodpovídá skutečnosti.**

⚠️ **Pravidla knihy:** ID se **nikdy** nepřečíslovávají · přepisuje se **jedině**
poslední oddíl · ostatní položky jen mění stav ve sloupci.

**Hotovo znamená:** poslední oddíl přepsaný na skutečný stav *(PHP38 čeká na
uživatele · PHP39 rozpracované · PHP40 hotové s čísly)*, a u PHP40 doplněný
stav **UZAVŘENO s commitem**.

---

## ⛔ ČEHO SE NEDOTÝKAT

* **PHP38** *(turnoverová brána + vrstvy)* — mění chování, chce vlastní měření
  a uživatelovo rozhodnutí.
* **OBRANA** — kouč nemá nic a **nevymýšlet to**. Čeká na uživatelovy odpovědi
  *(rozestavení při výkopu · rozebírat klec vs. tlačit na nosiče · kdy blitzovat
  místo značkování · screen).*
* **Odložené položky** PHP16, PHP23, PHP25 zbytek, PHP26 — *„neotvírat sám"*.
* **Ceny a bonusy v koučích** — nesou herní rozhodnutí uživatele.

---

## NA KONCI

Napiš do tohoto souboru u každého úkolu **stav a čísla**, a aktualizuj
poznámku na zítřek v paměti *(`project_bloodbowl_tomorrow_20260903.md`)* —
**jeden soubor, nezakládat nový.**
