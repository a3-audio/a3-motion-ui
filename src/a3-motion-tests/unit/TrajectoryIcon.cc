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

#include <a3-motion-ui/components/TrajectoryIcon.hh>

using namespace a3;

namespace
{
/** The whole normalised square the icon data lives in. */
juce::Rectangle<float> const wholeBox{ -1.f, -1.f, 2.f, 2.f };
}

/** A box wider than it is tall is what the Shape section actually gives the
 *  picture, and the radius used to come from the width alone -- so the figure
 *  ran out of the top and the bottom of it, over the word naming the section
 *  and into the clip field underneath. */
TEST (TrajectoryIcon, ItFitsTheShortSideOfTheBoxAsWellAsTheLong)
{
  juce::Rectangle<float> const wide{ 0.f, 0.f, 200.f, 80.f };

  auto const r = trajectoryIconRadius (wide, wholeBox);

  EXPECT_LE (r * 2.f, wide.getHeight ())
      << "the picture is taller than the box it is drawn in";
  EXPECT_GT (r, 0.f);
}

/** And a turned figure reaches further than an upright one: a square turned an
 *  eighth of a revolution reaches sqrt(2), not 1. The radius has to come off
 *  what the figure actually covers once it is turned, not off the figure it
 *  would have been. */
TEST (TrajectoryIcon, ATurnedFigureIsGivenLessRoom)
{
  juce::Rectangle<float> const square{ 0.f, 0.f, 100.f, 100.f };

  auto const upright = trajectoryIconRadius (square, wholeBox);

  // What a square covers once it is turned an eighth: sqrt(2) each way.
  auto const corner = std::sqrt (2.f);
  juce::Rectangle<float> const turned{ -corner, -corner, corner * 2.f,
                                       corner * 2.f };
  auto const leant = trajectoryIconRadius (square, turned);

  EXPECT_LT (leant, upright) << "a turned figure was given the same room";
  EXPECT_NEAR (leant * corner, upright, 0.5f)
      << "and exactly as much less as the turn costs";
}

/** A figure that only uses part of its box gets the room back. Normalising to
 *  a bounding box means most do use all of it, but a shape drawn inside one --
 *  a small orbit in the middle of the square -- should not be shrunk twice. */
TEST (TrajectoryIcon, AFigureThatDoesNotFillItsBoxIsNotShrunk)
{
  juce::Rectangle<float> const square{ 0.f, 0.f, 100.f, 100.f };
  juce::Rectangle<float> const half{ -0.5f, -0.5f, 1.f, 1.f };

  EXPECT_GT (trajectoryIconRadius (square, half),
             trajectoryIconRadius (square, wholeBox));
}

/** Nothing sensible to divide by, and nothing drawn: a radius of zero rather
 *  than a division by one. */
TEST (TrajectoryIcon, AnEmptyExtentAsksForNothing)
{
  juce::Rectangle<float> const square{ 0.f, 0.f, 100.f, 100.f };

  EXPECT_EQ (trajectoryIconRadius (square, {}), 0.f);
  EXPECT_EQ (trajectoryIconRadius ({}, wholeBox), 0.f);
}
