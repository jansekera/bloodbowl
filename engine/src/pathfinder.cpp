#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include "bb/macro_actions.h"   // gfiSequenceFailProb -- JEDNA definice ceny GFI
#include <cstring>
#include <cmath>
#include <algorithm>

namespace bb {

// BFS node for pathfinding
struct PathNode {
    int8_t x, y;
    int8_t cost;      // movement spent so far
    bool dodged;      // needed a dodge at some point
};

static constexpr int GRID_W = Position::PITCH_WIDTH;
static constexpr int GRID_H = Position::PITCH_HEIGHT;
static constexpr int GRID_SIZE = GRID_W * GRID_H;

// M6/B3(a)-follow-up (10.09.2026): Dijkstra ma DVE VRSTVY na pole --
// vrstva 0 = "reroll z dovednosti Dodge je jeste netknuty", vrstva 1 =
// "reroll uz je utraceny". Uzel = (pole, vrstva). Duvod a limity viz velky
// komentar u `riskWeightedDijkstra`.
static constexpr int kNodeCount = 2 * GRID_SIZE;

static inline int gridIdx(int x, int y) { return y * GRID_W + x; }

// ============================================================================
// PRVNI KROK PO NEJKRATSI CESTE  (M14, 01.09.2026)
//
// ⛔ PROC VZNIKL: `pickApproachStep` (helpers.cpp) vybira krok POUZE ze
//   sousednich poli, skore `vzdalenost*100 + tacklezony`. Nema znalost cesty
//   a nema PAMET. Kdyz je pole smerem k cili obsazene, uhne stranou; z toho
//   pole je pak nejlepsi zase to puvodni ⇒ CHUZE OSCILUJE A SPALI POHYB.
//   V makro vrstve je aspon pojistka proti smycce (`bestMove.target ==
//   lastPos`), v blitzove smycce (`action_resolver.cpp`) NENI ZADNA -- jen
//   kontrola "vubec se nehnul". Blitz pak padne na "nedosel", ackoli cesta
//   existovala; jen vedla stranou.
//
// ⭐ Pathfinder tu cestu UZ UMI SPOCITAT -- `canReachAdjacentTo` dela tyz BFS
//   a svuj vysledek ZAHAZUJE ("deliberately ignored", komentar tamtez).
//   Tady se misto zahozeni vrati PRVNI KROK po nejkratsi ceste.
//
// Tacklezony se NEZTRACEJI: rozhoduji mezi CESTAMI STEJNE DELKY (a mezi
// stejne dobrymi prvnimi kroky). Tim zustava zachovane to, o co puvodnimu
// pickeru slo -- blitzujici nechce doskocit tam, kde obrance dostane
// asistenci -- ale zmizi bloudeni.
// ============================================================================
// Signal ramene: kolikrat se chuze po ceste rozhodla JINAK nez hladovy vyber.
// ⛔ Tika jen pri ZMENENE VOLBE, ne pri kazdem volani -- poucení z P38/M13.
thread_local long g_blitzPathPicks = 0;
long takeBlitzPathPicksInSearch() { long v=g_blitzPathPicks; g_blitzPathPicks=0; return v; }

// ============================================================================
// ⭐⭐ SIGNÁL TIEBREAKU „ODSUŇ HO OD NOSIČE" (B-ROUND/1, 10.09.2026)
//
// ⛔ PROČ NESTAČÍ `g_blitzPathPicks`: ten tiká na „rozhodl jsem se jinak než
//   hladový výběr", což u těchhle picků platilo UŽ PŘEDTÍM ⇒ o tiebreaku
//   neříká nic. Čítač, který tiká i tam, kde se volba nezměnila, hlásí
//   „rameno jednalo" o rameni, které se jen dívalo (poučení z B2 a P35,
//   `macro_actions.cpp:664-670`).
// ⇒ FLIPS tiká VÝHRADNĚ tehdy, když se vítěz LIŠÍ od toho, koho by vybralo
//   pravidlo bez tiebreaku (tj. první striktní minimum ceny).
//
// ⭐ A `ELIGIBLE` je JMENOVATEL, bez kterého se nula nedá přečíst
//   ([[feedback_zero_needs_a_positive_control]]): kdyby FLIPS byly 0, teprve
//   ELIGIBLE odliší „shody cen skoro nejsou" od „máme rozbitý čítač".
thread_local long g_blitzPushTieEligible = 0;   // byl nosič A našlo se pole
thread_local long g_blitzPushTieFlips = 0;      // ...a tiebreak volbu ZMĚNIL
long takeBlitzPushTieEligibleInSearch() { long v=g_blitzPushTieEligible; g_blitzPushTieEligible=0; return v; }
long takeBlitzPushTieFlipsInSearch()    { long v=g_blitzPushTieFlips;    g_blitzPushTieFlips=0;    return v; }

// Delka NEJKRATSI cesty na pole sousedici s cilem (s rezervou na blok).
// -1 = nikam nevede. Pouziva se jen jako MERITKO pro hladovou chuzi.
int optimalPathStepsToAdjacent(const GameState& state, const Player& player,
                               Position target);

static constexpr int kInfCost = 1 << 28;

namespace {
// ============================================================================
// SDILENE BFS JADRO (M14b, 01.-09.09.2026) -- riziko-vazena Dijkstra z
// player.position. Cena kroku = 1 pole + P(neuspech)*kRiskMultiplier za
// tacklezonu/GFI (odvozeni a cisla viz puvodni komentar u
// nextStepTowardAdjacent nize -- historie a zmereni se NEKOPIRUJE dvakrat).
// Sdileno mezi `nextStepTowardAdjacent` (cil = pole vedle `target`, blokovan
// je `target` samotny -- tam stoji souper, kterym se neprochazi) a
// `nextStepToward` (cil = `target` samotny, blokovane pole je volitelne
// `blockedSquare`, napr. volny mic). ⛔ DVE KOPIE TEHOZ CENOVEHO VZORCE SE
// UZ JEDNOU ROZESLY (`endBlockActivation`, M1/N10, 25.08.) -- nekopirovat
// znovu, jen volat.
// `preferStraight`: pri PRESNE stejne cene upredonstni rovny krok pred
// diagonalnim (Manhattan tiebreak, `scoreMoveAction` mel tohle uz drive --
// `AdvanceWalksStraightAtEqualChebyshev`). Prida jen +1 jednotku klice na
// diagonalni krok -- zanedbatelne proti kScale=100 i proti nejmensimu
// realnemu riziku, takze to NIKDY nepretlaci skutecny rozdil v riziku/delce,
// jen rozhodne mezi jinak identickymi cestami. Default false, aby se
// nezmenilo uz zmerene a nasazene chovani `nextStepTowardAdjacent` (M14b).
//
// ============================================================================
// ⭐⭐⭐ REROLL Z DOVEDNOSTI DODGE VE VYBERU CESTY (M6/B3(a)-follow-up,
//   10.09.2026). Do dneska tady kazdy dodge stal holou `(cil-1)/6` a o
//   dovednosti se nevedelo -- elf s Dodge si vybiral cestu PRESNE JAKO
//   trpaslik bez ni, ackoli ma tyz krok znatelne levnejsi (r. 8078-8090:
//   jeden reroll selhaneho dodge NA HRACE A TAH; soused s Tackle ho na TOM
//   poli rusi, r. 8566-8571).
//
// ⭐ RESENI: ZDVOJENY STAV NA UZEL. Uzel neni pole, ale (pole, vrstva):
//     vrstva 0 = reroll jeste netknuty,  vrstva 1 = reroll uz utraceny.
//   Prvni PRIPUSTNY dodge na ceste se oceni `p*p` (selze jen kdyz selze
//   dvakrat) a cestu prevede do vrstvy 1; kazdy dalsi dodge uz stoji hole
//   `p`. Dodge, ktery reroll pouzit NESMI (Tackle u opousteneho pole),
//   stoji `p` a ve vrstve 0 ZUSTAVA -- dovednost se neutratila.
//   ⭐ Obe vrstvy jsou samostatne relaxovatelne (cesta pres Tackle-pole
//     dorazi jinam s netknutym rerollem nez cesta, ktera uz ho spalila), a
//     odpoved pro pole je LEVNEJSI z jeho dvou vrstev -- `bestLayerIdx`.
//
// ⛔⛔ A TED TO, CO SE NESMI ZAMLCET: TOHLE JE APROXIMACE, NE EXAKTNI CENA.
//   Exaktni tady byt NEMUZE: po pripustnem dodge je reroll zivy s pst. `q`
//   a utraceny s pst. `p*q`, takze nasledník je ROZDELENI PRES STAVY, ne
//   stav. Dijkstra ale potrebuje na uzel JEDEN skalar -- rozdeleni se do nej
//   nevejde a zadne zdvojeni to nespravi.
//   ⇒ Chyba ma ZNAMY SMER: pravidlo "prvni pripustny dodge za p², dal hole
//     p" PODCENUJE reroll na cestach se DVEMA A VICE dodgi, protoze dodge,
//     ktery vyjde uz prirozene, dovednost NESPOTREBUJE a reroll ma zustat k
//     dispozici i pro ten dalsi. ⇒ riziko se PREPLACI, cesta se jevi
//     nebezpecnejsi, nez je. Pro VYBER cesty je to bezpecny smer
//     (konzervativni), pro cislo, ktere se vydava jako pravdepodobnost, by
//     to bezpecne NEBYLO.
//   ⭐⭐ EXAKTNI OSETRENI ZIJE JINDE a jsou to DVE RUZNE VECI, ktere se
//     NESMI ZAMENOVAT: `pathFailProb` (nize v tomhle souboru) a
//     `estimateApproachFailChance` (macro_actions.cpp) chodi po UZ ZVOLENE,
//     plne zname ceste, takze si mohou dovolit dvoustavovy dopredny pruchod
//     R/S (R = vsechny dodge vysly prirozene, reroll netknuty; S = vysly,
//     ale reroll je pryc) a vraci PRESNOU pst. Tady se nepocita
//     pravdepodobnost, ale CENA VYBERU -- jina uloha, jine reseni.
//     ⛔ Zamena tehle dvojice uz tenhle projekt jednou stala korekci
//       (`cf8634e8`, 09.09.: "reroll se utrati az za SELHANY hod"). Kdo sem
//       priste sahne, at nekopiruje p² do `pathFailProb` ani R/S sem.
//
// ⚠️ Hrac BEZ dovednosti Dodge do vrstvy 1 NIKDY nevstoupi (prechod je za
//   `hasDodge`), takze cely beh je BIT ZA BITEM tentyz jednovrstvovy
//   Dijkstra jako pred touhle zmenou -- stejne poradi vybirani minima,
//   stejne klice, stejne `parent[]`. To je nosne: `nextStepTowardAdjacent`
//   je NASAZENA A UZ ZMERENA cesta M14b (viz cisla nize) a trpaslici se
//   nesmi pohnout ani o jednotku klice. Hlida to vlastni test.
// ⛔ GFI se NEDOTYKA -- `rerollAvailable=false` zustava. TYMOVY reroll je
//   sdileny zdroj napric celym tahem, jina a nedoresena uloha; zaparkovana
//   schvalne v `evidence/celotah_situace.md`, oddil `A7`. Neprepinat.
// ============================================================================
void riskWeightedDijkstra(const GameState& state, const Player& player,
                          int budget, Position blockedSquare,
                          int* key, int8_t* steps, int16_t* parent,
                          bool preferStraight = false) {
    constexpr double kRiskMultiplier = 4.0;
    constexpr int kScale = 100;     // 1 pole = 100 jednotek klice
    const int freeSteps = movementAfterStandUp(player);   // bez GFI
    const bool rerollAvailable = false;   // viz odduvodneni u puvodni funkce
    const bool blizzard = (state.weather == Weather::BLIZZARD);
    const bool hasDodge = player.hasSkill(SkillName::Dodge);
    bool done[kNodeCount];
    for (int i = 0; i < kNodeCount; ++i) {
        key[i] = kInfCost; parent[i] = -1; done[i] = false; steps[i] = 127;
    }

    const int startIdx = gridIdx(player.position.x, player.position.y);
    key[startIdx] = 0;      // start je ve vrstve 0: reroll netknuty
    steps[startIdx] = 0;

    for (;;) {
        int cur = -1, best = kInfCost;
        for (int i = 0; i < kNodeCount; ++i)
            if (!done[i] && key[i] < best) { best = key[i]; cur = i; }
        if (cur < 0) break;
        done[cur] = true;
        const int curSq = cur % GRID_SIZE;
        const int curLayer = cur / GRID_SIZE;
        Position curPos{static_cast<int8_t>(curSq % GRID_W),
                        static_cast<int8_t>(curSq / GRID_W)};
        if (steps[cur] >= budget) continue;

        for (auto& np : curPos.getAdjacent()) {
            if (!np.isOnPitch()) continue;
            if (np == blockedSquare) continue;    // blokovane pole se neprochazi
            if (state.getPlayerAtPosition(np) != nullptr) continue;
            const int nSq = gridIdx(np.x, np.y);
            const int nStep = steps[cur] + 1;
            int risk = 0;
            int nLayer = curLayer;
            // ⛔⛔⛔ OPRAVA 10.09.2026: DODGE SE HAZI PRI VYSTUPU, NE PRI VSTUPU.
            //   Do dneska tu stalo `countTacklezones(state, np, ...)`, tedy
            //   brana na CILOVEM poli. Pravidla (rules_bb2016.txt r. 480-486)
            //   rikaji opak, a doslova:
            //     „In order to LEAVE a square that is in one or more opposing
            //      tackle zones, a player must dodge out of the square... you
            //      must ALWAYS make a Dodge roll when you leave a tackle zone;
            //      EVEN IF there aren't any tackle zones on the square you are
            //      moving to."
            //   ⇒ Brana je pole, ktere se OPOUSTI (`curPos`). Obtiznost se
            //     naopak bere z CILE (r. 503-505: „Per opposing tackle zone on
            //     the square that the player is dodging to +1") -- a to
            //     `calculateDodgeTarget(dest=np, source=curPos)` uz delalo
            //     spravne, takze se nemeni.
            // ⭐ NALEZENO pres K5: `pathFailProb` vracela pro ustup z kontaktu
            //   PRESNE 0,0000 pro AG 1..4, pritom resolver hazi 5+ az 2+ --
            //   protoze ustupove pole je z definice mimo vsechny tacklezony,
            //   takze „vstupni" brana na nem neuctovala NIC.
            //   ⇒ Model pod-ocenoval UTEKY a pre-ocenoval PRIBLIZENI.
            // ⚠️ Meni to nasazenou cestu M14b, ale je to PRAVIDLOVA oprava
            //   (FRONTA A: „vady prubezne, bez ohledu na deltu"), ne taktika.
            //   `estimateApproachFailChance` uz to melo spravne (`cur`).
            if (countTacklezones(state, curPos, player.teamSide) > 0) {
                // `calculateDodgeTarget` uz vraci cil clampnuty do [2,6],
                // takze P_fail sama od sebe padne do [1/6, 5/6].
                const int dodgeTarget =
                    calculateDodgeTarget(state, player, /*dest=*/np, /*source=*/curPos);
                const double pFail = (dodgeTarget - 1) / 6.0;
                // Reroll se smi vzit jen ve vrstve 0 a jen kdyz ho soused
                // s Tackle u OPOUSTENEHO pole nerusi (`curPos`, ne `np` --
                // dodge se hazi pri VYSTUPU z tacklezony, r. 8566-8571).
                double priced = pFail;
                if (hasDodge && curLayer == 0 &&
                    !tackleNegatesDodgeReroll(state, player, curPos)) {
                    priced = pFail * pFail;
                    nLayer = 1;                 // pravo se tu povazuje za utracene
                }
                risk += static_cast<int>(std::lround(priced * kRiskMultiplier * kScale));
            }
            // GFI marginalne: kolik pole navic PRIDA k riziku uz naplanovane
            // sekvence. Strop je hracuv skutecny `maxGfiSquares` (2, se
            // Sprintem 3).
            const int gfiCap = maxGfiSquares(player);
            const int gfiBefore = std::clamp(steps[cur] - freeSteps, 0, gfiCap);
            const int gfiAfter  = std::clamp(nStep      - freeSteps, 0, gfiCap);
            if (gfiAfter > gfiBefore) {
                const double dFail =
                    gfiSequenceFailProb(gfiAfter,  rerollAvailable, blizzard) -
                    gfiSequenceFailProb(gfiBefore, rerollAvailable, blizzard);
                risk += static_cast<int>(std::lround(dFail * kRiskMultiplier * kScale));
            }
            const int nIdx = nLayer * GRID_SIZE + nSq;
            if (done[nIdx]) continue;
            const int diagBias = (preferStraight && np.x != curPos.x && np.y != curPos.y) ? 1 : 0;
            const int nk = key[cur] + kScale + risk + diagBias;
            if (nk < key[nIdx]) {
                key[nIdx] = nk;
                steps[nIdx] = static_cast<int8_t>(nStep);
                parent[nIdx] = static_cast<int16_t>(cur);
            }
        }
    }
}

// Levnejsi z obou vrstev pole `sq` -- vraci INDEX UZLU (tedy uz s vrstvou,
// aby na nem slo backtrackovat `parent[]`), nebo -1 kdyz je pole
// nedosazitelne v obou. Pri PRESNE stejne cene vyhrava vrstva 0; hrac bez
// dovednosti ma vrstvu 1 celou na `kInfCost`, takze mu to vzdy vrati presne
// to, co vracel jednovrstvovy kod.
// ============================================================================
// ⭐ ODSUN OD NOSICE JE SOUCAST VYBERU POLE DOSEDNUTI (B-ROUND / pripad 1,
//   10.09.2026). Uzivatel: "predřaď ten blitz s vyberem odkud, at jej odsuneš
//   od nosiče."
//   Zaver 25.08. ke stejne polozce: "vyber blitzu neni jen »kdo × koho«, ale
//   »z KTEREHO POLE« -- smer odsunu je soucast rozhodnuti."
//
// Mechanismus: pole dosednuti `L` urcuje utocnou linii `L->T` a odstrceni jde
// po ni dozadu, takze `L` FIXUJE MNOZINU kandidatu na odsun (rovne vzad + dva
// sousedi, `getPushbackSquares`). `pushDestScore` (P9c, block_handler.cpp,
// nasazeno 08.09.) uz mezi TEMI kandidaty vybira nejlepsi, jeho PRIMARNI
// kriterium je vzdalenost cile odsunu od NASEHO nosice se stropem 4. Tady se
// tedy `L` oceni tim, co z nej P9c NEJLEPE dokaze vytezit -- stejne kriterium,
// jen o vrstvu vys.
// ⚠️ Hypoteza (moje, ne merena): druhotne z toho tez padne telo blitzujiciho na
//   uhloprícku klece (`evidence/cage_built_20260910.md`: 0 rohu ve 33,1 % kol,
//   pritom vlastni telo sousedi s nosicem v 79,2 % -- tela jsou, jen ne na
//   uhloprickach). Nemereno, netvrdit jako nalez.
//
// ⛔⛔ TOHLE JE TIEBREAK, NE NOVA OPTIMALIZACE. `nextStepTowardAdjacent` je
//   nasazena a UZ ZMERENA cesta M14b; cena cesty zustava STRIKTNE primarni a
//   preference rozhoduje VYHRADNE pri PRESNE stejnem `key`. Bez vlastniho
//   nosice na hristi je funkce nula ⇒ chovani BIT ZA BITEM tehoz. Hlidaji to
//   dva plosne zamky v `test_pathfinder.cpp` (bez mice / mic u soupere).
static const Player* ourCarrierForPush(const GameState& state, const Player& mover) {
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return nullptr;
    const Player& c = state.getPlayer(state.ball.carrierId);
    if (c.teamSide != mover.teamSide || !c.isOnPitch()) return nullptr;
    if (c.id == mover.id) return nullptr;   // nosic blitzujici sam sebe neodsouva
    return &c;
}

// Nejlepsi dosazitelna vzdalenost odsunu od nosice, kdyz se utoci z `from` na
// `target`. Strop 4 je STEJNY jako v `pushDestScore` -- za nim je odsunuty hrac
// od mice tak ci tak mimo dosah a dalsi tlaceni nekupuje nic.
static int pushAwayScore(Position from, Position target, Position carrierPos) {
    Position cand[3];
    const int n = getPushbackSquares(from, target, cand);
    int best = 0;
    for (int i = 0; i < n; ++i) {
        const int d = std::min(std::max(std::abs(cand[i].x - carrierPos.x),
                                        std::abs(cand[i].y - carrierPos.y)), 4);
        if (d > best) best = d;
    }
    return best;
}

inline int bestLayerIdx(const int* key, int sq) {
    const int a = key[sq], b = key[GRID_SIZE + sq];
    if (a >= kInfCost && b >= kInfCost) return -1;
    return (b < a) ? (GRID_SIZE + sq) : sq;
}
} // namespace

bool nextStepTowardAdjacent(const GameState& state, const Player& player,
                            Position target, Position& outStep) {
    const int budget = movementAfterStandUp(player) + maxGfiSquares(player);
    if (budget <= 0) return false;

    // ⭐⭐ CENA KROKU = 1 POLE + K ZA TACKLEZONU (M14b, 01.09.2026).
    //   ZMERENO, ne odhadnuto: z vyhozenych blitzu je 2 953 z 3 420 (86 %)
    //   TURNOVER PRI DOBEHU -- blitzujici vlezl do tacklezony, hodil dodge
    //   nebo GFI a slozil se. Ne "nedosel", ale "po ceste zahodil kolo".
    //   ⛔ Puvodni `pickApproachStep` ma skore `vzdalenost*100 + TZ*12`, takze
    //     vzdalenost prevazi STOKRAT: chuze vleze do tacklezony, aby se
    //     priblizila o JEDNO pole, a vymeni jisty krok za ~1/3 sanci na
    //     ztratu kola.
    //   ⛔ A proto nepomohla ani prvni verze teto funkce (lexikograficka):
    //     minimalizovala TZ jen mezi STEJNE DLOUHYMI cestami, delsi ale
    //     bezpecnou cestu vzit neumela -- a presne o tu tady jde.
    //   ⇒ K = 2: krok do tacklezony stoji jako dve pole navic. Neni to ladici
    //     parametr, je to pomer rizika: dodge na 4+ pada ve tretine pripadu
    //     a stoji CELE KOLO, kdezto pole navic nestoji nic, dokud je rozpocet.
    //   ⚠️ DELKA se hlida ZVLAST (`steps <= budget`), takze zdrazeni nikdy
    //     nepovoli cestu, na kterou hrac nema pohyb -- a kdyz je tacklezona
    //     NEVYHNUTELNA, projde se, protoze levnejsi varianta neexistuje.
    // ⛔⛔ GFI NENI ZADARMO (nalez 01.09., druhe mereni). Prvni verze te ceny
    //   brala pole navic jako bezplatne -- jenze pole ZA HRANICI MA je Go For
    //   It a pada v sestine pripadu. Chuze proto uhnula tacklezone a misto
    //   dodge hodila GFI: turnovery pri dobehu 2 953 -> 3 107, tedy RIZIKO SE
    //   PRESUNULO, ne zmizelo. Cena kroku musi znat OBOJI.
    //   Pomer: dodge na 4+ pada ~1/3, GFI na 2+ pada 1/6 => tacklezona je
    //   zhruba dvakrat drazsi nez GFI. Odtud 2 a 1, ne z ladeni.
    //
    // ⛔⛔⛔ PAUSALNI CENA BYLA CHYBA -- ZMERENO (M14b, 08.09.2026).
    //   Parove A/B, 4 800 dvojic dw-dw (`ab_m14b_20260907/`):
    //   DELTA −0,0170 ± 0,0062 SE = −2,72 σ, 95% CI [−0,0292; −0,0048].
    //   Rameno tedy neni "bez ucinku", ono SKODI -- a mechanismus je znamy:
    //   `kTzCost` i `kGfiCost` byly PLOSNE, nezavisle na skutecnem riziku TOHO
    //   pole. Dodge na 3+ (pada 1/6) stal presne tolik co dodge na 6+ (pada
    //   5/6), takze chuze platila dve pole obchazky i za pole, ktere ji
    //   ohrozovalo sestinou. Obchazky byly prilis caste a prilis drahe.
    //
    //   ⭐ OPRAVA: cena rizika je PRAVDEPODOBNOSTNI, ne pausalni --
    //   cena = P(neuspech) * kRiskMultiplier v "polich". Vzorec (multiplikator
    //   4.0, reroll natvrdo false, GFI marginalne) ted zije ve sdilenem jadru
    //   `riskWeightedDijkstra` vyse -- historie kalibrace (6.0 -> 4.0, spatny
    //   vypocet 1/3 vs 1/2) zustava zapsana tam.
    //
    //   09.09.2026: zobecneno na OBECNY pohyb pres `nextStepToward` nize --
    //   W-GFI sonda ukazala, ze `movePlayerToward` (REPOSITION/SCORE/
    //   HAND_OFF_SCORE) mel tutez tridu vady (hladovy vyber bez pameti cesty).
    // ⭐ Pole jsou DVOUVRSTVOVA (reroll netknuty / utraceny) -- viz komentar
    //   u `riskWeightedDijkstra`. Index je UZEL, ne pole; pole = `i % GRID_SIZE`.
    int key[kNodeCount];            // cena (pole + riziko)
    int8_t steps[kNodeCount];       // ciste pole -- na tohle se vaze rozpocet
    int16_t parent[kNodeCount];
    riskWeightedDijkstra(state, player, budget, /*blockedSquare=*/target, key, steps, parent);
    const int startIdx = gridIdx(player.position.x, player.position.y);

    // Cilove pole: sousedi s `target` a v rozpoctu zbyva pole na BLOK
    // (r. 549-550, "the block costs one square of movement"). Obe vrstvy
    // soutezi ve TOMTEZ cyklu, takze "levnejsi z vrstev" vyjde samo -- a
    // zaroven se rozpocet hlida na KAZDE vrstve zvlast.
    // ⭐ Pri PRESNE stejne cene rozhoduje SMER ODSUNU -- viz `pushAwayScore`
    //   vyse. `carrier == nullptr` (nemame mic, nosic je mimo hriste, nebo je
    //   to blitzujici sam) ⇒ vsechna `tie` jsou 0, prvni nalezene minimum tedy
    //   vyhrava presne jako pred touhle zmenou.
    const Player* carrier = ourCarrierForPush(state, player);
    int bestIdx = -1, bestKey = kInfCost, bestTie = -1;
    // ⭐ Co by vybralo pravidlo BEZ tiebreaku -- tedy prvni STRIKTNI minimum
    //   ceny, presne jak to delal kod pred `a1d9b77d`. Slouzi jen cítaci:
    //   `flips` smi tiknout jen kdyz se vitez opravdu LISI (viz r. 54-70).
    int costOnlyIdx = -1;
    for (int i = 0; i < kNodeCount; ++i) {
        const int sq = i % GRID_SIZE;
        if (key[i] >= kInfCost || sq == startIdx) continue;
        if (steps[i] > budget - 1) continue;
        Position p2{static_cast<int8_t>(sq % GRID_W), static_cast<int8_t>(sq / GRID_W)};
        if (p2.distanceTo(target) != 1) continue;
        const int tie = carrier ? pushAwayScore(p2, target, carrier->position) : 0;
        if (key[i] < bestKey || (key[i] == bestKey && tie > bestTie)) {
            bestKey = key[i]; bestIdx = i; bestTie = tie;
        }
        if (costOnlyIdx < 0 || key[i] < key[costOnlyIdx]) costOnlyIdx = i;
    }
    if (bestIdx < 0) return false;
    if (carrier) {
        ++g_blitzPushTieEligible;
        if (bestIdx != costOnlyIdx) ++g_blitzPushTieFlips;
    }

    int idx = bestIdx;
    while (parent[idx] != -1 && parent[idx] != startIdx) idx = parent[idx];
    if (parent[idx] != startIdx) return false;
    outStep = Position{static_cast<int8_t>((idx % GRID_SIZE) % GRID_W),
                       static_cast<int8_t>((idx % GRID_SIZE) / GRID_W)};
    const Position greedy = pickApproachStep(state, player, player.position, target);
    if (greedy != outStep) ++g_blitzPathPicks;
    return true;
}

// Viz pathfinder.h -- 09.09.2026, zobecneni M14b pro OBECNY pohyb
// (`movePlayerToward`, macro_actions.cpp). `budget` je EXPLICITNI parametr
// (ne odvozeny z hrace), protoze volajici (napr. W-GFI rameno) muze chtit
// mensi rozpocet, nez je hracovo absolutni `maxGfiSquares`.
//
// ⭐ NEJDE JEN O "presny cil, jinak selhat" -- nekterym volajicim (REPOSITION
//   na geometricky, dosud neopravovany cil, viz W-CIL v task_queue.md) se
//   dnes vydavaji cile, ktere mohou byt obsazene. Stara hladova chuze na to
//   reagovala priblizenim NA DORAZ (dokud neco nezastavilo pokrok), pak
//   selhala (smycka/limit). Aby BFS nahradila i tohle chovani a nejen
//   presny-cil pripad, hleda GLOBALNE nejblizsi dosazitelne pole -- misto
//   hadani krok po kroku, ktere osciluje.
bool nextStepToward(const GameState& state, const Player& player,
                    Position target, int budget, Position blockedSquare,
                    Position& outStep) {
    if (budget <= 0) return false;
    if (target == player.position) return false;   // volajici uz je na cili

    int key[kNodeCount];
    int8_t steps[kNodeCount];
    int16_t parent[kNodeCount];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int startIdx = gridIdx(player.position.x, player.position.y);

    const int curDist = player.position.distanceTo(target);
    int bestDist = curDist, bestKeyAmongTies = kInfCost, goalIdx = -1;
    for (int i = 0; i < kNodeCount; ++i) {
        const int sq = i % GRID_SIZE;
        if (key[i] >= kInfCost || sq == startIdx) continue;
        Position p2{static_cast<int8_t>(sq % GRID_W), static_cast<int8_t>(sq / GRID_W)};
        const int d = p2.distanceTo(target);
        if (d < bestDist || (d == bestDist && key[i] < bestKeyAmongTies)) {
            bestDist = d; bestKeyAmongTies = key[i]; goalIdx = i;
        }
    }
    if (goalIdx < 0) return false;   // nic nezlepsi vzdalenost -- opravdu zaseknuto

    int idx = goalIdx;
    while (parent[idx] != -1 && parent[idx] != startIdx) idx = parent[idx];
    if (parent[idx] != startIdx) return false;
    outStep = Position{static_cast<int8_t>((idx % GRID_SIZE) % GRID_W),
                       static_cast<int8_t>((idx % GRID_SIZE) / GRID_W)};
    return true;
}

// 09.09.2026 (W-GFI gap oprava): delka NEJLEVNEJSI (riziko-vazene) cesty
// PRESNE na `target`, v ramci `budget` kroku -- -1 = nedosazitelne. Na
// rozdil od `nextStepToward` (ktery vraci jen dalsi krok) tohle vraci
// CELKOVY POCET KROKU, protoze volajici (W-GFI: kolik GFI poli chybi) ho
// potrebuje jako MERITKO, ne k chuzi. `preferStraight=true` shoduje se
// s tim, jakou cestu `nextStepToward` skutecne pouzije -- jinak by se
// merilo neco jineho, nez co pak walker udela.
int pathStepsToward(const GameState& state, const Player& player,
                    Position target, int budget, Position blockedSquare) {
    if (budget <= 0) return -1;
    if (target == player.position) return 0;

    int key[kNodeCount];
    int8_t steps[kNodeCount];
    int16_t parent[kNodeCount];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int targetIdx = bestLayerIdx(key, gridIdx(target.x, target.y));
    if (targetIdx < 0) return -1;
    return steps[targetIdx];
}

// ⭐⭐⭐ 09.09.2026 (uzivatel: "riskantni dodge se ma taky vyhodnotit a
//   kdyztak neprovest"). CELKOVA pravdepodobnost neuspechu (turnover)
//   NEJLEVNEJSI cesty na `target` -- kombinuje VSECHNY tacklezone-dodge
//   kroky NA CESTE a GFI kroky, ne jen GFI. Zmereno (ab_wgfi sonda):
//   z granted-GFI turnoveru bylo DODGE 70,9 %, GFI jen 29,1 % -- W-GFI
//   rameno cenilo jen GFI a dodge riziko na sve vlastni ceste vubec
//   nevidelo. Nezavisle udalosti: P(fail) = 1 - prod(1-p_i) pres CELOU
//   cestu. -1.0 = nedosazitelne v rozpoctu.
double pathFailProb(const GameState& state, const Player& player,
                    Position target, int budget, Position blockedSquare) {
    if (budget <= 0) return -1.0;
    if (target == player.position) return 0.0;

    int key[kNodeCount];
    int8_t steps[kNodeCount];
    int16_t parent[kNodeCount];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int startIdx = gridIdx(player.position.x, player.position.y);
    const int targetIdx = bestLayerIdx(key, gridIdx(target.x, target.y));
    if (targetIdx < 0) return -1.0;

    // Zrekonstruuj CELOU cestu (ne jen prvni krok jako nextStepToward) --
    // parent[] jde od cile zpatky ke startu. `chain` nese INDEXY UZLU
    // (pole + vrstva rerollu), pole se z nich vytahuje `% GRID_SIZE`.
    int chain[kNodeCount];
    int n = 0;
    for (int idx = targetIdx; idx != startIdx; idx = parent[idx]) {
        if (idx < 0 || n >= kNodeCount) return -1.0;  // nemelo by nastat pri validnim key[]
        chain[n++] = idx;
    }

    // Stejne konstanty jako `riskWeightedDijkstra` (rerollAvailable natvrdo
    // false -- viz odduvodneni tamtez), aby se cenilo totez, co se pak
    // skutecne pojede.
    const bool rerollAvailable = false;
    const bool blizzard = (state.weather == Weather::BLIZZARD);
    const int freeSteps = movementAfterStandUp(player);
    const int gfiCap = maxGfiSquares(player);

    // ⭐⭐⭐ M6/B3(a)-follow-up (10.09.2026): TADY JE OSETRENI REROLLU EXAKTNI.
    //   Na rozdil od `riskWeightedDijkstra` vyse (ktera VYBIRA cestu a musi si
    //   vystacit s jednim skalarem na uzel, viz jeji komentar o aproximaci)
    //   tahle funkce chodi po UZ ZVOLENE, plne zname ceste -- takze si smi
    //   drzet cele rozdeleni pres dva stavy, presne jako
    //   `estimateApproachFailChance` (macro_actions.cpp):
    //     R = P(vsechny dosavadni dodge vysly PRIROZENE, reroll netknuty)
    //     S = P(vsechny dosavadni dodge vysly, ale reroll uz je pryc)
    //   Krok s prirozenou pst. selhani `p` (q = 1-p) a pripustnosti `e`
    //   (`e` = u opousteneho pole NENI soused s Tackle, r. 8566-8571):
    //     e:  S' = R*p*q + S*q ;  R' = R*q
    //     !e: S' = S*q         ;  R' = R*q
    //   P(cesta prosla bez dodge-selhani) = R + S, a to se s GFI clenem
    //   spoji touz nezavislou soucinovou kombinaci, jakou uz funkce pouziva.
    // ⛔ NEZAMENOVAT S APROXIMACI VE VYBERU CESTY. Tohle vraci
    //   PRAVDEPODOBNOST, na ktere se dela go/no-go rozhodnuti (W-GFI), takze
    //   "konzervativni preplaceni" by tu byla vada, ne bezpecna strana.
    // ⚠️ Hrac BEZ dovednosti jde PUVODNI vetvi (jeden skalar `successProb`,
    //   tytez operace v temz poradi), aby se jeho cislo nezmenilo ani o ULP.
    const bool hasDodge = player.hasSkill(SkillName::Dodge);
    double rerollLive = 1.0;   // R
    double rerollGone = 0.0;   // S

    Position cur = player.position;
    double successProb = 1.0;
    int stepCount = 0;
    for (int i = n - 1; i >= 0; --i) {
        const int nSq = chain[i] % GRID_SIZE;
        Position np{static_cast<int8_t>(nSq % GRID_W),
                    static_cast<int8_t>(nSq / GRID_W)};
        // ⛔ Taz oprava jako v `riskWeightedDijkstra` vyse (r. 480-486):
        //   brana je pole, ktere se OPOUSTI (`cur`), ne cilove (`np`).
        if (countTacklezones(state, cur, player.teamSide) > 0) {
            const int dodgeTarget = calculateDodgeTarget(state, player, np, cur);
            const double pFail = (dodgeTarget - 1) / 6.0;
            if (!hasDodge) {
                successProb *= (1.0 - pFail);
            } else {
                const double q = 1.0 - pFail;
                if (!tackleNegatesDodgeReroll(state, player, cur)) {
                    rerollGone = rerollLive * pFail * q + rerollGone * q;
                } else {
                    rerollGone = rerollGone * q;
                }
                rerollLive = rerollLive * q;   // az PO S, ktere stare R potrebuje
            }
        }
        const int nStep = stepCount + 1;
        const int gfiBefore = std::clamp(stepCount - freeSteps, 0, gfiCap);
        const int gfiAfter  = std::clamp(nStep      - freeSteps, 0, gfiCap);
        if (gfiAfter > gfiBefore) {
            const double dFail = gfiSequenceFailProb(gfiAfter, rerollAvailable, blizzard)
                               - gfiSequenceFailProb(gfiBefore, rerollAvailable, blizzard);
            successProb *= (1.0 - dFail);
        }
        cur = np;
        stepCount = nStep;
    }
    if (hasDodge) successProb *= (rerollLive + rerollGone);
    return 1.0 - successProb;
}

bool canReachAdjacentTo(const GameState& state, const Player& player,
                        Position target, Position& outAdjacent,
                        int reserveMove) {
    if (!player.isOnPitch() || player.state == PlayerState::STUNNED) return false;

    // M13 krok B (31.08.2026): rozpocet po vstani ma JEDNO misto
    // (helpers.cpp, r. 690-695 + 8196-8198). Tady se drive odecitalo 3
    // natvrdo, coz je vadne POD 3 MA: pravidlo tam nedava zaporny rozpocet,
    // ale hod na 4+ a pak NULU ("he may not move further squares unless he
    // Goes For It"). Treemanovi (MA2) vychazelo maxMove -1 a blitz z lehu
    // se neanabidl ani pres GFI, ktere pravidlo vyslovne dovoluje.
    const int maxMove = movementAfterStandUp(player);

    // N11 (24.08.2026): Take Root, l. 8577-8579 -- zakorenený "may not Go For
    // It, be pushed back for any reason, or use any skill that would allow him
    // to move out of his current square". Zakaz zil jen v nabidce MOVE a v
    // blitz-bloku; pathfinder o nem nevedel, takze zakorenenemu Treemanovi se
    // nabidl BLITZ na nesousedni cil a smycka ho pres GFI opravdu posunula.
    int maxGfi = player.rooted ? 0 : (player.hasSkill(SkillName::Sprint) ? 3 : 2);
    int maxRange = maxMove + maxGfi - reserveMove;

    if (maxRange <= 0) return false;

    // Quick distance check
    int dist = player.position.distanceTo(target);
    if (dist > maxRange + 1) return false; // too far even in best case

    // BFS
    bool visited[GRID_SIZE];
    int8_t costAt[GRID_SIZE];
    std::memset(visited, 0, sizeof(visited));
    std::memset(costAt, 127, sizeof(costAt)); // max cost

    PathNode queue[GRID_SIZE];
    int qHead = 0, qTail = 0;

    // ⭐⭐ M13 krok B (31.08.2026): VSTANI SE UCTOVALO DVAKRAT.
    // `maxMove` uz vstani odecetl (vyse), a pak ho BFS ucetl jeste jednou jako
    // pocatecni cenu 3. Lezici MA6 tak mel maxRange 3+2-1=4 a zaroven start na
    // 3, takze mu na kroky zbyval JEDEN -- dosahl jen na souseda o dve pole.
    // Rozpocet je pritom jeden: pohyb po vstani + GFI, minus 1 na blok.
    const int startCost = 0;

    int startIdx = gridIdx(player.position.x, player.position.y);
    visited[startIdx] = true;
    costAt[startIdx] = startCost;
    queue[qTail++] = {player.position.x, player.position.y,
                      static_cast<int8_t>(startCost), false};

    Position bestAdj{-1, -1};
    int bestCost = 999;

    while (qHead < qTail) {
        PathNode cur = queue[qHead++];

        // Check if current position is adjacent to target
        Position curPos{cur.x, cur.y};
        if (curPos.distanceTo(target) == 1 && curPos != player.position) {
            if (cur.cost < bestCost) {
                bestCost = cur.cost;
                bestAdj = curPos;
            }
        }

        // Expand neighbors
        auto adj = curPos.getAdjacent();
        for (auto& np : adj) {
            if (!np.isOnPitch()) continue;

            int nIdx = gridIdx(np.x, np.y);
            if (visited[nIdx]) continue;

            // Can't move through occupied squares (except target itself for adjacent check)
            if (np != target && state.getPlayerAtPosition(np) != nullptr) continue;
            if (np == target) continue; // don't enter the target's square

            int newCost = cur.cost + 1;
            if (newCost > maxRange) continue;

            visited[nIdx] = true;
            costAt[nIdx] = newCost;
            queue[qTail++] = {np.x, np.y, static_cast<int8_t>(newCost), false};
        }
    }

    if (bestAdj.x >= 0) {
        outAdjacent = bestAdj;
        return true;
    }
    return false;
}

// ⛔⛔ `getValidMoveTargets` ODSTRANENA 02.09.2026 — MRTVY KOD, KTERY SE PRESTO
//   UDRZOVAL. V celem repu nemela JEDINEHO volajiciho (overeno grepem pres
//   *.cpp/*.h/*.py), a presto 24.08. dostala opravu N11 na `rooted`.
//
//   ⭐ DUVOD ODSTRANENI NENI UKLID, ALE DVE KOPIE TEHOZ PRAVIDLA:
//   pocitala nabidku pohybu (rozpocet, GFI, Sprint, rooted) podruhe vedle
//   `rules_engine.cpp`, a UZ SE ROZESLA -- zacinala na `canAct(state)`, tedy
//   jen STANDING, takze o M13 (lezici smi deklarovat akci, 31.08.) nevedela.
//   Kdyby ji nekdo zapojil, vratil by tim chovani pred M13.
//   „Dve kopie tehoz pravidla znamenaji, ze jedna zestarne" -- tenhle engine
//   to ma doloženo u `endBlockActivation` (25.08.) i u nabidky blitzu.
//
//   Historie je v gitu (naposledy `6e2f084c`), takze se nic neztratilo.
//   Kdyby se pathfinderova nabidka nekdy hodila, patri do `rules_engine`,
//   ne vedle nej.

int optimalPathStepsToAdjacent(const GameState& state, const Player& player,
                               Position target) {
    // Ciste BFS po POCTU POLI (bez vahy tacklezon) -- meritkem je DELKA,
    // ne bezpecnost. Kdyby se vazilo, porovnavalo by se s necim jinym, nez
    // hladova chuze vubec zkousi.
    const int budget = movementAfterStandUp(player) + maxGfiSquares(player);
    if (budget <= 0) return -1;
    int8_t dist[GRID_SIZE];
    std::memset(dist, -1, sizeof(dist));
    PathNode q[GRID_SIZE]; int h = 0, t = 0;
    const int s0 = gridIdx(player.position.x, player.position.y);
    dist[s0] = 0;
    q[t++] = {player.position.x, player.position.y, 0, false};
    int best = -1;
    while (h < t) {
        PathNode cur = q[h++];
        Position cp{cur.x, cur.y};
        if (cp != player.position && cp.distanceTo(target) == 1 &&
            dist[gridIdx(cur.x, cur.y)] <= budget - 1) {
            best = dist[gridIdx(cur.x, cur.y)];
            break;                       // BFS => prvni nalezene je nejkratsi
        }
        if (dist[gridIdx(cur.x, cur.y)] >= budget) continue;
        for (auto& np : cp.getAdjacent()) {
            if (!np.isOnPitch() || np == target) continue;
            if (state.getPlayerAtPosition(np) != nullptr) continue;
            const int ni = gridIdx(np.x, np.y);
            if (dist[ni] >= 0) continue;
            dist[ni] = static_cast<int8_t>(dist[gridIdx(cur.x, cur.y)] + 1);
            q[t++] = {np.x, np.y, dist[ni], false};
        }
    }
    return best;
}

} // namespace bb

