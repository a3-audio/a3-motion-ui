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

// Direction and end action are clip settings like the fade, so they have to
// come back with the clip. They used to live only in the UI's own table, where
// the engine could not see them and nothing outlived a restart.
TEST (MotionModePersistence, DirectionAndEndActionSurviveARoundTrip)
{
  auto pattern = aCircle ("Reversed");
  pattern->setPlayDirection (PlayDirection::Reverse);
  pattern->setEndAction (EndAction::Bounce);

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-mode.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_EQ (reloaded->getPlayDirection (), PlayDirection::Reverse);
  EXPECT_EQ (reloaded->getEndAction (), EndAction::Bounce);

  file.deleteFile ();
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


// Every clip setting, in one place, because the ones that were forgotten were
// forgotten quietly: the whole Elevation section and the playback length were
// never written, while Pattern.hh said in so many words that each pattern
// remembers its own elevation. It did — until it was saved.
//
// This test enumerates, and there is no way around that in C++: nothing can
// ask a class what settings it has. So it carries a duty instead — **a new
// clip setting is added here at the same time it is added to Pattern**, and if
// that feels like busywork, remember that skipping it is what made a clip come
// back as a different sound.
TEST (MotionModePersistence, EveryClipSettingSurvivesARoundTrip)
{
  auto pattern = aCircle ("Everything");

  // All deliberately away from their defaults, so a field that is not written
  // comes back visibly wrong rather than accidentally right.
  pattern->setPlayDirection (PlayDirection::Reverse);
  pattern->setEndAction (EndAction::Pause);
  pattern->setPlaybackLength ({ 3, 2, 0 });

  pattern->setReach (0.37f);
  pattern->setMirrorSouth (true);
  pattern->setClipTop (0.21f);
  pattern->setClipBottom (0.13f);
  pattern->setFlat (true);
  pattern->setFlatElevation (0.71f);

  pattern->setRotate (0.3f);
  pattern->setSpin (-4);
  pattern->setReachLfo (5);
  pattern->setEnvelopeAttack (1);
  pattern->setEnvelopeDecay (5);
  pattern->setEnvelopeMax (0.42f);

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-every-setting.svg");
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  EXPECT_EQ (reloaded->getPlayDirection (), PlayDirection::Reverse);
  EXPECT_EQ (reloaded->getEndAction (), EndAction::Pause);
  EXPECT_EQ (reloaded->getPlaybackLength (), Measure (3, 2, 0));

  EXPECT_FLOAT_EQ (reloaded->getReach (), 0.37f);
  EXPECT_TRUE (reloaded->getMirrorSouth ());
  EXPECT_FLOAT_EQ (reloaded->getClipTop (), 0.21f);
  EXPECT_FLOAT_EQ (reloaded->getClipBottom (), 0.13f);
  EXPECT_TRUE (reloaded->getFlat ());
  EXPECT_FLOAT_EQ (reloaded->getFlatElevation (), 0.71f);

  EXPECT_FLOAT_EQ (reloaded->getRotate (), 0.3f);
  EXPECT_EQ (reloaded->getSpin (), -4);
  EXPECT_EQ (reloaded->getReachLfo (), 5);
  EXPECT_EQ (reloaded->getEnvelopeAttack (), 1);
  EXPECT_EQ (reloaded->getEnvelopeDecay (), 5);
  EXPECT_FLOAT_EQ (reloaded->getEnvelopeMax (), 0.42f);

  file.deleteFile ();
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

// ── The Action key's mode ────────────────────────────────────────────────

TEST (MotionModePersistence, ActModeSurvivesARoundTrip)
{
  auto pattern = aCircle ("Stab");
  pattern->setActMode (ActMode::Hold);

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-actmode.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_EQ (reloaded->getActMode (), ActMode::Hold);

  file.deleteFile ();
}

TEST (MotionModePersistence, ATakeWrittenBeforeTheModeExistedIsAShot)
{
  // Every take on anybody's stick predates this setting, and every one of them
  // was a shot. Defaulting to Hold would change what those clips do the first
  // time they are opened, which is the one thing a new setting must not do.
  EXPECT_EQ (aCircle ("Fresh")->getActMode (), ActMode::OneShot);

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-actmode-old.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (aCircle ("Old"), file));

  // Strip the attribute, leaving the file an older build would have written.
  auto text = file.loadFileAsString ();
  auto const at = text.indexOf ("data-act-mode");
  ASSERT_GE (at, 0);
  auto const openQuote = text.indexOf (at, "\"");
  auto const closeQuote = text.indexOf (openQuote + 1, "\"");
  file.replaceWithText (text.substring (0, at)
                        + text.substring (closeQuote + 1));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_EQ (reloaded->getActMode (), ActMode::OneShot);

  file.deleteFile ();
}
