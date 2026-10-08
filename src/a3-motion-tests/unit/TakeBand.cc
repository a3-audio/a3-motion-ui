/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TakeSeed.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <memory>

using namespace a3;

// A take keeps the whole sphere from the moment it is laid out until it is
// saved or thrown away (maintainer, 2026-10-08). The knobs on the bar are
// locked for that; these are the other ways settings reach a pattern -- an
// accent, a clip loaded onto it -- and they must leave the band alone too.

namespace
{
/** Settings that would narrow a band if they landed: a base at ear height,
 *  a small reach, both clips, a sway, a swell, flat -- and a turn, which
 *  should land. */
ClipSettings aNarrowBandAndATurn ()
{
  ClipSettings settings;
  settings.elevationBase = 0.5f;
  settings.reach = 0.3f;
  settings.clipTop = 0.2f;
  settings.clipBottom = 0.1f;
  settings.elevationLfo = 3;
  settings.reachLfo = -2;
  settings.flat = true;
  settings.rotate = 0.4f;
  return settings;
}

void
expectTheWholeSphere (Pattern const &pattern)
{
  auto const params = pattern.getElevationParams ();
  EXPECT_FLOAT_EQ (params.elevationBase, 0.f);
  EXPECT_FLOAT_EQ (params.reach, 1.f);
  EXPECT_FLOAT_EQ (params.clipTop, 0.f);
  EXPECT_FLOAT_EQ (params.clipBottom, 0.f);
  EXPECT_FALSE (params.flat);
  EXPECT_EQ (pattern.getElevationLfo (), 0);
  EXPECT_EQ (pattern.getReachLfo (), 0);
}

std::shared_ptr<Pattern>
aHeldTake ()
{
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->resize (64);
  openToTheWholeSphere (*take);
  take->setBandHeld (true);
  return take;
}
}

TEST (TakeBand, ATakeHoldsItsBandFromTheMomentItIsAskedFor)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  EXPECT_FALSE (take->isBandHeld ());

  engine.recordPattern (take, Measure{ 8, 0, 0 }, Measure{ 1, 0, 0 });
  EXPECT_TRUE (take->isBandHeld ());
}

TEST (TakeBand, SettingsSpareAHeldBand)
{
  auto take = aHeldTake ();
  applyClipSettings (*take, aNarrowBandAndATurn ());

  expectTheWholeSphere (*take);
  EXPECT_FLOAT_EQ (take->getRotate (), 0.4f) << "the rest still lands";

  // Saved, it is a clip like any other: settings reach its band again.
  take->setBandHeld (false);
  applyClipSettings (*take, aNarrowBandAndATurn ());
  EXPECT_FLOAT_EQ (take->getReach (), 0.3f);
}

/** A clip loaded onto the take from FILES, the browser or a Cue with no
 *  figure of its own lands its values and its lanes -- not the band's. */
TEST (TakeBand, AClipLoadedOntoATakeSparesItsBand)
{
  auto take = aHeldTake ();
  Clip clip;
  clip.settings = aNarrowBandAndATurn ();
  for (auto const knob : { Knob::Reach, Knob::Elevation, Knob::Rotate })
    clip.lanes[static_cast<std::size_t> (knob)]
        = KnobLane::fromChangePoints ({ { 3, 0.25f } }, 64);

  applyClipValues (*take, clip);

  expectTheWholeSphere (*take);
  EXPECT_FALSE (take->hasLane (Knob::Reach));
  EXPECT_FALSE (take->hasLane (Knob::Elevation));
  EXPECT_TRUE (take->hasLane (Knob::Rotate));
}

namespace
{
std::shared_ptr<Pattern>
aClipWithAShortHold ()
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setChannel (0);
  pattern->setEnvelopeAttack (0);
  pattern->setEnvelopeDecay (0);
  pattern->setActMode (ActMode::Hold);
  return pattern;
}
}

