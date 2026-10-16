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
#include <UiSource.hh>

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-ui/io/PanelPot.hh>

using namespace a3;

// The truth comes from Core (maintainer, 2026-10-14). Motion sends a channel
// or mixer value only when a hand changed it, or a set was loaded by hand --
// never because it started, restarted or reconnected. On 2026-10-14 Core
// rebooted, Motion came up first on its own values and pushed them, and the
// rig went silent with Core's own recall already behind it.

namespace
{

constexpr index_t numChannels = 4;

/** Long enough for dozens of ticks at any tempo: the engine sends from its
 *  clock thread, every two to eight milliseconds. */
constexpr int ticksMs = 300;

/** What applySet() does at start-up: every channel's freq, Q and 3d taken
 *  from the set on disk, while Core has not answered yet. */
void
applyASetsChannelValues (MotionEngine &engine)
{
  for (index_t channel = 0; channel < numChannels; ++channel)
    {
      engine.setChannelPot1 (channel, 0.2f);
      engine.setChannelPot2 (channel, 0.7f);
      engine.setChannelPot3 (channel, 0.9f);
    }
}

struct StartedEngine
{
  StartedEngine ()
  {
    auto owned = std::make_unique<OfflineBackend> ();
    backend = owned.get ();
    engine = std::make_unique<MotionEngine> (numChannels, heightMap,
                                             std::move (owned));
  }

  HeightMapSphere heightMap;
  OfflineBackend *backend = nullptr;
  std::unique_ptr<MotionEngine> engine;
};

}

TEST (NoUnaskedValues, AStartWithCoreSilentSendsNoChannelValue)
{
  StartedEngine started;
  // The set is applied while the start-up hold waits for Core's answer, and
  // no answer comes.
  started.engine->holdOutputUntil (juce::Time::getMillisecondCounterHiRes ()
                                   + 50.);
  applyASetsChannelValues (*started.engine);

  juce::Thread::sleep (ticksMs);

  EXPECT_EQ (started.backend->potsSent.load (), 0)
      << "freq, Q or 3d went to Core although nobody touched them";
}

TEST (NoUnaskedValues, AStartWithoutAHoldSendsNoChannelValueEither)
{
  StartedEngine started;
  applyASetsChannelValues (*started.engine);

  juce::Thread::sleep (ticksMs);

  EXPECT_EQ (started.backend->potsSent.load (), 0);
}

TEST (NoUnaskedValues, AHandChangeAfterTheStartIsSent)
{
  StartedEngine started;
  applyASetsChannelValues (*started.engine);
  juce::Thread::sleep (ticksMs);
  ASSERT_EQ (started.backend->potsSent.load (), 0);

  // A hand on FREQ.
  started.engine->setChannelPot1 (2, 0.6f);

  EXPECT_TRUE (waitUntil ([&] { return started.backend->potsSent.load () > 0; }))
      << "a turned knob never reached Core";
}

TEST (NoUnaskedValues, AValueAdoptedFromCoreIsNotSentBack)
{
  StartedEngine started;
  applyASetsChannelValues (*started.engine);
  juce::Thread::sleep (ticksMs);
  ASSERT_EQ (started.backend->potsSent.load (), 0);

  // Core answers late, after the hold: its values are taken, not echoed or
  // ramped back at it.
  started.engine->adoptChannelPot1 (1, 0.4f);
  started.engine->adoptChannelPot2 (1, 0.1f);
  started.engine->adoptChannelPot3 (1, 0.3f);

  EXPECT_TRUE (waitUntil ([&] {
    return juce::approximatelyEqual (started.engine->getChannelPot3 (1), 0.3f);
  })) << "Core's value never reached the channel";
  EXPECT_FLOAT_EQ (started.engine->getChannelPot1 (1), 0.4f);
  EXPECT_FLOAT_EQ (started.engine->getChannelPot2 (1), 0.1f);

  juce::Thread::sleep (ticksMs);
  EXPECT_EQ (started.backend->potsSent.load (), 0)
      << "Core's own value was sent back to it";
}

