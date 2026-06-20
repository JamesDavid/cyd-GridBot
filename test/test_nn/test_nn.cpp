// NeuroBot tests: a single neuron must actually LEARN via backprop. The core
// teaching claim is "guess -> see the error -> nudge the weights -> get better",
// so the tests assert the loss falls and the learned decisions are correct on
// linearly separable problems (and that XOR, which needs a hidden layer, does NOT
// separate — the motivation for the next lesson).
#include <unity.h>
#include "game/Perceptron.h"

using namespace gb;

void setUp() {}
void tearDown() {}

// "turn if wall": 1 input. x=0 -> 0 (go), x=1 -> 1 (turn).
void test_learns_turn_if_wall() {
  Perceptron p; p.nIn = 1; p.lr = 0.8f; p.reset(7);
  const float X[2] = {0.0f, 1.0f};
  const float y[2] = {0.0f, 1.0f};
  float first = p.trainEpoch(X, y, 2);
  float last = first;
  for (int e = 0; e < 4000; e++) last = p.trainEpoch(X, y, 2);
  TEST_ASSERT_TRUE(last < first);          // it learned (loss fell)
  TEST_ASSERT_TRUE(last < 0.02f);          // and learned it well
  float x0 = 0.0f, x1 = 1.0f;
  TEST_ASSERT_TRUE(p.forward(&x0) < 0.5f); // no wall -> go
  TEST_ASSERT_TRUE(p.forward(&x1) > 0.5f); // wall -> turn
}

// OR is linearly separable -> a single neuron can learn it.
void test_learns_or() {
  Perceptron p; p.nIn = 2; p.lr = 0.8f; p.reset(42);
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float y[4] = {0, 1, 1, 1};
  for (int e = 0; e < 6000; e++) p.trainEpoch(X, y, 4);
  for (int i = 0; i < 4; i++) {
    float pred = p.forward(&X[i * 2]);
    if (y[i] > 0.5f) TEST_ASSERT_TRUE(pred > 0.5f);
    else             TEST_ASSERT_TRUE(pred < 0.5f);
  }
}

// XOR is NOT linearly separable -> a single neuron CANNOT solve it. This is the
// lesson that motivates a hidden layer; assert it genuinely fails to separate.
void test_xor_is_unlearnable_by_one_neuron() {
  Perceptron p; p.nIn = 2; p.lr = 0.8f; p.reset(99);
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float y[4] = {0, 1, 1, 0};
  for (int e = 0; e < 8000; e++) p.trainEpoch(X, y, 4);
  int correct = 0;
  for (int i = 0; i < 4; i++) {
    float pred = p.forward(&X[i * 2]);
    if ((pred > 0.5f) == (y[i] > 0.5f)) correct++;
  }
  TEST_ASSERT_TRUE(correct < 4);  // can't get all four (no single line separates XOR)
}

// Deterministic init: same seed -> same starting weights (reproducible lessons).
void test_reset_is_deterministic() {
  Perceptron a, b; a.nIn = 2; b.nIn = 2;
  a.reset(123); b.reset(123);
  for (int i = 0; i < 2; i++) TEST_ASSERT_EQUAL_FLOAT(a.w[i], b.w[i]);
  TEST_ASSERT_EQUAL_FLOAT(a.b, b.b);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_learns_turn_if_wall);
  RUN_TEST(test_learns_or);
  RUN_TEST(test_xor_is_unlearnable_by_one_neuron);
  RUN_TEST(test_reset_is_deterministic);
  return UNITY_END();
}
