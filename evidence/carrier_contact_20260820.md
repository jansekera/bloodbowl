# ZÁKAZ: NOSIČ NESMÍ KONČIT KOLO V KONTAKTU (20.08.2026, 3 000 her)

Uživatel 20.08.: *„mi přijde jako větší průšvih block na nosiče — ale ten je
méně častý — ale já mám za to, že se nesmí stávat vůbec."*
A dodatek: *„ten blitz na nosiče je řešitelný správnou klecí — a to také
řešíme — a uznáváme, že to ne vždy vyřešíme tak striktně jako block na nosiče."*

## ⭐⭐⭐ Proč je blok horší než blitz, ačkoli je vzácnější

**Blitz** stojí soupeře jeho **jedinou blitz akci za kolo**; dosáhl na nosiče
přes vzdálenost a my mu to můžeme leda **zdražit**. To je jeho zásluha.

**Blok je zadarmo.** Vyžaduje jen to, aby útočník u nosiče **UŽ STÁL** na
začátku svého kola — tedy že **my jsme své kolo ukončili s nepřítelem vedle
míče**. To není jeho zásluha, **je to náš stav, který jsme si vyrobili sami**.

⇒ **Dvě různé tvrdosti, ne dvě velikosti téhož:**

| | tvar pravidla | proč |
|---|---|---|
| **blok na nosiče** | ⛔ **ZÁKAZ — správná četnost je NULA** | je to naše rozhodnutí o cílovém poli |
| **blitz na nosiče** | ⚠️ **řeší se klecí, a ne vždy se to povede** | soupeř za něj platí svou jedinou akcí |

## ⛔ OPRAVA NEJCITOVANĚJŠÍHO ČÍSLA KAPITOLY

Spec 15.0b i paměť uvádějí jako *„nejtvrdší číslo kapitoly"*:
**„v 39,3 % našich kol končí nosič v kontaktu se soupeřem"**.

**To číslo počítá i LEŽÍCÍ soupeře.** Změřeno na témž korpusu, 24 754 našich
kol končících s naším stojícím nosičem:

| definice „v kontaktu" | | |
|---|---:|---:|
| soupeř **STOJÍCÍ** u nosiče | 3 048 | **12,3 %** |
| soupeř **jakýkoli, i ležící** | 9 622 | **38,9 %** ← *tohle je těch „39,3 %"* |
| **naše** tělo u nosiče | 19 981 | 80,7 % |
| jakýkoli soused | 20 985 | 84,8 % |

⛔ **Ležící soupeř nemá tackle zónu a NEMŮŽE blokovat.** Číslo, které o zákazu
něco říká, je tedy **12,3 %**, ne 39,3 %. ⇒ **Zákaz je 3× levnější, než jak
kapitola dosud zněla.**

*(Táž rodina jako K29⭐, které chyběla třetí klauzule kvůli lehlým soupeřům —
[[project_bloodbowl_cage_rule_20260819]].)*

## Řetěz od expozice

| | | |
|---|---:|---:|
| naše kola končící s naším stojícím nosičem | 24 754 | |
| nosič **ČISTÝ** *(zákaz dodržen)* | 21 706 | **87,7 %** |
| ⛔ **EXPOZICE** *(stojící soupeř u nosiče)* | 3 048 | **12,3 %** |
| ⌀ soupeřů u nosiče při expozici | 1,32 | |

**Z 3 048 expozic:** soupeř **udeřil blokem 1 568 = 51,4 %** · neudeřil 48,6 %.
**Z 1 568 bloků:** nosič sražen **48,3 %** · **přišli jsme o míč 47,4 %**.

⇒ **EXPOZICE → ZTRÁTA MÍČE: 24,4 % z expozic = 3,0 % našich kol.**

⚠️ **Poctivá mez:** „přišli jsme o míč" je tu **horní odhad** — počítá se jako
*„týž nosič už míč nedrží"*, takže sem spadne i případ, kdy míč sebral náš
spoluhráč a držení pokračovalo. Rozpad konců držení
([[project_bloodbowl_possession_end_20260820]]) dává na bloky **564** ztrát
proti zdejším 743; **rozdíl je právě tenhle překryv.**

## ⇒ Co z toho plyne

⭐ **Zákaz je levný a adresný:** proti blitzu potřebujeme **zdražit dosah**,
což je celá klec — drahé a nejisté *(brána klece 18.08. ŠKODILA, −3,7σ)*.
Proti bloku stačí **neukončit kolo v kontaktu**, a to je **rozhodnutí
o cílovém poli** — tedy přesně ta dimenze **KAM**, kterou dnes nacházíme
rozbitou na čtyřech místech *(P38 · P9 · P32 · P35)*.

⇒ **Zapsat jako zákaz do spec ČÁST 3 a jako kontrolu.** Strop: **3,0 %
našich kol**, tedy ~0,74 ztráty míče na zápas.
