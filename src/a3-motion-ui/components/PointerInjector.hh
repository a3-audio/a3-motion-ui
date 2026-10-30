// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <a3-motion-ui/components/RemotePointer.hh>

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** Puts the gate's decisions on the screen as touch #remoteTouchIndex.
 *
 *  The sink is where one touch event ends up -- in the app the top-level
 *  peer's handleMouseEvent, in tests a recorder -- so the mapping and the
 *  bookkeeping are testable without a window. */
class PointerInjector
{
public:
  struct Touch
  {
    juce::Point<float> local;
    juce::ModifierKeys mods;
    juce::int64 timeMs;
    int touchIndex;
  };

  using Sink = std::function<void (Touch const &)>;

  explicit PointerInjector (Sink sink);

  /** Maps `inject` (a share of `screen`) into `window` and delivers it.
   *  A down outside the window is ignored. While the source is down, moves
   *  and the up are clamped to the window and always delivered, because a
   *  touch must never be left down. */
  void deliver (Inject const &inject, juce::Rectangle<float> screen,
                juce::Rectangle<float> window, juce::int64 timeMs);

  bool isDown () const;

private:
  Sink _sink;
  bool _down = false;
};

/** The sink that injects into a window: handleMouseEvent as a touch. */
PointerInjector::Sink peerSink (juce::ComponentPeer &peer);

}
