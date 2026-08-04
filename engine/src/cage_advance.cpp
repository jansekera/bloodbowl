#include "bb/cage_advance.h"
#include "bb/turn_planner.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

int endzoneX(TeamSide side) {
    return (side == TeamSide::HOME) ? 25 : 0;
}

int forwardDx(TeamSide side) {
    return (side == TeamSide::HOME) ? 1 : -1;
}

int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - endzoneX(side));
}

} // anonymous namespace

bool CageAdvancePlanner::eligibleCornerPlayer(const Player& p) {
    // Activation-reliability nega-traits: the corner job is a formation
    // commitment -- a corner that fails its activation roll (or roots) is a
    // hole in the cage exactly when the carrier stands behind it.
    if (p.hasSkill(SkillName::BoneHead)) return false;
    if (p.hasSkill(SkillName::ReallyStupid)) return false;
    if (p.hasSkill(SkillName::WildAnimal)) return false;
    if (p.hasSkill(SkillName::TakeRoot)) return false;
    // Deathroller-type: ejected when the drive ends -- never a structural
    // corner, whatever its other stats (generic over the skill, not the
    // roster name; NoHands alone would NOT disqualify a corner).
    if (p.hasSkill(SkillName::SecretWeapon)) return false;
    // Cannot hold a position deliberately at all.
    if (p.hasSkill(SkillName::BallAndChain)) return false;
    return true;
}

CageAdvancePlanner::CageAdvancePlanner(const ValueFunction* vf, MCTSConfig config,
                                       uint32_t seed)
    : config_(config), evaler_(vf, config, seed), dice_(seed + 8686) {}

CageAdvancePlanner::ProbeStats CageAdvancePlanner::probeMacro(const GameState& state,
                                                              const Macro& m) {
    ProbeStats r;
    int nTO = 0;
    double acts = 0.0;
    for (int k = 0; k < PROBE_K; ++k) {
        GameState sim = state.clone();
        auto res = greedyExpandMacro(sim, m, dice_);
        if (res.turnover) nTO++;
        acts += static_cast<double>(res.actions.size());
    }
    r.pto = static_cast<double>(nTO) / PROBE_K;
    r.meanActions = acts / PROBE_K;
    return r;
}

