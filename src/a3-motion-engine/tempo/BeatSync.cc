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

#include <algorithm>
#include <cmath>

namespace a3
{

BeatSyncCorrection
BeatPhaseFollower::onBeat (double enginePositionInBar, int arrivingBeat,
                          int beatsPerBar)
{
  if (beatsPerBar < 1)
    return {};

  auto const bar = static_cast<double> (beatsPerBar);
  auto const target = std::fmod (
      std::fmod (static_cast<double> (arrivingBeat), bar) + bar, bar);

  // Ahead is positive, folded into [-bar/2, bar/2).
  auto ahead = std::fmod (enginePositionInBar - target, bar);
  if (ahead < -0.5 * bar)
    ahead += bar;
  if (ahead >= 0.5 * bar)
    ahead -= bar;

  auto const whole = std::lround (ahead);
  auto const fraction = ahead - static_cast<double> (whole);

  BeatSyncCorrection correction;

  // Which beat of the bar: renumbered once a second beat says the same.
  if (whole == 0)
    _wholePending = false;
  else if (_wholePending && _wholeBeats == whole)
    {
      correction.beatsToAdd = static_cast<int> (-whole);
      _wholePending = false;
    }
  else
    {
      _wholePending = true;
      _wholeBeats = whole;
    }

  // Where in the beat: eased, capped, and a stray until confirmed.
  auto const eased = [fraction] {
    return std::clamp (-fraction * beatSyncGain, -beatSyncMaxStep,
                       beatSyncMaxStep);
  };

  if (std::abs (fraction) <= beatSyncLockWindow)
    {
      _farPending = false;
      correction.timeShift = eased ();
    }
  else if (_farPending
           && std::abs (fraction - _farFraction) <= beatSyncLockWindow)
    {
      _farFraction = fraction;
      correction.timeShift = eased ();
    }
  else
    {
      _farPending = true;
      _farFraction = fraction;
    }

  return correction;
}

}
