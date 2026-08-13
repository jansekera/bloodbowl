# FRONTA ÚKOLŮ — 13.08.2026
*Nahrazuje `task_queue_20260812.md`. Čti tenhle.*

---

## CO SE DNES UZAVŘELO

| | výsledek |
|---|---|
| **T0 dluh** (3 pravidlové opravy z 12.08.) | výsledkově **neutrální**, žádná nad 1,5 SE; determinismus harnessu ověřen bit-identicky |
| **T3.1 brána klece** | **ZAMÍTNUTA** — 1500 párů, dw-we −0,0297 (−2,0σ), dw-sk +0,008. Nezahazovat: chybí jí plán trasy, ne schopnost |
| **audit měřicího aparátu** | 7 míst, kde se měřilo něco jiného; oprava `Check(ok,n,deg)` + N/A |
| **K34/K35/K36** | napsané a změřené |
| **testy** | 530/530 zelených |

---

## ⭐⭐⭐ HLAVNÍ ZJIŠTĚNÍ, PODLE KTERÉHO SE ŘADÍ ZBYTEK

**Co předpovídá TD** (plné drivy ≥7 kol, 195 drivů):

| | σ |
|---|---|
| K9a tempo | **4,2σ** |
| bloků na kolo | **2,7σ** |
| čistota rohů / `FB2 ≤ 1` | 2,6σ |
| **špinavých rohů** | **−2,2σ** |
| REACH0 (počet) | −1,8σ |
| *počet rohů klece* | *−0,2σ = nic* |
| *K33, K34 jako ano/ne* | *0,6σ / 0,8σ = nic* |

**Tempo a bití jsou dva nezávislé prediktory a brána je proti sobě vyměnila.**
Zvedla tempo (20,6→28,4 %) a shodila bití (76,1→73,2 %) i čistotu rohů
(79,4→72,6 %) ⇒ nula.

**Bilance plných drivů:** skórujeme **19 %**, ztratíme míč **38 %**,
došla kola 31 %, pomalá klec 12 %, míč nezískán **0 %**.

---

## POŘADÍ

### P0 — doměřit na velkém korpusu *(běží přes noc, 3000 her)*
1. **Předpovídá blok v kole N čistotu rohů v N+1?** Když ano, je doktrína
   „bít ty, kdo špiní roh" potvrzená a P2 má zelenou.
