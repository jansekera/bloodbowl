#pragma once

#include "bb/position.h"

namespace bb {

struct BallState {
    Position position{-1, -1};
    bool isHeld = false;
    int carrierId = -1;

    bool isOnPitch() const { return position.isOnPitch(); }

    static BallState onGround(Position p) {
        return BallState{p, false, -1};
    }

    static BallState carried(Position p, int carrierId) {
        return BallState{p, true, carrierId};
    }

    static BallState offPitch() {
        return BallState{{-1, -1}, false, -1};
    }
};

} // namespace bb
