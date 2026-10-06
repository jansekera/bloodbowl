#include "bb/one_cage.h"
#include <cstdio>
#include <cstdlib>
#include "bb/helpers.h"
#include "bb/pathfinder.h"
#include "bb/turn_planner.h"
#include "bb/turn_plan_record.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <tuple>

namespace bb {

namespace {

int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - ((side == TeamSide::HOME) ? 25 : 0));
}

int ceilDiv(int a, int b) { return (a + b - 1) / b; }

bool ourBall(const GameState& state) {
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return false;
    const Player& c = state.getPlayer(state.ball.carrierId);
    return c.teamSide == state.activeTeam && c.isOnPitch();
}

bool freeToAct(const Player& p) {
    return p.isOnPitch() && p.state == PlayerState::STANDING && p.canAct() &&
           !p.hasMoved && !p.hasActed;
}

// Kolik stojících soupeřů příští tah dosáhne na pole vedle `sq` (MA + 2 GFI).
int threatsTo(const GameState& state, Position sq, TeamSide mySide) {
    int n = 0;
    state.forEachOnPitch(opponent(mySide), [&](const Player& o) {
        if (o.state != PlayerState::STANDING) return;
        if (o.position.distanceTo(sq) - 1 <= o.stats.movement + 2) ++n;
    });
    return n;
}

} // namespace

Position farthestSafeForward(const GameState& state, const Player& carrier, int budget) {
    const TeamSide side = carrier.teamSide;
    // Menší je lepší: vzdálenost k TD zóně, pak kolik soupeřů na pole dosáhne,
    // pak odklon od středu hřiště.
    auto key = [&](Position sq) {
        return std::make_tuple(distToEndzone(sq, side), threatsTo(state, sq, side),
                               std::abs(sq.y - 7));
    };
    Position best = carrier.position;
    auto bestKey = key(best);
    for (int x = carrier.position.x - budget; x <= carrier.position.x + budget; ++x) {
        for (int y = carrier.position.y - budget; y <= carrier.position.y + budget; ++y) {
            const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            if (!sq.isOnPitch() || sq == carrier.position) continue;
            if (sq.y < 1 || sq.y > 13) continue;           // rohy klece musí mít kam stát
            if (distToEndzone(sq, side) > std::get<0>(bestKey)) continue;
            if (state.getPlayerAtPosition(sq)) continue;
            if (countTacklezones(state, sq, side, carrier.id) > 0) continue;
            const auto k = key(sq);
            if (!(k < bestKey)) continue;
            // -1 = nedojde, > 0 = cesta vede přes hod (dodge nebo GFI)
            if (pathFailProb(state, carrier, sq, budget, Position{-1, -1}) != 0.0) continue;
            best = sq;
            bestKey = k;
        }
    }
    return best;
}

ReleaseDecision decideRelease(const GameState& state, const Player& carrier,
                              const CageAdvancePlanner& planner) {
    ReleaseDecision r;
    const TeamSide side = carrier.teamSide;
    r.dist = distToEndzone(carrier.position, side);
    r.turnsLeft = std::clamp(9 - state.getTeamState(side).turnNumber, 0, 8);
    const int ma = std::max(1, static_cast<int>(carrier.stats.movement));
    const int reach = ma + 2;   // odsud tah na TD dohraje MCTS (SCORE_BALL)

    for (int s = carrier.movementRemaining; s >= 1; --s) {
        if (planner.tryAssign(state, carrier, s, {}).feasible) { r.cageStep = s; break; }
    }
    const int penalty = std::min(2, (corridorResistance(state, carrier, side) + 1) / 2);
    r.futurePace = std::max(0, r.cageStep - penalty);

    // Klec: tento tah cageStep, pak futurePace, poslední tah sólo na TD.
    if (r.cageStep > 0) {
        const int rest = r.dist - r.cageStep - reach;
        int need = 1;
        if (rest > 0) need = (r.futurePace > 0) ? ceilDiv(rest, r.futurePace) + 1 : INT_MAX;
        r.cageMakesIt = need <= r.turnsLeft - 1;
    }
    // Sólo od tohoto tahu: každý tah MA, poslední tah na TD.
    const int soloRest = r.dist - reach;
    const int soloNeed = (soloRest > 0 ? ceilDiv(soloRest, ma) : 0) + 1;
    r.soloMakesIt = soloNeed <= r.turnsLeft;

    r.release = !r.cageMakesIt && r.soloMakesIt;
    return r;
}

