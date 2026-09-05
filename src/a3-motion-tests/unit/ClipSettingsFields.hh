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

#include <a3-motion-engine/ClipSettings.hh>

#include <functional>
#include <utility>
#include <vector>

namespace a3
{

/** Every field of ClipSettings, with a way to move it off its default.
 *
 *  Shared rather than copied because two tests walk it for different reasons
 *  -- one checks that a field takes part in the comparison, the other that a
 *  fired action either drives it or deliberately leaves it alone. A field
 *  added to the struct and forgotten here would slip past both at once, so
 *  this list is the one place to keep honest.
 */
inline std::vector<
    std::pair<char const *, std::function<void (ClipSettings &)> > > const &
clipSettingsFields ()
{
  static std::vector<
      std::pair<char const *, std::function<void (ClipSettings &)> > > const
      fields{
        { "speedLog2", [] (ClipSettings &s) { s.speedLog2 = -3; } },
        { "rotate", [] (ClipSettings &s) { s.rotate = 0.3f; } },
        { "reach", [] (ClipSettings &s) { s.reach = 0.9f; } },
        { "clipTop", [] (ClipSettings &s) { s.clipTop = 0.2f; } },
        { "clipBottom", [] (ClipSettings &s) { s.clipBottom = 0.3f; } },
        { "mirrorSouth", [] (ClipSettings &s) { s.mirrorSouth = true; } },
        { "elevationBase",
          [] (ClipSettings &s) { s.elevationBase = 0.75f; } },
        { "flat", [] (ClipSettings &s) { s.flat = true; } },
        { "flatElevation", [] (ClipSettings &s) { s.flatElevation = 0.8f; } },
        { "spin", [] (ClipSettings &s) { s.spin = 5; } },
        { "reachLfo", [] (ClipSettings &s) { s.reachLfo = -4; } },
        { "envelopeAttack", [] (ClipSettings &s) { s.envelopeAttack = 6; } },
        { "envelopeDecay", [] (ClipSettings &s) { s.envelopeDecay = 6; } },
        { "envelopeMax", [] (ClipSettings &s) { s.envelopeMax = 0.1f; } },
        { "freqAttack", [] (ClipSettings &s) { s.freqAttack = 5; } },
        { "freqDecay", [] (ClipSettings &s) { s.freqDecay = 6; } },
        { "freqMax", [] (ClipSettings &s) { s.freqMax = 0.7f; } },
        { "qAttack", [] (ClipSettings &s) { s.qAttack = 1; } },
        { "qDecay", [] (ClipSettings &s) { s.qDecay = 4; } },
        { "qMax", [] (ClipSettings &s) { s.qMax = 0.6f; } },
        { "actMode", [] (ClipSettings &s) { s.actMode = ActMode::Hold; } },
        { "direction",
          [] (ClipSettings &s) { s.direction = PlayDirection::Reverse; } },
        { "endAction",
          [] (ClipSettings &s) { s.endAction = EndAction::Bounce; } },
        { "fadeReach", [] (ClipSettings &s) { s.fadeReach = 0.9f; } },
        { "bridgeBias", [] (ClipSettings &s) { s.bridgeBias = 3; } },
      };

  return fields;
}

}
