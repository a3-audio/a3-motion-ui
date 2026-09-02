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


#include "OverlaySideStrips.hh"

namespace a3
{

OverlaySideStrips::OverlaySideStrips ()
{
  // Not for itself, but for its children: the strips catch, the gap between
  // them — where the page is — does not.
  setInterceptsMouseClicks (false, true);

  _browseZone = std::make_unique<TouchControl> ();
  _browseZone->onDragIncrement = [this] (int, int, int increment) {
    // TouchControl counts up as more; here "more" is further down the list.
    if (onBrowse)
      onBrowse (-increment);
  };
  addAndMakeVisible (*_browseZone);

  _valueZone = std::make_unique<TouchControl> ();
  _valueZone->onDragIncrement = [this] (int, int, int increment) {
    if (onValue)
      onValue (increment);
  };
  _valueZone->onRelease = [this] (int, int) {
    if (onValueReleased)
      onValueReleased ();
  };
  addAndMakeVisible (*_valueZone);
}

void
OverlaySideStrips::setPanel (juce::Rectangle<int> panel)
{
  auto sides = getLocalBounds ();

  auto const leftWidth = juce::jlimit (0, sides.getWidth (),
                                       panel.getX () - sides.getX ());
  auto const rightWidth
      = juce::jlimit (0, sides.getWidth (), sides.getRight () - panel.getRight ());

  _browseZone->setBounds (sides.removeFromLeft (leftWidth));
  _valueZone->setBounds (sides.removeFromRight (rightWidth));
}

}
