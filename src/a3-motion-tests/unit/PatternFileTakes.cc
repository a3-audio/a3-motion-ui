/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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
#include <a3-motion-engine/TakeProjection.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <cmath>

using namespace a3;

// A take saved to the library and loaded again plays where it played before
// (#68). The writer used to treat it as a drawn shape: scaled so its furthest
// point became 1, thinned to 128 points a run, and every open run given a
// reversed copy -- and the reader took all of that at face value.

namespace
{

juce::File
freshDir (char const *name)
{
  auto const dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile (name);
  dir.deleteRecursively ();
  dir.createDirectory ();
  return dir;
}

Pos
directionAt (float frac, float azimuth)
{
  auto const t = frac * juce::MathConstants<float>::pi;
  return Pos::fromCartesian (std::sin (t) * std::cos (azimuth),
                             std::sin (t) * std::sin (azimuth), std::cos (t));
}

/** A clip squeezed and turned, which is what lets a written point lie up to
 *  twice outside the pad (#66). */
void
shapeLikeASqueezedClip (Pattern &pattern)
{
  pattern.setSqueezeX (-1.f); // x half as wide: written up to twice as far
  pattern.setSqueezeY (0.6f);
  pattern.setRotate (0.2f);
  pattern.setElevationBase (0.35f);
  pattern.setReach (0.7f);
}

/** Recorded over a squeezed clip: three open runs at uneven speed with gaps
 *  between them, written through the clip's inverse the way the engine
 *  writes a take. */
std::shared_ptr<Pattern>
aTakeOverASqueezedClip ()
{
  HeightMapSphere heightMap;
  auto take = std::make_shared<Pattern> ();
  take->setName ("Rec_Squeezed");
  shapeLikeASqueezedClip (*take);

  constexpr index_t numTicks = 1024; // eight beats
  take->resize (numTicks);

  struct Run
  {
    index_t from, to;
    float azimuthFrom, azimuthTo, frac;
  };
  for (auto const &run : { Run{ 0, 300, -2.5f, 0.4f, 0.55f },
                           Run{ 340, 700, 1.0f, 2.9f, 0.3f },
                           Run{ 760, 1000, 0.2f, -1.4f, 0.62f } })
    for (index_t tick = run.from; tick < run.to; ++tick)
      {
        auto const t = static_cast<float> (tick - run.from)
                       / static_cast<float> (run.to - run.from);
        // Eased, so the speed changes along the run: an arc-length resample
        // would play it at an even pace.
        auto const eased = t * t;
        auto const finger = directionAt (
            run.frac + 0.1f * std::sin (6.f * t),
            run.azimuthFrom + (run.azimuthTo - run.azimuthFrom) * eased);
        take->setTick (tick, writtenPosition (heightMap, finger, *take));
      }

  // How the engine starts a take; a pattern that went through it is one.
  take->setStatus (Pattern::Status::Recording);
  take->setStatus (Pattern::Status::Idle);
  return take;
}

float
furthestCoordinate (Pattern const &pattern)
{
  auto furthest = 0.f;
  for (auto const &tick : pattern.getTicks ().positions)
    if (tick.isValid ())
      furthest = std::max ({ furthest, std::abs (tick.x ()),
                             std::abs (tick.y ()) });
  return furthest;
}

void
expectPlaysTickForTick (Pattern const &original, Pattern const &reloaded,
                        Pattern const &clip)
{
  HeightMapSphere heightMap;
  ASSERT_EQ (reloaded.getNumTicks (), original.getNumTicks ());

  int differing = 0;
  for (index_t tick = 0; tick < original.getNumTicks (); ++tick)
    {
      auto const before = original.getTick (tick);
      auto const after = reloaded.getTick (tick);
      if (before.isValid () != after.isValid ())
        {
          if (++differing <= 5)
            ADD_FAILURE () << "tick " << tick << ": valid " << before.isValid ()
                           << " became " << after.isValid ();
          continue;
        }
      if (!before.isValid ())
        continue;

      auto const heard = playedPosition (heightMap, before, clip);
      auto const heardAgain = playedPosition (heightMap, after, clip);
      auto const off = std::hypot (heard.x () - heardAgain.x (),
                                   heard.y () - heardAgain.y (),
                                   heard.z () - heardAgain.z ());
      if (off > 1e-3f && ++differing <= 5)
        ADD_FAILURE () << "tick " << tick << " is heard " << off
                       << " away from where it played; stored ("
                       << before.x () << ", " << before.y () << "), loaded ("
                       << after.x () << ", " << after.y () << ")";
    }
  EXPECT_EQ (differing, 0) << "ticks that play somewhere else after a reload";
}

}

