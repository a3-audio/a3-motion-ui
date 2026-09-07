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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-ui/Helpers.hh>

using namespace a3;

namespace
{

int
countSubPaths (juce::Path const &path)
{
  int subPaths = 0;
  juce::Path::Iterator it (path);
  while (it.next ())
    if (it.elementType == juce::Path::Iterator::startNewSubPath)
      ++subPaths;
  return subPaths;
}

float
longestHop (juce::Path const &path)
{
  float longest = 0.f;
  juce::Path::Iterator it (path);
  juce::Point<float> previous;
  bool have = false;

  while (it.next ())
    {
      juce::Point<float> here;
      switch (it.elementType)
        {
        case juce::Path::Iterator::startNewSubPath:
          previous = { it.x1, it.y1 };
          have = true;
          continue;
        case juce::Path::Iterator::lineTo: here = { it.x1, it.y1 }; break;
        case juce::Path::Iterator::cubicTo: here = { it.x3, it.y3 }; break;
        default: continue;
        }
      if (have)
        longest = std::max (longest, here.getDistanceFrom (previous));
      previous = here;
      have = true;
    }
  return longest;
}

// A closed subpath ends in Z, and the writer put the next subpath's M straight
// after it with nothing in between: "... Z" + "M 0.9 0.2" came out as "ZM".
// Splitting on spaces then yields a token that is neither command, and the M
// -- with it the whole second subpath's starting point -- was dropped. The two
// runs became one, joined by a straight line across the sphere, and the blob
// jumped along it at the end of the trajectory.
TEST (SvgPathParsing, ASubPathIsNotLostWhenItsMFollowsAZ)
{
  auto const glued
      = std::string ("M 0 0 C 0.1 0 0.2 0 0.3 0 ZM 1 1 C 1.1 1 1.2 1 1.3 1");

  auto const path = svgDToPath (glued);

  EXPECT_EQ (countSubPaths (path), 2);
  EXPECT_LT (longestHop (path), 0.5f)
      << "the two runs were joined by a chord across the gap";
}

TEST (SvgPathParsing, SpacedCommandsStillParse)
{
  auto const spaced
      = std::string ("M 0 0 C 0.1 0 0.2 0 0.3 0 Z M 1 1 C 1.1 1 1.2 1 1.3 1");

  EXPECT_EQ (countSubPaths (svgDToPath (spaced)), 2);
}

// And what the writer produces has to survive its own reader.
TEST (SvgPathParsing, ATakeWithAGapIsWrittenAsTwoSubPaths)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("Gapped");
  pattern->resize (512);

  // Two arcs on opposite sides, with nothing in between: a finger that lifted,
  // moved, and came down again.
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const phase = juce::MathConstants<float>::twoPi
                         * static_cast<float> (tick) / 512.f;
      if (tick < 200)
        pattern->setTick (tick, Pos::fromCartesian (std::cos (phase) * 0.5f - 0.4f,
                                                    std::sin (phase) * 0.5f, 0.f));
      else if (tick >= 300)
        pattern->setTick (tick, Pos::fromCartesian (std::cos (phase) * 0.5f + 0.4f,
                                                    std::sin (phase) * 0.5f, 0.f));
    }

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-gapped-take.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const svg = file.loadFileAsString ();
  EXPECT_FALSE (svg.contains ("ZM"))
      << "a command run into the next one is not a command any more";

  auto const path = svgDToPath (PatternFile::peek (file).pathData);
  EXPECT_GE (countSubPaths (path), 2)
      << "the gap the finger left was written away";

  file.deleteFile ();
}


// And the same file read back for playback: that parser is a second copy of
// the same loop, and it had the same blind spot. A gap the finger left has to
// still be a gap in the ticks, or the blob walks the chord across it -- which
// is what jumping at the end of a trajectory looked like.
TEST (SvgPathParsing, AGapSurvivesBeingLoadedForPlayback)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("GappedPlayback");
  pattern->resize (512);

  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const phase = juce::MathConstants<float>::twoPi
                         * static_cast<float> (tick) / 512.f;
      if (tick < 200)
        pattern->setTick (tick, Pos::fromCartesian (std::cos (phase) * 0.5f - 0.4f,
                                                    std::sin (phase) * 0.5f, 0.f));
      else if (tick >= 300)
        pattern->setTick (tick, Pos::fromCartesian (std::cos (phase) * 0.5f + 0.4f,
                                                    std::sin (phase) * 0.5f, 0.f));
    }

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-gapped-playback.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  auto const &positions = reloaded->getTicks ().positions;
  auto const gapTicks = std::count_if (
      positions.begin (), positions.end (),
      [] (Pos const &p) { return !p.isValid (); });

  EXPECT_GT (gapTicks, 0)
      << "the two runs were joined, so playback walks straight across the gap";

  file.deleteFile ();
}

}
