#include "bb/policy_network.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bb {

// Linear constructor
PolicyNetwork::PolicyNetwork(std::vector<float> weights, float bias, float temperature)
    : weights_(std::move(weights)), bias_(bias), temperature_(temperature) {
    // Pad to correct size if needed
    if (static_cast<int>(weights_.size()) < POLICY_INPUT_SIZE) {
        weights_.resize(POLICY_INPUT_SIZE, 0.0f);
    }
}

// Neural constructor
PolicyNetwork::PolicyNetwork(std::vector<float> W1, std::vector<float> b1,
                              std::vector<float> W2, float b2,
                              int hiddenSize, float temperature)
    : neural_(true), hiddenSize_(hiddenSize),
      W1_(std::move(W1)), b1_(std::move(b1)), W2_(std::move(W2)),
      b2_(b2), temperature_(temperature) {}

float PolicyNetwork::evaluateAction(const float* stateFeatures, const float* actionFeatures) const {
    if (!neural_) {
        // Linear: dot product
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

    // Neural: input(85) -> hidden(ReLU) -> output(1)
    int H = hiddenSize_;
    float hidden[64]; // stack-allocate (max hidden size = 64)
    H = std::min(H, 64);

    for (int j = 0; j < H; ++j) {
        float sum = b1_[j];
        for (int i = 0; i < NUM_FEATURES; ++i) {
            sum += stateFeatures[i] * W1_[i * H + j];
        }
        for (int i = 0; i < NUM_ACTION_FEATURES; ++i) {
            sum += actionFeatures[i] * W1_[(NUM_FEATURES + i) * H + j];
        }
        hidden[j] = std::max(0.0f, sum); // ReLU
    }

    float output = b2_;
    for (int j = 0; j < H; ++j) {
        output += hidden[j] * W2_[j];
    }
    return output;
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

    // Neural policy: has policy_type == "neural"
    if (j.contains("policy_type") && j["policy_type"] == "neural") {
        int hiddenSize = j["policy_hidden_size"].get<int>();
        std::vector<float> W1, b1, W2;
        for (auto& v : j["policy_W1"]) W1.push_back(v.get<float>());
        for (auto& v : j["policy_b1"]) b1.push_back(v.get<float>());
        for (auto& v : j["policy_W2"]) W2.push_back(v.get<float>());
        float b2 = j.value("policy_b2", 0.0f);
        float temperature = j.value("policy_temperature", 1.0f);

        return std::make_unique<PolicyNetwork>(
            std::move(W1), std::move(b1), std::move(W2), b2, hiddenSize, temperature);
    }

    // Linear policy: has policy_weights array
    if (!j.contains("policy_weights")) return nullptr;

    std::vector<float> weights;
    for (auto& v : j["policy_weights"]) {
        weights.push_back(v.get<float>());
    }

    float bias = j.value("policy_bias", 0.0f);
    float temperature = j.value("policy_temperature", 1.0f);

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
