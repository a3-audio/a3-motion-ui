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

using namespace a3;

namespace
{

// A tapped take as it looks once its seams are closed: four positions, each
// held until the next tap, and not one invalid tick anywhere.
std::shared_ptr<Pattern>
tappedPattern ()
{
  std::vector<Pos> const corners
      = { Pos::fromCartesian (0.6f, 0.6f, 0.5f),
          Pos::fromCartesian (-0.6f, 0.6f, 0.5f),
          Pos::fromCartesian (-0.6f, -0.6f, 0.5f),
          Pos::fromCartesian (0.6f, -0.6f, 0.5f) };

  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("Tapped");
  pattern->resize (512);

  for (index_t tick = 0; tick < 512; ++tick)
    pattern->setTick (tick, corners[static_cast<size_t> (tick / 128)]);

  return pattern;
}

// What goes to disk is what the pattern is from then on. A tapped take
// written as one continuous path has its jumps turned into movement
// permanently -- reopening it cannot recover what the writer threw away.
TEST (PatternFileJumps, ATappedTakeIsWrittenAsDotsNotAsALine)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-tapped-take.svg");
  file.deleteFile ();

  ASSERT_TRUE (PatternFile::save (tappedPattern (), file));

  auto const svg = file.loadFileAsString ();
  EXPECT_TRUE (svg.contains ("<circle"))
      << "the taps were not written as dots";
  EXPECT_FALSE (svg.contains ("<path"))
      << "a path across the taps draws each jump as a movement";

  file.deleteFile ();
}

// And it comes back as a tapped take, not as a lap around the corners.
TEST (PatternFileJumps, ATappedTakeSurvivesTheRoundTrip)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-tapped-roundtrip.svg");
  file.deleteFile ();

  ASSERT_TRUE (PatternFile::save (tappedPattern (), file));
  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  auto const peeked = PatternFile::peek (file);
  EXPECT_EQ (peeked.jumpDots.size (), 4u);

  file.deleteFile ();
}

// A drawn trajectory must not be caught by this and reduced to dots.
TEST (PatternFileJumps, ADrawnTakeIsStillWrittenAsAPath)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("Drawn");
  pattern->resize (512);

  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi
                     * static_cast<float> (tick) / 512.f;
      pattern->setTick (
          tick, Pos::fromCartesian (std::cos (a) * 0.6f,
                                    std::sin (a) * 0.6f, 0.5f));
    }

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-drawn-take.svg");
  file.deleteFile ();

  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const svg = file.loadFileAsString ();
  EXPECT_TRUE (svg.contains ("<path"));
  EXPECT_FALSE (svg.contains ("<circle"));

  file.deleteFile ();
}

}
