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

#pragma once

#include <JuceHeader.h>

#include <functional>

#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

/** One meter, as its own component.
 *
 *  **Opaque on purpose.** A meter changes twenty five times a second, and
 *  when the page drew them, those scattered little rectangles merged into one
 *  repaint of the whole page: 533 full pages in two and a half seconds of
 *  dragging a fader, measured on the rig at about 2.4 ms each, which is what
 *  the hand felt as the fader catching. paintVuMeter fills its whole
 *  rectangle with the track before it draws anything into it, so this can
 *  say so and keep its refresh to itself.
 *
 *  The fader that drags the level stands over it as a second component -- the
 *  handle is the layer above, and it moves only while a finger is on it. */
class VuMeterView : public juce::Component
{
public:
  VuMeterView ();

  /** Where the reading comes from. Asked at paint time, so the store stays
   *  the page's and this keeps no copy of a level to go stale. */
  std::function<VuLevel ()> level;

  /** Up for a channel's column, Right for one of the master's output bars. */
  void setDirection (VuDirection direction);

  void paint (juce::Graphics &g) override;

private:
  VuDirection _direction = VuDirection::Up;
};

}
