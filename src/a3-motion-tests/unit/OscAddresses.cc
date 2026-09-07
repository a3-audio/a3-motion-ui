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

#include <a3-motion-engine/OscAddresses.hh>

using namespace a3;

TEST (OscAddresses, DefaultsAreWhatTheSystemHasAlwaysUsed)
{
  auto const a = loadOscAddresses (juce::var{});

  EXPECT_EQ (a.channelAzimuth, "/channel/{ch}/azimuth");
  EXPECT_EQ (a.channelElevation, "/channel/{ch}/elevation");
  EXPECT_EQ (a.channelPot1, "/channel/{ch}/pot_1");
  EXPECT_EQ (a.channelPot2, "/channel/{ch}/pot_2");
  EXPECT_EQ (a.channelThreeD, "/channel/{ch}/3d");
  EXPECT_EQ (a.iemAzimuth, "/StereoEncoder/azimuth");
  EXPECT_EQ (a.iemElevation, "/StereoEncoder/elevation");
  EXPECT_EQ (a.beatOut, "/beat");
  EXPECT_EQ (a.beatIn, "/beat");
  EXPECT_EQ (a.tap, "/tap");
  EXPECT_EQ (a.clockMode, "/clockmode");
  EXPECT_EQ (a.vuPrefix, "/vu/");
  EXPECT_EQ (a.energyRms, "/EnergyVisualizer/RMS");
}

TEST (OscAddresses, AnEntryOnlyReplacesItsOwn)
{
  auto const config
      = juce::JSON::parse (R"({"oscAddresses": {"tap": "/foot/tap"}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.tap, "/foot/tap");
  EXPECT_EQ (a.beatOut, "/beat");
  EXPECT_EQ (a.beatIn, "/beat");
  EXPECT_EQ (a.channelAzimuth, "/channel/{ch}/azimuth");
}

// Guards against a field being added to the struct and forgotten in
// loadOscAddresses(): every key below is given a value no default uses, and
// none of them may still read as its default afterwards.
TEST (OscAddresses, EveryFieldIsActuallyRead)
{
  auto const config = juce::JSON::parse (R"({"oscAddresses": {
      "channelAzimuth":   "/a/{ch}/x",
      "channelElevation": "/a/{ch}/y",
      "channelPot1":      "/a/{ch}/p",
      "channelPot2":      "/a/{ch}/q",
      "channelThreeD":      "/a/{ch}/r",
      "iemAzimuth":       "/b/x",
      "iemElevation":     "/b/y",
      "beatOut":          "/c/beat",
      "beatIn":           "/c/in",
      "tap":              "/c/tap",
      "clockMode":        "/c/mode",
      "vuPrefix":         "/d/",
      "energyRms":        "/e/rms"}})");
  auto const a = loadOscAddresses (config);
  auto const d = OscAddresses{};

  EXPECT_NE (a.channelAzimuth, d.channelAzimuth);
  EXPECT_NE (a.channelElevation, d.channelElevation);
  EXPECT_NE (a.channelPot1, d.channelPot1);
  EXPECT_NE (a.channelPot2, d.channelPot2);
  EXPECT_NE (a.channelThreeD, d.channelThreeD);
  EXPECT_NE (a.iemAzimuth, d.iemAzimuth);
  EXPECT_NE (a.iemElevation, d.iemElevation);
  EXPECT_NE (a.beatOut, d.beatOut);
  EXPECT_NE (a.beatIn, d.beatIn);
  EXPECT_NE (a.tap, d.tap);
  EXPECT_NE (a.clockMode, d.clockMode);
  EXPECT_NE (a.vuPrefix, d.vuPrefix);
  EXPECT_NE (a.energyRms, d.energyRms);
}

// The whole reason this is a unit of its own: juce::OSCMessage throws
// OSCFormatError on an address it will not accept. Somebody typing a path on
// the device must not be able to bring the app down, so a bad one is refused
// here and the default stands instead.
TEST (OscAddresses, AnAddressJuceWouldRejectFallsBackToTheDefault)
{
  for (auto const *bad : { "beat/without/leading/slash", "", "/with space",
                           "/with#hash" })
    {
      juce::DynamicObject::Ptr addresses{ new juce::DynamicObject{} };
      addresses->setProperty ("tap", juce::String (bad));
      juce::DynamicObject::Ptr root{ new juce::DynamicObject{} };
      root->setProperty ("oscAddresses", addresses.get ());

      auto const a = loadOscAddresses (juce::var{ root.get () });
      EXPECT_EQ (a.tap, "/tap") << "accepted: '" << bad << "'";
    }
}

