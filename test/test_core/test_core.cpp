// Phase 0 native sanity tests: prove the deterministic core works on the host.
#include <unity.h>
#include "core/Util.h"

void setUp() {}
void tearDown() {}

void test_seed_is_deterministic() {
  TEST_ASSERT_EQUAL_UINT32(gb::seedFor(73219, 14), gb::seedFor(73219, 14));
}

void test_seed_varies_by_level() {
  TEST_ASSERT_NOT_EQUAL(gb::seedFor(73219, 14), gb::seedFor(73219, 15));
}

void test_seed_varies_by_profile() {
  TEST_ASSERT_NOT_EQUAL(gb::seedFor(73219, 14), gb::seedFor(99999, 14));
}

void test_rng_deterministic() {
  gb::Rng a(gb::seedFor(1, 1)), b(gb::seedFor(1, 1));
  for (int i = 0; i < 100; i++) TEST_ASSERT_EQUAL_UINT32(a.next(), b.next());
}

void test_rng_below_in_range() {
  gb::Rng r(12345);
  for (int i = 0; i < 1000; i++) {
    uint32_t v = r.below(7);
    TEST_ASSERT_TRUE(v < 7);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_seed_is_deterministic);
  RUN_TEST(test_seed_varies_by_level);
  RUN_TEST(test_seed_varies_by_profile);
  RUN_TEST(test_rng_deterministic);
  RUN_TEST(test_rng_below_in_range);
  return UNITY_END();
}