CageController::CageController(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : planner_(vf, config, seed), config_(config) {}

bool CageController::released(const GameState& state, const Player& carrier) const {
    const int score = state.homeTeam.score + state.awayTeam.score;
    return carrier.id == releasedCarrier_ && state.half == releasedHalf_ &&
           score == releasedScore_;
}

bool CageController::stillValid(const GameState& state, const Macro& m) const {
    if (m.type == MacroType::PICKUP) return stagedMacroStillValid(state, m, false);
    if (!ourBall(state)) return false;    // míč pryč ⇒ zbytek plánu nemá smysl
    if (m.type == MacroType::REPOSITION) return stagedMacroStillValid(state, m, true);
    if (m.type == MacroType::BLOCK) {
        const Player& a = state.getPlayer(m.playerId);
        const Player& d = state.getPlayer(m.targetId);
        return freeToAct(a) && d.isOnPitch() && d.state == PlayerState::STANDING &&
               a.position.distanceTo(d.position) == 1;
    }
    return false;
}

void CageController::planStart(const GameState& state) {
    stage_ = Stage::DONE;
    const TurnGoal goal = classifyTurnGoal(state);

    if (goal == TurnGoal::PICKUP_BALL) {
        // Fáze 1: zvedá nejlepší z kandidátů, které nabízí i MCTS — napřed
        // ten, kdo k míči dojde bez hodu, pak Sure Hands, pak obratnost.
        std::vector<Macro> macros;
        getAvailableMacros(state, macros, config_.dauntlessInOffer);
        const Macro* best = nullptr;
        double bestFail = 2.0;
        auto handler = [&](const Player& p) {
            return (p.hasSkill(SkillName::SureHands) ? 100 : 0) + p.stats.agility;
        };
        for (const Macro& m : macros) {
            if (m.type != MacroType::PICKUP) continue;
            const Player& p = state.getPlayer(m.playerId);
            double fail = pathFailProb(state, p, m.targetPos,
                                       p.movementRemaining + maxGfiSquares(p), Position{-1, -1});
            if (fail < 0.0) fail = 1.0;
            if (!best || fail < bestFail - 1e-9 ||
                (std::abs(fail - bestFail) <= 1e-9 &&
                 handler(p) > handler(state.getPlayer(best->playerId)))) {
                best = &m;
                bestFail = fail;
            }
        }
        if (!best) return;
        Macro pick = *best;
        pick.cageManaged = true;   // expandPickup: po zvednutí co nejdál dopředu
        queue_ = {pick};
        phase_ = CagePhase::PICKUP;
        stage_ = Stage::AFTER_PICKUP;
        return;
    }

    // P150 (06.10.2026): stopa rozhodnutí řadiče pod BB_CAGE_DEBUG — proč tah klece není / je.
    const bool dbg = std::getenv("BB_CAGE_DEBUG") != nullptr;
    if (goal != TurnGoal::ADVANCE_BALL) {
        if (dbg) std::fprintf(stderr, "[cage ctl] bez plánu: cíl tahu %d není ADVANCE_BALL\n", static_cast<int>(goal));
        return;
    }
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!freeToAct(carrier)) {
        if (dbg) std::fprintf(stderr, "[cage ctl] bez plánu: nosič %d už hrál nebo nestojí\n", carrier.id);
        return;
    }
    if (dbg) {
        const ReleaseDecision r = decideRelease(state, carrier, planner_);
        std::fprintf(stderr, "[cage ctl] nosič %d (%d,%d): do TD %d, tahů %d, krok klece %d, další tempo %d, "
                     "klec doběhne %d, sólo doběhne %d, už vypuštěn %d => %s\n",
                     carrier.id, carrier.position.x, carrier.position.y, r.dist, r.turnsLeft, r.cageStep,
                     r.futurePace, r.cageMakesIt, r.soloMakesIt, released(state, carrier),
                     (released(state, carrier) || r.release) ? "VÝBĚH (fáze 3)" : "KLEC (fáze 2)");
    }

    if (!released(state, carrier) && !decideRelease(state, carrier, planner_).release) {
        // Fáze 2. Soupeř na poli rohu ⇒ napřed ho shodit (uživatel 02.10.).
        phase_ = CagePhase::CAGE;
        std::vector<Macro> macros;
        getAvailableMacros(state, macros, config_.dauntlessInOffer);
        std::vector<int> usedAttackers;
        for (const Position& d : carrier.position.getAdjacent()) {
            if (!d.isOnPitch() || std::abs(d.x - carrier.position.x) != 1 ||
                std::abs(d.y - carrier.position.y) != 1) continue;
            const Player* opp = state.getPlayerAtPosition(d);
            if (!opp || opp->teamSide == carrier.teamSide ||
                opp->state != PlayerState::STANDING) continue;
            const Macro* pickBlock = nullptr;
            for (const Macro& m : macros) {
                if (m.type != MacroType::BLOCK || m.targetId != opp->id) continue;
                if (m.playerId == carrier.id) continue;   // nosič nebojuje
                if (std::find(usedAttackers.begin(), usedAttackers.end(), m.playerId) !=
                    usedAttackers.end()) continue;
                const Player& a = state.getPlayer(m.playerId);
                if (!pickBlock) { pickBlock = &m; continue; }
                const Player& b = state.getPlayer(pickBlock->playerId);
                if (a.hasSkill(SkillName::Block) != b.hasSkill(SkillName::Block)
                        ? a.hasSkill(SkillName::Block)
                        : a.stats.strength > b.stats.strength) {
                    pickBlock = &m;
                }
            }
            if (pickBlock) {
                queue_.push_back(*pickBlock);
                usedAttackers.push_back(pickBlock->playerId);
            }
        }
        stage_ = Stage::AFTER_BLOCKS;
        return;
    }

    // Fáze 3: nosič co nejdál sám; markeři až podle místa, kam doopravdy došel.
    phase_ = CagePhase::RELEASE;
    releasedCarrier_ = carrier.id;
    releasedHalf_ = state.half;
    releasedScore_ = state.homeTeam.score + state.awayTeam.score;
    const Position dest = farthestSafeForward(state, carrier, carrier.movementRemaining);
    if (dbg) std::fprintf(stderr, "[cage ctl] výběh: nosič (%d,%d) -> (%d,%d)\n", carrier.position.x,
                          carrier.position.y, dest.x, dest.y);
    if (dest != carrier.position) {
        Macro run{MacroType::REPOSITION, carrier.id, -1, dest};
        run.cageManaged = true;
        queue_.push_back(run);
    }
    stage_ = Stage::AFTER_RUN;
}

