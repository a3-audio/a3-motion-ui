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

#include <a3-motion-ui/components/BarKnob.hh>

using namespace a3;

namespace
{

using Pieces = std::vector<std::pair<float, float> >;
constexpr float scale = 2.356f; // 135 degrees, a knob with ends
constexpr float ring = 3.1416f; // half a turn each way

}

TEST (BarKnob, AModulationAboveTheValueRunsUpFromThePointer)
{
  EXPECT_EQ (modulationArcs (0.2f, 1.f, scale, false), (Pieces{ { 0.2f, 1.f } }));
}

TEST (BarKnob, AModulationBelowTheValueRunsDownFromThePointer)
{
  // What #35 was about: sqzX swinging below its setting drew from the start
  // of the scale to the reach, and from the pointer to the end.
  EXPECT_EQ (modulationArcs (0.f, -0.8f, scale, false),
             (Pieces{ { -0.8f, 0.f } }));
}

TEST (BarKnob, ARingThatHasGoneRoundTheTopIsTwoPieces)
{
  EXPECT_EQ (modulationArcs (2.5f, -2.9f, ring, true),
             (Pieces{ { 2.5f, ring }, { -ring, -2.9f } }));
}

TEST (BarKnob, ARingStillMovingForwardIsOnePiece)
{
  EXPECT_EQ (modulationArcs (-1.f, 0.5f, ring, true),
             (Pieces{ { -1.f, 0.5f } }));
}

TEST (BarKnob, NoMovementIsNoArc)
{
  EXPECT_TRUE (modulationArcs (0.4f, 0.4f, scale, false).empty ());
  EXPECT_TRUE (modulationArcs (0.4f, 0.4f, ring, true).empty ());
}
