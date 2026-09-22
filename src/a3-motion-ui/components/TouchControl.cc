/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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


#include "TouchControl.hh"

#include <a3-motion-ui/theme/Theme.hh>

#include <cstdlib>

namespace a3
{

TouchControl::TouchControl ()
{
  setInterceptsMouseClicks (true, false);
}

void
TouchControl::setIdentity (int primary, int secondary)
{
  _primary = primary;
  _secondary = secondary;
}

void
TouchControl::mouseDown (juce::MouseEvent const &event)
{
  // A second finger on a list another finger is already dragging is not a
  // gesture of its own: it would scroll the list a second time.
  if (_latch != nullptr)
    {
      // A leader that is no longer on the glass let go somewhere this list
      // never heard about -- its page was hidden under it. Take over.
      auto const leader = _latch->leader ();
      if (leader >= 0 && leader != event.source.getIndex ())
        {
          auto *const held
              = juce::Desktop::getInstance ().getMouseSource (leader);
          if (held == nullptr || !held->isDragging ())
            _latch->release (leader);
        }

      if (!_latch->claim (event.source.getIndex ()))
        return;
      _latchedSource = event.source.getIndex ();
    }

  // Read here rather than in the constructor: the skin can change while
  // the app runs, and a drag should count with the value in force now.
  _drag = DragAccumulator{ pixelsPerStep () };
  if (_pixelsPerStep > 0)
    _drag.stepAtHalfWay ();

  // A drag that ran out of screen and was picked straight back up is one
  // drag. Without this the second half was a fresh gesture, and on a key that
  // also taps -- the speed keys -- that tap threw away the value the first
  // half had just reached.
  constexpr int resumeMs = 700;
  //
  // Not on a list: there the finger coming down again right after a scroll is
  // how a row is picked, and taking it for more of the drag swallowed the tap
  // and the double tap that opens a row.
  if (_pixelsPerStep == 0 && _lastDragEndedMs != 0
      && juce::Time::currentTimeMillis () - _lastDragEndedMs < resumeMs)
    _drag.resume ();

  if (onPress)
    onPress (_primary, _secondary);
}

void
TouchControl::mouseDrag (juce::MouseEvent const &event)
{
  if (_latch != nullptr && !_latch->leads (event.source.getIndex ()))
    return;

  if (onDragTo)
    onDragTo (_primary, _secondary, event.getPosition ());
  if (onDragBy)
    onDragBy (_primary, _secondary, event.getOffsetFromDragStart ());

  // JUCE's y grows downwards; a finger going up means more.
  auto const pending = _drag.stepsFor (-event.getDistanceFromDragStartY ());

  if (pending == 0 || !onDragIncrement)
    return;

  auto const direction = pending > 0 ? 1 : -1;
  for (int i = 0; i < std::abs (pending); ++i)
    onDragIncrement (_primary, _secondary, direction);
}

void
TouchControl::mouseUp (juce::MouseEvent const &event)
{
  if (_latch != nullptr)
    {
      auto const source = event.source.getIndex ();
      auto const led = _latch->leads (source);
      _latch->release (source);
      if (led)
        _latchedSource = -1;
      if (!led)
        return; // an ignored finger ends nothing: no tap, no release
    }

  // First and unconditionally: whoever is holding something needs to hear
  // that the finger left, and the branch below returns early on a tap.
  if (onRelease)
    onRelease (_primary, _secondary);

  if (_drag.hasMoved ())
    _lastDragEndedMs = juce::Time::currentTimeMillis ();

  if (!_drag.hasMoved ())
    {
      auto const here = getMouseXYRelative ();

      // Held across the first callback: a tap can close the page this control
      // stands on -- a menu row opens another list, the skin editor swaps its
      // rows -- and this object goes with it.
      juce::Component::SafePointer<TouchControl> alive{ this };

      if (onTap)
        onTap (_primary, _secondary);
      if (alive != nullptr && onTapAt)
        onTapAt (_primary, _secondary, here);
      return;
    }

  if (onDragEnd)
    onDragEnd (_primary, _secondary);
}

void
TouchControl::visibilityChanged ()
{
  // Hidden under a finger -- a list closing, a mask opening -- the finger's
  // mouseUp may never arrive here, and a latch left held would ignore every
  // finger on the list from then on.
  if (!isVisible () && _latch != nullptr && _latchedSource >= 0)
    {
      _latch->release (_latchedSource);
      _latchedSource = -1;
    }
}

int
TouchControl::pixelsPerStep () const
{
  // A list sets its row height, so the page follows the finger rather than
  // running ahead of it; everything else keeps the skin's step.
  return _pixelsPerStep > 0 ? _pixelsPerStep : theme ().touchDragPixelsPerStep;
}


void
TouchControl::mouseDoubleClick (juce::MouseEvent const &)
{
  // JUCE's own: 400 ms between the two, and 25 px apart for a touch where a
  // mouse gets 8 (MouseInputSource). We used to count this out ourselves,
  // believing the tolerance was a mouse's -- it is not, for a finger.
  if (onDoubleTap)
    onDoubleTap (_primary, _secondary);
}

}
