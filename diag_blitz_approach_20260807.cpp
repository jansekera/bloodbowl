// diag_blitz_approach_20260807.cpp
//
// FÁZE 0 z evidence/blitz_approach_detour_plan_20260807.md (item7 backlog,
// vstupní podmínka BLITZ série -- rozhodnutí uživatele 2026-08-07).
//
// Otázka: dnešní pickApproachStep má vzdálenost absolutně nadřazenou
// (`dist*100 + 20/12 * TZ`), takže krok stranou/dozadu NIKDY nevznikne.
// Kolik nás to stojí? Pro KAŽDÝ skutečně zvolený BLITZ, kde blitzer není
// hned vedle cíle, spočítáme a porovnáme tři trasy:
//
//   A  = dnešní greedy trasa (pickApproachStep), tj. co exekutor opravdu
//        udělá (action_resolver.cpp) a co odhaduje estimateApproachFailChance.
//   B1 = OPTIMÁLNÍ trasa do TÉHOŽ finálního pole jako A -- izoluje čistě
//        otázku detouru (stejný blok, jiná cesta).
//   B2 = OPTIMÁLNÍ trasa do LIBOVOLNÉHO pole sousedícího s cílem -- navíc
//        připouští lepší volbu finálního pole (fáze 1 designu).
//
// Cena trasy je celá cesta, ne jeden krok (podmínka uživatele z 30.07.):
// dodge hod za každé opuštění pole v soupeřově TZ (obtížnost přes
// calculateDodgeTarget, tj. včetně TZ cílového pole) + GFI 1/6 za každý
// krok nad rámec pohybu. Rezervujeme 1 MP na samotný blok (CRP), stejně
// jako estimateApproachFailChance. Strop 2 GFI = pravidlový limit; trasa,
// která se do něj nevejde, se počítá jako NEDORAZÍ (podmínka 2 uživatele).
//
// Build:
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_blitz_approach_20260807.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o diag_blitz_approach
// Run: ./diag_blitz_approach [repoRoot=.] [gamesPerMatchup=4] [seedBase=91000]

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/macro_mcts.h"
#include "bb/helpers.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/dice.h"
#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/roster.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <string>
#include <vector>

using namespace bb;

