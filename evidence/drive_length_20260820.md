# JAK DLOUHO TRVÁ NÁŠ TD DRIVE (20.08.2026, 3 000 her)

Vzniklo z uživatelovy otázky: *„tohle s klecí je zajímavé a dlouhé — a proti
tomu stojí trpaslíci, kteří dali TD za 8 kol."* A z jeho upřesnění, že
**potvrzení „trpaslík dá TD za 8 kol" máme i bez odehrání zápasu** — je to
standard rasy, ne hypotéza k ověření.

Skript `diag_drive_len_20260820.py`. Drive = souvislý úsek NAŠICH kol s míčem.

## Výsledek

| | |
|---|---:|
| drivů s míčem celkem | **5 751** |
| z toho **skórujících** | **1 047 = 18,2 %** |
| ⌀ délka drivu s míčem | 3,48 kola |
| **⌀ kol na TD** | **4,08** |
| medián | **4** |
| min / max | 1 / **7** |

**Rozložení kol na TD:**

| kol | drivů | podíl |
|---:|---:|---:|
| 1 | 203 | **19,4 %** |
| 2 | 149 | 14,2 % |
| 3 | 120 | 11,5 % |
| 4 | 103 | 9,8 % |
| 5 | 85 | 8,1 % |
| 6 | 137 | 13,1 % |
| 7 | 250 | **23,9 %** |
| **8** | **0** | **0,0 %** |

## ⭐⭐⭐ Co to říká — a je to obráceně, než otázka čekala

**Naše skórující drivy NEJSOU pomalé. Jsou rychlé.** Medián **4 kola**
z osmi, průměr 4,08. Když klec dojde, dojde **s rezervou půlky půle**.

⇒ ⛔ **Klec nespotřebovává těch osm kol.** Hypotéza *„procedura je moc dlouhá
na rozpočet, který máme"* se **nepotvrzuje** — a sedí to k T0.1, kde
**KLEC 33,8 % a SÓLO 34,6 %** vyšly prakticky stejně: **složitost klece nám
tempo ani nebere, ani nedává.**

⇒ ⭐ **Vada není v DÉLCE drivu, ale v jejich POČTU: 81,8 % držení míče
neskončí ničím.** Průměrné držení trvá **3,48 kola** — tedy drive nekončí
tím, že by došla půle, ale **dřív**.

⛔ **A rozložení je DVOUVRCHOLOVÉ, což je vlastní nález:**
* **19,4 % TD padne za JEDNO kolo** — to není grind, to je krátké pole
  (výkop, turnover u jejich endzony);
* **23,9 % za SEDM kol** — to je grind, dojetý na doraz;
* mezi tím je díra.
⇒ **Hrajeme dvě různé hry a míchá se nám to do jednoho průměru.** Doktrína
psaná pro grind se měří i na drivech, které byly rozhodnuté rozestavěním.

⚠️ **A max = 7, nikdy 8** — drive, který by potřeboval osmé kolo, **nikdy
neuspěje**. Je to strop rozvrhu, ne náhoda.

## ⇒ Otázka, která z toho vypadla (Q10)

Standard rasy je **1 TD za půli**. My máme **0,337 TD na CELÝ zápas**, tedy
zhruba **šestinu** standardu — a přitom **když skórujeme, jsme rychlí**.

⇒ **Neptáme se „je klec moc dlouhá", ale „proč 81,8 % držení míče umře".**
Další krok je rozpad konců drivu: **turnover · konec půle · odebraný míč** —
teprve to řekne, jestli je vada v držení, v tempu, nebo v rozvrhu.

Viz [[project_bloodbowl_where_lost_20260820]] · [[project_bloodbowl_k9c_phased_20260820]] ·
[[project_bloodbowl_pace_root_cause_20260811]]
