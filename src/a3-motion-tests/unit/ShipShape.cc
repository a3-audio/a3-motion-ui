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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-ui/components/fpv/ShipShape.hh>

using namespace a3;

namespace
{
float const pi = juce::MathConstants<float>::pi;
}

TEST (ShipShape, AShipPointsAlongItsMovement)
{
  ShipHeading heading;
  heading.update ({ 100.f, 100.f }, 1.f);
  heading.update ({ 110.f, 100.f }, 1.f); // moved right
  EXPECT_NEAR (heading.radians (), pi / 2.f, 1e-4f);

  heading.update ({ 110.f, 90.f }, 1.f); // moved up
  EXPECT_NEAR (heading.radians (), 0.f, 1e-4f);
}

TEST (ShipShape, AShipStandingStillKeepsItsHeading)
{
  ShipHeading heading;
  heading.update ({ 0.f, 0.f }, 1.f);
  heading.update ({ 0.f, 10.f }, 1.f);   // moved down
  heading.update ({ 0.2f, 10.1f }, 1.f); // jitter below the step
  EXPECT_NEAR (heading.radians (), pi, 1e-4f);
}

TEST (ShipShape, AShipThatVanishedDoesNotJumpItsHeadingOnReturn)
{
  ShipHeading heading;
  heading.update ({ 0.f, 0.f }, 1.f);
  heading.update ({ 10.f, 0.f }, 1.f); // right
  heading.lose ();
  heading.update ({ -500.f, 300.f }, 1.f); // reappears far away
  EXPECT_NEAR (heading.radians (), pi / 2.f, 1e-4f)
      << "the first point after a loss only anchors";
}

TEST (ShipShape, TheNoseIsAheadAlongTheHeading)
{
  auto const path = shipPath ({ 50.f, 50.f }, 0.f, 20.f);
  auto const box = path.getBounds ();
  EXPECT_NEAR (box.getY (), 40.f, 0.01f) << "nose half a length up";
  EXPECT_NEAR (box.getWidth (), 20.f * shipWidthOfLength, 0.01f);
  EXPECT_TRUE (path.contains (50.f, 45.f));
}

TEST (ShipShape, TurningTheShipTurnsTheNose)
{
  auto const box = shipPath ({ 50.f, 50.f }, pi / 2.f, 20.f).getBounds ();
  EXPECT_NEAR (box.getRight (), 60.f, 0.01f) << "nose half a length right";
}
