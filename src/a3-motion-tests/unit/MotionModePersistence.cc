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
