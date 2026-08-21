# P40 ANALÝZA 21.08. (Fable) — brzda, rasy a audit klece

Zadání: `evidence/fable_brief_p40_20260821.md` + doplněk (externí doktrína
bbtactics, nezávislý zdroj). Nové skripty: `diag_p40_rows_verify_20260821.py`,
`diag_p40_attrition_20260821.py`, `diag_p40_brake_20260821.py` (vše čistá
analýza; engine nepřestavěn, žádná noc nespuštěna, žádný kód neopraven).

Ověření §1 zadání: dekódování rows **potvrzeno přesně** (P38 dwarf
0,4401→0,5378 · we 0,5513→0,6466; cena kritéria dwarf **+0,0343 ± 0,0139
(+2,46σ)** · we −0,0104 ± 0,0157 (−0,66σ); shoda základen **6 266 z 13 600
= 46,1 %**). Posuny základen sedí (dwarf +0,0122, we −0,0160) — ale pozor,
jsou to POSUNY SEEDŮ mezi nocemi, viz (1c).

---

## (0) VERDIKT Q-D: `cageScoreForSquare` MĚŘÍ KLEC PODLE SPEC — brzda je doktrína, ne bug

Audit `engine/src/macro_actions.cpp:1316` (predikát), `:1426` (filtr v
`expandAdvance`), `:1459` (`expandCage`), proti spec 15.0b.