2. REACH0 jako počet (na 195 drivech jen −1,8σ)
3. K36 `LOCKED` — koše měly n=4 až 16
4. skórovací podíl po fázích
5. ⭐ **Řetěz „špinavý roh → zamčené tělo → chybějící roh příště"**
   *(uživatel 13.08., z vlastní hry: „šel jsem přes varování do špinavých
   rohů — výsledek nebyl ztráta míče, protože soupeř byl skaven a ne
   wood-elf, ale následná ztráta rychlosti a ztráta těl vhodných a volných
   pro rohy později.")*
   Měřit: koreluje počet špinavých rohů v kole N s počtem zamčených těl
   a s počtem obsazených rohů v N+1?
   **Vysvětluje, proč je špinavý roh −2,2σ, i když ztrátu míče nezpůsobí
   hned:** účet nepřijde v kole, kdy se chyba udělá.
   ⇒ Táž chyba má proti různým soupeřům různou splatnost: proti wood-elfovi
   okamžitě (ztráta míče), proti skavenovi na splátky (zámky). Odložená
   varianta je nebezpečnější, protože se u stolu nespojí s příčinou.

### P1 — přepsat `K33` a `K34` na spojité *(levné, opravuje metr)*
Jako prahy nepředpovídají nic (0,6σ / 0,8σ), jako počty patří k nejsilnějším.
Platí i pro E1: *„ani jeden otevřený roh"* je správný **cíl**, ale jako
**kontrola** se má měřit `REACH0` jako počet — rozdíl mezi 1 a 4 je podle E1
mezi 8,3 % a 33 % ztráty míče.

### P2 — doktrína „BÍT TY, KDO ŠPINÍ ROH" *(nejpřímější akce z dnešních dat)*
Špinavý roh je **−2,2σ**, počet rohů **0σ**. Čistý roh se nevyrábí
obsazováním, ale sražením toho, kdo ho špiní.
⇒ Priorita blitzu: soupeř sousedící s rohem klece **před** ostatními cíli.
⚠️ Čeká na P0.1.

### P3 — fázový plán trasy *(doktrína uživatele, 13.08.)*
Postup má tři fáze: sólo Runner + kick-off return → klec → sólo výběh
u endzone. Důsledky:
* **rovnoměrná podlaha K9a je špatný model** — 3,14 pole/kolo trpaslík neumí,
  fázově tutéž vzdálenost ujde
* **bez fáze v modelu nejde odlišit chybu od záměru** („nezbylo na doplnění
  rohů" je vada ve fázi 2 a správné rozhodnutí ve fázi 3)
* oprava **kick-off returnu** tím přestává být okrajová (fáze 1 na něm stojí)

### P4 — `CHAIN_SCORE` je aktivní bug
Krok 1 (pass) spálí `passUsedThisTurn`, krok 2 (hand-off) se pak nenabídne ⇒
**přihrávka se provede, předání selže, tah je pryč.** Opravit nebo odstranit.

### P5 — hand-off pro výměnu nosiče
Filtr váží předání cenou přihrávky (33 %), i když by ho provedl jako hand-off
(83 %); práh 0,5 zahodí i Runner→Runner (44 %). Kritérium je **„nosič je
špatný"** (AG≤2 bez Sure Hands a nedoběhne), ne „příjemce je lepší".
⚠️ **Priorita snížena:** Longbeard nese jen **9 %** kol (staré „44–49 %" bylo
předopravní číslo z korpusu 30.07.). Očekávaný zisk ~0,17 pole/kolo je pod
šumovým dnem ⇒ **neměřit A/B, jen opravit a ověřit na kontrolách.**
Návrh patche: `scratchpad/handoff_fix_plan.md`.

### P6 — zobecnit item 14 na výběr cíle a na pickup
* **BLITZ**: cíl se vybírá podle surových kostek, blitzer podle kostek **+ cesty**
* **PICKUP**: váží cenu sebrání, ne cestu k míči (připouští 2 GFI = 30 % pád)
* nástroj existuje: `estimateApproachFailChance` (`macro_actions.cpp:206`),
  použitý jen dvakrát, oba u blitzu
* porušené vlastní pravidlo z 03.08.: *„BLITZ pohyb → obecný pohyb"*

### P7 — sdílený limit pass / hand-off *(parita, ZHORŠÍ NÁS)*
CRP má dva nezávislé limity, engine jeden (`passUsedThisTurn`). Soupeř tedy
nemůže udělat hand-off + pass v jednom kole a utéct nám. **Jediný dnešní
nález, který po opravě hraje proti nám** ⇒ měřit zvlášť.

### P8 — výběr cíle faulu
Bere **prvního ležícího v pořadí sousedních polí**, nehodnotí nic. Přitom
z 12.08. víme, že Gutter Runner je 4,4× lepší cíl a Thick Skull se nefauluje.

---

## ⚠️ ZNÁMÁ OMEZENÍ KORPUSU *(zapsat, ať se podle nich nenavrhuje)*
* **Soupeřova AI nehraje proti našim slabinám cíleně** — Runner nevypadne ze
  hřiště ani jednou ve 120 hrách, protože si pro něj nikdo nechodí. Lidský
  soupeř by to dělal (AG3 máme jen 4 z 11).
* **Sdílený limit pass/hand-off** dělá hbité rasy slabšími, než jsou.
* **SPP se nesledují** — elfí AI nemá důvod házet kvůli bodům, ve skutečné hře
  ho má.

## OTEVŘENÁ OTÁZKA Č. 1
**Proč zlepšení procesu nevede k výsledku** — brána zlepšila skoro všechny
kontroly a chess se nehnul. Část je vysvětlená (vyměnila tempo za bití), ale
zbytek ne. Dokud to nevíme, je „úplná procedura jako nulová hypotéza pro
učení" postavená na kontrolách, o kterých nevíme, že k něčemu jsou.
