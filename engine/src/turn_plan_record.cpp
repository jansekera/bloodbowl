#include "bb/turn_plan_record.h"

namespace bb {

TurnPlanRecord& currentTurnPlanRecord() {
    static thread_local TurnPlanRecord rec;
    return rec;
}

void resetTurnPlanRecord() {
    currentTurnPlanRecord() = TurnPlanRecord{};
}

} // namespace bb
