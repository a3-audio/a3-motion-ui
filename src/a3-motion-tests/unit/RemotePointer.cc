// SPDX-FileCopyrightText: 2026 A3 Audio <contact@a3-audio.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <cmath>
#include <limits>

#include <a3-motion-ui/components/RemotePointer.hh>

using namespace a3;
using Phase = PointerEvent::Phase;

namespace
{
PointerEvent
at (Phase phase, float x = .5f, float y = .5f)
{
  return { phase, x, y };
}

/** A gate that has heard LIVE on at t = 0. */
RemotePointerGate
liveGate ()
{
  RemotePointerGate gate;
  gate.live (true, 0.);
  return gate;
}
}

TEST (RemotePointer, AGoodPointerParses)
{
  auto const down = parsePointer (0, .25f, .75f);
  ASSERT_TRUE (down.has_value ());
  EXPECT_EQ (down->phase, Phase::down);
  EXPECT_FLOAT_EQ (down->x, .25f);
  EXPECT_FLOAT_EQ (down->y, .75f);
  EXPECT_EQ (parsePointer (1, 0.f, 1.f)->phase, Phase::move);
  EXPECT_EQ (parsePointer (2, 1.f, 0.f)->phase, Phase::up);
}

TEST (RemotePointer, ABadPointerIsNothing)
{
  auto const nan = std::numeric_limits<float>::quiet_NaN ();
  auto const inf = std::numeric_limits<float>::infinity ();

  EXPECT_FALSE (parsePointer (-1, .5f, .5f));
  EXPECT_FALSE (parsePointer (3, .5f, .5f));
  EXPECT_FALSE (parsePointer (0, nan, .5f));
  EXPECT_FALSE (parsePointer (0, .5f, inf));
  EXPECT_FALSE (parsePointer (0, -.01f, .5f));
  EXPECT_FALSE (parsePointer (0, .5f, 1.01f));
}

TEST (RemotePointer, TheRemotePointerIsTouchNineAndNeverIndexZero)
{
  EXPECT_EQ (remoteTouchIndex, 9);
}

TEST (RemotePointer, ScreenToWindowMapsAShareOfTheScreenIntoTheWindow)
{
  juce::Rectangle<float> const screen (0.f, 0.f, 1000.f, 500.f);
  juce::Rectangle<float> const window (100.f, 50.f, 400.f, 200.f);

  auto const local = screenToWindow ({ .3f, .2f }, screen, window);

  ASSERT_TRUE (local.has_value ());
  EXPECT_FLOAT_EQ (local->x, 200.f);   // 300 on the screen, 100 into the window
  EXPECT_FLOAT_EQ (local->y, 50.f);    // 100 on the screen, 50 into the window
}

TEST (RemotePointer, ScreenToWindowKnowsTheScreensOwnOrigin)
{
  juce::Rectangle<float> const screen (1920.f, 0.f, 1000.f, 500.f);
  juce::Rectangle<float> const window (1920.f, 0.f, 1000.f, 500.f);

  auto const local = screenToWindow ({ .5f, .5f }, screen, window);

  ASSERT_TRUE (local.has_value ());
  EXPECT_FLOAT_EQ (local->x, 500.f);
  EXPECT_FLOAT_EQ (local->y, 250.f);
}

TEST (RemotePointer, ScreenToWindowIgnoresAPointOutsideTheWindow)
{
  juce::Rectangle<float> const screen (0.f, 0.f, 1000.f, 500.f);
  juce::Rectangle<float> const window (100.f, 50.f, 400.f, 200.f);

  EXPECT_FALSE (screenToWindow ({ .05f, .2f }, screen, window));
  EXPECT_FALSE (screenToWindow ({ .3f, .9f }, screen, window));
  EXPECT_FALSE (screenToWindow ({ .6f, .2f }, screen, window));
}

TEST (RemotePointer, TheWholeScreenWindowTakesTheScreensFarEdge)
{
  juce::Rectangle<float> const screen (0.f, 0.f, 800.f, 480.f);

  EXPECT_TRUE (screenToWindow ({ 0.f, 0.f }, screen, screen));
  EXPECT_TRUE (screenToWindow ({ 1.f, 1.f }, screen, screen));
}

TEST (RemotePointerGate, IsOffAtStartAndTakesNothing)
{
  RemotePointerGate gate;

  EXPECT_FALSE (gate.isLive (0.));
  EXPECT_TRUE (gate.pointer (at (Phase::down), 0.).empty ());
  EXPECT_FALSE (gate.held ());
}

TEST (RemotePointerGate, WhileLiveADownIsInjected)
{
  auto gate = liveGate ();

  auto const out = gate.pointer (at (Phase::down, .2f, .3f), 10.);

  ASSERT_EQ (out.size (), 1u);
  EXPECT_EQ (out[0].phase, Phase::down);
  EXPECT_FLOAT_EQ (out[0].x, .2f);
  EXPECT_FLOAT_EQ (out[0].y, .3f);
  EXPECT_TRUE (gate.held ());
}

TEST (RemotePointerGate, AMoveAndAnUpFollowTheDown)
{
  auto gate = liveGate ();
  gate.pointer (at (Phase::down), 10.);

  auto const move = gate.pointer (at (Phase::move, .6f, .7f), 20.);
  ASSERT_EQ (move.size (), 1u);
  EXPECT_EQ (move[0].phase, Phase::move);

  auto const up = gate.pointer (at (Phase::up, .6f, .7f), 30.);
  ASSERT_EQ (up.size (), 1u);
  EXPECT_EQ (up[0].phase, Phase::up);
  EXPECT_FALSE (gate.held ());
}

