// SPDX-FileCopyrightText: 2026 A3 Audio <contact@a3-audio.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RemotePointer.hh"

#include <cmath>

namespace a3
{

std::optional<PointerEvent>
parsePointer (int phase, float x, float y)
{
  if (phase < 0 || phase > 2)
    return std::nullopt;
  if (!std::isfinite (x) || !std::isfinite (y))
    return std::nullopt;
  if (x < 0.f || x > 1.f || y < 0.f || y > 1.f)
    return std::nullopt;

  return PointerEvent{ static_cast<PointerEvent::Phase> (phase), x, y };
}

std::vector<Inject>
RemotePointerGate::live (bool on, double nowMs)
{
  if (on)
    {
      _live = true;
      _lastOnMs = nowMs;
      return {};
    }

  _live = false;
  return releaseHeld ();
}

std::vector<Inject>
RemotePointerGate::releaseHeld ()
{
  if (!_held)
    return {};

  _held = false;
  return { { PointerEvent::Phase::up, _lastX, _lastY } };
}

std::vector<Inject>
RemotePointerGate::pointer (PointerEvent const &event, double nowMs)
{
  using Phase = PointerEvent::Phase;

  if (!isLive (nowMs))
    return {};

  if (event.phase != Phase::down && !_held)
    return {};

  std::vector<Inject> out;
  if (event.phase == Phase::down)
    out = releaseHeld ();

  out.push_back ({ event.phase, event.x, event.y });

  _held = event.phase != Phase::up;
  _lastX = event.x;
  _lastY = event.y;
  _lastPointerMs = nowMs;
  return out;
}

std::vector<Inject>
RemotePointerGate::tick (double nowMs)
{
  if (_live && !isLive (nowMs))
    {
      _live = false;
      return releaseHeld ();
    }

  if (_held && nowMs - _lastPointerMs > releaseAfterMs)
    return releaseHeld ();

  return {};
}

bool
RemotePointerGate::isLive (double nowMs) const
{
  return _live && nowMs - _lastOnMs < liveLeaseMs;
}

bool
RemotePointerGate::held () const
{
  return _held;
}

std::optional<juce::Point<float>>
screenToWindow (juce::Point<float> share, juce::Rectangle<float> screen,
                juce::Rectangle<float> window)
{
  auto const onScreen = juce::Point<float> (
      screen.getX () + share.x * screen.getWidth (),
      screen.getY () + share.y * screen.getHeight ());

  // Closed on every side: a window as big as the screen must take the
  // screen's far edge, and Rectangle::contains would not.
  if (onScreen.x < window.getX () || onScreen.x > window.getRight ()
      || onScreen.y < window.getY () || onScreen.y > window.getBottom ())
    return std::nullopt;

  return onScreen - window.getPosition ();
}

}
