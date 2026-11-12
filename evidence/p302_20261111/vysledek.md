# P302 krok 1 — měřidlo „šance soupeře získat míč“ na situacích P250b (11.–12. 11. 2026)

`situace.py … 20 1` (jen náš tah + odpověď soupeře, párově, **K = 1**), stejné situace jako P250b: na začátku našeho
tahu leží míč volně a náš stojící hráč na něj dosáhne. **135 situací** (skaveni 55, lesní elfové 46, lidé 34) × orkové /
lidé / trpaslíci. Běh 22:01–01:50. Ramena: vše · bez_b (bez obestoupení) · bez_c (bez odtlačení na míč).
Nový ukazatel **`souper_mic`** = s jakou šancí má soupeř míč po našem tahu: drží ho ⇒ 1, leží volně ⇒
`opponent_pickup_chance` (nejlepší soupeřův zvedač: doběh bez pádu × zvednutí), máme ho my ⇒ 0. Menší = lépe.
Rozdíl = vše − rameno (± chyba průměru, σ).

| ukazatel | vše | vše − bez_b | vše − bez_c |
|---|---|---|---|
| **soupeř má míč (šance)** | 0,163 | **+0,022 ± 0,011 (+2,0σ)** — horší | 0,000 |
| míč volný po našem tahu | 0,296 | +0,022 (+1,7σ) | 0,000 |
| míč u nás po našem tahu | 0,681 | **−0,030 ± 0,015 (−2,0σ)** | 0,000 |
| míč u nás po tahu soupeře | 0,541 | −0,030 (−2,0σ) | 0,000 |
| míč u soupeře po našem tahu | 0,022 | +0,007 (+1,0σ) | 0,000 |
| turnover | 0,444 | +0,022 (+1,7σ) | 0,000 |
| TD soupeře | 0,000 | 0 | 0 |

Podle týmu (`souper_mic` vše / bez_b): skaveni 0,183 / 0,174 · elfové 0,144 / 0,123 · lidé 0,155 / 0,113.
Pozitivní kontrola (čítače a/b/c): vše 7486/842/46 · bez_b 7336/**0**/32 · bez_c 7530/857/**0**.
Volný míč po našem tahu (vše): 40× (z 135), průměrná šance soupeře ho zvednout **0,474**.

**Čtení.**
- **Obestoupení (P250b) v dnešní podobě škodí**: AI ho občas zvolí místo zvednutí, míč pak leží (+2,2 b.), máme ho méně
  (−3,0 b., −2σ) a soupeř ho zvedne skoro napůl (0,47). Obestupuje se na **nejbližší** pole u míče, ne na to, které
  soupeři zvednutí ztíží ⇒ přesně cíl kroku 2.
- **Odtlačení na míč (P250c)** v zahraných tazích nic nezměnilo (stejně jako 6.11.: 0 změn v 315 situacích).
- Měřidlo funguje (rozliší ramena, šance volného míče ~0,47 dává smysl pro AG3 soupeře s 0–1 zónou).

**Další krok (2):** obestoupit na pole, které **soupeři zvednutí nejvíc ztíží** (minimum `opponentPickupChance` po přesunu),
a nabízet ho jen tehdy, když to šanci soupeře opravdu sníží. Změřit tímtéž (vše vs. bez_b).
