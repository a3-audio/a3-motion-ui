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

#include "FiredAction.hh"

namespace a3
{

FlightMotion
flightMotionFrom (ActionScriptResult const &result)
{
  auto const &s = result.settings;
  auto const said = [&result] (char const *name) {
    return result.assigned.contains (name);
  };

  FlightMotion motion;
  if (said ("spin"))
    motion.spin = s.spin;
  if (said ("sway"))
    motion.sway = s.elevationLfo;
  if (said ("swell"))
    motion.swell = s.reachLfo;
  if (said ("tilt"))
    motion.tilt = s.tilt;
  if (said ("roll"))
    motion.roll = s.roll;
  if (said ("tswp"))
    motion.tiltSweep = s.tiltLfo;
  if (said ("rswp"))
    motion.rollSweep = s.rollLfo;
  if (said ("speedLog2"))
    motion.speedLog2 = s.speedLog2;
  return motion;
}

FiredAction
fireActionAt (juce::String const &source, ClipSettings const &base,
              juce::int64 seed, ActionFeel const &feel)
{
  auto const result = runActionScript (source, base, seed);
  return { withFeel (result.settings, feel), flightMotionFrom (result),
           result.pilot };
}

}
