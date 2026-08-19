# CO BRÁNÍ ČTVRTÉMU ROHU — rozklad stropu 0,73 rohu/kolo (Fable, 19.08.2026)

Zadání: `evidence/fable_brief_corner_gap_20260819.md` (P34, K29⭐⭐).
Nástroje: `diag_corner_gap_20260819.py` (rozklad, párování, cena kroku, σ)
a `diag_corner_gap2_20260819.py` (redirect-párování, MA-rezerva, σ-binarizace).
Korpus: `corpus_baseline_20260817_data`, 3 000 her.

**Vstupní kontrola prošla přesně:** pravidlo 2,7 % · (1) 9,7 % · (3) 24,3 % ·
strop 0,73 rohu/kolo (18 081 rohů / 24 692 kol) — týž korpus, týž způsob čtení.
σ-kotvy sedí s tabulkou 18.08. do poslední číslice (K9a 0,437/0,211 → 20,7σ ·
špinavé −6,8σ · rohů všech −2,1σ · čistých −0,2σ).

---

## 1. NÁLEZ

**Čtvrtému rohu nebrání povinnosti, soupeř ani hřiště — brání mu volba.
Polovina těl na zakázaném poli (50,2 %) tam v tomtéž kole DOŠLA vlastním
pohybem, a 73,6 % z nich mělo čistý prázdný roh hned vedle zvoleného cíle;
dalších 20,4 % těl stálo celé kolo nevyužitých s čistým rohem na dosah bez
jediného hodu. Všechny „legitimní" překážky dohromady (drahé markování,
zamčení, roh není k mání) kryjí 2,0 % těl.**

Z celého stropu 0,73 rohu/kolo je **0,165 rohu/kolo (1,36/zápas) zadarmo
úplně** — stojící, nehravší tělo, žádná TZ, žádný hod (kategorie (5)) — a
dalších **až 0,301 rohu/kolo (2,48/zápas)** stojí jen jiné cílové pole
pohybu, který se stejně odehrál (kategorie (1a), horní mez). Společné
párování obou: **0,464 rohu/kolo = 3,82 rohu/zápas = 64 % stropu.**

---

## 2. Rozklad — čísla s jmenovateli

Jmenovatele: **n = 24 692** našich kol s míčem na konci kola (3 000 her) ·
**Q = 12 942** kvalifikujících kol (chybí roh ∧ naše tělo ortogonálně
u nosiče; 52,4 % z n) · **B = 20 889** ortogonálních těl v těch kolech
(⌀ 1,61 těla na kvalifikující kolo). Každé tělo padne do právě jedné
kategorie (kaskáda; součet ověřen assertem = 20 889 = 100,0 %).

| kategorie | těl | % z B |
|---|---:|---:|
| **(1a) hrálo: na ortogonální pole DOŠLO vlastním pohybem v tomtéž kole** | **10 482** | **50,2 %** |
| — z toho ČISTÝ prázdný roh hned vedle zvoleného cíle | 7 716 | 73,6 % z (1a) |
| (1b) hrálo jinak (blok/blitz/jednalo z místa) | 2 925 | 14,0 % |
| (0) leží/omráčeno a nehrálo *(brief ji nevyjmenovává; bez ní by kaskáda ležící tělo tiše pustila do (5))* | 2 359 | 11,3 % |
| (2) markuje DRAZE (únik soupeře ≥ 0,20 ⇒ R3 platí a roh přebije) | 294 | 1,4 % |
| (3) zamčené — markuje jen LEVNĚ (K30b: povinnost bez ceny, roh nemá přebít; ale krok stojí NÁŠ dodge, ⌀ P(selhání) = 0,44, n = 133) | 133 | 0,6 % |
| (4) roh není k mání (obsazen soupeřem / mimo hřiště, 15.5) | 9 | 0,04 % |
| (4b) roh je, ale 2-krokový přesun vede jen přes TZ / obsazená pole | 322 | 1,5 % |
| **(5č) NIC TOMU NEBRÁNILO — stojící, nehrálo, nemarkuje, mimo TZ, čistý prázdný roh dosažitelný BEZ HODU** | **4 266** | **20,4 %** |
| (5š) krok zdarma, ale jen do ŠPINAVÉHO rohu (−6,8σ ⇒ nedělat) | 99 | 0,5 % |

Poznámky k zápisu:

* **(2) vs (3):** „markuje soupeře" a „stojí v soupeřově TZ" je u stojícího
  soupeře TENTÝŽ geometrický predikát (sousedství). Čisté (3) bez (2) je
  prázdná množina KONSTRUKCÍ, ne měřením — kaskáda proto dělí podle toho,
  ČÍ dodge je drahý: (2) = soupeřův (povinnost drží), (3) = jen náš (cena
  kroku). Nula „zamčené bez markování" je nula s jmenovatelem B, ne N/A.