void CageController::planAfterPickup(const GameState& state) {
    stage_ = Stage::DONE;
    if (!ourBall(state)) return;
    CageAdvancePlan plan = planner_.buildFillOnly(state, {});
    if (plan.valid) queue_ = std::move(plan.macros);
}

void CageController::planAdvance(const GameState& state) {
    stage_ = Stage::DONE;
    if (!ourBall(state)) return;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!freeToAct(carrier)) return;
    CageAdvancePlan plan = planner_.build(state);
    if (std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] plán postupu: verdikt %d, platný %d, krok %d, rohy stojí %d, po tahu %d, maker %zu\n",
                     static_cast<int>(plan.verdict), plan.valid, plan.step, plan.builtCorners,
                     plan.filledCorners, plan.macros.size());
    }
    if (plan.valid) queue_ = std::move(plan.macros);
}

void CageController::planMarkers(const GameState& state) {
    stage_ = Stage::DONE;
    if (!ourBall(state)) return;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    const TeamSide side = carrier.teamSide;
    const Position cp = carrier.position;

    // Soupeři, kteří příští tah dojdou k nosiči a ještě nikým markovaní nejsou.
    std::vector<const Player*> threats;
    state.forEachOnPitch(opponent(side), [&](const Player& o) {
        if (o.state != PlayerState::STANDING) return;
        if (countTacklezones(state, o.position, o.teamSide) > 0) return;  // už markovaný
        if (o.position.distanceTo(cp) - 1 > o.stats.movement + 2) return;
        threats.push_back(&state.getPlayer(o.id));
    });
    std::sort(threats.begin(), threats.end(), [&](const Player* a, const Player* b) {
        return a->position.distanceTo(cp) < b->position.distanceTo(cp);
    });

    std::vector<int> usedMarkers;
    std::vector<Position> usedSquares;
    for (const Player* o : threats) {
        int bestId = -1;
        Position bestSq{-1, -1};
        int bestSteps = INT_MAX;
        state.forEachOnPitch(side, [&](const Player& m) {
            if (m.id == carrier.id || !freeToAct(m)) return;
            if (std::find(usedMarkers.begin(), usedMarkers.end(), m.id) != usedMarkers.end()) return;
            for (const Position& sq : o->position.getAdjacent()) {
                if (!sq.isOnPitch() || state.getPlayerAtPosition(sq)) continue;
                if (std::find(usedSquares.begin(), usedSquares.end(), sq) != usedSquares.end()) continue;
                if (pathFailProb(state, m, sq, m.movementRemaining, Position{-1, -1}) != 0.0) continue;
                const int steps = pathStepsToward(state, m, sq, m.movementRemaining, Position{-1, -1});
                if (steps >= 0 && steps < bestSteps) {
                    bestSteps = steps;
                    bestId = m.id;
                    bestSq = sq;
                }
            }
        });
        if (bestId < 0) continue;
        Macro mark{MacroType::REPOSITION, bestId, -1, bestSq};
        mark.cageManaged = true;   // markování je bez hodu, žádné GFI navrch
        queue_.push_back(mark);
        usedMarkers.push_back(bestId);
        usedSquares.push_back(bestSq);
    }
}

