#pragma once

#include "bb/enums.h"
#include <cstdint>
#include <random>
#include <vector>

namespace bb {

class DiceRollerBase {
public:
    virtual ~DiceRollerBase() = default;
    virtual int rollD6() = 0;
    virtual int rollD8() = 0;
    virtual int roll2D6() { return rollD6() + rollD6(); }
    virtual BlockDiceFace rollBlockDie() {
        int roll = rollD6();
        switch (roll) {
            case 1: return BlockDiceFace::ATTACKER_DOWN;
            case 2: return BlockDiceFace::BOTH_DOWN;
            case 3: case 4: return BlockDiceFace::PUSHED;
            case 5: return BlockDiceFace::DEFENDER_STUMBLES;
            default: return BlockDiceFace::DEFENDER_DOWN;
        }
    }
};

class DiceRoller : public DiceRollerBase {
    std::mt19937 rng_;
public:
    explicit DiceRoller(uint32_t seed);
    DiceRoller();  // uses random_device

    int rollD6() override;
    int rollD8() override;
};

class FixedDiceRoller : public DiceRollerBase {
    std::vector<int> rolls_;
    size_t index_ = 0;

    int next();
public:
    explicit FixedDiceRoller(std::vector<int> rolls);

    int rollD6() override;
    int rollD8() override;

    size_t remaining() const { return rolls_.size() - index_; }
};

} // namespace bb