TEST (NoUnaskedValues, AHandChangeAfterAnAdoptedValueIsSent)
{
  StartedEngine started;
  started.engine->adoptChannelPot3 (0, 0.3f);
  juce::Thread::sleep (ticksMs);
  ASSERT_EQ (started.backend->potsSent.load (), 0);

  started.engine->setChannelPot3 (0, 0.8f);

  EXPECT_TRUE (waitUntil ([&] { return started.backend->potsSent.load () > 0; }));
}

// The set restored at start-up is Motion's own business -- slots, clips,
// actions, positions -- and none of it reaches Core's mixer: the one road
// there is MixerState's touch path, and applySet() takes no part in it.
TEST (NoUnaskedValues, ApplyingASetTouchesNoMixerValue)
{
  auto const body = test::uiComponentBodyOf (
      "A3MotionUIComponent::applySet (juce::File const &file)\n{");
  ASSERT_TRUE (body.isNotEmpty ());

  EXPECT_FALSE (body.contains ("FromTouch"));
  EXPECT_FALSE (body.contains ("_mixerState"));
  EXPECT_FALSE (body.contains ("_mixerSender"));
}

// Core's answer to the recall is adopted, not set: set would have the next
// tick ramp it back at Core.
TEST (NoUnaskedValues, CoresChannelValuesAreAdopted)
{
  auto const body = test::uiComponentBodyOf (
      "A3MotionUIComponent::onChannelValue (");
  ASSERT_TRUE (body.isNotEmpty ());

  EXPECT_TRUE (body.contains ("adoptChannelPot1"));
  EXPECT_TRUE (body.contains ("adoptChannelPot2"));
  EXPECT_TRUE (body.contains ("adoptChannelPot3"));
  EXPECT_FALSE (body.contains ("setChannelPot"));
}

// The panel's 3d pots are absolute. Their first reading is where they happen
// to stand when Motion starts -- not a hand -- and sending it would set every
// channel's 3d to the panel at each start, restart and reconnect.
TEST (PanelPot, TheFirstReadingIsABaselineNotAChange)
{
  PanelPot pot;
  EXPECT_FALSE (pot.read (3000).has_value ());
}

TEST (PanelPot, AMoveBeyondTheDeadbandIsReported)
{
  PanelPot pot;
  pot.read (3000);

  auto const moved = pot.read (3000 + PanelPot::deadband + 1);

  ASSERT_TRUE (moved.has_value ());
  EXPECT_FLOAT_EQ (*moved, static_cast<float> (3000 + PanelPot::deadband + 1)
                               / PanelPot::fullScale);
}

TEST (PanelPot, JitterInsideTheDeadbandIsNot)
{
  PanelPot pot;
  pot.read (3000);

  EXPECT_FALSE (pot.read (3000 + PanelPot::deadband).has_value ());
  EXPECT_FALSE (pot.read (3000 - PanelPot::deadband).has_value ());
}

TEST (PanelPot, AReportedMoveIsTheNewReference)
{
  PanelPot pot;
  pot.read (1000);
  ASSERT_TRUE (pot.read (2000).has_value ());

  EXPECT_FALSE (pot.read (2000 + PanelPot::deadband).has_value ());
  EXPECT_TRUE (pot.read (1000).has_value ());
}

// protocol.h documents the pots as 0..4095 and the firmware now ends there:
// the end stops read exactly 0 and 1 (2026-10-17: they read 1/4095 too much).
TEST (PanelPot, TheEndStopsReadExactlyZeroAndOne)
{
  PanelPot pot;
  pot.read (2000);
  auto const top = pot.read (static_cast<int> (PanelPot::fullScale));
  ASSERT_TRUE (top.has_value ());
  EXPECT_EQ (*top, 1.f);

  auto const bottom = pot.read (0);
  ASSERT_TRUE (bottom.has_value ());
  EXPECT_EQ (*bottom, 0.f);
}
