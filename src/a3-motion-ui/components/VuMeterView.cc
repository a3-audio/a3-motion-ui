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

#include "VuMeterView.hh"

#include <a3-motion-ui/components/DeskMeter.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

VuMeterView::VuMeterView ()
{
  // It fills its own bounds, so the page underneath need not be redrawn for
  // it -- see the class comment for what that cost before.
  setOpaque (true);

  // A meter is read, never touched: the fader over it takes the drag, and a
  // hit area here would swallow the half of it that overlaps.
  setInterceptsMouseClicks (false, false);
}

void
VuMeterView::setDirection (VuDirection direction)
{
  if (_direction == direction)
    return;

  _direction = direction;
  repaint ();
}

void
VuMeterView::setScale (VuMeterScale scale)
{
  if (_scale == scale)
    return;

  _scale = scale;
  repaint ();
}

void
VuMeterView::paint (juce::Graphics &g)
{
  auto const reading = level ? level () : VuReading{};
  if (_scale == VuMeterScale::Continuous)
    {
      paintVuMeter (g, getLocalBounds (), reading, _direction);
      return;
    }
  // The desk meter leaves its marks' lane and the well's corners to the
  // ground; this view is opaque, so it fills them first.
  g.fillAll (toColour (theme ().surfaceRaised));
  paintDeskMeter (g, getLocalBounds (), reading, _direction);
}

}
