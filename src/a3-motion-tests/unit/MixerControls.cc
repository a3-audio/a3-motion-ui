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

#include <cstring>
#include <set>

#include <a3-motion-ui/components/MixerControls.hh>

using namespace a3;

// The order the maintainer asked for, top to bottom on a strip: what a hand
// coming from a mixer expects to find, in the order it is found there.
TEST (MixerControls, AStripReadsGainEqVolumeThenTheTwoKeys)
{
  EXPECT_EQ (mixerControlOrder[0], MixerControl::Gain);
  EXPECT_EQ (mixerControlOrder[1], MixerControl::EqHigh);
  EXPECT_EQ (mixerControlOrder[2], MixerControl::EqMid);
  EXPECT_EQ (mixerControlOrder[3], MixerControl::EqLow);
  EXPECT_EQ (mixerControlOrder[4], MixerControl::Volume);
  EXPECT_EQ (mixerControlOrder[5], MixerControl::Pfl);
  EXPECT_EQ (mixerControlOrder[6], MixerControl::Fx);
  EXPECT_EQ (numMixerControls, 7);
}

// One table, two arrangements. A control listed twice would take two places
// in the overlay and leave the strip a hole; one missing would be reachable
// in one view and not the other.
TEST (MixerControls, EveryControlAppearsExactlyOnce)
{
  std::set<MixerControl> seen;
  for (auto control : mixerControlOrder)
    EXPECT_TRUE (seen.insert (control).second) << "listed twice";

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numMixerControls));
}

// Five continuous, two keys, and the keys are last -- so a layout can take
// the toggles off the end of the strip without knowing which they are.
TEST (MixerControls, TheTogglesAreTheLastTwoAndNothingBefore)
{
  auto seenAToggle = false;
  for (auto control : mixerControlOrder)
    {
      if (mixerControlIsAToggle (control))
        seenAToggle = true;
      else
        EXPECT_FALSE (seenAToggle)
            << "a continuous control after a toggle breaks the split";
    }

  EXPECT_TRUE (mixerControlIsAToggle (MixerControl::Pfl));
  EXPECT_TRUE (mixerControlIsAToggle (MixerControl::Fx));
  EXPECT_FALSE (mixerControlIsAToggle (MixerControl::Volume));
}

// Read in the dark, at a glance, in a column 100 px wide at its narrowest.
// Four characters is what fits; anything longer is drawn clipped, which reads
// as a fault rather than as an abbreviation.
TEST (MixerControls, EveryLabelIsShortEnoughToBeRead)
{
  for (auto control : mixerControlOrder)
    {
      auto const *label = mixerControlLabel (control);
      ASSERT_NE (label, nullptr);
      EXPECT_GT (std::strlen (label), 0u);
      EXPECT_LE (std::strlen (label), 4u) << label;
    }
}

// The master section and the filter are their own short lists, so the overlay
// does not spell them out either.
TEST (MixerControls, TheMasterSectionIsFiveAndTheFilterIsThree)
{
  EXPECT_EQ (numMasterControls, 5);
  EXPECT_EQ (masterControlOrder[0], MasterControl::Volume);
  EXPECT_EQ (masterControlOrder[4], MasterControl::Return);

  EXPECT_EQ (numFilterControls, 3);
  EXPECT_EQ (filterControlOrder[0], FilterControl::Mode);
}

// Read at compile time, because the arrays that hold the values are sized
// from these tables and indexed by these positions.
TEST (MixerControls, ThePositionsAreKnownAtCompileTime)
{
  static_assert (controlSlot (MixerControl::Gain) == 0);
  static_assert (controlSlot (MixerControl::Fx) == numMixerControls - 1);
  static_assert (controlSlot (MasterControl::Volume) == 0);
  static_assert (controlSlot (FilterControl::Mode) == 0);

  for (auto control : mixerControlOrder)
    EXPECT_GE (controlSlot (control), 0);
  SUCCEED ();
}
