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
    // Static roster-template label (replay logging only, no gameplay effect);
    // points into the static TeamRoster data, cheap to copy with the state.
    const char* positionName = "";
    int8_t movementRemaining = 0;
    bool hasMoved = false;
    bool hasActed = false;
    bool usedBlitz = false;
    bool lostTacklezones = false;
    bool proUsedThisTurn = false;
    // Sweltering Heat (package G, 2026-08-10): "Roll a D6 for each player on
    // the pitch at the end of a drive. On a roll of 1 the player collapses and
    // may not be set up for the next kick-off." One drive out, then back --
    // unlike a KO there is no recovery roll, so it needs its own flag rather
    // than a PlayerState.
    bool outNextSetup = false;
    // Was he actually placed on the pitch for the drive currently being
    // played? Secret Weapon ejection needs this as a fact rather than an
    // inference: a player can sit in the KO box across a drive boundary --
    // a bribe at the end of one drive leaves him in the game but off the
    // field for the next -- and inferring "he must have played" from a KO
    // state would send off a man who never took the field (user, 2026-08-11).
    bool playedThisDrive = false;

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
