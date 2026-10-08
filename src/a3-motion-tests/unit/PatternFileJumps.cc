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
#include <a3-motion-engine/PatternGenerator.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

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

// ---------------------------------------------------------------------------
//  A time per hit (#61)
// ---------------------------------------------------------------------------

juce::File
tempSvg (char const *name)
{
  auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile (name);
  file.deleteFile ();
  return file;
}

float
furthest (Pattern const &pattern)
{
  auto reach = 0.f;
  for (index_t t = 0; t < pattern.getNumTicks (); ++t)
    if (auto const p = pattern.getTick (t); p.isValid ())
      reach = std::max (reach, std::hypot (p.x (), p.y ()));
  return reach;
}

// Tick for tick: the same ticks empty, and every held tick at the same place
// once both are brought to the same scale (a shape file normalises it).
void
expectSameRhythm (Pattern const &expected, Pattern const &actual,
                  std::string const &name)
{
  ASSERT_EQ (expected.getNumTicks (), actual.getNumTicks ()) << name;
  auto const scaleE = furthest (expected);
  auto const scaleA = furthest (actual);
  ASSERT_GT (scaleE, 0.f) << name;
  ASSERT_GT (scaleA, 0.f) << name;

  int wrong = 0;
  for (index_t t = 0; t < expected.getNumTicks (); ++t)
    {
      auto const e = expected.getTick (t);
      auto const a = actual.getTick (t);
      auto const same
          = e.isValid () == a.isValid ()
            && (!e.isValid ()
                || (std::abs (e.x () / scaleE - a.x () / scaleA) < 1e-3f
                    && std::abs (e.y () / scaleE - a.y () / scaleA) < 1e-3f));
      if (!same && wrong++ < 3)
        ADD_FAILURE () << name << ": tick " << t << " differs";
    }
  EXPECT_EQ (wrong, 0) << name;
}

using Generator = std::function<std::unique_ptr<Pattern> ()>;

std::vector<std::pair<std::string, Generator>>
rhythmShapes ()
{
  static HeightMapSphere const heightMap;
  constexpr float radius = 0.8f;
  return {
    { "Tresillo", [] { return PatternGenerator::createTresillo (4, radius, heightMap); } },
    { "Gallop", [] { return PatternGenerator::createGallop (4, radius, heightMap); } },
    { "Offbeat", [] { return PatternGenerator::createOffbeat (4, radius, heightMap); } },
    { "Shuffle", [] { return PatternGenerator::createShuffle (4, radius, heightMap); } },
    { "Clave 3-2", [] { return PatternGenerator::createClave32 (8, radius, heightMap); } },
    { "Clave 2-3", [] { return PatternGenerator::createClave23 (8, radius, heightMap); } },
    { "Ping Pong", [] { return PatternGenerator::createPingPong (4, radius, heightMap); } },
    { "Four Floor", [] { return PatternGenerator::createFourFloor (4, radius, heightMap); } },
    { "Echo", [] { return PatternGenerator::createEcho (16, radius, heightMap); } },
  };
}

// The issue itself: a tresillo, a gallop, an offbeat and a shuffle came back
// from their files as even jumps. Whatever the generator plays, the file plays.
TEST (PatternFileJumps, ARhythmShapeComesBackWithItsHitTimes)
{
  for (auto const &[name, create] : rhythmShapes ())
    {
      std::shared_ptr<Pattern> const made = create ();
      auto const file = tempSvg ("a3-rhythm-hit-times.svg");
      ASSERT_TRUE (PatternFile::save (made, file)) << name;
      auto const loaded = PatternFile::load (file);
      ASSERT_NE (loaded, nullptr) << name;
      expectSameRhythm (*made, *loaded, name);
      file.deleteFile ();
    }
}

// One circle per hit, each saying when -- the gallop's twelve hits over three
// places used to be folded into three dots.
TEST (PatternFileJumps, EveryHitIsWrittenWithItsTick)
{
  static HeightMapSphere const heightMap;
  std::shared_ptr<Pattern> const gallop
      = PatternGenerator::createGallop (4, 0.8f, heightMap);
  auto const file = tempSvg ("a3-gallop-hits.svg");
  ASSERT_TRUE (PatternFile::save (gallop, file));

  auto const xml = juce::XmlDocument::parse (file);
  ASSERT_NE (xml, nullptr);
  std::vector<int> ats;
  for (auto *child : xml->getChildWithTagNameIterator ("circle"))
    {
      ASSERT_TRUE (child->hasAttribute ("data-at"));
      ats.push_back (child->getIntAttribute ("data-at"));
    }

  auto const sixteenth = TempoClock::getTicksPerBeat () / 4;
  std::vector<int> expected;
  for (int beat = 0; beat < 4; ++beat)
    for (int s : { 0, 2, 3 })
      expected.push_back ((beat * 4 + s) * sixteenth);
  EXPECT_EQ (ats, expected);

  file.deleteFile ();
}

juce::File
handWrittenDots (char const *name, juce::String const &circles)
{
  auto const file = tempSvg (name);
  file.replaceWithText (
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\" "
      "data-name=\"Hand\" data-beats=\"4\" data-ppqn=\"128\">"
      + circles + "</svg>");
  return file;
}

