# P39 NA CELÉM KORPUSU 20.08.2026 — nosič se neaktivuje

`corpus_baseline_20260819_data`, **3 000 her**, skript `diag_carrier_idle_20260820.py`.
*(19.08. byl tenhle rozpad jen ze vzorku 800 her.)*

```
kol s naším stojícím nosičem: 17728
  Δx = 0                                   6216   35.1 % z 17728
  ├─ nosič NEJEDNAL VŮBEC                  5213   83.9 % z 6216
  │   ├─ byl přitom VOLNÝ (0 TZ)           3302   63.3 % z 5213
  │   └─ stál v soupeřově TZ               1911   36.7 % z 5213
  └─ nosič JEDNAL, ale Δx=0                1003   16.1 % z 6216
```

⇒ **REPLIKUJE.** 83,9 % proti 83,8 % ze vzorku. P39 stojí.

## ⛔ Opravené jmenovatele — číslo se zhoršilo

Doklad z 19.08. (`cage_ma_cap_20260819.md`) měl v téhle tabulce **rozbité
jmenovatele**: uváděl „*z toho* byl volný 58,3 %", ale 957 + 684 = **1 641**,
tedy podíly byly z VŠECH kol s Δx = 0, ne z kol, kde nosič nejednal.
Se správným jmenovatelem je nosič volný v **63,3 %**, ne v 58,3 %.

⚠️ **Tatáž vada se mi povedla i v první verzi tohohle skriptu** — klíč
„byl volný" se zvyšoval v obou větvích a součet dal 5 497 proti jmenovateli
5 213. Opraveno před zápisem; komentář je ve skriptu.

## Co z toho plyne

Rozvrh je mechanicky nesplnitelný jen ve **27,6 %** kol (4 892 / 17 728,
také replikuje), a ze splnitelných se nosič nehne ve **37,7 %** (4 837 / 12 836).
⇒ **Drivy neprohráváme rozvrhem, ale nevyužitím**, a nosič v naprosté většině
těch kol **nemá vůbec žádnou událost** — a ve dvou třetinách je přitom volný.

⭐ **Napojení na P38:** základní `expandAdvance` stahuje cíl zpět, dokud
není TZ-free, a když nedojde nikam, `if (steps <= 0) return result;` ⇒
**nosič se nehne**. Zapnuté rameno P38 tuhle smyčku obchází, protože si pole
najde do strany. ⇒ **P38 mohlo z velké části opravit P39, aniž se tak jmenuje.**
Rozhodnout to má zadání pro Fable `fable_brief_p38_decomposition_20260820.md`.
