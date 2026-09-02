# SMYČKA V MAKRO CHŮZI — ZACHYCENO 300 SITUACÍ (02.09.2026)

> **Uživatel 02.09.:** *„máš k té smyčce i příklad ze hry?"* — neměl jsem.
> Čítač počítal, ale situaci nezaznamenával, takže nebylo o čem diskutovat.

**Jak zachyceno:** `BB_WALKLOOP_DUMP=<soubor>` zapne dump prvních 300 případů
(pozice všech hráčů, cíl chůze, krok, zbývající pohyb). ⚠️ Bez té proměnné je
to **úplně vypnuté** — v noci ani v běžném běhu to nestojí nic.

## ⭐⭐ NÁLEZ: VADA NENÍ V CHŮZI, JE V ZADÁNÍ CÍLE

```
cíl je OBSAZENÝ jiným hráčem   284/300 = 95 %
  z toho VLASTNÍM hráčem       267/300 = 89 %
  z toho soupeřem               17/300 =  6 %
cíl je SÁM MOVER                 0/300 =  0 %
vzdálenost od cíle = 1         263/300 = 88 %
zbývající pohyb                1-4 pole  (hráč MÁ čím jít)
obsazující STOJÍ                273/284
```

**Co se děje:** makro vypustí `REPOSITION` na pole, **kde už stojí náš vlastní
hráč**. Mover tam dojde, zastaví se **vedle** (vzdálenost 1), pohyb mu ještě
zbývá — ale vstoupit nemůže. Uhne stranou, z nového pole je nejlepší krok zpět,
a pojistka proti smyčce to utne. Makro selže.

⇒ **Chůze se chová správně.** Nemá kam jít, protože **cíl neexistuje jako volné
pole.** Oprava nepatří do `movePlayerToward`, ale do **výběru cíle**.

## ⛔ ČTYŘI MOJE HYPOTÉZY, KTERÉ TOHLE VYVRÁTILO

Za 01.-02.09. jsem k té smyčce vyslovil pět domněnek a **všechny byly mimo**:
oscilace z Čebyševovy metriky · dvoupolová obcházka *(`OBCHAZKA 0`)* ·
tacklezóny mezi stejně dlouhými cestami · promrhaná rezerva · nedosažitelný cíl.
⭐ **Rozhodlo až zachycení situace, ne úvaha o kódu.** Zapsáno jako doklad
k [[feedback_wrong_result_looks_normal]].

## ⭐ SOUVISLOST S `C4` V CELOTAHU

`evidence/celotah_situace.md`, situace **C4: „nosiči zavazí VLASTNÍ hráči"**
*(M11, 149/149 vlastními)*. Tohle je **tentýž jev o patro níž**: nejenže naše
těla brání nosiči v postupu — **makro na ně přímo posílá další hráče.**

## ⏰ CO TO NEŘÍKÁ *(a nesmí se dopočítat)*

* **Kolik to stojí.** Když makro selže, netuším, jestli plánovač vybere něco
  rozumného, nebo přijde o aktivaci. ⇒ **Bez toho to není vada do fronty A.**
  Odpoví na to audit celotahu *(otázka 5)*.
* **Jestli je cíl obsazený UŽ PŘI VÝBĚRU, nebo se tam někdo přesune až potom.**
  Dump zachycuje stav **v okamžiku zaseknutí**, ne v okamžiku volby. To je
  rozdíl mezi „makro zadalo nesmysl" a „mezitím se to změnilo" —
  a rozhoduje o tom, kde oprava patří.

## ZADÁNÍ NA DALŠÍ KROK

Zachytit stav i **v okamžiku vypuštění makra** a porovnat. Teprve pak se dá
říct, jestli je vada ve výběru cíle, nebo v tom, že plán zastará během tahu.