1. **Všechny tři klauzule tam JSOU.** (1) 4 rohy: čtyři diagonály na hřišti
   + 4 RŮZNÁ stojící těla s dosahem (greedy, Chebyshev ≤ movementRemaining);
   (2) čisté: žádný STOJÍCÍ soupeř do 1 od rohu; (3) sousedi: čtyři
   ortogonály kandidáta PRÁZDNÉ (i od vlastních těl — přesně dle spec 15.0b,
   „(3) zakazuje i VLASTNÍ tělo navíc") a soupeř v žádném z osmi
   (stojící přes TZ-filtr kandidáta, lehlý přes ortho/roh testy).
2. **Je to KONJUNKCE, ne součet.** Predikát vrací −1/1, používá se jako
   binární filtr (`< 0 → continue`). Žádné aditivní skóre. Chyba σ-tabulky
   (rozklad konjunkce) se tu NEopakuje.
3. **Lehlý vs stojící: rozlišuje, směr spec.** Roh sousedící s LEHLÝM
   soupeřem je „čistý" (klauzule 2 počítá jen stojící) — to je věrné spec
   i doktríně 20.08. („ležící stojí BLITZ"). ⛔ Ale v NAŠEM enginu ta
   bezpečnost neplatí (P45: `resolveStandUp` nenastaví `hasActed`, ležící
   vstane a hned blokuje) ⇒ predikát měří klec v prostředí, kde je lehlý
   ≈ stojící, a čistotu rohů NADHODNOCUJE. Efekt kritéria je tedy měřen
   v prostředí nepřátelském vůči kleci; po opravě P45 může jedině růst,
   klesnout by neměl. (Lehlý soupeř NA ortogonále/rohu se správně zamítá.)
4. **Kotvení: predikát správně, PIPELINE ne.** `cageScoreForSquare` kotví
   rohy na KANDIDÁTNÍ (budoucí) pole nosiče — správně dle 15.0c. Ale nic
   pak tu klec nestaví: `expandCage` kotví na `carrier.position` v okamžiku
   SVÉHO provedení, neimplementuje ANI JEDNU z klauzulí (bez čistoty, bez
   klauzule 3, „nejbližší volné tělo, max 4 kroky"). Nepřímý odhad X3
   z korpusu (MOVE události): u trpaslíka skončilo 10 028 výplňových pohybů
   diagonálně u STARÉHO pole nosiče proti 19 280 u NOVÉHO; z těch u starého
   jich 2 703 proběhlo PŘED pohybem nosiče = čistý podpis „klec se postavila
   kolem pole, které nosič vzápětí opustil" (~0,9 kola/hru; elf 1 889).
   ⚠️ Odhad zahrnuje i REPOSITION pohyby, `TurnLog` makra neloguje — přesné
   číslo nejde, ale X3 je poprvé aspoň řádově vidět a NENÍ nulové.
5. **Odpor koridoru: potvrzeno, predikát o něm neví nic.** A nově změřeno,
   že brzda jde odporu dokonce PROTI — viz (1b).
6. **Blokovala by spec-věrná klec pořád ve 2 z 5 kol? ANO.** Rozklad důvodů
   na zrušeném picku (nezávislé klauzule, součet > 100 %):
   trpaslík z 6 898 blokovaných: špinavý roh 46,3 % · nedostatek výplní
   (dosah) 47,4 % · <4 rohy (lajna/EZ) 23,8 % · naše tělo na ortogonále
   10,2 % · lehlý soupeř na ortogonále 7,0 % · lehlý na rohu 7,6 %.
   Elf z 4 678: špinavý roh 60,3 % · výplně 26,7 % · <4 rohy 23,0 % ·
   naše ortho tělo 17,9 % · lehlý ortho 13,4 % · lehlý roh 12,0 %.
   Jediná kandidátní VADA (smíšené časové kotvení: klauzule 3 se čte
   z dnešní desky, klauzule 1 z budoucí — naše stojící tělo na ortogonále
   kandidáta, které by samo dosáhlo na roh, pole zabije) ruší blok jen
   v **250 z 6 898 (3,6 %)** u trpaslíka a **271 z 4 678 (5,8 %)** u elfa.
   ⇒ **Brzda vzniká z PRAVIDLA, ne z vady. Rasový nález z §0 není artefakt
   implementace.**

Externí doktrína (bbtactics, doplněk 21.08.) definici nezávisle potvrzuje:
pět hráčů, čtyři diagonální rohy, „none of your five players end the turn
in the tacklezone of an opposing player". Čistota NOSIČE se u nás vynucuje
TZ-filtrem kandidáta (`countTacklezones > 0 → continue`) — je ale SPOLEČNÁ
oběma ramenům, takže není součástí testovaného kritéria; rohy pokrývá
klauzule 2. Konjunkce sedí. Jediný rozdíl proti doktríně: doktrína mluví
o KONCI kola, predikát je prospektivní (dosah místo výsledku) a cestu
výplní neoceňuje (žádné dodge po cestě, žádné povinnosti těl — viz rozklad
20.08.: po vyloučení těl s povinností padá plnitelnost ~3×).

## (1) VERDIKT K HYPOTÉZE O BRZDĚ: v zadaném tvaru PADÁ; data ukazují jiný mechanismus

**(1a) Brzda NENÍ rasová — blokuje OBĚMA rasám totéž.** Zadání (Q-A)
předregistrovalo: u trpaslíka má blokovaný pick vyšší odpor/blíž lajně,
u elfa stejně dobrý nebo lepší; „kdyby to vyšlo u obou stejně, hypotéza
padá". Vyšlo to u obou STEJNĚ:

| blokovaný pick (placebo našlo, P38 ne) | dwarf (n=6 898) | wood-elf (n=4 678) |
|---|---|---|
| pick BLÍŽ lajně než nosič | 74,1 % | 76,2 % |
| mean vzdálenost od lajny: pick / nosič | 2,87 / 4,95 | 2,96 / 4,91 |
| pick má NIŽŠÍ odpor koridoru než náhrada | 54,6 % | 60,7 % |
| mean Δodpor (pick − skutečná náhrada) | −1,00 | −1,31 |
| mean prog: pick / náhrada | 4,25 / 2,72 | 3,92 / 2,00 |
| náhrada = nosič STOJÍ (fallback na 0) | 31,3 % | 48,3 % |

Brzda u obou ras ruší tentýž tah: **dlouhý boční výběh K LAJNĚ do nízkého
odporu**. Hypotéza „brzda ruší trpaslíkovi výběh do zdi" je vyvrácená
dvakrát: pick míří OD odporu (ne do zdi) a vzorec je rasově identický.

**(1b) Most přes corridor_resistance NEFUNGUJE — a to je informace.**
Zrušený pick má odpor NIŽŠÍ než to, co po brzdě zbude (mean −1,0/−1,3);
σ-tabulka (odpor −9,6σ) by tedy brzdě předpověděla ŠKODU, a trpaslíkovi
přitom pomáhá. ⇒ Hodnota brzdy neteče přes odpor koridoru; teče přes
**geometrii lajny** — kritérium klece je nechtěný, ale funkční **vynucovač
U1** (pole ≤ 1 od lajny nemá 4 rohy; přetížený kraj dělá rohy špinavé).
To také vysvětluje rasovou asymetrii UŽITKU bez rasové brzdy: podle U1
(19.08., změřeno) je hák lajny **MA toho, kdo na nosiče dosáhne** — trpaslík
tu čelí wood-elfům (max MA 9, ×1,89), elf trpaslíkům (max MA 6, ×1,31).
Táž zrušená chyba stojí trpaslíka hodně a elfa skoro nic. Navíc elf platí
za brzdu víc: ve 48,3 % blokovaných kol mu fallback nosiče ZASTAVÍ
(= znovuvyrobené P39), u trpaslíka 31,3 %.

**(1c) Jak silný je rasový nález vůbec.** Cena kritéria (delta-of-deltas,
očištěno o posun základen): dwarf +0,0343 ± 0,0139 (+2,46σ), elf −0,0104
± 0,0157 (−0,66σ); asymetrie sama **+0,0447 ± 0,0210 (+2,13σ)**, post-hoc,
nepárově. ⚠️ **„Kritérium elfovi ŠKODÍ" prokázané NENÍ** — arm-vs-arm
−0,0265 ± 0,0112 (−2,37σ) je z ~60 % posun základny mezi seedy (−0,0160);
očištěný odhad −0,66σ je slučitelný s nulou. Prokazatelné je jen
„elfovi nepomáhá". Argument zadání „skutečná asymetrie ≥ tabulka" je
aritmeticky správně u trpaslíka; u elfa jde týmž krokem o KOREKCI směrem
k nule, ne o zesílení.

**(1d) Q-B: mechanismus na efekt STAČÍ.** Brzda mění trpaslíkovi
**2,299 kola/hru** (blokováno; z toho 29,5 % byla v korpusu idle kola)
a celkem se volba pole liší ve **4,515 kolech/hru**. Na cenu +0,034 TD/hru
stačí ~0,015 TD na blokované kolo — při per-turn ztrátě míče u lajny vs
střed (U1: rozdíl ~8–10 pp proti rychlému soupeři) a konverzi držení na TD
13,3 % je to řádově přesně ono. Efekt NENÍ větší než mechanismus; nic
dalšího není třeba najmenovat.

**(1e) Q-C: attrition PADÁ.** Δattr trpaslíka (P38 − placebo, rameno na
něm): −0,0541 ± 0,0171 (−3,17σ) — reálné, pod placebem víc chodí a víc
schytá (potvrzuje čísla zadání 1,043/0,989 = sum(home_attr) na 6 800+6 800
hrách). Ale přes within-arm sklon TD~attr (−0,044 až −0,062 TD/bod) to
vysvětlí **+0,0029 z +0,0465, tj. 6,2 %** ΔTD. U elfa Δattr +0,14σ,
vysvětlí 0,9 %. ⇒ Zisk nese změna pohybu míče, ne bití; P42 tímhle
kanálem kritérium klece NEnahradí (může být dobrá z vlastních důvodů).
⚠️ Sklon je korelační (špatné hry mají obojí), 6,2 % ber jako horní odhad.

**(1f) Screen (třetí možnost uživatele): elf místo klece NEHRAJE screen,
hraje rozsyp.** Definice zapsaná před měřením (viz docstring skriptu:
hrozby = stojící soupeři ≤ 5 od nosiče; screen-tělo cheb ≥ 2 od nosiče,
mezi nosičem a hrozbou v bboxu+1; SCREEN = ≥ 3 taková těla po dvou
cheb ≥ 2). V blokovaných kolech na desce následujícího logu:
elf **SCREEN 510 z 4 258 klasifikovaných (12,0 %)**, rozsyp 88,0 %
(neklasifikováno bez hrozby do 5: 420 z 4 678); trpaslík 661 z 5 779
(11,4 %) — **stejně**. Engine screen nehraje ani rasou, ani situací.
⚠️ Metr není vyhladovělý počtem (510/661 případů), ale měří NÁHODNÉ
formace — engine screen-doktrínu nemá, takže tohle je nález o absenci,
ne o kvalitě screenů. Geometrické rozřešení „lajna špatná pro klec,
dobrá pro screen" z korpusu NEROZHODNU: nosiče u „screenů" stojí
o ~0,4 pole DÁL od lajny než u rozsypu (5,36/5,25 vs 4,93/4,84), tedy
doktrinální podpis (screen u lajny) v datech není — protože není ani
screen. Jde to rozhodnout až s implementovaným screen chováním (noc).

## (2) Q8 — DOPORUČENÍ

* **Nasadit všem (P38 s kritériem): ANO, teď.** Trpaslíkovi prokazatelně
  nejlepší rameno (+4,59σ proti placebu arm-vs-arm, +2,46σ očištěně);
  elfovi proti jeho nejlepšímu ramenu stojí odhadem −0,0104 ± 0,0157
  (slučitelné s nulou) a proti základně pořád +0,0953 ± 0,0110. Riziko
  špatné volby je asymetrické: placebo všem by trpaslíka stálo 2,5–4,6σ
  prokázaného zisku; P38 všem stojí elfa možná nic.
* **Podmíněně (druhý krok, až po noci z (5a)):** pokud se potvrdí
  mechanismus přes soupeřův dosah, správná podmínka NENÍ rasa, ale
  **max MA soupeře ≥ 8 ⇒ kritérium klece ON** (totéž kritérium jako U1
  „max MA ≥ 8 = zákaz lajny, ≤ 7 = kompromis"). Nepodmiňovat rasou —
  stejná lekce jako U1 a „klec pomáhá všem": pravidlo nezužovat na rasu,
  u které se našlo.
* **Nenasadit a jít na P42: NE.** (1e) — attrition nese ≤ 6 % efektu.
  P42 řešit jako samostatnou položku, ne jako náhradu.

## (3) PŘEJMENOVÁNÍ P38

Ano, ale ne na „boční volnost" paušálně (to říkala souhrnná delta, která
zprůměrovala dvě opačné rasy). Poctivé jméno podle rozkladu:
**„volba pole nosiče: boční volnost + veto polí, kde nemůže vzniknout
klec"**. Většinu zisku (+0,070 z +0,083 dvoustranně) nese boční volnost
(společná s placebem); veto přidává trpaslíkovi +0,034 a funguje jako
implicitní U1 (lajna + přetížené kraje), **ne** jako stavba klece —
`expandCage` klec podle kritéria nestaví (0). „Kritérium klece" je
v názvu obhajitelné jen jako tvar veta, ne jako mechanismus zisku.

## (4) PODÍLY S JMENOVATELI (a vyhladovělost metrů)

Korpus: 3 000 her; kola se stojícím nosičem dwarf 17 728 · we 13 401.
* placebo najde pole: dw 17 523/17 728 (98,8 %) · we 12 912/13 401 (96,4 %)
* P38 najde pole: dw 10 625/17 728 (59,9 %) · we 8 234/13 401 (61,4 %)
  (konzistentní s 58,9 % z 20.08. na idle podmnožině — replika sedí)
* blokováno: dw 6 898/17 728 (38,9 %) · we 4 678/13 401 (34,9 %)
* oba najdou, různá pole: dw 6 647/17 728 (37,5 %) · we 4 638/13 401 (34,6 %)
* relaxace kotvení ruší blok: dw 250/6 898 (3,6 %) · we 271/4 678 (5,8 %)
* pick blíž lajně: dw 5 111/6 898 (74,1 %) · we 3 566/4 678 (76,2 %)
* pick s nižším odporem: dw 3 768/6 898 (54,6 %) · we 2 838/4 678 (60,7 %)
* fallback zastaví nosiče: dw 2 162/6 898 (31,3 %) · we 2 258/4 678 (48,3 %)
* screen: we 510/4 258 (12,0 %) · dw 661/5 779 (11,4 %); neklasifikováno
  we 420/4 678 · dw 1 119/6 898
* X3 výplně u starého pole PŘED pohybem nosiče: dw 2 703 (z 10 028 u
  starého; u nového 19 280) · we 1 889 (z 6 715; u nového 14 547)
* noci: 13 600 + 13 600 řádků, 8+8 shardů; per-race průměry na n=6 800
* attrition: sum(home/away_attr), n=6 800 na buňku; Δattr dw −3,17σ
Vyhladovělost: žádný z hlavních metrů není vyhladovělý (tisíce případů).
Výjimky ohlášené výše: screen měří náhodné formace (absence doktríny),
X3 je kontaminované REPOSITION pohyby, sklon TD~attr je korelační,
rasový rozpad nocí je post-hoc a nepárový (46,1 % náhodné shody základen).

## (5) CO Z TĚCHTO DAT NEJDE — ZADÁNÍ PRO NOC

* **(5a) Rozhodnout „vlastní pomalost vs soupeřův dosah"** — v dw-we jsou
  dokonale zaměněné (jedna rasa pomalá, druhá rychlá). Noc: P38 vs placebo
  na ZRCADLOVÝCH matchupech dw-dw a we-we (případně vs human/orc).
  Předpověď mechanismu „soupeřův dosah": kritérium pomůže OBĚMA ve we-we
  (nosič čelí MA 9) a NIKOMU v dw-dw (čelí MA 6). Předpověď „vlastní
  pomalost": pomůže trpaslíkovi v dw-dw a elfovi ve we-we ne. Ostrá
  diskriminace. ⚠️ Rozpočet párů NEpočítat z podílu identických párů
  (lekce CRN 18.08.).
* **(5b) Levnější rasově neutrální náhrada veta**: placebo + veto lajny
  (min(y,14−y) ≥ 3 pro cílové pole nosiče). Blokované picky mají mean 2,9
  od lajny ⇒ veto by zrušilo většinu téže třídy tahů bez blokování
  centrálních obchvatů (u elfa 48,3 % fallback-zastavení by odpadlo).
  Předpověď: vrátí většinu trpaslíkovy ceny +0,034 a elfovi nic nevezme.
* **(5c) Elfí ztráta z kritéria** (−0,66σ očištěně) — rozhodne až párový
  běh P38 vs placebo se STEJNÝMI seedy/CRN na obou ramenech (letošní noci
  mají různé seedy, srovnání nese plný šum základen).
* **(5d) X3 přesně** — vyžaduje logování maker v `TurnLog` (dnes se makra
  nelogují; korpusový odhad nerozliší CAGE od REPOSITION).
* **(5e) Screen vs klec podle geometrie** (doplněk uživatele, bod 4) —
  z korpusu nerozhodnutelné, engine screen nehraje. Nejdřív by musel
  existovat screen-arm (samostatné zadání, ne noc k P40).
* **(5f) Po opravě P45 přeměřit P38 vs placebo** — kritérium dnes měří
  klec v prostředí, kde lehlý blokuje zadarmo; oprava P45 mění cenu
  „čistoty" rohů a může zisk kritéria zvýšit (viz (0) bod 3).
