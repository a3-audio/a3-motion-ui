/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/Measure.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>

namespace a3
{

/** How hard gravity pulls at `now`: 1 + depth * (1 - beatFraction)^2.
 *
 *  The wells pull hardest at each beat's onset and relax towards the next, so
 *  the bends land on the beat; on the one of the bar the depth is
 *  pulseDownbeatDepth, on every other beat pulseDepth. Never below 1, and
 *  exactly 1 with both depths at 0. Time is the beat clock's, so tempo needs
 *  nothing of its own. */
float gravityPulse (Measure now, int beatsPerBar, FlightTuning const &tuning);

}
