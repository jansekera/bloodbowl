#pragma once

#include "bb/enums.h"
#include "bb/position.h"
#include "bb/player_stats.h"
#include <bitset>
#include <cstdint>

namespace bb {

class SkillSet {
    std::bitset<128> bits_;
public:
    bool has(SkillName s) const { return bits_.test(static_cast<size_t>(s)); }
    void add(SkillName s) { bits_.set(static_cast<size_t>(s)); }
    void remove(SkillName s) { bits_.reset(static_cast<size_t>(s)); }
    int count() const { return static_cast<int>(bits_.count()); }
    void clear() { bits_.reset(); }
    bool operator==(const SkillSet& o) const { return bits_ == o.bits_; }
};

struct Player {
    int id = 0;                     // 1-22
    TeamSide teamSide = TeamSide::HOME;
    PlayerState state = PlayerState::OFF_PITCH;
    Position position{0, 0};
    PlayerStats stats{};
    SkillSet skills;
    int8_t movementRemaining = 0;
    bool hasMoved = false;
    bool hasActed = false;
    bool usedBlitz = false;
    bool lostTacklezones = false;
    bool proUsedThisTurn = false;

    bool hasSkill(SkillName s) const { return skills.has(s); }

    bool isOnPitch() const { return bb::isOnPitch(state); }

    bool canAct() const {
        return bb::canAct(state) && !hasActed && !lostTacklezones;
    }

    bool canMove() const {
        return bb::canAct(state) && !lostTacklezones && movementRemaining > 0;
    }
};

} // namespace bb
