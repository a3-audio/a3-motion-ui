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
#include <a3-motion-engine/OscTruth.hh>
#include <a3-motion-ui/osc/VuRouting.hh>

using namespace a3;

// Motion against the real a3-osc.json -- a3-core ships it, so it is not in
// this repository. $A3_OSC_TRUTH points at a checkout's copy; without it the
// installed one is read (test.sh finds one and says which). Without either
// the suite fails rather than skips: ctest counts a skip as passed, and a
// green run that held Motion against nothing is the kind that lies.

namespace
{
OscTruth
realTruth ()
{
  return loadOscTruth (oscTruthFile ());
}
}

TEST (OscTruthContract, TheTruthHasEveryAddressMotionSpeaks)
{
  auto const truth = realTruth ();
  ASSERT_TRUE (truth.isValid ()) << truth.error ();

  EXPECT_EQ (missingOscKeys (truth).joinIntoString (", "), "");
}

TEST (OscTruthContract, TheTruthSaysWhereMotionSendsAndListens)
{
  auto const truth = realTruth ();
  ASSERT_TRUE (truth.isValid ()) << truth.error ();

  EXPECT_TRUE (truth.endpoint ("core", "osc").has_value ());
  EXPECT_TRUE (truth.endpoint ("beat-analyzer", "clock").has_value ());
  for (auto const *role : { "osc", "vu", "energy" })
    EXPECT_GT (truth.port ("motion", role), 0) << role;
}

TEST (OscTruthContract, TheChannelMapHasEveryMeterMotionShows)
{
  auto const truth = realTruth ();
  ASSERT_TRUE (truth.isValid ()) << truth.error ();

  juce::StringArray missing;
  for (auto const &name : vuMeterNames ())
    if (truth.vuNumber (name) == 0)
      missing.add (name);
  EXPECT_EQ (missing.joinIntoString (", "), "");
}
