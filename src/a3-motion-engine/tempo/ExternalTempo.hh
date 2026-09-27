/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

/** How many reports in a row a real tempo change has to hold before it is
 *  taken. Fewer is a stray: the source miscounted a beat. */
constexpr int externalTempoChangeBeats = 4;

/** How many reports in a row the other octave -- half or double -- has to
 *  hold before it is taken. Fewer is the source miscounting the same beat;
 *  more is a track that changed. Sixteen are about seven seconds at 140. */
constexpr int externalTempoOctaveBeats = 16;

/** How close a report has to be to count as the tempo it follows, and to the
 *  next one to count as the same change -- as a share of the tempo. */
constexpr float externalTempoTolerance = 0.03f;

/** The share of the way to a new tempo covered on each beat. */
constexpr float externalTempoGlide = 0.25f;

/** The tempo the engine runs at, followed from the one an external clock
 *  reports on every beat (a3-motion-ui#36).
 *
 *  The engine took every report as it came, and an unsteady source -- one
 *  that flipped between 70 and 140 on a sixth of the beats of a 140 track --
 *  ran every clip at double speed for a beat. This stands between the two:
 *  half or double is the same tempo counted differently, a far-off report
 *  that does not hold is a stray, and whatever is taken is glided to rather
 *  than stepped. The engine's own phase follower keeps it on the beat. */
class ExternalTempoFollower
{
public:
  /** One report, on a beat; the tempo to run at now. */
  float onBeat (float reportedBpm);

  /** Forget what was followed: the next report is taken as it is. */
  void reset ();

private:
  bool isNear (float bpm, float reference) const;

  float _tempo = 0.f;
  float _target = 0.f;
  float _candidate = 0.f;
  int _candidateBeats = 0;
  int _octaveBeats = 0;
};

}
