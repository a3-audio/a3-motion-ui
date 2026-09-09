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

#include <a3-motion-ui/components/BarFader.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{
constexpr ControlMetrics metrics{ fingertipSize, 12.f, 12.f };

juce::Rectangle<int>
aStrip ()
{
  // minimumChannelWidth/minimumMotionHeight are float thresholds (see
  // ControllerLayout.hh), not pixel counts, so they need an explicit
  // conversion here -- a braced list handing a float to Rectangle<int>'s int
  // parameters is a narrowing conversion and does not compile.
  return { 0, 0, juce::roundToInt (minimumChannelWidth),
           juce::roundToInt (minimumMotionHeight * 2) };
}
}

// Up is more, the way every fader in every booth works and the way
// DragAccumulator already counts.
TEST (BarFader, MoreValueSitsHigher)
{
  auto const low = faderGeometry (aStrip (), metrics, 0.1f);
  auto const high = faderGeometry (aStrip (), metrics, 0.9f);

  EXPECT_GT (low.cap.getY (), high.cap.getY ());
}

// The two ends are reachable: at zero and at one the cap is still wholly
// inside the track, or the last bit of the throw would be unhittable.
TEST (BarFader, TheCapStaysInsideTheTrackAtBothEnds)
{
  for (auto fraction : { 0.f, 1.f })
    {
      auto const geometry = faderGeometry (aStrip (), metrics, fraction);
      EXPECT_TRUE (geometry.track.contains (geometry.cap)) << fraction;
    }
}

// Hit in a hurry, in the dark, by a hand that is also doing something else.
TEST (BarFader, TheCapIsNeverSmallerThanAFingertip)
{
  auto const geometry = faderGeometry (aStrip (), metrics, 0.5f);
  EXPECT_GE (geometry.cap.getHeight (), fingertipSize / 2);
  EXPECT_GE (geometry.cap.getWidth (), fingertipSize / 2);
}

// The caption is under the track and does not sit on it -- a word crossed by
// a moving cap is a word you read twice.
TEST (BarFader, TheCaptionSitsUnderTheTrack)
{
  auto const geometry = faderGeometry (aStrip (), metrics, 0.5f);
  EXPECT_GE (geometry.caption.getY (), geometry.track.getBottom ());
  EXPECT_FALSE (geometry.caption.intersects (geometry.track));
}

// Everything inside the bounds it was given, at any aspect: the overlay hands
// this a tall narrow column and the bar's tab a short wide one.
TEST (BarFader, EverythingStaysInsideTheBoundsAtEveryAspect)
{
  for (auto const &bounds :
       { juce::Rectangle<int> (0, 0, minimumChannelWidth,
                               minimumMotionHeight * 3),
         juce::Rectangle<int> (0, 0, minimumChannelWidth * 3,
                               minimumMotionHeight) })
    {
      auto const geometry = faderGeometry (bounds, metrics, 0.5f);
      EXPECT_TRUE (bounds.contains (geometry.track));
      EXPECT_TRUE (bounds.contains (geometry.caption));
    }
}

// resized() is called with an empty rectangle before the window has a size.
TEST (BarFader, AnEmptyBoundsGivesEmptyGeometryAndDoesNotDivideByZero)
{
  auto const geometry = faderGeometry ({}, metrics, 0.5f);
  EXPECT_TRUE (geometry.track.isEmpty ());
}
