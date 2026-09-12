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
