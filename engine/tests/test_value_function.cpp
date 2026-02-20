#include <gtest/gtest.h>
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include <cmath>

using namespace bb;

TEST(ValueFunction, LinearDotProduct) {
    std::vector<float> weights = {1.0f, 2.0f, 3.0f, 0.5f};
    LinearValueFunction vf(weights);

    float features[] = {1.0f, 1.0f, 1.0f, 1.0f};
    // 1*1 + 2*1 + 3*1 + 0.5*1 = 6.5
    EXPECT_NEAR(vf.evaluate(features, 4), 6.5f, 0.001f);
}

TEST(ValueFunction, LinearZeroWeights) {
    std::vector<float> weights(NUM_FEATURES, 0.0f);
    LinearValueFunction vf(weights);

    float features[NUM_FEATURES];
    for (int i = 0; i < NUM_FEATURES; ++i) features[i] = 1.0f;

    EXPECT_NEAR(vf.evaluate(features, NUM_FEATURES), 0.0f, 0.001f);
}

TEST(ValueFunction, LinearPartialFeatures) {
    // Weights longer than features — should only use available features
    std::vector<float> weights = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    LinearValueFunction vf(weights);

    float features[] = {1.0f, 1.0f, 1.0f};
    // 1*1 + 2*1 + 3*1 = 6
    EXPECT_NEAR(vf.evaluate(features, 3), 6.0f, 0.001f);
}

TEST(ValueFunction, NeuralForwardPass) {
    // Simple neural network: 2 inputs, 2 hidden, 1 output
    // W1 = [[1, 0], [0, 1]], b1 = [0, 0]
    // W2 = [1, 1], b2 = 0
    // Input [1, 1] → hidden [ReLU(1), ReLU(1)] = [1, 1] → tanh(1+1) = tanh(2) ≈ 0.964
    std::vector<std::vector<float>> W1 = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    std::vector<float> b1 = {0.0f, 0.0f};
    std::vector<float> W2 = {1.0f, 1.0f};
    float b2 = 0.0f;

    NeuralValueFunction vf(2, 2, W1, b1, W2, b2);

    float features[] = {1.0f, 1.0f};
    float expected = std::tanh(2.0f);
    EXPECT_NEAR(vf.evaluate(features, 2), expected, 0.001f);
}

TEST(ValueFunction, NeuralReLU) {
    // Test that ReLU zeroes negative hidden values
    // W1 = [[1, -1]], b1 = [0, 0], W2 = [1, 1], b2 = 0
    // Input [1] → hidden [ReLU(1), ReLU(-1)] = [1, 0] → tanh(1) ≈ 0.762
    std::vector<std::vector<float>> W1 = {{1.0f, -1.0f}};
    std::vector<float> b1 = {0.0f, 0.0f};
    std::vector<float> W2 = {1.0f, 1.0f};
    float b2 = 0.0f;

    NeuralValueFunction vf(1, 2, W1, b1, W2, b2);

    float features[] = {1.0f};
    float expected = std::tanh(1.0f);
    EXPECT_NEAR(vf.evaluate(features, 1), expected, 0.001f);
}

TEST(ValueFunction, LoadLinearFromJson) {
    std::string json = "[1.0, 2.0, 3.0, 0.5]";
    auto vf = loadValueFunctionFromString(json);
    ASSERT_NE(vf, nullptr);

    float features[] = {1.0f, 1.0f, 1.0f, 1.0f};
    EXPECT_NEAR(vf->evaluate(features, 4), 6.5f, 0.001f);
}

TEST(ValueFunction, LoadNeuralFromJson) {
    std::string json = R"({
        "type": "neural",
        "hidden_size": 2,
        "W1": [[1.0, 0.0], [0.0, 1.0]],
        "b1": [0.0, 0.0],
        "W2": [[1.0], [1.0]],
        "b2": [0.0]
    })";
    auto vf = loadValueFunctionFromString(json);
    ASSERT_NE(vf, nullptr);

    float features[] = {1.0f, 1.0f};
    float expected = std::tanh(2.0f);
    EXPECT_NEAR(vf->evaluate(features, 2), expected, 0.001f);
}

TEST(ValueFunction, ZeroLinearWeightsZeroOutput) {
    std::string json = "[0.0, 0.0, 0.0]";
    auto vf = loadValueFunctionFromString(json);
    ASSERT_NE(vf, nullptr);

    float features[] = {5.0f, 3.0f, 1.0f};
    EXPECT_NEAR(vf->evaluate(features, 3), 0.0f, 0.001f);
}
