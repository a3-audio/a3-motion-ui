#include <gtest/gtest.h>

TEST (HelloTest, UpstreamExample)
{
  // Expect two strings not to be equal.
  EXPECT_STRNE ("hello", "world");
  // Expect equality.
  EXPECT_EQ (7 * 6, 42);
}
