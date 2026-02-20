#pragma once

#include <vector>
#include <memory>
#include <string>

namespace bb {

class ValueFunction {
public:
    virtual ~ValueFunction() = default;
    virtual float evaluate(const float* features, int numFeatures) const = 0;
};

class LinearValueFunction : public ValueFunction {
    std::vector<float> weights_;
public:
    explicit LinearValueFunction(std::vector<float> weights);
    float evaluate(const float* features, int numFeatures) const override;
};

class NeuralValueFunction : public ValueFunction {
    int inputSize_;
    int hiddenSize_;
    std::vector<std::vector<float>> W1_;  // [input][hidden]
    std::vector<float> b1_;               // [hidden]
    std::vector<float> W2_;               // [hidden] (single output)
    float b2_;
public:
    NeuralValueFunction(int inputSize, int hiddenSize,
                        std::vector<std::vector<float>> W1,
                        std::vector<float> b1,
                        std::vector<float> W2,
                        float b2);
    float evaluate(const float* features, int numFeatures) const override;
};

// Load from JSON file (auto-detects linear vs neural format)
std::unique_ptr<ValueFunction> loadValueFunction(const std::string& path);

// Load from JSON string (for testing)
std::unique_ptr<ValueFunction> loadValueFunctionFromString(const std::string& json);

} // namespace bb