TEST (RemotePointerGate, AMoveOrUpWithoutADownIsNothing)
{
  auto gate = liveGate ();

  EXPECT_TRUE (gate.pointer (at (Phase::move), 10.).empty ());
  EXPECT_TRUE (gate.pointer (at (Phase::up), 20.).empty ());
}

TEST (RemotePointerGate, ADownWhileHeldLetsGoFirst)
{
  auto gate = liveGate ();
  gate.pointer (at (Phase::down, .1f, .1f), 10.);
  gate.pointer (at (Phase::move, .4f, .4f), 20.);

  auto const out = gate.pointer (at (Phase::down, .8f, .9f), 30.);

  ASSERT_EQ (out.size (), 2u);
  EXPECT_EQ (out[0].phase, Phase::up);
  EXPECT_FLOAT_EQ (out[0].x, .4f) << "the up is where the old press was";
  EXPECT_FLOAT_EQ (out[0].y, .4f);
  EXPECT_EQ (out[1].phase, Phase::down);
  EXPECT_FLOAT_EQ (out[1].x, .8f);
  EXPECT_TRUE (gate.held ());
}

TEST (RemotePointerGate, GoingOffMidDragReleases)
{
  auto gate = liveGate ();
  gate.pointer (at (Phase::down, .1f, .1f), 10.);
  gate.pointer (at (Phase::move, .4f, .5f), 20.);

  auto const out = gate.live (false, 30.);

  ASSERT_EQ (out.size (), 1u);
  EXPECT_EQ (out[0].phase, Phase::up);
  EXPECT_FLOAT_EQ (out[0].x, .4f);
  EXPECT_FLOAT_EQ (out[0].y, .5f);
  EXPECT_FALSE (gate.held ());
  EXPECT_FALSE (gate.isLive (30.));
}

TEST (RemotePointerGate, GoingOffWhileNothingIsHeldReleasesNothing)
{
  auto gate = liveGate ();

  EXPECT_TRUE (gate.live (false, 10.).empty ());
}

TEST (RemotePointerGate, AfterOffNothingIsTaken)
{
  auto gate = liveGate ();
  gate.live (false, 10.);

  EXPECT_TRUE (gate.pointer (at (Phase::down), 20.).empty ());
}

TEST (RemotePointerGate, TheLeaseRunsOut)
{
  auto gate = liveGate ();
  gate.pointer (at (Phase::down, .3f, .3f), 11990.);
  gate.pointer (at (Phase::move, .3f, .3f), 12000. - 1.);

  EXPECT_TRUE (gate.isLive (11999.));
  EXPECT_FALSE (gate.isLive (12000.));

  auto const out = gate.tick (12000.);

  ASSERT_EQ (out.size (), 1u);
  EXPECT_EQ (out[0].phase, Phase::up);
  EXPECT_FALSE (gate.held ());
  EXPECT_FALSE (gate.isLive (12000.));
}

TEST (RemotePointerGate, TheLeaseRunningOutWhileNothingIsHeldIsQuiet)
{
  auto gate = liveGate ();

  EXPECT_TRUE (gate.tick (13000.).empty ());
  EXPECT_FALSE (gate.isLive (13000.));
}

TEST (RemotePointerGate, AnotherOnRenewsTheLease)
{
  auto gate = liveGate ();
  gate.live (true, 5000.);
  gate.live (true, 10000.);

  EXPECT_TRUE (gate.isLive (21000.));
  EXPECT_TRUE (gate.tick (21000.).empty ());
  EXPECT_FALSE (gate.isLive (22000.));
}

TEST (RemotePointerGate, ASilentHeldPointerIsReleasedAfterOneSecond)
{
  auto gate = liveGate ();
  gate.live (true, 400.);
  gate.pointer (at (Phase::down, .2f, .2f), 500.);
  gate.pointer (at (Phase::move, .25f, .3f), 600.);

  EXPECT_TRUE (gate.tick (1600.).empty ()) << "exactly one second is not over";

  auto const out = gate.tick (1601.);
  ASSERT_EQ (out.size (), 1u);
  EXPECT_EQ (out[0].phase, Phase::up);
  EXPECT_FLOAT_EQ (out[0].x, .25f);
  EXPECT_FLOAT_EQ (out[0].y, .3f);
  EXPECT_FALSE (gate.held ());
  EXPECT_TRUE (gate.isLive (1601.)) << "silent is not off";
}

TEST (RemotePointerGate, AKeepAliveMoveKeepsAPressHeld)
{
  auto gate = liveGate ();
  gate.live (true, 100.);
  gate.pointer (at (Phase::down), 100.);
  for (double t = 350.; t < 3000.; t += 250.)
    {
      gate.live (true, t);
      gate.pointer (at (Phase::move), t);
      EXPECT_TRUE (gate.tick (t + 10.).empty ());
    }

  EXPECT_TRUE (gate.held ());
}

TEST (RemotePointerGate, AReleasedPressTakesNoMoreMoves)
{
  auto gate = liveGate ();
  gate.pointer (at (Phase::down), 10.);
  gate.tick (2000.);

  EXPECT_TRUE (gate.pointer (at (Phase::move), 2010.).empty ());
}