TEST (PatternFileTakes, ARecordedPatternIsATake)
{
  Pattern drawn;
  EXPECT_FALSE (drawn.isTake ());

  auto const take = aTakeOverASqueezedClip ();
  EXPECT_TRUE (take->isTake ());
}

TEST (PatternFileTakes, ASavedTakePlaysTickForTickWhereItPlayedBefore)
{
  auto const take = aTakeOverASqueezedClip ();
  ASSERT_GT (furthestCoordinate (*take), 1.2f)
      << "the take has to reach past the pad for this to test the scale";

  auto const file = freshDir ("a3-take-keeps-size").getChildFile ("08_Rec.svg");
  ASSERT_TRUE (PatternFile::save (take, file));
  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  EXPECT_TRUE (reloaded->isTake ());
  expectPlaysTickForTick (*take, *reloaded, *take);
}

// Saved in place after it was loaded: still a take, still the same ticks.
TEST (PatternFileTakes, ATakeSavedAgainStaysTheSame)
{
  auto const take = aTakeOverASqueezedClip ();
  auto const dir = freshDir ("a3-take-saved-again");

  ASSERT_TRUE (PatternFile::save (take, dir.getChildFile ("first.svg")));
  auto const once = PatternFile::load (dir.getChildFile ("first.svg"));
  ASSERT_NE (once, nullptr);
  ASSERT_TRUE (PatternFile::save (once, dir.getChildFile ("second.svg")));
  auto const twice = PatternFile::load (dir.getChildFile ("second.svg"));
  ASSERT_NE (twice, nullptr);

  EXPECT_TRUE (twice->isTake ());
  expectPlaysTickForTick (*take, *twice, *take);
}

// A tapped take keeps when each tap came, not just where: the dots a shape of
// taps is written as are spread evenly on load.
TEST (PatternFileTakes, ATappedTakeKeepsItsTiming)
{
  auto take = std::make_shared<Pattern> ();
  take->setName ("Rec_Tapped");
  take->resize (512);
  std::vector<std::pair<index_t, Pos>> const taps
      = { { 0, Pos::fromCartesian (0.6f, 0.6f, 0.f) },
          { 50, Pos::fromCartesian (-1.4f, 0.6f, 0.f) },
          { 300, Pos::fromCartesian (-0.6f, -0.6f, 0.f) },
          { 330, Pos::fromCartesian (0.6f, -1.6f, 0.f) } };
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto held = taps.front ().second;
      for (auto const &[from, at] : taps)
        if (tick >= from)
          held = at;
      take->setTick (tick, held);
    }
  take->setStatus (Pattern::Status::Recording);
  take->setStatus (Pattern::Status::Idle);
  take->markComplete (); // as RecordingSeam leaves a finished take

  auto const file = freshDir ("a3-tapped-take-timing").getChildFile ("04_Rec.svg");
  ASSERT_TRUE (PatternFile::save (take, file));
  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  Pattern plain;
  expectPlaysTickForTick (*take, *reloaded, plain);

  // Between ticks as well: a jump is stood on, not slid across, at any speed.
  int sliding = 0;
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const before = take->getInterpolatedTick (tick + 0.5);
      auto const after = reloaded->getInterpolatedTick (tick + 0.5);
      if (std::hypot (before.x () - after.x (), before.y () - after.y ())
          > 1e-3f)
        ++sliding;
    }
  EXPECT_EQ (sliding, 0) << "half-ticks that slide across a tap after a reload";

  // And it is still pictured as its taps, where they were.
  auto const peeked = PatternFile::peek (file);
  EXPECT_TRUE (peeked.pathData.empty ());
  ASSERT_EQ (peeked.jumpDots.size (), 4u);
  EXPECT_NEAR (peeked.jumpDots[1].first, -1.4f, 1e-3f);
}