// Every file written before #61 -- and every user's own -- has no times, and
// plays exactly as it did: the dots spread evenly, the last tick of each
// left empty so the jump is a jump.
TEST (PatternFileJumps, DotsWithoutTimesStillSpreadEvenly)
{
  auto const file = handWrittenDots (
      "a3-untimed-dots.svg",
      "<circle cx=\"0\" cy=\"1\" r=\"0.05\"/>"
      "<circle cx=\"-0.866\" cy=\"-0.5\" r=\"0.05\"/>"
      "<circle cx=\"0.866\" cy=\"-0.5\" r=\"0.05\"/>");
  auto const loaded = PatternFile::load (file);
  ASSERT_NE (loaded, nullptr);
  ASSERT_EQ (loaded->getNumTicks (), 512u);

  auto const perDot = 512u / 3u;
  for (index_t t = 0; t < 512; ++t)
    {
      auto const dot = t / perDot;
      auto const tick = loaded->getTick (t);
      if (t % perDot == perDot - 1 || dot >= 3)
        {
          EXPECT_FALSE (tick.isValid ()) << t;
          continue;
        }
      ASSERT_TRUE (tick.isValid ()) << t;
      EXPECT_NEAR (tick.y (), dot == 0 ? 1.f : -0.5f, 1e-4f) << t;
    }
  file.deleteFile ();
}

// Half a timing is no timing: a file that says when for only some of its
// dots is read the old way rather than guessed at.
TEST (PatternFileJumps, PartlyTimedDotsAreReadAsUntimed)
{
  auto const timed = handWrittenDots (
      "a3-partly-timed.svg",
      "<circle cx=\"-1\" cy=\"0\" r=\"0.05\" data-at=\"64\"/>"
      "<circle cx=\"1\" cy=\"0\" r=\"0.05\"/>");
  auto const loaded = PatternFile::load (timed);
  ASSERT_NE (loaded, nullptr);
  EXPECT_NEAR (loaded->getTick (0).x (), -1.f, 1e-4f)
      << "the first dot starts the clip, as it always did";
  EXPECT_NEAR (loaded->getTick (300).x (), 1.f, 1e-4f);
  timed.deleteFile ();
}

// A timed file that does not start on tick 0 holds its last place until the
// first hit, the way the loop comes round.
TEST (PatternFileJumps, BeforeTheFirstHitTheLastOneHolds)
{
  auto const file = handWrittenDots (
      "a3-offbeat-hand.svg",
      "<circle cx=\"-1\" cy=\"0\" r=\"0.05\" data-at=\"64\"/>"
      "<circle cx=\"1\" cy=\"0\" r=\"0.05\" data-at=\"320\"/>");
  auto const loaded = PatternFile::load (file);
  ASSERT_NE (loaded, nullptr);
  EXPECT_NEAR (loaded->getTick (0).x (), 1.f, 1e-4f);
  EXPECT_FALSE (loaded->getTick (63).isValid ());
  EXPECT_NEAR (loaded->getTick (64).x (), -1.f, 1e-4f);
  EXPECT_NEAR (loaded->getTick (318).x (), -1.f, 1e-4f);
  EXPECT_FALSE (loaded->getTick (319).isValid ());
  EXPECT_NEAR (loaded->getTick (511).x (), 1.f, 1e-4f);
  file.deleteFile ();
}

// The shipped rhythm shapes are the generator's, timings and all.
TEST (PatternFileJumps, TheShippedRhythmShapesPlayTheirRhythm)
{
  static HeightMapSphere const heightMap;
  struct Shipped
  {
    char const *file;
    Generator create;
  };
  std::vector<Shipped> const shipped = {
    { "04_Rhythm_Tresillo.svg", [] { return PatternGenerator::createTresillo (4, 0.8f, heightMap); } },
    { "04_Rhythm_Gallop.svg", [] { return PatternGenerator::createGallop (4, 0.8f, heightMap); } },
    { "04_Rhythm_Offbeat.svg", [] { return PatternGenerator::createOffbeat (4, 0.8f, heightMap); } },
    { "04_Rhythm_Shuffle.svg", [] { return PatternGenerator::createShuffle (4, 0.8f, heightMap); } },
    { "08_Rhythm_Clave_3-2.svg", [] { return PatternGenerator::createClave32 (8, 0.8f, heightMap); } },
    { "08_Rhythm_Clave_2-3.svg", [] { return PatternGenerator::createClave23 (8, 0.8f, heightMap); } },
    { "04_Rhythm_Ping_Pong.svg", [] { return PatternGenerator::createPingPong (4, 0.8f, heightMap); } },
    { "04_Rhythm_Four_Floor.svg", [] { return PatternGenerator::createFourFloor (4, 0.8f, heightMap); } },
    { "16_Rhythm_Echo.svg", [] { return PatternGenerator::createEcho (16, 0.8f, heightMap); } },
  };
  for (auto const &s : shipped)
    {
      auto const loaded = PatternFile::load (
          juce::File (A3_PATTERN_SYSTEM_DIR).getChildFile (s.file));
      ASSERT_NE (loaded, nullptr) << s.file;
      expectSameRhythm (*s.create (), *loaded, s.file);
    }
}

}
