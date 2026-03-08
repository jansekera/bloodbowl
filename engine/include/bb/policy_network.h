#pragma once

#include "bb/action_features.h"
#include "bb/feature_extractor.h"
#include <vector>
#include <memory>
#include <string>

namespace bb {

constexpr int POLICY_INPUT_SIZE = NUM_FEATURES + NUM_ACTION_FEATURES; // 70 + 15 = 85

class PolicyNetwork {
    // Linear mode
    std::vector<float> weights_;  // 85 weights (70 state + 15 action)
    float bias_ = 0.0f;

    // Neural mode: input(85) → hidden(ReLU) → output(1)
    bool neural_ = false;
    int hiddenSize_ = 0;
    std::vector<float> W1_;  // POLICY_INPUT_SIZE * hiddenSize, row-major
    std::vector<float> b1_;  // hiddenSize
    std::vector<float> W2_;  // hiddenSize
    float b2_ = 0.0f;

    float temperature_ = 1.0f;

public:
    // Linear constructors
    PolicyNetwork() : weights_(POLICY_INPUT_SIZE, 0.0f) {}
    PolicyNetwork(std::vector<float> weights, float bias, float temperature = 1.0f);

    // Neural constructor
    PolicyNetwork(std::vector<float> W1, std::vector<float> b1,
                  std::vector<float> W2, float b2,
                  int hiddenSize, float temperature = 1.0f);

    // Compute logit for a single action
    float evaluateAction(const float* stateFeatures, const float* actionFeatures) const;

    // Compute softmax priors for all actions.
    // actionFeatures: packed array of numActions * NUM_ACTION_FEATURES floats
    // outPriors: array of numActions floats, will sum to 1.0
    void computePriors(const float* stateFeatures, const float* actionFeatures,
                       int numActions, float* outPriors) const;

    bool isNeural() const { return neural_; }
    float temperature() const { return temperature_; }
    void setTemperature(float t) { temperature_ = t; }
};

// Load from JSON string with "policy_weights" array and "policy_bias" float
std::unique_ptr<PolicyNetwork> loadPolicyNetwork(const std::string& jsonStr);

// Load from JSON file
std::unique_ptr<PolicyNetwork> loadPolicyNetworkFromFile(const std::string& path);

} // namespace bb
