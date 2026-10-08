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

#include <JuceHeader.h>

#include <a3-motion-ui/components/RecordingIndicator.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <memory>
#include <vector>

using namespace a3;

// Nothing asked for, nothing running: the indicator is not there at all. It is
// the one mark on the bar that keeps a shape of its own, so it must not stand
// around when there is no take.
TEST (RecordingIndicator, NothingAskedForShowsNothing)
{
  EXPECT_EQ (recordingIndicatorFor (false, false), RecordingIndicator::Off);
}

// Between the key and the downbeat. This is the window the whole thing exists
// for: the hand has pressed REC and now has to know *when* to start moving,
// and until this there was nothing on the bar to count off.
TEST (RecordingIndicator, AskedForButNotYetRunningCountsIn)
{
  EXPECT_EQ (recordingIndicatorFor (true, false),
             RecordingIndicator::CountIn);
}

// Once the take is running the count-in is over, whatever else is pending.
// A blink that carried on under a running take would be the loudest thing on
// the screen saying the least -- and it is the state that writes over
// something that does not come back.
TEST (RecordingIndicator, ARunningTakeEndsTheCountIn)
{
  EXPECT_EQ (recordingIndicatorFor (false, true),
             RecordingIndicator::Running);
  EXPECT_EQ (recordingIndicatorFor (true, true), RecordingIndicator::Running)
      << "a take scheduled behind a running one must not reopen the count-in";
}

// The bar shows the take wherever it runs, not only when its clip is the one
// on screen: one take runs at a time and it writes over something that does
// not come back, so "am I recording?" must not depend on which clip is open.
// The channel is found by the row that holds the take, and its colour is what
// tells the eye which channel it is.
TEST (RecordingIndicator, TheTakeIsFoundOnWhicheverChannelHoldsIt)
{
  auto const take = std::make_shared<int> (0);
  std::vector<std::vector<std::shared_ptr<int> > > rows (4);
  for (auto &row : rows)
    row = { std::make_shared<int> (1), nullptr, std::make_shared<int> (2) };
  rows[2][1] = take;

  EXPECT_EQ (channelHoldingTake (rows, take), 2);
}

// No take, or a take no row holds any more (its slot was cleared under it):
// no channel, so the bar shows nothing rather than a colour that is wrong.
TEST (RecordingIndicator, NoTakeOrAnUnheldTakeHasNoChannel)
{
  std::vector<std::vector<std::shared_ptr<int> > > rows (4);
  for (auto &row : rows)
    row = { std::make_shared<int> (1), nullptr };

  EXPECT_EQ (channelHoldingTake (rows, std::shared_ptr<int>{}), -1)
      << "an empty slot must not match an absent take";
  EXPECT_EQ (channelHoldingTake (rows, std::make_shared<int> (3)), -1);
}

namespace
{
// Pixels of the painted bar that are clearly the take's colour: its own
// channel's colour is what tells the eye which channel is recording, so the
// paint is checked for that colour rather than for "something changed".
int
pixelsNear (juce::Image const &image, juce::Colour colour)
{
  auto count = 0;
  for (auto y = 0; y < image.getHeight (); ++y)
    for (auto x = 0; x < image.getWidth (); ++x)
      {
        auto const p = image.getPixelAt (x, y);
        auto const hueDistance
            = std::abs (p.getHue () - colour.getHue ());
        if (p.getSaturation () > 0.4f
            && juce::jmin (hueDistance, 1.f - hueDistance) < 0.03f)
          ++count;
      }
  return count;
}

juce::Image
paintedBar (float progress, juce::Colour colour)
{
  juce::Value bpm{ 120.0 };
  StatusBar bar{ bpm };
  bar.setBounds (0, 0, 768, bar.preferredHeight ());
  bar.setRecordingProgress (progress, colour);
  return bar.createComponentSnapshot (bar.getLocalBounds ());
}
}

// The fill paints in the colour it is handed -- the recording channel's --
// and grows with the take. A second channel's colour lands as that colour,
// which is the whole of "which channel is recording" on the bar.
TEST (RecordingIndicator, TheFillPaintsInTheTakesChannelColour)
{
  auto const magenta = juce::Colour (0xffff00ff);
  auto const green = juce::Colour (0xff00ff00);

  auto const none = pixelsNear (paintedBar (-1.f, magenta), magenta);
  auto const half = pixelsNear (paintedBar (0.5f, magenta), magenta);
  auto const full = pixelsNear (paintedBar (1.f, magenta), magenta);

  EXPECT_EQ (none, 0) << "no take, no fill";
  EXPECT_GT (half, 0);
  EXPECT_GT (full, half) << "the fill grows with the take";

  EXPECT_GT (pixelsNear (paintedBar (1.f, green), green), 0)
      << "another channel's take paints in that channel's colour";
  EXPECT_EQ (pixelsNear (paintedBar (1.f, green), magenta), 0);
}
