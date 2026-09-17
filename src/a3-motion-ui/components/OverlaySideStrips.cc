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
    // The page follows the finger, the way it does on a phone: dragging
    // upwards carries the page up, which brings later rows into view.
    //
    // It was negated here, so the list ran *against* the hand — the one
    // convention every hand in the room already has, and the one it did not
    // follow. TouchControl counts upwards as more, and further down the list
    // is what more means.
    if (onBrowse)
      onBrowse (increment);
  };
  _browseZone->setFingerLatch (&FingerLatch::forGroup (FingerLatch::menuList));
  addAndMakeVisible (*_browseZone);

  // The right strip scrolls as well. It used to turn the highlighted row and
  // apply it on release -- an edit without a mask, one drag away from a
  // scroll. A value changes in its mask now, and either hand can scroll.
  _rightZone = std::make_unique<TouchControl> ();
  _rightZone->onDragIncrement = [this] (int, int, int increment) {
    if (onBrowse)
      onBrowse (increment);
  };
  _rightZone->setFingerLatch (&FingerLatch::forGroup (FingerLatch::menuList));
  addAndMakeVisible (*_rightZone);
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
  _rightZone->setBounds (sides.removeFromRight (rightWidth));
}

}
