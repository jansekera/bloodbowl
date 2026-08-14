# Pre-registrace očekávání — špinavé rohy / řetěz zámků (14.08.2026)

Zapsáno PŘED spuštěním výpočtu nad korpusem `diag_replay_mine_20260813_big_data`
(3000 her). Cokoli níže se po výpočtu nemění; report cituje tenhle soubor.

## A) P0.1 — blok → čistota rohů

* **A1 (adresný test, jádro doktríny):** kola, kde v kole N+1 srazíme blokem
  PRÁVĚ toho soupeře, který na začátku N+1 špiní roh („polluter"), skončí
  s čistějšími rohy na konci N+1 než kola, kde blokujeme někoho jiného nebo
  nikoho. Očekávám rozdíl ≥ +15 pp v P(roh vyčištěn) a ≥ 3σ při tomto n.
* **A2 (korelační, replikace −2,2σ/+2,7σ z drivů):** bloky v kole N korelují
  kladně s čistými rohy na konci N+1 i po kontrole na hustotu soupeřů
  (OPP3, REACH0); efekt bude slabší než surový, ale nezmizí (zůstane ≥ 2σ).
* Očekávám, že „blokovat kohokoli" bude slabší než „blokovat polluter a";
  pokud vyjdou stejně, doktrína „bít ty, kdo špiní roh" se redukuje na
  „prostě bít" a prioritu blitzu na rohy klece to NEpodpoří.

## B) P0.5 — řetěz špinavý roh → zámek → chybějící roh/pomalost v N+1

* **B0 (přiznaná tautologie):** D_N → L_N je z definice částečně mechanické
  (stojící tělo na špinavém rohu JE v soupeřově TZ, tedy zamčené).
  Vypovídající je až perzistence: zámek na ZAČÁTKU N+1 (po soupeřově kole).
* **B1:** špinavé rohy na konci N předpovídají, že totéž tělo je na začátku
  N+1 stále zamčené nebo na zemi; očekávám P(zamčené ∨ na zemi) ≥ 50 %.
* **B2:** D_N → méně čistých rohů na konci N+1 a nižší Δx nosiče v N+1,
  a to i po kontrole na OPP3/REACH0/kolo. Očekávám ≈ −0,3 až −0,5 pole
  Δx na jeden špinavý roh, ≥ 3σ.
* **B3 (splatnost po soupeřích):** relativní nárůst P(ztráta míče v soupeřově
  kole | špinavý roh vs bez) bude NEJVĚTŠÍ proti wood-elfovi, nejmenší proti
  orkovi/humanovi; odložený účet (Δx v N+1, zámky) bude naopak výraznější
  proti skavenovi/orkovi. Tedy: wood-elf platí hned, skaven na splátky.
* **B4:** po kontrole na hustotu efekt D_N NEZMIZÍ (tj. špinavý roh není jen
  proxy „je kolem hodně soupeřů"). Kdyby zmizel, celá doktrína rohů je
  epifenomén hustoty a priorita blitzu se má řídit hustotou, ne rohy.

## C) Rozpočet — doplněno 14.08. PŘED výpočtem části C
(A/B extrakce v tu chvíli doběhla, ale žádné číslo z A/B/C jsem ještě neviděl)

* **C1:** většina špinavých rohů (odhad 50–70 %) má na začátku kola aspoň
  jednoho volného stojícího souseda pollutera (mimo rohy a nosiče) ⇒ jde
  očistit BLOKEM zdarma, bez blitzu. Konflikt s prolomením zdi je tedy
  menší, než uživatelova námitka předpokládá.
* **C2:** blitz padá na roh klece jen v menšině kol (< 15 %); kola s blitzem
  na roh budou mít v N+1 čistší rohy, ale Δx v témž kole nižší (blitz nešel
  do zdi). Po kontrole na hustotu čekám, že rozdíl v Δx bude < 0,5 pole.
* **C3:** aspoň třetina idle těl (K31) v kolech se špinavým rohem na
  pollutera DOSÁHNE (MA+2) ⇒ „nedostatek zdrojů" je zčásti alokační chyba.

## Co bych považoval za překvapení

* Že adresné bití polluterů NEčistí rohy (pak čistota vzniká soupeřovým
  rozhodnutím odejít, ne naším bitím).
* Že proti skavenovi je okamžitá splatnost stejná jako proti wood-elfovi.
* Že po kontrole na hustotu se −2,2σ špinavých rohů otočí k nule.