bool CageController::next(const GameState& state, Macro& out) {
    if (state.phase != GamePhase::PLAY) return false;
    const int turn = state.getTeamState(state.activeTeam).turnNumber;
    if (team_ != state.activeTeam || turn_ != turn || half_ != state.half) {
        team_ = state.activeTeam;
        turn_ = turn;
        half_ = state.half;
        stage_ = Stage::START;
        phase_ = CagePhase::NONE;
        queue_.clear();
        idx_ = 0;
    }

    for (int guard = 0; guard < 6; ++guard) {
        if (idx_ < queue_.size()) {
            const Macro& m = queue_[idx_];
            if (!stillValid(state, m)) {   // plán se rozešel s deskou ⇒ zbytek tahu MCTS
                queue_.clear();
                idx_ = 0;
                stage_ = Stage::DONE;
                return false;
            }
            out = m;
            ++idx_;
            return true;
        }
        queue_.clear();
        idx_ = 0;
        switch (stage_) {
            case Stage::START:        planStart(state); break;
            case Stage::AFTER_PICKUP: planAfterPickup(state); break;
            case Stage::AFTER_BLOCKS: planAdvance(state); break;
            case Stage::AFTER_RUN:    planMarkers(state); break;
            case Stage::DONE:         return false;
        }
        if (!queue_.empty()) {
            ++adopted_;
            TurnPlanRecord& rec = currentTurnPlanRecord();
            rec.written = true;
            rec.adopted = true;
        }
    }
    return false;
}

} // namespace bb
