// diag_macro_floor_audit_harness.cpp  (2026-07-16)
//
// READ-ONLY diagnostic: systematic audit of MCTS prior-floor asymmetry across
// ALL macro types (third-instance hunt after the CAGE 2026-07-03 and ADVANCE
// 2026-07-16 floor fixes). Modeled on diag_advance_vs_block_harness.cpp.
//
// Reads turn-start snapshots (dumped from diag_perplayer_grounding_data/
// main_postfix/ by diag_macro_floor_audit_20260716.py) on stdin and runs the
// REAL engine code (linked against engine/build/libbb_engine.so):
//
//   counts             per root: candidate count + post-renorm prior mass per
//                      macro type (real getAvailableMacros + real expand())
//   screen <K>         per root (offense/loose ctx): per-candidate one-ply
//                      MC Q (greedyExpandMacro + real simulate(), K samples)
//   deepdive <g> <t> <seeds> <K>
//                      one snapshot: root priors, one-ply Q (K=3000-grade),
//                      full production MacroMCTSSearch::search() over many
//                      seeds -> chosen% + visit share (ADVANCE-diag format)
//
// Reconstruction assumptions (same as the ADVANCE harness, see its evidence
// file): fresh-turn flags, movementRemaining=MA, rerolls=2 both sides,
// weather NICE, both turnNumbers = snapshot turn, kickingTeam=AWAY (unused).
//
// NOTE: `#define private public` before macro_mcts.h exposes expand()/
// simulate() for direct invocation -- compile-time only trick, calls land in
// the compiled .so. No engine source modified.
//
// Build (repo root):
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_macro_floor_audit_harness.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o <scratch>/diag_macro_floor_audit

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include "bb/policy_network.h"
#include "bb/policies.h"
#include "bb/dice.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define private public
#include "bb/macro_mcts.h"
#undef private

using namespace bb;

static const char* macroName(MacroType t) {
    switch (t) {
        case MacroType::SCORE: return "SCORE";
        case MacroType::ADVANCE: return "ADVANCE";
        case MacroType::CAGE: return "CAGE";
        case MacroType::BLITZ: return "BLITZ";
        case MacroType::BLOCK: return "BLOCK";
        case MacroType::PICKUP: return "PICKUP";
        case MacroType::PASS_ACTION: return "PASS_ACTION";
        case MacroType::FOUL: return "FOUL";
        case MacroType::REPOSITION: return "REPOSITION";
        case MacroType::END_TURN: return "END_TURN";
        case MacroType::BLITZ_AND_SCORE: return "BLITZ_AND_SCORE";
        case MacroType::HAND_OFF_SCORE: return "HAND_OFF_SCORE";
        case MacroType::PASS_SCORE: return "PASS_SCORE";
        case MacroType::CHAIN_SCORE: return "CHAIN_SCORE";
        default: return "?";
    }
}

static std::string macroLabel(const Macro& m) {
    char buf[96];
    if (m.type == MacroType::REPOSITION)
        snprintf(buf, sizeof buf, "%s p%d->(%d,%d)", macroName(m.type), m.playerId,
                 (int)m.targetPos.x, (int)m.targetPos.y);
    else
        snprintf(buf, sizeof buf, "%s p%d t%d", macroName(m.type), m.playerId, m.targetId);
    return buf;
}

// ---------------------------------------------------------------------------
// Snapshot stream parsing.
// Format (whitespace separated):
//   G <home_race> <away_race>
//   T <gidx> <tidx> <half> <turn> <active 0=home 1=away> <hs> <as>
//     <held 0|1> <carrier> <bx> <by> <np> {<id> <x> <y> <st>} * np
// ---------------------------------------------------------------------------
struct SnapPlayer { int id, x, y, st; };
struct Snapshot {
    std::string homeRace, awayRace;
    int gidx, tidx, half, turn, active, hs, as_, held, carrier, bx, by;
    std::vector<SnapPlayer> players;
};

