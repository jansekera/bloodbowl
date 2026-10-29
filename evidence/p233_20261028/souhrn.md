# P233 cílené měření — souhrn (29.10.2026)

Co P233 dělá (uživatel 28.10.): pomalejší klec před zdí, kterou nesmí bít, zeď čistí od kraje — blitz na krajního
hráče s asistenty na 2 kostky naše, pak rány dovnitř; klec se to kolo nepohne. Zapíná se jen tehdy, když P229
(prolomení zdi) nemá žádnou dovolenou ránu (žádný blok rohem ani blitz na 2+ kostky).

Měřeno párově (tentýž stav a semínko, jen náš tah + odpověď soupeře), zmrazená knihovna `zmrazeno_p233`
(commit 8fa3c1db). Týmy bez Block (nemrtví, Nurgle, Chaos) × člověk, skaven, lesní elf, 10 pozic na dvojici =
90 pozic, pozice z `../p230_20261027/cilene.py`. Ramena: vše / bez P233 / bez P233 ani P230.

## Výsledek: P233 se v těchto pozicích nezapnulo ani jednou

- Ramena se liší v **0 z 90 pozic** (`tabulka.txt`), stejně i bez P230.
- Pozitivní kontrola dosahu (`dosah_p233.py` s `BB_CAGE_DEBUG=1`, rameno „vše“, všech 90 pozic):
  řadič došel k zdi (P229) v 59 pozicích a **pokaždé měl ránu** (blok rohem na 2 kostky, v 16 i blitz);
  k P233 nedošel ani jednou, k P230 také ne. Ve zbylých 31 pozicích k zdi nedošel vůbec.
- Proč: P229 postaví asistenty na křídla zdi a s nimi má roh 2 kostky. V testu (zeď pěti ST 3 těsně u sebe)
  se asistence křídel a rohů ruší — taková zeď v generovaných pozicích nevzniká.

Čtení: nula **neříká nic o účinku P233** — situace, pro kterou je, v těchto pozicích nenastala. Neříká ani, že
v běžných hrách nenastane (generátor staví zeď jinak než soupeř ve hře).

Další krok (návrh): spočítat, jak často P233 dojde ke slovu v běžných hrách (`BB_CAGE_DEBUG` řádky P229/P233
v hrách jako u P232); nastane-li, měřit párově na těch tazích.

## Konkrétní situace × předchozí commit (29.10., `situace_p233.py`)

Zombíci (MA 4, ST 3, bez Block) v kleci kolem (12,7), zeď beastmenů chaosu (MA 6, ST 3) na x = 14; P233 (8fa3c1db)
proti předchozímu commitu 407527c6 (`../p230_20261027/zmrazeno_p232`), 5 semínek, náš tah + odpověď.

| situace | předchozí commit | P233 |
|---|---|---|
| S1 zeď 5, pomocníci 3 (příklad uživatele) | celá klec se posune o 3 řady ke kraji (nosič (12,4)), přední roh blitzuje krajního (14,5); 1× z 5 turnover | asistent → blitz pomocníkem na kraj (14,9) → roh blokuje dovnitř; 2× z 5 všechny 3 rány (zeď 5 → 2, klec na místě); 3× jen blitz (soupeř neupadl) a klec pak uhne o 2 řady k otevřenému kraji |
| S2 zeď 4 | blok rohem + blitz (P229) | totéž — zeď 4 má 2 kostky už bez P233 |
| S3 jeden pomocník | žádná rána, klec o 2 řady | totéž (kontrola) |
| S4 za zdí leží 7 soupeřů (řetěz všude) | blitz do řetězu (1× nosič sražen a míč ztracen, 1× turnover) | totéž — P233 nebije, převezme staré chování |
| S5 jeden ležící na (15,6) | jako S1 | jako S1 (P233 bije druhý kraj už v S1) |

Odpověď soupeře: nosič sražen 0/5 u obou v S1; hrozba (carrier_blitz_threat) 0,04–0,06 předtím (klec dál od zdi) × 0,07 s P233.
