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
  // 1280 divides evenly; 1283 leaves a remainder of 3 for the last strip.
  for (auto const width : { 1280, 1283 })
    {
      juce::Rectangle<int> const area{ 0, 40, width, 760 };
      auto const l = fpvLayout (area, gap);
      auto const equal = (width - 3 * gap) / 4;
      for (int ch = 0; ch < 4; ++ch)
        {
          auto const &s = l.strips[static_cast<size_t> (ch)].whole;
          EXPECT_GE (s.getY (), l.sphere.getBottom ());
          EXPECT_EQ (s.getBottom (), area.getBottom ());
          if (ch < 3)
            EXPECT_EQ (s.getWidth (), equal) << width;
          else
            {
              EXPECT_GE (s.getWidth (), equal) << width;
              EXPECT_LE (s.getWidth (), equal + 3) << width;
            }
          if (ch > 0)
            EXPECT_EQ (s.getX (),
                       l.strips[static_cast<size_t> (ch - 1)].whole.getRight ()
                           + gap);
        }
      EXPECT_EQ (l.strips[3].whole.getRight (), area.getRight ());
    }
}

TEST (FpvLayout, ATinyAreaNeverGivesANegativeWidth)
{
  juce::Rectangle<int> const row{ 0, 0, 8, 30 };
  auto const strips = fpvStripRow (row, gap);
  for (auto const &s : strips)
    {
      EXPECT_GE (s.whole.getWidth (), 0);
      EXPECT_TRUE (row.contains (s.whole));
    }
  auto const l = fpvLayout ({ 0, 0, 8, 90 }, gap);
  for (auto const &s : l.strips)
    EXPECT_GE (s.whole.getWidth (), 0);
}

TEST (FpvLayout, AStripsSectionsStackTopToBottomAndFillIt)
{
  // The default screen, and an odd small height that the fractions round on.
  for (auto const area : { screen, juce::Rectangle<int>{ 0, 0, 203, 97 } })
    for (auto const &s : fpvLayout (area, gap).strips)
      {
        EXPECT_EQ (s.header.getY (), s.whole.getY ());
        EXPECT_EQ (s.clip.getY (), s.header.getBottom ());
        EXPECT_EQ (s.instruments.getY (), s.clip.getBottom ());
        EXPECT_EQ (s.meter.getY (), s.instruments.getBottom ());
        EXPECT_EQ (s.meter.getBottom (), s.whole.getBottom ());
        for (auto const &r : { s.header, s.clip, s.instruments, s.meter })
          {
            EXPECT_EQ (r.getWidth (), s.whole.getWidth ());
            EXPECT_EQ (r.getX (), s.whole.getX ());
          }
      }
}

TEST (FpvLayout, TheStripRowAloneSplitsLikeTheBottomThird)
{
  auto const l = fpvLayout (screen, gap);
  auto const row = l.strips[0].whole.getUnion (l.strips[3].whole);
  auto const again = fpvStripRow (row, gap);
  for (size_t i = 0; i < 4; ++i)
    {
      EXPECT_EQ (again[i].whole, l.strips[i].whole) << i;
      EXPECT_EQ (again[i].header, l.strips[i].header) << i;
      EXPECT_EQ (again[i].meter, l.strips[i].meter) << i;
    }
}

TEST (FpvLayout, AnEmptyAreaGivesEmptyRectangles)
{
  auto const l = fpvLayout ({}, gap);
  EXPECT_TRUE (l.sphere.isEmpty ());
  EXPECT_TRUE (l.strips[0].whole.isEmpty ());
}
