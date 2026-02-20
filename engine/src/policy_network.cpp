#include "bb/policy_network.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bb {

PolicyNetwork::PolicyNetwork(std::vector<float> weights, float bias, float temperature)
    : weights_(std::move(weights)), bias_(bias), temperature_(temperature) {
    // Pad to correct size if needed
    if (static_cast<int>(weights_.size()) < POLICY_INPUT_SIZE) {
        weights_.resize(POLICY_INPUT_SIZE, 0.0f);
    }
}

float PolicyNetwork::evaluateAction(const float* stateFeatures, const float* actionFeatures) const {
    float logit = bias_;
    int nState = std::min(static_cast<int>(weights_.size()), NUM_FEATURES);
    for (int i = 0; i < nState; ++i) {
        logit += weights_[i] * stateFeatures[i];
    }
    int nAction = std::min(static_cast<int>(weights_.size()) - NUM_FEATURES, NUM_ACTION_FEATURES);
    for (int i = 0; i < nAction; ++i) {
        logit += weights_[NUM_FEATURES + i] * actionFeatures[i];
    }
    return logit;
}

void PolicyNetwork::computePriors(const float* stateFeatures, const float* actionFeatures,
                                   int numActions, float* outPriors) const {
    if (numActions <= 0) return;

    if (numActions == 1) {
        outPriors[0] = 1.0f;
        return;
    }

    // Compute logits
    float maxLogit = -1e30f;
    for (int i = 0; i < numActions; ++i) {
        float logit = evaluateAction(stateFeatures, &actionFeatures[i * NUM_ACTION_FEATURES]);
        outPriors[i] = logit / temperature_;
        if (outPriors[i] > maxLogit) maxLogit = outPriors[i];
    }

    // Softmax with numerical stability
    float sumExp = 0.0f;
    for (int i = 0; i < numActions; ++i) {
        outPriors[i] = std::exp(outPriors[i] - maxLogit);
        sumExp += outPriors[i];
    }

    if (sumExp > 0.0f) {
        for (int i = 0; i < numActions; ++i) {
            outPriors[i] /= sumExp;
        }
    } else {
        // Fallback: uniform
        float uniform = 1.0f / numActions;
        for (int i = 0; i < numActions; ++i) {
            outPriors[i] = uniform;
        }
    }
}

std::unique_ptr<PolicyNetwork> loadPolicyNetwork(const std::string& jsonStr) {
    auto j = nlohmann::json::parse(jsonStr);

    if (!j.contains("policy_weights")) return nullptr;

    std::vector<float> weights;
    for (auto& v : j["policy_weights"]) {
        weights.push_back(v.get<float>());
    }

    float bias = 0.0f;
    if (j.contains("policy_bias")) {
        bias = j["policy_bias"].get<float>();
    }

    float temperature = 1.0f;
    if (j.contains("policy_temperature")) {
        temperature = j["policy_temperature"].get<float>();
    }

    return std::make_unique<PolicyNetwork>(std::move(weights), bias, temperature);
}

std::unique_ptr<PolicyNetwork> loadPolicyNetworkFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return loadPolicyNetwork(content);
}

} // namespace bb
