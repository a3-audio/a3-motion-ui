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

#include <map>
#include <utility>

#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>

using namespace a3;

namespace
{
/** An adapter with no hardware: it records what would go to the panel. The
 *  thread is never started; the test drives processOutput() itself, which is
 *  what the thread's loop does between polls. */
class RecordingAdapter : public InputOutputAdapter
{
public:
  using InputOutputAdapter::processOutput;

  void
  setPadLED (index_t channel, index_t pad, juce::Colour colour)
  {
    auto &value = getPadLED (channel, pad);
    value = juce::VariantConverter<juce::Colour>::toVar (colour);
    // The Value notifies asynchronously; the message loop is not run here.
    valueChanged (value);
  }

  std::map<int, juce::Colour> keys;
  std::map<std::pair<index_t, index_t>, juce::Colour> pads;

private:
  void processInput () override {}

  void
  outputButtonLED (Button button, juce::Colour colour) override
  {
    keys[static_cast<int> (button)] = colour;
  }

  void
  outputPadLED (PadIndex padIndex, juce::Colour colour) override
  {
    pads[{ padIndex.channel, padIndex.pad }] = colour;
  }
};

auto const lit = juce::Colour::fromRGB (40, 90, 200);
}

// At start the UI paints the whole panel in one go, faster than the adapter's
// thread takes a turn. 41 LEDs went into a FIFO of 31; the rest were dropped,
// a different handful each start, and never sent again.
TEST (AdapterLedOutput, AWholePanelPaintedAtOnceReachesTheOutput)
{
  RecordingAdapter adapter;

  for (int key = 0; key < numEndKeys; ++key)
    adapter.setButtonLED (static_cast<EndKey> (key), lit);
  for (index_t channel = 0; channel < adapter.getNumChannels (); ++channel)
    for (index_t pad = 0; pad < adapter.getNumPadsPerChannel (); ++pad)
      adapter.setPadLED (channel, pad, lit);

  adapter.processOutput ();

  EXPECT_EQ (adapter.keys.size (), static_cast<std::size_t> (numEndKeys));
  EXPECT_EQ (adapter.pads.size (),
             static_cast<std::size_t> (adapter.getNumChannels ()
                                       * adapter.getNumPadsPerChannel ()));
}

TEST (AdapterLedOutput, APadChangedTwiceBeforeAPollShowsItsLastColour)
{
  RecordingAdapter adapter;

  adapter.setPadLED (2, 5, juce::Colours::red);
  adapter.setPadLED (2, 5, lit);
  adapter.processOutput ();

  ASSERT_EQ (adapter.pads.count ({ 2, 5 }), 1u);
  EXPECT_EQ (adapter.pads.at ({ 2, 5 }), lit);
}
