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

#include "WaitUntil.hh"

#include <OfflineBackend.hh>

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/ChannelSource.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

using namespace a3;

// Engine mode (Plan 4): a channel can be fed by a StemDeck deck instead of
// the analog input. Core says so with /channel/{ch}/source; Motion then shows
// the deck and sends the channel no positions, 3D, FREQ or Q -- the deck's
// stems are placed by Core, not by this device. The trajectories pause, they
// are not deleted.

namespace
{
/** An engine whose channels 0 and 1 are watched: channel 1 stays analog and
 *  is the clock for "the loop has been round". */
struct Watched
{
  HeightMapSphere heightMap;
  OfflineBackend *backend = nullptr;
  std::unique_ptr<MotionEngine> engine;

  Watched ()
  {
    auto owned = std::make_unique<OfflineBackend> ();
    backend = owned.get ();
    engine = std::make_unique<MotionEngine> (4, heightMap, std::move (owned));
    for (index_t channel = 2; channel < 4; ++channel)
      engine->setPreviewMode (channel, true);
  }

  /** Channel 1's first values go out, so every channel has been primed by
   *  the loop (the first pass sends no pot). */
  bool
  primed ()
  {
    return waitUntil ([&] { return backend->positionsOn[1].load () > 0; });
  }

  /** Change everything channels 0 and 1 could send, and wait until channel 1
   *  has sent it: the same tick has passed channel 0 before channel 1. */
  bool
  changeBothAndWaitForChannelOne ()
  {
    for (index_t channel = 0; channel < 2; ++channel)
      {
        engine->setChannel3DPosition (channel,
                                      Pos::fromSpherical (40.f, 10.f, 1.f));
        engine->setChannelPot1 (channel, 0.9f);
        engine->setChannelPot2 (channel, 0.8f);
        engine->setChannelPot3 (channel, 0.7f);
      }
    return waitUntil ([&] {
      return backend->potsOn[1].load () >= 3
             && backend->positionsOn[1].load () >= 2;
    });
  }
};
}

TEST (ChannelSource, DeckChannelSendsNoPositions)
{
  Watched w;
  w.engine->setChannelSource (0, 1);
  ASSERT_TRUE (w.primed ());
  ASSERT_TRUE (w.changeBothAndWaitForChannelOne ());

  EXPECT_EQ (w.backend->positionsOn[0].load (), 0);
  EXPECT_EQ (w.backend->potsOn[0].load (), 0);
}

// Core numbers decks 1..4 (A..D): a channel on deck C or D is a deck channel
// too. Literal numbers, not maxDecks: the wire is the thing being pinned.
TEST (ChannelSource, DecksCAndDSendNoPositionsEither)
{
  for (auto const deck : { 3, 4 })
    {
      Watched w;
      w.engine->setChannelSource (0, deck);
      EXPECT_EQ (w.engine->getChannelSource (0), deck);
      ASSERT_TRUE (w.primed ()) << deck;
      ASSERT_TRUE (w.changeBothAndWaitForChannelOne ()) << deck;

      EXPECT_EQ (w.backend->positionsOn[0].load (), 0) << deck;
      EXPECT_EQ (w.backend->potsOn[0].load (), 0) << deck;
    }
}

TEST (ChannelSource, SourceFiveIsUnknownAndAnalog)
{
  Watched w;
  w.engine->setChannelSource (0, 5);
  EXPECT_EQ (w.engine->getChannelSource (0), analogSource);
  EXPECT_TRUE (
      waitUntil ([&] { return w.backend->positionsOn[0].load () > 0; }));
}

TEST (ChannelSource, TheWireNamesFourDecks)
{
  EXPECT_EQ (sourceFromWire (3), 3);
  EXPECT_EQ (sourceFromWire (4), 4);
  EXPECT_EQ (sourceFromWire (5), analogSource);
  EXPECT_EQ (deckFaceText (3, true), "DECK C");
  EXPECT_EQ (deckFaceText (4, false), "DECK D (absent)");

  ChannelSources sources;
  sources.setSource (2, 4);
  EXPECT_EQ (sources.faceText (2), "DECK D (absent)");
  sources.setPresent (4, true);
  EXPECT_EQ (sources.faceText (2), "DECK D");
  sources.setPresent (5, true);
  EXPECT_FALSE (sources.isPresent (5));
}

