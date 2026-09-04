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
#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/Playhead.hh>

using namespace a3;

namespace
{

std::shared_ptr<Pattern>
aCircle (juce::String const &name)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName (name.toStdString ());
  pattern->resize (512);
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi * tick / 512.f;
      pattern->setTick (tick, Pos::fromCartesian (std::cos (a) * 0.6f,
                                                  std::sin (a) * 0.6f, 0.f));
    }
  return pattern;
}

// Every value has to survive, not just the one that was tried first.
TEST (MotionModePersistence, EveryEndActionSurvives)
{
  for (auto const action : { EndAction::Loop, EndAction::Stop,
                             EndAction::Bounce, EndAction::Random })
    EXPECT_EQ (endActionFromName (endActionToName (action)), action);
}

// A file written before the setting existed plays the way every clip did then.
TEST (MotionModePersistence, AFileWithoutThemLoopsForwards)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-mode-legacy.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (aCircle ("Plain"), file));

  // Strip the two attributes the way an older file simply would not have them.
  auto text = file.loadFileAsString ();
  text = text.replace ("data-direction=\"fwd\"", "")
             .replace ("data-end-action=\"loop\"", "");
  file.replaceWithText (text);

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_EQ (reloaded->getPlayDirection (), PlayDirection::Forward);
  EXPECT_EQ (reloaded->getEndAction (), EndAction::Loop);

  file.deleteFile ();
}

}

// A file written before a setting existed has to keep loading, and come back
// with the value it always behaved as having. Everything added here has a
// default, and the default is what an old file means.
TEST (MotionModePersistence, AFileWithoutTheNewSettingsLoadsWithTheirDefaults)
{
  auto const written = aCircle ("Old");
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-old-settings.svg");
  ASSERT_TRUE (PatternFile::save (written, file));

  // Strip the newer attributes back out, which is what an older file is.
  auto text = file.loadFileAsString ();
  for (auto const *attribute :
       { "data-reach", "data-clip-top", "data-clip-bottom",
         "data-mirror-south", "data-flat", "data-flat-elevation",
         "data-playback" })
    {
      auto const at = text.indexOf (attribute);
      if (at < 0)
        continue;
      auto const end = text.indexOf (at + 1, "\"") + 1;
      auto const close = text.indexOf (end, "\"") + 1;
      text = text.substring (0, at) + text.substring (close);
    }
  file.replaceWithText (text);

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  Pattern const fresh;
  EXPECT_FLOAT_EQ (reloaded->getReach (), fresh.getReach ());
  EXPECT_EQ (reloaded->getMirrorSouth (), fresh.getMirrorSouth ());
  EXPECT_FLOAT_EQ (reloaded->getClipTop (), fresh.getClipTop ());
  EXPECT_FLOAT_EQ (reloaded->getClipBottom (), fresh.getClipBottom ());
  EXPECT_EQ (reloaded->getFlat (), fresh.getFlat ());
  EXPECT_FLOAT_EQ (reloaded->getFlatElevation (), fresh.getFlatElevation ());

  file.deleteFile ();
}

// ── Where a shape sits is part of what it is ─────────────────────────────

TEST (MotionModePersistence, ASavedShapeDoesNotMoveTowardsTheMiddle)
{
  // A triangle with a corner up: its bounding box is not symmetric about the
  // origin, and the file used to be normalised on that box -- which slid the
  // whole shape until the box was centred. The origin is the middle of the
  // room, so that moved the sound; and since rotation turns about the origin,
  // a shape sitting off it swung round instead of spinning in place.
  auto pattern = std::make_shared<Pattern> ();
  pattern->resize (512);
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const t = static_cast<float> (tick) / 512.f;
      auto const seg = static_cast<int> (t * 3.f) % 3;
      auto const frac = t * 3.f - std::floor (t * 3.f);

      auto const corner = [] (int i) {
        auto const a = juce::MathConstants<float>::twoPi * i / 3.f
                       - juce::MathConstants<float>::halfPi;
        return juce::Point<float> (0.8f * std::cos (a), 0.8f * std::sin (a));
      };
      auto const from = corner (seg);
      auto const to = corner ((seg + 1) % 3);
      pattern->setTick (tick, Pos::fromCartesian (
                                  from.x + (to.x - from.x) * frac,
                                  from.y + (to.y - from.y) * frac, 0.f));
    }

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-centre.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  // The mean of the corners is the origin, and it still is after the trip.
  auto const ticks = reloaded->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());

  float sx = 0.f, sy = 0.f;
  int n = 0;
  for (auto const &p : ticks)
    {
      if (!p.isValid ())
        continue;
      sx += p.x ();
      sy += p.y ();
      ++n;
    }
  ASSERT_GT (n, 0);

  EXPECT_NEAR (sx / n, 0.f, 0.03f);
  EXPECT_NEAR (sy / n, 0.f, 0.03f) << "the shape slid to centre its box";

  file.deleteFile ();
}

