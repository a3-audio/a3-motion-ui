// SPDX-FileCopyrightText: 2026 A3 Audio <contact@a3-audio.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <cstdlib>
#include <vector>

#include <a3-motion-ui/components/PointerInjector.hh>
#include <a3-motion-ui/components/TouchGrabs.hh>

using namespace a3;
using Phase = PointerEvent::Phase;

namespace
{
juce::Rectangle<float> const screen (0.f, 0.f, 1000.f, 500.f);
juce::Rectangle<float> const window (100.f, 50.f, 400.f, 200.f);

struct Recorder
{
  std::vector<PointerInjector::Touch> touches;
  PointerInjector injector{ [this] (PointerInjector::Touch const &touch) {
    touches.push_back (touch);
  } };
};
}

TEST (PointerInjector, ADownArrivesAsTouchNineWithTheLeftButton)
{
  Recorder r;

  r.injector.deliver ({ Phase::down, .3f, .2f }, screen, window, 1234);

  ASSERT_EQ (r.touches.size (), 1u);
  EXPECT_EQ (r.touches[0].touchIndex, 9);
  EXPECT_EQ (r.touches[0].touchIndex, remoteTouchIndex);
  EXPECT_TRUE (r.touches[0].mods.isLeftButtonDown ());
  EXPECT_FLOAT_EQ (r.touches[0].local.x, 200.f);
  EXPECT_FLOAT_EQ (r.touches[0].local.y, 50.f);
  EXPECT_EQ (r.touches[0].timeMs, 1234);
  EXPECT_TRUE (r.injector.isDown ());
}

TEST (PointerInjector, AMoveKeepsTheButtonAndAnUpDropsIt)
{
  Recorder r;
  r.injector.deliver ({ Phase::down, .3f, .2f }, screen, window, 1);

  r.injector.deliver ({ Phase::move, .35f, .25f }, screen, window, 2);
  r.injector.deliver ({ Phase::up, .35f, .25f }, screen, window, 3);

  ASSERT_EQ (r.touches.size (), 3u);
  EXPECT_TRUE (r.touches[1].mods.isLeftButtonDown ());
  EXPECT_FALSE (r.touches[2].mods.isLeftButtonDown ());
  EXPECT_EQ (r.touches[2].touchIndex, 9);
  EXPECT_FALSE (r.injector.isDown ());
}

TEST (PointerInjector, ADownOutsideTheWindowIsNothing)
{
  Recorder r;

  r.injector.deliver ({ Phase::down, .02f, .02f }, screen, window, 1);
  r.injector.deliver ({ Phase::move, .3f, .2f }, screen, window, 2);
  r.injector.deliver ({ Phase::up, .3f, .2f }, screen, window, 3);

  EXPECT_TRUE (r.touches.empty ())
      << "a press that never started inside is nothing, moves and up too";
}

// Only a DOWN outside the window is ignored. Once a press has started inside,
// its moves and its up are always delivered, clamped to the window, because
// an up that is dropped leaves JUCE's touch #9 down for good.
TEST (PointerInjector, AHeldPressMovingOutsideIsClampedToTheEdgeAndStaysDown)
{
  Recorder r;
  r.injector.deliver ({ Phase::down, .3f, .2f }, screen, window, 1);

  r.injector.deliver ({ Phase::move, .9f, .9f }, screen, window, 2);

  ASSERT_EQ (r.touches.size (), 2u);
  EXPECT_TRUE (r.touches[1].mods.isLeftButtonDown ());
  EXPECT_EQ (r.touches[1].touchIndex, 9);
  EXPECT_FLOAT_EQ (r.touches[1].local.x, 399.f)
      << "the window's last column: x == width lies outside contains ()";
  EXPECT_FLOAT_EQ (r.touches[1].local.y, 199.f) << "the window's last row";
  EXPECT_TRUE (r.injector.isDown ());
}

TEST (PointerInjector, AnUpOutsideTheWindowIsClampedDeliveredAndEndsThePress)
{
  Recorder r;
  r.injector.deliver ({ Phase::down, .3f, .2f }, screen, window, 1);

  r.injector.deliver ({ Phase::up, .02f, .9f }, screen, window, 2);

  ASSERT_EQ (r.touches.size (), 2u);
  EXPECT_FALSE (r.touches[1].mods.isLeftButtonDown ());
  EXPECT_EQ (r.touches[1].touchIndex, 9);
  EXPECT_FLOAT_EQ (r.touches[1].local.x, 0.f) << "the window's left edge";
  EXPECT_FLOAT_EQ (r.touches[1].local.y, 199.f);
  EXPECT_FALSE (r.injector.isDown ());
}

TEST (PointerInjector, AfterTheUpNothingMoreIsLeftToLift)
{
  Recorder r;
  r.injector.deliver ({ Phase::down, .3f, .2f }, screen, window, 1);
  r.injector.deliver ({ Phase::up, .9f, .9f }, screen, window, 2);

  r.injector.deliver ({ Phase::up, .3f, .2f }, screen, window, 3);
  r.injector.deliver ({ Phase::move, .3f, .2f }, screen, window, 4);

  EXPECT_EQ (r.touches.size (), 2u);
}

// The same event through a real window's peer: it reaches a component as a
// touch with index 9, not as the mouse and not as finger 0. Needs a display;
// without one JUCE would end the process, so the test says so and stands
// aside.
namespace
{
struct Catcher : public juce::Component
{
  void
  mouseDown (juce::MouseEvent const &e) override
  {
    sawType = e.source.getType ();
    sawIndex = e.source.getIndex ();
    ++downs;
  }

  juce::MouseInputSource::InputSourceType sawType
      = juce::MouseInputSource::InputSourceType::mouse;
  int sawIndex = -1;
  int downs = 0;
};
}

TEST (PointerInjector, ArrivesAsTouchNine)
{
  if (std::getenv ("DISPLAY") == nullptr)
    GTEST_SKIP () << "no DISPLAY: a window cannot be made";

  Catcher catcher;
  catcher.setBounds (0, 0, 200, 100);
  catcher.setVisible (true);
  catcher.addToDesktop (0);
  auto *peer = catcher.getPeer ();
  ASSERT_NE (peer, nullptr);

  PointerInjector injector (peerSink (*peer));
  auto const bounds = peer->getBounds ().toFloat ();
  injector.deliver ({ Phase::down, .5f, .5f }, bounds, bounds, 1);

  EXPECT_EQ (catcher.downs, 1);
  EXPECT_EQ (catcher.sawType, juce::MouseInputSource::InputSourceType::touch);
  EXPECT_EQ (catcher.sawIndex, 9);
  EXPECT_EQ (grabKey (catcher.sawType, catcher.sawIndex), 9);

  injector.deliver ({ Phase::up, .5f, .5f }, bounds, bounds, 2);
}
