#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>

namespace bb {

struct Position {
    int8_t x = 0;
    int8_t y = 0;

    static constexpr int PITCH_WIDTH = 26;   // 0-25
    static constexpr int PITCH_HEIGHT = 15;  // 0-14

    bool isOnPitch() const {
        return x >= 0 && x < PITCH_WIDTH && y >= 0 && y < PITCH_HEIGHT;
    }

    // Home endzone: x == 0, Away endzone: x == 25
    bool isInEndZone(bool home) const {
        return home ? (x == 0) : (x == PITCH_WIDTH - 1);
    }

    bool isInWideZone() const {
        return y >= 0 && y < 4 || y >= 11 && y < PITCH_HEIGHT;
    }

    // Chebyshev distance
    int distanceTo(Position other) const {
        return std::max(std::abs(x - other.x), std::abs(y - other.y));
    }

    // Returns all 8 adjacent positions (some may be off-pitch)
    std::array<Position, 8> getAdjacent() const;

    // Count of adjacent positions that are on-pitch
    int adjacentOnPitchCount() const;

    bool operator==(Position o) const { return x == o.x && y == o.y; }
    bool operator!=(Position o) const { return !(*this == o); }
};

} // namespace bb
