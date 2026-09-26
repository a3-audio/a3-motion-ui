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
#include <set>

using namespace a3;

namespace
{

// The stroke list is what both rasterisers of the line map paint -- the
// software strokes and the GPU pass (a3-motion-ui#34). These tests pin down
// the rules the software path has always followed, so the GPU pass inherits
// them instead of re-deriving them.

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
  return stroke.width == lineMapCoreWidth * lineMapTexels;
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

TEST (LineMapStrokes, TheGpuPaintsTheMapOnlyWhenTheConfigSaysTrue)
{
  auto const config = [] (juce::String const &ui) {
    return juce::JSON::parse ("{\"ui\": " + ui + "}");
  };
  EXPECT_FALSE (gpuLineMapsWanted (juce::JSON::parse ("{}")));
  EXPECT_FALSE (gpuLineMapsWanted (config ("{}")));
  EXPECT_FALSE (gpuLineMapsWanted (config ("{\"gpuLineMaps\": false}")));
  EXPECT_FALSE (gpuLineMapsWanted (config ("{\"gpuLineMaps\": \"true\"}")));
  EXPECT_TRUE (gpuLineMapsWanted (config ("{\"gpuLineMaps\": true}")));
}
