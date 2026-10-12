#include "bb/coach_choice.h"

#include <stdexcept>
#include <string>

namespace bb {

namespace {
thread_local bool g_coachOn = false;
thread_local TeamSide g_coachSide = TeamSide::HOME;
thread_local std::vector<int> g_coachAnswers;
thread_local size_t g_coachNext = 0;
}

bool coachDecides(TeamSide side) { return g_coachOn && side == g_coachSide; }
bool coachModeOn() { return g_coachOn; }

int askCoach(CoachQuestion q, int playerId, int otherId, int roll, int target, int options) {
    if (g_coachNext >= g_coachAnswers.size())
        throw CoachChoicePending(q, playerId, otherId, roll, target, options);
    const int a = g_coachAnswers[g_coachNext++];
    if (a < 0 || a > 30 || !(options & (1 << a)))
        throw std::invalid_argument("odpoved kouce " + std::to_string(a) +
                                    " neni mezi moznostmi (maska " + std::to_string(options) + ")");
    return a;
}

CoachChoiceScope::CoachChoiceScope(TeamSide side, std::vector<int> answers) {
    g_coachOn = true;
    g_coachSide = side;
    g_coachAnswers = std::move(answers);
    g_coachNext = 0;
}

CoachChoiceScope::~CoachChoiceScope() {
    g_coachOn = false;
    g_coachAnswers.clear();
    g_coachNext = 0;
}

} // namespace bb
