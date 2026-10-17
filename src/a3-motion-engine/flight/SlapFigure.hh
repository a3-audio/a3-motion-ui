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

#pragma once

#include <a3-motion-engine/flight/GameFigures.hh>

#include <array>

namespace a3
{

/** The axis a slap steps along. Left-right always; front-back and up-down
 *  want energy above 6 kHz in the band. */
enum class SlapAxis
{
  LeftRight,
  FrontBack,
  UpDown,
};
constexpr int slapAxes = 3;

/** One of the `allowed` axes, thrown by its weight; LeftRight when none is
 *  allowed (or none weighs anything): a press must be heard. */
SlapAxis chooseSlapAxis (std::array<bool, slapAxes> const &allowed, juce::Random &dice,
                         GameTuning const &tuning);

/** The two points of `axis`, fixed in the room: left and right, or front
 *  and back, at slapSideRadius; up-down the middle (the band's top, a hair
 *  towards `azimuth` so the heard azimuth stays put) and the rim at
 *  `azimuth`, radians. */
std::array<Vec2, 2> slapSides (SlapAxis axis, float azimuth, GameTuning const &tuning);

/** How the ear crosses between the sides of `axis`. */
GlidePath slapGlidePath (SlapAxis axis);

/** Sets `plan` up as a slap: two bars from its opening (or less, by the
 *  call's end), the opening at the front when it snaps, the axis thrown among
 *  those the band allows whose sides keep clear of the other flying ships,
 *  the first side the farther from the opening. A partner (part 1) stands
 *  on the opposite point on the same beats. Not one point depends on where
 *  the groups stand: a side change, not an impulse towards a group. */
void planSlap (GamePlan &plan, std::array<ShipState, flightShips> const &ships,
               FigureCall const &call, int beatsPerBar, juce::Random &dice,
               GameTuning const &tuning);

/** How many of `plan`'s events have fallen by `beats`, the opening the
 *  first; 0 before it. */
int slapEventsBy (GamePlan const &plan, double beats, int beatsPerBar, GameTuning const &tuning);

/** Where ship `ship` stands at `beats`: the point of the last event. */
OrbitPoint slapGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
                     GameTuning const &tuning);

/** Held from the opening to the end; the opening glides the short way, every
 *  side change along the axis' own path. */
FigureStance slapStance (GamePlan const &plan, double beats, int beatsPerBar,
                         GameTuning const &tuning);

}
