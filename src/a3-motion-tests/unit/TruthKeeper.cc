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

#include <a3-motion-engine/OscTruth.hh>
#include <a3-motion-engine/TruthKeeper.hh>

// Motion takes its truth from Core like the desk and StemDeck (spec
// truth-from-core, step 3). The same decisions as StemDeck's keeper, the
// same tests; and the start order with real files.

namespace tk = a3::truthkeeper;

TEST (KeeperDecide, TheSameFingerprintNeedsNoFetch)
{
  EXPECT_FALSE (tk::needsFetch (std::string (64, 'a'), std::string (64, 'a')));
  EXPECT_TRUE (tk::needsFetch (std::string (64, 'b'), std::string (64, 'a')));
}

TEST (KeeperDecide, AnOverrideIsNotFollowed)
{
  EXPECT_TRUE (tk::followsCore (nullptr));
  EXPECT_TRUE (tk::followsCore (""));
  EXPECT_FALSE (tk::followsCore ("/x.json"));
}

TEST (KeeperVerify, BodyHeaderAndAnnouncementMustAgree)
{
  std::string const h (64, 'c');
  EXPECT_TRUE (tk::verified (h, h, h));
  EXPECT_FALSE (tk::verified (h, h, std::string (64, 'd')));
  EXPECT_FALSE (tk::verified (h, std::string (64, 'd'), h));
  EXPECT_FALSE (tk::verified ("", "", ""));
}

TEST (KeeperStart, OverrideThenCacheThenPackage)
{
  EXPECT_EQ (tk::startPath ("/o.json", "/c.json", true, "/p.json"), "/o.json");
  EXPECT_EQ (tk::startPath (nullptr, "/c.json", true, "/p.json"), "/c.json");
  EXPECT_EQ (tk::startPath ("", "/c.json", true, "/p.json"), "/c.json");
}

TEST (KeeperStart, AnUnparsableCacheIsSkipped)
{
  EXPECT_EQ (tk::startPath (nullptr, "/c.json", false, "/p.json"), "/p.json");
}

TEST (KeeperStart, TheCacheIsInHome)
{
  EXPECT_EQ (tk::cachePath ("/home/aaa"), "/home/aaa/.cache/a3/a3-osc.json");
}

namespace
{
juce::File
freshHome ()
{
  auto home = juce::File::createTempFile ("home");
  home.createDirectory ();
  return home;
}
}

/** The truth the suite runs against ($A3_OSC_TRUTH, a3-core's). */
juce::File
suiteTruth ()
{
  return juce::File (juce::SystemStats::getEnvironmentVariable ("A3_OSC_TRUTH", {}));
}

TEST (KeeperStart, MotionReadsAUsableCacheFirst)
{
  auto const home = freshHome ();
  auto const cache = home.getChildFile (".cache/a3/a3-osc.json");
  cache.create ();
  ASSERT_TRUE (suiteTruth ().existsAsFile ()) << "set A3_OSC_TRUTH";
  cache.replaceWithText (suiteTruth ().loadFileAsString ());
  EXPECT_EQ (a3::oscTruthFileFrom (nullptr, home).getFullPathName (), cache.getFullPathName ());
  home.deleteRecursively ();
}

TEST (KeeperStart, MotionSkipsAGarbageCache)
{
  auto const home = freshHome ();
  auto const cache = home.getChildFile (".cache/a3/a3-osc.json");
  cache.create ();
  cache.replaceWithText ("{");
  EXPECT_EQ (a3::oscTruthFileFrom (nullptr, home).getFullPathName (), "/usr/share/a3/a3-osc.json");
  home.deleteRecursively ();
}

TEST (KeeperStart, MotionWithoutACacheReadsThePackage)
{
  auto const home = freshHome ();
  EXPECT_EQ (a3::oscTruthFileFrom (nullptr, home).getFullPathName (), "/usr/share/a3/a3-osc.json");
  EXPECT_EQ (a3::oscTruthFileFrom ("/tmp/elsewhere.json", home).getFullPathName (), "/tmp/elsewhere.json");
  home.deleteRecursively ();
}

// Final review 2026-10-02: the start check is the keeper's own. A truth Motion
// refused (it lacks an address Motion speaks) must not come back through the
// cache StemDeck shares with it.
TEST (KeeperStart, MotionSkipsACacheThatLacksItsAddresses)
{
  auto const home = freshHome ();
  auto const cache = home.getChildFile (".cache/a3/a3-osc.json");
  cache.create ();
  cache.replaceWithText (R"({"addresses": {"channel.volume": {"pattern": "/channel/{ch}/volume"}}})");
  EXPECT_EQ (a3::oscTruthFileFrom (nullptr, home).getFullPathName (), "/usr/share/a3/a3-osc.json");
  EXPECT_TRUE (a3::unusableOscTruth (cache.loadFileAsString ()).isNotEmpty ());
  home.deleteRecursively ();
}

// The fingerprint Motion compares is of the bytes it loaded, taken when it
// loaded them -- not a second read of a file StemDeck may rewrite meanwhile.
TEST (KeeperOwn, TheTruthKnowsTheDigestOfWhatWasRead)
{
  auto const file = juce::File::createTempFile ("json");
  file.replaceWithText (R"({"addresses": {}})");
  EXPECT_EQ (a3::loadOscTruth (file).digest (),
             juce::SHA256 (file).toHexString ());
  file.deleteFile ();
}

