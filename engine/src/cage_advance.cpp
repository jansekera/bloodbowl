#include "bb/cage_advance.h"
#include "bb/turn_planner.h"
#include "bb/helpers.h"
#include "bb/turn_plan_record.h"
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

static bool isReservedId(int id, const std::vector<int>& reserved) {
    return std::find(reserved.begin(), reserved.end(), id) != reserved.end();
}

int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - endzoneX(side));
}

// How much of the cage would end the turn inside an opponent's tackle zone if
// the carrier stepped `step` squares forward. A marked corner is not merely a
// weaker corner: the opponent blocks it out and the cage opens, which is why
// the standing rule (user, since 2026-08-04 "release the marked corners"; and
// bbtactics "Cage Basics": none of the five may end the turn in a tackle zone)
// treats it as no corner at all.
//
// The carrier counts double. Measured 2026-08-11 on the replay corpus: he ends
// marked in 40% of our advance turns against 11% for the occupied corners, so
// he is the bigger hole by a wide margin -- which was the opposite of what the
// code review predicted.
int cageExposure(const GameState& state, const Player& carrier, int step) {
    const int dx = forwardDx(carrier.teamSide);
    Position dest{static_cast<int8_t>(carrier.position.x + dx * step),
                  carrier.position.y};
    if (!dest.isOnPitch()) return 99;
    int e = 2 * countTacklezones(state, dest, carrier.teamSide);
    for (int sx : {dx, -dx}) {
        for (int sy : {-1, 1}) {
            Position slot{static_cast<int8_t>(dest.x + sx),
                          static_cast<int8_t>(dest.y + sy)};
            if (slot.isOnPitch()) {
                e += countTacklezones(state, slot, carrier.teamSide);
            }
        }
    }
    return e;
}

} // anonymous namespace

// See the header for why this is not a planner member any more (K9b, 08-18).
int corridorResistance(const GameState& state, const Player& carrier,
                       TeamSide mySide) {
    const int dx = forwardDx(mySide);
    int n = 0;
    state.forEachOnPitch(opponent(mySide), [&](const Player& p) {
        if (p.state != PlayerState::STANDING) return;
        int ahead = (p.position.x - carrier.position.x) * dx;
        if (ahead < 1 || ahead > CageAdvancePlanner::CORRIDOR_DEPTH) return;
        if (std::abs(p.position.y - carrier.position.y) >
            CageAdvancePlanner::CORRIDOR_HALF_WIDTH) return;
        ++n;
    });
    return n;
}

int corridorStrength(const GameState& state, const Player& carrier,
                     TeamSide mySide) {
    const int dx = forwardDx(mySide);
    int st = 0;
    state.forEachOnPitch(opponent(mySide), [&](const Player& p) {
        if (p.state != PlayerState::STANDING) return;
        int ahead = (p.position.x - carrier.position.x) * dx;
        if (ahead < 1 || ahead > CageAdvancePlanner::CORRIDOR_DEPTH) return;
        if (std::abs(p.position.y - carrier.position.y) >
            CageAdvancePlanner::CORRIDOR_HALF_WIDTH) return;
        st += p.stats.strength;
    });
    return st;
}

TempoSnapshot tempoSnapshot(const GameState& state, const Player& carrier,
                            TeamSide mySide) {
    TempoSnapshot t;
    const int ezX = (mySide == TeamSide::HOME) ? 25 : 0;
    const int dist = std::abs(carrier.position.x - ezX);
    const TeamState& my = state.getTeamState(mySide);
    // Táž definice jako v plánovači (ř. 464): turnsLeft = 9 - turnNumber.
    const int turnsLeft = std::clamp(9 - static_cast<int>(my.turnNumber), 0,
                                     CageAdvancePlanner::MAX_HALF_TURNS);
    const int usable = std::max(1, turnsLeft - CageAdvancePlanner::RESERVE_TURNS);

    t.distToEndzone = static_cast<int8_t>(std::min(dist, 127));
    t.turnsLeft = static_cast<int8_t>(turnsLeft);
    t.required = static_cast<float>(dist) / static_cast<float>(usable);

    const int resistance = corridorResistance(state, carrier, mySide);
    const int penalty = std::min(2, (resistance + 1) / 2);
    t.achievable = static_cast<float>(
        static_cast<int>(carrier.movementRemaining) +
        CageAdvancePlanner::CARRIER_GFI_MAX - penalty);
    return t;
}

