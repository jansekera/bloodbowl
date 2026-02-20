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

TEST(PassRange, FromDistance) {
    EXPECT_EQ(passRangeFromDistance(1), PassRange::QUICK_PASS);
    EXPECT_EQ(passRangeFromDistance(3), PassRange::QUICK_PASS);
    EXPECT_EQ(passRangeFromDistance(4), PassRange::SHORT_PASS);
    EXPECT_EQ(passRangeFromDistance(6), PassRange::SHORT_PASS);
    EXPECT_EQ(passRangeFromDistance(7), PassRange::LONG_PASS);
    EXPECT_EQ(passRangeFromDistance(10), PassRange::LONG_PASS);
    EXPECT_EQ(passRangeFromDistance(11), PassRange::LONG_BOMB);
}

TEST(Weather, FromRoll) {
    EXPECT_EQ(weatherFromRoll(2), Weather::SWELTERING_HEAT);
    EXPECT_EQ(weatherFromRoll(3), Weather::SWELTERING_HEAT);
    EXPECT_EQ(weatherFromRoll(4), Weather::VERY_SUNNY);
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
