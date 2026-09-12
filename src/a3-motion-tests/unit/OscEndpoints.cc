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

#include <a3-motion-engine/OscEndpoints.hh>

using namespace a3;

namespace
{
juce::var
shippedConfig ()
{
  return juce::JSON::parse (
      juce::File (A3_CONFIG_JSON_PATH).loadFileAsString ());
}
}

// The one that matters. The mixer's fifteen messages spent a branch's worth
// of commits going to the beat-analyzer, because the sender they were handed
// was the beat clock's -- and UDP never answered, so nothing said so. Core is
// where `oscSender.port` points, and it is where the spatial data has always
// gone; this is the claim that the mixer goes to the same place.
TEST (OscEndpoints, CoreIsWhereTheSpatialDataAlreadyGoes)
{
  auto const config = shippedConfig ();
  ASSERT_TRUE (config["oscSender"].isObject ())
      << "the shipped config has no oscSender block to point anywhere";

  auto const endpoints = loadOscEndpoints (config);

  EXPECT_EQ (endpoints.host, config["oscSender"]["host"].toString ());
  EXPECT_EQ (endpoints.corePort,
             static_cast<int> (config["oscSender"]["port"]))
      << "the mixer and MotionEngine's SpatBackendA3 have to agree on where "
         "Core is, and SpatBackendA3 reads oscSender.port";
}

// And the other half of it: the beat clock is a different process on the same
// machine, so the two ports being equal would mean one of the two is wrong.
TEST (OscEndpoints, TheShippedBeatClockGoesSomewhereElse)
{
  auto const endpoints = loadOscEndpoints (shippedConfig ());

  EXPECT_NE (endpoints.corePort, endpoints.beatclockPort)
      << "the shipped config points the beat clock at the beat-analyzer; if "
         "these are equal, one of the two senders is aimed at the wrong "
         "process";
}

TEST (OscEndpoints, ABeatclockPortMovesOnlyTheBeatClock)
{
  auto const config = juce::JSON::parse (R"({
    "oscSender": { "host": "10.0.0.7", "port": 9000, "beatclockPort": 7775 }
  })");
  auto const endpoints = loadOscEndpoints (config);

  EXPECT_EQ (endpoints.host, "10.0.0.7");
  EXPECT_EQ (endpoints.corePort, 9000);
  EXPECT_EQ (endpoints.beatclockPort, 7775);
}

// A config written before the beat clock had a port of its own still has to
// work, and the only reading of it that can be right is that everything goes
// to the one port it names.
TEST (OscEndpoints, WithoutABeatclockPortTheClockGoesWithEverythingElse)
{
  auto const config = juce::JSON::parse (
      R"({ "oscSender": { "host": "127.0.0.1", "port": 9001 } })");
  auto const endpoints = loadOscEndpoints (config);

  EXPECT_EQ (endpoints.corePort, 9001);
  EXPECT_EQ (endpoints.beatclockPort, 9001);
}

TEST (OscEndpoints, AConfigWithoutAnOscSenderBlockKeepsTheDefaults)
{
  auto const endpoints = loadOscEndpoints (juce::var{});

  EXPECT_EQ (endpoints.host, OscEndpoints{}.host);
  EXPECT_EQ (endpoints.corePort, OscEndpoints{}.corePort);
  EXPECT_EQ (endpoints.beatclockPort, OscEndpoints{}.beatclockPort);
}
