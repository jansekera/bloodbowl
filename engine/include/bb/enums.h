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
    END_TURN, SETUP_PLAYER, END_SETUP
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

inline PassRange passRangeFromDistance(int dist) {
    if (dist <= 3)  return PassRange::QUICK_PASS;
    if (dist <= 6)  return PassRange::SHORT_PASS;
    if (dist <= 10) return PassRange::LONG_PASS;
    return PassRange::LONG_BOMB;
}

// --- Weather ---
enum class Weather : uint8_t {
    SWELTERING_HEAT, VERY_SUNNY, NICE, POURING_RAIN, BLIZZARD
};

inline Weather weatherFromRoll(int roll) {
    if (roll <= 3)  return Weather::SWELTERING_HEAT;
    if (roll == 4)  return Weather::VERY_SUNNY;
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

} // namespace bb
