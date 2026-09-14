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

## ÚKOL 4 — ZMĚŘIT DVĚ POTVRZENÉ NESROVNALOSTI V DODGI *(neopravovat!)*

`TacklezoneCalculator::calculateDodgeTarget()` se rozchází s `rules_bb2016.txt`
ve **dvou** bodech. **Obojí je potvrzené proti textu pravidel 14.09.**
⛔ **Úkolem NENÍ to opravit** — oprava mění chování kouče i trénink.
**Úkolem je změřit, jestli to vůbec sepne**, a napsat čísla.

**(A) Chybí bonus `+1` za dodge na ÚPLNĚ VOLNÉ pole.**
Pravidla: cíl = `(7−AG) − 1 + TZ`. Engine: `(7−AG) + max(0, TZ−1)`.
⇒ Liší se **jen při `TZ == 0`**: pravidla `2+`, engine `3+` *(pro AG4)*.
**Změř:** jaký podíl skutečně provedených dodgů má na cílovém poli **nula**
soupeřových TZ. *(Bez toho je to vada na papíře.)*

**(B) Skill `Dodge` je modelovaný jako `−1` k cíli, pravidla dávají RE-ROLL.**
Text: *„is allowed to **re-roll the D6** if he fails to dodge… may only re-roll
**one** failed Dodge roll **per turn**"* *(`rules_bb2016.txt`, ř. 8086-8092)*.
⇒ Není to jiná velikost, ale **jiný druh věci**: re-roll se za kolo spotřebuje,
`−1` platí pořád. Engine dělá elfa **slabším na prvním úniku** a **silnějším na
každém dalším v témže kole**.
**Změř:** kolik dodgů za kolo dělá jeden hráč s `Dodge` *(rozdělení 1, 2, 3+)* —
to řekne, jak velký je ten rozdíl v provozu.

⚠️ **Souvislost, kvůli které to není hygiena:** šance nosiče na útěk je **jádro
kritéria pro obranné L**, které se teď navrhuje. Dokud tohle nesedí, staví se
doktrína na špatných číslech.

## ÚKOL 5 — ZKONTROLOVAT MIGHTY BLOW PROTI PRAVIDLŮM *(jen čtení)*

Fronta má pravidlo: **balík E (roster revize) musí projít korektnostní seznam
DŘÍV, než někomu skill přidá** — jinak se trénuje na nesprávném pravidle.
⇒ Uživatel 14.09. rozhodl, že **u změny TV dostanou trpaslíci Stand Firm
a Mighty Blow**. Dnes je **nemá nikdo z měřené pětky**, takže případná vada
**není vidět**.

* **Stand Firm** — zkontrolován 14.09., vypadá správně *(`holdsGround()`,
  i uprostřed řetězu, správně jako VOLBA)*. ⭐ Starý nález *„chain push
  Stand Firmem projde"* **už neplatí**.
* **Mighty Blow** — ⛔ **NEZKONTROLOVÁN.** Projdi `BlockHandler.php:615, 633,
  729, 802` a porovnej s `rules_bb2016.txt`: platí `+1` na **brnění NEBO na
  zranění** *(ne na obojí v témže bloku)*, a **nepoužije se při crowd surfu**
  *(tam hází dav)*. Napiš, co sedí a co ne. **Neopravovat bez zadání.**

## ÚKOL 6 — OVĚŘIT, ŽE BALÍK G JE OPRAVDU DOTAŽENÝ *(čtení + měření)*

**Proč:** změna TV *(balík E)* je v pořadí **„až po G"**, a G je odškrtnuté
**v zápisu z 10.08., starém přes měsíc**. ⛔ **Odškrtnutí v zápisu není doklad.**
Živá fronta `evidence/task_queue.md` balíky A–G vůbec nevede.

**G = „návrat mrtvol":** rezervy · persistence zranění **uvnitř zápasu**
· soupiska **> 11**. Tedy všechno, co musí **přežít konec drivu**.

**Co se tvrdí, že je hotové** *(commity z 10.08.)*:
| položka | commit |
|---|---|
| Sweltering Heat | `4bd66a4` |
| surf: Stunned → **rezervy**, ne KO *(bez zvláštního stavu, `OFF_PITCH`)* | `4bd66a4` |
| tabulka trvalých následků **D68** + apothecary + Regeneration do rezerv | `1e2f646`, `6211a58` |

**Hotovo znamená — u KAŽDÉ položky doklad z kódu A z běhu:**
1. **V kódu:** najdi místo, které to dělá, a napiš soubor a řádek.
   ⛔ Nestačí, že konstanta nebo enum existuje — **musí to někdo volat**
   *(přesně tohle 13.09. selhalo u `CoachHeuristics`: 295 řádků, nula volajících)*.
2. ⭐⭐ **POZITIVNÍ KONTROLA MĚŘENÍM.** Před opravou bylo naměřeno
   **`DEAD/hru = 0,00` ve všech 3 200 hrách** a *„rozdíl v přeživších na konci
   zápasu 0,2 hráče"*. ⇒ **Změř to znovu** *(stačí ~20 zápasů na pozadí)*:
   * `DEAD / hru` — **musí být > 0**, jinak smrt pořád nenastává;
   * kolik hráčů je na konci zápasu **v rezervách** *(`OFF_PITCH`)*;
   * jestli se **KO hráči vracejí** mezi drivy;
   * kolik těl stojí na hřišti na začátku 2. půle *(soupiska > 11 má smysl,
     jen když se z rezerv doplňuje)*.
   ⛔ **Když vyjde nula, je to nález, ne chyba měření** — ale ke každé nule
   napiš, čím jsi ověřil, že by měřidlo jedničku našlo.
3. **Napiš verdikt po položkách:** hotovo / napsané ale nezapojené / chybí.

⚠️ Souvisí: paměť `project_bloodbowl_casualties_dont_persist_20260807` —
tam je původní nález, že zranění nepřežijí drive.
⛔ **Neopravovat.** Úkolem je zjistit stav, ne ho měnit.

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
