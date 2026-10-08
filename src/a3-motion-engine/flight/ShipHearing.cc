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

/** Where `from` ends up turned by `radians` about a great circle that leaves it
 *  in a fixed, arbitrary direction -- for opposite directions, which have no
 *  one great circle of their own. About from x up, or from x front near a pole. */
Pos
turnedAboutSomeGreatCircle (Pos const &from, double radians, float distance)
{
  auto const length = static_cast<double> (from.distance ());
  double const v[3] = { from.x () / length, from.y () / length, from.z () / length };
  auto axis = [&v] (double const (&about)[3], double (&k)[3])
  {
    k[0] = v[1] * about[2] - v[2] * about[1];
    k[1] = v[2] * about[0] - v[0] * about[2];
    k[2] = v[0] * about[1] - v[1] * about[0];
    return std::sqrt (k[0] * k[0] + k[1] * k[1] + k[2] * k[2]);
  };
  double k[3];
  double const up[3] = { 0., 0., 1. };
  double const front[3] = { 1., 0., 0. };
  auto norm = axis (up, k);
  if (norm < 1e-3)
    norm = axis (front, k);
  for (auto &component : k)
    component /= norm;
  // k is perpendicular to v, so Rodrigues' rotation is cos v + sin (k x v).
  auto const c = std::cos (radians);
  auto const s = std::sin (radians);
  auto const kxv0 = k[1] * v[2] - k[2] * v[1];
  auto const kxv1 = k[2] * v[0] - k[0] * v[2];
  auto const kxv2 = k[0] * v[1] - k[1] * v[0];
  return Pos::fromCartesian (static_cast<float> ((c * v[0] + s * kxv0) * distance),
                             static_cast<float> ((c * v[1] + s * kxv1) * distance),
                             static_cast<float> ((c * v[2] + s * kxv2) * distance));
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
    return { turnedAboutSomeGreatCircle (from, limit, to.distance ()), false };

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
