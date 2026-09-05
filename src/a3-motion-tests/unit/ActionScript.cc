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

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <set>

using namespace a3;

namespace
{
ClipSettings
run (juce::String const &source, ClipSettings current = {},
     juce::int64 seed = 1)
{
  auto const result = runActionScript (source, current, seed);
  EXPECT_TRUE (result.errors.isEmpty ())
      << "unexpected error: " << result.errors.joinIntoString ("; ");

  return result.settings;
}
}

// ── Assignments ──────────────────────────────────────────────────────────

// The shape of the thing: SuperCollider's environment variables, one per
// line, ending in a semicolon. Anybody who has typed ~freq = 400; into a
// SuperCollider window already knows how to write one of these.
TEST (ActionScript, ANameGetsAValue)
{
  auto const out = run ("~spin = 4;\n~reach = 0.8;\n");

  EXPECT_EQ (out.spin, 4);
  EXPECT_FLOAT_EQ (out.reach, 0.8f);
}

TEST (ActionScript, BlankLinesAndCommentsAreNotErrors)
{
  auto const out = run (R"(
// how the sound is thrown
~spin = 4;   // fast, one way

~reach = 0.8;
)");

  EXPECT_EQ (out.spin, 4);
  EXPECT_FLOAT_EQ (out.reach, 0.8f);
}

// The lists are symbols, the way they are in SuperCollider. \stop reads as
// the word it is; 3 would read as whatever the enum happens to be ordered as
// this week.
TEST (ActionScript, ListsAreWrittenAsSymbols)
{
  auto const out = run ("~end = \\bounce;\n~dir = \\reverse;\n~act = \\hold;\n");

  EXPECT_EQ (out.endAction, EndAction::Bounce);
  EXPECT_EQ (out.direction, PlayDirection::Reverse);
  EXPECT_EQ (out.actMode, ActMode::Hold);
}

TEST (ActionScript, SwitchesAreTrueAndFalse)
{
  auto const out = run ("~flat = true;\n~mirrorSouth = false;\n");

  EXPECT_TRUE (out.flat);
  EXPECT_FALSE (out.mirrorSouth);
}

// ── Expressions ──────────────────────────────────────────────────────────

TEST (ActionScript, ArithmeticWorksAndMultiplyBindsTighter)
{
  auto const out = run ("~reach = 0.25 + 0.5 * 0.5;\n");

  EXPECT_FLOAT_EQ (out.reach, 0.5f);
}

TEST (ActionScript, BracketsChangeThat)
{
  auto const out = run ("~reach = (0.25 + 0.5) * 0.4;\n");

  EXPECT_FLOAT_EQ (out.reach, 0.3f);
}

TEST (ActionScript, ANegativeNumberIsANumber)
{
  auto const out = run ("~spin = -3;\n~bias = 0 - 2;\n");

  EXPECT_EQ (out.spin, -3);
  EXPECT_EQ (out.bridgeBias, -2);
}

// Reading a name gives what the clip has now, so a script can move a value
// rather than only set one -- "half of wherever this was" is a thing a set
// wants to say, and it cannot be written as a number.
TEST (ActionScript, ANameReadsWhatTheClipHasNow)
{
  ClipSettings current;
  current.reach = 0.8f;
  current.spin = 2;

  auto const out = run ("~reach = ~reach * 0.5;\n~spin = ~spin + 1;\n", current);

  EXPECT_FLOAT_EQ (out.reach, 0.4f);
  EXPECT_EQ (out.spin, 3);
}

// And it reads what the script has done so far, not what the clip came in
// with: two lines about the same value read top to bottom like every other
// line in the file.
TEST (ActionScript, ALaterLineSeesTheEarlierOne)
{
  auto const out = run ("~reach = 0.4;\n~reach = ~reach + 0.2;\n");

  EXPECT_FLOAT_EQ (out.reach, 0.6f);
}

// ── Chance ───────────────────────────────────────────────────────────────

TEST (ActionScript, RrandStaysBetweenItsEnds)
{
  for (juce::int64 seed = 0; seed < 40; ++seed)
    {
      auto const out = run ("~reach = rrand(0.2, 0.6);\n", {}, seed);

      EXPECT_GE (out.reach, 0.2f);
      EXPECT_LE (out.reach, 0.6f);
    }
}

// Two whole numbers give a whole number, as they do in SuperCollider: spin is
// a step, and rrand(-4, 4) has to be able to say so.
TEST (ActionScript, RrandOfTwoWholeNumbersIsWhole)
{
  std::set<int> seen;

  for (juce::int64 seed = 0; seed < 60; ++seed)
    {
      auto const out = run ("~spin = rrand(-4, 4);\n", {}, seed);
      EXPECT_GE (out.spin, -4);
      EXPECT_LE (out.spin, 4);
      seen.insert (out.spin);
    }

  EXPECT_GT (seen.size (), 3u) << "the dice are not being thrown at all";
}

