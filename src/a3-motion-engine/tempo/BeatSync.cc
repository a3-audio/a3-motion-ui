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

#include "BeatSync.hh"

#include <cmath>

namespace a3
{

double
beatSyncShift (double enginePositionInBar, int arrivingBeat, int beatsPerBar)
{
  if (beatsPerBar < 1)
    return 0.0;

  auto const bar = static_cast<double> (beatsPerBar);
  auto const target = std::fmod (
      std::fmod (static_cast<double> (arrivingBeat), bar) + bar, bar);

  // Ahead is positive, folded into [-bar/2, bar/2).
  auto ahead = std::fmod (enginePositionInBar - target, bar);
  if (ahead < -0.5 * bar)
    ahead += bar;
  if (ahead >= 0.5 * bar)
    ahead -= bar;

  if (std::abs (ahead) > beatSyncLockWindow)
    return -ahead;
  return -ahead * beatSyncGain;
}

}