// The picture of a take is the take: the browser, the pad and the preview draw
// it from the file's path, in the coordinates it plays in.
TEST (PatternFileTakes, ATakesPathIsDrawnWhereItPlays)
{
  auto const take = aTakeOverASqueezedClip ();
  auto const file = freshDir ("a3-take-picture").getChildFile ("08_Rec.svg");
  ASSERT_TRUE (PatternFile::save (take, file));

  auto const path = PatternFile::peek (file).pathData;
  ASSERT_FALSE (path.empty ());

  // Three runs, three subpaths: nothing drawn across a gap.
  int moves = 0;
  for (auto const c : path)
    if (c == 'M')
      ++moves;
  EXPECT_EQ (moves, 3);

  // At the size it plays, not scaled down to the pad.
  auto furthest = 0.f;
  for (auto const &token :
       juce::StringArray::fromTokens (juce::String (path), " ", ""))
    if (token.containsOnly ("-0123456789."))
      furthest = std::max (furthest, std::abs (token.getFloatValue ()));
  EXPECT_NEAR (furthest, furthestCoordinate (*take), 1e-3f);
}

// ── Shapes stay what they were ───────────────────────────────────────────

// A drawn shape is still written the way the library's shapes are: its
// furthest point at 1, as a shape and not as a take.
TEST (PatternFileTakes, AShapeIsStillNormalisedAndNotATake)
{
  auto shape = std::make_shared<Pattern> ();
  shape->setName ("Small Circle");
  shape->resize (512);
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi
                     * static_cast<float> (tick) / 512.f;
      shape->setTick (tick, Pos::fromCartesian (0.5f * std::cos (a),
                                                0.5f * std::sin (a), 0.f));
    }

  auto const file = freshDir ("a3-shape-normalised").getChildFile ("04_C.svg");
  ASSERT_TRUE (PatternFile::save (shape, file));
  EXPECT_FALSE (file.loadFileAsString ().contains ("data-kind"));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_FALSE (reloaded->isTake ());
  EXPECT_NEAR (furthestCoordinate (*reloaded), 1.f, 0.02f);
}

TEST (PatternFileTakes, NoShippedShapeLoadsAsATake)
{
  auto const files = juce::File (A3_PATTERN_SYSTEM_DIR)
                         .findChildFiles (juce::File::findFiles, false, "*.svg");
  ASSERT_FALSE (files.isEmpty ());

  for (auto const &file : files)
    if (auto const pattern = PatternFile::load (file))
      EXPECT_FALSE (pattern->isTake ()) << file.getFileName ();
}

// A take written before #68 carries no mark and loads the way it always
// did: its path read as a drawn shape, the reversed half included.
TEST (PatternFileTakes, AnOldTakeLoadsAsItAlwaysDid)
{
  auto const file = freshDir ("a3-old-take").getChildFile ("04_Rec_old.svg");
  file.replaceWithText (
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\" "
      "data-name=\"Rec_old\" data-beats=\"4\" data-ppqn=\"128\">"
      "<path d=\"M -1 0 L 1 0 L -1 0\" fill=\"none\" stroke=\"black\"/>"
      "</svg>");

  auto const pattern = PatternFile::load (file);
  ASSERT_NE (pattern, nullptr);
  EXPECT_FALSE (pattern->isTake ());
  ASSERT_EQ (pattern->getNumTicks (), 512u);
  EXPECT_NEAR (pattern->getTick (0).x (), -1.f, 1e-3f);
  EXPECT_NEAR (pattern->getTick (256).x (), 1.f, 1e-2f);
  EXPECT_NEAR (pattern->getTick (384).x (), 0.f, 1e-2f);
}
