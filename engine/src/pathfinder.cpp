#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include <cstring>
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
    constexpr int INF = 1 << 28;
    // ⛔⛔ GFI NENI ZADARMO (nalez 01.09., druhe mereni). Prvni verze te ceny
    //   brala pole navic jako bezplatne -- jenze pole ZA HRANICI MA je Go For
    //   It a pada v sestine pripadu. Chuze proto uhnula tacklezone a misto
    //   dodge hodila GFI: turnovery pri dobehu 2 953 -> 3 107, tedy RIZIKO SE
    //   PRESUNULO, ne zmizelo. Cena kroku musi znat OBOJI.
    //   Pomer: dodge na 4+ pada ~1/3, GFI na 2+ pada 1/6 => tacklezona je
    //   zhruba dvakrat drazsi nez GFI. Odtud 2 a 1, ne z ladeni.
    constexpr int kTzCost  = 2;     // krok do tacklezony
    constexpr int kGfiCost = 1;     // krok za hranici MA (Go For It)
    const int freeSteps = movementAfterStandUp(player);   // bez GFI
    int key[GRID_SIZE];             // cena (pole + riziko)
    int8_t steps[GRID_SIZE];        // ciste pole -- na tohle se vaze rozpocet
    int16_t parent[GRID_SIZE];
    bool done[GRID_SIZE];
    for (int i = 0; i < GRID_SIZE; ++i) {
        key[i] = INF; parent[i] = -1; done[i] = false; steps[i] = 127;
    }

    const int startIdx = gridIdx(player.position.x, player.position.y);
    key[startIdx] = 0;
    steps[startIdx] = 0;

    for (;;) {
        int cur = -1, best = INF;
        for (int i = 0; i < GRID_SIZE; ++i)
            if (!done[i] && key[i] < best) { best = key[i]; cur = i; }
        if (cur < 0) break;
        done[cur] = true;
        Position curPos{static_cast<int8_t>(cur % GRID_W),
                        static_cast<int8_t>(cur / GRID_W)};
        if (steps[cur] >= budget) continue;

        for (auto& np : curPos.getAdjacent()) {
            if (!np.isOnPitch()) continue;
            if (np == target) continue;                        // cil se neprochazi
            if (state.getPlayerAtPosition(np) != nullptr) continue;
            const int nIdx = gridIdx(np.x, np.y);
            if (done[nIdx]) continue;
            const int tz = countTacklezones(state, np, player.teamSide);
            const int nStep = steps[cur] + 1;
            const int nk = key[cur] + 1 + (tz > 0 ? kTzCost : 0)
                         + (nStep > freeSteps ? kGfiCost : 0);
            if (nk < key[nIdx]) {
                key[nIdx] = nk;
                steps[nIdx] = static_cast<int8_t>(nStep);
                parent[nIdx] = static_cast<int16_t>(cur);
            }
        }
    }

    // Cilove pole: sousedi s `target` a v rozpoctu zbyva pole na BLOK
    // (r. 549-550, "the block costs one square of movement").
    int bestIdx = -1, bestKey = INF;
    for (int i = 0; i < GRID_SIZE; ++i) {
        if (key[i] >= INF || i == startIdx) continue;
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

int getValidMoveTargets(const GameState& state, const Player& player,
                        MoveTarget* out, int maxOut) {
    if (!canAct(player.state) || player.lostTacklezones) return 0;

    int count = 0;
    bool inTZ = countTacklezones(state, player.position, player.teamSide) > 0;

    // N11 (24.08.2026): Take Root, l. 8577-8579 -- zakorenený "may not Go For
    // It, be pushed back for any reason, or use any skill that would allow him
    // to move out of his current square". Zakaz zil jen v nabidce MOVE a v
    // blitz-bloku; pathfinder o nem nevedel, takze zakorenenemu Treemanovi se
    // nabidl BLITZ na nesousedni cil a smycka ho pres GFI opravdu posunula.
    int maxGfi = player.rooted ? 0 : (player.hasSkill(SkillName::Sprint) ? 3 : 2);
    bool canGfi = player.movementRemaining <= 0 && player.movementRemaining > -maxGfi;

    auto adj = player.position.getAdjacent();
    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        if (state.getPlayerAtPosition(pos) != nullptr) continue;

        // Check if player has movement remaining (including GFI)
        int movLeft = player.movementRemaining - 1;
        if (movLeft < -maxGfi) continue;

        bool isGfi = (movLeft < 0);

        if (count < maxOut) {
            out[count++] = {pos, inTZ, isGfi};
        }
    }

    return count;
}

} // namespace bb
