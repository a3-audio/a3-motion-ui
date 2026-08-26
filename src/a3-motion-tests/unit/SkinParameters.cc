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

#include <a3-motion-ui/theme/SkinParameters.hh>

using namespace a3;

namespace
{

juce::var
parse (juce::String const &text)
{
  return juce::JSON::parse (text);
}

// The editor edits the skin file, not a hand-written list of roles: a key
// added to a skin has to show up without anyone remembering to register it.

TEST (SkinParameters, EveryLeafIsListedByItsPath)
{
  auto const params = skinParameters (parse (R"({
    "accent": { "r": 1, "g": 2, "b": 3 },
    "sphereScale": 0.62
  })"));

  ASSERT_EQ (params.size (), 4u);
  EXPECT_EQ (params[0].path, "accent.b") << "sorted, so the list does not "
                                            "reshuffle between sessions";
  EXPECT_EQ (params[1].path, "accent.g");
  EXPECT_EQ (params[2].path, "accent.r");
  EXPECT_EQ (params[3].path, "sphereScale");
}

TEST (SkinParameters, ArraysAreListedByIndex)
{
  auto const params = skinParameters (parse (R"({
    "channels": [ { "r": 9 }, { "r": 8 } ]
  })"));

  ASSERT_EQ (params.size (), 2u);
  EXPECT_EQ (params[0].path, "channels.0.r");
  EXPECT_EQ (params[1].path, "channels.1.r");
}

// A skin file holds nothing but numbers, but config.json holds host names
// too, and the same editor shows both — a keyboard is what a text row needs,
// and there is one now.
TEST (SkinParameters, TextIsListedAndMarkedAsText)
{
  auto const params = skinParameters (parse (R"({
    "host": "127.0.0.1",
    "port": 9000
  })"));

  ASSERT_EQ (params.size (), 2u);
  EXPECT_EQ (params[0].path, "host");
  EXPECT_TRUE (params[0].isText);
  EXPECT_EQ (params[1].path, "port");
  EXPECT_FALSE (params[1].isText);
}

TEST (SkinParameters, TextIsReadAndWrittenByItsPath)
{
  auto skin = parse (R"({ "osc": { "host": "127.0.0.1" } })");

  EXPECT_EQ (skinText (skin, "osc.host"), "127.0.0.1");

  setSkinText (skin, "osc.host", "192.168.8.10");

  EXPECT_EQ (skinText (skin, "osc.host"), "192.168.8.10");
}

// Nothing else is offered: a nested object or an array is structure, and
// there is no control on this panel that could edit structure.
TEST (SkinParameters, StructureItselfIsNotAParameter)
{
  auto const params = skinParameters (parse (R"({
    "osc": { "host": "a" },
    "flag": true
  })"));

  ASSERT_EQ (params.size (), 1u);
  EXPECT_EQ (params[0].path, "osc.host");
}

TEST (SkinParameters, AWholeNumberIsRememberedAsOne)
{
  auto const params = skinParameters (parse (R"({
    "accent": { "r": 128 },
    "sphereScale": 0.62
  })"));

  ASSERT_EQ (params.size (), 2u);
  EXPECT_TRUE (params[0].isWholeNumber) << params[0].path.toStdString ();
  EXPECT_FALSE (params[1].isWholeNumber) << params[1].path.toStdString ();
}

TEST (SkinParameters, AValueIsReadAndWrittenByItsPath)
{
  auto skin = parse (R"({ "accent": { "r": 1, "g": 2, "b": 3 } })");

  EXPECT_DOUBLE_EQ (skinValue (skin, "accent.g"), 2.0);

  setSkinValue (skin, "accent.g", 200.0);

  EXPECT_DOUBLE_EQ (skinValue (skin, "accent.g"), 200.0);
  EXPECT_DOUBLE_EQ (skinValue (skin, "accent.r"), 1.0) << "its neighbours stay";
}

TEST (SkinParameters, AValueInAnArrayIsWrittenToo)
{
  auto skin = parse (R"({ "channels": [ { "r": 9 }, { "r": 8 } ] })");

  setSkinValue (skin, "channels.1.r", 77.0);

  EXPECT_DOUBLE_EQ (skinValue (skin, "channels.1.r"), 77.0);
  EXPECT_DOUBLE_EQ (skinValue (skin, "channels.0.r"), 9.0);
}

// A colour channel is written as 128, not 128.0 — and a value that is a
// float in the file stays one even when it lands on a whole number, or the
// next session would step it in ones instead of hundredths.
TEST (SkinParameters, AWholeNumberIsWrittenBackWhole)
{
  auto skin = parse (R"({ "accent": { "r": 1 }, "sphereScale": 0.62 })");

  setSkinValue (skin, "accent.r", 128.0, true);
  setSkinValue (skin, "sphereScale", 1.0, false);

  auto const reread = skinParameters (skin);
  ASSERT_EQ (reread.size (), 2u);
  EXPECT_TRUE (reread[0].isWholeNumber) << reread[0].path.toStdString ();
  EXPECT_FALSE (reread[1].isWholeNumber) << reread[1].path.toStdString ();

  EXPECT_FALSE (juce::JSON::toString (skin).contains ("128.0"));
}

// Stepping compounds, and a double carries every rounding of the way: an
// encoder run left 2.98150695788495 in the file. Nobody edits that by hand
// afterwards, and every diff of the file is noise.
TEST (SkinParameters, AWrittenFloatIsRoundedToSomethingReadable)
{
  auto skin = parse (R"({ "corona": { "sizeMin": 0.95 } })");

  auto value = 0.95;
  for (int i = 0; i < 12; ++i)
    value = stepSkinValue (value, 1, false);
  setSkinValue (skin, "corona.sizeMin", value);

  auto const written = juce::JSON::toString (skin);
  EXPECT_FALSE (written.contains ("2.98150695788495")) << written;
  EXPECT_NEAR (skinValue (skin, "corona.sizeMin"), 2.9815, 0.0001);
}

// One encoder has to cover both a colour channel counted in 255ths and a
// wrap angle counted in degrees, so a step proportional to the value is the
// only one that is usable at both ends.
TEST (SkinParameters, TheStepFollowsTheSizeOfTheValue)
{
  EXPECT_NEAR (skinValueStep (0.5, false), 0.05, 0.001);
  EXPECT_NEAR (skinValueStep (65.0, false), 6.5, 0.001);
}

TEST (SkinParameters, AWholeNumberStepsByOne)
{
  EXPECT_DOUBLE_EQ (skinValueStep (128.0, true), 1.0);
  EXPECT_DOUBLE_EQ (skinValueStep (0.0, true), 1.0);
}

// A value at zero must still be reachable: a proportional step would leave
// it stuck there forever.
TEST (SkinParameters, AValueAtZeroCanStillBeRaised)
{
  EXPECT_GT (skinValueStep (0.0, false), 0.0);
}

TEST (SkinParameters, SteppingIsReversible)
{
  auto const start = 0.62;
  auto const up = stepSkinValue (start, 1, false);
  auto const back = stepSkinValue (up, -1, false);

  EXPECT_NEAR (back, start, 0.01);
}

// A colour channel above 255 is not a brighter colour, it is a broken file.
TEST (SkinParameters, AColourChannelStopsAt255)
{
  EXPECT_DOUBLE_EQ (stepSkinValue (255.0, 1, true, true), 255.0);
  EXPECT_DOUBLE_EQ (stepSkinValue (0.0, -1, true, true), 0.0);
}

TEST (SkinParameters, APathIsRecognisedAsAColourChannel)
{
  EXPECT_TRUE (isColourChannelPath ("accent.r"));
  EXPECT_TRUE (isColourChannelPath ("channels.2.b"));
  EXPECT_TRUE (isColourChannelPath ("sphereGlow.g"));
  EXPECT_FALSE (isColourChannelPath ("sphereScale"));
  EXPECT_FALSE (isColourChannelPath ("sphereGlow.netGain"));
}

}
