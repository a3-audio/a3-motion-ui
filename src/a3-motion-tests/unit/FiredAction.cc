/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/FiredAction.hh>

#include <JuceHeader.h>

using namespace a3;

// A press, worked out once (FPV phase B): what the clip wears, what a flying
// ship does, what the pilot is asked. One run of the script, one throw of the
// dice, so the three cannot disagree.

namespace
{
FiredAction
fire (juce::String const &source, juce::int64 seed = 1)
{
  return fireActionAt (source, ClipSettings{}, seed, actionFeelFrom (ClipSettings{}));
}

juce::Array<juce::File>
shippedActions ()
{
  juce::File const dir (A3_PATTERN_ACTIONS_DIR);
  auto files = dir.findChildFiles (juce::File::findFiles, false, "*.scd");
  files.removeIf ([] (juce::File const &f) { return f.getFileName () == "README.scd"; });
  return files;
}
}

// FULL must not change: the clip side is exactly what a press resolved to
// before, for every action that ships.
TEST (FiredAction, TheClipSideIsWhatAPressAlwaysResolvedTo)
{
  auto const files = shippedActions ();
  ASSERT_FALSE (files.isEmpty ());
  ClipSettings base;
  base.spin = 2;
  base.reach = 0.3f;
  auto const feel = actionFeelFrom (base);
  for (auto const &file : files)
    {
      auto const source = file.loadFileAsString ();
      EXPECT_EQ (fireActionAt (source, base, 5, feel).settings,
                 resolveActionAt (source, base, 5, feel))
          << file.getFileName ();
    }
}

TEST (FiredAction, OnlyTheNamedMotionKeysFly)
{
  auto const fired = fire ("~spin = 5;\n~sqzX = 0.5;\n~base = 1;\n");
  ASSERT_TRUE (fired.flight.spin.has_value ());
  EXPECT_EQ (*fired.flight.spin, 5);
  EXPECT_FALSE (fired.flight.sway.has_value ());
  EXPECT_FALSE (fired.flight.tilt.has_value ());
  EXPECT_FALSE (fired.flight.speedLog2.has_value ());
}

TEST (FiredAction, EveryFlyingKeyIsRead)
{
  auto const fired = fire ("~spin = -3;\n~sway = 4;\n~swell = -2;\n~tilt = 1;\n"
                           "~roll = -0.5;\n~tswp = 6;\n~rswp = -7;\n~speedLog2 = -1;\n");
  EXPECT_EQ (fired.flight.spin, std::optional<int> (-3));
  EXPECT_EQ (fired.flight.sway, std::optional<int> (4));
  EXPECT_EQ (fired.flight.swell, std::optional<int> (-2));
  ASSERT_TRUE (fired.flight.tilt.has_value ());
  EXPECT_FLOAT_EQ (*fired.flight.tilt, 1.f);
  ASSERT_TRUE (fired.flight.roll.has_value ());
  EXPECT_FLOAT_EQ (*fired.flight.roll, -0.5f);
  EXPECT_EQ (fired.flight.tiltSweep, std::optional<int> (6));
  EXPECT_EQ (fired.flight.rollSweep, std::optional<int> (-7));
  EXPECT_EQ (fired.flight.speedLog2, std::optional<int> (-1));
}

// The spec: no sensible flight sense.
TEST (FiredAction, TheSqueezesRotateAndElevationDoNotFly)
{
  auto const fired = fire ("~sqzX = 0.5;\n~sqzY = -0.5;\n~strX = 3;\n~strY = -3;\n"
                           "~rotate = 0.3;\n~base = 1;\n~reach = -0.5;\n~clipTop = 0.2;\n"
                           "~clipBottom = 0.2;\n~flat = true;\n~flatElevation = 0.3;\n");
  EXPECT_FALSE (fired.flight.any ());
}

TEST (FiredAction, AShippedSpinFlies)
{
  juce::File const file = juce::File (A3_PATTERN_ACTIONS_DIR).getChildFile ("Move Spin.scd");
  ASSERT_TRUE (file.existsAsFile ());
  auto const fired = fire (file.loadFileAsString ());
  EXPECT_EQ (fired.flight.spin, std::optional<int> (5));
  EXPECT_FALSE (fired.pilot.game.has_value ()) << "shipped scripts carry the section commented";
}

TEST (FiredAction, ThePilotRidesAlong)
{
  auto const fired = fire ("~game = \\formation;\n~with = \\all;\n");
  ASSERT_TRUE (fired.pilot.game.has_value ());
  EXPECT_EQ (*fired.pilot.game, PilotGame::Formation);
  EXPECT_EQ (fired.pilot.with, PilotRecruit::All);
  EXPECT_FALSE (fired.flight.any ());
}

TEST (FiredAction, OneThrowOfTheDiceForAllThree)
{
  auto const source = "~spin = rrand(2, 6);";
  auto const first = fire (source, 9);
  auto const again = fire (source, 9);
  ASSERT_TRUE (first.flight.spin.has_value ());
  EXPECT_EQ (first.flight.spin, again.flight.spin);
  EXPECT_EQ (*first.flight.spin, first.settings.spin) << "the ship flies what the clip wears";
  EXPECT_GE (*first.flight.spin, 2);
  EXPECT_LE (*first.flight.spin, 6);
}
