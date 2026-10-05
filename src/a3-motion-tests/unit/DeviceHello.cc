/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/DeviceHello.hh>
#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/OscTruth.hh>

#include <MadeUpOscTruth.hh>

using namespace a3;

// Motion says /device/hello to Core like the desk and StemDeck do: once at
// start, then every 30 s for as long as it runs -- a Core restarted after the
// first one would otherwise never hear it again (the desk, 2026-10-01).

TEST (HelloSchedule, TheFirstHelloIsDueAtStart)
{
  HelloSchedule const schedule;
  EXPECT_TRUE (schedule.due (0.));
  EXPECT_TRUE (schedule.due (123456.));
}

TEST (HelloSchedule, TheNextOneIsDueThirtySecondsAfterTheLast)
{
  HelloSchedule schedule;
  schedule.said (1000.);
  EXPECT_FALSE (schedule.due (1000.));
  EXPECT_FALSE (schedule.due (1000. + 29999.));
  EXPECT_TRUE (schedule.due (1000. + 30000.));
}

TEST (HelloSchedule, ItKeepsSayingItForAsLongAsMotionRuns)
{
  HelloSchedule schedule;
  auto said = 0;
  for (double now = 0.; now < 10. * 60. * 1000.; now += 50.)
    if (schedule.due (now))
      {
        schedule.said (now);
        ++said;
      }
  EXPECT_EQ (said, 20);
}

TEST (DeviceHello, NamesMotionAndTheTruthItSpeaks)
{
  auto const addresses = oscAddressesFrom (madeUpOscTruth ());
  auto const message = helloMessage (addresses, "abc123");

  EXPECT_EQ (message.getAddressPattern ().toString (), "/t/device.hello");
  ASSERT_EQ (message.size (), 2);
  ASSERT_TRUE (message[0].isString ());
  ASSERT_TRUE (message[1].isString ());
  EXPECT_EQ (message[0].getString (), "motion");
  EXPECT_EQ (message[1].getString (), "abc123");
}

// Core's fingerprint (a3_osc_join.fingerprint) is the sha256 of the truth's
// canonical form: json.dumps(sort_keys=True, separators=(",", ":"),
// ensure_ascii=False) in UTF-8. That form is what Core serves at /api/truth
// and what Motion keeps in ~/.cache/a3, so the hash of the bytes Motion read
// is Core's own. Both values below came out of Core's code:
//   canonical(d).decode() and fingerprint(d), for this d.
TEST (DeviceHello, TheDigestOfCoresBodyIsCoresFingerprint)
{
  auto const file = juce::File::createTempFile ("json");
  file.replaceWithText (
      R"({"addresses":{"device.hello":{"args":"ss","pattern":"/device/hello"}},)"
      R"("hosts":{"core":"127.0.0.1"},)"
      R"("listeners":[{"port":9000,"program":"core","role":"osc"}]})",
      false, false, nullptr);
  EXPECT_EQ (loadOscTruth (file).digest (),
             "23566ae8066d4b1aaf7ccaea6233595dcc7fae1b63e6bd687efae30d5e09ee27");
  file.deleteFile ();
}

// "Green is not called": the schedule is only worth something if the app
// asks it, with the digest of the truth it actually loaded.
TEST (DeviceHello, TheAppSaysHelloWithTheLoadedDigest)
{
  auto const ui = juce::File (__FILE__)
                      .getParentDirectory ()
                      .getParentDirectory ()
                      .getSiblingFile ("a3-motion-ui")
                      .getChildFile ("components/A3MotionUIComponent.cc");
  auto const text = ui.loadFileAsString ();
  ASSERT_TRUE (text.isNotEmpty ()) << ui.getFullPathName ();
  EXPECT_TRUE (text.contains ("helloMessage (_oscAddresses, "
                              "installedOscTruth ().digest ())"));
  EXPECT_TRUE (text.contains ("sayHelloWhenDue ();"));
}
