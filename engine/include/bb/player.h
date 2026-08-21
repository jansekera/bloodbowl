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
    // BB2016 l. 703-708: "All face-down players are turned face up at the END
    // of their team's next turn... Note that a player may not turn face up on
    // the turn they are Stunned."  The flip therefore belongs at END of turn,
    // and must skip anyone stunned during that very turn. Cleared for the
    // active team in resetPlayersForNewTurn, so a player stunned during the
    // OPPONENT's turn has it clear by the start of his own turn and flips at
    // that turn's end -- while one stunned during his own team's turn keeps
    // the flag through it and flips a turn later.
    bool stunnedThisTurn = false;
    bool hasActed = false;
    bool usedBlitz = false;
    bool lostTacklezones = false;
    bool proUsedThisTurn = false;
    // BB2016 l. 8573 a spol.: "Immediately after declaring an ACTION with this
    // player, roll a D6" -- JEDNOU za akci, ne za každé pole. Náš vícepolový
    // pohyb je N akcí MOVE, takže se hod dělal N-krát. Dokud ležící big guy
    // nikdy nejednal, nevadilo to; P45 to probudilo (oprava 21.08.).
    bool bigGuyCheckedThisTurn = false;
    // BB2016 l. 8089-8090 (Dodge): "the player may only re-roll ONE failed
    // Dodge roll PER TURN"; l. 8541-8542 (Sure Feet): "may only use the Sure
    // Feet skill once per turn". attemptRoll dosud žádný stav za kolo nemělo,
    // takže se rerollovalo neomezeně -- a nadržovalo to dodge týmům
    // (skaven, wood-elf) proti nám (oprava 21.08.).
    bool dodgeRerollUsedThisTurn = false;
    bool sureFeetRerollUsedThisTurn = false;
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
