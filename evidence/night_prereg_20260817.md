# PŘEDREGISTRACE — NOC 17.→18.08.2026
**Zapsáno PŘED spuštěním. Prahy platí tak, jak stojí tady.**

---

## ⛔ NEJDŘÍV: PROČ NE P17 WRESTLE *(byl to kandidát č. 1 — strop ho vyřadil)*

Pravidlo ze 14.08. *„před každým ramenem počítat strop napřed"* ho zastavilo
dřív, než se na něj sáhlo:

* **Wrestle nemá žádný trpaslík.** V rosterech, které hrajeme, ho má jen
  skavení **„Lineman +Wrestle" ×2** — a ti **nemají Block**. Kombinace
  *Block+Wrestle*, na kterou je P17 napsané, **na hřišti vůbec nenastává**.
* Expozice v korpusu (750 skavenských her, 33 953 bloků): wrestler **útočí
  3,20×/hru** (7,07 % bloků), **je blokován 3,87×/hru** (8,55 %). Netriviální,
  ale **jen v jednom ze čtyř matchupů** ⇒ ~25 % her.
* ⭐ **A míří to proti nám.** P17 opravuje kanál, který používá **soupeř**.
  Po opravě náš chess proti skavenovi **klesne** — část našich 451 TD je
  artefakt toho, že skaven svou dovednost neumí použít.

⇒ **P17 je oprava PARITY, ne zlepšení, a chess A/B pro ni není správný nástroj**
(měřilo by se „o kolik hůř vypadáme, když soupeř hraje správně", a pre-registrovaný
práh je psaný na zlepšení). **Zůstává otevřené jako parita; nepatří na noc.**

---

## BĚH: PŘEMĚŘIT BRÁNU KLECE S CRN

### Proč právě tohle
1. **Máme v účetnictví verdikt, který podle Fableho neobstojí.** Brána klece
   byla 13.08. **ZAMÍTNUTA** na dw-we −0,0297 (−2,0σ). Fable 17.08.: to leží
   **uvnitř toho, co produkují prokazatelně mrtvá ramena** (+2,3σ, −2,07σ)
   ⇒ správný zápis je **NEROZHODNUTO**, ne zamítnutí.
2. **Je to nejlepší první použití CRN na reálném rameni** — a zároveň jeho
   ověření proti známé odpovědi. Když CRN vrátí těsný interval kolem nuly,
   máme naráz **verdikt i důkaz o nástroji**.
3. **Nula implementace** ⇒ nula rizika, že noc padne na chybě napsané odpoledne.

### ⚠️ Poctivá výhrada k síle testu
CRN pomáhá tím víc, čím **vzácnější** rameno je. Brána klece je **všudypřítomná**
(sahá na každé kolo s klecí), takže podle Fableho odhadu je redukce SE jen
**15–25 %**, ne desetinásobná. Proto se jede **6 000 párů na jeden matchup**,
ne 3 000 na tři.

Odhad: SD páru ~0,52 × 0,8 (CRN) / √6000 ⇒ **SE ≈ 0,55 pp**.

### Konfigurace
* `MODE=0` *(cand = cageAdvance ON, base = produkce)*, **PAIRS=750, SHARDS=8**
  ⇒ **6 000 párů** na matchup `dw-we` — tam padlo původní zamítnutí.

### ⛔ NULOVÁ KONTROLA — a proč ta první verze tohohle dokumentu neplatila

