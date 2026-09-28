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

#include "SpaceTurn.hh"

#include <a3-motion-engine/Pattern.hh>

#include <cmath>

namespace a3
{

namespace
{
constexpr float quarterTurn = 1.57079632679f;

/** About the left-right axis: positive takes the front (x) down. */
Pos
aboutLeftRight (Pos const &d, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return Pos::fromCartesian (d.x () * c + d.z () * s, d.y (),
                             -d.x () * s + d.z () * c);
}

/** About the front-back axis: positive takes the left (y) down. */
Pos
aboutFrontBack (Pos const &d, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return Pos::fromCartesian (d.x (), d.y () * c + d.z () * s,
                             -d.y () * s + d.z () * c);
}
}

Pos
turnedInSpace (Pos const &direction, SpaceTurn turn)
{
  if (!direction.isValid ())
    return direction;
  return aboutFrontBack (aboutLeftRight (direction, turn.tilt * quarterTurn),
                         turn.roll * quarterTurn);
}

Pos
unturnedInSpace (Pos const &direction, SpaceTurn turn)
{
  if (!direction.isValid ())
    return direction;
  return aboutLeftRight (aboutFrontBack (direction, -turn.roll * quarterTurn),
                         -turn.tilt * quarterTurn);
}

float
wrappedLean (float quarterTurns)
{
  auto const half = leanQuarterTurnsPerTurn * 0.5f;
  // Untouched when already on the ring: shifting by a half turn and back
  // costs a float's last bit, and a saved lean has to come back as it was.
  if (quarterTurns >= -half && quarterTurns < half)
    return quarterTurns;

  auto wrapped = std::fmod (quarterTurns + half, leanQuarterTurnsPerTurn);
  if (wrapped < 0.f)
    wrapped += leanQuarterTurnsPerTurn;
  return wrapped - half;
}

namespace
{
/** A standing lean and how far its sweep has carried it round, counted only
 *  while the sweep runs. */
float
leanOf (float standing, int sweep, float phase)
{
  auto const turned = sweep == 0 ? 0.f : phase * leanQuarterTurnsPerTurn;
  return wrappedLean (standing + turned);
}
}

SpaceTurn
spaceTurnOf (Pattern const &pattern)
{
  // Round and round, like the spin, rather than out and back (2026-09-28):
  // the phase is in cycles, and a cycle is a whole turn of the plane.
  // Through getKnob: a take's lanes play over the settings here too.
  return { leanOf (pattern.getKnob (Knob::Tilt),
                   pattern.getKnobStep (Knob::TiltSweep),
                   pattern.getTiltLfoPhase ()),
           leanOf (pattern.getKnob (Knob::Roll),
                   pattern.getKnobStep (Knob::RollSweep),
                   pattern.getRollLfoPhase ()) };
}

}