namespace {

constexpr int MAX_GFI = 2;          // rules cap per activation
constexpr double GFI_FAIL = 1.0 / 6.0;

struct Route {
    bool arrives = false;
    double fail = 1.0;      // P(something on the way fails)
    int steps = 0;
    int detourSteps = 0;    // steps that did NOT reduce distance to target
    Position finalSquare{-1, -1};
};

double dodgeFailChance(const GameState& s, const Player& mover, Position from,
                       Position to) {
    if (countTacklezones(s, from, mover.teamSide) == 0) return 0.0;
    int t = calculateDodgeTarget(s, mover, to, from);
    return std::clamp((t - 1) / 6.0, 0.0, 5.0 / 6.0);
}

// Route A: exactly what the executor walks (action_resolver.cpp) and what
// estimateApproachFailChance prices -- pickApproachStep, step by step.
Route routeGreedy(const GameState& s, const Player& mover, Position target) {
    Route r;
    Position cur = mover.position;
    int moveLeft = static_cast<int>(mover.movementRemaining) - 1;
    int maxSteps = std::max(0, moveLeft) + MAX_GFI;
    double survive = 1.0;
    int startDist = cur.distanceTo(target);
    (void)startDist;
    for (int guard = 0; guard < 32 && cur.distanceTo(target) > 1; ++guard) {
        Position next = pickApproachStep(s, mover, cur, target);
        if (next.x < 0) { r.fail = 1.0; r.arrives = false; return r; }
        if (r.steps >= maxSteps) { r.arrives = false; r.fail = 1.0 - survive; return r; }
        survive *= (1.0 - dodgeFailChance(s, mover, cur, next));
        if (moveLeft <= 0) survive *= (1.0 - GFI_FAIL);
        if (next.distanceTo(target) >= cur.distanceTo(target)) r.detourSteps++;
        moveLeft -= 1;
        r.steps++;
        cur = next;
    }
    r.arrives = (cur.distanceTo(target) <= 1);
    r.fail = 1.0 - survive;
    r.finalSquare = cur;
    return r;
}

// Routes B1/B2: best achievable route by total survival probability.
// Dijkstra over (square, stepsUsed) -- stepsUsed matters because GFI risk
// starts once the movement budget runs out. `fixedGoal` set = B1 (same
// final square as the greedy route), unset = B2 (any square adjacent to
// the target).
Route routeOptimal(const GameState& s, const Player& mover, Position target,
                   const Position* fixedGoal) {
    Route r;
    int moveLeft = std::max(0, static_cast<int>(mover.movementRemaining) - 1);
    int maxSteps = moveLeft + MAX_GFI;

    auto idx = [&](Position p, int st) {
        return (static_cast<int>(p.y) * 32 + static_cast<int>(p.x)) * (maxSteps + 1) + st;
    };
    std::vector<double> best(32 * 20 * (maxSteps + 1), -1.0);
    std::vector<int> prevDetour(best.size(), 0);

    struct Node { double survive; Position pos; int steps; int detour; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const {
        return a.survive < b.survive;  // max-heap
    } };
    std::priority_queue<Node, std::vector<Node>, Cmp> pq;

    Position start = mover.position;
    best[idx(start, 0)] = 1.0;
    pq.push({1.0, start, 0, 0});

    auto isGoal = [&](Position p) {
        if (fixedGoal) return p == *fixedGoal;
        return p.distanceTo(target) <= 1;
    };

    while (!pq.empty()) {
        Node n = pq.top();
        pq.pop();
        if (n.survive < best[idx(n.pos, n.steps)] - 1e-12) continue;
        if (n.steps > 0 && isGoal(n.pos)) {
            r.arrives = true;
            r.fail = 1.0 - n.survive;
            r.steps = n.steps;
            r.detourSteps = n.detour;
            r.finalSquare = n.pos;
            return r;  // first popped goal = best survival (max-heap Dijkstra)
        }
        if (n.steps >= maxSteps) continue;
        for (Position nx : n.pos.getAdjacent()) {
            if (!nx.isOnPitch()) continue;
            const Player* occ = s.getPlayerAtPosition(nx);
            if (occ && occ->id != mover.id) continue;
            double surv = n.survive * (1.0 - dodgeFailChance(s, mover, n.pos, nx));
            if (n.steps >= moveLeft) surv *= (1.0 - GFI_FAIL);
            int st = n.steps + 1;
            int det = n.detour + (nx.distanceTo(target) >= n.pos.distanceTo(target) ? 1 : 0);
            if (surv > best[idx(nx, st)] + 1e-12) {
                best[idx(nx, st)] = surv;
                prevDetour[idx(nx, st)] = det;
                pq.push({surv, nx, st, det});
            }
        }
    }
    return r;  // arrives=false
}

struct Sample {
    Route a, b1, b2;
    int blitzerId = 0, targetId = 0;
    Position from{-1, -1}, to{-1, -1};
    int startDist = 0;
    std::string matchup;
};

std::vector<Sample> g_samples;

// Copy of simulateGame's loop with a hook before every executed action.
void playGameInstrumented(const TeamRoster& home, const TeamRoster& away,
                          ActionSelector homePolicy, ActionSelector awayPolicy,
                          uint32_t seed, const std::string& matchup) {
    GameState state;
    DiceRoller dice(seed);
    constexpr int MAX_ACTIONS = 5000;
    const TeamSide openingKickingTeam = TeamSide::AWAY;
    state.half = 1;
    state.kickingTeam = openingKickingTeam;
    setupHalf(state, home, away, state.kickingTeam);
    simpleKickoff(state, dice);

    std::vector<Action> actions;
    int totalActions = 0;
    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        if (state.phase == GamePhase::TOUCHDOWN) {
            state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide;
            setupDrive(state, home, away, state.kickingTeam);
            simpleKickoff(state, dice);
            continue;
        }
        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(openingKickingTeam);
            setupHalf(state, home, away, state.kickingTeam);
            simpleKickoff(state, dice);
            continue;
        }
        actions.clear();
        getAvailableActions(state, actions);
        if (actions.empty()) {
            Action endTurn;
            endTurn.type = ActionType::END_TURN;
            executeAction(state, endTurn, dice, nullptr);
            totalActions++;
            continue;
        }
        ActionSelector& policy = (state.activeTeam == TeamSide::HOME)
                                     ? homePolicy : awayPolicy;
        Action chosen = policy(state);

