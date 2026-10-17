// SPDX-FileCopyrightText: 2026 A3 Audio <contact@a3-audio.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** The pro-DJ app's pointer, as it arrives from Core: `/motion/pointer iff`.
 *  x and y are a share of the screen picture (0,0 top-left), not of any
 *  window -- the screencast shows the whole screen. */
struct PointerEvent
{
  enum class Phase
  {
    down,
    move,
    up
  };

  Phase phase;
  float x, y;
};

/** Phase 0..2, x and y finite and in 0..1; anything else is not a pointer. */
std::optional<PointerEvent> parsePointer (int phase, float x, float y);

/** What the gate asks the injector to do. */
struct Inject
{
  PointerEvent::Phase phase;
  float x, y;
};

/** Whether a remote pointer may touch this screen, and when it must let go.
 *
 *  The pointer is accepted only while pro-DJ LIVE is on, and LIVE is a lease:
 *  Core repeats "on" every 5 s, and a device that stops hearing it falls back
 *  to off after liveLeaseMs. Without that a missed "off" (Core restart,
 *  network drop) would leave the screen open to a pointer for good. For the
 *  same reason a held pointer that goes silent is released after
 *  releaseAfterMs: UDP loses an "up" now and then, and a touch held forever
 *  is worse than a touch let go early.
 *
 *  Pure: time comes in as an argument, results go out as a list. */
class RemotePointerGate
{
public:
  static constexpr double releaseAfterMs = 1000.0;
  static constexpr double liveLeaseMs = 12000.0;

  /** LIVE announced on or off. Off while held releases at the last point. */
  std::vector<Inject> live (bool on, double nowMs);

  /** A pointer arrived. Nothing while off; a move or up without a down is
   *  nothing; a down while held lets go first. */
  std::vector<Inject> pointer (PointerEvent const &event, double nowMs);

  /** Called regularly: the lease running out switches off (and releases);
   *  a held pointer silent for longer than releaseAfterMs is released. */
  std::vector<Inject> tick (double nowMs);

  bool isLive (double nowMs) const;
  bool held () const;

private:
  /** An up at the last point, if a press is held. */
  std::vector<Inject> releaseHeld ();

  bool _live = false;
  double _lastOnMs = 0.0;
  bool _held = false;
  float _lastX = 0.f, _lastY = 0.f;
  double _lastPointerMs = 0.0;
};

/** A point given as a share of the screen, in the window's own coordinates;
 *  nothing when it lies outside the window. */
std::optional<juce::Point<float>>
screenToWindow (juce::Point<float> share, juce::Rectangle<float> screen,
                juce::Rectangle<float> window);

/** The remote pointer is a touch source of its own. Index 0 belongs to the
 *  DJ's first finger (and to the mouse JUCE emulates for it), so the remote
 *  one never shares it; grabKey (touch, 9) keeps its grab apart. */
constexpr int remoteTouchIndex = 9;

}
