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

#include <a3-motion-ui/components/LineMapStrokes.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <algorithm>
#include <cmath>
#include <set>

using namespace a3;

namespace
{

// The stroke list is what the GPU paints into the line map
// (a3-motion-ui#34). These tests pin down the rules the maps followed while
// they were stroked in software, so the GPU pass inherits them instead of
// re-deriving them.

ProjectedLine
straightLine (int count, float depth = 1.f)
{
  ProjectedLine line;
  for (auto i = 0; i < count; ++i)
    {
      auto const t = static_cast<float> (i) / static_cast<float> (count - 1);
      line.points.push_back ({ -0.8f + 1.6f * t, 0.1f });
      line.depth.push_back (depth);
      line.startsRun.push_back (i == 0);
    }
  return line;
}

bool
isCore (MapStroke const &stroke)
{
  return std::abs (stroke.width - lineMapCoreWidth * lineMapTexels) < 1e-6f;
}

juce::uint8
asStored (float value)
{
  return juce::Colour::fromFloatRGBA (value, 0.f, 0.f, 1.f).getRed ();
}

}

TEST (LineMapStrokes, TheMapCoversTheExtentAroundTheCentre)
{
  auto const half = static_cast<float> (lineMapSize) * 0.5f;
  EXPECT_EQ (toLineMap ({ 0.f, 0.f }), juce::Point<float> (half, half));
  EXPECT_EQ (toLineMap ({ lineMapExtent, -lineMapExtent }),
             juce::Point<float> (static_cast<float> (lineMapSize), 0.f));
}

TEST (LineMapStrokes, FewerThanTwoPointsPaintNothing)
{
  EXPECT_TRUE (lineMapStrokes ({}).empty ());
  EXPECT_TRUE (lineMapStrokes (straightLine (1)).empty ());
}

TEST (LineMapStrokes, TheConeIsPaintedWidestFirstAndTheCoreOverIt)
{
  auto const strokes = lineMapStrokes (straightLine (400));
  ASSERT_FALSE (strokes.empty ());

  auto const firstCore
      = std::find_if (strokes.begin (), strokes.end (), isCore);
  ASSERT_NE (firstCore, strokes.end ());
  EXPECT_TRUE (std::all_of (firstCore, strokes.end (), isCore))
      << "nothing of the cone may be painted over the core";

  auto const cone = std::vector<MapStroke> (strokes.begin (), firstCore);
  EXPECT_TRUE (std::is_sorted (cone.begin (), cone.end (),
                               [] (MapStroke const &a, MapStroke const &b) {
                                 return a.width > b.width;
                               }))
      << "a narrower step has to land on top of a wider one";
  EXPECT_EQ (cone.front ().width, lineMapSteps[0].width * lineMapTexels);
}

TEST (LineMapStrokes, EveryConeStepCarriesItsNearness)
{
  std::set<juce::uint8> expected, painted;
  for (auto const &step : lineMapSteps)
    expected.insert (asStored (step.nearness));
  for (auto const &stroke : lineMapStrokes (straightLine (400)))
    if (!isCore (stroke))
      painted.insert (stroke.colour.getRed ());
  EXPECT_EQ (painted, expected);
}

TEST (LineMapStrokes, TheCoreSaysWhereAlongTheFigureItIs)
{
  std::vector<float> along;
  for (auto const &stroke : lineMapStrokes (straightLine (400)))
    if (isCore (stroke))
      {
        EXPECT_EQ (stroke.colour.getRed (), asStored (lineMapCoreNearness));
        along.push_back (stroke.colour.getFloatGreen ());
      }
  ASSERT_GT (along.size (), 10u);
  EXPECT_TRUE (std::is_sorted (along.begin (), along.end ()));
  EXPECT_LT (along.front (), 0.05f);
  EXPECT_GT (along.back (), 0.95f);
}

TEST (LineMapStrokes, DepthIsTheFaintestFadeInThePiece)
{
  auto line = straightLine (400);
  line.depth[5] = -0.9f; // one point far behind, in the first piece of each
  auto const strokes = lineMapStrokes (line);
  ASSERT_FALSE (strokes.empty ());
  EXPECT_EQ (strokes.front ().colour.getBlue (),
             juce::Colour::fromFloatRGBA (0.f, 0.f, lineDepthFade (-0.9f), 1.f)
                 .getBlue ());
}

TEST (LineMapStrokes, APenLiftedInTheLineStaysLifted)
{
  auto line = straightLine (400);
  line.startsRun[200] = true;
  auto const lifted = toLineMap (line.points[200]);

  auto found = false;
  for (auto const &stroke : lineMapStrokes (line))
    if (isCore (stroke))
      for (std::size_t i = 0; i < stroke.points.size (); ++i)
        if (stroke.points[i] == lifted)
          {
            EXPECT_TRUE (stroke.lifts[i]);
            found = true;
          }
  EXPECT_TRUE (found);
}

TEST (LineMapStrokes, EveryStrokeStartsWithItsPenDownAndHasALine)
{
  for (auto const &stroke : lineMapStrokes (straightLine (400)))
    {
      ASSERT_EQ (stroke.points.size (), stroke.lifts.size ());
      ASSERT_GE (stroke.points.size (), 2u);
      EXPECT_TRUE (stroke.lifts.front ());
      EXPECT_EQ (stroke.colour.getAlpha (), 255);
    }
}