        // --- the measurement hook: a BLITZ that has to walk first.
        if (chosen.type == ActionType::BLITZ && chosen.playerId > 0 &&
            chosen.targetId > 0) {
            const Player& mover = state.getPlayer(chosen.playerId);
            const Player& tgt = state.getPlayer(chosen.targetId);
            if (mover.isOnPitch() && tgt.isOnPitch() &&
                mover.position.distanceTo(tgt.position) > 1) {
                Sample sm;
                sm.matchup = matchup;
                sm.blitzerId = mover.id;
                sm.targetId = tgt.id;
                sm.from = mover.position;
                sm.to = tgt.position;
                sm.startDist = mover.position.distanceTo(tgt.position);
                sm.a = routeGreedy(state, mover, tgt.position);
                if (sm.a.finalSquare.x >= 0) {
                    sm.b1 = routeOptimal(state, mover, tgt.position, &sm.a.finalSquare);
                }
                sm.b2 = routeOptimal(state, mover, tgt.position, nullptr);
                g_samples.push_back(sm);
            }
        }

        executeAction(state, chosen, dice, nullptr);
        totalActions++;
    }
}

MCTSConfig makeConfig(const PolicyNetwork* pol) {
    // Champion fairtest config, same as the F1/grind harnesses.
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    return cfg;
}