static bool readSnapshots(std::vector<Snapshot>& out) {
    char tag[8];
    std::string hr, ar;
    char buf[64];
    while (scanf("%7s", tag) == 1) {
        if (!strcmp(tag, "G")) {
            if (scanf("%63s", buf) != 1) return false;
            hr = buf;
            if (scanf("%63s", buf) != 1) return false;
            ar = buf;
        } else if (!strcmp(tag, "T")) {
            Snapshot s;
            s.homeRace = hr; s.awayRace = ar;
            int np;
            if (scanf("%d %d %d %d %d %d %d %d %d %d %d %d",
                      &s.gidx, &s.tidx, &s.half, &s.turn, &s.active,
                      &s.hs, &s.as_, &s.held, &s.carrier, &s.bx, &s.by, &np) != 12)
                return false;
            for (int i = 0; i < np; ++i) {
                SnapPlayer p;
                if (scanf("%d %d %d %d", &p.id, &p.x, &p.y, &p.st) != 4) return false;
                s.players.push_back(p);
            }
            out.push_back(std::move(s));
        } else {
            return false;
        }
    }
    return true;
}

static bool makeState(const Snapshot& sn, GameState& s) {
    const TeamRoster* hr = getDevelopedRoster(sn.homeRace.c_str(), 1200);
    const TeamRoster* ar = getDevelopedRoster(sn.awayRace.c_str(), 1200);
    if (!hr || !ar) return false;
    setupHalf(s, *hr, *ar, TeamSide::AWAY);  // stats/skills per id; positions overridden

    s.half = sn.half;
    s.phase = GamePhase::PLAY;
    s.activeTeam = (sn.active == 0) ? TeamSide::HOME : TeamSide::AWAY;
    s.weather = Weather::NICE;
    s.kickingTeam = TeamSide::AWAY;   // not used by the macro search path
    s.turnoverPending = false;
    s.currentActivationId = -1;

    s.homeTeam.score = sn.hs; s.awayTeam.score = sn.as_;
    s.homeTeam.turnNumber = sn.turn; s.awayTeam.turnNumber = sn.turn;
    s.homeTeam.rerolls = 2; s.awayTeam.rerolls = 2;
    s.homeTeam.rerollUsedThisTurn = s.awayTeam.rerollUsedThisTurn = false;
    s.homeTeam.blitzUsedThisTurn = s.awayTeam.blitzUsedThisTurn = false;
    s.homeTeam.passUsedThisTurn = s.awayTeam.passUsedThisTurn = false;
    s.homeTeam.foulUsedThisTurn = s.awayTeam.foulUsedThisTurn = false;

    for (auto& p : s.players) {
        p.state = PlayerState::OFF_PITCH;
        p.position = {0, 0};
        p.hasMoved = p.hasActed = p.usedBlitz = false;
        p.lostTacklezones = p.proUsedThisTurn = false;
    }
    for (const auto& pl : sn.players) {
        if (pl.id < 1 || pl.id > 22) return false;
        if (pl.x < 0 || pl.x > 25 || pl.y < 0 || pl.y > 14) return false;
        Player& p = s.getPlayer(pl.id);
        p.state = (pl.st == 0) ? PlayerState::STANDING
                : (pl.st == 1) ? PlayerState::PRONE
                : (pl.st == 2) ? PlayerState::STUNNED : PlayerState::OFF_PITCH;
        if (p.state == PlayerState::OFF_PITCH) continue;
        p.position = {(int8_t)pl.x, (int8_t)pl.y};
        p.movementRemaining = p.stats.movement;
    }

    if (sn.held) {
        if (sn.carrier < 1 || sn.carrier > 22) return false;
        const Player& c = s.getPlayer(sn.carrier);
        if (!c.isOnPitch() || c.state != PlayerState::STANDING) return false;
        s.ball.position = c.position;
        s.ball.isHeld = true;
        s.ball.carrierId = sn.carrier;
    } else {
        if (sn.bx < 0 || sn.bx > 25 || sn.by < 0 || sn.by > 14) return false;
        s.ball.position = {(int8_t)sn.bx, (int8_t)sn.by};
        s.ball.isHeld = false;
        s.ball.carrierId = -1;
    }
    // sanity: active side must have at least one standing player
    bool anyStanding = false;
    s.forEachOnPitch(s.activeTeam, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) anyStanding = true;
    });
    return anyStanding;
}

