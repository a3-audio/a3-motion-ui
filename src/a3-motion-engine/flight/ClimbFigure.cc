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

#include "ClimbFigure.hh"

#include <a3-motion-engine/flight/FigureMath.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Stretch 1 is the last before the 1, 2 the one before it, and so on. */
double
stretchOf (double beatsToGo, double stretchBeats)
{
  return std::max (1., std::ceil (beatsToGo / stretchBeats - 1e-9));
}

double
stretchBeatsOf (int beatsPerBar, GameTuning const &tuning)
{
  return static_cast<double> (std::max (1, tuning.climbDoubleEveryBars)) * beatsPerBar;
}
}

float
climbLapBars (double barsToGo, GameTuning const &tuning)
{
  auto const stretch = stretchOf (barsToGo, static_cast<double> (std::max (1, tuning.climbDoubleEveryBars)));
  auto const lap = tuning.climbFastestLapBars * std::pow (2.f, static_cast<float> (stretch - 1.));
  auto const slowest = std::max (tuning.climbFastestLapBars, tuning.climbStartLapBars);
  return std::clamp (lap, tuning.climbFastestLapBars, slowest);
}

double
climbLaps (double from, double to, double climaxBeats, int beatsPerBar, GameTuning const &tuning)
{
  if (beatsPerBar <= 0 || to <= from)
    return 0.;
  auto const bar = static_cast<double> (beatsPerBar);
  auto const stretchBeats = stretchBeatsOf (beatsPerBar, tuning);
  auto laps = 0.;
  auto now = from;
  while (now < to)
    {
      auto const toGo = climaxBeats - now;
      auto const stretchEnd = climaxBeats - (stretchOf (toGo, stretchBeats) - 1.) * stretchBeats;
      auto const end = stretchEnd > now ? std::min (to, stretchEnd) : to;
      laps += (end - now) / (static_cast<double> (climbLapBars (toGo / bar, tuning)) * bar);
      now = end;
    }
  return laps;
}

ClimbLanding
climbLanding (MusicCue const &cue, double opening, int beatsPerBar, GameTuning const &tuning)
{
  auto const bar = static_cast<double> (std::max (1, beatsPerBar));
  auto const towardsADrop = cue.section == MusicSection::Build && cue.next
                            && *cue.next == MusicSection::Drop && cue.changeBar >= 0;
  if (towardsADrop)
    {
      auto const drop = static_cast<double> (cue.changeBar) * bar;
      if (drop - opening >= bar)
        return { drop, cue.previewed };
    }
  auto const phrase = static_cast<double> (tuning.fallbackPhraseBars) * bar;
  auto const after = opening + bar;
  return { phrase > 0. ? std::ceil (after / phrase) * phrase : after, false };
}

bool
dropOpensOn (MusicCue const &cue, long long bar)
{
  if (cue.section == MusicSection::Drop)
    return cue.sectionStartBar == bar;
  return cue.section == MusicSection::Build && cue.next && *cue.next == MusicSection::Drop
         && cue.changeBar == bar;
}

void
planClimb (GamePlan &plan, std::array<ShipState, flightShips> const &ships, MusicCue const &cue,
           int beatsPerBar, FlightTuning const &flight, GameTuning const &tuning)
{
  auto const landing = climbLanding (cue, plan.startBeats, beatsPerBar, tuning);
  plan.climaxBeats = landing.climaxBeats;
  plan.chainsIntoSlap = landing.chainsIntoSlap;
  plan.endBeats = plan.climaxBeats + static_cast<double> (tuning.climbHoldBars) * beatsPerBar;
  plan.radius = flight.orbitRadius;

  auto const &lead = ships[shipIndex (plan.leader)];
  auto const sense = flightSense (lead.p, lead.v);
  plan.axis = angleOf (lead.p);
  auto part = 1;
  for (auto s = 0; s < flightShips; ++s)
    {
      if (!plan.crew[shipIndex (s)])
        continue;
      plan.turn[shipIndex (s)] = sense;
      plan.part[shipIndex (s)] = s == plan.leader ? 0 : part++;
    }
}

OrbitPoint
climbGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar, GameTuning const &tuning)
{
  auto const bar = static_cast<double> (beatsPerBar);
  auto const sense = plan.turn[shipIndex (ship)];
  auto const share = plan.crewSize > 0
                         ? 2.f * pi<float> () * static_cast<float> (plan.part[shipIndex (ship)])
                               / static_cast<float> (plan.crewSize)
                         : 0.f;
  auto laps = climbLaps (plan.startBeats, std::min (beats, plan.climaxBeats), plan.climaxBeats,
                         beatsPerBar, tuning);
  auto radius = tuning.climbTopRadius;
  auto inwards = 0.f; // floor units a beat, towards the middle
  double lapBeats;
  if (beats < plan.climaxBeats)
    {
      auto const span = plan.climaxBeats - plan.startBeats;
      auto const u = static_cast<float> (span > 0. ? (beats - plan.startBeats) / span : 1.);
      radius = plan.radius + (tuning.climbTopRadius - plan.radius) * std::clamp (u, 0.f, 1.f);
      inwards = span > 0. ? (plan.radius - tuning.climbTopRadius) / static_cast<float> (span) : 0.f;
      lapBeats = static_cast<double> (climbLapBars ((plan.climaxBeats - beats) / bar, tuning)) * bar;
    }
  else
    {
      lapBeats = static_cast<double> (tuning.climbHoldLapBars) * bar;
      laps += (beats - plan.climaxBeats) / lapBeats;
    }
  auto const turns = static_cast<float> (laps - std::floor (laps));
  auto const angle = plan.axis + share + sense * 2.f * pi<float> () * turns;
  auto const out = polar (angle, 1.f);
  Vec2 const along{ -out.y, out.x };
  auto const rate = sense * 2.f * pi<float> () / static_cast<float> (lapBeats);
  return { out * radius, along * (radius * rate) - out * inwards };
}

}
