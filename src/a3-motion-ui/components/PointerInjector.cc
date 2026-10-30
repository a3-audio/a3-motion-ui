// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PointerInjector.hh"

namespace a3
{

PointerInjector::PointerInjector (Sink sink) : _sink (std::move (sink)) {}

namespace
{
juce::Point<float>
clampedToWindow (juce::Point<float> screenPoint,
                 juce::Rectangle<float> window)
{
  // To the last column and row, not to width and height: Component::contains
  // is half-open, so a point on x == width lies outside the window and its
  // up would reach nothing. Chosen over relying on the up going to the
  // component that took the down, which holds only for the mouse grab.
  return { juce::jlimit (0.f, juce::jmax (0.f, window.getWidth () - 1.f),
                         screenPoint.x - window.getX ()),
           juce::jlimit (0.f, juce::jmax (0.f, window.getHeight () - 1.f),
                         screenPoint.y - window.getY ()) };
}
}

void
PointerInjector::deliver (Inject const &inject, juce::Rectangle<float> screen,
                          juce::Rectangle<float> window, juce::int64 timeMs)
{
  using Phase = PointerEvent::Phase;

  auto const isDown = inject.phase == Phase::down;
  if (!isDown && !_down)
    return;

  auto const onScreen = juce::Point<float> (
      screen.getX () + inject.x * screen.getWidth (),
      screen.getY () + inject.y * screen.getHeight ());

  // Only a press that would start outside the window is ignored. Once down,
  // moves and the up follow the finger out of it, clamped to its edge: an up
  // that is dropped leaves JUCE's touch #9 down for good.
  juce::Point<float> local;
  if (_down)
    local = clampedToWindow (onScreen, window);
  else if (auto const inside = screenToWindow ({ inject.x, inject.y }, screen,
                                               window))
    local = *inside;
  else
    return;

  _down = inject.phase != Phase::up;

  juce::ModifierKeys const mods
      = _down ? juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)
              : juce::ModifierKeys ();
  _sink ({ local, mods, timeMs, remoteTouchIndex });
}

bool
PointerInjector::isDown () const
{
  return _down;
}

PointerInjector::Sink
peerSink (juce::ComponentPeer &peer)
{
  // By reference: the injector lives in the component whose window this is,
  // and goes before it.
  return [&peer] (PointerInjector::Touch const &touch) {
    peer.handleMouseEvent (juce::MouseInputSource::InputSourceType::touch,
                           touch.local, touch.mods, 1.f, 0.f, touch.timeMs,
                           {}, touch.touchIndex);
  };
}

}
