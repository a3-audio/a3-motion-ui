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

#include <algorithm>

namespace a3
{

/** Every constant of the flight model, named, with its start value. The rig
 *  tunes these; the tests assert behaviours and do not depend on them.
 *
 *  Time is in beats throughout, so every speed follows the tempo without code
 *  of its own. Lengths are floor units: the room's edge is at radius 1. */
struct FlightTuning
{
  // gravity, steering: tuned together against unit/FlightGravity.cc
  // (2026-10-08). The plan's start values let a crowd just outside the room
  // pin the ship to the rim for good; three things set the values below.
  //
  // - gravityMax == steerMax. The pull can match the steering but never beat
  //   it at full error, so every capture on PATROL lets go: a ship that a
  //   group holds falls behind its rabbit until the steering wins (a crowd
  //   right on the path holds it ~1 bar). Below that, gravity beats the
  //   *linear* steering (kSteer * error) near a body, which is what bends.
  // - A small softening (0.045) makes the field steep: strong within ~0.15
  //   of a body (slingshots, the dead zone's wall), weak at 0.3 (a group
  //   off the path only nudges). A wider core reached the far ship as well.
  // - cSteer low (damping ratio ~0.4): the velocity term pulls the speed
  //   back to the rabbit's, so a high one swallowed every slingshot.
  //
  // The audible bars these meet, from the research notes (B1, B2): a
  // slingshot speeds the ship up >= 1.5x (smaller is not heard as faster),
  // and a group's bend pulls the closest pass >= 0.1 nearer.
  float gravity = 0.03f;     // G
  float softening = 0.045f;  // epsilon, floor units; 0 is read as flightSofteningFloor
  float gravityMax = 2.f;    // |a| cap, floor units per beat^2
  // the big path
  float orbitRadius = 0.7f;
  float orbitEccentricity = 0.2f;
  float orbitLapBars = 4.f;         // <= 0: the rabbit stands at its slot
  float orbitPrecessionBars = 32.f; // <= 0: the path does not turn
  // steering
  // kSteer: a PD loop without feed-forward of the rabbit's turn lags it by
  // about (centripetal acceleration / kSteer). The path's tightest bend needs
  // ~0.13 floor units/beat^2, so 0.6 left the ship 0.24 behind; 2.5 keeps it
  // within ~0.07.
  float steerStiffness = 2.5f; // kSteer, per beat^2
  float steerDamping = 1.3f;   // cSteer, per beat
  float steerMax = 2.f;        // == gravityMax, see above
  float damping = 0.05f;  // per beat
  float speedMin = 0.08f; // floor units per beat
  // 0.9 a beat is ~150 deg/s round the path at 120 BPM: well under the
  // ~360 deg/s where a path turns into a swirl (research notes, B1).
  float speedMax = 0.9f;
  float rimSoft = 0.9f;
  float rimStiffness = 4.f;
  // escort
  float captureRadius = 0.15f;
  float captureLapBars = 2.f;
  // ships among themselves, wander
  float separation = 0.004f;
  float wanderRadius = 0.12f;
  // the beat
  float pulseDepth = 0.3f;
  float pulseDownbeatDepth = 0.6f;
};

/** The smallest softening the field uses. A softening of 0 would divide
 *  0 by 0 for a ship exactly on a body (or on another ship); this floor keeps
 *  that finite, and gravityMax caps what it gives. */
constexpr float flightSofteningFloor = 1e-4f;

/** softening^2, never below flightSofteningFloor^2. */
inline float
flightSofteningSquared (FlightTuning const &tuning)
{
  auto const softening = std::max (tuning.softening, flightSofteningFloor);
  return softening * softening;
}

}
