#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include <cstring>

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
