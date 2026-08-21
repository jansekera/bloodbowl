# ZADÁNÍ PRO FABLE — RULES-PARITY: ODVOZUJEME VŠECHNO OD BLOKU?

## 0. ⏰ PROČ TO HOŘÍ PRÁVĚ TEĎ

**Právě běží víkendový sběr korpusu** — 15 dvojic × 1 200 her, do neděle večer.
⭐⭐ **Vadný METR korpus neznehodnotí** *(jen se podle něj hůř čte)*, ale
**vadné PRAVIDLO ano.** Dnes nás jedno stálo celý korpus z 19.08.:
oprava vstávání změnila TD o **40 %** *(0,740 → 0,443, párově, −6,12σ)*.
⇒ **Když v enginu leží další vada téhle velikosti, chceme to vědět DNES**,
dokud se dá sběr zabít a pustit znovu — ne v neděli po 54 hodinách.
**Tvoje nálezy proto řaď podle jednoho kritéria: mohlo by to znehodnotit
korpus, který se sbírá právě teď?**

## 1. Hypotéza k rozhodnutí

⭐⭐⭐ **Kód odvozuje všechno od BLOKU, protože blok byl první, co se
implementovalo. Akce, které blok NEJSOU, dostávají blokovou logiku.**

**Dva doložené případy, oba z 21.08.:**

* **VSTÁVÁNÍ = pohyb, který není pohyb.** `isFreeToAct()` žádá STANDING, takže
  žádné makro na ležícího nesáhlo, a `movePlayerToward` bral „ležící, už stojí
  na cíli" jako *dorazil* ⇒ přesun na vlastní pole byl no-op. **Výsledek: ležící
  se postavil v 0,4 % případů (1 067 z 280 719), a to při KAŽDÉM MA.** K tomu
  `resolveStandUp` pod 3 MA rovnou selhal místo hodu 4+, a nenastavoval
  `hasMoved`, takže po vstání šel blok. *(P45, opraveno dnes.)*
* **FAUL = asistence, která není bloková.** `resolveFoul` volá `countAssists`,
  a ta má v sobě výjimku Guard. **BB2016 ř. 8160: „This skill may not be used
  to assist a foul."** a ř. 1849: *„No player from either side may assist a foul
  if they are in the tackle zone of an opposing player."* ⇒ **žádná výjimka.**
  Guard má v TV1200 **trpaslík 6 a ork 6** hráčů. *(P54, NEOPRAVENO — běží
  sběr.)*

## 2. Kde hledat — akce, které nejsou blok

Projdi **každou** akci a ptej se: *dostává vlastní pravidla, nebo zděděná?*
`MOVE` · `BLITZ` · `PASS` · `HAND_OFF` · `FOUL` · `MULTIPLE_BLOCK` ·
`BALL_AND_CHAIN` · `TTM` · `BOMB` · `GAZE` · vstávání · GFI · dodge · pickup ·
catch · interception · kick-off události · throw-in.

**Konkrétní podezření k prověření** *(ne vyčerpávající seznam — najdi další)*:
1. **Sdílené pomocné funkce.** `countAssists` se ukázala jako sdílená mezi
   blokem a faulem, ačkoli pravidla se liší. **Kdo další ji volá?** A totéž
   pro `getBlockDiceInfo`, `pickApproachStep`, `countTacklezones`.
2. **Dovednosti podmíněné typem akce.** Guard je *„assist a **block**"*.
   ⭐ **Kolik dalších dovedností má v textu omezení na konkrétní akci, které
   my ignorujeme?** Podívej se hlavně na: **Dauntless · Frenzy · Stand Firm ·
   Juggernaut · Multiple Block · Piling On · Dirty Player · Sneaky Git ·
   Sure Hands · Strip Ball · Wrestle · Tackle · Side Step · Grab**.
3. **Akce, které plánovač NEVIDÍ**, jako vstávání. Existuje akce, která je
   v `getAvailableActions`, ale žádné makro ji nikdy nevybere? *(Vstávání bylo
   přesně tohle — nabízené a nikdy nevybrané, a poznalo se to až po měření
   na korpusu, ne z kódu.)*
4. **Akce, které nic neemitují do logu.** Vstávání neemitovalo NIC ⇒ v korpusu
   se nedalo odlišit *„nikdo nevstává"* od *„nevstávání se neloguje"*.
   **Které další akce jsou v logu neviditelné?**

## 3. ⛔ EDICE: BB2016, NE CRP

⚠️⚠️ Zápis ze 07.08. obě edice **ZTOTOŽNIL**, a řada řádků auditu je
odškrtnutá jako *„ověřeno proti staženému CRP textu"* ⇒ **u všech je špatný
ZDROJ.** Cílová edice je **BB2016**.
Na disku *(mimo git)*: **`rules_bb2016.txt`** a `rules_crp_lrb6.txt`.
**Cituj číslo řádku z `rules_bb2016.txt`**, ať se to dá ověřit.
⭐ Kde se edice liší, řekni to — je to samostatný nález.

## 4. Tvrdá omezení

⛔ **NEOPRAVUJ KÓD. NEPŘESTAVUJ engine. NESPOUŠTĚJ dlouhé běhy.**
⚠️⚠️ **Sběr běží na 10 z 12 jader.** Tohle je **čtení kódu a pravidel**.
Když potřebuješ číslo, ber **starý** korpus `corpus_baseline_20260819_data`,
jednovláknově a s `nice -n 19`.
⚠️ Starý korpus je **z jiného enginu** *(vstávání)* ⇒ čísla z něj jsou
ilustrace mechanismu, **ne platné hodnoty**.

## 5. Výstup

1. ⏰ **NEJDŘÍV: našel jsi něco, co by znehodnotilo PRÁVĚ BĚŽÍCÍ korpus?**
   Ano/ne, a čím. Tohle chci vědět dřív než cokoli jiného.
2. **Verdikt k hypotéze** — je „všechno se odvozuje od bloku" třída, nebo dvě
   náhody?
3. **Tabulka nálezů**: `co | pravidlo (řádek v rules_bb2016.txt) | co dělá náš
   kód | odhad dopadu | znehodnocuje korpus?`
4. **Pořadí oprav podle dopadu**, a u každé jestli je to změna **pravidel**
   *(mění chování — drahé, nová základní čára)* nebo **pozorovací**
   *(smí se batchovat)*.
5. **Co nejde rozhodnout čtením** — jmenuj jako zadání pro měření, ne závěr.

⚠️ **Falzifikace je plnohodnotný výsledek.** Když je engine jinak v pořádku
a tohle byly dvě náhody, řekni to — ušetří nám to audit, který nepotřebujeme.

Výstup ulož do `evidence/fable_rules_parity_20260821.md`.
