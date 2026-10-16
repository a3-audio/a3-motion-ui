/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <gtest/gtest.h>

#include <vector>

#include <a3-motion-ui/io/InputOutputAdapter.hh>

using namespace a3;

namespace
{
/** An adapter with no hardware. The thread is never started: the test calls
 *  the input functions the thread would call, then timerCallback(), which is
 *  the message thread's turn to take them. */
class InputAdapter : public InputOutputAdapter
{
public:
  using InputOutputAdapter::inputPadValue;
  using InputOutputAdapter::inputPanelKey;

  static PadIndex
  pad (index_t channel, index_t pad)
  {
    return { channel, pad };
  }

private:
  void processInput () override {}
  void outputButtonLED (Button, juce::Colour) override {}
  void outputPadLED (PadIndex, juce::Colour) override {}
};
}

// The input FIFO held 31 messages and dropped the rest silently. Panel input
// comes in bursts -- a hand across the pads, an encoder spun -- and the
// message thread can be late.
TEST (AdapterInput, ABurstLargerThanTheOldQueueArrivesWholeAndInOrder)
{
  InputAdapter adapter;
  std::vector<int> arrived;
  adapter.onPanelKey = [&arrived] (PanelCell cell, bool) {
    arrived.push_back (cell.col);
  };

  constexpr int burst = 100;
  for (int col = 0; col < burst; ++col)
    adapter.inputPanelKey ({ 0, col }, true);

  adapter.timerCallback ();

  ASSERT_EQ (arrived.size (), static_cast<std::size_t> (burst));
  for (int col = 0; col < burst; ++col)
    EXPECT_EQ (arrived[static_cast<std::size_t> (col)], col);
}

// Worse than a lost press: a lost release. The adapter marks each pad's state
// as sent whether or not the message fitted, so a dropped release left the
// pad held in the UI until it was pressed again.
TEST (AdapterInput, NoPadIsLeftHeldAfterABurstOfPressesAndReleases)
{
  InputAdapter adapter;

  for (index_t channel = 0; channel < adapter.getNumChannels (); ++channel)
    for (index_t pad = 0; pad < adapter.getNumPadsPerChannel (); ++pad)
      {
        adapter.inputPadValue (InputAdapter::pad (channel, pad), true);
        adapter.inputPadValue (InputAdapter::pad (channel, pad), false);
      }

  adapter.timerCallback ();

  for (index_t channel = 0; channel < adapter.getNumChannels (); ++channel)
    for (index_t pad = 0; pad < adapter.getNumPadsPerChannel (); ++pad)
      EXPECT_FALSE (static_cast<bool> (adapter.getPad (channel, pad).getValue ()))
          << "channel " << channel << " pad " << pad << " is still held";
}
