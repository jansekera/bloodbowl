# Fable 26.08.2026 — čím se liší drivy, které TD daly (HOTOVO 26.08. dopoledne)

**Jednou větou:** rozdíl A vs D1 není ve startu, v prvním držení ani v soupeřově
tlaku na startu — je v tom, že **trpasličí nosič ve stejném stavu (i volný, s
bezpečným polem před sebou) v D1 stojí a v A jde** (86–89 % rozdílu tempa, replikuje
přes dva enginy, **rasově specifické** — u skavena/elfa volný nosič v A i D1 jde
stejně). „První držení v kole 3,10" byl artefakt: 35 % D1 drivů začíná uprostřed
půle po soupeřově TD. Prahová hypotéza platí pro TAH (nosič stojí / jde), ne pro drive.

Data: `blitzlanding_replic_20260825_corpus_data/` (3 000 her, dnešní engine), 3 667
přijímacích drivů trpaslíka: A 241 · B 126 · C 1 622 · D1 1 443 · D2 235.
Nástroj: `diag_fable_drives_20260826.py` (staví na `diag_drive_failure_20260811.py`,
stejné dělení drivů, stejné kategorie, stejná definice prvního držení).
Surové výstupy: `evidence/fable_drives_full_run_20260826.txt` (dnešní korpus, plné
drivy), `evidence/fable_drives_crosses_{dwarf,skaven,woodelf}_20260826.txt` (18 000 her).

**Tempo:** 3,7 ms/hru ⇒ celý korpus 3 000 her **11 s** (změřeno na 50 hrách = 0,2 s,
pak spuštěno celé); křížový korpus 18 000 her ≈ 70 s na jeden běh. Noční běh ve
12:00 tím není ohrožen — nic z toho nebylo třeba krátit.

---

## ⭐⭐⭐ NÁLEZ 1 (mění zadání): „první držení v kole 3,10" NENÍ zpoždění, je to POZDNÍ START DRIVU

| skupina | n | drive ZAČÍNÁ v kole | 1. držení v kole |
|---|---|---|---|
| A (TD) | 241 | **1,07** | 1,20 |
| D1 (pozdní start) | 1 443 | **2,86** | 3,10 |
| D2 (pomalá klec) | 235 | **1,01** | 1,02 |

Rozdělení 1. držení u D1 (všech 1 443): k1 844 · k2 73 · k3 47 · k4 44 · k5 46 · k6 44
· k7 83 · **k8 262**. Těch 262 drivů „s prvním držením v kole 8" **začalo v kole 8** —
jsou to drivy po soupeřově TD z kola 7/8. Rozdíl 1,20 vs 3,10 je z **0,24 kola**
skutečného zpoždění sebrání (to ráno stálo správně) a z **~1,8 kola** toho, že drive
vůbec začal později. **501 z 1 443 D1 drivů (35 %) nezačíná v kole 1.** A-drivy
nezačínají později prakticky nikdy (233 z 241 v kole 1).

⇒ Otázka 3 ze zadání („proč je první držení v kole 3,10, když to není fumble ani
start drivu") stojí na chybném čtení: metr `zpoždění 1. držení vs start drivu` v
`drives.txt` (0,24) je správně, ale průměr `1. držení: kolo 3,10` v témž výpisu
je **absolutní kolo, ne zpoždění**, a čte se vedle něj jako zpoždění.
**Není to naše volba v drivu ani soupeřův zásah v drivu — je to důsledek toho, KDY
soupeř skóroval v předchozím drivu** (tedy naší obrany), plus definice D1
(`zbývalo_kol × 2,61 < zbývalo_polí` je u drivu se 2–4 koly splněna automaticky).

Metodicky: **A vs D1 se smí srovnávat jen na plných drivech** (start v kole 1).
Všechno níže je na plných drivech: **A 233 · D1 942 · D2 234** (B 70, C 1 518).

### Co zbylo z „pozdního držení" na plných drivech
1. držení: A k1 211 / k2 21 / k3 1 — D1 k1 844 / k2 73 / k3 15 / k4 6 / k5 3 / k8 1.
Tahy před prvním držením 1,10 (A) vs 1,15 (D1) na drive — z toho ~1,0 je **tah
sebrání samotný** (držení se ukáže až na dalším snímku). Zbytek: neúspěšný pickup
9 % (A) vs 10 % (D1) tahů; „žádný pokus, míč v dosahu MA" 0 % (A) vs 2 % (D1, 23
tahů); míč napřed držel soupeř 1 % obou. **Rozdíl v prvním držení mezi A a D1
na plných drivech NENÍ** (1,10 vs 1,15). Hotovo, tahle stopa je slepá.

