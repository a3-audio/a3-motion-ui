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

#include <UiSource.hh>

#include <JuceHeader.h>

// Read from the source, as ChannelValueReset and DeviceHello do: these live
// in the component that owns the engine, which the runner cannot build.

namespace
{
juce::String
bodyOf (juce::String const &signature)
{
  return a3::test::uiComponentBodyOf (signature);
}
}

// A turn of 3D, FREQ or Q redraws the channel values and nothing else (#64).
// It used to describe the shown clip all over again on every step -- the
// elevation figures of every playing clip, the shape's SVG, twenty setters
// over the whole bar -- for a value none of that shows. Touch drags arrive
// at 60-120 Hz, so that was the lag behind the finger. The set is still
// saved: the value is part of it.
TEST (ChannelPotTurn, ATurnRedrawsTheChannelValuesNotTheWholeBar)
{
  auto const body
      = bodyOf ("A3MotionUIComponent::setChannelPotValue (index_t channel,");
  ASSERT_TRUE (body.isNotEmpty ());

  EXPECT_TRUE (body.contains ("refreshChannelValues ()"));
  EXPECT_TRUE (body.contains ("scheduleSetSave ()"));
  EXPECT_FALSE (body.contains ("updateClipSettingsDisplay ()"));
}

// Reaching for a pot on the face already shown selects nothing again (#64).
// Every touch-down on a face pot chose the face, and choosing re-ran
// selectClip(): the bar described afresh and the ACTION page listing its
// files from disk, before the first step of the drag.
TEST (ChannelPotTurn, ReachingForAPotOnTheShownFaceChoosesNothingAgain)
{
  auto const body = bodyOf (
      "A3MotionUIComponent::chooseChannelFace (index_t channel, bool "
      "mayTurnOver)");
  ASSERT_TRUE (body.isNotEmpty ());

  auto const guard = body.indexOf ("!mayTurnOver && faceIsShown");
  auto const select = body.indexOf ("selectClip (");
  ASSERT_GE (guard, 0) << "no early return for the face already shown";
  ASSERT_GE (select, 0);
  EXPECT_LT (guard, select);
}