TEST (KeeperOwn, MotionHandsTheLoadedDigestToTheKeeper)
{
  auto const app = juce::File (__FILE__).getParentDirectory ().getParentDirectory ()
                       .getSiblingFile ("a3-motion-ui").getChildFile ("StandaloneApp.cc");
  auto const text = app.loadFileAsString ();
  ASSERT_TRUE (text.isNotEmpty ()) << app.getFullPathName ();
  EXPECT_TRUE (text.contains ("installedOscTruth ().digest ()"));
  EXPECT_FALSE (text.contains ("SHA256 (oscTruthFile ())"));
}

// The two literals the keeper knows before it has a truth (a3-core's guard
// allows them by name) are the truth's own -- else it goes deaf, suites green.
TEST (KeeperBootstrap, PortAndWordAreTheTruths)
{
  ASSERT_TRUE (suiteTruth ().existsAsFile ()) << "set A3_OSC_TRUTH";
  auto const truth = a3::loadOscTruth (suiteTruth ());
  EXPECT_EQ (truth.pattern ("core.here"), juce::String (tk::announceAddress));
  juce::var parsed;
  ASSERT_TRUE (juce::JSON::parse (suiteTruth ().loadFileAsString (), parsed).wasOk ());
  auto port = -1;
  for (auto const &listener : *parsed["listeners"].getArray ())
    if (listener["program"] == "devices" && listener["role"] == "announce")
      port = (int) listener["port"];
  EXPECT_EQ (port, tk::announcePort);
}

// Final review of step 3 (minor 4): members die in reverse order, so the pool
// -- whose destructor waits for a job still running -- must be the last one,
// or that job writes `_busy` and `_lastReason` after they are gone.
TEST (KeeperLink, ThePoolIsTheLastMember)
{
  auto const header = juce::File (__FILE__).getParentDirectory ().getParentDirectory ()
                          .getSiblingFile ("a3-motion-engine").getChildFile ("TruthKeeperLink.hh");
  auto const text = header.loadFileAsString ();
  ASSERT_TRUE (text.isNotEmpty ()) << header.getFullPathName ();
  auto const pool = text.indexOf ("juce::ThreadPool _fetcher");
  ASSERT_GE (pool, 0);
  auto const rest = text.substring (pool, text.indexOf ("JUCE_DECLARE_NON_COPYABLE"));
  EXPECT_FALSE (rest.contains ("_busy")) << rest;
  EXPECT_FALSE (rest.contains ("_lastReason")) << rest;
}

// Motion opens its window once, on Core's truth if Core announces it within
// 10 s, else on the truth on disk (decided 2026-10-07, a3-system#74).
TEST (KeeperStartup, TheWaitIsTenSeconds)
{
  EXPECT_EQ (tk::startupWaitMs, 10000);
}

TEST (KeeperStartup, CoresTruthOnDiskOpensAtOnce)
{
  tk::StartupWait wait ("abc");
  EXPECT_FALSE (wait.isOpen ());
  EXPECT_EQ (wait.announced ("abc"), tk::StartupStep::open);
  EXPECT_TRUE (wait.isOpen ());
}

TEST (KeeperStartup, AnotherTruthIsFetchedAndThenOpened)
{
  tk::StartupWait wait ("old");
  EXPECT_EQ (wait.announced ("new"), tk::StartupStep::fetch);
  EXPECT_FALSE (wait.isOpen ());
  EXPECT_EQ (wait.fetched (), tk::StartupStep::open);
}

TEST (KeeperStartup, AnnouncementsDuringTheFetchAreIgnored)
{
  tk::StartupWait wait ("old");
  EXPECT_EQ (wait.announced ("new"), tk::StartupStep::fetch);
  EXPECT_EQ (wait.announced ("new"), tk::StartupStep::stay);
  EXPECT_EQ (wait.announced ("old"), tk::StartupStep::stay);
}

TEST (KeeperStartup, SilenceOpensAtTheTimeout)
{
  tk::StartupWait wait ("abc");
  EXPECT_EQ (wait.timedOut (), tk::StartupStep::open);
}

TEST (KeeperStartup, ItOpensOnlyOnce)
{
  tk::StartupWait wait ("abc");
  EXPECT_EQ (wait.announced ("abc"), tk::StartupStep::open);
  EXPECT_EQ (wait.announced ("abc"), tk::StartupStep::stay);
  EXPECT_EQ (wait.announced ("xyz"), tk::StartupStep::stay);
  EXPECT_EQ (wait.timedOut (), tk::StartupStep::stay);
  EXPECT_EQ (wait.fetched (), tk::StartupStep::stay);
}

TEST (KeeperStartup, AFetchLandingAfterTheTimeoutDoesNotOpenAgain)
{
  tk::StartupWait wait ("old");
  EXPECT_EQ (wait.announced ("new"), tk::StartupStep::fetch);
  EXPECT_EQ (wait.timedOut (), tk::StartupStep::open);
  EXPECT_EQ (wait.fetched (), tk::StartupStep::stay);
}

TEST (KeeperStartup, NoTruthOnDiskFetchesAnyAnnouncement)
{
  tk::StartupWait wait ("");
  EXPECT_EQ (wait.announced ("abc"), tk::StartupStep::fetch);
}

// The candidate Motion waits with is the digest of the file it will load.
TEST (KeeperOwn, TheFileDigestIsTheDigestOfWhatLoads)
{
  auto const file = juce::File::createTempFile ("json");
  file.replaceWithText (R"({"addresses": {}})");
  EXPECT_EQ (a3::oscTruthFileDigest (file), a3::loadOscTruth (file).digest ());
  file.deleteFile ();
  EXPECT_TRUE (a3::oscTruthFileDigest (file).isEmpty ());
}
