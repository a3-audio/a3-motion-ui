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

#include "TrajectoryShaping.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

namespace a3
{

float
squeezeFactor (float amount)
{
  return std::pow (2.f, std::clamp (amount, -1.f, 1.f));
}

float
turnsOf (Pattern const &pattern)
{
  auto const spun
      = pattern.getSpin () == 0 ? 0.f : pattern.getSpinPhase ();

  // Wrapped, so a reader has a position rather than a running total.
  auto wrapped = std::fmod (pattern.getRotate () + spun, 1.f);
  if (wrapped < 0.f)
    wrapped += 1.f;

  return wrapped;
}

PlaneShaping
shapingOf (Pattern const &pattern)
{
  PlaneShaping shaping;

  shaping.turns = turnsOf (pattern);

  // Each squeeze, swept out of where it was set and back if its own sweep is
  // running. Bipolar, so the sign names one of the two ends rather than one
  // of zero and one -- pressing an axis flat and pulling it out are the two
  // directions, and a sweep has to be able to ask for either.
  shaping.squeezeX
      = lfoSweepBipolar (pattern.getSqueezeX (), pattern.getSqueezeXLfo (),
                         pattern.getSqueezeXLfoPhase ());
  shaping.squeezeY
      = lfoSweepBipolar (pattern.getSqueezeY (), pattern.getSqueezeYLfo (),
                         pattern.getSqueezeYLfoPhase ());

  return shaping;
}

Pos
shapedPosition (Pos const &recorded, PlaneShaping const &shaping)
{
  // No sign to arrange here, unlike spinPosition(): the screen mirrors these
  // coordinates, and a mirror leaves a scaling about the origin alone.
  auto const squeezed = Pos::fromCartesian (
      recorded.x () * squeezeFactor (shaping.squeezeX),
      recorded.y () * squeezeFactor (shaping.squeezeY), recorded.z ());

  return spinPosition (squeezed, shaping.turns);
}

}
