/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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

#include <a3-motion-ui/components/ChannelValueReset.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>

using namespace a3;

// Twelve o'clock for all three. The knobs sweep 270 degrees, so the middle of
// the travel is the middle of the range -- and a hand reaching for "put that
// back" is reaching for the same place on all three, not for three different
// ones it would have to remember.
TEST (ChannelValueReset, AllThreeRestAtTwelveOClock)
{
  for (auto const row : { channelRowThreeD, channelRowFreq, channelRowQ })
    {
      auto const rest = channelValueRestPosition (row);
      ASSERT_TRUE (rest.has_value ()) << row;
      EXPECT_FLOAT_EQ (*rest, 0.5f) << row;
    }
}

// A row nobody has decided about writes nothing. The way this goes wrong is a
// fourth row being added to the grid and inheriting whichever value the switch
// happened to fall through to.
TEST (ChannelValueReset, ARowWithNoRestPositionWritesNothing)
{
  EXPECT_FALSE (channelValueRestPosition (-1).has_value ());
  EXPECT_FALSE (channelValueRestPosition (numChannelRows).has_value ());
}

// With the panel answering, these three are physical pots and an encoder pair.
// A value that jumped away from where the pot is standing would be telling the
// truth about neither, and the next hair of pot movement would snatch it back.
TEST (ChannelValueReset, ThePanelOwnsTheValuesWhileItIsAnswering)
{
  EXPECT_FALSE (channelValueResetIsAllowed (true));
}

// Without it the screen is the only way in, so two taps are the only way back.
// "Without it" is answered at runtime, not at build time: this device is built
// with the hardware interface compiled in and still spends sessions with no
// panel on the wire.
TEST (ChannelValueReset, TheScreenMayResetWhenNoPanelAnswers)
{
  EXPECT_TRUE (channelValueResetIsAllowed (false));
}
