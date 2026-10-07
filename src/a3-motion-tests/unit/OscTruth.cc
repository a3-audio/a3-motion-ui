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

#include <a3-motion-engine/MeterBallistics.hh>
#include <a3-motion-engine/OscTruth.hh>

#include <cstdlib>

using namespace a3;

namespace
{
// The shape of a3-core's a3-osc.json, cut down to what these tests ask. Not a
// copy of the real file: the real one is checked in OscTruthContract.
constexpr char const *sample = R"({
  "hosts": { "core": "192.168.8.10", "local": "127.0.0.1", "any": "0.0.0.0" },
  "listeners": [
    { "program": "core", "role": "osc", "host": "any", "port": 9000 },
    { "program": "mixer", "role": "osc", "host": "core", "port": 7772 }
  ],
  "addresses": {
    "channel.volume": { "pattern": "/channel/{ch}/volume", "ch": [1, 4] },
    "vu": { "pattern": "/vu/{n}", "n": [1, 3] }
  },
  "vu_meters": [ "in1_pre", "in2_pre", "main_sub" ]
})";
}

TEST (OscTruth, AnswersWhereAProgramListens)
{
  auto const truth = parseOscTruth (sample);
  ASSERT_TRUE (truth.isValid ()) << truth.error ();

  EXPECT_EQ (truth.host ("core"), "192.168.8.10");
  EXPECT_EQ (truth.port ("mixer", "osc"), 7772);
}

// A listener on every interface is reached on this machine: Motion runs on
// the Core box, and "0.0.0.0" is not an address anything can be sent to.
TEST (OscTruth, AWildcardListenerIsReachedLocally)
{
  auto const endpoint = parseOscTruth (sample).endpoint ("core", "osc");
  ASSERT_TRUE (endpoint.has_value ());
  EXPECT_EQ (endpoint->host, "127.0.0.1");
  EXPECT_EQ (endpoint->port, 9000);
}

TEST (OscTruth, AnEndpointOnANamedHostIsThatHost)
{
  auto const endpoint = parseOscTruth (sample).endpoint ("mixer", "osc");
  ASSERT_TRUE (endpoint.has_value ());
  EXPECT_EQ (endpoint->host, "192.168.8.10");
}

TEST (OscTruth, GivesAnAddressItsPattern)
{
  EXPECT_EQ (parseOscTruth (sample).pattern ("channel.volume"),
             "/channel/{ch}/volume");
}

// Motion asks for a meter by what it measures, never by its number: the
// number is the channel map's business and moved once already (0..39 to 1..40).
TEST (OscTruth, NumbersAMeterByItsPlaceInTheMap)
{
  auto const truth = parseOscTruth (sample);
  EXPECT_EQ (truth.vuNumber ("in1_pre"), 1);
  EXPECT_EQ (truth.vuNumber ("main_sub"), 3);
}

// Nothing is made up. A fact the file does not have comes back as nothing,
// and the caller decides what that costs -- a guessed port would be a second
// truth nobody wrote down.
TEST (OscTruth, AFactItDoesNotHaveIsNothing)
{
  auto const truth = parseOscTruth (sample);
  EXPECT_TRUE (truth.host ("radla").isEmpty ());
  EXPECT_EQ (truth.port ("radla", "osc"), -1);
  EXPECT_FALSE (truth.endpoint ("radla", "osc").has_value ());
  EXPECT_TRUE (truth.pattern ("channel.nothing").isEmpty ());
  EXPECT_EQ (truth.vuNumber ("main_top99"), 0);
}

TEST (OscTruth, UnreadableJsonIsSaid)
{
  auto const truth = parseOscTruth ("{ not json");
  EXPECT_FALSE (truth.isValid ());
  EXPECT_FALSE (truth.error ().isEmpty ());
}

TEST (OscTruth, AMissingFileIsSaidWithItsPath)
{
  auto const missing = juce::File::getSpecialLocation (
                           juce::File::tempDirectory)
                           .getChildFile ("a3-osc-does-not-exist.json");
  auto const truth = loadOscTruth (missing);
  EXPECT_FALSE (truth.isValid ());
  EXPECT_TRUE (truth.error ().contains (missing.getFullPathName ()));
}

TEST (OscTruth, TheEnvironmentPointsAtAnotherFile)
{
  ::setenv ("A3_OSC_TRUTH", "/tmp/elsewhere.json", 1);
  EXPECT_EQ (oscTruthFile ().getFullPathName (), "/tmp/elsewhere.json");
  ::unsetenv ("A3_OSC_TRUTH");
  // Without the override the start order decides (cache, then the package):
  // tests/unit/TruthKeeper.cc holds it with a temporary home.
}

// The meters' ballistics are Core's to set (2026-10-07): one block in the
// truth, read by every display. A truth from before it carries none.
TEST (OscTruth, ATruthWithoutMetersGivesTheSharedRule)
{
  auto const ballistics = parseOscTruth (sample).meterBallistics ();
  MeterBallisticsParameters const defaults;
  EXPECT_FLOAT_EQ (ballistics.attackMs, defaults.attackMs);
  EXPECT_FLOAT_EQ (ballistics.releaseDbPerSecond, defaults.releaseDbPerSecond);
  EXPECT_FLOAT_EQ (ballistics.peakHoldSeconds, defaults.peakHoldSeconds);
}

TEST (OscTruth, ReadsTheMetersBallistics)
{
  auto const truth = parseOscTruth (R"({
    "meters": { "attack_ms": 5, "release_db_per_second": 30,
                "peak_hold_seconds": 2.5 }
  })");
  ASSERT_TRUE (truth.isValid ()) << truth.error ();

  auto const ballistics = truth.meterBallistics ();
  EXPECT_FLOAT_EQ (ballistics.attackMs, 5.f);
  EXPECT_FLOAT_EQ (ballistics.releaseDbPerSecond, 30.f);
  EXPECT_FLOAT_EQ (ballistics.peakHoldSeconds, 2.5f);
}

// A key the block lacks, or one that is not a number, is the rule's.
TEST (OscTruth, AMissingOrBrokenMetersKeyIsTheRules)
{
  auto const truth = parseOscTruth (R"({
    "meters": { "release_db_per_second": 30, "peak_hold_seconds": "long" }
  })");
  auto const ballistics = truth.meterBallistics ();
  MeterBallisticsParameters const defaults;
  EXPECT_FLOAT_EQ (ballistics.attackMs, defaults.attackMs);
  EXPECT_FLOAT_EQ (ballistics.releaseDbPerSecond, 30.f);
  EXPECT_FLOAT_EQ (ballistics.peakHoldSeconds, defaults.peakHoldSeconds);
}
