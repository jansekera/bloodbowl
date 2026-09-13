# KLEC V PHP — CO SE 12.09.2026 SKUTEČNĚ NAIMPLEMENTOVALO

⛔⛔ **ČTI NEJDŘÍV STARŠÍ ZÁZNAMY — DOKTRÍNA UŽ EXISTOVALA.** Uživatel 12.09.:
*„tohle všechno jsme už ale probírali dříve velmi podrobně při chystání klece."*
Má pravdu a je to moje chyba v postupu: dnešek jsem odvozoval znovu, místo abych
si přečetl, co je hotové.

| soubor | co v něm je |
|---|---|
| `cage_corner_doctrine_20260807.md` | **doktrína rohů** — závazný vstup uživatele |
| `cage_ma_cap_20260819.md` | **strop klece z nejpomalejšího MA rohu** *(dnes jsem ho „objevil" znovu)* |
| `cage_absent_and_collapse_20260820.md` | proč se klec nestaví a kdy se hroutí |
| `cage_built_20260910.md` | **napřed ověřit, že klec vůbec stavíme, teprve pak měřit posun** *(dnes taky znovu)* |

⇒ Tenhle soubor **nenahrazuje** doktrínu, jen říká, **co z ní je v PHP kouči
hotové** a čím se to tam prosadilo.

## Co skupina „klec" v PHP umí k 12.09. *(a je na to test)*

**Cíl podle uživatele:** *„sestavit klec co nejdříve a pohnout s celou klecí
s nosičem kupředu a i do boku."* — `tests/AI/CagePlaybookTest.php`, dvě kola
na vlastní polovině, **zelené**.

| pravidlo | jak je v kódu |
|---|---|
| klec = nosič + **čtyři diagonální rohy** | roh `+1,5`; hrana jen jako vzdálenost v gradientu |
| **čistý roh je podmínka** *(„jinak o míč přijdeme")* | roh se soupeřem vedle `−2,0` |
| **napřed sestavit, pak hnout** | bez klece má nosič strop **1 pole** — ale jen když má kdo přijít |
| **posun podle nejpomalejšího**, dohon přes GFI | strop `min MA` rohů, nad ním `−0,9`; přes GFI jen `−0,25` |
| kdo v rohu stojí, **zůstává stát** | `STAND_PAT` s bonusem `+1,8` |
| **nosič nebojuje** | blok, blitz, multiblok a faul mu nejsou nabídnuty *(i v Greedym)* |
| **uvnitř klece se míč nepředává** | hand-off, pass a TTM zakázány, když klec stojí |
| nosič **nesmí skončit vedle soupeře** | `−5,0` |
| nosič si vybírá **místo pro celou klec** | `+0,6` za každý čistý roh cílového pole |
| **čekání jedno pole před TD zónou** | při zdržování `+2,5` pro `distToEndZone == 1` *(přední rohy už v zóně)* |
| **rozpuštění klece, když je natěsno** | `musiVyrazitSam()`: `zbývá kol ≤ kol na cestu + 1` ⇒ strop se neuplatní |

## ⏰ NEHOTOVÉ — a proč

- **Volný běh proti pomalému soupeři** *(uživatel 12.09.: „proti pomalému
  soupeři může nosič na naší půli běžet sám rychleji, dokud se k němu soupeř
  nedostane")*. Naimplementováno jako *„na cílové pole nedosáhne ani nejbližší
  stojící soupeř (MA + 2 GFI)"* — a **vráceno zpátky**, protože to shodilo
  dva testy: ve scénáři *„sestav klec v prvním kole"* jsou soupeři daleko,
  takže výjimka sepne vždycky a klec pak nevznikne.
  ⇒ **Ty dvě pravidla si odporují a rozhodnutí patří uživateli:** buď se volný
  běh pustí jen dokud klec ještě nestojí, nebo se drží přednost stavění klece.
- **`PHP36` — clona ze zbylých šesti hráčů** *(„budou bránit soupeři cestu
  ke kleci")*: nezačato.

## ⛔ KDO PATŘÍ DO ROHŮ — DOKTRÍNA, KTEROU JSEM 13.09. POPŘEL

Navrhl jsem *„do rohů nedávat nejpomalejšího hráče"*. **Uživatel to odmítl:**
*„tohle jsme už zkoušeli — rychlejší mají jiné úkoly — do rohů patří longbeard."*

⭐ A stojí to v záznamech **od 07.08.2026**: `cage_corner_doctrine_20260807.md`
r. 30 — *„CÍLOVÝ STAV: klasická klec s Longbeardy na rozích, jak ji hrají lidé."*

⇒ **Rohy = pomalí, ale odolní** *(Block, Tackle, Thick Skull)*. Rychlí hráči
mají clonu, blitz a příjem. ⛔ Kouč dnes rohy obsazuje **podle vzdálenosti**,
role neřeší — až se na to sáhne, musí platit tohle, ne „kdo je blíž".

⭐ **A vychází to i časově:** Longbeard má MA 4, potřeba je ~2,8 pole za kolo
*(19 polí od výkopu, minus kolo na sestavení a kolo na doběhnutí)*. Rezerva
~1,2 pole na kolo, tedy klec smí za půli dvakrát stát a TD pořád stihne.

## 📊 ZMĚŘENO 13.09.2026 — SEKCE SPLNĚNA *(8 zápasů, dev rostery)*

```
klec na startu kola                     75 kol
  ⭐⭐ CÍL: celá, čistá, posunutá        47    62,7 %
  kolo skončilo touchdownem (neměří se) 12
  aspoň jeden roh není čistý            16    21,3 %
       rohů vzatých soupeřem             0
       prázdných rohů                   16
       našich se soupeřem vedle          1
  posun nosiče: průměr 4,56 pole, maximum 7
```

⭐ **Rozpočet vychází s rezervou:** potřeba **2,8 pole na kolo**
*(19 polí od výkopu, minus kolo na sestavení a kolo na doběhnutí)*,
naměřeno **4,56**. A není to jen papír — **12 kol skončilo touchdownem**.

⭐ **Cesta sem** *(vše za 12.-13.09.)*: klec vznikala v **2,2 %** kol →
po tlumení běhu vpřed **5,2 %** → po zbytku oprav **25,3 %**.
Cíl se nedal číst vůbec, dokud se neopravil rozbitý čítač *(`071b9f83`)*,
a posun nešel číst, dokud se z něj nevyňala kola s touchdownem.

⛔ **JEDINÁ ZBÝVAJÍCÍ PŘÍČINA NEÚSPĚCHU JE PRÁZDNÝ ROH** *(16 ze 17)*.
Soupeř roh nevzal ani jednou. Klec se tedy nerozpadá tím, že by ji někdo
rozebral — **někdo z posádky nedojde**. ⇒ To je úloha pro **clonu** (`PHP36`)
a **prorážení vpřed** (`celotah A12`), obojí uživatelem odloženo.
