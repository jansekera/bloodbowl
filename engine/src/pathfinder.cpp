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
void riskWeightedDijkstra(const GameState& state, const Player& player,
                          int budget, Position blockedSquare,
                          int* key, int8_t* steps, int16_t* parent,
                          bool preferStraight = false) {
    constexpr double kRiskMultiplier = 4.0;
    constexpr int kScale = 100;     // 1 pole = 100 jednotek klice
    const int freeSteps = movementAfterStandUp(player);   // bez GFI
    const bool rerollAvailable = false;   // viz odduvodneni u puvodni funkce
    const bool blizzard = (state.weather == Weather::BLIZZARD);
    bool done[GRID_SIZE];
    for (int i = 0; i < GRID_SIZE; ++i) {
        key[i] = kInfCost; parent[i] = -1; done[i] = false; steps[i] = 127;
    }

    const int startIdx = gridIdx(player.position.x, player.position.y);
    key[startIdx] = 0;
    steps[startIdx] = 0;

    for (;;) {
        int cur = -1, best = kInfCost;
        for (int i = 0; i < GRID_SIZE; ++i)
            if (!done[i] && key[i] < best) { best = key[i]; cur = i; }
        if (cur < 0) break;
        done[cur] = true;
        Position curPos{static_cast<int8_t>(cur % GRID_W),
                        static_cast<int8_t>(cur / GRID_W)};
        if (steps[cur] >= budget) continue;

        for (auto& np : curPos.getAdjacent()) {
            if (!np.isOnPitch()) continue;
            if (np == blockedSquare) continue;    // blokovane pole se neprochazi
            if (state.getPlayerAtPosition(np) != nullptr) continue;
            const int nIdx = gridIdx(np.x, np.y);
            if (done[nIdx]) continue;
            const int tz = countTacklezones(state, np, player.teamSide);
            const int nStep = steps[cur] + 1;
            int risk = 0;
            if (tz > 0) {
                // `calculateDodgeTarget` uz vraci cil clampnuty do [2,6],
                // takze P_fail sama od sebe padne do [1/6, 5/6].
                const int dodgeTarget =
                    calculateDodgeTarget(state, player, /*dest=*/np, /*source=*/curPos);
                const double pFail = (dodgeTarget - 1) / 6.0;
                risk += static_cast<int>(std::lround(pFail * kRiskMultiplier * kScale));
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
    int key[GRID_SIZE];             // cena (pole + riziko)
    int8_t steps[GRID_SIZE];        // ciste pole -- na tohle se vaze rozpocet
    int16_t parent[GRID_SIZE];
    riskWeightedDijkstra(state, player, budget, /*blockedSquare=*/target, key, steps, parent);
    const int startIdx = gridIdx(player.position.x, player.position.y);

    // Cilove pole: sousedi s `target` a v rozpoctu zbyva pole na BLOK
    // (r. 549-550, "the block costs one square of movement").
    int bestIdx = -1, bestKey = kInfCost;
    for (int i = 0; i < GRID_SIZE; ++i) {
        if (key[i] >= kInfCost || i == startIdx) continue;
        if (steps[i] > budget - 1) continue;
        Position p2{static_cast<int8_t>(i % GRID_W), static_cast<int8_t>(i / GRID_W)};
        if (p2.distanceTo(target) != 1) continue;
        if (key[i] < bestKey) { bestKey = key[i]; bestIdx = i; }
    }
    if (bestIdx < 0) return false;

    int idx = bestIdx;
    while (parent[idx] != -1 && parent[idx] != startIdx) idx = parent[idx];
    if (parent[idx] != startIdx) return false;
    outStep = Position{static_cast<int8_t>(idx % GRID_W),
                       static_cast<int8_t>(idx / GRID_W)};
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

    int key[GRID_SIZE];
    int8_t steps[GRID_SIZE];
    int16_t parent[GRID_SIZE];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int startIdx = gridIdx(player.position.x, player.position.y);

    const int curDist = player.position.distanceTo(target);
    int bestDist = curDist, bestKeyAmongTies = kInfCost, goalIdx = -1;
    for (int i = 0; i < GRID_SIZE; ++i) {
        if (key[i] >= kInfCost || i == startIdx) continue;
        Position p2{static_cast<int8_t>(i % GRID_W), static_cast<int8_t>(i / GRID_W)};
        const int d = p2.distanceTo(target);
        if (d < bestDist || (d == bestDist && key[i] < bestKeyAmongTies)) {
            bestDist = d; bestKeyAmongTies = key[i]; goalIdx = i;
        }
    }
    if (goalIdx < 0) return false;   // nic nezlepsi vzdalenost -- opravdu zaseknuto

    int idx = goalIdx;
    while (parent[idx] != -1 && parent[idx] != startIdx) idx = parent[idx];
    if (parent[idx] != startIdx) return false;
    outStep = Position{static_cast<int8_t>(idx % GRID_W),
                       static_cast<int8_t>(idx / GRID_W)};
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

    int key[GRID_SIZE];
    int8_t steps[GRID_SIZE];
    int16_t parent[GRID_SIZE];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int targetIdx = gridIdx(target.x, target.y);
    if (key[targetIdx] >= kInfCost) return -1;
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

    int key[GRID_SIZE];
    int8_t steps[GRID_SIZE];
    int16_t parent[GRID_SIZE];
    riskWeightedDijkstra(state, player, budget, blockedSquare, key, steps, parent,
                        /*preferStraight=*/true);
    const int startIdx = gridIdx(player.position.x, player.position.y);
    const int targetIdx = gridIdx(target.x, target.y);
    if (key[targetIdx] >= kInfCost) return -1.0;

    // Zrekonstruuj CELOU cestu (ne jen prvni krok jako nextStepToward) --
    // parent[] jde od cile zpatky ke startu.
    int chain[GRID_SIZE];
    int n = 0;
    for (int idx = targetIdx; idx != startIdx; idx = parent[idx]) {
        if (idx < 0 || n >= GRID_SIZE) return -1.0;  // nemelo by nastat pri validnim key[]
        chain[n++] = idx;
    }

    // Stejne konstanty jako `riskWeightedDijkstra` (rerollAvailable natvrdo
    // false -- viz odduvodneni tamtez), aby se cenilo totez, co se pak
    // skutecne pojede.
    const bool rerollAvailable = false;
    const bool blizzard = (state.weather == Weather::BLIZZARD);
    const int freeSteps = movementAfterStandUp(player);
    const int gfiCap = maxGfiSquares(player);

    Position cur = player.position;
    double successProb = 1.0;
    int stepCount = 0;
    for (int i = n - 1; i >= 0; --i) {
        Position np{static_cast<int8_t>(chain[i] % GRID_W),
                    static_cast<int8_t>(chain[i] / GRID_W)};
        if (countTacklezones(state, np, player.teamSide) > 0) {
            const int dodgeTarget = calculateDodgeTarget(state, player, np, cur);
            const double pFail = (dodgeTarget - 1) / 6.0;
            successProb *= (1.0 - pFail);
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