---

## NÁLEZ 2: rozdíl A vs D1 se dělá po TAZÍCH, a je z 86 % v tom, CO DĚLÁME VE STEJNÉM STAVU

Postup nosiče v jednom našem tahu (jen tahy, kde jsme míč udrželi do dalšího snímku;
TD tah se nepočítá): **A 2,74 · D1 1,70 · D2 1,27 pole/tah.**

Rozklad rozdílu A − D1 = 1,05 pole/tah podle stavu nosiče na startu tahu
(tacklezóny = stojící soupeři vedle nosiče):

| stav nosiče na startu | podíl tahů A / D1 | postup A / D1 | příspěvek: skladba stavů | příspěvek: chování ve stavu |
|---|---|---|---|---|
| volný (0 TZ) | 51 % / 46 % | 3,06 / 2,33 | +0,15 | **+0,35** |
| 1 TZ | 29 % / 26 % | 2,82 / 1,74 | +0,05 | **+0,30** |
| ≥2 TZ | 18 % / 22 % | 2,07 / 0,82 | −0,05 | **+0,25** |
| turnover dřív než se nosič pohnul | 2 % / 7 % | 0 / 0 | — | — |
| **součet** | | | **+0,15 (14 %)** | **+0,90 (86 %)** |

⇒ **Soupeřův tlak (kolik TZ nosič má) vysvětluje 14 % rozdílu. 86 % je v tom, co
nosič ve stejném stavu udělá.** To je NAŠE strana. Pozor na výběrový efekt: A jsou
drivy, kde to vyšlo — ale ve stavu „volný nosič, bezpečné pole vpřed, žádný
předčasný turnover" (kde soupeř nedělá nic) A postupuje 3,11 a D1 2,56, a D1 v něm
**neudělá ani krok ve 23 % tahů** (A 16 %). Soupeř v tom tahu nosiče nedrží.

### Tahy, kde nosič neudělal krok (0 kroků)
- Podíl na tazích s držením: **A 32 % · D1 52 % · D2 64 %.**
- Z nich: turnover dřív, než se nosič pohnul: A 7 % / D1 13 % (na všech tazích
  s držením **A 2 % vs D1 7 %** — naše řazení akcí + kostky).
- Nosič bez jediné události (stojí a nic): A 52 % / D1 56 % z 0-krokových tahů.
- ⭐ **Nosič má událost, ale 0 kroků: v 96–97 % je to BLOCK.** Nosič míče
  BLOKUJE místo aby šel: A 181 tahů, D 1 635 tahů (= **24 % všech D-tahů
  s držením**, u A 16 %). Data neříkají, jestli blok byl nutný (nabídky maker
  v korpusu nejsou); říkají, že po bloku nosič nešel.
- Volný nosič, 0 kroků, bez předčasného turnoveru: měl **bezpečné pole vpřed
  (0 TZ) v 71 % (A) / 58 % (D1)**. Když ho neměl, ve **86–88 % byla všechna tři
  pole před ním obsazená NAŠIMI hráči** — čelo klece stojí před nosičem a
  nosič za ním. To je klec, která brání vlastnímu nosiči v pohybu (zapsat do
  okruhu KLEC, ne řešit teď).

