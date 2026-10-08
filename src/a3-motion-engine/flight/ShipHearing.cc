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

#include "ShipHearing.hh"

#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/flight/Handover.hh>

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Short of the poles, where azimuth has no meaning. */
constexpr float highestElevation = 89.9f;
/** Below this the two directions are taken as opposite. */
constexpr double noGreatCircle = 1e-6;

Pos
swungInHeight (Pos const &direction, float degrees)
{
  if (juce::exactlyEqual (degrees, 0.f))
    return direction;
  auto const elevation = std::clamp (direction.elevation () + degrees,
                                     -highestElevation, highestElevation);
  return Pos::fromSpherical (direction.azimuth (), elevation, direction.distance ());
}

double
dot (Pos const &a, Pos const &b)
{
  return static_cast<double> (a.x () * b.x () + a.y () * b.y () + a.z () * b.z ());
}
}

Pos
heardShip (Pos const &mapped, FlightMotion const &motion, float weight,
           double beats, int beatsPerBar, FlightTuning const &tuning)
{
  if (weight <= 0.f || !motion.any () || !mapped.isValid ())
    return mapped;
  auto const swung
      = swungInHeight (mapped, swayDegrees (motion, beats, beatsPerBar, tuning));
  auto const leant
      = turnedInSpace (swung, flightLean (motion, beats, beatsPerBar, tuning));
  return handover (mapped, leant, smoothstep (weight));
}

TurnLimited
limitTurn (Pos const &from, Pos const &to, float maxDegrees)
{
  if (!from.isValid () || !to.isValid ())
    return { to, true };

  auto const lengths = static_cast<double> (from.distance () * to.distance ());
  if (lengths <= 0.)
    return { to, true };
  auto const angle = std::acos (std::clamp (dot (from, to) / lengths, -1., 1.));
  auto const limit = static_cast<double> (maxDegrees) * juce::MathConstants<double>::pi / 180.;
  if (angle <= limit)
    return { to, true };

  auto const sine = std::sin (angle);
  if (sine < noGreatCircle)
    return { from, false };

  // Spherical interpolation by exactly `limit`, on the unit sphere, then back
  // at `to`'s distance.
  auto const a = std::sin (angle - limit) / sine / static_cast<double> (from.distance ());
  auto const b = std::sin (limit) / sine / static_cast<double> (to.distance ());
  auto const x = a * from.x () + b * to.x ();
  auto const y = a * from.y () + b * to.y ();
  auto const z = a * from.z () + b * to.z ();
  auto const scale = static_cast<double> (to.distance ());
  return { Pos::fromCartesian (static_cast<float> (x * scale),
                               static_cast<float> (y * scale),
                               static_cast<float> (z * scale)),
           false };
}

}