// ── The shape and its clip, together ─────────────────────────────────────

// Four tests used to live here, each asserting that some setting survived a
// PatternFile round trip. They were right about the guarantee and wrong about
// where it lives: settings are a clip's business now, so the journey they were
// protecting is this one -- write the shape, write the clip, read both back,
// and the clip is what it was.
//
// Stated end to end rather than per file, because that is the promise the
// device makes. Either file alone tells only half of it.
TEST (MotionModePersistence, AShapeAndItsClipTogetherRestoreTheWholeClip)
{
  auto pattern = aCircle ("Everything");

  // All deliberately away from their defaults, so a field that is not written
  // comes back visibly wrong rather than accidentally right.
  pattern->setPlayDirection (PlayDirection::Reverse);
  pattern->setEndAction (EndAction::Pause);
  pattern->setActMode (ActMode::Hold);
  pattern->setSpeedLog2 (-2);
  pattern->setFadeSixteenths (9);
  pattern->setRotate (0.375f);
  pattern->setSpin (4);
  pattern->setReachLfo (-2);
  pattern->setEnvelopeAttack (5);
  pattern->setEnvelopeDecay (1);
  pattern->setEnvelopeMax (0.6f);
  pattern->setReach (0.42f);
  pattern->setMirrorSouth (true);
  pattern->setClipTop (0.15f);
  pattern->setClipBottom (0.25f);
  pattern->setFlat (true);
  pattern->setFlatElevation (0.35f);

  auto const dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("a3-shape-and-clip");
  dir.deleteRecursively ();
  dir.createDirectory ();

  auto const shapeFile = dir.getChildFile ("16_Everything.svg");
  ASSERT_TRUE (PatternFile::save (pattern, shapeFile));

  Clip clip;
  clip.name = "Everything";
  clip.svg = "16_Everything";
  clip.settings = clipSettingsFrom (*pattern);
  auto const clipFile = dir.getChildFile ("Everything.json");
  ASSERT_TRUE (ClipFile::save (clip, clipFile));

  auto const reloaded = PatternFile::load (shapeFile);
  ASSERT_NE (reloaded, nullptr);

  auto const readClip = ClipFile::load (clipFile);
  ASSERT_TRUE (readClip.has_value ());
  applyClipSettings (*reloaded, readClip->settings);

  EXPECT_EQ (reloaded->getPlayDirection (), PlayDirection::Reverse);
  EXPECT_EQ (reloaded->getEndAction (), EndAction::Pause);
  EXPECT_EQ (reloaded->getActMode (), ActMode::Hold);
  EXPECT_EQ (reloaded->getSpeedLog2 (), -2);
  EXPECT_EQ (reloaded->getFadeSixteenths (), 9);
  EXPECT_FLOAT_EQ (reloaded->getRotate (), 0.375f);
  EXPECT_EQ (reloaded->getSpin (), 4);
  EXPECT_EQ (reloaded->getReachLfo (), -2);
  EXPECT_EQ (reloaded->getEnvelopeAttack (), 5);
  EXPECT_EQ (reloaded->getEnvelopeDecay (), 1);
  EXPECT_FLOAT_EQ (reloaded->getEnvelopeMax (), 0.6f);
  EXPECT_FLOAT_EQ (reloaded->getReach (), 0.42f);
  EXPECT_TRUE (reloaded->getMirrorSouth ());
  EXPECT_FLOAT_EQ (reloaded->getClipTop (), 0.15f);
  EXPECT_FLOAT_EQ (reloaded->getClipBottom (), 0.25f);
  EXPECT_TRUE (reloaded->getFlat ());
  EXPECT_FLOAT_EQ (reloaded->getFlatElevation (), 0.35f);

  // ... and the geometry came back too, which is the half the shape file owns.
  EXPECT_EQ (reloaded->getNumTicks (), pattern->getNumTicks ());

  dir.deleteRecursively ();
}
