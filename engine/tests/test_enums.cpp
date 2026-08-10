#include <gtest/gtest.h>
#include "bb/enums.h"

using namespace bb;

TEST(TeamSide, Opponent) {
    EXPECT_EQ(opponent(TeamSide::HOME), TeamSide::AWAY);
    EXPECT_EQ(opponent(TeamSide::AWAY), TeamSide::HOME);
}

TEST(PlayerState, IsOnPitch) {
    EXPECT_TRUE(isOnPitch(PlayerState::STANDING));
    EXPECT_TRUE(isOnPitch(PlayerState::PRONE));
    EXPECT_TRUE(isOnPitch(PlayerState::STUNNED));
    EXPECT_FALSE(isOnPitch(PlayerState::KO));
    EXPECT_FALSE(isOnPitch(PlayerState::INJURED));
    EXPECT_FALSE(isOnPitch(PlayerState::DEAD));
    EXPECT_FALSE(isOnPitch(PlayerState::EJECTED));
    EXPECT_FALSE(isOnPitch(PlayerState::OFF_PITCH));
}

TEST(PlayerState, CanAct) {
    EXPECT_TRUE(canAct(PlayerState::STANDING));
    EXPECT_FALSE(canAct(PlayerState::PRONE));
    EXPECT_FALSE(canAct(PlayerState::STUNNED));
    EXPECT_FALSE(canAct(PlayerState::KO));
}

TEST(PlayerState, ExertsTacklezone) {
    EXPECT_TRUE(exertsTacklezone(PlayerState::STANDING));
    EXPECT_FALSE(exertsTacklezone(PlayerState::PRONE));
    EXPECT_FALSE(exertsTacklezone(PlayerState::STUNNED));
}

TEST(GamePhase, IsPlayable) {
    EXPECT_TRUE(isPlayable(GamePhase::PLAY));
    EXPECT_TRUE(isPlayable(GamePhase::KICKOFF));
    EXPECT_FALSE(isPlayable(GamePhase::COIN_TOSS));
    EXPECT_FALSE(isPlayable(GamePhase::SETUP));
    EXPECT_FALSE(isPlayable(GamePhase::GAME_OVER));
}

TEST(GamePhase, IsSetup) {
    EXPECT_TRUE(isSetup(GamePhase::SETUP));
    EXPECT_FALSE(isSetup(GamePhase::PLAY));
}

TEST(ActionType, RequiresPlayer) {
    EXPECT_TRUE(requiresPlayer(ActionType::MOVE));
    EXPECT_TRUE(requiresPlayer(ActionType::BLOCK));
    EXPECT_TRUE(requiresPlayer(ActionType::BLITZ));
    EXPECT_TRUE(requiresPlayer(ActionType::PASS));
    EXPECT_FALSE(requiresPlayer(ActionType::END_TURN));
    EXPECT_FALSE(requiresPlayer(ActionType::END_SETUP));
}

TEST(SkillName, ValuesAreSequential) {
    EXPECT_EQ(static_cast<int>(SkillName::Block), 0);
    EXPECT_EQ(static_cast<int>(SkillName::MultipleBlock), 73);
    EXPECT_EQ(static_cast<int>(SkillName::SKILL_COUNT), 74);
}

TEST(PassRange, Modifier) {
    EXPECT_EQ(passModifier(PassRange::QUICK_PASS), 1);
    EXPECT_EQ(passModifier(PassRange::SHORT_PASS), 0);
    EXPECT_EQ(passModifier(PassRange::LONG_PASS), -1);
    EXPECT_EQ(passModifier(PassRange::LONG_BOMB), -2);
}

TEST(PassRange, FromOffsetGridIsSymmetric) {
    // The range ruler does not care about direction, so the grid must be
    // symmetric in dx/dy. This is the invariant the table was reconstructed
    // under; it catches a mistyped cell immediately.
    for (int dy = 0; dy < 14; ++dy) {
        for (int dx = 0; dx < 14; ++dx) {
            PassRange a, b;
            bool okA = passRangeFromOffset(dx, dy, a);
            bool okB = passRangeFromOffset(dy, dx, b);
            EXPECT_EQ(okA, okB) << "asymmetric reachability at " << dx << "," << dy;
            if (okA && okB) {
                EXPECT_EQ(a, b) << "asymmetric band at " << dx << "," << dy;
            }
        }
    }
}

TEST(PassRange, FromOffsetIsNotAChebyshevRadius) {
    // (13,0) is a Long Bomb but (5,12) cannot be thrown at all, even though
    // both are 13.00 squares away -- the ruler is a shaped template, which is
    // exactly why a distance function cannot stand in for the grid.
    PassRange r;
    EXPECT_TRUE(passRangeFromOffset(13, 0, r));
    EXPECT_EQ(r, PassRange::LONG_BOMB);
    EXPECT_FALSE(passRangeFromOffset(5, 12, r));
    // Negative offsets mirror.
    EXPECT_TRUE(passRangeFromOffset(-13, 0, r));
    EXPECT_EQ(r, PassRange::LONG_BOMB);
}

TEST(Weather, FromRoll) {
    // CRP weather table: 2 Heat, 3 Very Sunny, 4-10 Nice, 11 Rain, 12 Blizzard
    EXPECT_EQ(weatherFromRoll(2), Weather::SWELTERING_HEAT);
    EXPECT_EQ(weatherFromRoll(3), Weather::VERY_SUNNY);
    EXPECT_EQ(weatherFromRoll(4), Weather::NICE);
    EXPECT_EQ(weatherFromRoll(5), Weather::NICE);
    EXPECT_EQ(weatherFromRoll(10), Weather::NICE);
    EXPECT_EQ(weatherFromRoll(11), Weather::POURING_RAIN);
    EXPECT_EQ(weatherFromRoll(12), Weather::BLIZZARD);
}

TEST(KickoffEvent, FromRoll) {
    EXPECT_EQ(kickoffEventFromRoll(2), KickoffEvent::GET_THE_REF);
    EXPECT_EQ(kickoffEventFromRoll(7), KickoffEvent::BRILLIANT_COACHING);
    EXPECT_EQ(kickoffEventFromRoll(12), KickoffEvent::PITCH_INVASION);
}
