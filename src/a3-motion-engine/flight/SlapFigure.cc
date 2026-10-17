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

#include "SlapFigure.hh"

#include <a3-motion-engine/flight/GameMoments.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** The middle, a hair towards the down point: the band's top, with an
 *  azimuth of its own. */
constexpr float poleOffset = 1e-3f;

size_t
at (int ship)
{
  return static_cast<size_t> (ship);
}

bool
keepsClear (std::array<Vec2, 2> const &sides, GamePlan const &plan,
            std::array<ShipState, flightShips> const &ships, FigureCall const &call,
            GameTuning const &tuning)
{
  for (auto s = 0; s < flightShips; ++s)
    {
      if (plan.crew[at (s)] || !call.flying[at (s)])
        continue;
      for (auto const &side : sides)
        if (ships[at (s)].p.getDistanceFrom (side) < tuning.slapClearSide)
          return false;
    }
  return true;
}

Vec2
slapPoint (GamePlan const &plan, int ship, int event)
{
  auto const mirrored = plan.part[at (ship)] % 2;
  if (plan.snaps && event == 0)
    return mirrored != 0 ? -plan.opening : plan.opening;
  auto const side = (plan.snaps ? event - 1 : event) + mirrored;
  return plan.sides[static_cast<size_t> (side % 2)];
}
}

SlapAxis
chooseSlapAxis (std::array<bool, slapAxes> const &allowed, juce::Random &dice,
                GameTuning const &tuning)
{
  std::array<float, slapAxes> const weights{ tuning.slapLeftRightWeight,
                                             tuning.slapFrontBackWeight,
                                             tuning.slapUpDownWeight };
  auto total = 0.f;
  for (auto a = 0; a < slapAxes; ++a)
    if (allowed[at (a)])
      total += std::max (0.f, weights[at (a)]);
  if (total <= 0.f)
    return SlapAxis::LeftRight;

  auto thrown = dice.nextFloat () * total;
  auto last = SlapAxis::LeftRight;
  for (auto a = 0; a < slapAxes; ++a)
    {
      if (!allowed[at (a)] || weights[at (a)] <= 0.f)
        continue;
      last = static_cast<SlapAxis> (a);
      thrown -= weights[at (a)];
      if (thrown < 0.f)
        return last;
    }
  return last; // rounding left the throw on the last edge
}

std::array<Vec2, 2>
slapSides (SlapAxis axis, float azimuth, GameTuning const &tuning)
{
  auto const r = tuning.slapSideRadius;
  switch (axis)
    {
    case SlapAxis::LeftRight:
      return { Vec2{ 0.f, r }, Vec2{ 0.f, -r } };
    case SlapAxis::FrontBack:
      return { Vec2{ r, 0.f }, Vec2{ -r, 0.f } };
    case SlapAxis::UpDown:
      {
        Vec2 const way{ std::cos (azimuth), std::sin (azimuth) };
        return { way * poleOffset, way * r };
      }
    }
  return { Vec2{ 0.f, r }, Vec2{ 0.f, -r } };
}

GlidePath
slapGlidePath (SlapAxis axis)
{
  switch (axis)
    {
    case SlapAxis::LeftRight:
      return GlidePath::ViaFront;
    case SlapAxis::FrontBack:
      return GlidePath::ViaSide;
    case SlapAxis::UpDown:
      return GlidePath::Vertical;
    }
  return GlidePath::ViaFront;
}

void
planSlap (GamePlan &plan, std::array<ShipState, flightShips> const &ships, FigureCall const &call,
          int beatsPerBar, juce::Random &dice, GameTuning const &tuning)
{
  plan.snaps = call.snaps;
  plan.climaxBeats = plan.startBeats;
  plan.endBeats = std::min (plan.startBeats
                                + static_cast<double> (tuning.slapBars) * beatsPerBar,
                            call.endByBeats);
  plan.opening = call.snaps ? Vec2{ tuning.slapSideRadius, 0.f } : plan.from[at (plan.leader)];
  auto const azimuth = std::atan2 (plan.opening.y, plan.opening.x);

  std::array<bool, slapAxes> allowed{};
  for (auto a = 0; a < slapAxes; ++a)
    {
      auto const axis = static_cast<SlapAxis> (a);
      auto const banded = axis == SlapAxis::LeftRight || call.bandFreq >= tuning.highBandFreq;
      allowed[at (a)] = banded && keepsClear (slapSides (axis, azimuth, tuning), plan, ships,
                                              call, tuning);
    }
  auto const axis = chooseSlapAxis (allowed, dice, tuning);
  auto sides = slapSides (axis, azimuth, tuning);
  // The first step is the bigger one: away from where it opens.
  if (sides[0].getDistanceFrom (plan.opening) < sides[1].getDistanceFrom (plan.opening))
    std::swap (sides[0], sides[1]);
  plan.sides = sides;
  plan.sideGlide = slapGlidePath (axis);
  for (auto s = 0; s < flightShips; ++s)
    plan.part[at (s)] = s == plan.leader ? 0 : 1;
}

int
slapEventsBy (GamePlan const &plan, double beats, int beatsPerBar, GameTuning const &tuning)
{
  if (beats < plan.startBeats)
    return 0;
  auto count = 1;
  auto event = plan.startBeats;
  for (;;)
    {
      auto const next = nextSlapEvent (event, beatsPerBar, tuning);
      if (next > beats || next >= plan.endBeats)
        return count;
      event = next;
      ++count;
    }
}

OrbitPoint
slapGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar, GameTuning const &tuning)
{
  auto const events = slapEventsBy (plan, beats, beatsPerBar, tuning);
  if (events == 0)
    return { plan.from[at (ship)], {} };
  return { slapPoint (plan, ship, events - 1), {} };
}

FigureStance
slapStance (GamePlan const &plan, double beats, int beatsPerBar, GameTuning const &tuning)
{
  FigureStance stance;
  auto const events = slapEventsBy (plan, beats, beatsPerBar, tuning);
  stance.hold = events > 0 && beats < plan.endBeats;
  stance.glide = events <= 1 ? GlidePath::Shortest : plan.sideGlide;
  return stance;
}

}
