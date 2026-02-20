#include <gtest/gtest.h>
#include "bb/player.h"

using namespace bb;

TEST(SkillSet, AddHasRemove) {
    SkillSet ss;
    EXPECT_FALSE(ss.has(SkillName::Block));
    EXPECT_EQ(ss.count(), 0);

    ss.add(SkillName::Block);
    EXPECT_TRUE(ss.has(SkillName::Block));
    EXPECT_FALSE(ss.has(SkillName::Dodge));
    EXPECT_EQ(ss.count(), 1);

    ss.add(SkillName::Dodge);
    EXPECT_EQ(ss.count(), 2);

    ss.remove(SkillName::Block);
    EXPECT_FALSE(ss.has(SkillName::Block));
    EXPECT_TRUE(ss.has(SkillName::Dodge));
    EXPECT_EQ(ss.count(), 1);
}

TEST(SkillSet, AllSkills) {
    SkillSet ss;
    // Add all 74 skills
    for (int i = 0; i < static_cast<int>(SkillName::SKILL_COUNT); ++i) {
        ss.add(static_cast<SkillName>(i));
    }
    EXPECT_EQ(ss.count(), 74);
    EXPECT_TRUE(ss.has(SkillName::Block));
    EXPECT_TRUE(ss.has(SkillName::MultipleBlock));
}

TEST(SkillSet, Clear) {
    SkillSet ss;
    ss.add(SkillName::Block);
    ss.add(SkillName::Dodge);
    ss.clear();
    EXPECT_EQ(ss.count(), 0);
    EXPECT_FALSE(ss.has(SkillName::Block));
}

TEST(Player, HasSkill) {
    Player p;
    p.skills.add(SkillName::Block);
    p.skills.add(SkillName::Dodge);

    EXPECT_TRUE(p.hasSkill(SkillName::Block));
    EXPECT_TRUE(p.hasSkill(SkillName::Dodge));
    EXPECT_FALSE(p.hasSkill(SkillName::Tackle));
}

TEST(Player, IsOnPitch) {
    Player p;
    p.state = PlayerState::STANDING;
    EXPECT_TRUE(p.isOnPitch());

    p.state = PlayerState::PRONE;
    EXPECT_TRUE(p.isOnPitch());

    p.state = PlayerState::KO;
    EXPECT_FALSE(p.isOnPitch());

    p.state = PlayerState::OFF_PITCH;
    EXPECT_FALSE(p.isOnPitch());
}

TEST(Player, CanAct) {
    Player p;
    p.state = PlayerState::STANDING;
    p.hasActed = false;
    p.lostTacklezones = false;
    EXPECT_TRUE(p.canAct());

    // Already acted
    p.hasActed = true;
    EXPECT_FALSE(p.canAct());

    // Lost tacklezones
    p.hasActed = false;
    p.lostTacklezones = true;
    EXPECT_FALSE(p.canAct());

    // Prone
    p.lostTacklezones = false;
    p.state = PlayerState::PRONE;
    EXPECT_FALSE(p.canAct());
}

TEST(Player, CanMove) {
    Player p;
    p.state = PlayerState::STANDING;
    p.lostTacklezones = false;
    p.movementRemaining = 6;
    EXPECT_TRUE(p.canMove());

    p.movementRemaining = 0;
    EXPECT_FALSE(p.canMove());

    p.movementRemaining = 6;
    p.state = PlayerState::PRONE;
    EXPECT_FALSE(p.canMove());
}
