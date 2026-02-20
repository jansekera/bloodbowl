#include <gtest/gtest.h>
#include "bb/policy_network.h"
#include <cmath>
#include <numeric>

using namespace bb;

TEST(PolicyNetwork, ZeroWeightsGiveUniform) {
    PolicyNetwork pn;  // default: all zeros

    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[3 * NUM_ACTION_FEATURES] = {};

    // 3 actions, all zero features → all same logit → uniform
    float priors[3];
    pn.computePriors(stateFeats, actionFeats, 3, priors);

    EXPECT_NEAR(priors[0], 1.0f / 3.0f, 0.001f);
    EXPECT_NEAR(priors[1], 1.0f / 3.0f, 0.001f);
    EXPECT_NEAR(priors[2], 1.0f / 3.0f, 0.001f);
}

TEST(PolicyNetwork, SoftmaxSumsToOne) {
    std::vector<float> weights(POLICY_INPUT_SIZE, 0.0f);
    weights[0] = 1.0f;    // state feature 0
    weights[NUM_FEATURES] = 2.0f;  // action feature 0

    PolicyNetwork pn(weights, 0.0f);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 0.5f;

    // 5 actions with varying action features
    constexpr int N = 5;
    float actionFeats[N * NUM_ACTION_FEATURES] = {};
    for (int i = 0; i < N; ++i) {
        actionFeats[i * NUM_ACTION_FEATURES] = static_cast<float>(i) * 0.3f;
    }

    float priors[N];
    pn.computePriors(stateFeats, actionFeats, N, priors);

    float sum = 0.0f;
    for (int i = 0; i < N; ++i) {
        EXPECT_GE(priors[i], 0.0f);
        sum += priors[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.001f);
}

TEST(PolicyNetwork, SingleWeightCorrectLogit) {
    std::vector<float> weights(POLICY_INPUT_SIZE, 0.0f);
    weights[NUM_FEATURES] = 1.0f;  // Only action feature [0] matters

    PolicyNetwork pn(weights, 0.0f);

    float stateFeats[NUM_FEATURES] = {};

    // Action 0: feature[0] = 1.0 → logit = 1.0
    // Action 1: feature[0] = 0.0 → logit = 0.0
    float actionFeats[2 * NUM_ACTION_FEATURES] = {};
    actionFeats[0] = 1.0f;  // action 0, feature 0
    // action 1 stays 0

    float priors[2];
    pn.computePriors(stateFeats, actionFeats, 2, priors);

    // softmax([1, 0]) → [e/(e+1), 1/(e+1)] ≈ [0.731, 0.269]
    float expected0 = std::exp(1.0f) / (std::exp(1.0f) + 1.0f);
    EXPECT_NEAR(priors[0], expected0, 0.01f);
    EXPECT_GT(priors[0], priors[1]);
}

TEST(PolicyNetwork, SingleAction) {
    PolicyNetwork pn;
    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[NUM_ACTION_FEATURES] = {};

    float priors[1];
    pn.computePriors(stateFeats, actionFeats, 1, priors);

    EXPECT_FLOAT_EQ(priors[0], 1.0f);
}

TEST(PolicyNetwork, EvaluateAction) {
    std::vector<float> weights(POLICY_INPUT_SIZE, 0.0f);
    weights[0] = 0.5f;
    weights[NUM_FEATURES + 1] = 2.0f;

    PolicyNetwork pn(weights, 0.1f);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 3.0f;

    float actionFeats[NUM_ACTION_FEATURES] = {};
    actionFeats[1] = 1.0f;

    // Expected: 0.5*3.0 + 2.0*1.0 + 0.1 = 3.6
    float logit = pn.evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 3.6f, 0.001f);
}

TEST(PolicyNetwork, TemperatureAffectsSoftmax) {
    std::vector<float> weights(POLICY_INPUT_SIZE, 0.0f);
    weights[NUM_FEATURES] = 1.0f;

    // Low temperature → sharper distribution
    PolicyNetwork pnLow(weights, 0.0f, 0.1f);
    // High temperature → flatter distribution
    PolicyNetwork pnHigh(weights, 0.0f, 10.0f);

    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[2 * NUM_ACTION_FEATURES] = {};
    actionFeats[0] = 1.0f;

    float priorsLow[2], priorsHigh[2];
    pnLow.computePriors(stateFeats, actionFeats, 2, priorsLow);
    pnHigh.computePriors(stateFeats, actionFeats, 2, priorsHigh);

    // Low temp should be more concentrated
    EXPECT_GT(priorsLow[0], priorsHigh[0]);
    // High temp should be closer to uniform
    EXPECT_GT(priorsHigh[1], priorsLow[1]);
}

TEST(PolicyNetwork, LoadFromJSON) {
    std::string json = R"({
        "policy_weights": [0.1, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                           0.0, 0.0, 0.0, 0.0, 0.0],
        "policy_bias": 0.05
    })";

    auto pn = loadPolicyNetwork(json);
    ASSERT_NE(pn, nullptr);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 1.0f;
    stateFeats[1] = 1.0f;

    float actionFeats[NUM_ACTION_FEATURES] = {};
    actionFeats[0] = 1.0f;  // This maps to weight index 70 (= 0.5)

    // logit = 0.1*1 + 0.2*1 + 0.5*1 + 0.05 = 0.85
    float logit = pn->evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 0.85f, 0.001f);
}