Napsal jsem sem odpoledne, že nulu udělá **mode 2** (obě ramena táž
konfigurace). **Prověřeno pořádně a je to TAUTOLOGIE.** Pod CRN hrají obě
orientace doslova tutéž hru, takže
`chessCandAway = 1 − chessCandHome` **algebraicky**, a delta **musí** vyjít 0
bez ohledu na to, jestli je na rameni cokoli špatně. Mode 2 tedy umí chytit
jedinou věc: hrubou chybu v seedování. **Jako kontrola ramene neplatí a nesmí
se tak jmenovat.** *(Dnešní běh na 50 párech ji „prošel" — a neřekl tím nic.)*

⭐ **Skutečná nulová kontrola je JINDE: uvnitř samotného běhu, po párech.**

Pod CRN se dvě orientace liší **jen tím, které straně je přiřazeno rameno**.
⇒ **Pár, ve kterém rameno NIC NEUDĚLALO, musí mít deltu přesně 0.**
Harness proto od 17.08. tiskne:

```
arm acted in N/M pairs; pairs that moved: X; MOVED WITHOUT THE ARM ACTING: Z
```

* **`Z` MUSÍ BÝT 0.** Když není, **flag změnil běh jinudy než přes měřenou
  vlastnost** — a delta se **nesmí číst**. Tohle je test, který by 15.08.
  chytil `dw-sk` (+2,28 pp při mrtvém rameni) okamžitě a **v tom samém běhu**,
  bez druhého matchupu a bez 6 000 her navíc.
* `arm acted in N/M` je **poctivý jmenovatel**: běh, kde je N malé, neměřil
  skoro nic, jakkoli hezké má SE.

⇒ **Mode 2 se pouští dál (`CONTROL_MODE2=1`), ale jen jako smoke test
seedování** — je za pár minut a chytí překlep v CRN. **Verdikt o rameni na něm
nestojí.**

⚠️ **A pořád platí, že matchup s nulovou expozicí je silnější,** kde existuje:
u Dauntlessu ověřoval navíc, že *jiná konfigurace při stejném chování* dá nulu.
Brána klece takový matchup nemá — proto ten per-pair test.

### ✅ NÁSTROJ OVĚŘEN V OBOU SMĚRECH *(17.08. odpoledne, před spuštěním)*

Nula na mrtvém rameni sama nedokazuje nic — mohla by znamenat, že CRN udělalo
nulu ze všeho. Proto obě strany:

| | rameno jednalo | párů se hnulo | leak | delta |
|---|---|---|---|---|
| `dw-sk`, rameno **mrtvé** | **0/40** | **0** | 0 | **+0,0000 ± 0,0000** |
| `dw-orc`, rameno **živé** | **40/40** | 20 (50 %) | 0 | +11,25 ± 5,72 pp |

⇒ `dw-sk` dával 15.08. **+2,28 pp (+2,3 SE)** při témž mrtvém rameni. **Teď je to
exaktní nula** — a živé rameno se přesto hýbe. To je ta oprava.

### ⛔ TOHLE NENÍ REPLIKACE BĚHU Z 13.08.
Původní zamítnutí běželo na **jiném enginu**: bez P13, bez opravy hand-offu
i bez odmítnutí darovaného TD. Dnešní běh proto odpovídá na
**„pomáhá brána v DNEŠNÍM enginu?"**, ne na „byl tehdejší výsledek správně?".
⇒ **Kdyby vyšlo jinak než tehdy, není to důkaz, že CRN opravilo starou chybu** —
mohl to být kterýkoli z těch tří commitů. Obojí se musí zapsat jako možnost.

### Metrika a prahy *(pre-registrováno)*
* **Primární:** párová delta chess na `dw-we`, 6 000 párů, **jen `n_nonzero`
  páry nesou informaci** — mean se ale bere přes VŠECHNY páry.
* **BRÁNA ŠKODÍ:** delta ≤ **−0,015** *(≈ 2,7 SE)*
* **BRÁNA POMÁHÁ:** delta ≥ **+0,015**
* **NEROZHODNUTO:** mezi tím — **a zapíše se to jako NEROZHODNUTO**, ne jako
  potvrzení zamítnutí. *(Přesně tenhle krok se 13.08. neudělal.)*
* ⚠️ **Práh je 1,5 pp, ne 2 pp**, protože CRN sníží SE; kdyby `n_nonzero` vyšlo
  nečekaně nízko, **práh se NEPOSOUVÁ** — výsledek se zapíše jako
  „nedostatečná síla" s uvedením `n_nonzero`.

### Předregistrované předpovědi *(ať se to nedá číst zpětně)*
| | čekám |
|---|---|
| delta chess dw-we | **kolem nuly**, uvnitř ±0,015 ⇒ NEROZHODNUTO |
| `n_nonzero` | **vysoký** (>80 %) — brána sahá na skoro každé kolo s klecí |
| `MOVED WITHOUT THE ARM ACTING` | **0** — jinak se delta nečte |
| mode 2 smoke test | exaktně 0 *(tautologie, nic tím netvrdíme)* |
| K9a tempo | dolů *(brána platí tempem — to je známé)* |
| bloky/kolo | nahoru |

### Falzifikátor nástroje
Kdyby CRN běh vrátil **stejně široký interval** jako běh bez CRN, je Fableho
odhad redukce špatný a **CRN u všudypřítomných ramen nepomáhá** — to je taky
výsledek a patří zapsat.

### Příkaz
```
MODE=0 PAIRS=750 SHARDS=8 MATCHUPS="1:dw-we:1" \
  CONTROL_MODE2=1 CONTROL_PAIRS=50 OUT=gate_crn_20260817 \
  setsid nohup ./run_night_ab.sh > gate_crn_launch.log 2>&1 & disown
```
`run_night_ab.sh` odmítne A/B bez nulové kontroly (P20). Brána klece nemá
matchup s nulovou expozicí, proto `CONTROL_MODE2=1` — **ale verdikt stojí na
per-pair testu `MOVED WITHOUT THE ARM ACTING`, ne na té noze.**

### Pořadí čtení výsledku *(aby se nečetlo od konce)*
1. **`MOVED WITHOUT THE ARM ACTING` = 0?** Ne ⇒ konec, výsledek se nečte.
2. **`arm acted in N/M`** — jak velký je skutečný vzorek?
3. Teprve pak **delta a práh**.
