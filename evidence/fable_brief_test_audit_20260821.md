# ZADÁNÍ PRO FABLE — AUDIT TESTŮ: CO NÁM TESTY VŮBEC HLÍDAJÍ?

⏰ **SPUSTIT AŽ PO rules-parity auditu** *(`fable_rules_parity_20260821.md`)* —
jeho výstup je **vstupem** téhle úlohy. Pouštět je souběžně znamená zaplatit
tutéž práci dvakrát *(obojí porovnává kód s `rules_bb2016.txt`)* a dostat dva
nálezy, které si budou částečně odporovat.

## 0. ⛔ TOHLE NENÍ AUDIT POKRYTÍ KÓDU

**Obě dnešní pravidlové vady měly pokrytí a testy prošly.**

⭐⭐⭐ **Nejhorší z nich byl test SÁM:**
```
TEST(MoveHandler, StandUpNotEnoughMA) {          // smazáno 21.08.
    gs.getPlayer(1).movementRemaining = 2;
    auto result = resolveStandUp(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.success);                 // ⛔ BB2016 ř. 691-693 žádá HOD 4+
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}
```
Někdo napsal kód, pak napsal test, který s kódem **souhlasí**, a od té chvíle
byla vada **certifikovaná**. Důsledek: **Treeman s MA 2 nevstal ani jednou
z 911 sražení**, a plošně se ležící postavil v **0,4 %** případů.
`countAssists` je testovaná taky — jen nikdo netestoval, že ji volá **faul**,
kde Guard platit nesmí *(BB2016 ř. 8160)*.

⇒ **Chybějící osa není pokrytí KÓDU, ale pokrytí PRAVIDEL.** Otázka nezní
*„je ten řádek otestovaný"*, ale **„je to pravidlo otestované, a proti jakému
zdroji"**.

## 1. Úloha A — TESTY, KTERÉ FIXUJÍ CHOVÁNÍ BEZ OPORY V PRAVIDLECH ⭐ *(hlavní)*

Nejlevnější a podle nás nejúčinnější. Projdi **572 testů** v `engine/tests/`
a najdi ty, které **tvrdí něco o herní mechanice** a přitom:
* **necitují pravidlo** *(žádný odkaz na řádek `rules_bb2016.txt`, žádné
  zdůvodnění — jen „takhle se to chová")*; **a zároveň**
* to tvrzení **jde ověřit proti pravidlům**.

U každého takového rozhodni: **✅ souhlasí s pravidly** *(jen chybí citace —
doplnit)* · **⛔ ODPORUJE jim** *(certifikuje vadu — to je nález)* ·
**➖ pravidla o tom nic neříkají** *(je to naše volba, ne pravidlo — a to má
být v testu napsané)*.

⚠️ **Nesnaž se to udělat pro všech 572.** Ohranič se na testy, které se dotýkají
**pravidlové mechaniky** *(pohyb, blok, faul, odsun, dodge, GFI, vstávání,
přihrávka, sebrání, zranění, dovednosti)*, a přiznej, kolik jsi jich prošel
a kolik vynechal. **Vytiskni jmenovatel** *(lekce 13.08./18.08.)*.

## 2. Úloha B — PRAVIDLA BEZ TESTU

Opačný směr, a **drahý** — pravidlová kniha je velká.
⇒ **Ohranič to na to, co vůbec implementujeme**: projdi `SkillName` v
`engine/include/bb/enums.h` a hlavní akce, a řekni, které z nich **nemají ani
jeden test**. ⭐ Precedens: `PilingOn` je v enumu a v celém `engine/src/` se
nevyskytuje *(T5.15)* — mrtvá hodnota. **Kolik takových je?**

## 3. Úloha C — CO JE V N/A NAŠICH KONTROL

⚠️ **Tohle už ve frontě máme jako T2.18** *(uživatel 20.08.)* — sem patří jako
táž rodina, ne jako nová položka.
Kontrola, která vrací N/A, se u nás opakovaně četla jako „nic tam není".
**Doložený případ:** `plan.*` je `NOT_CONSULTED` ve **100 %** kol, všechna pole
nula — **celý rok**, a nula se četla jako fakt.
**Umí každá kontrola říct, KOLIK případů skončilo v jejím N/A a proč?**

## 4. Vstup, který dostaneš

Výstup rules-parity auditu. **U každého tam nalezeného rozporu kódu a pravidla
se rovnou zeptej: „a proč to nechytil test?"** — to je mnohem cílenější než
procházet testy naslepo, a je to zároveň odpověď na uživatelovu otázku, jestli
se tyhle vady dají hledat **systematicky**, nebo jen tak, že o nich náhodou
přijde řeč.

## 5. ⭐ Proč to vzniklo — kontext, který má nést váhu

Uživatel 21.08. si všiml, že **obě dnešní pravidlové vady vzešly z ROZHOVORU,
ani jednu nenašla kontrola** — a to máme **572 testů** a sadu kontrol `K*`,
z nichž **ani jedna se nedotýká vstávání ani asistencí u faulu**.
⇒ Hypotéza: **kontrola ověřuje to, co nás napadlo ověřit**, a pravidlová vada
je z definice to, co nás nenapadlo. **Tvůj úkol je zjistit, jestli se z toho dá
ven systematicky** — nebo jestli je aparát strukturálně odkázaný na to, že si
toho někdo všimne.

## 6. Tvrdá omezení

⛔ **NEOPRAVUJ testy ani kód. NEPŘESTAVUJ engine.** Je to audit, ne úklid.
⚠️ Může běžet víkendový sběr — nespouštěj nic dlouhého a drž se čtení.
⚠️ **Edice je BB2016** *(`rules_bb2016.txt`)*, ne CRP — zápis ze 07.08. je
ztotožnil a u starých položek je „ověřeno proti CRP" **špatný zdroj**.

**Výstup** do `evidence/fable_test_audit_20260821.md`: verdikt k hypotéze ·
tabulka `test | co tvrdí | pravidlo | ✅/⛔/➖` · seznam neotestovaných
dovedností a akcí · pořadí podle dopadu · a **jmenovatele u všeho**.
