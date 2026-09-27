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

#include "ExternalTempo.hh"

#include <cmath>

namespace a3
{

bool
ExternalTempoFollower::isNear (float bpm, float reference) const
{
  return reference > 0.f
         && std::abs (bpm / reference - 1.f) <= externalTempoTolerance;
}

float
ExternalTempoFollower::onBeat (float reportedBpm)
{
  if (!std::isfinite (reportedBpm) || reportedBpm <= 0.f)
    return _tempo;

  if (_tempo <= 0.f)
    {
      _tempo = _target = reportedBpm;
      return _tempo;
    }

  if (isNear (reportedBpm, _target))
    {
      // The tempo it follows, drifting: taken, and glided to below.
      _target = reportedBpm;
      _candidateBeats = 0;
      _octaveBeats = 0;
    }
  else if (isNear (reportedBpm * 2.f, _target)
           || isNear (reportedBpm * 0.5f, _target))
    {
      // Half or double: the same tempo, counted differently -- unless it
      // holds long enough to be a track that changed.
      _candidateBeats = 0;
      if (++_octaveBeats >= externalTempoOctaveBeats)
        {
          _target = reportedBpm;
          _octaveBeats = 0;
        }
    }
  else
    {
      // Far off: a stray, until it holds.
      _octaveBeats = 0;
      if (_candidateBeats > 0 && isNear (reportedBpm, _candidate))
        ++_candidateBeats;
      else
        {
          _candidate = reportedBpm;
          _candidateBeats = 1;
        }

      if (_candidateBeats >= externalTempoChangeBeats)
        {
          _target = reportedBpm;
          _candidateBeats = 0;
        }
    }

  _tempo += (_target - _tempo) * externalTempoGlide;
  if (std::abs (_target - _tempo) < 0.001f)
    _tempo = _target;
  return _tempo;
}

void
ExternalTempoFollower::reset ()
{
  *this = ExternalTempoFollower{};
}

}
