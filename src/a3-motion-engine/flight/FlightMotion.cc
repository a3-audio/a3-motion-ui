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

#include "FlightMotion.hh"

#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
constexpr float degreesPerTurn = 360.f;

int
senseOf (int step)
{
  return step > 0 ? 1 : -1;
}

/** Where a cycle of `cycle` beats stands at `beats`, 0..1, running backwards
 *  for a negative `sense`. Counted off the beat, so it is locked to the bar
 *  like every TempoLfo cycle. */
float
cyclePhase (double beats, float cycle, int sense)
{
  if (cycle <= 0.f)
    return 0.f;
  auto const cycles = static_cast<double> (sense) * beats / static_cast<double> (cycle);
  return static_cast<float> (cycles - std::floor (cycles));
}

/** The beats one whole turn takes at the angular cap. */
float
shortestTurnBeats (FlightTuning const &tuning)
{
  return tuning.angularCapDegreesPerBeat > 0.f
             ? degreesPerTurn / tuning.angularCapDegreesPerBeat
             : 0.f;
}

/** One lean, turned round by its sweep the way SpaceTurn's leanOf turns a
 *  clip's: the sweep's phase in whole turns, four quarter turns each. */
float
sweptLean (std::optional<float> lean, std::optional<int> sweep, double beats,
           int beatsPerBar, FlightTuning const &tuning)
{
  auto const standing = lean.value_or (0.f);
  if (!sweep || *sweep == 0)
    return wrappedLean (standing);
  auto const cycle = cycleBeats (*sweep, beatsPerBar, shortestTurnBeats (tuning));
  return wrappedLean (standing
                      + cyclePhase (beats, cycle, senseOf (*sweep))
                            * leanQuarterTurnsPerTurn);
}
}

bool
FlightMotion::any () const
{
  return spin || sway || swell || tilt || roll || tiltSweep || rollSweep
         || speedLog2;
}

bool
FlightMotion::movesTheFloor () const
{
  return spin || swell || speedLog2;
}

float
baseLapsPerBeat (int beatsPerBar, FlightTuning const &tuning)
{
  auto const lap = tuning.orbitLapBars * static_cast<float> (beatsPerBar);
  return lap > 0.f ? 1.f / lap : 0.f;
}

float
wantedLapsPerBeat (FlightMotion const &motion, int beatsPerBar,
                   FlightTuning const &tuning)
{
  auto laps = baseLapsPerBeat (beatsPerBar, tuning);
  if (motion.spin)
    {
      // The clip's sense (spinPosition): a positive spin turns against the
      // floor's counter-clockwise, which is the base path's way round.
      auto const bars = lfoBarsPerCycle (*motion.spin);
      laps = bars > 0.f && beatsPerBar > 0
                 ? -static_cast<float> (senseOf (*motion.spin))
                       / (bars * static_cast<float> (beatsPerBar))
                 : 0.f;
    }
  if (motion.speedLog2)
    laps *= std::exp2 (-static_cast<float> (*motion.speedLog2));

  auto const cap = std::max (tuning.angularCapDegreesPerBeat, 0.f) / degreesPerTurn;
  return std::clamp (laps, -cap, cap);
}

float
cycleBeats (int step, int beatsPerBar, float shortestBeats)
{
  auto const bars = lfoBarsPerCycle (step);
  if (bars <= 0.f || beatsPerBar <= 0)
    return 0.f;
  return std::max (bars * static_cast<float> (beatsPerBar), shortestBeats);
}

float
shortestTravelBeats (float degrees, float capDegreesPerBeat)
{
  return capDegreesPerBeat > 0.f
             ? pi<float> () * std::abs (degrees) / capDegreesPerBeat
             : 0.f;
}

float
swellScale (FlightMotion const &motion, double beats, int beatsPerBar,
            FlightTuning const &tuning)
{
  if (!motion.swell || *motion.swell == 0)
    return 1.f;
  auto const longAxis = tuning.orbitRadius * (1.f + tuning.orbitEccentricity);
  auto const outer = longAxis > 0.f ? tuning.swellOuterReach / longAxis : 1.f;
  auto const end = *motion.swell > 0 ? outer : tuning.swellInnerScale;
  auto const cycle = cycleBeats (*motion.swell, beatsPerBar, 0.f);
  return 1.f
         + (end - 1.f)
               * lfoTravel (cyclePhase (beats, cycle, senseOf (*motion.swell)));
}

float
swayDegrees (FlightMotion const &motion, double beats, int beatsPerBar,
             FlightTuning const &tuning)
{
  if (!motion.sway || *motion.sway == 0)
    return 0.f;
  auto const cycle = cycleBeats (
      *motion.sway, beatsPerBar,
      shortestTravelBeats (tuning.swayTravelDegrees, tuning.verticalCapDegreesPerBeat));
  auto const way = *motion.sway > 0 ? -1.f : 1.f;
  return way * tuning.swayTravelDegrees
         * lfoTravel (cyclePhase (beats, cycle, senseOf (*motion.sway)));
}

SpaceTurn
flightLean (FlightMotion const &motion, double beats, int beatsPerBar,
            FlightTuning const &tuning)
{
  return { sweptLean (motion.tilt, motion.tiltSweep, beats, beatsPerBar, tuning),
           sweptLean (motion.roll, motion.rollSweep, beats, beatsPerBar, tuning) };
}

}
