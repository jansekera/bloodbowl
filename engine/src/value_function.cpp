#include "bb/value_function.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace bb {

// --- LinearValueFunction ---

LinearValueFunction::LinearValueFunction(std::vector<float> weights)
    : weights_(std::move(weights)) {}

float LinearValueFunction::evaluate(const float* features, int numFeatures) const {
    float sum = 0.0f;
    int n = std::min(numFeatures, static_cast<int>(weights_.size()));
    for (int i = 0; i < n; ++i) {
        sum += weights_[i] * features[i];
    }
    return sum;
}

// --- NeuralValueFunction ---

NeuralValueFunction::NeuralValueFunction(int inputSize, int hiddenSize,
                                         std::vector<std::vector<float>> W1,
                                         std::vector<float> b1,
                                         std::vector<float> W2,
                                         float b2)
    : inputSize_(inputSize), hiddenSize_(hiddenSize),
      W1_(std::move(W1)), b1_(std::move(b1)),
      W2_(std::move(W2)), b2_(b2) {}

float NeuralValueFunction::evaluate(const float* features, int numFeatures) const {
    // Hidden layer: h = ReLU(features @ W1 + b1)
    std::vector<float> hidden(hiddenSize_);
    int inSize = std::min(numFeatures, inputSize_);

    for (int j = 0; j < hiddenSize_; ++j) {
        float sum = b1_[j];
        for (int i = 0; i < inSize; ++i) {
            sum += features[i] * W1_[i][j];
        }
        hidden[j] = std::max(0.0f, sum);  // ReLU
    }

    // Output: tanh(hidden @ W2 + b2)
    float out = b2_;
    for (int j = 0; j < hiddenSize_; ++j) {
        out += hidden[j] * W2_[j];
    }
    return std::tanh(out);
}

// --- JSON Loading ---

namespace {

std::unique_ptr<ValueFunction> parseNeuralWeights(const nlohmann::json& j,
                                                    const std::string& w1Key = "W1",
                                                    const std::string& b1Key = "b1",
                                                    const std::string& w2Key = "W2",
                                                    const std::string& b2Key = "b2") {
    int hiddenSize = j["hidden_size"].get<int>();

    auto W1_json = j[w1Key];
    int inputSize = static_cast<int>(W1_json.size());

    std::vector<std::vector<float>> W1(inputSize);
    for (int i = 0; i < inputSize; ++i) {
        W1[i].resize(hiddenSize);
        for (int jj = 0; jj < hiddenSize; ++jj) {
            W1[i][jj] = W1_json[i][jj].get<float>();
        }
    }

    std::vector<float> b1(hiddenSize);
    for (int j2 = 0; j2 < hiddenSize; ++j2) {
        b1[j2] = j[b1Key][j2].get<float>();
    }

    std::vector<float> W2(hiddenSize);
    for (int j2 = 0; j2 < hiddenSize; ++j2) {
        // W2 in PHP format: [[w0], [w1], ...] (each row is [value])
        if (j[w2Key][j2].is_array()) {
            W2[j2] = j[w2Key][j2][0].get<float>();
        } else {
            W2[j2] = j[w2Key][j2].get<float>();
        }
    }

    float b2 = 0.0f;
    if (j[b2Key].is_array()) {
        b2 = j[b2Key][0].get<float>();
    } else {
        b2 = j[b2Key].get<float>();
    }

    return std::make_unique<NeuralValueFunction>(inputSize, hiddenSize,
                                                  std::move(W1), std::move(b1),
                                                  std::move(W2), b2);
}

std::unique_ptr<ValueFunction> parseJson(const nlohmann::json& j) {
    if (j.is_object() && j.contains("type")) {
        std::string type = j["type"].get<std::string>();

        // AlphaZero combined format: value weights inside combined file
        if (type == "alphazero_linear" && j.contains("value_weights")) {
            auto& vw = j["value_weights"];
            std::vector<float> weights;
            weights.reserve(vw.size());
            for (auto& v : vw) {
                weights.push_back(v.get<float>());
            }
            return std::make_unique<LinearValueFunction>(std::move(weights));
        }

        if (type == "alphazero_neural" && j.contains("value_W1")) {
            return parseNeuralWeights(j, "value_W1", "value_b1", "value_W2", "value_b2");
        }

        // Standard neural model
        if (type == "neural") {
            return parseNeuralWeights(j);
        }
    }

    // Linear model: plain array of floats
    if (j.is_array()) {
        std::vector<float> weights;
        weights.reserve(j.size());
        for (auto& v : j) {
            weights.push_back(v.get<float>());
        }
        return std::make_unique<LinearValueFunction>(std::move(weights));
    }

    return nullptr;
}

} // anonymous namespace

std::unique_ptr<ValueFunction> loadValueFunction(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;
    nlohmann::json j = nlohmann::json::parse(file);
    return parseJson(j);
}

std::unique_ptr<ValueFunction> loadValueFunctionFromString(const std::string& json) {
    auto j = nlohmann::json::parse(json);
    return parseJson(j);
}

} // namespace bb
