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

namespace a3
{

/** How far the engine's clock should move to sit on a beat that just arrived,
 *  in beats. Positive moves it forwards, negative holds it back.
 *
 *  In EXT and PIO the engine used to take only the *tempo* from the beats it
 *  was sent and count on by itself. Measured on 2026-09-17 against a click of
 *  known tempo, its beats then slid a whole beat against the music every
 *  twenty-odd seconds and nothing ever pulled them back: any error in the
 *  tempo, however small, adds up without bound when nothing looks at the
 *  phase.
 *
 *  `enginePositionInBar` is where the engine's clock stood when the beat
 *  arrived, in beats from its own downbeat (0 up to `beatsPerBar`).
 *  `arrivingBeat` is which beat of the bar the source says that was, counted
 *  from 0 -- the source's downbeat is the one a clip waiting for a downbeat
 *  has to start on.
 *
 *  The error is taken the short way round the bar. Inside `beatSyncLockWindow`
 *  it is pulled in by `beatSyncGain` a beat, which is inaudible and still
 *  holds a drift of a few per cent to a fraction of a tick. Outside it the
 *  engine is on the wrong beat, and easing there would be many beats out of
 *  time, so it goes the whole way at once. Exactly half a bar goes forwards:
 *  catching up beats standing still. */
double beatSyncShift (double enginePositionInBar, int arrivingBeat,
                      int beatsPerBar);

/** Past this far off, in beats, the engine jumps instead of easing. */
constexpr double beatSyncLockWindow = 0.25;
/** The share of a small error taken out on each beat. */
constexpr double beatSyncGain = 0.3;

}
