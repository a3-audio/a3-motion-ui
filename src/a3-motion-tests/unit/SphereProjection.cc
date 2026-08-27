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

#include <a3-motion-ui/components/SphereProjection.hh>

#include <cmath>

using namespace a3;

namespace
{

// The display is an orthographic view of the upper hemisphere from above, and
// a blob is drawn by dropping z. So a drag has to invert exactly that, or the
// blob lands somewhere other than where the finger is — which is what dragging
// through the height map's pattern coordinates did: the finger's radius was
// read as a pattern radius, and 1.0 there is 45 degrees off the zenith rather
// than the horizon.

TEST (SphereProjection, TheCentreOfTheDiscIsStraightUp)
{
  auto const direction = discToDirection (Pos::fromCartesian (0.f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 1.f, 0.001f);
}

TEST (SphereProjection, TheRimIsTheHorizon)
{
  auto const direction = discToDirection (Pos::fromCartesian (1.f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 0.f, 0.001f);
  EXPECT_NEAR (direction.x (), 1.f, 0.001f);
}

TEST (SphereProjection, HalfWayOutIsSixtyDegreesUp)
{
  auto const direction = discToDirection (Pos::fromCartesian (0.5f, 0.f, 0.f));

  EXPECT_NEAR (direction.elevation (), 60.f, 0.5f);
}

// The whole point: what a drag writes and what the drawing reads back have to
// be the same place.
TEST (SphereProjection, DiscToDirectionAndBackIsTheIdentity)
{
  for (auto const point : { std::make_pair (0.3f, 0.2f),
                            std::make_pair (-0.7f, 0.1f),
                            std::make_pair (0.f, -0.95f) })
    {
      auto const flat = Pos::fromCartesian (point.first, point.second, 0.f);
      auto const back = directionToDisc (discToDirection (flat));

      EXPECT_NEAR (back.x (), point.first, 0.001f);
      EXPECT_NEAR (back.y (), point.second, 0.001f);
    }
}

TEST (SphereProjection, AzimuthSurvivesTheTrip)
{
  auto const flat = Pos::fromCartesian (0.4f, -0.6f, 0.f);
  auto const direction = discToDirection (flat);

  EXPECT_NEAR (direction.azimuth (), flat.azimuth (), 0.5f);
}

// A finger outside the sphere has no direction below the horizon to point at,
// so it is held at the rim rather than producing a NaN.
TEST (SphereProjection, BeyondTheRimIsHeldAtTheHorizon)
{
  auto const direction = discToDirection (Pos::fromCartesian (1.6f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 0.f, 0.001f);
  EXPECT_NEAR (std::hypot (direction.x (), direction.y ()), 1.f, 0.001f);
}

}