* **(1a) je nová informace proti zadání.** Brief četl kategorii (1) jako
  „rozpočet kola je pryč". Ex post ano — ale u 50,2 % těl ten rozpočet
  utratil pohyb, který skončil NA zakázaném poli, a u 73,6 % z nich byl
  čistý prázdný roh sousedem zvoleného cíle. To není chybějící rozpočet,
  to je **vada volby cílového pole** — táž akce, jiné pole. Souvisí s
  nálezem 14.08. „filtr oceňuje jinou akci, než jakou resolver provede":
  tady žádný filtr cílové pole vůči pravidlu klece vůbec nehodnotí.
* Povinnosti a fyzika dohromady — (2) + (3) + (4) + (4b) — kryjí **758 těl
  = 3,6 % z B**. Konflikt „roh vs povinnost R3", kvůli kterému má brief
  kategorie 2–3, se v datech prakticky nekoná.

---

## 3. Kategorie (5) zvlášť — číslo pro rozhodnutí

Per-tělo: 4 266 těl (20,4 % z B). Turn-level, po max. párování těl na čisté
prázdné rohy (dvě těla si nemohou nárokovat týž roh):

* **(5) = 4 075 rohů zadarmo = 0,165 rohu/kolo = 1,36 rohu/zápas**
  (jmenovatel: n = 24 692 kol, 3 000 zápasů).
* Kol s ≥ 1 rohem zadarmo: 3 293 = **13,3 % všech kol s míčem** (25,4 % z Q).
* Je to **22,6 % stropu 0,73** — a na rozdíl od stropu je to po odečtení
  rozpočtu kola, povinností, TZ, ležení, obsazenosti i dosažitelnosti.

Druhé, měkčí číslo — **(1a)-redirect** (hrálo, ale roh sousedil s cílem):
po párování **7 432 rohů = 0,301/kolo = 2,48/zápas** (horní mez: sousedství
rohu s cílem ⊄ ověřená dráha; 65,4 % těl dojelo s MA-rezervou 0, takže
redirect mohl stát GFI nebo stejný počet kroků — rezervu ≥ 1 pole mělo
34,6 % těl, n = 7 716).

**Společně (1a)+(5č), jedno párování: 11 458 rohů = 0,464/kolo =
3,82/zápas; aspoň jeden takový roh existuje v 8 674 kolech = 35,1 % všech
kol s míčem.** Pro implementaci: (5) je čistý zisk „přidej krok nehravšímu
tělu", (1a) je „při výběru cílového pole pohybu preferuj roh před
ortogonálou" — obě jsou volba, žádná nestojí blitz.

---

## 4. σ pro K29⭐⭐ jako prediktor skórujícího drivu

Metodika P30 doslova (`diag_drive_predictors_20260813`: plné drivy ≥ 7
našich kol, per-drive průměr, Welch). Kotvy reprodukují tabulku 18.08.
přesně, takže σ je na stejném základě. Drivů 3 869 (podmnožina 5 044
z P30, kde na konci kola existuje náš nosič), z toho 822 s TD.

| varianta | TD drivy | bez TD | σ | půlka A | půlka B |
|---|---:|---:|---:|---:|---:|
| PRAVIDLO per-drive průměr | 0,022 | 0,022 | **0,0σ** | +0,8σ | −0,8σ |
| PRAVIDLO aspoň jednou v drivu (ano/ne) | 0,123 | 0,118 | **+0,4σ** | +1,0σ | −0,5σ |

**Kam patří: do šumového suterénu tabulky, pod práh |σ| ≥ 3, vedle
rohů_všech a rohů_ČISTÝCH — s povinnou anotací, PROČ tam je.** Není to
verdikt o pravidle (pravidlo je zadané a platí): je to **vyhladovělý metr**.
Korpus pravidlo hraje v 2,2 % kol a per-drive průměr je proto skoro
konstantní nula — tabulka měří korpus, který pravidlo NEHRAJE, takže ho
neumí ocenit. Kontrola vyhladovění binarizací („aspoň jednou", base rate
12,3 % drivů) dopadá stejně (+0,4σ), obě půlky pod 1σ ⇒ replikovaná NULA
na tomto korpusu, ne nestabilní signál. Táž rodina jako P10a: prior floor
s vyprázdněnou pozicí — dokud plnění nezvedneme, jediné měřitelné zuby
pravidla v tabulce nese jeho rozložená klauzule „žádný špinavý roh"
(rohů_ŠPINAVÝCH −6,8σ, replikuje). **Po případné implementaci P34 se σ
musí přeměřit — teprve korpus, který pravidlo hraje, ho umí ocenit.**

---

## 5. Co ten krok stojí

Měřeno přepočtem E1/E2 (jediná definice, import z
`diag_exposure_scan_20260812`) na kopii snímku se spárovanými (5č) těly
přesunutými do rohů; n = 3 293 kol s přesunem:

