#include "bb/dice.h"
#include <stdexcept>

namespace bb {

// --- DiceRoller ---

DiceRoller::DiceRoller(uint32_t seed) : rng_(seed) {}

DiceRoller::DiceRoller() : rng_(std::random_device{}()) {}

int DiceRoller::rollD6() {
    std::uniform_int_distribution<int> dist(1, 6);
    return dist(rng_);
}

int DiceRoller::rollD8() {
    std::uniform_int_distribution<int> dist(1, 8);
    return dist(rng_);
}

// --- FixedDiceRoller ---

FixedDiceRoller::FixedDiceRoller(std::vector<int> rolls)
    : rolls_(std::move(rolls)) {}

int FixedDiceRoller::next() {
    if (index_ >= rolls_.size()) {
        throw std::out_of_range("FixedDiceRoller: no more rolls");
    }
    return rolls_[index_++];
}

int FixedDiceRoller::rollD6() { return next(); }
int FixedDiceRoller::rollD8() { return next(); }

} // namespace bb