TEST (ActionScript, ChooseTakesOneOfTheList)
{
  std::set<int> seen;

  for (juce::int64 seed = 0; seed < 60; ++seed)
    {
      auto const out = run ("~spin = [-4, 0, 4].choose;\n", {}, seed);
      EXPECT_TRUE (out.spin == -4 || out.spin == 0 || out.spin == 4)
          << "chose something that was not in the list: " << out.spin;
      seen.insert (out.spin);
    }

  EXPECT_GT (seen.size (), 1u) << "it always picks the same one";
}

// The same seed gives the same throw. That is what makes a random action
// something you can rehearse: the roll happens when the script is applied,
// and applying it again with the same seed lands in the same place.
TEST (ActionScript, TheSameSeedThrowsTheSame)
{
  auto const source = "~spin = rrand(-4, 4);\n~reach = rrand(0.0, 1.0);\n";

  auto const first = run (source, {}, 12345);
  auto const again = run (source, {}, 12345);

  EXPECT_EQ (first.spin, again.spin);
  EXPECT_FLOAT_EQ (first.reach, again.reach);
}

// ── What a script may not do ─────────────────────────────────────────────

// A typo takes its own line down and leaves the rest standing. On a device
// with no compiler to run and a keyboard that appears over the screen, a
// script that refused to do anything because of one bad line would be a
// script you could not get back to working.
TEST (ActionScript, ABadLineIsReportedAndTheRestStillRuns)
{
  ClipSettings current;
  auto const result
      = runActionScript ("~spin = 4;\n~spn = 2;\n~reach = 0.9;\n", current, 1);

  EXPECT_EQ (result.settings.spin, 4);
  EXPECT_FLOAT_EQ (result.settings.reach, 0.9f);

  ASSERT_EQ (result.errors.size (), 1);
  EXPECT_TRUE (result.errors[0].contains ("2")) << result.errors[0];
  EXPECT_TRUE (result.errors[0].contains ("spn")) << result.errors[0];
}

TEST (ActionScript, AValueOutsideItsRangeIsBroughtBackIn)
{
  auto const out = run ("~reach = 4;\n~envelopeMax = -1;\n~spin = 99;\n");

  EXPECT_LE (out.reach, 1.f);
  EXPECT_GE (out.envelopeMax, 0.f);
  EXPECT_LE (out.spin, lfoMaxStep);
}

TEST (ActionScript, RubbishIsReportedRatherThanGuessedAt)
{
  ClipSettings current;

  for (auto const *source : { "~spin = ;\n", "~spin 4;\n", "~spin = (4;\n",
                              "~end = \\sideways;\n", "~spin = zzz(1);\n" })
    {
      auto const result = runActionScript (source, current, 1);
      EXPECT_FALSE (result.errors.isEmpty ())
          << "accepted rubbish: " << source;
    }
}

// ── Writing one back out ─────────────────────────────────────────────────

// A script the app wrote has to be one the app can read, or "save action"
// and "open action" would be two different formats wearing one name.
TEST (ActionScript, WhatIsWrittenCanBeReadBack)
{
  ClipSettings settings;
  settings.spin = -3;
  settings.reach = 0.42f;
  settings.flat = true;
  settings.endAction = EndAction::Bounce;
  settings.direction = PlayDirection::Reverse;
  settings.actMode = ActMode::Hold;
  settings.qMax = 0.75f;
  settings.bridgeBias = 2;

  auto const source = actionScriptFor (settings);
  auto const back = run (source);

  EXPECT_EQ (back, settings) << "round trip through:\n" << source;
}

// The scripts that ship with the app have to read. They are the first thing
// anybody opens to find out what the format is, so a broken one teaches the
// wrong syntax before it fails to do anything.
TEST (ActionScript, EveryShippedActionReads)
{
  auto const dir = juce::File::getCurrentWorkingDirectory ()
                       .getChildFile ("../pattern/actions");
  if (!dir.isDirectory ())
    GTEST_SKIP () << "not run from the build directory";

  auto const files
      = dir.findChildFiles (juce::File::findFiles, false, "*.scd");
  EXPECT_FALSE (files.isEmpty ()) << "no actions ship at all";

  for (auto const &file : files)
    {
      auto const result
          = runActionScript (file.loadFileAsString (), ClipSettings{}, 7);

      EXPECT_TRUE (result.errors.isEmpty ())
          << file.getFileName () << ": " << result.errors.joinIntoString ("; ");
    }
}
