#pragma once

#include "bb/action_features.h"
#include "bb/feature_extractor.h"
#include <vector>
#include <memory>
#include <string>

namespace bb {

constexpr int POLICY_INPUT_SIZE = NUM_FEATURES + NUM_ACTION_FEATURES; // 70 + 15 = 85

class PolicyNetwork {
    std::vector<float> weights_;  // 85 weights (70 state + 15 action)
    float bias_ = 0.0f;
    float temperature_ = 1.0f;

public:
    PolicyNetwork() : weights_(POLICY_INPUT_SIZE, 0.0f) {}
    PolicyNetwork(std::vector<float> weights, float bias, float temperature = 1.0f);

    // Compute logit for a single action: dot(weights, concat(stateFeats, actionFeats)) + bias
    float evaluateAction(const float* stateFeatures, const float* actionFeatures) const;

    // Compute softmax priors for all actions.
    // actionFeatures: packed array of numActions * NUM_ACTION_FEATURES floats
    // outPriors: array of numActions floats, will sum to 1.0
    void computePriors(const float* stateFeatures, const float* actionFeatures,
                       int numActions, float* outPriors) const;

    float temperature() const { return temperature_; }
    void setTemperature(float t) { temperature_ = t; }
};

// Load from JSON string with "policy_weights" array and "policy_bias" float
std::unique_ptr<PolicyNetwork> loadPolicyNetwork(const std::string& jsonStr);

// Load from JSON file
std::unique_ptr<PolicyNetwork> loadPolicyNetworkFromFile(const std::string& path);

} // namespace bb
