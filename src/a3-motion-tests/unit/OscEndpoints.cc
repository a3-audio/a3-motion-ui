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

#include <MadeUpOscTruth.hh>

using namespace a3;

// Where Core listens is where the spatial position and the mixer both go.
// They went to two different places once: the mixer's fifteen messages spent
// a branch going to the beat-analyzer, and UDP never answered to say so.
TEST (OscEndpoints, CoreIsWhereTheTruthSaysCoreListens)
{
  auto const endpoints = oscEndpointsFrom (madeUpOscTruth ());

  EXPECT_EQ (endpoints.core.host, "10.9.9.10");
  EXPECT_EQ (endpoints.core.port, 19000);
}

// The beat clock is a different process: the beat, the tap and the clock mode
// go to the beat-analyzer and nowhere else.
TEST (OscEndpoints, TheBeatClockGoesToTheAnalyzer)
{
  // On the Core's own machine: the analyzer listens on "any", so locally.
  auto const endpoints = oscEndpointsFrom (madeUpOscTruth (), { "10.9.9.10" });

  EXPECT_EQ (endpoints.beatclock.host, "127.0.0.9");
  EXPECT_EQ (endpoints.beatclock.port, 17775);
}

TEST (OscEndpoints, MotionListensWhereTheTruthSays)
{
  auto const endpoints = oscEndpointsFrom (madeUpOscTruth ());

  EXPECT_EQ (endpoints.receivePort, 17771);
  EXPECT_EQ (endpoints.vuPort, 17772);
  EXPECT_EQ (endpoints.energyPort, 17777);
}

// Nothing is made up: without the truth there is no port, and a socket asked
// to open on none does not open -- which the log says -- rather than opening
// on a number somebody once typed into the code.
TEST (OscEndpoints, WithoutTheTruthThereIsNowhere)
{
  auto const endpoints = oscEndpointsFrom (parseOscTruth ("{ not json"));

  EXPECT_EQ (endpoints.core.port, -1);
  EXPECT_TRUE (endpoints.core.host.isEmpty ());
  EXPECT_EQ (endpoints.beatclock.port, -1);
  EXPECT_EQ (endpoints.receivePort, -1);
  EXPECT_EQ (endpoints.vuPort, -1);
  EXPECT_EQ (endpoints.energyPort, -1);
}

// Motion on a machine of its own (a3nuc2, 2026-10-06): Core and the analyzer
// listen on "any", which Motion read as "here" and sent its hello, the tap and
// the beat to itself. Away from the Core they are at the truth's core host.
namespace
{
OscTruth
truthWithCoreOnAny ()
{
  return parseOscTruth (R"({
    "hosts": { "local": "127.0.0.9", "any": "0.0.0.0", "core": "10.9.9.10" },
    "listeners": [
      { "program": "core", "role": "osc", "host": "any", "port": 19000 },
      { "program": "beat-analyzer", "role": "clock", "host": "any", "port": 17775 },
      { "program": "motion", "role": "osc", "host": "any", "port": 17771 }
    ],
    "addresses": {}
  })");
}
}

TEST (OscEndpoints, AwayFromTheCoreCoreIsAtTheCoreHost)
{
  auto const endpoints = oscEndpointsFrom (truthWithCoreOnAny (), { "10.9.9.20" });

  EXPECT_EQ (endpoints.core.host, "10.9.9.10");
  EXPECT_EQ (endpoints.core.port, 19000);
  EXPECT_EQ (endpoints.beatclock.host, "10.9.9.10");
  EXPECT_EQ (endpoints.beatclock.port, 17775);
}

TEST (OscEndpoints, OnTheCoreCoreStaysLocal)
{
  auto const endpoints
      = oscEndpointsFrom (truthWithCoreOnAny (), { "127.0.0.1", "10.9.9.10" });

  EXPECT_EQ (endpoints.core.host, "127.0.0.9");
  EXPECT_EQ (endpoints.beatclock.host, "127.0.0.9");
}

TEST (OscEndpoints, MotionsOwnSocketsDoNotMove)
{
  auto const endpoints = oscEndpointsFrom (truthWithCoreOnAny (), { "10.9.9.20" });

  EXPECT_EQ (endpoints.receivePort, 17771);
}
