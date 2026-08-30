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
TouchControl::mouseDown (juce::MouseEvent const &)
{
  // Read here rather than in the constructor: the skin can change while
  // the app runs, and a drag should count with the value in force now.
  _drag = DragAccumulator{ theme ().touchDragPixelsPerStep };

  if (onPress)
    onPress (_primary, _secondary);
}

void
TouchControl::mouseDrag (juce::MouseEvent const &event)
{
  // JUCE's y grows downwards; a finger going up means more.
  auto const pending = _drag.stepsFor (-event.getDistanceFromDragStartY ());

  if (pending == 0 || !onDragIncrement)
    return;

  auto const direction = pending > 0 ? 1 : -1;
  for (int i = 0; i < std::abs (pending); ++i)
    onDragIncrement (_primary, _secondary, direction);
}

void
TouchControl::mouseUp (juce::MouseEvent const &)
{
  if (_drag.emittedSteps () == 0)
    {
      if (onTap)
        onTap (_primary, _secondary);
      return;
    }

  if (onRelease)
    onRelease (_primary, _secondary);
}

}