CageSnapshot cageSnapshot(const GameState& state, const Player& carrier,
                          TeamSide mySide) {
    CageSnapshot c;
    const int dx = forwardDx(mySide);
    c.corners = c.cornersMarked = 0;
    c.orthoOccupied = c.orthoOurs = 0;
    c.aheadOccupied = c.aheadOurs = 0;

    // Rohy: 4 diagonály. Roh drží jen NÁŠ STOJÍCÍ hráč -- ležící tělo klec
    // nekryje a soupeřovo tělo tam vůbec nemá co dělat.
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            Position slot{static_cast<int8_t>(carrier.position.x + sx),
                          static_cast<int8_t>(carrier.position.y + sy)};
            if (!slot.isOnPitch()) continue;
            const Player* p = state.getPlayerAtPosition(slot);
            if (!p || p->teamSide != mySide ||
                p->state != PlayerState::STANDING) continue;
            ++c.corners;
            if (countTacklezones(state, slot, mySide) > 0) ++c.cornersMarked;
        }
    }

    // Ortogonály MAJÍ být prázdné (11.08.). Tady se počítá KDOKOLI, protože
    // munici pro chain push dělá tělo, ne jeho dres -- a zvlášť se vede,
    // kolik z toho jsme my, protože to je ta vada, kterou 26.08. našel M11.
    for (auto d : {std::pair<int, int>{dx, 0}, {-dx, 0}, {0, -1}, {0, 1}}) {
        Position slot{static_cast<int8_t>(carrier.position.x + d.first),
                      static_cast<int8_t>(carrier.position.y + d.second)};
        if (!slot.isOnPitch()) continue;
        const Player* p = state.getPlayerAtPosition(slot);
        if (!p) continue;
        ++c.orthoOccupied;
        const bool ours = (p->teamSide == mySide);
        if (ours) ++c.orthoOurs;
        if (d.first == dx && d.second == 0) {
            c.aheadOccupied = 1;
            c.aheadOurs = ours ? 1 : 0;
        }
    }

    c.carrierTz = static_cast<int8_t>(
        countTacklezones(state, carrier.position, mySide));
    return c;
}

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

    // Carrier leg: `step` squares straight forward. Beyond MA the carrier
    // may take up to CARRIER_GFI_MAX real GFI rolls (tempo emergency, user
    // doctrine 2026-08-04) -- the caller decides whether the schedule
    // actually needs them and prices the risk in the probe stage.
    // step 0 = the carrier stays put and only the corners re-form around
    // him. That is the cage-FILL case (user, 2026-08-05: "fallback to search
    // is unacceptable -- the carrier running out of the cage on his own is a
    // fine fallback", ironically; the mandated minimum is posun -> doplnit ->
    // never a solo run).
    if (step < 0 || step > static_cast<int>(carrier.movementRemaining)
                              + CARRIER_GFI_MAX) {
        return res;
    }
    Position newPos{static_cast<int8_t>(carrier.position.x + dx * step),
                    carrier.position.y};
    if (!newPos.isOnPitch()) return res;
    if (newPos.y < 1 || newPos.y > 13) return res;  // corner rows must exist
    // The carrier's target may hold a TEAMMATE: real formations right after
    // a pickup are scrum piles, and the untangling doctrine (user,
    // 2026-08-04) is "the ones in FRONT move first so they stop blocking".
    // The blocker is drafted into a corner slot below and vacates before the
    // carrier walks (dependency-ordered execution). An opponent on the
    // square stays a hard fail -- clearing bodies is BLITZ/search work.
    const Player* carrierBlocker = nullptr;
    if (const Player* occ = (step == 0) ? nullptr
                                        : state.getPlayerAtPosition(newPos)) {
        if (occ->teamSide != mySide) return res;
        if (!occ->canAct() || occ->hasMoved || occ->hasActed) return res;
        if (!eligibleCornerPlayer(*occ) || isReservedId(occ->id, reservedPlayerIds)) {
            return res;
        }
        carrierBlocker = occ;
    }
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
            // Corner substitution (user 2026-08-04): a candidate standing in
            // an enemy tackle zone must DODGE out (dwarf AG2: ~50% fail) --
            // the probe then vetoes the whole plan. Prefer a FREE body for
            // the new corner and leave the engaged one standing where it
            // binds defenders. Block/blitz corner-release is the later,
            // complex layer (queued with the blitz-priority discussion).
            bool aFree = countTacklezones(state, a->position, mySide, a->id) == 0;
            bool bFree = countTacklezones(state, b->position, mySide, b->id) == 0;
            if (aFree != bFree) return aFree;
            // Tempo sustainability (user design input 2026-08-03, wired
            // 2026-08-04): a corner slower than the planned step caps the
            // whole cage's pace NEXT turn -- prefer corners whose MA
            // sustains the step, above mere closeness (slow-strong pieces
            // stay great corners for a STATIC cage, but they throttle a
            // rolling one).
            bool aKeeps = static_cast<int>(a->stats.movement) >= step;
            bool bKeeps = static_cast<int>(b->stats.movement) >= step;
            if (aKeeps != bKeeps) return aKeeps;
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

    // The carrier's target square holds a teammate who was NOT drafted into
    // any slot above: give him the nearest still-empty slot so he vacates
    // with a purpose (untangling doctrine). No slot for him -> no plan at
    // this step.
    if (carrierBlocker && !isAssigned(carrierBlocker->id)) {
        SlotAssignment* dest = nullptr;
        int bestD = 1000;
        for (auto& sa : res.slots) {
            if (sa.playerId >= 0 || !sa.slot.isOnPitch()) continue;
            if (state.getPlayerAtPosition(sa.slot) != nullptr) continue;
            int d = carrierBlocker->position.distanceTo(sa.slot);
            bool gfi = false;
            if (d > static_cast<int>(carrierBlocker->movementRemaining)) {
                if (d == static_cast<int>(carrierBlocker->movementRemaining) + 1 &&
                    gfiBudget > 0) {
                    gfi = true;
                } else {
                    continue;
                }
            }
            if (d < bestD) { bestD = d; dest = &sa; dest->needsGfi = gfi; }
        }
        if (!dest) return res;  // feasible stays false
        dest->playerId = carrierBlocker->id;
        if (dest->needsGfi) { gfiBudget--; res.gfi++; }
        res.filled++;
        res.open--;
        assignedIds.push_back(carrierBlocker->id);
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


// Mandated minimum when the advance will not run (user, 2026-08-05: "fallback
// to search is unacceptable" -- the hierarchy is advance -> fill -> never a
// solo run, and "we cannot let the dwarves throw away the attempt at a TD in
// turn 1"). Measured 2026-08-11 with the gate forced on: the advance declines
// in 85% of ADVANCE turns (TEMPO_INSUFFICIENT 48%, DICEY 37%) and every one of
// those turns fell through to search(), which averages 1.73 squares against
// the plan's 5.00. Filling the cage where the carrier stands is strictly
// better than handing the turn over with the cage half-built.
//
// Deliberately dice-free and carrier-free: no GFI, no carrier move, so this
// can never itself cause the turnover it exists to prevent.
CageAdvancePlan CageAdvancePlanner::buildFillOnly(
        const GameState& state, const std::vector<int>& reservedPlayerIds) {
    CageAdvancePlan plan;
    if (state.phase != GamePhase::PLAY) return plan;
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return plan;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (carrier.teamSide != state.activeTeam || !carrier.isOnPitch()) return plan;
    if (carrier.state != PlayerState::STANDING) return plan;

    // A slot that would need a GFI is left open rather than killing the whole
    // fill -- the first cut rejected the plan outright whenever any single
    // corner needed a rush. Feasibility is still required: filling one corner
    // out of four is not a cage, and a lone body next to the carrier is the
    // formation problem the ADVANCE path already reports as NOT_APPLICABLE.
    AssignmentResult a = tryAssign(state, carrier, 0, reservedPlayerIds);
    if (!a.feasible) return plan;

    std::vector<Macro> macros;
    for (const auto& sa : a.slots) {
        if (sa.playerId < 0 || sa.stayPut || sa.needsGfi) continue;
        macros.push_back({MacroType::REPOSITION, sa.playerId, -1, sa.slot});
    }
    if (macros.empty()) return plan;   // cage already whole, or nobody reaches

    plan.step = 0;
    plan.carrierGfi = 0;
    plan.filledCorners = a.filled;
    plan.openCorners = a.open;
    plan.macros = std::move(macros);
    plan.verdict = CageAdvanceVerdict::FILL_ONLY;
    plan.valid = true;
    return plan;
}

// Thin wrapper: run the planner, then record what it decided. Every number
// below used to be computed and discarded, which made "the cage crawls with
// nobody in front of it" undiagnosable -- there was no telling a plan that
// runs and chooses to crawl from one that bails out with TEMPO_INSUFFICIENT
// and hands the turn to search(). Those two want opposite fixes.
CageAdvancePlan CageAdvancePlanner::build(const GameState& state,
                                          const std::vector<int>& reservedPlayerIds) {
    CageAdvancePlan plan = buildImpl(state, reservedPlayerIds);
    if (!plan.valid) {
        CageAdvancePlan fill = buildFillOnly(state, reservedPlayerIds);
        if (fill.valid) {
            fill.requiredPace = plan.requiredPace;
            fill.achievablePace = plan.achievablePace;
            fill.rawAchievableStep = plan.rawAchievableStep;
            fill.resistance = plan.resistance;
            plan = std::move(fill);
        }
    }
    TurnPlanRecord& r = currentTurnPlanRecord();
    r.written = true;
    r.verdict = static_cast<uint8_t>(plan.verdict);
    r.requiredPace = static_cast<float>(plan.requiredPace);
    r.achievablePace = static_cast<float>(plan.achievablePace);
    r.rawAchievableStep = static_cast<int8_t>(plan.rawAchievableStep);
    r.step = static_cast<int8_t>(plan.step);
    r.resistance = static_cast<int8_t>(plan.resistance);
    r.filledCorners = static_cast<int8_t>(plan.filledCorners);
    r.openCorners = static_cast<int8_t>(plan.openCorners);
    r.carrierGfi = static_cast<int8_t>(plan.carrierGfi);
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        if (c.teamSide == state.activeTeam && c.isOnPitch()) {
            r.distToEndzone = static_cast<int16_t>(distToEndzone(c.position, c.teamSide));
            r.turnsLeft = static_cast<int16_t>(
                std::max(0, 9 - state.getTeamState(c.teamSide).turnNumber));
            r.exposure = static_cast<int8_t>(std::min(
                127, cageExposure(state, c, plan.step > 0 ? plan.step : 1)));
        }
    }
    return plan;
}

