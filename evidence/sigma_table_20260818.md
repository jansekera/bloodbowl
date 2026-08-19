# σ-TABULKA PŘEPOČÍTÁNA — P30 (18.08.2026)

Spouštěč splněn: `corpus_baseline_20260817_data`, **3 000 her**, otisk enginu `5e5ab352`.
Nástroj `diag_drive_predictors_20260813.py`. Stará tabulka stála na **120 hrách**
(195 drivů / 35 s TD) a byla od 17.08. **POZASTAVENÁ**, protože na jiném korpusu
téže velikosti nevycházela.

Nově: **5 044 drivů, z toho 827 se skórováním** — 24× víc TD drivů než tehdy.

## Výsledek

| veličina | TD drivy | bez TD | σ (3000 her) | σ (120 her, 13.08.) |
|---|---|---|---|---|
| **K9a splněno** | 0,437 | 0,211 | **20,7σ** | 4,2σ |
| **REACH0 jako POČET** | 1,385 | 2,100 | **−16,7σ** | −1,8σ |
| **Δx nosiče** | 2,733 | 2,081 | **13,9σ** | 2,4σ |
| K35 `FB2 ≤ 1` | 0,876 | 0,789 | 11,6σ | 2,6σ |
| bloků na kolo | 1,703 | 1,522 | 10,4σ | 2,7σ |
| K34 `REACH0=0` (ano/ne) | 0,457 | 0,355 | 9,3σ | 0,8σ |
| **rohů ŠPINAVÝCH (počet)** | 0,121 | 0,173 | **−6,8σ** | *neměřeno* |
| K29 čistota rohů | 0,822 | 0,769 | 5,1σ | 2,6σ |
| **K33 „byl v kole blok" (ano/ne)** | 0,760 | 0,773 | **−2,5σ** | +0,6σ |
| rohů všech (počet) | 1,254 | 1,313 | −2,1σ | −0,2σ |
| rohů ČISTÝCH (počet) | 1,133 | 1,140 | −0,2σ | *neměřeno* |

## ⭐ Test stability: korpus rozpůlen na sudé/liché hry (2× 1 500)

To je ta kontrola, kvůli které byla tabulka pozastavená — *„na jiném korpusu
téže velikosti nevychází."* Teď se dá udělat uvnitř jednoho korpusu.

| veličina | půlka A | půlka B | replikuje? |
|---|---|---|---|
| K9a | 14,6σ | 14,7σ | ✅ |
| REACH0 počet | −12,6σ | −11,1σ | ✅ |
| Δx | 9,7σ | 10,0σ | ✅ |
| K35 FB2 | 8,5σ | 8,0σ | ✅ |
| bloků/kolo | 7,9σ | 6,9σ | ✅ |
| K34 | 6,4σ | 6,8σ | ✅ |
| rohů špinavých | −4,7σ | −4,9σ | ✅ |
| K29 čistota | 3,4σ | 3,8σ | ✅ |
| **K33 blok ano/ne** | **−2,8σ** | **−0,7σ** | ⛔ |
| **rohů všech** | **−0,4σ** | **−2,6σ** | ⛔ |
| **rohů ČISTÝCH** | **+0,9σ** | **−1,2σ** | ⛔ mění znaménko |

⭐ **PRAVIDLO, které z toho vypadlo:** na korpusu 1 500 her **replikuje všechno
s |σ| ≥ 3 a nic pod tím**. Tabulka tedy **smí řadit frontu**, ale jen po řádek
K29 (5,1σ); tři nejslabší řádky jsou šum a nesmí se o ně opřít žádné rozhodnutí.
*(Přesně tímhle propadla stará tabulka: nejvíc se citovaly řádky 0,6–2,7σ.)*

## ⭐⭐ Tři věcné změny proti 13.08.

**(1) „Byl v kole blok?" předpovídá TD ZÁPORNĚ** (+0,6σ → −2,5σ), a hlavně:
je to jediná z hlavních veličin, která **v půlkách nereplikuje**. Souhlasí to
s nálezem 14.08., že *obecné* bloky zhoršují čistotu rohů (−4,5σ).
⇒ **„bít víc" je definitivně mrtvá rada**; platí jen „bít toho správného".
Zároveň ⚠️ **`bloků na kolo` jako POČET je +10,4σ a replikuje** — takže to není
„bít je špatné", ale **„blok jako ano/ne nic neměří"**. Táž vada jako u REACH0.

**(2) Binarizace ničí signál — potvrzeno podruhé a silněji** (P1).
REACH0 jako počet −16,7σ, tatáž věc jako ano/ne 9,3σ. Rohy jako počet −2,1σ
(šum), čistota jako podíl 5,1σ. ⇒ **P1 (přepsat K33/K34 na spojité) povýšit** —
už to není „levné, opravuje metr", je to **druhý nejsilnější prediktor v tabulce,
který dnešní kontrola zahazuje**.

**(3) Nezáleží na tom, kolik máme ČISTÝCH rohů, ale kolik máme ŠPINAVÝCH.**
Počet čistých rohů: **−0,2σ = nic** (a v půlkách mění znaménko).
Počet špinavých rohů: **−6,8σ** a replikuje.
⇒ Doktrína se nemá formulovat jako *„postav čtvrtý roh"*, ale jako
*„odkliď soupeře od rohu, který už stojí"* — což je přesně **P2 + P9c**.
Tabulka tím **sama povyšuje P9c**, nezávisle na argumentaci ze 14.08.

## Drobnost k opravě

Hlavička tiskne `engine korpusu: NEZNÁMÝ (korpus bez otisku — viz P22)`, ačkoli
otisk **existuje** — leží v `corpus_baseline_20260817/ENGINE_HEAD`, tedy v běhovém
adresáři, ne v datovém. Skript ho hledá jen v datovém. Táž rodina jako ranní
„stará binárka?": **kontrola hlásí „nevím" nad odpovědí, která leží o adresář vedle.**
