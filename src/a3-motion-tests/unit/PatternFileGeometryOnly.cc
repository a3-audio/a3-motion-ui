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

#include <cmath>

using namespace a3;

namespace
{
std::shared_ptr<Pattern>
aCircleWithSettings ()
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("Geometry");
  pattern->resize (512);
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi * tick / 512.f;
      pattern->setTick (tick, Pos::fromCartesian (std::cos (a) * 0.6f,
                                                  std::sin (a) * 0.6f, 0.f));
    }

  pattern->setSpin (3);
  pattern->setFadeReach (0.8f);
  pattern->setBridgeBias (-2);
  pattern->setSpeedLog2 (-2);
  pattern->setRotate (0.25f);
  pattern->setReach (0.4f);
  pattern->setEndAction (EndAction::Bounce);
  pattern->setActMode (ActMode::Hold);
  pattern->setPlayDirection (PlayDirection::Reverse);

  return pattern;
}
}

// The whole point of the split, stated so it cannot quietly come undone: a
// shape file says where the sound goes and nothing about how it is played.
// Without this test the next setting to be added lands back in the SVG,
// because that is where all the others used to be.
TEST (PatternFileGeometryOnly, AShapeFileHoldsNoSettings)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-geometry-only.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (aCircleWithSettings (), file));

  auto const text = file.loadFileAsString ();

  for (auto const *forbidden :
       { "data-spin", "data-fade", "data-end-action", "data-direction",
         "data-act-mode", "data-rotate", "data-reach", "data-reach-lfo",
         "data-env-attack", "data-env-decay", "data-env-max",
         "data-mirror-south", "data-clip-top", "data-clip-bottom",
         "data-flat", "data-flat-elevation", "data-playback-bar",
         "data-playback-beat", "data-playback-tick" })
    EXPECT_FALSE (text.contains (forbidden))
        << forbidden << " is a setting and has no business in a shape file";

  file.deleteFile ();
}

// ... and the geometry is all still there. Half of this change would be worse
// than none of it.
TEST (PatternFileGeometryOnly, AShapeFileStillHoldsItsGeometry)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-geometry-kept.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (aCircleWithSettings (), file));

  auto const text = file.loadFileAsString ();
  for (auto const *required :
       { "data-ppqn", "data-beats", "data-name", "<path" })
    EXPECT_TRUE (text.contains (required)) << required << " is missing";

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  EXPECT_EQ (reloaded->getNumTicks (), 512u);

  file.deleteFile ();
}

// The reader keeps understanding the old attributes. It has to: the migration
// reads takes written the old way, and it runs on every start until the last
// one is done.
TEST (PatternFileGeometryOnly, TheReaderStillUnderstandsAnOldFile)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-geometry-old.svg");
  file.deleteFile ();
  file.replaceWithText (
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\""
      " data-name=\"Old\" data-beats=\"4\" data-ppqn=\"128\""
      " data-spin=\"3\" data-end-action=\"bounce\">"
      "<path d=\"M -0.5 0 C -0.25 0.5 0.25 -0.5 0.5 0\"/></svg>");

  auto const pattern = PatternFile::load (file);
  ASSERT_NE (pattern, nullptr);
  EXPECT_EQ (pattern->getSpin (), 3);
  EXPECT_EQ (pattern->getEndAction (), EndAction::Bounce);

  file.deleteFile ();
}
