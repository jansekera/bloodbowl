#pragma once

#include <cstdint>

namespace bb {

// --- TeamSide ---
enum class TeamSide : uint8_t { HOME, AWAY };

inline TeamSide opponent(TeamSide side) {
    return side == TeamSide::HOME ? TeamSide::AWAY : TeamSide::HOME;
}

// --- PlayerState ---
enum class PlayerState : uint8_t {
    STANDING, PRONE, STUNNED, KO, INJURED, DEAD, EJECTED, OFF_PITCH
};

inline bool isOnPitch(PlayerState s) {
    return s == PlayerState::STANDING || s == PlayerState::PRONE || s == PlayerState::STUNNED;
}

inline bool canAct(PlayerState s) {
    return s == PlayerState::STANDING;
}

inline bool exertsTacklezone(PlayerState s) {
    return s == PlayerState::STANDING;
}

// --- GamePhase ---
enum class GamePhase : uint8_t {
    COIN_TOSS, SETUP, KICKOFF, PLAY, TOUCHDOWN, HALF_TIME, GAME_OVER
};

inline bool isPlayable(GamePhase p) {
    return p == GamePhase::PLAY || p == GamePhase::KICKOFF;
}

inline bool isSetup(GamePhase p) {
    return p == GamePhase::SETUP;
}

// --- ActionType ---
enum class ActionType : uint8_t {
    MOVE, BLOCK, BLITZ, PASS, HAND_OFF, FOUL,
    THROW_TEAM_MATE, BOMB_THROW, HYPNOTIC_GAZE,
    BALL_AND_CHAIN, MULTIPLE_BLOCK,
    END_TURN, SETUP_PLAYER, END_SETUP,
    // F12 (24.08.2026): Leap dosud NEBYL akci -- `resolveLeap` byl hotovy, mel
    // tri zelene testy a NEMEL ZADNEHO VOLAJICIHO, takze oba wardanceri za cely
    // rok neskocili. Pridano na KONEC, aby se neposunuly ciselne hodnoty
    // ostatnich (python binding, logy).
    LEAP
};

inline bool requiresPlayer(ActionType t) {
    switch (t) {
        case ActionType::END_TURN:
        case ActionType::END_SETUP:
            return false;
        default:
            return true;
    }
}

// --- SkillName ---
// Values 0-73 for bitset indexing. Order matches PHP enum.
enum class SkillName : uint8_t {
    Block = 0,
    Catch,
    Dodge,
    Frenzy,
    Guard,
    MightyBlow,
    Pass,
    SideStep,
    StandFirm,
    StripBall,
    SureHands,          // 10
    Tackle,
    SureFeet,
    NervesOfSteel,
    Pro,
    Regeneration,
    ThickSkull,
    Horns,
    Dauntless,
    BigHand,
    Loner,              // 20
    BoneHead,
    ReallyStupid,
    WildAnimal,
    ThrowTeamMate,
    RightStuff,
    Stunty,
    PrehensileTail,
    TakeRoot,
    JumpUp,
    Sprint,             // 30
    BreakTackle,
    DirtyPlayer,
    Juggernaut,
    NoHands,
    SecretWeapon,
    Wrestle,
    Claw,
    Grab,
    Tentacles,
    DisturbingPresence, // 40
    DivingTackle,
    Leap,
    Accurate,
    StrongArm,
    SafeThrow,
    TwoHeads,
    ExtraArms,
    SneakyGit,
    Fend,
    PilingOn,           // 50
    Kick,
    KickOffReturn,
    Leader,
    HailMaryPass,
    DumpOff,
    DivingCatch,
    Shadowing,
    Stab,
    Bombardier,
    Bloodlust,          // 60
    HypnoticGaze,
    BallAndChain,
    Decay,
    Chainsaw,
    FoulAppearance,
    AlwaysHungry,
    VeryLongLegs,
    Animosity,
    PassBlock,
    NurglesRot,         // 70
    Titchy,
    Stakes,
    MultipleBlock,      // 73
    SKILL_COUNT         // 74 — not a real skill, used for bounds
};

// --- SkillCategory ---
enum class SkillCategory : uint8_t {
    GENERAL, AGILITY, STRENGTH, PASSING, MUTATION, EXTRAORDINARY
};

// --- BlockDiceFace ---
// 6-sided die: Attacker Down (1), Both Down (1), Pushed (2), Defender Stumbles (1), Defender Down (1)
enum class BlockDiceFace : uint8_t {
    ATTACKER_DOWN, BOTH_DOWN, PUSHED, DEFENDER_STUMBLES, DEFENDER_DOWN
};

// --- PassRange ---
enum class PassRange : uint8_t {
    QUICK_PASS, SHORT_PASS, LONG_PASS, LONG_BOMB
};

inline int passModifier(PassRange r) {
    switch (r) {
        case PassRange::QUICK_PASS: return 1;
        case PassRange::SHORT_PASS: return 0;
        case PassRange::LONG_PASS:  return -1;
        case PassRange::LONG_BOMB:  return -2;
    }
    return 0;
}


// --- CasualtyResult ---
// CRP Casualty table, rolled as D68 (a D6 for the tens digit, a D8 for the
// units): 11-38 Badly Hurt, 41-48 miss the next game, 51-58 lasting damage,
// 61-68 DEAD. Within a single match only two of these behave differently --
// Badly Hurt lets an Apothecary return the player to Reserves, and DEAD is
// permanent - but the rest are recorded so a league model has them later.
enum class CasualtyResult : uint8_t {
    BADLY_HURT, MISS_NEXT_GAME, NIGGLING, MA_LOSS, AV_LOSS, AG_LOSS, ST_LOSS, DEAD
};

// Lower is milder. Used when an Apothecary lets us pick between two rolls:
// taking the milder is free and never wrong, so it is automatic (user's rule
// 2026-08-10: optional skills default ON when they cost nothing).
inline int casualtySeverity(CasualtyResult c) {
    return static_cast<int>(c);
}

// --- Weather ---
enum class Weather : uint8_t {
    SWELTERING_HEAT, VERY_SUNNY, NICE, POURING_RAIN, BLIZZARD
};

// CRP Weather table (2D6): 2 Sweltering Heat, 3 Very Sunny, 4-10 Nice,
// 11 Pouring Rain, 12 Blizzard. Ours used to be shifted by one (a 3 gave
// Heat and a 4 gave Very Sunny), over-generating both bad-weather results
// at Nice's expense -- rules parity, 2026-08-10.
// Regular Throwing Ranges, keyed on the |dx|,|dy| offset from the thrower
// (rules parity, 2026-08-10). The range ruler is a SHAPED PHYSICAL TEMPLATE,
// not a radius: (13,0) is a Long Bomb while (5,12) cannot be thrown at all,
// and both sit at exactly 13.00 squares, so no distance function reproduces
// it. We used Chebyshev distance with 3/6/10 thresholds, which disagreed
// with 81 of the 196 cells -- every one of them in the throwing side's
// favour, including 45 where we allowed a pass the rules forbid outright.
// Source: the printed grid, transcribed with the symmetry constraint
// band(dx,dy) == band(dy,dx); see evidence/pass_range_grid_20260810.txt.
inline bool passRangeFromOffset(int dx, int dy, PassRange& out) {
    constexpr auto Q = PassRange::QUICK_PASS;
    constexpr auto S = PassRange::SHORT_PASS;
    constexpr auto L = PassRange::LONG_PASS;
    constexpr auto B = PassRange::LONG_BOMB;
    constexpr auto X = static_cast<PassRange>(0xFF);  // no pass possible
    static constexpr PassRange GRID[14][14] = {
        { Q, Q, Q, Q, S, S, S, L, L, L, L, B, B, B },  // dy=0
        { Q, Q, Q, Q, S, S, S, L, L, L, L, B, B, B },  // dy=1
        { Q, Q, Q, S, S, S, S, L, L, L, L, B, B, B },  // dy=2
        { Q, Q, S, S, S, S, S, L, L, L, B, B, B, X },  // dy=3
        { S, S, S, S, S, S, L, L, L, L, B, B, B, X },  // dy=4
        { S, S, S, S, S, L, L, L, L, B, B, B, X, X },  // dy=5
        { S, S, S, S, L, L, L, L, L, B, B, B, X, X },  // dy=6
        { L, L, L, L, L, L, L, L, B, B, B, X, X, X },  // dy=7
        { L, L, L, L, L, L, L, B, B, B, X, X, X, X },  // dy=8
        { L, L, L, L, L, B, B, B, B, X, X, X, X, X },  // dy=9
        { L, L, L, B, B, B, B, B, X, X, X, X, X, X },  // dy=10
        { B, B, B, B, B, B, B, X, X, X, X, X, X, X },  // dy=11
        { B, B, B, B, B, X, X, X, X, X, X, X, X, X },  // dy=12
        { B, B, B, X, X, X, X, X, X, X, X, X, X, X },  // dy=13
    };
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if (adx >= 14 || ady >= 14) return false;
    PassRange r = GRID[ady][adx];
    if (r == X) return false;
    out = r;
    return true;
}

inline Weather weatherFromRoll(int roll) {
    if (roll <= 2)  return Weather::SWELTERING_HEAT;
    if (roll == 3)  return Weather::VERY_SUNNY;
    if (roll <= 10) return Weather::NICE;
    if (roll == 11) return Weather::POURING_RAIN;
    return Weather::BLIZZARD;
}

// --- KickoffEvent ---
enum class KickoffEvent : uint8_t {
    GET_THE_REF = 2,
    RIOT = 3,
    PERFECT_DEFENCE = 4,
    HIGH_KICK = 5,
    CHEERING = 6,
    BRILLIANT_COACHING = 7,
    CHANGING_WEATHER = 8,
    QUICK_SNAP = 9,
    BLITZ = 10,
    THROW_A_ROCK = 11,
    PITCH_INVASION = 12
};

inline KickoffEvent kickoffEventFromRoll(int roll) {
    return static_cast<KickoffEvent>(roll);
}

// --- RosterSpeed ---
enum class RosterSpeed : uint8_t { SLOW, MIXED, FAST };

} // namespace bb
