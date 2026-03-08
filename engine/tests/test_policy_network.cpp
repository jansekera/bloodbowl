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

// --- Neural PolicyNetwork tests ---

TEST(PolicyNetwork, NeuralZeroWeightsGiveUniform) {
    int H = 4;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);
    float b2 = 0.0f;

    PolicyNetwork pn(W1, b1, W2, b2, H);
    EXPECT_TRUE(pn.isNeural());

    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[3 * NUM_ACTION_FEATURES] = {};

    float priors[3];
    pn.computePriors(stateFeats, actionFeats, 3, priors);

    EXPECT_NEAR(priors[0], 1.0f / 3.0f, 0.001f);
    EXPECT_NEAR(priors[1], 1.0f / 3.0f, 0.001f);
    EXPECT_NEAR(priors[2], 1.0f / 3.0f, 0.001f);
}

TEST(PolicyNetwork, NeuralForwardPass) {
    // Minimal network: input_size=85, hidden=2
    int H = 2;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);
    float b2 = 0.0f;

    // Set W1[0, 0] = 1.0 (state feature 0 → hidden 0)
    // Row-major: W1[i * H + j]
    W1[0 * H + 0] = 1.0f;
    // Set W2[0] = 1.0 (hidden 0 → output)
    W2[0] = 1.0f;
    b2 = 0.5f;

    PolicyNetwork pn(W1, b1, W2, b2, H);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 3.0f;

    float actionFeats[NUM_ACTION_FEATURES] = {};

    // Forward: hidden[0] = ReLU(3.0*1.0 + 0) = 3.0
    //          hidden[1] = ReLU(0) = 0
    //          output = 3.0*1.0 + 0*0 + 0.5 = 3.5
    float logit = pn.evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 3.5f, 0.001f);
}

TEST(PolicyNetwork, NeuralReLUBlocksNegative) {
    int H = 2;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);
    float b2 = 0.0f;

    // W1[0,0] = -1.0 → negative pre-activation → ReLU clips to 0
    W1[0 * H + 0] = -1.0f;
    W2[0] = 1.0f;

    PolicyNetwork pn(W1, b1, W2, b2, H);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 3.0f;

    float actionFeats[NUM_ACTION_FEATURES] = {};

    // hidden[0] = ReLU(-3.0) = 0, output = 0
    float logit = pn.evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 0.0f, 0.001f);
}

TEST(PolicyNetwork, NeuralActionFeatures) {
    int H = 2;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);
    float b2 = 0.0f;

    // Action feature 0 → hidden 1: W1[(NUM_FEATURES+0) * H + 1] = 2.0
    W1[NUM_FEATURES * H + 1] = 2.0f;
    W2[1] = 0.5f;

    PolicyNetwork pn(W1, b1, W2, b2, H);

    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[NUM_ACTION_FEATURES] = {};
    actionFeats[0] = 4.0f;

    // hidden[1] = ReLU(4.0*2.0) = 8.0, output = 8.0*0.5 = 4.0
    float logit = pn.evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 4.0f, 0.001f);
}

TEST(PolicyNetwork, NeuralSoftmaxSumsToOne) {
    int H = 4;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);

    // Some non-trivial weights
    W1[0 * H + 0] = 0.5f;
    W1[NUM_FEATURES * H + 1] = 1.0f;
    W2[0] = 1.0f;
    W2[1] = -0.5f;

    PolicyNetwork pn(W1, b1, W2, 0.1f, H);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 1.0f;

    constexpr int N = 4;
    float actionFeats[N * NUM_ACTION_FEATURES] = {};
    for (int i = 0; i < N; ++i) {
        actionFeats[i * NUM_ACTION_FEATURES] = static_cast<float>(i) * 0.5f;
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

TEST(PolicyNetwork, NeuralLoadFromJSON) {
    // 85 inputs, hidden_size=2
    // W1: 85*2 = 170 values, all zero except [0]=1.0 (state feat 0 → hidden 0)
    // b1: [0.0, 0.0]
    // W2: [1.0, 0.0]
    // b2: 0.1

    std::string json = R"({
        "policy_type": "neural",
        "policy_hidden_size": 2,
        "policy_W1": [1.0, 0.0)" + std::string(168 * 4 + 168 * 1, ' ');  // dummy - need proper array

    // Build proper JSON programmatically
    std::string w1 = "[1.0, 0.0";
    for (int i = 1; i < POLICY_INPUT_SIZE; ++i) {
        w1 += ", 0.0, 0.0";
    }
    w1 += "]";

    json = R"({"policy_type": "neural", "policy_hidden_size": 2, )"
           R"("policy_W1": )" + w1 + R"(, )"
           R"("policy_b1": [0.0, 0.0], )"
           R"("policy_W2": [1.0, 0.0], )"
           R"("policy_b2": 0.1, )"
           R"("policy_temperature": 0.5})";

    auto pn = loadPolicyNetwork(json);
    ASSERT_NE(pn, nullptr);
    EXPECT_TRUE(pn->isNeural());
    EXPECT_NEAR(pn->temperature(), 0.5f, 0.001f);

    float stateFeats[NUM_FEATURES] = {};
    stateFeats[0] = 2.0f;
    float actionFeats[NUM_ACTION_FEATURES] = {};

    // hidden[0] = ReLU(2.0*1.0) = 2.0, hidden[1] = 0
    // output = 2.0*1.0 + 0.1 = 2.1
    float logit = pn->evaluateAction(stateFeats, actionFeats);
    EXPECT_NEAR(logit, 2.1f, 0.001f);
}

TEST(PolicyNetwork, NeuralTemperature) {
    int H = 2;
    std::vector<float> W1(POLICY_INPUT_SIZE * H, 0.0f);
    std::vector<float> b1(H, 0.0f);
    std::vector<float> W2(H, 0.0f);

    W1[NUM_FEATURES * H + 0] = 1.0f;  // action feat 0 → hidden 0
    W2[0] = 1.0f;

    PolicyNetwork pnLow(W1, b1, W2, 0.0f, H, 0.1f);
    PolicyNetwork pnHigh(W1, b1, W2, 0.0f, H, 10.0f);

    float stateFeats[NUM_FEATURES] = {};
    float actionFeats[2 * NUM_ACTION_FEATURES] = {};
    actionFeats[0] = 2.0f;  // action 0: logit > 0
    // action 1: logit = 0

    float priorsLow[2], priorsHigh[2];
    pnLow.computePriors(stateFeats, actionFeats, 2, priorsLow);
    pnHigh.computePriors(stateFeats, actionFeats, 2, priorsHigh);

    // Low temp → sharper, high temp → flatter
    EXPECT_GT(priorsLow[0], priorsHigh[0]);
    EXPECT_GT(priorsHigh[1], priorsLow[1]);
}