| | kol | % |
|---|---:|---:|
| ΔREACH0 se ZLEPŠÍ (−1 až −3 soupeřů s dodge-free cestou k nosiči) | 998 | **30,3 %** |
| ΔREACH0 beze změny | 2 179 | 66,2 % |
| ΔREACH0 se zhorší (+1 až +3) | 116 | 3,5 % |
| ΔFB2 beze změny | 3 293 | 100,0 % |

* Krok nestojí žádný hod (definice (5)), žádný blitz, 1–2 pole pohybu
  nehravšího těla.
* Tělo v čistém rohu NOVĚ nemarkuje **v 0 z 4 075** (plyne z definice
  čistého rohu; počítáno jako kontrola definice, ne jako zjištění).
* Outlet/hand-off se nerozpadá: roh je pořád soused nosiče, vzdálenost
  pro předávku se krokem nemění (argument z geometrie, ne měření).
* Screen: 1 087 z 4 266 (5č) těl (25,5 %) má stojícího soupeře do 2 polí,
  takže NĚCO clonit mohou — ale čistá bilance téhož kroku na REACH0 je
  9× častěji k lepšímu než k horšímu (30,3 % vs 3,5 %). Kvalifikovaný
  závěr: **měřitelná cena kroku je záporná** (krok expozici nosiče spíš
  snižuje — roh kryje diagonálu dvěma TZ, ortogonála nekryje nic, přesně
  jak říká 15.0b).
* Jediná skutečná cena v datech: (5š) — 99 těl (0,5 % z B) má zdarma jen
  ŠPINAVÝ roh; tam se krok dělat nemá (špinavý roh −6,8σ, spálené tělo).

---

## 6. ⭐ CO JSEM NEZMĚŘIL A PROČ

1. **Kauzální efekt.** Žádný A/B, žádný engine (zákaz běhu do ~12:20 —
   a správně: tohle čtení má rozhodnout, JESTLI se A/B vůbec spustí).
   0,165/kolo je strop VOLBY po odečtení překážek, ne slíbený zisk;
   o zisku rozhodne až noční A/B P34.
2. **Skutečná dráha (1a)-redirectu.** Počítám jen „čistý roh sousedí se
   zvoleným cílem" + MA-rezervu (65,4 % těl rezerva 0 ⇒ redirect mohl
   stát GFI nebo nic — podle toho, jestli roh ležel na dráze). Přehrání
   dráhy krok po kroku s dodge/GFI jsem nedělal — proto je 0,301/kolo
   horní mez a uvádím ji odděleně od čistého 0,165.
3. **Vícekolový záměr.** Jestli tělo stojí ortogonálně SCHVÁLNĚ (příprava
   hand-offu na příští kolo), korpus říct neumí — není kontrafaktuál.
   Jediný nepřímý doklad: plán enginu je `NOT_CONSULTED` ve 100 % kol
   (ČÁST 11), takže dnes není kam záměr uložit.
4. **Sekvenční interakce přesunů.** Párování je statické: druhé tělo jde
   po deskách PŘED přesunem prvního; dva 2-krokové přesuny si teoreticky
   mohou překážet. Podceňuje i nadceňuje jednotky případů, ne stovky.
5. **(0) leží — 11,3 % těl.** Nehravší ležící tělo mohlo vstát (3 MA)
   a krok do rohu dodat; nechal jsem je konzervativně MIMO (5). Kdyby se
   započítala, (5) by rostlo — tohle je rezerva, ne díra.
6. **R3 do hloubky.** (2) beru jako legitimní povinnost bez ověření,
   jestli téhož soupeře nedrží už jiné naše tělo nebo roh (R1 > R3).
   Zmenšit může jedině (2) → (5); (2) je ale 1,4 %, takže o výsledek nejde.
7. **Chain-push kontaminace (1).** „Hrálo" čtu jako „má událost"; vzácný
   chain-push našeho těla v našem kole by ho označil za hravší neprávem.
   Neizoloval jsem to; posouvá jednotlivá těla mezi (1) a ostatními.
8. **Replikace na `corpus_baseline_20260819_data`** — sběr v době psaní
   běžel (~634/3 000 her). Geometrie rohů je opravou exportu HAND_OFF
   nedotčená, takže korpus 17.08. stačí; přeměření na novém korpusu je
   legitimní druhé kolo, ne podmínka.
9. **Zděděné vady metodiky P30** (výběr plných drivů ≥ 7 kol koreluje
   s výsledkem; kolo s TD čte snímek po resetu) — replikoval jsem je
   VĚDOMĚ, aby σ bylo srovnatelné s tabulkou 18.08.; kotvy to potvrzují.

Kontroly, které držely: replika vstupních čísel přesně · σ-kotvy přesně ·
assert úplnosti kaskády (jednou skutečně vystřelil a chytil chybu tisku) ·
marked_after = 0 jako test definice čistého rohu · půlky korpusu u σ.