TEST (ChannelSource, AnalogAgainResumes)
{
  Watched w;
  w.engine->setChannelSource (0, 2);
  ASSERT_TRUE (w.primed ());
  ASSERT_TRUE (w.changeBothAndWaitForChannelOne ());
  ASSERT_EQ (w.backend->positionsOn[0].load (), 0);

  w.engine->setChannelSource (0, 0);
  EXPECT_TRUE (waitUntil ([&] {
    return w.backend->positionsOn[0].load () > 0
           && w.backend->potsOn[0].load () > 0;
  }));
}

TEST (ChannelSource, UnknownSourceIsAnalog)
{
  for (auto const unknown : { -1, 99, 5 })
    {
      Watched w;
      w.engine->setChannelSource (0, unknown);
      EXPECT_EQ (w.engine->getChannelSource (0), analogSource) << unknown;
      EXPECT_TRUE (waitUntil (
          [&] { return w.backend->positionsOn[0].load () > 0; }))
          << unknown;
    }
}

TEST (ChannelSource, TheWireValueIsADeckOnlyInsideTheRange)
{
  EXPECT_EQ (sourceFromWire (0), analogSource);
  EXPECT_EQ (sourceFromWire (1), 1);
  EXPECT_EQ (sourceFromWire (maxDecks), maxDecks);
  EXPECT_EQ (sourceFromWire (maxDecks + 1), analogSource);
  EXPECT_EQ (sourceFromWire (-3), analogSource);
}

TEST (ChannelSource, DecksAreLettersOnTheFace)
{
  EXPECT_EQ (deckFaceText (analogSource, true), "");
  EXPECT_EQ (deckFaceText (1, true), "DECK A");
  EXPECT_EQ (deckFaceText (2, true), "DECK B");
  EXPECT_EQ (deckFaceText (2, false), "DECK B (absent)");
}

TEST (ChannelSource, ADeckNotReportedPresentIsAbsent)
{
  ChannelSources sources;
  sources.setSource (0, 1);
  EXPECT_EQ (sources.faceText (0), "DECK A (absent)");

  sources.setPresent (1, true);
  EXPECT_EQ (sources.faceText (0), "DECK A");
  EXPECT_EQ (sources.faceText (1), "");

  sources.setPresent (1, false);
  EXPECT_EQ (sources.faceText (0), "DECK A (absent)");
}

// The first boot id Motion hears is only noted: Core answers the recall with
// its channel sources within a moment of Motion's start, and the first
// announcement arrives after that. Resetting on it wiped a valid state.
TEST (ChannelSource, TheFirstBootIdOnlyRecords)
{
  ChannelSources sources;
  sources.setSource (0, 1);
  sources.setPresent (1, true);

  EXPECT_FALSE (sources.coreAnnounced ("boot-1"));
  EXPECT_EQ (sources.source (0), 1) << "the first id wiped the recalled state";
  EXPECT_TRUE (sources.isPresent (1));
  EXPECT_FALSE (sources.coreAnnounced ("boot-1")) << "the same boot again";
  EXPECT_EQ (sources.source (0), 1);
}

TEST (ChannelSource, ANewCoreBringsEverythingBackToAnalog)
{
  ChannelSources sources;
  EXPECT_FALSE (sources.coreAnnounced ("boot-1"));
  sources.setSource (0, 1);
  sources.setSource (3, 2);
  sources.setPresent (1, true);

  EXPECT_TRUE (sources.coreAnnounced ("boot-2"));
  for (int channel = 0; channel < 4; ++channel)
    EXPECT_EQ (sources.source (channel), analogSource) << channel;
  EXPECT_FALSE (sources.isPresent (1));

  sources.setSource (0, 1);
  EXPECT_FALSE (sources.coreAnnounced ("boot-2"));
  EXPECT_EQ (sources.source (0), 1);
}

TEST (ChannelSource, AnAnnouncementWithoutABootIdResetsNothing)
{
  ChannelSources sources;
  sources.setSource (0, 1);
  EXPECT_FALSE (sources.coreAnnounced (""));
  EXPECT_EQ (sources.source (0), 1);
}
