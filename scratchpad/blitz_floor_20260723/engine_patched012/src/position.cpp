#include "bb/position.h"

namespace bb {

std::array<Position, 8> Position::getAdjacent() const {
    return {{
        {static_cast<int8_t>(x - 1), static_cast<int8_t>(y - 1)},
        {static_cast<int8_t>(x    ), static_cast<int8_t>(y - 1)},
        {static_cast<int8_t>(x + 1), static_cast<int8_t>(y - 1)},
        {static_cast<int8_t>(x - 1), static_cast<int8_t>(y    )},
        {static_cast<int8_t>(x + 1), static_cast<int8_t>(y    )},
        {static_cast<int8_t>(x - 1), static_cast<int8_t>(y + 1)},
        {static_cast<int8_t>(x    ), static_cast<int8_t>(y + 1)},
        {static_cast<int8_t>(x + 1), static_cast<int8_t>(y + 1)},
    }};
}

int Position::adjacentOnPitchCount() const {
    int count = 0;
    for (auto adj : getAdjacent()) {
        if (adj.isOnPitch()) ++count;
    }
    return count;
}

} // namespace bb
