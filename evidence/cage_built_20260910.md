# STAVÍME VŮBEC KLEC? — KONTROLA PŘED MĚŘENÍM POSUNU (10.09.2026)

**Zadání uživatele 10.09.:** *„Hlavní je kontrola, že stavíme správnou a čistou
klec — předtím jsme kontrolovali posun klece a až potom jsme zjistili, že klec
nestavíme; tak nemůžeme měřit její posun."*

⇒ Je to jeho vlastní pravidlo z `celotah_situace.md`: ⛔ **NEMĚŘIT, CO MODEL
NEUMÍ.** Skript `diag_cage_built_20260910.py`, korpus `corpus_m11_20260909_data`
*(1000 her, engine `cf8634e8`, produkční nastavení)*. Definice drží
`cageSnapshot` *(`cage_advance.cpp:113-135`)*, ať jsou čísla srovnatelná s tím,
co engine počítá sám.

## ⛔⛔⛔ ODPOVĚĎ: KLEC NESTAVÍME. ČISTÁ KLEC JE V 8,3 % KOL.

**Trpaslík, 7 173 kol se stojícím nosičem** *(histogram, ne průměr — na tom to
stojí)*:

| rohů obsazených naším stojícím hráčem | | |
|---|---|---|
| **0 rohů** | 2 372 | **33,1 %** |
| 1 roh | 1 548 | 21,6 % |
| 2 rohy | 1 314 | 18,3 % |
| 3 rohy | 909 | 12,7 % |
| **4 rohy** | 1 030 | **14,4 %** |
| *(zbytek — musí být 0)* | 0 | |

⇒ **průměr 1,54 ze 4 — a právě ten se dřív čítal.** Histogram ale říká něco
jiného než *„většinou stavíme dva"*: **v každém třetím kole nestojí ani jeden
roh**, a plný prstenec je jen v **14,4 %**.

| ⭐ ČISTÁ KLEC *(4 rohy · 0 označených · nosič mimo TZ)* | **593 = 8,27 %** |
|---|---|

**Pozitivní kontrola:** nejlepší dosažené kolo = **4 rohy, 0 označených, nosič
mimo TZ** ⇒ čítač jedničku najít **umí**, těch 8 % není rozbité měřidlo.

## ⛔⛔ A DOMINANTNÍ DÍRA NENÍ ROH — JE TO NOSIČ SÁM

| | trpaslík | ork |
|---|---|---|
| **nosič sám v soupeřově TZ** | **53,1 %** | 41,9 % |
| aspoň 1 roh označený | 30,7 % | 24,0 % |
| čistá klec | 8,27 % | 12,10 % |

⭐ **Sedí to na doktrínu zapsanou v kódu** *(`cage_advance.cpp:35-38`, měřeno
11.08.: „nosič končí označený ve 40 % našich advance kol proti 11 % u rohů, takže
je to větší díra — a to byl opak toho, co předpověděla revize kódu")*.
⇒ **Dnes je to 53 %, tedy ještě víc.** Klec kolem markovaného nosiče je
bezcenná, takže **tohle je první vada okruhu, ne targeting rohů.**

## ⭐ ALE TVAR SE NEZTRATIL — DIAGONÁLY SE PREFERUJÍ 2,3×

Aby se to nečetlo jako „těla se poflakují kdekoliv": obsazenost **na pole** je
**38,5 % u diagonál** *(1,54 ze 4)* proti **17,0 % u ortogonál** *(0,68 ze 4)*.
⇒ **Nějaký klecový záměr v enginu existuje** — chybí ho **dokončit**, ne
nasměrovat. A těla u nosiče vůbec jsou: aspoň jedno sousedí v **79,2 %** kol.

## ⇒ CO Z TOHO PLYNE PRO POŘADÍ PRACÍ *(moje odvození, ne uživatelova věta)*

1. ⛔ **Měřit posun klece je pořád předčasné** — přesně jak uživatel řekl. Klec
   je celá jen v 8,3 % kol; posun něčeho, co ve 92 % neexistuje, není měřitelný.
2. ⭐⭐ **První položkou okruhu se stává `P42`** *(„nosič nekončí v kontaktu",
   kontrola `K38` stojí, chybí rameno)*, ne `W-CIL`. Nosič v TZ v 53 % kol je
   větší díra než to, kam chodí doprovod — a je to jeho vlastní pohyb, tedy
   **jedno tělo, žádná koordinace** *(na rozdíl od doplnění rohů)*.
3. ⏰ **`W-CIL` zůstává druhý** — a jeho zadání se tím zpřesnilo: nejde
   o *„špatný směr"*, ale o **doplnění chybějících rohů** *(0 rohů ve 33 % kol
   při 79 % kol s aspoň jedním tělem u nosiče ⇒ těla tam jsou, jen ne na
   diagonálách)*.
4. ⚠️ **A pozor na past, do které jsme málem šlápli znovu:** kdyby se teď
   opravovalo „následování těl do boku" *(P32/P46/W-CIL)*, měřilo by se to na
   populaci, kde klec v 92 % kol není — tedy **stejná chyba jako s posunem**.
