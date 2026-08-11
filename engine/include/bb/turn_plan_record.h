#pragma once

#include <cstdint>

namespace bb {

// What the turn planner decided, and why -- recorded so replay analysis can
// stop guessing at it.
//
// Until 2026-08-11 every one of these numbers was computed and thrown away.
// cage_advance.cpp works out the schedule (requiredPace), what the corners can
// actually sustain (achievablePace), and the list of bodies standing in the
// corridor -- then reduces the lot to a single step length and forgets it. So
// when the replay corpus showed the cage advancing 2.69 squares a turn with
// NOBODY in front of it (a human team manages 4.31), there was no way to tell
// the two candidate explanations apart: is the plan running and choosing to
// crawl, or is it bailing out with TEMPO_INSUFFICIENT / DICEY and handing the
// turn to search()? Those want opposite fixes.
//
// Written once per team-turn, at the root only -- the planners are called from
// MacroMCTSPolicy::tryStagedMacro, never inside a rollout, so nothing in the
// search overwrites it. thread_local because self-play runs games in parallel.
struct TurnPlanRecord {
    bool written = false;        // planner ran this turn at all
    uint8_t goal = 0;            // TurnGoal
    uint8_t verdict = 255;       // CageAdvanceVerdict; 255 = cage planner not consulted
    bool adopted = false;        // plan was valid and its macros were staged

    // Cage-advance arithmetic (zero unless the cage planner ran)
    int16_t distToEndzone = 0;
    int16_t turnsLeft = 0;
    float requiredPace = 0.0f;
    float achievablePace = 0.0f;
    int8_t rawAchievableStep = 0;
    int8_t step = 0;
    int8_t resistance = 0;       // standing opponents in the advance corridor
    int8_t filledCorners = 0;
    int8_t openCorners = 0;
    int8_t carrierGfi = 0;
    // Tackle-zone exposure of the chosen destination (carrier weighted double),
    // i.e. the quantity the 08-11 cage fix optimises. Lets a later run show
    // whether that fix actually moved anything.
    int8_t exposure = 0;
};

// Thread-local slot for the turn currently being planned.
TurnPlanRecord& currentTurnPlanRecord();
void resetTurnPlanRecord();

} // namespace bb
