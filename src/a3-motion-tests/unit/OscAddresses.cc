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
  EXPECT_EQ (a.channelPot3, "/channel/{ch}/pot_3");
  EXPECT_EQ (a.iemAzimuth, "/StereoEncoder/azimuth");
  EXPECT_EQ (a.iemElevation, "/StereoEncoder/elevation");
  EXPECT_EQ (a.beat, "/beat");
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
  EXPECT_EQ (a.beat, "/beat");
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
      "channelPot3":      "/a/{ch}/r",
      "iemAzimuth":       "/b/x",
      "iemElevation":     "/b/y",
      "beat":             "/c/beat",
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
  EXPECT_NE (a.channelPot3, d.channelPot3);
  EXPECT_NE (a.iemAzimuth, d.iemAzimuth);
  EXPECT_NE (a.iemElevation, d.iemElevation);
  EXPECT_NE (a.beat, d.beat);
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
                               a.channelPot3, a.iemAzimuth,
                               a.iemElevation, a.beat, a.tap, a.clockMode })
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


// pot_3 deliberately, not `3d`: A3 Core dispatches on the last path element,
// and `3d` there is a toggle that fires on the value 1 — a continuous value
// sent to it would flip the state every time it passed 1.0 and do nothing
// otherwise. See a3-core.py's osc_handler_channel.
TEST (OscAddresses, TheThirdPotDoesNotCollideWithCoresThreeDeeToggle)
{
  auto const a = loadOscAddresses (juce::var{});

  EXPECT_FALSE (withChannel (a.channelPot3, 0).endsWith ("/3d"));
  EXPECT_TRUE (withChannel (a.channelPot3, 2).endsWith ("/pot_3"));
}