void report(const char* title, const std::vector<Sample>& v) {
    if (v.empty()) { printf("\n=== %s: zadne vzorky ===\n", title); return; }
    int n = static_cast<int>(v.size());
    int aArrive = 0, b1Better1 = 0, b1Better5 = 0, b2Better5 = 0;
    int b2SameSquare = 0, b1Detour = 0, aFailsB2Arrives = 0;
    double sumA = 0, sumB1 = 0, sumB2 = 0, sumD1 = 0, sumD2 = 0;
    double maxGain = 0;
    for (const auto& s : v) {
        aArrive += s.a.arrives ? 1 : 0;
        sumA += s.a.fail; sumB1 += s.b1.fail; sumB2 += s.b2.fail;
        double d1 = s.a.fail - s.b1.fail;
        double d2 = s.a.fail - s.b2.fail;
        if (s.b1.arrives) { sumD1 += d1; if (d1 > 0.01) b1Better1++; if (d1 > 0.05) b1Better5++; }
        if (s.b2.arrives) { sumD2 += d2; if (d2 > 0.05) b2Better5++; }
        if (s.b1.arrives && s.b1.detourSteps > 0) b1Detour++;
        if (s.b2.arrives && s.b2.finalSquare == s.a.finalSquare) b2SameSquare++;
        if (!s.a.arrives && s.b2.arrives) aFailsB2Arrives++;
        maxGain = std::max(maxGain, std::max(d1, d2));
    }
    printf("\n=== %s: N=%d blitzu s chuzi ===\n", title, n);
    printf("  dnesni trasa A: dorazi %d/%d (%.0f%%), stredni riziko cesty %.1f%%\n",
           aArrive, n, 100.0 * aArrive / n, 100.0 * sumA / n);
    printf("  B1 (stejne finalni pole, optimalni cesta): stredni riziko %.1f%%"
           " | zlepsi >1pp: %d (%.0f%%), >5pp: %d (%.0f%%), stredni zisk %.2f pp\n",
           100.0 * sumB1 / n, b1Better1, 100.0 * b1Better1 / n,
           b1Better5, 100.0 * b1Better5 / n, 100.0 * sumD1 / n);
    printf("  B2 (i jine finalni pole):                  stredni riziko %.1f%%"
           " | zlepsi >5pp: %d (%.0f%%), stredni zisk %.2f pp\n",
           100.0 * sumB2 / n, b2Better5, 100.0 * b2Better5 / n, 100.0 * sumD2 / n);
    printf("  B1 pouziva detour (krok bez postupu): %d (%.0f%%)\n",
           b1Detour, 100.0 * b1Detour / n);
    printf("  B2 konci na JINEM finalnim poli nez A: %d (%.0f%%)\n",
           n - b2SameSquare, 100.0 * (n - b2SameSquare) / n);
    printf("  A nedorazi, ale B2 ano: %d | nejvetsi jednotlivy zisk %.1f pp\n",
           aFailsB2Arrives, 100.0 * maxGain);
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int games = (argc > 2) ? atoi(argv[2]) : 4;
    uint32_t seedBase = (argc > 3) ? static_cast<uint32_t>(atoi(argv[3])) : 91000u;
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    printf("vf=%s policy=%s | %d her/matchup, seedBase=%u\n",
           vf ? "loaded" : "NULL", pol ? "loaded" : "NULL", games, seedBase);

    struct MU { const char* h; const char* a; };
    const MU matchups[] = {{"dwarf", "woodelf"}, {"dwarf", "skaven"}, {"orc", "skaven"}};

    for (const auto& mu : matchups) {
        const TeamRoster* hr = getDevelopedRoster(mu.h, 1200);
        const TeamRoster* ar = getDevelopedRoster(mu.a, 1200);
        if (!hr || !ar) { fprintf(stderr, "roster fail\n"); return 1; }
        std::string label = std::string(mu.h) + "-" + mu.a;
        size_t before = g_samples.size();
        for (int g = 0; g < games; ++g) {
            uint32_t seed = seedBase + static_cast<uint32_t>(g) * 1013u;
            MCTSConfig cfg = makeConfig(pol.get());
            MacroMCTSPolicy homePol(vf.get(), cfg, seed * 2654435761u + 11u);
            MacroMCTSPolicy awayPol(vf.get(), cfg, seed * 2654435761u + 47u);
            playGameInstrumented(*hr, *ar,
                                 [&](const GameState& s) { return homePol(s); },
                                 [&](const GameState& s) { return awayPol(s); },
                                 seed, label);
            printf("  %s hra %d hotova (vzorku celkem %zu)\n", label.c_str(), g,
                   g_samples.size());
        }
        std::vector<Sample> sub(g_samples.begin() + before, g_samples.end());
        report(label.c_str(), sub);
    }

    report("CELKEM", g_samples);

    // Konkretni situace (uzivatel: situace, ne jen agregaty): 8 nejvetsich zisku.
    std::vector<Sample> byGain = g_samples;
    std::sort(byGain.begin(), byGain.end(), [](const Sample& x, const Sample& y) {
        double gx = std::max(x.b1.arrives ? x.a.fail - x.b1.fail : 0.0,
                             x.b2.arrives ? x.a.fail - x.b2.fail : 0.0);
        double gy = std::max(y.b1.arrives ? y.a.fail - y.b1.fail : 0.0,
                             y.b2.arrives ? y.a.fail - y.b2.fail : 0.0);
        return gx > gy;
    });
    printf("\n=== 8 situaci s nejvetsim rozdilem ===\n");
    for (int i = 0; i < 8 && i < static_cast<int>(byGain.size()); ++i) {
        const Sample& s = byGain[i];
        printf("  [%s] p%d (%d,%d) -> cil p%d (%d,%d), vzdalenost %d\n",
               s.matchup.c_str(), s.blitzerId, s.from.x, s.from.y,
               s.targetId, s.to.x, s.to.y, s.startDist);
        printf("      A: riziko %.1f%% kroku %d dorazi=%d konci (%d,%d)\n",
               100.0 * s.a.fail, s.a.steps, s.a.arrives ? 1 : 0,
               s.a.finalSquare.x, s.a.finalSquare.y);
        printf("      B1: riziko %.1f%% kroku %d detouru %d dorazi=%d\n",
               100.0 * s.b1.fail, s.b1.steps, s.b1.detourSteps, s.b1.arrives ? 1 : 0);
        printf("      B2: riziko %.1f%% kroku %d detouru %d konci (%d,%d)\n",
               100.0 * s.b2.fail, s.b2.steps, s.b2.detourSteps,
               s.b2.finalSquare.x, s.b2.finalSquare.y);
    }
    printf("\nDONE\n");
    return 0;
}
