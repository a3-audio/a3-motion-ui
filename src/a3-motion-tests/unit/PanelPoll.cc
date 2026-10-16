#include <a3-motion-ui/io/PanelPoll.hh>
#include <gtest/gtest.h>

using namespace a3::panel_poll;

// The firmware filters the pots on its own scan now (~45 ms for a full move),
// so the only lag left was the host asking for them every fifth poll.
TEST (PanelPoll, PotsAreRequestedInEveryCycle)
{
  bool asksForPots = false;
  for (auto const byte : request)
    asksForPots = asksForPots || byte == cmdGetPots;

  EXPECT_TRUE (asksForPots);
}

TEST (PanelPoll, TheReplyHoldsButtonsEncodersAndPotsInOrder)
{
  EXPECT_EQ (request.size (), 3u);
  EXPECT_EQ (replySize, btnFrameSize + encFrameSize + potFrameSize);
  EXPECT_EQ (potOffset, btnFrameSize + encFrameSize);
}
