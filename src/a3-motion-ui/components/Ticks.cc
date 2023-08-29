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

#include "Ticks.hh"

#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{

Ticks::Ticks (int numTicks) : _numTicks (numTicks), _currentTick (0)
{
  _currentTick = 1;
}

void
Ticks::setCurrentTick (int tick)
{
  jassert (tick >= 0 && tick < _numTicks);
  _currentTick = tick;
}

void
Ticks::paint (juce::Graphics &g)
{
  g.setColour (juce::Colours::lightgrey);
  g.fillAll ();

  auto bounds = getLocalBounds ();
  auto width = bounds.getWidth () / float (_numTicks);
  for (auto tick = 0; tick < _numTicks; ++tick)
    {
      auto boundsSquare = bounds;
      boundsSquare.setX (std::round (tick * width));
      boundsSquare.setWidth (width);
      boundsSquare.expand (-2, -2);

      g.setColour (tick <= _currentTick ? juce::Colours::white
                                        : Colours::statusBar);

      g.fillRect (boundsSquare);
    }
}

}