static const char* rootCtx(const GameState& s) {
    if (!s.ball.isHeld && s.ball.isOnPitch()) return "loose";
    if (!s.ball.isHeld) return "none";
    const Player& c = s.getPlayer(s.ball.carrierId);
    return (c.teamSide == s.activeTeam) ? "off" : "def";
}

static MCTSConfig makeConfig(const PolicyNetwork* pol) {
    // == replay production config (see ADVANCE harness / bb_module.cpp)
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.dirichletWeight = 0.25f;
    cfg.vfBlend = 0.0f;
    cfg.nRollouts = 1;
    cfg.leafLookahead = false;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    return cfg;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "mode required: counts|screen|deepdive\n"); return 1; }
    std::string mode = argv[1];

    auto vf = loadValueFunction("weights_best.json");
    auto pol = loadPolicyNetworkFromFile("weights_policy.json");
    if (!vf || !pol) { fprintf(stderr, "weights load failed\n"); return 1; }
    MCTSConfig cfg = makeConfig(pol.get());
    MacroMCTSSearch search(vf.get(), cfg, 12345);

    std::vector<Snapshot> snaps;
    if (!readSnapshots(snaps)) { fprintf(stderr, "snapshot parse error\n"); return 1; }
    fprintf(stderr, "parsed %zu snapshots\n", snaps.size());

    // ------------------------------------------------------------- counts --
    if (mode == "counts") {
        int skipped = 0;
        for (const auto& sn : snaps) {
            GameState s;
            if (!makeState(sn, s)) { skipped++; continue; }
            MacroMCTSNode root;
            root.visits = 1;
            search.expand(&root, s);
            // per-type: count, prior sum, prior max
            std::map<int, std::pair<int, std::pair<float, float>>> agg;
            for (auto& c : root.children) {
                auto& e = agg[(int)c.macro.type];
                e.first++;
                e.second.first += c.prior;
                e.second.second = std::max(e.second.second, c.prior);
            }
            printf("ROOT g=%d t=%d ctx=%s n=%zu", sn.gidx, sn.tidx, rootCtx(s),
                   root.children.size());
            for (auto& [ty, e] : agg)
                printf(" %s:%d:%.4f:%.4f", macroName((MacroType)ty), e.first,
                       e.second.first, e.second.second);
            printf("\n");
        }
        fprintf(stderr, "counts done, skipped=%d\n", skipped);
        return 0;
    }

    // ------------------------------------------------------------- screen --
    if (mode == "screen") {
        int K = (argc > 2) ? atoi(argv[2]) : 150;
        int skipped = 0, done = 0;
        for (const auto& sn : snaps) {
            GameState s;
            if (!makeState(sn, s)) { skipped++; continue; }
            const char* ctx = rootCtx(s);
            // defctx variant (2026-07-17): screen ONLY ctx=def (off/loose were
            // covered by today's original runs; everything else identical).
            if (strcmp(ctx, "def") != 0) continue;
            MacroMCTSNode root;
            root.visits = 1;
            search.expand(&root, s);
            printf("SROOT g=%d t=%d ctx=%s n=%zu\n", sn.gidx, sn.tidx, ctx,
                   root.children.size());
            for (auto& c : root.children) {
                DiceRoller d(777);
                double sum = 0; int nTO = 0;
                for (int k = 0; k < K; ++k) {
                    GameState sim = s.clone();
                    auto res = greedyExpandMacro(sim, c.macro, d);
                    sum += search.simulate(sim, s.activeTeam);
                    if (res.turnover) nTO++;
                }
                printf("Q type=%s prior=%.4f q=%.4f pto=%.3f label=%s\n",
                       macroName(c.macro.type), c.prior, sum / K,
                       (double)nTO / K, macroLabel(c.macro).c_str());
            }
            if (++done % 100 == 0) fprintf(stderr, "screened %d roots\n", done);
        }
        fprintf(stderr, "screen done: %d roots, skipped=%d\n", done, skipped);
        return 0;
    }

    // ----------------------------------------------------------- deepdive --
    if (mode == "deepdive") {
        if (argc < 4) { fprintf(stderr, "deepdive <gidx> <tidx> [seeds] [K]\n"); return 1; }
        int gidx = atoi(argv[2]), tidx = atoi(argv[3]);
        int nSeeds = (argc > 4) ? atoi(argv[4]) : 400;
        int K = (argc > 5) ? atoi(argv[5]) : 3000;
        const Snapshot* target = nullptr;
        for (const auto& sn : snaps)
            if (sn.gidx == gidx && sn.tidx == tidx) { target = &sn; break; }
        if (!target) { fprintf(stderr, "snapshot g=%d t=%d not found\n", gidx, tidx); return 1; }
        GameState s;
        if (!makeState(*target, s)) { fprintf(stderr, "reconstruction failed\n"); return 1; }

        printf("=== DEEPDIVE g=%d t=%d %s(H) vs %s(A) half=%d turn=%d active=%s "
               "score %d-%d ctx=%s ===\n",
               gidx, tidx, target->homeRace.c_str(), target->awayRace.c_str(),
               target->half, target->turn,
               s.activeTeam == TeamSide::HOME ? "home" : "away",
               target->hs, target->as_, rootCtx(s));

        MacroMCTSNode root;
        root.visits = 1;
        search.expand(&root, s);
        printf("\n--- root macros (real expand(), n=%zu) ---\n", root.children.size());
        for (auto& c : root.children)
            printf("  prior=%.4f  %s\n", c.prior, macroLabel(c.macro).c_str());

        printf("\n--- one-ply MC Q (greedyExpandMacro + real simulate(), K=%d) ---\n", K);
        printf("%-30s %8s %8s %10s %10s\n", "macro", "meanQ", "p(TO)", "Q|noTO", "Q|TO");
        for (auto& c : root.children) {
            DiceRoller d(777);
            double sum = 0, sumTO = 0, sumOK = 0;
            int nTO = 0;
            for (int k = 0; k < K; ++k) {
                GameState sim = s.clone();
                auto res = greedyExpandMacro(sim, c.macro, d);
                double v = search.simulate(sim, s.activeTeam);
                sum += v;
                if (res.turnover) { nTO++; sumTO += v; } else sumOK += v;
            }
            printf("%-30s %+8.4f %8.3f %+10.4f %+10.4f\n",
                   macroLabel(c.macro).c_str(), sum / K, (double)nTO / K,
                   nTO < K ? sumOK / (K - nTO) : 0.0, nTO ? sumTO / nTO : 0.0);
        }

        printf("\n--- full production search x%d seeds ---\n", nSeeds);
        std::map<std::string, int> chosen;
        std::map<std::string, double> visitShare;
        for (int i = 0; i < nSeeds; ++i) {
            MacroMCTSSearch s2(vf.get(), cfg, 1000 + i);
            Macro best = s2.search(s);
            chosen[macroLabel(best)]++;
            int tot = 0;
            for (auto& cv : s2.lastChildVisits()) tot += cv.visits;
            for (auto& cv : s2.lastChildVisits())
                visitShare[macroLabel(cv.macro)] += tot ? (double)cv.visits / tot : 0;
        }
        std::vector<std::pair<int, std::string>> byCount;
        for (auto& [k, v] : chosen) byCount.push_back({v, k});
        std::sort(byCount.rbegin(), byCount.rend());
        for (auto& [v, k] : byCount)
            printf("  chosen %5.1f%%  avg-visit-share %5.1f%%  %s\n",
                   100.0 * v / nSeeds, 100.0 * visitShare[k] / nSeeds, k.c_str());
        // also visit share of never-chosen candidates
        for (auto& [k, vs] : visitShare)
            if (!chosen.count(k))
                printf("  chosen   0.0%%  avg-visit-share %5.1f%%  %s\n",
                       100.0 * vs / nSeeds, k.c_str());
        return 0;
    }

    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 1;
}
