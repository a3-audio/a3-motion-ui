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

#include <a3-motion-ui/components/fpv/FpvLayout.hh>

using namespace a3;

namespace
{
juce::Rectangle<int> const screen{ 0, 40, 1280, 760 };
int const gap = 4;
}

TEST (FpvLayout, TheSphereTakesTheTopTwoThirds)
{
  auto const l = fpvLayout (screen, gap);
  EXPECT_EQ (l.sphere.getY (), screen.getY ());
  // The gap between sphere and strips is wanted and comes out of the sphere's
  // two thirds, hence gap + 1.
  EXPECT_NEAR (l.sphere.getHeight (), screen.getHeight () * 2 / 3, gap + 1);
  EXPECT_EQ (l.sphere.getWidth (), screen.getWidth ());
}

TEST (FpvLayout, FourStripsShareTheBottomThirdLeftToRight)
{
  auto const l = fpvLayout (screen, gap);
  for (int ch = 0; ch < 4; ++ch)
    {
      auto const &s = l.strips[ch].whole;
      EXPECT_GE (s.getY (), l.sphere.getBottom ());
      EXPECT_EQ (s.getBottom (), screen.getBottom ());
      EXPECT_NEAR (s.getWidth (), (screen.getWidth () - 3 * gap) / 4, 1);
      if (ch > 0)
        EXPECT_EQ (s.getX (), l.strips[ch - 1].whole.getRight () + gap);
    }
  EXPECT_EQ (l.strips[3].whole.getRight (), screen.getRight ());
}

TEST (FpvLayout, AStripsSectionsStackTopToBottomAndFillIt)
{
  auto const s = fpvLayout (screen, gap).strips[2];
  EXPECT_EQ (s.header.getY (), s.whole.getY ());
  EXPECT_EQ (s.clip.getY (), s.header.getBottom ());
  EXPECT_EQ (s.instruments.getY (), s.clip.getBottom ());
  EXPECT_EQ (s.meter.getY (), s.instruments.getBottom ());
  EXPECT_EQ (s.meter.getBottom (), s.whole.getBottom ());
  for (auto const &r : { s.header, s.clip, s.instruments, s.meter })
    EXPECT_EQ (r.getWidth (), s.whole.getWidth ());
}

TEST (FpvLayout, AnEmptyAreaGivesEmptyRectangles)
{
  auto const l = fpvLayout ({}, gap);
  EXPECT_TRUE (l.sphere.isEmpty ());
  EXPECT_TRUE (l.strips[0].whole.isEmpty ());
}
