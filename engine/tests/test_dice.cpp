#include <gtest/gtest.h>
#include "bb/dice.h"

using namespace bb;

TEST(FixedDiceRoller, SequentialConsumption) {
    FixedDiceRoller roller({3, 5, 2, 6, 4, 1});

    EXPECT_EQ(roller.remaining(), 6);
    EXPECT_EQ(roller.rollD6(), 3);
    EXPECT_EQ(roller.rollD6(), 5);
    EXPECT_EQ(roller.remaining(), 4);
    EXPECT_EQ(roller.rollD8(), 2);
    EXPECT_EQ(roller.remaining(), 3);
}

TEST(FixedDiceRoller, Roll2D6ConsumesTwoRolls) {
    FixedDiceRoller roller({3, 4, 6});

    EXPECT_EQ(roller.roll2D6(), 7);  // 3 + 4
    EXPECT_EQ(roller.remaining(), 1);
    EXPECT_EQ(roller.rollD6(), 6);
}

TEST(FixedDiceRoller, BlockDie) {
    // Block die: 1=AD, 2=BD, 3=Push, 4=Push, 5=DS, 6=DD
    FixedDiceRoller roller({1, 2, 3, 4, 5, 6});

    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::ATTACKER_DOWN);
    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::BOTH_DOWN);
    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::PUSHED);
    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::PUSHED);          // 2 pushed faces
    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::DEFENDER_STUMBLES);
    EXPECT_EQ(roller.rollBlockDie(), BlockDiceFace::DEFENDER_DOWN);
}

TEST(FixedDiceRoller, ThrowsWhenExhausted) {
    FixedDiceRoller roller({3});
    roller.rollD6();
    EXPECT_THROW(roller.rollD6(), std::out_of_range);
}

TEST(DiceRoller, ProducesValidD6) {
    DiceRoller roller(42);
    for (int i = 0; i < 100; ++i) {
        int val = roller.rollD6();
        EXPECT_GE(val, 1);
        EXPECT_LE(val, 6);
    }
}

TEST(DiceRoller, ProducesValidD8) {
    DiceRoller roller(42);
    for (int i = 0; i < 100; ++i) {
        int val = roller.rollD8();
        EXPECT_GE(val, 1);
        EXPECT_LE(val, 8);
    }
}

TEST(DiceRoller, DeterministicWithSeed) {
    DiceRoller a(12345);
    DiceRoller b(12345);

    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(a.rollD6(), b.rollD6());
    }
}