/** An ACT accent on a take: the action lands on it except for the band, and
 *  when the finger lifts, what comes back is not a band either. */
TEST (TakeBand, AnAccentOnATakeSparesItsBand)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto take = aClipWithAShortHold ();
  engine.recordPattern (take, Measure{ 8, 0, 0 }, Measure{ 1, 0, 0 });

  auto action = aNarrowBandAndATurn ();
  action.spin = -4;
  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, take);
  ASSERT_TRUE (waitUntil ([&] { return take->getSpin () == -4; }))
      << "the action never reached the take";
  expectTheWholeSphere (*take);

  engine.setChannelAccentHeld (0, false, nullptr);
  ASSERT_TRUE (waitUntil ([&] { return take->getSpin () == 0; }))
      << "the take never came back from the action";
  expectTheWholeSphere (*take);
}

/** Scenario B: the accent went up on the old clip before REC, so what the
 *  engine falls back to is the old clip's settings -- band and all. Pressed
 *  again on the take and let go, that must not put the old band on the take. */
TEST (TakeBand, AnAccentFromBeforeTheTakeDoesNotRestoreAnOldBandOntoIt)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto clip = aClipWithAShortHold ();
  clip->setElevationBase (0.5f);
  clip->setReach (0.3f);
  ClipSettings action;
  action.spin = -4;
  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, clip);
  ASSERT_TRUE (waitUntil ([&] { return clip->getSpin () == -4; }));

  auto take = aClipWithAShortHold ();
  take->setElevationBase (0.5f);
  take->setReach (0.3f);
  engine.recordPattern (take, Measure{ 8, 0, 0 }, Measure{ 1, 0, 0 });
  engine.setChannelAccentHeld (0, true, take);
  ASSERT_TRUE (waitUntil ([&] { return take->getSpin () == -4; }));

  engine.setChannelAccentHeld (0, false, nullptr);
  ASSERT_TRUE (waitUntil ([&] { return take->getSpin () == 0; }));
  expectTheWholeSphere (*take);
}

/** REC pressed again before the downbeat calls the take off -- in the engine
 *  too, or its queued start fires anyway: a take running in Loop that nobody
 *  owns, isRecording() stuck and REC dead. */
TEST (TakeBand, ATakeCalledOffBeforeItsDownbeatNeverStarts)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);
  engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  // Half a bar at 240 BPM: half a second from now.
  engine.recordPattern (take, Measure{ 0, 2, 0 }, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine.isRecordingOrScheduled (); }));
  engine.cancelScheduledRecording (take);

  juce::Thread::sleep (1200);
  EXPECT_FALSE (engine.isRecording ()) << "the called-off take started";
  EXPECT_FALSE (engine.isRecordingOrScheduled ());
  EXPECT_FALSE (take->wasRecording ());
}

/** The same once the take has started -- REC again on the very downbeat, the
 *  start and the call-off in one tick: the call-off stops it, so no take
 *  records on that nobody owns. */
TEST (TakeBand, ATakeCalledOffAfterItsStartStops)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);
  engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  engine.recordPattern (take, Measure{}, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine.isRecording (); }));

  engine.cancelScheduledRecording (take);
  EXPECT_TRUE (waitUntil ([&] { return !engine.isRecordingOrScheduled (); }))
      << "the take went on recording after it was called off";
  EXPECT_NE (take->getStatus (), Pattern::Status::Recording);
}

/** A take called off before its downbeat is never announced as recording --
 *  the strip would turn red and stay red. */
TEST (TakeBand, ATakeCalledOffIsNeverAnnouncedAsRecording)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap, offlineBackend ());
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);
  engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

  auto const announced = engine.recordingAnnouncements ();
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  engine.recordPattern (take, Measure{ 0, 2, 0 }, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine.isRecordingOrScheduled (); }));
  engine.cancelScheduledRecording (take);

  juce::Thread::sleep (1200);
  EXPECT_EQ (engine.recordingAnnouncements (), announced);
}
