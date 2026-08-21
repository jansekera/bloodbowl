# FABLE AUDIT — TESTY: CO NÁM VŮBEC HLÍDAJÍ? (21.08.2026)

Zadání: `evidence/fable_brief_test_audit_20260821.md`. Vstup:
`evidence/fable_rules_parity_20260821.md` (13 nálezů F1-F13).
Edice **BB2016** (`rules_bb2016.txt`, čísla řádků odtud).

⚠️ ROZPRACOVÁNO — píše se průběžně. Není-li na konci „HOTOVO", audit byl useknut.

## JMENOVATELE (průběžně)

* Testů celkem: **577** ve 31 souborech (`grep -c "^TEST" engine/tests/test_*.cpp`).
* Řádků s odkazem na pravidla (BB2016/CRP/ř.) ve všech testech: **40** — doplním rozbor.

## ČÁST 1 — „PROČ TO NECHYTIL TEST?" (F1-F13 z rules-parity)

Prošel jsem test k místu KAŽDÉHO z 13 nálezů. Vzor je jednoznačný — tři mechanismy selhání:

**Mechanismus T-I: test CERTIFIKUJE vadu** (aserce souhlasí s chybným kódem):

* **F1 stun** — `GameState.ResetPlayersForNewTurn` (původní znění, git `4ff6f6d6`):
  `p.state = STUNNED; reset; EXPECT_EQ(p.state, PRONE)  // stunned → prone`.
  Komentář „stunned → prone" je přepis KÓDU, ne pravidla (ř. 703-710 žádá flip
  na KONCI příštího kola). Test byl 21.08. opraven spolu s kódem.
* **F2b faul dublety** — původní `test_foul_handler.cpp` používal kostky
  `{5,4,3,3}` v ČTYŘECH testech: injury 3+3 JE dublet (ř. 1878-1882 → vyloučení
  + turnover), a testy asertovaly průchod bez vyloučení. Vada byla certifikovaná
  4×. Opraveno 21.08. (kostky změněny na `{...,3,4}` s citací l. 1878).
* **P45/StandUpNotEnoughMA** — doloženo v briefu, původní znění potvrzeno
  z gitu (`EXPECT_FALSE(result.success)` pro MA2, žádný hod 4+; ř. 691-693).

**Mechanismus T-II: test testuje jen HAPPY PATH, který s pravidly souhlasí —
chybí negativní případ, a právě v něm je vada:**

* **F3 Frenzy** — `BlockHandler.FrenzyDoubleBlock` testuje PUSHED → druhý blok
  (správně). Komentář ale zní *„Both still standing + adjacent → mandatory 2nd
  block"* — to je PODMÍNKA Z KÓDU, ne z pravidel (ř. 8138-8140: jen po
  Pushed/DS). Chybí test „po BOTH DOWN druhý blok NENÍ". Komentář vadné
  kritérium dokonce slovně fixuje.
* **F11 Wrestle** — `BlockHandler.WrestleBothProne` asertuje `no turnover` pro
  ne-nosiče (správně). Chybí případ „aktivní hráč držel míč → turnover"
  (ř. 8677-8678). Kód vrací ok() bezpodmínečně a žádný test to nezpochybní.
* **F7 fumble** — test se JMENUJE `FumbleOnNatural1`: název sám fixuje špatnou
  sémantiku (ř. 1742-1745: modifikovaný výsledek ≤ 1). Scénář (přirozená 1 bez
  modifikátorů) pravidlům neodporuje — chybí test „hod 2 s modifikátorem −1 =
  fumble", a ten by kód shodil.
* **F9 throw-in** — 4 testy (`ThrowInSideExit*`, `ThrowInCornerExit`,
  `ThrowInFinalBounceOntoPlayer`) pokrývají výhradně dráhy, kde se kód
  s pravidly shoduje (dopad na prázdné pole → bounce, ř. 872-874). Nepokrytý je
  přesně (a) dopad na STOJÍCÍHO hráče → má být catch (ř. 871-872), (b) míč
  letící znovu ven → má se vhodit ZNOVU (ř. 875-877), u nás klamp. Navíc
  komentáře citují **LRB6**, ne BB2016 (obsahově tu shodné, ale špatný zdroj —
  táž rodina jako zápis ze 07.08.).

**Mechanismus T-III: test je VAKUÓZNÍ (nic neasertuje) nebo asertuje neurčitě:**

* **F8 rozptyl passu** — `PassHandler.InaccuratePassScatters` má v kostkách
  NAKÓDOVANOU vadnou šablonu (D8 směr × D6 vzdálenost, komentář „Scatter:
  D8=3(E) D6=1") a pak asertuje `EXPECT_GE(result.turnover + result.success, 0)`
  — **vždy pravda**. Komentář v testu: *„This test verifies the pass completes
  without crash."* Totéž `HailMaryPassScatters3Times` (*„Just verify
  completes"*). Dva testy klíčové mechaniky = smoke testy s nulovou asercí.
* **F10 výkop** — `KickoffHandler.BallScatterStaysOnPitch` asertuje „míč nikdy
  neskončí mimo hřiště" — to projde i po opravě (touchback → isHeld), ale
  NEUMÍ vadu chytit; test na touchback při rozptylu ven (ř. 279-281) neexistuje.
  `ThrowARockStunsPlayers` čeká jen KNOCKED_DOWN event (kód dělá jen stun; F13),
  `PitchInvasionCanStunPlayers` je výslovně „Just verify it doesn't crash".

**F12 Leap — zvláštní případ: testy certifikují MRTVÝ KÓD.** `resolveLeap` má
3 zelené testy (`test_big_guy_handler.cpp:218-260`, LeapSuccess/LeapFail/
LeapWithVeryLongLegs) — a funkce nemá žádného volajícího. Zelený test tu
vyrábí falešný signál „Leap funguje", zatímco se nikdy nezahraje. Unit test
resolver testuje, ale NIKDO netestuje, že akce je v NABÍDCE — přesně třída
„vstávání" (P45: resolveStandUp taky měl testy, a nikdo nevstal).

**F2a, F4, F5, F6:** F2a (Guard u faulu) — `Helpers.AssistsGuardInEnemyTZ`
testoval countAssists v blokovém kontextu; test „faul nesmí použít Guard"
neexistoval (dnes doplněn, `test_foul_handler.cpp:197`). F4/F5 (limit 1/kolo)
— žádný test neexistoval, dnes opraveno v kódu (`helpers.cpp:252-263`). F6
(Pass reroll na nepřesnou) — testy rerollový řetěz passu na nepřesné přihrávce
vůbec nespouštějí.

**Skóre: 13 nálezů parity → 0 chytil test.** 3× test vadu přímo certifikoval
(T-I), 6× chyběl negativní případ (T-II), 3× vakuózní aserce (T-III), 1× test
zeleně kryje mrtvý kód (F12).

## ČÁST 2 — ÚLOHA A: TESTY FIXUJÍCÍ CHOVÁNÍ BEZ OPORY V PRAVIDLECH

(doplňuje se)

## ČÁST 3 — ÚLOHA B: DOVEDNOSTI A AKCE BEZ TESTU

(doplňuje se)

## ČÁST 4 — ÚLOHA C: N/A KONTROL (rodina T2.18)

(doplňuje se)

## VERDIKT K HYPOTÉZE

(doplňuje se)
