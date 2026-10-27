# Partie „zeď“ 27. 10. 2026 — orkové (uživatel) × lidé (engine), poznatky pro řadič klece

Pozice: pomalejší orčí klec (nosič thrower MA5, zadní rohy black orkové MA4, přední rohy linemani s Wrestle
bez Block) před zdí lidí na x=14 (MA6–7, dva blitzeři s Guard). 1. poločas, 4. kolo orků. Server
`bb_play_session_20261012.py` s `BB_PARTIE=zed-orkove`, log `prikazy.log` (záloha před vrácením tahu
`prikazy_pred_vracenim_klec_*.log`). Lidi hrál engine (macro MCTS 200 iterací, `tahai`, přeplánováno při
každé odchylce kostek). Ukončeno v 8. kole za 0:0 — uživatel: „důležité jsou poznatky“.

## Průběh (zkráceně)
- **4. kolo:** blitz black orka na krajního hráče zdi z pole budoucího rohu (13,4), Pushed bez následování
  („bude roh“); markér k dolnímu konci zdi; klec přestavěna o 4 řady výš ve stejném sloupci, rohy /0.
  Uživatel opravil moje dosazení dvou hráčů na pole vedle nosiče: **„zničil jsi klec — klec má být jen
  čtyři rohy“** (vráceno a přehráno).
- **Odpověď lidí:** blitz na roh zvenku (Both Down, oba Block), bloky na přední rohy; k nosiči se nedostali.
- **5. kolo:** blok OBH8 s asistencí (Guard) na hráče u čáry, klec o 2 sloupce (12 → 14); rohy z hráčů
  klece, „priorita je pohyb klece — neber kleci jiné hráče krom vázaného“.
- **Odpověď lidí:** blitz zvenku srazil roh (omráčen), dva hráči přimarkovali nosiče.
- **6. kolo:** uvolnění nosiče (blok, blitz z pole pod nosičem); nosič dál nepostoupil.
- **7. kolo:** HBG21 se Stand Firm stál vedle nosiče, dva bloky na něj nevyšly (Pushed / Both Down);
  přihrávka ze zóny (5+) nevyšla ⇒ turnover, míč sebrali lidé.

## Poznatky (uživatel, doslova kde jsou uvozovky)
1. **Klec = jen čtyři rohy.** Pole přímo vedle nosiče (ortogonální) zůstávají volná — „zničil jsi klec —
   to nedělej“. Volní hráči patří do druhé řady (např. (12,5), (10,3)), ne k nosiči.
2. **Pomalejší klec u zdi, rána nejde:** *„šel bych do cílové situace kdy je celá klec OK na místě
   s nižším Y — první všechny zadní hráče až na jednoho poslat blíž k míči dopředu — další volné dopředu —
   další na sousedy se zdí, ať se zeď pak nemůže hnout k našemu nosiči“*; *„zadní rohy na volnější
   křídlo, kudy bude moct běžet nosič k TD zóně soupeře“*. ⇒ posun klece po řadách na volnější stranu,
   stejný sloupec; zbytek váže zeď.
3. **Na roh přednostně hráč bez GFI:** *„jestli by na roh došel někdo bez GFI … asi by měl přednost“*
   (v partii šlo 7 ↔ 11 vyměnit a GFI by nebylo).
4. **1 kostka smí i s Wrestle:** *„stejně jako bereš 1 kostku s block, tak ber stejně s wrestle — není
   turnover na both down“* (ne nosič míče).
5. **Priorita je pohyb klece:** *„neber kleci jiné hráče krom vázaného“*; rána, která klec pustí dál,
   jde PŘED pohybem klece (*„jinak napřed block — ať se pohne klec více“*).
6. **Odtlačovat od klece:** uživatel volil (14,0), po rozboru *„máš pravdu, 15 je lepší“* — soupeř dál od
   nosiče, útočník bez následování zůstane rohem.

## Co ukázala hra (můj rozbor, ne slova uživatele)
- Posun klece po řadách fungoval: čistá klec, lidé se 4. kolo k nosiči nedostali.
- **Tempo:** za 4 kola klec ušla 2 sloupce (12 → 14); pomalá klec bez Block na předních rozích proti
  rychlejší zdi TD v poločase nestihla.
- **Roh dosažitelný blitzem zvenku** stál dvakrát (Both Down; podruhé omráčený roh). Engine lidí volil
  plán „srazit roh + přimarkovat nosiče“, ne ránu na nosiče — P228 počítá jen ránu na nosiče.
- **Stand Firm u hráče vedle nosiče** zrušil 3 rány (Pushed nic nedělá) — výběr cíle ran má počítat jen
  Defender Down / Stumbles.
- Přihrávka jako poslední možnost TD (5+ ze zóny) nevyšla; uživatel se ptal, proč ji minulé kolo nechtěl —
  minule šlo o 1 pole a příjemce už hrál, teď o 4–5 polí a poslední šanci.

## Pro kód (kniha P229, P230)
- `hitAllowed` + Wrestle (ne nosič) — test, mutace.
- P230: posun klece po řadách (bod 2) jako náhrada „nosič sám bokem“ (dnes `OneCageKeepsCorners.FrenzyCornerDoesNotBlock` padá právě tím).
- Výběr rohu: bez hodu před GFI; volní nikdy na pole vedle nosiče.
- Hrozba přes roh zvenku (blitz na roh + přimarkování) — ověřit, co P228 počítá.