CageAdvancePlan CageAdvancePlanner::buildImpl(const GameState& state,
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
    // No minimum on ALREADY-built corners (user standard 2026-08-04: "build
    // a proper cage, always"): the cage is built AT THE CARRIER'S TARGET
    // square from whoever can reach the slots. builtCorners stays a
    // diagnostic; tryAssign's feasibility (>= TRIGGER_MIN_CORNERS slots
    // FILLED after the move, never degrading a standing cage) is the gate.

    // --- Tempo: computed, never a constant (constraint 1). Same
    // turnsLeft = 9 - turnNumber schedule simulate()'s idealDist pacing uses.
    const TeamState& my = state.getTeamState(mySide);
    int dist = distToEndzone(carrier.position, mySide);
    int turnsLeft = std::clamp(9 - my.turnNumber, 0, MAX_HALF_TURNS);
    // DIAG/EXPERIMENT (2026-08-06 tempo doctrine A/B, config_.cageGrind,
    // DEFAULT OFF -- no behavior change in production): option (a) "grind".
    // When the schedule cannot be met, push the cage at the max DICE-FREE
    // achievable step anyway instead of returning TEMPO_INSUFFICIENT.
    // Grind never spends carrier GFI: the schedule is unmeetable, so dice
    // risk buys nothing (banking never buys dice risk -- doctrine above).
    //
    // 2026-08-11: the "walk as far as you safely can when the schedule cannot
    // be met" branch is no longer config-gated. The user's hierarchy is
    // advance -> fill -> never a solo run, and his stated preference is "move
    // more squares forward when it is possible". Bailing out of the advance
    // put the turn back in search()'s hands, which manages 1.73 squares
    // against this planner's 5.00; now that a cage-FILL fallback exists
    // underneath, the branch below is the middle tier of that hierarchy
    // rather than an alternative to giving up.
    //
    // The 08-10 A/B that measured cageGrind at zero compared it against
    // fallback-to-search with no fill underneath, so it does not settle this
    // arrangement -- it still owes its own A/B.
    const bool grind = true;
    int usable = turnsLeft - RESERVE_TURNS;
    if (usable < 1) {
        if (!grind) {
            plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
            return plan;
        }
        usable = 1;  // grind: keep the pace math defined; tempoFail below
    }
    plan.requiredPace = static_cast<double>(dist) / usable;

    // Opponent resistance in the advance corridor (constraint 1): each
    // screen body ahead costs an extra arc/block, i.e. pace. Hoisted out of
    // the planner on 08-18 (K9b) so it exists even when the gate does not run
    // -- ONE definition, used both here and by the per-turn snapshot.
    plan.resistance = corridorResistance(state, carrier, mySide);
    int penalty = std::min(2, (plan.resistance + 1) / 2);

    // Role-achievable raw step: the largest step the actual corner
    // assignment (incl. reformation reach and the GFI rules) sustains. The
    // ceiling comes from the CARRIER'S REAL MA (+ the GFI emergency reach),
    // never a constant -- corner sustainability emerges from tryAssign's
    // per-slot reach checks (user constraint 2026-08-03/04).
    AssignmentResult assign;
    int maxNoGfi = static_cast<int>(carrier.movementRemaining);
    for (int step = maxNoGfi + CARRIER_GFI_MAX; step >= 1; --step) {
        AssignmentResult a = tryAssign(state, carrier, step, reservedPlayerIds);
        if (a.feasible) {
            assign = std::move(a);
            plan.rawAchievableStep = step;
            break;
        }
    }
    plan.achievablePace = plan.rawAchievableStep - penalty;
    if (plan.rawAchievableStep < 1) {
        // No step has a feasible cage at the destination (not enough bodies
        // in reach) -- a formation problem, not a schedule one.
        plan.verdict = CageAdvanceVerdict::NOT_APPLICABLE;
        return plan;
    }
    const bool tempoFail = (plan.achievablePace < 1.0 ||
                            plan.achievablePace + 1e-9 < plan.requiredPace);
    if (tempoFail && !grind) {
        plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
        return plan;
    }

    // Final step -- "bank while the corridor is clear" (user doctrine
    // 2026-08-04, applies to ALL bash-style drives, not just dwarfs):
    // resistance arrives MID-drive almost always, so a clear corridor is
    // walked at MAX dice-free pace to build schedule cushion; end-of-drive
    // overshoot is the existing stall logic's job. With opponents already
    // in the corridor the plan reverts to schedule pace (grind). Carrier
    // GFI squares are spent ONLY when the schedule cannot be met within
    // plain MA (tempo emergency) -- banking never buys dice risk.
    int finalStep;
    int scheduleStep = 1;   // hoisted: the exposure pass below needs the
                            // shortest step that still meets the schedule
    if (tempoFail) {
        // Grind branch (config_.cageGrind only): max dice-free step, no GFI.
        plan.grindMode = true;
        plan.carrierGfi = 0;
        finalStep = std::min(plan.rawAchievableStep, maxNoGfi);
        if (finalStep < 1) {
            plan.verdict = CageAdvanceVerdict::TEMPO_INSUFFICIENT;
            return plan;
        }
    } else {
        scheduleStep = std::clamp(static_cast<int>(std::ceil(plan.requiredPace - 1e-9)),
                                  1, static_cast<int>(plan.achievablePace));
        int bankStep = std::min(plan.rawAchievableStep, maxNoGfi);
        finalStep = (plan.resistance == 0) ? std::max(scheduleStep, bankStep)
                                           : scheduleStep;
        plan.carrierGfi = std::clamp(finalStep - maxNoGfi, 0, CARRIER_GFI_MAX);
    }

    // Pick the least exposed destination among the steps that cost us nothing.
    // Until now the planner had no notion of tackle zones at all -- corner
    // slots were purely geometric -- so the cage routinely parked itself
    // inside them.
    //
    // The bounds are the whole design, because tempo is the one thing we must
    // not trade away: we score in only 18-35% of matches, so reaching the
    // endzone at all is the binding constraint.
    //   upper: never add a GFI, never exceed what the corners can sustain;
    //   lower: never drop below the schedule -- but banking IS negotiable,
    //          since a banked square is a bonus and an unmarked carrier is
    //          not. In grind mode the schedule is already unmeetable, so
    //          shortening is off the table entirely.
    {
        const int loStep = tempoFail ? finalStep : std::min(scheduleStep, finalStep);
        const int hiStep = (plan.carrierGfi > 0)
                               ? finalStep
                               : std::min(plan.rawAchievableStep, maxNoGfi);
        int bestStep = finalStep;
        int bestExposure = cageExposure(state, carrier, finalStep);
        for (int s = hiStep; s >= loStep; --s) {   // ties keep the longer step
            if (s == finalStep) continue;
            const int e = cageExposure(state, carrier, s);
            if (e >= bestExposure) continue;
            if (!tryAssign(state, carrier, s, reservedPlayerIds).feasible) continue;
            bestStep = s;
            bestExposure = e;
        }
        finalStep = bestStep;
    }

    if (finalStep != plan.rawAchievableStep) {
        AssignmentResult a = tryAssign(state, carrier, finalStep, reservedPlayerIds);
        if (!a.feasible && plan.grindMode) {
            // grind: walk down to any feasible dice-free step
            for (int s = finalStep - 1; s >= 1 && !a.feasible; --s) {
                AssignmentResult a2 = tryAssign(state, carrier, s, reservedPlayerIds);
                if (a2.feasible) { a = std::move(a2); finalStep = s; }
            }
        }
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

    // --- Macros. Base order: front movers, back movers, carrier last
    // (risk-last -- the screen forms before the carrier commits, and a GFI
    // carrier leg stays at the very end). Execution order is then
    // SITUATIONAL (user doctrine 2026-08-04): whoever stands on another
    // mover's target square goes first, so pile-ups untangle front-first
    // instead of deadlocking the walk.
    std::vector<Macro> macros;
    std::vector<bool> macroGfi;
    for (const auto& sa : assign.slots) {
        if (sa.playerId < 0 || sa.stayPut) continue;
        macros.push_back({MacroType::REPOSITION, sa.playerId, -1, sa.slot});
        macroGfi.push_back(sa.needsGfi);
    }
    if (plan.step > 0) {   // cage-fill (step 0) never moves the carrier
        Macro cm{MacroType::REPOSITION, carrier.id, -1, assign.newCarrierPos};
        cm.gfiAllowance = plan.carrierGfi;
        macros.push_back(cm);
        macroGfi.push_back(false);
    }
    // Dependency sort (stable): repeatedly pick the first not-yet-placed
    // macro whose target square is not the CURRENT position of another
    // unplaced mover. A cycle (mutual swaps) falls back to base order.
    {
        std::vector<Macro> ordered;
        std::vector<bool> orderedGfi;
        std::vector<size_t> left(macros.size());
        for (size_t i = 0; i < left.size(); ++i) left[i] = i;
        while (!left.empty()) {
            size_t pickAt = 0;
            bool found = false;
            for (size_t li = 0; li < left.size() && !found; ++li) {
                const Macro& cand = macros[left[li]];
                bool blocked = false;
                for (size_t lj = 0; lj < left.size(); ++lj) {
                    if (lj == li) continue;
                    const Player& other = state.getPlayer(macros[left[lj]].playerId);
                    if (other.position == cand.targetPos) { blocked = true; break; }
                }
                if (!blocked) { pickAt = li; found = true; }
            }
            if (!found) pickAt = 0;  // cycle: fall back to base order
            ordered.push_back(macros[left[pickAt]]);
            orderedGfi.push_back(macroGfi[left[pickAt]]);
            left.erase(left.begin() + pickAt);
        }
        macros = std::move(ordered);
        macroGfi = std::move(orderedGfi);
    }

    // Probe each macro on the EVOLVING projection (item13 pattern) -- step
    // k's safety only means anything given steps 1..k-1 -- then execute it
    // there to advance the occupancy picture.
    GameState projected = state.clone();
    for (size_t i = 0; i < macros.size(); ++i) {
        const Macro& m = macros[i];
        // The carrier's GFI leg is an ACCEPTED dice risk (tempo emergency):
        // it gets the relaxed ceiling, everything else stays dice-free.
        double ceiling = SAFE_PTO;
        if (m.gfiAllowance == 1) ceiling = SAFE_PTO_GFI1;
        else if (m.gfiAllowance >= 2) ceiling = SAFE_PTO_GFI2;
        auto pr = probeMacro(projected, m);
        if (pr.pto > ceiling || pr.meanActions < 0.5) {
            if (getenv("BB_CAGE_DEBUG")) {
                fprintf(stderr, "[cage DICEY] leg %zu/%zu player=%d gfi=%d "
                        "pto=%.3f ceil=%.3f meanActs=%.2f target=(%d,%d)\n",
                        i, macros.size(), m.playerId, m.gfiAllowance,
                        pr.pto, ceiling, pr.meanActions,
                        m.targetPos.x, m.targetPos.y);
            }
            plan.verdict = CageAdvanceVerdict::DICEY;
            plan.diceyLegIdx = static_cast<int>(i);
            plan.diceyPto = pr.pto;
            plan.diceyCeil = ceiling;
            plan.diceyMeanActs = pr.meanActions;
            plan.diagMacros = macros;
            plan.diagMacroCornerGfi.assign(macroGfi.begin(), macroGfi.end());
            return plan;
        }
        // Execute on the projection. The macro is probed within its risk
        // ceiling, but the expansion still rolls real dice -- retry before
        // giving up on the plan (GFI legs fail a real fraction of attempts,
        // so they get more retries; the RISK is priced above, the retries
        // just need one clean sample to keep projecting).
        bool ok = false;
        int attempts = m.gfiAllowance > 0 ? 8 : 3;
        for (int attempt = 0; attempt < attempts && !ok; ++attempt) {
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
            if (getenv("BB_CAGE_DEBUG")) {
                GameState dbg = projected.clone();
                auto r = greedyExpandMacro(dbg, m, dice_);
                const Player& moved = dbg.getPlayer(m.playerId);
                fprintf(stderr, "[cage EXEC-FAIL] leg %zu/%zu player=%d gfi=%d "
                        "target=(%d,%d) endpos=(%d,%d) to=%d phase=%d acts=%zu\n",
                        i, macros.size(), m.playerId, m.gfiAllowance,
                        m.targetPos.x, m.targetPos.y, moved.position.x,
                        moved.position.y, (int)r.turnover, (int)dbg.phase,
                        r.actions.size());
            }
            plan.verdict = CageAdvanceVerdict::DICEY;
            plan.diceyLegIdx = static_cast<int>(i);
            plan.diceyPto = pr.pto;
            plan.diceyCeil = ceiling;
            plan.diceyMeanActs = pr.meanActions;
            plan.diceyExecFail = true;
            plan.diagMacros = macros;
            plan.diagMacroCornerGfi.assign(macroGfi.begin(), macroGfi.end());
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
