#include <unity.h>
#include "SemVer.h"

void setUp() {}
void tearDown() {}

static void test_equal() {
  TEST_ASSERT_EQUAL_INT(0, compareSemVer({1, 2, 3}, {1, 2, 3}));
}

static void test_major_dominates() {
  TEST_ASSERT_TRUE(compareSemVer({2, 0, 0}, {1, 99, 99}) > 0);
  TEST_ASSERT_TRUE(compareSemVer({1, 99, 99}, {2, 0, 0}) < 0);
}

static void test_minor_dominates_patch() {
  TEST_ASSERT_TRUE(compareSemVer({1, 3, 0}, {1, 2, 9}) > 0);
  TEST_ASSERT_TRUE(compareSemVer({1, 2, 9}, {1, 3, 0}) < 0);
}

static void test_patch() {
  TEST_ASSERT_TRUE(compareSemVer({0, 1, 1}, {0, 1, 0}) > 0);
  TEST_ASSERT_TRUE(compareSemVer({0, 1, 0}, {0, 1, 1}) < 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_equal);
  RUN_TEST(test_major_dominates);
  RUN_TEST(test_minor_dominates_patch);
  RUN_TEST(test_patch);
  return UNITY_END();
}