CageAdvancePlanner::AssignmentResult CageAdvancePlanner::tryAssign(
        const GameState& state, const Player& carrier, int step,
        const std::vector<int>& reservedPlayerIds) const {
    AssignmentResult res;
    TeamSide mySide = carrier.teamSide;
    int dx = forwardDx(mySide);

    // Carrier leg: `step` squares straight forward, never a GFI for the
    // ball carrier.
    if (step < 1 || step > static_cast<int>(carrier.movementRemaining)) return res;
    Position newPos{static_cast<int8_t>(carrier.position.x + dx * step),
                    carrier.position.y};
    if (!newPos.isOnPitch()) return res;
    if (newPos.y < 1 || newPos.y > 13) return res;  // corner rows must exist
    if (state.getPlayerAtPosition(newPos) != nullptr) return res;
    res.newCarrierPos = newPos;

    // Slots: front diagonal pair first (the screen the advance is for),
    // then the back pair.
    const Position slots[4] = {
        {static_cast<int8_t>(newPos.x + dx), static_cast<int8_t>(newPos.y - 1)},
        {static_cast<int8_t>(newPos.x + dx), static_cast<int8_t>(newPos.y + 1)},
        {static_cast<int8_t>(newPos.x - dx), static_cast<int8_t>(newPos.y - 1)},
        {static_cast<int8_t>(newPos.x - dx), static_cast<int8_t>(newPos.y + 1)},
    };

    auto isReserved = [&](int id) {
        return std::find(reservedPlayerIds.begin(), reservedPlayerIds.end(), id)
               != reservedPlayerIds.end();
    };

    // Candidate pool: free-to-act, skill-eligible, unreserved teammates.
    std::vector<const Player*> pool;
    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (p.id == carrier.id) return;
        if (!p.canAct() || p.hasMoved || p.hasActed) return;
        if (!eligibleCornerPlayer(p)) return;
        if (isReserved(p.id)) return;
        pool.push_back(&p);
    });

    std::vector<int> assignedIds;
    auto isAssigned = [&](int id) {
        return std::find(assignedIds.begin(), assignedIds.end(), id)
               != assignedIds.end();
    };

    int gfiBudget = 1;  // constraint 2: at most ONE corner on the allowance
    for (const Position& slot : slots) {
        SlotAssignment sa;
        sa.slot = slot;
        if (!slot.isOnPitch()) {
            res.open++;
            res.slots.push_back(sa);
            continue;
        }

        const Player* occ = state.getPlayerAtPosition(slot);
        if (occ && occ->teamSide != mySide) {
            // An opponent stands on the slot: open corner (clearing him is
            // the search/BLITZ fallback's job, not this dice-free plan's).
            res.open++;
            res.slots.push_back(sa);
            continue;
        }
        if (occ && occ->id == carrier.id) {  // cannot happen geometrically
            res.open++;
            res.slots.push_back(sa);
            continue;
        }
        if (occ && !isAssigned(occ->id)) {
            // A teammate already stands here and is not moving elsewhere:
            // the corner exists by staying put -- no macro, no dice. This
            // covers both pool members (consumed below so they are not
            // drafted away) and ineligible/spent teammates (a body on the
            // slot is a corner regardless of who it is).
            sa.playerId = occ->id;
            sa.stayPut = true;
            res.filled++;
            assignedIds.push_back(occ->id);
            pool.erase(std::remove(pool.begin(), pool.end(), occ), pool.end());
            res.slots.push_back(sa);
            continue;
        }
        // Slot is empty (or being vacated by an already-assigned mover):
        // draft the best remaining candidate. Reliability outranks skills
        // (constraint 2): no-GFI arrivals first, then distance, THEN
        // Guard/StandFirm as tiebreaks.
        const Player* best = nullptr;
        bool bestGfi = false;
        auto better = [&](const Player* a, bool aGfi,
                          const Player* b, bool bGfi) {
            if (aGfi != bGfi) return !aGfi;
            int da = a->position.distanceTo(slot);
            int db = b->position.distanceTo(slot);
            if (da != db) return da < db;
            // Straighter route first (Manhattan tiebreak): at equal Chebyshev
            // reach, a same-wing candidate walks an unobstructed lane while a
            // cross-cage candidate must thread between the carrier and the
            // standing corners -- observed to stop one square short and void
            // the whole plan. Reliability outranks skills (constraint 2), so
            // this sits ABOVE the Guard/StandFirm preference.
            int ma = std::abs(a->position.x - slot.x) + std::abs(a->position.y - slot.y);
            int mb = std::abs(b->position.x - slot.x) + std::abs(b->position.y - slot.y);
            if (ma != mb) return ma < mb;
            bool ga = a->hasSkill(SkillName::Guard);
            bool gb = b->hasSkill(SkillName::Guard);
            if (ga != gb) return ga;
            bool fa = a->hasSkill(SkillName::StandFirm);
            bool fb = b->hasSkill(SkillName::StandFirm);
            if (fa != fb) return fa;
            return a->id < b->id;
        };
        for (const Player* p : pool) {
            int d = p->position.distanceTo(slot);
            bool gfi;
            if (d <= static_cast<int>(p->movementRemaining)) {
                gfi = false;
            } else if (d == static_cast<int>(p->movementRemaining) + 1 &&
                       gfiBudget > 0) {
                gfi = true;  // the single 1-GFI allowance (see header)
            } else {
                continue;  // out of reach -> cannot serve this slot
            }
            if (!best || better(p, gfi, best, bestGfi)) {
                best = p;
                bestGfi = gfi;
            }
        }
        if (!best) {
            res.open++;
            res.slots.push_back(sa);
            continue;
        }
        sa.playerId = best->id;
        sa.needsGfi = bestGfi;
        if (bestGfi) {
            gfiBudget--;
            res.gfi++;
        }
        res.filled++;
        assignedIds.push_back(best->id);
        pool.erase(std::remove(pool.begin(), pool.end(), best), pool.end());
        res.slots.push_back(sa);
    }

    // Feasibility: never degrade the standing cage, and keep at least a
    // 2-corner screen; a 3+-corner cage must stay 3+ after the move.
    int built = 0;
    for (auto& d : carrier.position.getAdjacent()) {
        if (!d.isOnPitch()) continue;
        if (std::abs(d.x - carrier.position.x) != 1 ||
            std::abs(d.y - carrier.position.y) != 1) continue;  // diagonals only
        const Player* p = state.getPlayerAtPosition(d);
        if (p && p->teamSide == mySide && p->state == PlayerState::STANDING) built++;
    }
    int minFilled = std::min(built, 3);
    res.feasible = (res.filled >= TRIGGER_MIN_CORNERS && res.filled >= minFilled);
    return res;
}

