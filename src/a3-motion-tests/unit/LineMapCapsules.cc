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

#include <a3-motion-ui/components/LineMapCapsules.hh>

#include <cmath>

using namespace a3;

namespace
{

MapStroke
stroke (std::vector<juce::Point<float> > points, std::vector<bool> lifts,
        float width, juce::Colour colour = juce::Colours::red)
{
  MapStroke s;
  s.points = std::move (points);
  s.lifts = std::move (lifts);
  s.width = width;
  s.colour = colour;
  return s;
}

float
distanceToSegment (float px, float py, CapsuleVertex const &v)
{
  auto const dx = v.bx - v.ax, dy = v.by - v.ay;
  auto const length2 = dx * dx + dy * dy;
  auto t = length2 > 0.f ? ((px - v.ax) * dx + (py - v.ay) * dy) / length2 : 0.f;
  t = std::clamp (t, 0.f, 1.f);
  return std::hypot (px - (v.ax + t * dx), py - (v.ay + t * dy));
}

/** Whether (px, py) lies in one of the two triangles of the quad at `first`. */
bool
quadCovers (std::vector<CapsuleVertex> const &v, std::size_t first, float px,
            float py)
{
  auto const inTriangle = [&] (std::size_t i) {
    auto const sign = [] (float x1, float y1, float x2, float y2, float x3,
                          float y3) {
      return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
    };
    auto const d1 = sign (px, py, v[i].x, v[i].y, v[i + 1].x, v[i + 1].y);
    auto const d2 = sign (px, py, v[i + 1].x, v[i + 1].y, v[i + 2].x, v[i + 2].y);
    auto const d3 = sign (px, py, v[i + 2].x, v[i + 2].y, v[i].x, v[i].y);
    auto const negative = d1 < 0 || d2 < 0 || d3 < 0;
    auto const positive = d1 > 0 || d2 > 0 || d3 > 0;
    return !(negative && positive);
  };
  return inTriangle (first) || inTriangle (first + 3);
}

}

TEST (LineMapCapsules, SixCornersForEverySegmentAndNoneAcrossALiftedPen)
{
  // Three segments drawn, one gap where the pen is lifted.
  auto const vertices = capsuleVertices (
      { stroke ({ { 10, 10 }, { 20, 10 }, { 30, 10 }, { 50, 10 }, { 60, 10 } },
                { true, false, false, true, false }, 4.f) });
  EXPECT_EQ (vertices.size (), 3u * 6u);
}

TEST (LineMapCapsules, EveryCornerKnowsItsSegmentWidthAndColour)
{
  auto const colour = juce::Colour::fromFloatRGBA (0.25f, 0.5f, 0.75f, 1.f);
  auto const vertices = capsuleVertices (
      { stroke ({ { 10, 20 }, { 40, 60 } }, { true, false }, 6.f, colour) });
  ASSERT_EQ (vertices.size (), 6u);
  for (auto const &v : vertices)
    {
      EXPECT_EQ (v.ax, 10.f);
      EXPECT_EQ (v.ay, 20.f);
      EXPECT_EQ (v.bx, 40.f);
      EXPECT_EQ (v.by, 60.f);
      EXPECT_EQ (v.halfWidth, 3.f);
      EXPECT_FLOAT_EQ (v.r, colour.getFloatRed ());
      EXPECT_FLOAT_EQ (v.g, colour.getFloatGreen ());
      EXPECT_FLOAT_EQ (v.b, colour.getFloatBlue ());
    }
}

TEST (LineMapCapsules, TheQuadHoldsTheWholeCapsuleAndItsEdge)
{
  // Every texel the shader could keep -- within half the width plus the one
  // it fades -- has to be inside the quad, or the stroke comes out clipped
  // at its rounded ends and along its sides.
  auto const width = 12.f;
  auto const vertices = capsuleVertices (
      { stroke ({ { 30, 40 }, { 70, 55 } }, { true, false }, width) });
  ASSERT_EQ (vertices.size (), 6u);

  for (auto x = 0.f; x <= 100.f; x += 0.5f)
    for (auto y = 0.f; y <= 100.f; y += 0.5f)
      if (distanceToSegment (x, y, vertices[0]) <= width * 0.5f + 1.f)
        EXPECT_TRUE (quadCovers (vertices, 0, x, y)) << x << ", " << y;
}

TEST (LineMapCapsules, APointWithNoLengthIsStillARoundDot)
{
  // Two identical points joined: a segment of no length, drawn by a round
  // cap as a dot. The quad must still hold it.
  auto const vertices = capsuleVertices (
      { stroke ({ { 50, 50 }, { 50, 50 } }, { true, false }, 8.f) });
  ASSERT_EQ (vertices.size (), 6u);
  for (auto angle = 0.f; angle < 6.3f; angle += 0.1f)
    EXPECT_TRUE (quadCovers (vertices, 0, 50.f + 4.9f * std::cos (angle),
                             50.f + 4.9f * std::sin (angle)));
}

TEST (LineMapCapsules, TheStrokesKeepTheirPaintingOrder)
{
  auto const vertices = capsuleVertices (
      { stroke ({ { 0, 0 }, { 10, 0 } }, { true, false }, 20.f,
                juce::Colours::blue),
        stroke ({ { 0, 0 }, { 10, 0 } }, { true, false }, 4.f,
                juce::Colours::red) });
  ASSERT_EQ (vertices.size (), 12u);
  EXPECT_EQ (vertices[0].halfWidth, 10.f);
  EXPECT_EQ (vertices[6].halfWidth, 2.f);
}
