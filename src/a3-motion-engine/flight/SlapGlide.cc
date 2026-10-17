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


#include "SlapGlide.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Short of the pole, where a direction's azimuth means nothing. */
constexpr float nearThePole = 89.f;

/** Whether turning from `from` by `delta` degrees passes `through`. */
bool
passes (float from, float delta, float through)
{
  auto const to = std::remainder (through - from, 360.f);
  if (delta >= 0.f)
    return (to < 0.f ? to + 360.f : to) <= delta;
  return (to > 0.f ? to - 360.f : to) >= delta;
}

/** Degrees to turn from azimuth `from` to `to` along `path`: the short way,
 *  unless the other way passes the front (ViaFront) or the side `from` is
 *  heard on (ViaSide) and the short one does not. */
float
turnAlong (float from, float to, GlidePath path)
{
  auto const shortest = std::remainder (to - from, 360.f);
  auto const other = shortest >= 0.f ? shortest - 360.f : shortest + 360.f;
  auto const through = [&] (float mark) {
    return passes (from, shortest, mark) || !passes (from, other, mark) ? shortest : other;
  };
  switch (path)
    {
    case GlidePath::ViaFront:
      return through (0.f);
    case GlidePath::ViaSide:
      return through (from >= 0.f ? 90.f : -90.f);
    case GlidePath::Shortest:
    case GlidePath::Vertical:
      return shortest;
    }
  return shortest;
}
}

double
slapGlideLength (GlidePath path, GameTuning const &tuning)
{
  auto const glide = static_cast<double> (tuning.slapGlideBeats);
  if (path != GlidePath::Vertical || tuning.slapVerticalRateFactor <= 0.f)
    return glide;
  return glide / static_cast<double> (tuning.slapVerticalRateFactor);
}

Pos
slapGlideAt (Pos const &from, Pos const &to, GlidePath path, double elapsed,
             GameTuning const &tuning)
{
  auto const length = slapGlideLength (path, tuning);
  if (!from.isValid () || !to.isValid () || length <= 0. || elapsed >= length)
    return to;
  auto const u = static_cast<float> (std::clamp (elapsed / length, 0., 1.));
  auto const elevation = from.elevation () + (to.elevation () - from.elevation ()) * u;
  auto const azimuth
      = path == GlidePath::Vertical
            ? (to.elevation () < nearThePole ? to.azimuth () : from.azimuth ())
            : from.azimuth () + turnAlong (from.azimuth (), to.azimuth (), path) * u;
  return Pos::fromSpherical (azimuth, elevation, to.distance ());
}

std::optional<Pos>
SlapGlider::step (FigureStance const &stance, Vec2 holdAt, Pos const &lastHeard,
                  Pos const &target, double beats, GameTuning const &tuning)
{
  auto const moved = stance.hold && (!_held || holdAt != _heldAt);
  _held = stance.hold;
  _heldAt = holdAt;
  if (moved && lastHeard.isValid ())
    {
      _gliding = true;
      _from = lastHeard;
      _path = stance.glide;
      _startBeats = beats;
    }
  if (!_gliding)
    return std::nullopt;
  auto const elapsed = beats - _startBeats;
  if (elapsed >= slapGlideLength (_path, tuning))
    {
      _gliding = false;
      return target;
    }
  return slapGlideAt (_from, target, _path, elapsed, tuning);
}

void
SlapGlider::reset ()
{
  *this = SlapGlider{};
}

}