// The placeholder is part of the template, and JUCE would reject a bare '{'
// in an address — so validity is judged on the substituted address, not on
// the template.
TEST (OscAddresses, AChannelTemplateIsJudgedAfterSubstitution)
{
  auto const config = juce::JSON::parse (
      R"({"oscAddresses": {"channelAzimuth": "/src/{ch}/az"}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.channelAzimuth, "/src/{ch}/az");
}

TEST (OscAddresses, WithChannelSubstitutesThePlaceholder)
{
  EXPECT_EQ (withChannel ("/channel/{ch}/azimuth", 2), "/channel/2/azimuth");
  EXPECT_EQ (withChannel ("/src/{ch}/{ch}", 7), "/src/7/7");
}

TEST (OscAddresses, APatternWithoutThePlaceholderIsLeftAlone)
{
  EXPECT_EQ (withChannel ("/StereoEncoder/azimuth", 3),
             "/StereoEncoder/azimuth");
}

TEST (OscAddresses, JuceAcceptsEverySubstitutedDefault)
{
  auto const a = loadOscAddresses (juce::var{});

  for (auto const &pattern : { a.channelAzimuth, a.channelElevation,
                               a.channelPot1, a.channelPot2,
                               a.channelThreeD, a.iemAzimuth,
                               a.iemElevation, a.beatOut, a.beatIn, a.tap,
                               a.clockMode })
    for (int ch = 0; ch < 4; ++ch)
      EXPECT_TRUE (isSendableOscAddress (withChannel (pattern, ch)))
          << pattern;
}

// A prefix is compared with startsWith rather than sent, so it need not be a
// whole valid address — but an empty one would match every message there is.
TEST (OscAddresses, AnEmptyPrefixIsRefused)
{
  auto const config
      = juce::JSON::parse (R"({"oscAddresses": {"vuPrefix": ""}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.vuPrefix, "/vu/");
}


// `3d` is Core's continuous crossfade now; its old boolean toggle moved to
// `4d`. The name is free again, and it is the one that says what the value
// does.
TEST (OscAddresses, TheThirdValueGoesToCoresThreeDee)
{
  auto const a = loadOscAddresses (juce::var{});

  EXPECT_EQ (withChannel (a.channelThreeD, 2), "/channel/2/3d");
}

// A config written while it was still called pot_3 keeps working.
TEST (OscAddresses, TheOldPotThreeNameIsStillRead)
{
  auto const config = juce::JSON::parse (
      R"({"oscAddresses": {"out": {"channelPot3": "/old/{ch}/name"}}})");

  EXPECT_EQ (loadOscAddresses (config).channelThreeD, "/old/{ch}/name");
}


// The addresses are grouped in the file so the Network page can show them
// under headings: what goes out, what comes in, and the beat clock — which
// is neither, because `beat` is sent in INT mode and received in EXT.
TEST (OscAddresses, GroupedBlocksAreRead)
{
  auto const config = juce::JSON::parse (R"({"oscAddresses": {
      "out": { "channelAzimuth": "/o/{ch}/az", "iemAzimuth": "/o/iem" },
      "in":  { "vuPrefix": "/i/", "energyRms": "/i/rms" },
      "beatclock": { "beat": "/b/beat", "tap": "/b/tap",
                     "clockMode": "/b/mode" }}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.channelAzimuth, "/o/{ch}/az");
  EXPECT_EQ (a.iemAzimuth, "/o/iem");
  EXPECT_EQ (a.vuPrefix, "/i/");
  EXPECT_EQ (a.energyRms, "/i/rms");
  EXPECT_EQ (a.beatOut, "/b/beat");
  EXPECT_EQ (a.tap, "/b/tap");
  EXPECT_EQ (a.clockMode, "/b/mode");

  // Untouched keys keep their defaults.
  EXPECT_EQ (a.channelPot1, "/channel/{ch}/pot_1");
}

// A config written before the grouping still works — the block was flat when
// it first shipped, and a file on a device does not rewrite itself.
TEST (OscAddresses, AFlatBlockIsStillRead)
{
  auto const config = juce::JSON::parse (
      R"({"oscAddresses": {"tap": "/flat/tap", "vuPrefix": "/flat/"}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.tap, "/flat/tap");
  EXPECT_EQ (a.vuPrefix, "/flat/");
}

// Grouped wins: if both are present the newer shape is the one meant.
TEST (OscAddresses, AGroupedEntryOverridesAFlatOne)
{
  auto const config = juce::JSON::parse (R"({"oscAddresses": {
      "tap": "/flat/tap",
      "beatclock": { "tap": "/grouped/tap" }}})");

  EXPECT_EQ (loadOscAddresses (config).tap, "/grouped/tap");
}


// The beat has one key that means two things, depending on which group it
// sits in: `out.beat` is what INT mode sends, `in.beat` is what EXT follows.
// They default to the same address and usually stay that way — but the file
// can now say so in both places, which is where a reader looks.
TEST (OscAddresses, BeatIsReadPerDirection)
{
  auto const config = juce::JSON::parse (R"({"oscAddresses": {
      "out": { "beat": "/sent/beat" },
      "in":  { "beat": "/heard/beat" }}})");
  auto const a = loadOscAddresses (config);

  EXPECT_EQ (a.beatOut, "/sent/beat");
  EXPECT_EQ (a.beatIn, "/heard/beat");
}

TEST (OscAddresses, BothBeatsDefaultToTheSameAddress)
{
  auto const a = loadOscAddresses (juce::var{});
  EXPECT_EQ (a.beatOut, a.beatIn);
  EXPECT_EQ (a.beatOut, "/beat");
}