// ── The braid and its strand map ────────────────────────────────

namespace
{

SheathRing const plainBraid{ 0.f, 3.f, 0.f, 1 };

std::size_t
pointsIn (BraidCord const &cord)
{
  std::size_t n = 0;
  for (auto const &band : cord.pieces)
    for (auto const &piece : band)
      n += piece.points.size ();
  return n;
}

}

TEST (LineMapStrokes, APlainLineIsOnePieceInTheMiddleTier)
{
  // No braid: every sample has depth 0, which is the middle of five tiers,
  // and a line wholly in front of the ball is the front band.
  auto const cord = braidCord (straightLine (50, 0.9f), plainBraid, 0.f);
  auto const &piece = cord.pieces[3][2];
  ASSERT_EQ (piece.points.size (), 51u) << "a start and fifty points drawn";
  EXPECT_TRUE (piece.lifts.front ());
  EXPECT_EQ (pointsIn (cord), piece.points.size ());
}

TEST (LineMapStrokes, ABandChangeCutsAndStitchesToTheLastPoint)
{
  auto line = straightLine (50, 0.9f);
  for (auto i = 25; i < 50; ++i)
    line.depth[static_cast<std::size_t> (i)] = -0.9f;
  auto const cord = braidCord (line, plainBraid, 0.f);

  auto const &front = cord.pieces[3][2];
  auto const &back = cord.pieces[0][2];
  ASSERT_FALSE (back.points.empty ());
  EXPECT_TRUE (back.lifts.front ());
  EXPECT_EQ (back.points.front (), front.points.back ())
      << "the back piece starts where the front one stopped, so no gap";
}

TEST (LineMapStrokes, ALiftedPenStaysLiftedInTheBraid)
{
  auto line = straightLine (50, 0.9f);
  line.startsRun[20] = true;
  // Held, not referenced through the call: a reference into the returned
  // cord dangled, and the test passed or failed by chance.
  auto const cord = braidCord (line, plainBraid, 0.f);
  auto const &piece = cord.pieces[3][2];
  auto const lifted = line.points[20];
  auto found = false;
  for (std::size_t i = 0; i < piece.points.size (); ++i)
    if (piece.points[i] == lifted && piece.lifts[i])
      found = true;
  EXPECT_TRUE (found);
}

TEST (LineMapStrokes, StrandsWithARadiusLeaveTheLine)
{
  SheathRing const braid{ 0.02f, 3.f, 0.f, 3 };
  auto const line = straightLine (50, 0.9f);
  auto const cord = braidCord (line, braid, 0.f);
  EXPECT_GE (pointsIn (cord), 3u * 50u);

  auto off = 0;
  for (auto const &band : cord.pieces)
    for (auto const &piece : band)
      for (auto const &p : piece.points)
        if (std::abs (p.y - line.points[0].y) > 1e-4f)
          ++off;
  EXPECT_GT (off, 0);
}

TEST (LineMapStrokes, TheStrandMapIsPaintedBackTiersFirst)
{
  SheathRing const braid{ 0.02f, 3.f, 0.f, 3 };
  auto const strokes
      = strandMapStrokes (braidCord (straightLine (200, 0.9f), braid, 0.3f));
  ASSERT_FALSE (strokes.empty ());

  std::vector<juce::uint8> fronts;
  for (auto const &stroke : strokes)
    {
      EXPECT_EQ (stroke.width, 1.7f * strandMapTexels);
      EXPECT_EQ (stroke.colour.getRed (), 255);
      EXPECT_EQ (stroke.colour.getBlue (), 0);
      fronts.push_back (stroke.colour.getGreen ());
    }
  EXPECT_TRUE (std::is_sorted (fronts.begin (), fronts.end ()))
      << "a nearer tier has to land on a farther one";
}

TEST (LineMapStrokes, TheStrandMapHasTheLineMapsExtentOnAFinerGrid)
{
  auto const half = static_cast<float> (strandMapSize) * 0.5f;
  EXPECT_EQ (toStrandMap ({ 0.f, 0.f }), juce::Point<float> (half, half));
  EXPECT_EQ (toStrandMap ({ lineMapExtent, lineMapExtent }),
             juce::Point<float> (static_cast<float> (strandMapSize),
                                 static_cast<float> (strandMapSize)));
}

TEST (LineMapStrokes, APathOfPointsLiftsThePenWhereItSays)
{
  auto const path = pathOf ({ { 0, 0 }, { 10, 0 }, { 20, 5 }, { 30, 5 } },
                            { true, false, true, false });
  juce::Path::Iterator it (path);
  std::vector<juce::Path::Iterator::PathElementType> kinds;
  while (it.next ())
    kinds.push_back (it.elementType);
  using E = juce::Path::Iterator;
  EXPECT_EQ (kinds, (std::vector<juce::Path::Iterator::PathElementType>{
                        E::startNewSubPath, E::lineTo, E::startNewSubPath,
                        E::lineTo }));
}