### Sražen v soupeřově tahu (soupeřova strana)
Nosič sražen v soupeřově tahu, které následuje: **A 5 % · D1 11 % · D2 14 %** tahů
s držením. Soupeřův podíl je reálný, ale menší než náš.

---

## NÁLEZ 3: PRAHOVÁ HYPOTÉZA (2,86 vs 2,02, nic mezi) — VYVRÁCENA na drivech, POTVRZENA na tazích

Rozdělení průměrného tempa ZA DRIVE (plné drivy):

| | <1 | 1–1,5 | 1,5–2 | 2–2,5 | 2,5–3 | 3–3,5 | 3,5–4 | 4+ |
|---|---|---|---|---|---|---|---|---|
| A (233) | 0 % | 2 % | 7 % | 23 % | 25 % | 25 % | 7 % | 10 % |
| D1 (937) | 15 % | 25 % | 21 % | 20 % | 8 % | 7 % | 2 % | 2 % |

Široká, jednovrcholová rozdělení, která se překrývají (D1 má 19 % drivů nad 2,5,
A má 32 % pod 2,5). **Žádný práh mezi drivy není.**

Proč je 2,86 stabilní: A-drivy skórují **v kole 8 ve 162 z 233 (70 %)**, k7 37,
k6 22, k5 11, k4 1. Tempo včetně TD tahu = startovní vzdálenost / použitá kola ≈
21,5 / 7,5 ≈ 2,76 (48 % A-drivů leží v pásmu 2,5–3). **Konstanta 2,86 je
aritmetika výběru** (A = „ušel ~21 polí za ≤8 kol"), ne režim.

⚠️ Zdánlivý nesoulad s pamětí z 20.08. („TD drivy medián 4 kola z 8, max 7, nikdy
8") je vyřešen v nálezu 5: jiná populace (všechna držení vs přijímací plné drivy)
a posun definice o jedna.

Kde práh JE: v jednotlivém tahu. Postup nosiče za tah:

| | <0 | 0 | 1 | 2 | 3 | 4 | 5 | 6+ |
|---|---|---|---|---|---|---|---|---|
| A | 2 % | 22 % | 17 % | 6 % | 13 % | 12 % | 13 % | 16 % |
| D1 | 3 % | 38 % | 20 % | 6 % | 12 % | 8 % | 6 % | 8 % |

Dva vrcholy: **0–1 pole** (nosič stojí / posun o jedno) a **3–6+** (nosič jde).
Prostředek (2 pole) je díra u obou. A má „jde" v 54 % tahů, D1 v 34 %. **Jednotkou
překlopení je tah, ne drive** — cíl „častěji překlopit do rychlého režimu" platí,
ale rychlý režim je tah, ve kterém se nosič vůbec pohne.

Tempo podle kola (jen udržené tahy): A k2 2,76 → k3 2,05 → k4 2,09 → k5 2,31 →
k6 2,78 → k7 5,14. D1: 2,50 → 1,97 → 1,34 → 1,20 → 1,23 → 1,64 → 1,91. Obě skupiny
po prvním kole ZPOMALÍ; A se v k5–k7 rozjede (útočný přechod), D1 zůstane u ~1,2
a rozjede se až v k7–k8, kdy je pozdě. Tvar „fázový přechod v k5–k6" v A je vidět
i u volného nosiče (A k5 3,32 / k6 3,72 / k7 5,35 vs D1 1,77 / 2,00 / 2,52).

---

## NÁLEZ 4: ODPOR — na startu stejný, rozdíl vzniká během drivu (a je to výsledek bitvy)

Odpor v koridoru (stojící soupeři před nosičem v pásu ±2) na startu našeho tahu:

| kolo | A pás / min pás / soupeřů stojí | D1 pás / min pás / soupeřů stojí |
|---|---|---|
| k2 | 5,89 / 1,25 / 9,87 | 6,60 / 1,38 / 10,29 |
| k4 | 4,64 / 0,41 / 8,81 | 6,12 / 0,44 / 9,72 |
| k6 | 2,56 / 0,10 / 7,20 | 4,94 / 0,23 / 8,92 |
| k8 | 0,46 / 0,01 / 5,98 | 3,55 / 0,13 / 7,91 |

- V k2 (první tah s míčem, po soupeřově jediném tahu) je rozdíl **0,7 těla** a
  „nejlepší volný pás" je stejný (1,25 vs 1,38). Start je prakticky stejný — i
  startovní vzdálenost (A 21,6 / D1 22,4), i nejbližší trpaslík k míči (3,8 / 4,5).
- Během drivu A srazí/vyřadí soupeřovy stojící z 9,9 na 6,0 (**−3,9 těla**), D1 z
  10,3 na 7,9 (**−2,4**); naše stojící: A drží 9,8–10,0, D1 klesá 10,0 → 9,05.
  Průměrný odpor za drive 3,50 vs 5,42 je tedy **součet** (a) toho, že A postoupí
  dál (méně soupeřů „před" nosičem je aritmetika pozice) a (b) toho, že A vyhrává
  bitvu těl. Odpor NENÍ vstupní podmínka, je to průběžný výsledek.
- Čí je to volba: bitva těl je náš výběr bloků + kostky + soupeřovy volby, z dat
  nerozložitelné bez přehrání. Označuji jako **SMÍŠENÉ**.

⚠️ D2 začíná blíž (19,4 pole) a s nejbližším trpaslíkem 2,0 od míče — je to
skupina, kde míč spadl k naší formaci; přesto postup 1,27/tah a 64 % tahů bez
kroku. D2 je nejčistší „stojíme dobrovolně".

---

## SOUHRN — složky rozdílu a čí jsou (plné drivy, A 233 vs D1 942)

| složka | A | D1 | čí | váha |
|---|---|---|---|---|
| start drivu (kolo) | 1,00 | 1,00 (po filtru; bez filtru 2,86) | předchozí obrana / definice D1 | mimo drive |
| startovní vzdálenost, dopad míče | 21,6 / 3,8 | 22,4 / 4,5 | soupeřův kop | ~0 |
| první držení | 1,10 tahu | 1,15 tahu | kostky (pickup) | ~0 |
| turnover dřív než se nosič pohne | 2 % tahů | 7 % tahů | **naše řazení** (+kostky) | malá |
| nosič sražen v soupeřově tahu | 5 % | 11 % | **soupeř** | střední |
| tacklezóny na nosiči (skladba stavů) | 47 % v TZ | 52 % | soupeř + naše vystavení | **14 % rozdílu tempa** |
| postup ve stejném stavu (nosič stojí / blokuje / jde) | 3,06 / 2,82 / 2,07 | 2,33 / 1,74 / 0,82 | **naše** | **86 % rozdílu tempa** |
| bitva těl (soupeřovi stojící k8) | 6,0 | 7,9 | smíšené | průběžný výsledek |

---

## NÁLEZ 5: KŘÍŽOVÝ KORPUS (18 000 her, engine 21.08. před pravidlovým kolem) — replikace a rasová kontrola

Plné drivy. Trpaslík 6 000 her: A 429 · D1 1 791 · D2 543. Skaven 4 800 her: A 1 064 ·
D1 58 · D2 508. Wood-elf 4 800 her: A 1 489 · D1 531 · D2 373.

| | trpaslík 26.08. | trpaslík 21.08. | skaven 21.08. | wood-elf 21.08. |
|---|---|---|---|---|
| postup/tah A vs D1 | 2,74 / 1,70 | 2,74 / 1,76 | 1,71 / 1,33 | 2,78 / 2,26 |
| rozdíl: skladba stavů / chování ve stavu | 0,15 / **0,90** | 0,10 / **0,87** | −0,02 / 0,39 | 0,26 / 0,26 |
| **volný nosič**: postup A / D1 | **3,06 / 2,33** | **3,21 / 2,34** | 2,27 / 2,26 | 4,15 / 4,24 |
| tahy s 0 kroky A / D1 | 32 % / 52 % | 35 % / 51 % | 60 % / 66 % | 49 % / 59 % |
| nosič s událostí a 0 kroky = BLOCK | 96–97 % | 96 % | 88–92 % (+PASS 4–8 %) | 60–73 % (+PASS 21–37 %) |
| turnover dřív než se nosič pohne A / D1 | 2 % / 7 % | 2 % / 6 % | 6 % / 6 % | 4 % / 10 % |
| TD v kole 8 (podíl A) | 70 % | 64 % | 23 % | 27 % |
| tempo A za drive | 2,83 | 2,84 | 1,95 | 3,18 |

Čtení:
- **Trpaslík replikuje přes řez enginu** (21.08. vs 26.08.): 89 % / 86 % rozdílu
  v chování ve stavu, volný nosič 3,2 vs 2,3, nosič-BLOCK 96 %. Není to artefakt
  dnešního korpusu ani pravidlového kola.
- ⭐ **U skavena a wood-elfa VOLNÝ nosič postupuje v A i D1 STEJNĚ** (2,27/2,26;
  4,15/4,24). Jejich rozdíl A vs D1 sedí v tacklezónách a předčasných
  turnoverech, ne v tom, co dělá volný nosič. **Rozdíl „volný trpasličí nosič
  v A jde, v D1 stojí" je TRPASLIČÍ VLASTNOST, ne obecný rys enginu.** Zapadá do
  tvaru *účel univerzální, nástroj rasový*: skaven/elf mají v A tempo z toho,
  že nosič utíká z TZ (skaven ≥2 TZ 32 % tahů v A a jde 1,57), trpaslík z toho,
  že se volný nosič vůbec rozejde.
- Skavení A-drivy: TD v k3–k5 36 %, tempo za drive 1,95 (nosič 60 % tahů stojí a
  pak sprintuje 7,9 pole v k7) — jiný tvar drivu, „stůj a sprintuj". Trpaslík ho
  s MA 4–6 nemá k dispozici; jeho jediná cesta je jít každý tah.
- Nosič, který blokuje místo aby šel, je univerzální (i elf 60–73 %); u
  skavena/elfa přibývá PASS s 0 kroky (přihrávka místo běhu) — u trpaslíka 0 %.

### Sladění s pamětí 20.08. („TD drivy medián 4 kola, max 7, nikdy 8")
Není to rozpor, jsou to dvě populace: 20.08. počítalo **všechna držení** (5 751
včetně obranných steal-drivů a krátkého pole — 19,4 % TD „za 1 kolo"), zde jsou
jen **přijímací plné drivy** (A 233). A počítalo držené snímky: jejich „7" =
mých „8 použitých kol" (posun definice o jedna). Přijímací drive trpaslíka je grind
na doraz: TD v k8 v 64–70 %, k7 16 %.

---

## KDE DATA NESTAČÍ — co by se muselo doměřit (nález, ne odhad)

1. **„Nosič stál, ač měl bezpečné pole"** (D1: 398 tahů; A: 72) — data říkají, ŽE
   stál a že bezpečné pole existovalo; NEříkají, jestli ho hledání nabídlo a
   zamítlo, nebo vůbec nenabídlo (nabídky maker v korpusu nejsou). Rozlišení
   „nenabídnuto" vs „zamítnuto" **vyžaduje přehrání přes engine** s výpisem
   nabídky v těch konkrétních snímcích (ctx jsou v `--details` výpisu základního
   nástroje; můj skript je umí vypsat po doplnění jednoho řádku).
2. **Nosič BLOKUJE (1 635 D-tahů)** — jestli blok byl jediná legální/rozumná akce
   (obklíčení), nebo volba, kterou vybírač upřednostnil před pohybem. Totéž:
   přehrání.
3. **Bitva těl** (soupeřovi stojící k8: A 6,0 vs D1 7,9) — rozklad na kostky ×
   výběr bloků × soupeřovy volby jde jen párovým A/B (týž engine, seed), ne z
   korpusu.
4. **Bezpečné pole vpřed** je změřeno jako 3 pole přímo před nosičem s 0 TZ; NEBERE
   v úvahu, zda krok rozbije klec (nosič odejde od rohů). Metr „bezpečné" tedy
   nadhodnocuje volnost u nosiče, který stojí v kleci schválně. Rozlišení chce
   stav klece na snímku (`filled_corners` v `plan{}` existuje, ale `plan` se
   zapisuje jen u `written=True` — nekontrolováno, kolik snímků ho má).
5. **Výběrový efekt A**: A jsou drivy, kde to vyšlo. „Ve stejném stavu jde dál"
   může zčásti znamenat „ve stejném stavu měl štěstí na to, co přišlo potom".
   Čistý test je párový: stejný snímek, dvě politiky. Tohle je korelace, ne
   kauzalita — ale u volného nosiče bez TZ a s bezpečným polem před sebou
   kostky do postupu nemluví (žádný dodge, žádné GFI potřeba), takže tam je
   výběrový efekt nejslabší.

---

## ZAŘADITELNÉ NÁLEZY (bez doporučení pro kód — pořadí okruhů PRAVIDLA ✅ → POHYB → KLEC → …)

- **[POHYB]** ⭐⭐⭐ Trpasličí nosič, který je VOLNÝ (0 TZ) a má před sebou bezpečné
  pole, neudělá krok ve 23 % tahů (D1) / 16 % (A); postup ve stavu „volný" 2,33 vs
  3,06. 86–89 % rozdílu tempa A vs D1 je v chování ve stejném stavu. Rasově
  specifické (skaven/elf: volný nosič v A i D1 stejně). ⇒ Kandidát na měření po
  přehrání: nabídnuto vs zamítnuto.
- **[POHYB]** ⭐⭐ Nosič míče BLOKUJE místo aby šel: 24 % D-tahů s držením (A 16 %),
  96 % „událost bez kroku". Univerzální (i elf).
- **[POHYB / CELOTAH]** ⭐ Turnover dřív, než se nosič pohne: A 2 % vs D1 7 % tahů
  (řazení akcí; „nosič první událost tahu" je jen 26–28 % v obou skupinách).
- **[KLEC]** ⭐⭐ Když volný nosič nemá volné pole vpřed, je to v 86–88 % proto, že
  všechna tři pole před ním obsazují NAŠI hráči (čelo klece před nosičem). 298
  D-tahů. Zapsat, neřešit teď.
- **[KLEC / ÚTOK]** ⭐ Fázový přechod je vidět v datech: A-drivy zpomalí v k3–k4
  (2,05) a rozjedou se v k5–k7 (2,31 → 2,78 → 5,14); D1 zůstane u 1,2–1,3 a rozjede
  se až v k7–k8. Souvisí s [[project_bloodbowl_attack_defense_doctrine_20260825]]
  (útok = moment rozhodnutí opustit klec).
- **[METODIKA / drives.txt]** ⛔ Výpis `[D1] 1. držení: kolo 3,10` je ABSOLUTNÍ kolo
  a vedle metru `zpoždění 0,24` se čte jako zpoždění. 35 % D1 drivů nezačíná v
  kole 1 — **srovnání A vs D1 platí jen na plných drivech**; do
  `diag_drive_failure` by patřil sloupec „start drivu" (změna nástroje, ne enginu).
- **[OBRANA]** 501 D1 drivů z 1 443 jsou drivy po soupeřově TD uprostřed půle. Jejich
  neúspěch je zaúčtovaný obraně, ne útoku; kategorie D1 je s nimi kontaminovaná.
- **[METODIKA]** Prahová hypotéza 2,86 vs 2,02: na drivech vyvrácena (široká
  překrývající se rozdělení; 2,86 = 21,5 polí / 7,5 kola, aritmetika výběru),
  na tazích potvrzena (dva vrcholy 0–1 a 3–6+, prostředek prázdný). Jednotka
  překlopení je TAH.
