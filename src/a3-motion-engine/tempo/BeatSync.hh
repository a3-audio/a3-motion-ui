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

/** Keeps the engine's clock on the beats an external clock sends (EXT, PIO),
 *  without ever jumping (a3-motion-ui#36).
 *
 *  In EXT and PIO the engine used to take only the *tempo* from the beats it
 *  was sent and count on by itself. Measured on 2026-09-17 against a click of
 *  known tempo, its beats then slid a whole beat against the music every
 *  twenty-odd seconds: any error in the tempo adds up when nothing looks at
 *  the phase. So each arriving beat pulls the clock towards it.
 *
 *  It used to jump the whole way past a quarter beat, and an unsteady source
 *  -- outliers up to 400 ms at 70 BPM -- made the engine jump its phase on
 *  stray beats. Now the error is taken in two parts:
 *
 *  - **Which beat of the bar** (the whole beats): renumbered, not moved in
 *    time. Which beat it is changes; where playback is does not, so a clip
 *    plays on without a burst of ticks. Only once a second beat confirms it.
 *  - **Where in the beat** (the fraction): pulled in by `beatSyncGain`, never
 *    by more than `beatSyncMaxStep` a beat. Past `beatSyncLockWindow` it is a
 *    stray until the next beat confirms it, and then caught up by the same
 *    capped steps -- never in one jump.
 *
 *  Taken the short way round the bar; exactly half a bar goes forwards. */
struct BeatSyncCorrection
{
  /** How far to move the clock's time, in beats; positive is forwards. */
  double timeShift = 0.0;
  /** How many beats to add to the clock's beat number, without moving it. */
  int beatsToAdd = 0;
};

class BeatPhaseFollower
{
public:
  /** `enginePositionInBar` is where the engine's clock stood when the beat
   *  arrived, in beats from its own downbeat (0 up to `beatsPerBar`).
   *  `arrivingBeat` is which beat of the bar the source says that was,
   *  counted from 0. */
  BeatSyncCorrection onBeat (double enginePositionInBar, int arrivingBeat,
                             int beatsPerBar);

private:
  bool _farPending = false;
  double _farFraction = 0.0;
  bool _wholePending = false;
  long _wholeBeats = 0;
};

/** Inside this far off, in beats, a beat is taken at once; past it, only when
 *  the next one confirms it. */
constexpr double beatSyncLockWindow = 0.25;
/** The share of a small error taken out on each beat. */
constexpr double beatSyncGain = 0.3;
/** The most the engine's time is moved on one beat, in beats. */
constexpr double beatSyncMaxStep = 0.08;

}