CageAdvancePlan CageAdvancePlanner::build(const GameState& state,
                                          const std::vector<int>& reservedPlayerIds) {
    CageAdvancePlan plan;

    // --- Trigger: our held ball, ADVANCE goal, movable carrier, cage built.
    if (classifyTurnGoal(state) != TurnGoal::ADVANCE_BALL) return plan;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!carrier.canAct() || carrier.hasMoved || carrier.hasActed) return plan;

    TeamSide mySide = carrier.teamSide;
    int dx = forwardDx(mySide);
    for (auto& d : carrier.position.getAdjacent()) {
        if (!d.isOnPitch()) continue;
        if (std::abs(d.x - carrier.position.x) != 1 ||
            std::abs(d.y - carrier.position.y) != 1) continue;
        const Player* p = state.getPlayerAtPosition(d);
        if (p && p->teamSide == mySide && p->state == PlayerState::STANDING)
            plan.builtCorners++;
    }
    if (plan.builtCorners < TRIGGER_MIN_CORNERS) return plan;

    // --- Tempo: computed, never a constant (constraint 1). Same
    // turnsLeft = 9 - turnNumber schedule simulate()'s idealDist pacing uses.
    const TeamState& my = state.getTeamState(mySide);
    int dist = distToEndzone(carrier.position, mySide);
    int turnsLeft = std::clamp(9 - my.turnNumber, 0, MAX_HALF_TURNS);
    int usable = turnsLeft - RESERVE_TURNS;
    if (usable < 1) {
        plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
        return plan;
    }
    plan.requiredPace = static_cast<double>(dist) / usable;

    // Opponent resistance in the advance corridor (constraint 1): each
    // screen body ahead costs an extra arc/block, i.e. pace.
    state.forEachOnPitch(opponent(mySide), [&](const Player& p) {
        if (p.state != PlayerState::STANDING) return;
        int ahead = (p.position.x - carrier.position.x) * dx;
        if (ahead < 1 || ahead > CORRIDOR_DEPTH) return;
        if (std::abs(p.position.y - carrier.position.y) > CORRIDOR_HALF_WIDTH) return;
        plan.resistance++;
    });
    int penalty = std::min(2, (plan.resistance + 1) / 2);

    // Role-achievable raw step: the largest step the actual corner
    // assignment (incl. reformation reach and the GFI rules) sustains.
    AssignmentResult assign;
    for (int step = MAX_STEP; step >= 1; --step) {
        AssignmentResult a = tryAssign(state, carrier, step, reservedPlayerIds);
        if (a.feasible) {
            assign = std::move(a);
            plan.rawAchievableStep = step;
            break;
        }
    }
    plan.achievablePace = plan.rawAchievableStep - penalty;
    if (plan.rawAchievableStep < 1 || plan.achievablePace < 1.0 ||
        plan.achievablePace + 1e-9 < plan.requiredPace) {
        plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
        return plan;
    }

    // Final step: meet the schedule, never outrun it (grind doctrine keeps
    // the reserve; stalling deeper is the existing stall logic's job).
    int finalStep = std::clamp(static_cast<int>(std::ceil(plan.requiredPace - 1e-9)),
                               1, static_cast<int>(plan.achievablePace));
    if (finalStep != plan.rawAchievableStep) {
        AssignmentResult a = tryAssign(state, carrier, finalStep, reservedPlayerIds);
        if (!a.feasible) {
            plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
            return plan;
        }
        assign = std::move(a);
    }
    plan.step = finalStep;
    plan.filledCorners = assign.filled;
    plan.openCorners = assign.open;
    plan.gfiCorners = assign.gfi;

    // --- Macros: front movers, back movers, carrier LAST. Probe each on the
    // EVOLVING projection (item13 pattern) -- step k's safety only means
    // anything given steps 1..k-1 -- then execute it there to advance the
    // occupancy picture.
    std::vector<Macro> macros;
    std::vector<bool> macroGfi;
    for (const auto& sa : assign.slots) {
        if (sa.playerId < 0 || sa.stayPut) continue;
        macros.push_back({MacroType::REPOSITION, sa.playerId, -1, sa.slot});
        macroGfi.push_back(sa.needsGfi);
    }
    macros.push_back({MacroType::REPOSITION, carrier.id, -1, assign.newCarrierPos});
    macroGfi.push_back(false);

    GameState projected = state.clone();
    for (size_t i = 0; i < macros.size(); ++i) {
        const Macro& m = macros[i];
        auto pr = probeMacro(projected, m);
        if (pr.pto > SAFE_PTO || pr.meanActions < 0.5) {
            plan.verdict = CageAdvanceVerdict::DICEY;
            return plan;
        }
        // Execute on the projection. The macro is probed dice-free, but the
        // expansion still rolls real dice for any tail this cheap model
        // missed -- retry a couple of times before giving up on the plan.
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
            GameState next = projected.clone();
            auto r = greedyExpandMacro(next, m, dice_);
            if (r.turnover || next.phase != GamePhase::PLAY ||
                next.activeTeam != state.activeTeam) {
                continue;
            }
            const Player& moved = next.getPlayer(m.playerId);
            int miss = moved.position.distanceTo(m.targetPos);
            // Movers must ARRIVE; the single 1-GFI corner walks dice-free
            // and may stop one square short (closes next turn -- header).
            int allowed = macroGfi[i] ? 1 : 0;
            if (miss > allowed) continue;
            projected = std::move(next);
            ok = true;
        }
        if (!ok) {
            plan.verdict = CageAdvanceVerdict::DICEY;
            return plan;
        }
    }

    plan.planValue = evaler_.evaluateLeaf(projected, mySide);
    plan.macros = std::move(macros);
    plan.verdict = CageAdvanceVerdict::PLAN_READY;
    plan.valid = true;
    return plan;
}

} // namespace bb
