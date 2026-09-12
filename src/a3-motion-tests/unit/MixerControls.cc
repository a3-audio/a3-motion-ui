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
//
// SEND joined on 2026-09-12, after VOL rather than at the top where the desk
// has it: the top of the strip is the set-once end, the foot is where the
// hand goes during a set, and a send is a gesture rather than a setting.
TEST (MixerControls, AStripReadsGainEqVolumeSendThenTheTwoKeys)
{
  EXPECT_EQ (mixerControlOrder[0], MixerControl::Gain);
  EXPECT_EQ (mixerControlOrder[1], MixerControl::EqHigh);
  EXPECT_EQ (mixerControlOrder[2], MixerControl::EqMid);
  EXPECT_EQ (mixerControlOrder[3], MixerControl::EqLow);
  EXPECT_EQ (mixerControlOrder[4], MixerControl::Volume);
  EXPECT_EQ (mixerControlOrder[5], MixerControl::FxSend);
  EXPECT_EQ (mixerControlOrder[6], MixerControl::Pfl);
  EXPECT_EQ (mixerControlOrder[7], MixerControl::Fx);
  EXPECT_EQ (numMixerControls, 8);
}

// The send is turned, not pressed -- and the layout takes the toggles off the
// end of the order without knowing which they are, so a continuous control
// that drifted behind them would be drawn as a key.
TEST (MixerControls, TheSendIsTurnedAndComesBeforeTheKeys)
{
  EXPECT_FALSE (mixerControlIsAToggle (MixerControl::FxSend));
  EXPECT_TRUE (mixerControlIsAToggle (mixerControlOrder[6]));
  EXPECT_TRUE (mixerControlIsAToggle (mixerControlOrder[7]));
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

// The same for the other two tables, and for the same reason plus one: the
// address arrays are indexed by these positions, so a control listed twice
// takes two addresses and leaves the control it displaced with none. That
// control is then unreachable and one of the two duplicates sends the wrong
// address -- and every length still agrees, so nothing else here notices.
TEST (MixerControls, EveryMasterControlAppearsExactlyOnce)
{
  std::set<MasterControl> seen;
  for (auto control : masterControlOrder)
    EXPECT_TRUE (seen.insert (control).second) << "listed twice";

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numMasterControls));
}

TEST (MixerControls, EveryFilterControlAppearsExactlyOnce)
{
  std::set<FilterControl> seen;
  for (auto control : filterControlOrder)
    EXPECT_TRUE (seen.insert (control).second) << "listed twice";

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numFilterControls));
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

// Two taps put a control back. Only SEND has somewhere to go back to, and
// that is the point of asking per control rather than resetting everything.
//
// GAIN and VOL deliberately have no rest position. Zero on either is a mute,
// and a mute two fingertips away from a control that is dragged all evening
// is a way to silence the room by accident. SEND is the one where zero is
// unambiguous and safe: take the effect out.
TEST (MixerControls, OnlyTheSendHasARestPosition)
{
  EXPECT_TRUE (mixerControlRestPosition (MixerControl::FxSend).has_value ());
  EXPECT_FLOAT_EQ (*mixerControlRestPosition (MixerControl::FxSend), 0.f);

  for (auto const control : mixerControlOrder)
    if (control != MixerControl::FxSend)
      EXPECT_FALSE (mixerControlRestPosition (control).has_value ())
          << mixerControlLabel (control);
}

// A control nobody has decided about writes nothing rather than falling
// through to a plausible number. The way this goes wrong is an eighth control
// arriving and inheriting whichever value the switch happened to reach.
TEST (MixerControls, AnUndecidedControlHasNoRestPosition)
{
  EXPECT_FALSE (mixerControlRestPosition (static_cast<MixerControl> (99))
                    .has_value ());
}
